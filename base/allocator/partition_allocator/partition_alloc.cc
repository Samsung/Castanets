// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc.h"

#include <string.h>

#include <memory>

#include "base/allocator/partition_allocator/oom.h"
#include "base/allocator/partition_allocator/page_allocator_internal.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_direct_map_extent.h"
#include "base/allocator/partition_allocator/partition_oom.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/spin_lock.h"
#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace base {

template <bool thread_safe>
NOINLINE void PartitionRoot<thread_safe>::OutOfMemory(size_t size) {
#if !defined(ARCH_CPU_64_BITS)
  // Check whether this OOM is due to a lot of super pages that are allocated
  // but not committed, probably due to http://crbug.com/421387.
  if (total_size_of_super_pages + total_size_of_direct_mapped_pages -
          total_size_of_committed_pages >
      kReasonableSizeOfUnusedPages) {
    internal::PartitionOutOfMemoryWithLotsOfUncommitedPages(size);
  }
#endif
  if (internal::g_oom_handling_function)
    (*internal::g_oom_handling_function)(size);
  OOM_CRASH(size);
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::DecommitEmptyPages() {
  for (Page*& page : global_empty_page_ring) {
    if (page)
      page->DecommitIfPossible(this);
    page = nullptr;
  }
}

// Two partition pages are used as guard / metadata page so make sure the super
// page size is bigger.
static_assert(kPartitionPageSize * 4 <= kSuperPageSize, "ok super page size");
static_assert(!(kSuperPageSize % kPartitionPageSize), "ok super page multiple");
// Four system pages gives us room to hack out a still-guard-paged piece
// of metadata in the middle of a guard partition page.
static_assert(kSystemPageSize * 4 <= kPartitionPageSize,
              "ok partition page size");
static_assert(!(kPartitionPageSize % kSystemPageSize),
              "ok partition page multiple");
static_assert(sizeof(internal::PartitionPage<internal::ThreadSafe>) <=
                  kPageMetadataSize,
              "PartitionPage should not be too big");
static_assert(sizeof(internal::PartitionBucket<internal::ThreadSafe>) <=
                  kPageMetadataSize,
              "PartitionBucket should not be too big");
static_assert(kPageMetadataSize * kNumPartitionPagesPerSuperPage <=
                  kSystemPageSize,
              "page metadata fits in hole");
// Limit to prevent callers accidentally overflowing an int size.
static_assert(kGenericMaxDirectMapped <=
                  (1UL << 31) + kPageAllocationGranularity,
              "maximum direct mapped allocation");
// Check that some of our zanier calculations worked out as expected.
static_assert(kGenericSmallestBucket == alignof(std::max_align_t),
              "generic smallest bucket");
static_assert(kGenericMaxBucketed == 983040, "generic max bucketed");
static_assert(kMaxSystemPagesPerSlotSpan < (1 << 8),
              "System pages per slot span must be less than 128.");

Lock& GetHooksLock() {
  static NoDestructor<Lock> lock;
  return *lock;
}

std::atomic<bool> PartitionAllocHooks::hooks_enabled_(false);
std::atomic<PartitionAllocHooks::AllocationObserverHook*>
    PartitionAllocHooks::allocation_observer_hook_(nullptr);
std::atomic<PartitionAllocHooks::FreeObserverHook*>
    PartitionAllocHooks::free_observer_hook_(nullptr);
std::atomic<PartitionAllocHooks::AllocationOverrideHook*>
    PartitionAllocHooks::allocation_override_hook_(nullptr);
std::atomic<PartitionAllocHooks::FreeOverrideHook*>
    PartitionAllocHooks::free_override_hook_(nullptr);
std::atomic<PartitionAllocHooks::ReallocOverrideHook*>
    PartitionAllocHooks::realloc_override_hook_(nullptr);

void PartitionAllocHooks::SetObserverHooks(AllocationObserverHook* alloc_hook,
                                           FreeObserverHook* free_hook) {
  AutoLock guard(GetHooksLock());

  // Chained hooks are not supported. Registering a non-null hook when a
  // non-null hook is already registered indicates somebody is trying to
  // overwrite a hook.
  PA_CHECK((!allocation_observer_hook_ && !free_observer_hook_) ||
           (!alloc_hook && !free_hook))
      << "Overwriting already set observer hooks";
  allocation_observer_hook_ = alloc_hook;
  free_observer_hook_ = free_hook;

  hooks_enabled_ = allocation_observer_hook_ || allocation_override_hook_;
}

void PartitionAllocHooks::SetOverrideHooks(AllocationOverrideHook* alloc_hook,
                                           FreeOverrideHook* free_hook,
                                           ReallocOverrideHook realloc_hook) {
  AutoLock guard(GetHooksLock());

  PA_CHECK((!allocation_override_hook_ && !free_override_hook_ &&
            !realloc_override_hook_) ||
           (!alloc_hook && !free_hook && !realloc_hook))
      << "Overwriting already set override hooks";
  allocation_override_hook_ = alloc_hook;
  free_override_hook_ = free_hook;
  realloc_override_hook_ = realloc_hook;

  hooks_enabled_ = allocation_observer_hook_ || allocation_override_hook_;
}

void PartitionAllocHooks::AllocationObserverHookIfEnabled(
    void* address,
    size_t size,
    const char* type_name) {
  if (auto* hook = allocation_observer_hook_.load(std::memory_order_relaxed))
    hook(address, size, type_name);
}

bool PartitionAllocHooks::AllocationOverrideHookIfEnabled(
    void** out,
    int flags,
    size_t size,
    const char* type_name) {
  if (auto* hook = allocation_override_hook_.load(std::memory_order_relaxed))
    return hook(out, flags, size, type_name);
  return false;
}

void PartitionAllocHooks::FreeObserverHookIfEnabled(void* address) {
  if (auto* hook = free_observer_hook_.load(std::memory_order_relaxed))
    hook(address);
}

bool PartitionAllocHooks::FreeOverrideHookIfEnabled(void* address) {
  if (auto* hook = free_override_hook_.load(std::memory_order_relaxed))
    return hook(address);
  return false;
}

void PartitionAllocHooks::ReallocObserverHookIfEnabled(void* old_address,
                                                       void* new_address,
                                                       size_t size,
                                                       const char* type_name) {
  // Report a reallocation as a free followed by an allocation.
  AllocationObserverHook* allocation_hook =
      allocation_observer_hook_.load(std::memory_order_relaxed);
  FreeObserverHook* free_hook =
      free_observer_hook_.load(std::memory_order_relaxed);
  if (allocation_hook && free_hook) {
    free_hook(old_address);
    allocation_hook(new_address, size, type_name);
  }
}

bool PartitionAllocHooks::ReallocOverrideHookIfEnabled(size_t* out,
                                                       void* address) {
  if (ReallocOverrideHook* hook =
          realloc_override_hook_.load(std::memory_order_relaxed)) {
    return hook(out, address);
  }
  return false;
}

void PartitionAllocGlobalInit(OomFunction on_out_of_memory) {
  PA_DCHECK(on_out_of_memory);
  internal::g_oom_handling_function = on_out_of_memory;

#if defined(ARCH_CPU_64_BITS) && !defined(OS_NACL)
  // Reserve address space for partition alloc.
  if (IsPartitionAllocGigaCageEnabled())
    internal::PartitionAddressSpace::Init();
#endif
}

void PartitionAllocGlobalUninitForTesting() {
#if defined(ARCH_CPU_64_BITS) && !defined(OS_NACL)
  if (IsPartitionAllocGigaCageEnabled())
    internal::PartitionAddressSpace::UninitForTesting();
#endif
  internal::g_oom_handling_function = nullptr;
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::InitSlowPath() {
  ScopedGuard guard{lock_};

  if (initialized.load(std::memory_order_relaxed))
    return;

  // We mark the sentinel bucket/page as free to make sure it is skipped by our
  // logic to find a new active page.
  //
  // This may be executed several times, once per PartitionRoot. This is not an
  // issue, as the operation is atomic and idempotent.
  Bucket::get_sentinel_bucket()->active_pages_head = Page::get_sentinel_page();

  // This is a "magic" value so we can test if a root pointer is valid.
  inverted_self = ~reinterpret_cast<uintptr_t>(this);

  // Precalculate some shift and mask constants used in the hot path.
  // Example: malloc(41) == 101001 binary.
  // Order is 6 (1 << 6-1) == 32 is highest bit set.
  // order_index is the next three MSB == 010 == 2.
  // sub_order_index_mask is a mask for the remaining bits == 11 (masking to 01
  // for
  // the sub_order_index).
  size_t order;
  for (order = 0; order <= kBitsPerSizeT; ++order) {
    size_t order_index_shift;
    if (order < kGenericNumBucketsPerOrderBits + 1)
      order_index_shift = 0;
    else
      order_index_shift = order - (kGenericNumBucketsPerOrderBits + 1);
    order_index_shifts[order] = order_index_shift;
    size_t sub_order_index_mask;
    if (order == kBitsPerSizeT) {
      // This avoids invoking undefined behavior for an excessive shift.
      sub_order_index_mask =
          static_cast<size_t>(-1) >> (kGenericNumBucketsPerOrderBits + 1);
    } else {
      sub_order_index_mask = ((static_cast<size_t>(1) << order) - 1) >>
                             (kGenericNumBucketsPerOrderBits + 1);
    }
    order_sub_index_masks[order] = sub_order_index_mask;
  }

  // Set up the actual usable buckets first.
  // Note that typical values (i.e. min allocation size of 8) will result in
  // pseudo buckets (size==9 etc. or more generally, size is not a multiple
  // of the smallest allocation granularity).
  // We avoid them in the bucket lookup map, but we tolerate them to keep the
  // code simpler and the structures more generic.
  size_t i, j;
  size_t current_size = kGenericSmallestBucket;
  size_t current_increment =
      kGenericSmallestBucket >> kGenericNumBucketsPerOrderBits;
  Bucket* bucket = &buckets[0];
  for (i = 0; i < kGenericNumBucketedOrders; ++i) {
    for (j = 0; j < kGenericNumBucketsPerOrder; ++j) {
      bucket->Init(current_size);
      // Disable pseudo buckets so that touching them faults.
      if (current_size % kGenericSmallestBucket)
        bucket->active_pages_head = nullptr;
      current_size += current_increment;
      ++bucket;
    }
    current_increment <<= 1;
  }
  PA_DCHECK(current_size == 1 << kGenericMaxBucketedOrder);
  PA_DCHECK(bucket == &buckets[0] + kGenericNumBuckets);

  // Then set up the fast size -> bucket lookup table.
  bucket = &buckets[0];
  Bucket** bucket_ptr = &bucket_lookups[0];
  for (order = 0; order <= kBitsPerSizeT; ++order) {
    for (j = 0; j < kGenericNumBucketsPerOrder; ++j) {
      if (order < kGenericMinBucketedOrder) {
        // Use the bucket of the finest granularity for malloc(0) etc.
        *bucket_ptr++ = &buckets[0];
      } else if (order > kGenericMaxBucketedOrder) {
        *bucket_ptr++ = Bucket::get_sentinel_bucket();
      } else {
        Bucket* valid_bucket = bucket;
        // Skip over invalid buckets.
        while (valid_bucket->slot_size % kGenericSmallestBucket)
          valid_bucket++;
        *bucket_ptr++ = valid_bucket;
        bucket++;
      }
    }
  }
  PA_DCHECK(bucket == &buckets[0] + kGenericNumBuckets);
  PA_DCHECK(bucket_ptr == &bucket_lookups[0] + ((kBitsPerSizeT + 1) *
                                                kGenericNumBucketsPerOrder));
  // And there's one last bucket lookup that will be hit for e.g. malloc(-1),
  // which tries to overflow to a non-existent order.
  *bucket_ptr = Bucket::get_sentinel_bucket();

  initialized = true;
}

template <bool thread_safe>
bool PartitionRoot<thread_safe>::ReallocDirectMappedInPlace(
    internal::PartitionPage<thread_safe>* page,
    size_t raw_size) {
  PA_DCHECK(page->bucket->is_direct_mapped());

  raw_size = internal::PartitionCookieSizeAdjustAdd(raw_size);

  // Note that the new size might be a bucketed size; this function is called
  // whenever we're reallocating a direct mapped allocation.
  size_t new_size = Bucket::get_direct_map_size(raw_size);
  if (new_size < kGenericMinDirectMappedDownsize)
    return false;

  // bucket->slot_size is the current size of the allocation.
  size_t current_size = page->bucket->slot_size;
  char* char_ptr = static_cast<char*>(Page::ToPointer(page));
  if (new_size == current_size) {
    // No need to move any memory around, but update size and cookie below.
  } else if (new_size < current_size) {
    size_t map_size = DirectMapExtent::FromPage(page)->map_size;

    // Don't reallocate in-place if new size is less than 80 % of the full
    // map size, to avoid holding on to too much unused address space.
    if ((new_size / kSystemPageSize) * 5 < (map_size / kSystemPageSize) * 4)
      return false;

    // Shrink by decommitting unneeded pages and making them inaccessible.
    size_t decommit_size = current_size - new_size;
    DecommitSystemPages(char_ptr + new_size, decommit_size);
    SetSystemPagesAccess(char_ptr + new_size, decommit_size, PageInaccessible);
  } else if (new_size <= DirectMapExtent::FromPage(page)->map_size) {
    // Grow within the actually allocated memory. Just need to make the
    // pages accessible again.
    size_t recommit_size = new_size - current_size;
    SetSystemPagesAccess(char_ptr + current_size, recommit_size, PageReadWrite);
    RecommitSystemPages(char_ptr + current_size, recommit_size);

#if DCHECK_IS_ON()
    memset(char_ptr + current_size, kUninitializedByte, recommit_size);
#endif
  } else {
    // We can't perform the realloc in-place.
    // TODO: support this too when possible.
    return false;
  }

#if DCHECK_IS_ON()
  // Write a new trailing cookie.
  internal::PartitionCookieWriteValue(char_ptr + raw_size -
                                      internal::kCookieSize);
#endif

  page->set_raw_size(raw_size);
  PA_DCHECK(page->get_raw_size() == raw_size);

  page->bucket->slot_size = new_size;
  return true;
}

template <bool thread_safe>
void* PartitionRoot<thread_safe>::ReallocFlags(int flags,
                                               void* ptr,
                                               size_t new_size,
                                               const char* type_name) {
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  CHECK_MAX_SIZE_OR_RETURN_NULLPTR(new_size, flags);
  void* result = realloc(ptr, new_size);
  PA_CHECK(result || flags & PartitionAllocReturnNull);
  return result;
#else
  if (UNLIKELY(!ptr))
    return AllocFlags(flags, new_size, type_name);
  if (UNLIKELY(!new_size)) {
    Free(ptr);
    return nullptr;
  }

  if (new_size > kGenericMaxDirectMapped) {
    if (flags & PartitionAllocReturnNull)
      return nullptr;
    internal::PartitionExcessiveAllocationSize(new_size);
  }

  const bool hooks_enabled = PartitionAllocHooks::AreHooksEnabled();
  bool overridden = false;
  size_t actual_old_size;
  if (UNLIKELY(hooks_enabled)) {
    overridden = PartitionAllocHooks::ReallocOverrideHookIfEnabled(
        &actual_old_size, ptr);
  }
  if (LIKELY(!overridden)) {
    auto* page =
        Page::FromPointer(internal::PartitionCookieFreePointerAdjust(ptr));
    bool success = false;
    {
      internal::ScopedGuard<thread_safe> guard{lock_};
      // TODO(palmer): See if we can afford to make this a CHECK.
      PA_DCHECK(IsValidPage(page));

      if (UNLIKELY(page->bucket->is_direct_mapped())) {
        // We may be able to perform the realloc in place by changing the
        // accessibility of memory pages and, if reducing the size, decommitting
        // them.
        success = ReallocDirectMappedInPlace(page, new_size);
      }
    }
    if (success) {
      if (UNLIKELY(hooks_enabled)) {
        PartitionAllocHooks::ReallocObserverHookIfEnabled(ptr, ptr, new_size,
                                                          type_name);
      }
      return ptr;
    }

    const size_t actual_new_size = ActualSize(new_size);
    actual_old_size = PartitionAllocGetSize<thread_safe>(ptr);

    // TODO: note that tcmalloc will "ignore" a downsizing realloc() unless the
    // new size is a significant percentage smaller. We could do the same if we
    // determine it is a win.
    if (actual_new_size == actual_old_size) {
      // Trying to allocate a block of size |new_size| would give us a block of
      // the same size as the one we've already got, so re-use the allocation
      // after updating statistics (and cookies, if present).
      page->set_raw_size(internal::PartitionCookieSizeAdjustAdd(new_size));
#if DCHECK_IS_ON()
      // Write a new trailing cookie when it is possible to keep track of
      // |new_size| via the raw size pointer.
      if (page->get_raw_size_ptr())
        internal::PartitionCookieWriteValue(static_cast<char*>(ptr) + new_size);
#endif
      return ptr;
    }
  }

  // This realloc cannot be resized in-place. Sadness.
  void* ret = AllocFlags(flags, new_size, type_name);
  if (!ret) {
    if (flags & PartitionAllocReturnNull)
      return nullptr;
    internal::PartitionExcessiveAllocationSize(new_size);
  }

  size_t copy_size = actual_old_size;
  if (new_size < copy_size)
    copy_size = new_size;

  memcpy(ret, ptr, copy_size);
  Free(ptr);
  return ret;
#endif
}

template <bool thread_safe>
static size_t PartitionPurgePage(internal::PartitionPage<thread_safe>* page,
                                 bool discard) {
  const internal::PartitionBucket<thread_safe>* bucket = page->bucket;
  size_t slot_size = bucket->slot_size;
  if (slot_size < kSystemPageSize || !page->num_allocated_slots)
    return 0;

  size_t bucket_num_slots = bucket->get_slots_per_span();
  size_t discardable_bytes = 0;

  size_t raw_size = page->get_raw_size();
  if (raw_size) {
    uint32_t used_bytes = static_cast<uint32_t>(RoundUpToSystemPage(raw_size));
    discardable_bytes = bucket->slot_size - used_bytes;
    if (discardable_bytes && discard) {
      char* ptr = reinterpret_cast<char*>(
          internal::PartitionPage<thread_safe>::ToPointer(page));
      ptr += used_bytes;
      DiscardSystemPages(ptr, discardable_bytes);
    }
    return discardable_bytes;
  }

  constexpr size_t kMaxSlotCount =
      (kPartitionPageSize * kMaxPartitionPagesPerSlotSpan) / kSystemPageSize;
  PA_DCHECK(bucket_num_slots <= kMaxSlotCount);
  PA_DCHECK(page->num_unprovisioned_slots < bucket_num_slots);
  size_t num_slots = bucket_num_slots - page->num_unprovisioned_slots;
  char slot_usage[kMaxSlotCount];
#if !defined(OS_WIN)
  // The last freelist entry should not be discarded when using OS_WIN.
  // DiscardVirtualMemory makes the contents of discarded memory undefined.
  size_t last_slot = static_cast<size_t>(-1);
#endif
  memset(slot_usage, 1, num_slots);
  char* ptr = reinterpret_cast<char*>(
      internal::PartitionPage<thread_safe>::ToPointer(page));
  // First, walk the freelist for this page and make a bitmap of which slots
  // are not in use.
  for (internal::PartitionFreelistEntry* entry = page->freelist_head; entry;
       /**/) {
    size_t slot_index = (reinterpret_cast<char*>(entry) - ptr) / slot_size;
    PA_DCHECK(slot_index < num_slots);
    slot_usage[slot_index] = 0;
    entry = internal::EncodedPartitionFreelistEntry::Decode(entry->next);
#if !defined(OS_WIN)
    // If we have a slot where the masked freelist entry is 0, we can actually
    // discard that freelist entry because touching a discarded page is
    // guaranteed to return original content or 0. (Note that this optimization
    // won't fire on big-endian machines because the masking function is
    // negation.)
    if (!internal::PartitionFreelistEntry::Encode(entry))
      last_slot = slot_index;
#endif
  }

  // If the slot(s) at the end of the slot span are not in used, we can truncate
  // them entirely and rewrite the freelist.
  size_t truncated_slots = 0;
  while (!slot_usage[num_slots - 1]) {
    truncated_slots++;
    num_slots--;
    PA_DCHECK(num_slots);
  }
  // First, do the work of calculating the discardable bytes. Don't actually
  // discard anything unless the discard flag was passed in.
  if (truncated_slots) {
    size_t unprovisioned_bytes = 0;
    char* begin_ptr = ptr + (num_slots * slot_size);
    char* end_ptr = begin_ptr + (slot_size * truncated_slots);
    begin_ptr = reinterpret_cast<char*>(
        RoundUpToSystemPage(reinterpret_cast<size_t>(begin_ptr)));
    // We round the end pointer here up and not down because we're at the end of
    // a slot span, so we "own" all the way up the page boundary.
    end_ptr = reinterpret_cast<char*>(
        RoundUpToSystemPage(reinterpret_cast<size_t>(end_ptr)));
    PA_DCHECK(end_ptr <= ptr + bucket->get_bytes_per_span());
    if (begin_ptr < end_ptr) {
      unprovisioned_bytes = end_ptr - begin_ptr;
      discardable_bytes += unprovisioned_bytes;
    }
    if (unprovisioned_bytes && discard) {
      PA_DCHECK(truncated_slots > 0);
      size_t num_new_entries = 0;
      page->num_unprovisioned_slots += static_cast<uint16_t>(truncated_slots);

      // Rewrite the freelist.
      internal::PartitionFreelistEntry* head = nullptr;
      internal::PartitionFreelistEntry* back = head;
      for (size_t slot_index = 0; slot_index < num_slots; ++slot_index) {
        if (slot_usage[slot_index])
          continue;

        auto* entry = reinterpret_cast<internal::PartitionFreelistEntry*>(
            ptr + (slot_size * slot_index));
        if (!head) {
          head = entry;
          back = entry;
        } else {
          back->next = internal::PartitionFreelistEntry::Encode(entry);
          back = entry;
        }
        num_new_entries++;
#if !defined(OS_WIN)
        last_slot = slot_index;
#endif
      }

      page->freelist_head = head;
      if (back)
        back->next = internal::PartitionFreelistEntry::Encode(nullptr);

      PA_DCHECK(num_new_entries == num_slots - page->num_allocated_slots);
      // Discard the memory.
      DiscardSystemPages(begin_ptr, unprovisioned_bytes);
    }
  }

  // Next, walk the slots and for any not in use, consider where the system page
  // boundaries occur. We can release any system pages back to the system as
  // long as we don't interfere with a freelist pointer or an adjacent slot.
  for (size_t i = 0; i < num_slots; ++i) {
    if (slot_usage[i])
      continue;
    // The first address we can safely discard is just after the freelist
    // pointer. There's one quirk: if the freelist pointer is actually NULL, we
    // can discard that pointer value too.
    char* begin_ptr = ptr + (i * slot_size);
    char* end_ptr = begin_ptr + slot_size;
#if !defined(OS_WIN)
    if (i != last_slot)
      begin_ptr += sizeof(internal::PartitionFreelistEntry);
#else
    begin_ptr += sizeof(internal::PartitionFreelistEntry);
#endif
    begin_ptr = reinterpret_cast<char*>(
        RoundUpToSystemPage(reinterpret_cast<size_t>(begin_ptr)));
    end_ptr = reinterpret_cast<char*>(
        RoundDownToSystemPage(reinterpret_cast<size_t>(end_ptr)));
    if (begin_ptr < end_ptr) {
      size_t partial_slot_bytes = end_ptr - begin_ptr;
      discardable_bytes += partial_slot_bytes;
      if (discard)
        DiscardSystemPages(begin_ptr, partial_slot_bytes);
    }
  }
  return discardable_bytes;
}

template <bool thread_safe>
static void PartitionPurgeBucket(
    internal::PartitionBucket<thread_safe>* bucket) {
  if (bucket->active_pages_head !=
      internal::PartitionPage<thread_safe>::get_sentinel_page()) {
    for (internal::PartitionPage<thread_safe>* page = bucket->active_pages_head;
         page; page = page->next_page) {
      PA_DCHECK(page !=
                internal::PartitionPage<thread_safe>::get_sentinel_page());
      PartitionPurgePage(page, true);
    }
  }
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::PurgeMemory(int flags) {
  ScopedGuard guard{lock_};
  if (flags & PartitionPurgeDecommitEmptyPages)
    DecommitEmptyPages();
  if (flags & PartitionPurgeDiscardUnusedSystemPages) {
    for (size_t i = 0; i < kGenericNumBuckets; ++i) {
      Bucket* bucket = &buckets[i];
      if (bucket->slot_size >= kSystemPageSize)
        PartitionPurgeBucket(bucket);
    }
  }
}

template <bool thread_safe>
static void PartitionDumpPageStats(PartitionBucketMemoryStats* stats_out,
                                   internal::PartitionPage<thread_safe>* page) {
  uint16_t bucket_num_slots = page->bucket->get_slots_per_span();

  if (page->is_decommitted()) {
    ++stats_out->num_decommitted_pages;
    return;
  }

  stats_out->discardable_bytes += PartitionPurgePage(page, false);

  size_t raw_size = page->get_raw_size();
  if (raw_size) {
    stats_out->active_bytes += static_cast<uint32_t>(raw_size);
  } else {
    stats_out->active_bytes +=
        (page->num_allocated_slots * stats_out->bucket_slot_size);
  }

  size_t page_bytes_resident =
      RoundUpToSystemPage((bucket_num_slots - page->num_unprovisioned_slots) *
                          stats_out->bucket_slot_size);
  stats_out->resident_bytes += page_bytes_resident;
  if (page->is_empty()) {
    stats_out->decommittable_bytes += page_bytes_resident;
    ++stats_out->num_empty_pages;
  } else if (page->is_full()) {
    ++stats_out->num_full_pages;
  } else {
    PA_DCHECK(page->is_active());
    ++stats_out->num_active_pages;
  }
}

template <bool thread_safe>
static void PartitionDumpBucketStats(
    PartitionBucketMemoryStats* stats_out,
    const internal::PartitionBucket<thread_safe>* bucket) {
  PA_DCHECK(!bucket->is_direct_mapped());
  stats_out->is_valid = false;
  // If the active page list is empty (==
  // internal::PartitionPage::get_sentinel_page()), the bucket might still need
  // to be reported if it has a list of empty, decommitted or full pages.
  if (bucket->active_pages_head ==
          internal::PartitionPage<thread_safe>::get_sentinel_page() &&
      !bucket->empty_pages_head && !bucket->decommitted_pages_head &&
      !bucket->num_full_pages)
    return;

  memset(stats_out, '\0', sizeof(*stats_out));
  stats_out->is_valid = true;
  stats_out->is_direct_map = false;
  stats_out->num_full_pages = static_cast<size_t>(bucket->num_full_pages);
  stats_out->bucket_slot_size = bucket->slot_size;
  uint16_t bucket_num_slots = bucket->get_slots_per_span();
  size_t bucket_useful_storage = stats_out->bucket_slot_size * bucket_num_slots;
  stats_out->allocated_page_size = bucket->get_bytes_per_span();
  stats_out->active_bytes = bucket->num_full_pages * bucket_useful_storage;
  stats_out->resident_bytes =
      bucket->num_full_pages * stats_out->allocated_page_size;

  for (internal::PartitionPage<thread_safe>* page = bucket->empty_pages_head;
       page; page = page->next_page) {
    PA_DCHECK(page->is_empty() || page->is_decommitted());
    PartitionDumpPageStats(stats_out, page);
  }
  for (internal::PartitionPage<thread_safe>* page =
           bucket->decommitted_pages_head;
       page; page = page->next_page) {
    PA_DCHECK(page->is_decommitted());
    PartitionDumpPageStats(stats_out, page);
  }

  if (bucket->active_pages_head !=
      internal::PartitionPage<thread_safe>::get_sentinel_page()) {
    for (internal::PartitionPage<thread_safe>* page = bucket->active_pages_head;
         page; page = page->next_page) {
      PA_DCHECK(page !=
                internal::PartitionPage<thread_safe>::get_sentinel_page());
      PartitionDumpPageStats(stats_out, page);
    }
  }
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::DumpStats(const char* partition_name,
                                           bool is_light_dump,
                                           PartitionStatsDumper* dumper) {
  ScopedGuard guard{lock_};
  PartitionMemoryStats stats = {0};
  stats.total_mmapped_bytes =
      total_size_of_super_pages + total_size_of_direct_mapped_pages;
  stats.total_committed_bytes = total_size_of_committed_pages;

  size_t direct_mapped_allocations_total_size = 0;

  static const size_t kMaxReportableDirectMaps = 4096;

  // Allocate on the heap rather than on the stack to avoid stack overflow
  // skirmishes (on Windows, in particular).
  std::unique_ptr<uint32_t[]> direct_map_lengths = nullptr;
  if (!is_light_dump) {
    direct_map_lengths =
        std::unique_ptr<uint32_t[]>(new uint32_t[kMaxReportableDirectMaps]);
  }

  PartitionBucketMemoryStats bucket_stats[kGenericNumBuckets];
  size_t num_direct_mapped_allocations = 0;
  {
    for (size_t i = 0; i < kGenericNumBuckets; ++i) {
      const Bucket* bucket = &buckets[i];
      // Don't report the pseudo buckets that the generic allocator sets up in
      // order to preserve a fast size->bucket map (see
      // PartitionRoot::Init() for details).
      if (!bucket->active_pages_head)
        bucket_stats[i].is_valid = false;
      else
        PartitionDumpBucketStats(&bucket_stats[i], bucket);
      if (bucket_stats[i].is_valid) {
        stats.total_resident_bytes += bucket_stats[i].resident_bytes;
        stats.total_active_bytes += bucket_stats[i].active_bytes;
        stats.total_decommittable_bytes += bucket_stats[i].decommittable_bytes;
        stats.total_discardable_bytes += bucket_stats[i].discardable_bytes;
      }
    }

    for (DirectMapExtent* extent = direct_map_list;
         extent && num_direct_mapped_allocations < kMaxReportableDirectMaps;
         extent = extent->next_extent, ++num_direct_mapped_allocations) {
      PA_DCHECK(!extent->next_extent ||
                extent->next_extent->prev_extent == extent);
      size_t slot_size = extent->bucket->slot_size;
      direct_mapped_allocations_total_size += slot_size;
      if (is_light_dump)
        continue;
      direct_map_lengths[num_direct_mapped_allocations] = slot_size;
    }
  }

  if (!is_light_dump) {
    // Call |PartitionsDumpBucketStats| after collecting stats because it can
    // try to allocate using |PartitionRoot::Alloc()| and it can't
    // obtain the lock.
    for (size_t i = 0; i < kGenericNumBuckets; ++i) {
      if (bucket_stats[i].is_valid)
        dumper->PartitionsDumpBucketStats(partition_name, &bucket_stats[i]);
    }

    for (size_t i = 0; i < num_direct_mapped_allocations; ++i) {
      uint32_t size = direct_map_lengths[i];

      PartitionBucketMemoryStats mapped_stats = {};
      mapped_stats.is_valid = true;
      mapped_stats.is_direct_map = true;
      mapped_stats.num_full_pages = 1;
      mapped_stats.allocated_page_size = size;
      mapped_stats.bucket_slot_size = size;
      mapped_stats.active_bytes = size;
      mapped_stats.resident_bytes = size;
      dumper->PartitionsDumpBucketStats(partition_name, &mapped_stats);
    }
  }

  stats.total_resident_bytes += direct_mapped_allocations_total_size;
  stats.total_active_bytes += direct_mapped_allocations_total_size;
  dumper->PartitionDumpTotals(partition_name, &stats);
}

template struct BASE_EXPORT PartitionRoot<internal::ThreadSafe>;
template struct BASE_EXPORT PartitionRoot<internal::NotThreadSafe>;

}  // namespace base
