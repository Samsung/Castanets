// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/storage/storage_queue.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "chrome/browser/policy/messaging_layer/util/task_runner_context.h"

namespace reporting {

namespace {

// The size in bytes that all files and records are rounded to (for privacy:
// make it harder to differ between kinds of records).
constexpr size_t FRAME_SIZE = 16u;

// Size of the buffer to read data to. Must be multiple of FRAME_SIZE
constexpr size_t BUFFER_SIZE = 1024u * 1024u;  // 1 MiB
static_assert(BUFFER_SIZE % FRAME_SIZE == 0u,
              "Buffer size not multiple of frame size");

// Helper functions for FRAME_SIZE alignment support.
size_t RoundUpToFrameSize(size_t size) {
  return (size + FRAME_SIZE - 1) / FRAME_SIZE * FRAME_SIZE;
}
size_t GetPaddingToNextFrameSize(size_t size) {
  return FRAME_SIZE - (size % FRAME_SIZE);
}

// Internal structure of the record header. Must fit in FRAME_SIZE.
struct RecordHeader {
  uint64_t record_seq_number;
  uint32_t record_size;  // Size of the blob, not including RecordHeader
  uint32_t record_hash;  // Hash of the blob, not including RecordHeader
  // Data starts right after the header.
};
}  // namespace

// static
void StorageQueue::Create(
    const Options& options,
    StartUploadCb start_upload_cb,
    base::OnceCallback<void(StatusOr<scoped_refptr<StorageQueue>>)>
        completion_cb) {
  // Initialize StorageQueue object loading the data.
  class StorageQueueInitContext
      : public TaskRunnerContext<StatusOr<scoped_refptr<StorageQueue>>> {
   public:
    StorageQueueInitContext(
        scoped_refptr<StorageQueue> storage_queue,
        base::OnceCallback<void(StatusOr<scoped_refptr<StorageQueue>>)>
            callback)
        : TaskRunnerContext<StatusOr<scoped_refptr<StorageQueue>>>(
              std::move(callback),
              storage_queue->sequenced_task_runner_),
          storage_queue_(std::move(storage_queue)) {
      DCHECK(storage_queue_);
    }

   private:
    // Context can only be deleted by calling Response method.
    ~StorageQueueInitContext() override = default;

    void OnStart() override {
      auto init_status = storage_queue_->Init();
      if (!init_status.ok()) {
        Response(StatusOr<scoped_refptr<StorageQueue>>(init_status));
        return;
      }
      Response(std::move(storage_queue_));
    }

    scoped_refptr<StorageQueue> storage_queue_;
  };

  // Create StorageQueue object.
  // Cannot use base::MakeRefCounted<StorageQueue>, because constructor is
  // private.
  scoped_refptr<StorageQueue> storage_queue = base::WrapRefCounted(
      new StorageQueue(options, std::move(start_upload_cb)));

  // Asynchronously run initialization.
  Start<StorageQueueInitContext>(std::move(storage_queue),
                                 std::move(completion_cb));
}

StorageQueue::StorageQueue(const Options& options,
                           StartUploadCb start_upload_cb)
    : options_(options),
      start_upload_cb_(std::move(start_upload_cb)),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock()})) {
  DETACH_FROM_SEQUENCE(storage_queue_sequence_checker_);
}

StorageQueue::~StorageQueue() {
  // TODO(b/153364303): Should be
  // DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);

  // Stop upload timer.
  upload_timer_.AbandonAndStop();
  // CLose all opened files.
  for (auto& file : files_) {
    file.second->Close();
  }
}

Status StorageQueue::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  // Make sure the assigned directory exists.
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(options_.directory(), &error)) {
    return Status(
        error::UNAVAILABLE,
        base::StrCat(
            {"Fileset directory '", options_.directory().MaybeAsASCII(),
             "' does not exist, error=", base::File::ErrorToString(error)}));
  }
  RETURN_IF_ERROR(EnumerateDataFiles());
  RETURN_IF_ERROR(ScanLastFile());
  // Initiate periodic uploading, if needed.
  if (!options_.upload_period().is_zero()) {
    upload_timer_.Start(FROM_HERE, options_.upload_period(), this,
                        &StorageQueue::PeriodicUpload);
  }
  return Status::StatusOK();
}

Status StorageQueue::EnumerateDataFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  first_seq_number_ = 0;
  base::FileEnumerator dir_enum(
      options_.directory(),
      /*recursive=*/false, base::FileEnumerator::FILES,
      base::StrCat({options_.file_prefix(), FILE_PATH_LITERAL(".*")}));
  base::FilePath full_name;
  while (full_name = dir_enum.Next(), !full_name.empty()) {
    const auto extension = dir_enum.GetInfo().GetName().Extension();
    if (extension.empty()) {
      return Status(error::INTERNAL,
                    base::StrCat({"File has no extension: '",
                                  full_name.MaybeAsASCII(), "'"}));
    }
    uint64_t seq_number;
    bool success = base::StringToUint64(
        dir_enum.GetInfo().GetName().Extension().substr(1), &seq_number);
    if (!success) {
      return Status(error::INTERNAL,
                    base::StrCat({"File extension does not parse: '",
                                  full_name.MaybeAsASCII(), "'"}));
    }
    if (!files_
             .emplace(seq_number, base::MakeRefCounted<SingleFile>(
                                      full_name, dir_enum.GetInfo().GetSize()))
             .second) {
      return Status(error::ALREADY_EXISTS,
                    base::StrCat({"Sequencing duplicated: '",
                                  full_name.MaybeAsASCII(), "'"}));
    }
    if (first_seq_number_ > seq_number) {
      first_seq_number_ = seq_number;  // Records with this number exist.
    }
  }
  return Status::StatusOK();
}

Status StorageQueue::ScanLastFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  next_seq_number_ = 0;
  if (files_.empty()) {
    return Status::StatusOK();
  }
  next_seq_number_ = files_.rbegin()->first;
  // Scan the file. Open it and leave open, because it might soon be needed
  // again (for the next or repeated Upload), and we won't waste time closing
  // and reopening it. If the file remains open for too long, it will auto-close
  // by timer.
  scoped_refptr<SingleFile> last_file = files_.rbegin()->second.get();
  auto open_status = last_file->Open(/*read_only=*/true);
  if (!open_status.ok()) {
    LOG(ERROR) << "Error opening file " << last_file->name()
               << ", status=" << open_status;
    return Status(error::DATA_LOSS, base::StrCat({"Error opening file: '",
                                                  last_file->name(), "'"}));
  }
  uint32_t pos = 0;
  for (;;) {
    // Read the header
    auto read_result = last_file->Read(pos, sizeof(RecordHeader));
    if (read_result.status().error_code() == error::OUT_OF_RANGE) {
      // End of file detected.
      break;
    }
    if (!read_result.ok()) {
      // Error detected.
      LOG(ERROR) << "Error reading file " << last_file->name()
                 << ", status=" << read_result.status();
      break;
    }
    pos += read_result.ValueOrDie().size();
    if (read_result.ValueOrDie().size() < sizeof(RecordHeader)) {
      // Error detected.
      LOG(ERROR) << "Incomplete record header in file " << last_file->name();
      break;
    }
    // Copy the header, since the buffer might be overwritten later on.
    const RecordHeader header =
        *reinterpret_cast<const RecordHeader*>(read_result.ValueOrDie().data());
    // Read the data (rounded to frame size).
    const size_t data_size = RoundUpToFrameSize(header.record_size);
    read_result = last_file->Read(pos, data_size);
    if (!read_result.ok()) {
      // Error detected.
      LOG(ERROR) << "Error reading file " << last_file->name()
                 << ", status=" << read_result.status();
      break;
    }
    pos += read_result.ValueOrDie().size();
    if (read_result.ValueOrDie().size() < data_size) {
      // Error detected.
      LOG(ERROR) << "Incomplete record in file " << last_file->name();
      break;
    }
    // Everything looks all right. Advance the sequencing number.
    if (header.record_seq_number != next_seq_number_) {
      LOG(ERROR) << "Sequencing number mismatch, expected=" << next_seq_number_
                 << ", actual=" << header.record_seq_number << ", file "
                 << last_file->name();
      break;
    }
    ++next_seq_number_;
  }
  return Status::StatusOK();
}

StatusOr<scoped_refptr<StorageQueue::SingleFile>> StorageQueue::AssignLastFile(
    size_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  if (files_.empty()) {
    // Create the very first file (empty).
    next_seq_number_ = 0;
    auto insert_result = files_.emplace(
        next_seq_number_,
        base::MakeRefCounted<SingleFile>(
            options_.directory()
                .Append(options_.file_prefix())
                .AddExtensionASCII(base::NumberToString(next_seq_number_)),
            /*size=*/0));
    DCHECK(insert_result.second);
  }
  if (size > options_.total_size()) {
    return Status(error::OUT_OF_RANGE, "Too much data to be recorded at once");
  }
  scoped_refptr<SingleFile> last_file = files_.rbegin()->second;
  if (last_file->size() > 0 &&  // Cannot have a file with no records.
      last_file->size() + size + sizeof(RecordHeader) + FRAME_SIZE >
          options_.single_file_size()) {
    // The last file will become too large, asynchronously close it and add
    // new.
    last_file->Close();
    auto insert_result = files_.emplace(
        next_seq_number_,
        base::MakeRefCounted<SingleFile>(
            options_.directory()
                .Append(options_.file_prefix())
                .AddExtensionASCII(base::NumberToString(next_seq_number_)),
            /*size=*/0));
    if (!insert_result.second) {
      return Status(
          error::ALREADY_EXISTS,
          base::StrCat({"Sequence number already assigned: '",
                        base::NumberToString(next_seq_number_), "'"}));
    }
    last_file = insert_result.first->second;
  }
  return last_file;
}

Status StorageQueue::WriteHeaderAndBlock(
    base::span<const uint8_t> data,
    scoped_refptr<StorageQueue::SingleFile> file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  // Prepare header.
  RecordHeader header;
  // Assign sequence number.
  header.record_seq_number = next_seq_number_++;
  header.record_hash = 0;  // TODO(b/157940996): Add hash calculation
  header.record_size = data.size();
  // Write to the last file, update sequencing number.
  auto open_status = file->Open(/*read_only=*/false);
  if (!open_status.ok()) {
    return Status(error::ALREADY_EXISTS, "Cannot open file");
  }
  auto write_status = file->Append(base::make_span(
      reinterpret_cast<const uint8_t*>(&header), sizeof(header)));
  if (!write_status.ok()) {
    return Status(error::RESOURCE_EXHAUSTED, "Cannot write file");
  }
  if (data.size() > 0) {
    write_status = file->Append(data);
    if (!write_status.ok()) {
      return Status(error::RESOURCE_EXHAUSTED, "Cannot write file");
    }
    // Pad to the whole frame, if necessary.
    const size_t pad_size =
        GetPaddingToNextFrameSize(sizeof(header) + data.size());
    if (pad_size != FRAME_SIZE) {
      // TODO(b/157943388): Fill in with random bytes.
      uint8_t junk_bytes[FRAME_SIZE];
      memset(&junk_bytes[0], 0, FRAME_SIZE);
      write_status = file->Append(base::make_span(&junk_bytes[0], pad_size));
      if (!write_status.ok()) {
        return Status(error::RESOURCE_EXHAUSTED, "Cannot pad file");
      }
    }
  }
  return Status::StatusOK();
}

class StorageQueue::ReadContext : public TaskRunnerContext<Status> {
 public:
  ReadContext(std::unique_ptr<UploaderInterface> uploader,
              scoped_refptr<StorageQueue> storage_queue)
      : TaskRunnerContext<Status>(
            base::BindOnce(&UploaderInterface::Completed,
                           base::Unretained(uploader.get())),
            storage_queue->sequenced_task_runner_),
        uploader_(std::move(uploader)),
        storage_queue_(storage_queue) {
    DCHECK(storage_queue_);
    DCHECK(uploader_.get());
    DETACH_FROM_SEQUENCE(read_sequence_checker_);
  }

 private:
  // Context can only be deleted by calling Response method.
  ~ReadContext() override = default;

  void OnStart() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(read_sequence_checker_);
    seq_number_ = storage_queue_->first_seq_number_;
    // If the last file is not empty (has at least one record),
    // close it and create the new one, so that its records are
    // also included in the reading.
    const Status last_status = storage_queue_->SwitchLastFileIfNotEmpty();
    if (!last_status.ok()) {
      Response(last_status);
      return;
    }

    // Collect and set aside the files in the set that have data for the Upload.
    const uint64_t first_file_seq_number =
        storage_queue_->CollectFilesForUpload(seq_number_, &files_);
    if (files_.empty()) {
      Response(Status(error::OUT_OF_RANGE,
                      "Sequence number not found in StorageQueue."));
      return;
    }

    // Register with storage_queue, to make sure selected files are not removed.
    ++(storage_queue_->active_read_operations_);

    // The first file is the current file now, and we are at its start.
    current_file_ = files_.begin();
    current_pos_ = 0;
    // Read from it until the specified sequencing number is found.
    for (uint64_t seq_number = first_file_seq_number; seq_number < seq_number_;
         ++seq_number) {
      const auto blob = EnsureBlob(seq_number);
      if (!blob.ok()) {
        // File found to be corrupt.
        Response(blob.status());
        return;
      }
    }

    // seq_number_ blob is ready.
    const auto blob = EnsureBlob(seq_number_);
    if (!blob.ok()) {
      // File found to be corrupt.
      Response(blob.status());
      return;
    }
    CallCurrentRecord(blob.ValueOrDie());
  }

  void OnCompletion() override {
    // Unregister with storage_queue.
    if (!files_.empty()) {
      auto count = --(storage_queue_->active_read_operations_);
      DCHECK_GE(count, 0);
    }
  }

  // Makes a call to UploaderInterface instance provided by user, which can
  // place processing of the record on any thread(s). Once it returns, it will
  // schedule NextRecord to execute on the sequential thread runner of this
  // StorageQueue.
  void CallCurrentRecord(base::span<const uint8_t> blob) {
    uploader_->ProcessBlob(
        blob, base::BindOnce(&ReadContext::ScheduleNextRecord, this));
  }

  // Schedules NextRecord to execute on the StorageQueue sequential task runner.
  void ScheduleNextRecord(bool more_records) {
    Schedule(&ReadContext::NextRecord, this, more_records);
  }

  // If more records are expected, retrieves the next record (if present) and
  // sends for processing, or calls Response with error status. Otherwise, call
  // Response(OK).
  void NextRecord(bool more_records) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(read_sequence_checker_);
    if (!more_records) {
      Response(Status::StatusOK());  // Requested to stop reading.
      return;
    }
    auto blob = EnsureBlob(++seq_number_);
    if (blob.status().error_code() == error::OUT_OF_RANGE) {
      // Reached end of file, switch to the next one (if present).
      ++current_file_;
      if (current_file_ == files_.end()) {
        Response(Status::StatusOK());
        return;
      }
      current_pos_ = 0;
      blob = EnsureBlob(seq_number_);
    }
    if (!blob.ok()) {
      Response(blob.status());
      return;
    }
    CallCurrentRecord(blob.ValueOrDie());
  }

  StatusOr<base::span<const uint8_t>> EnsureBlob(uint64_t seq_number) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(read_sequence_checker_);
    // Read from the current file at the current offset.
    RETURN_IF_ERROR((*current_file_)->Open(/*read_only=*/true));
    auto read_result =
        (*current_file_)->Read(current_pos_, sizeof(RecordHeader));
    RETURN_IF_ERROR(read_result.status());
    auto header_data = read_result.ValueOrDie();
    if (header_data.empty()) {
      // No more blobs.
      return Status(error::OUT_OF_RANGE, "Reached end of data");
    }
    current_pos_ += header_data.size();
    if (header_data.size() != sizeof(RecordHeader)) {
      // File corrupt, header incomplete.
      return Status(error::INTERNAL,
                    base::StrCat({"File corrupt: ", (*current_file_)->name()}));
    }
    // Check the header match.
    const RecordHeader& header =
        *reinterpret_cast<const RecordHeader*>(header_data.data());
    if (header.record_seq_number != seq_number) {
      return Status(
          error::INTERNAL,
          base::StrCat({"File corrupt: ", (*current_file_)->name(),
                        " seq=", base::NumberToString(header.record_seq_number),
                        " expected=", base::NumberToString(seq_number)}));
    }
    // TODO(b/157940996): Add hash verification.
    // Read the record blob (align size to FRAME_SIZE).
    const size_t data_size = RoundUpToFrameSize(header.record_size);
    read_result = (*current_file_)->Read(current_pos_, data_size);
    RETURN_IF_ERROR(read_result.status());
    current_pos_ += read_result.ValueOrDie().size();
    if (read_result.ValueOrDie().size() != data_size) {
      // File corrupt, blob incomplete.
      return Status(
          error::INTERNAL,
          base::StrCat({"File corrupt: ", (*current_file_)->name(), " size=",
                        base::NumberToString(read_result.ValueOrDie().size()),
                        " expected=", base::NumberToString(data_size)}));
    }
    return read_result.ValueOrDie().first(header.record_size);
  }

  // Files that will be read (in order of sequence numbers).
  std::vector<scoped_refptr<SingleFile>> files_;
  uint64_t seq_number_ = 0;  // Sequencing number of the blob being read.
  uint32_t current_pos_;
  std::vector<scoped_refptr<SingleFile>>::iterator current_file_;
  const std::unique_ptr<UploaderInterface> uploader_;
  const scoped_refptr<StorageQueue> storage_queue_;

  SEQUENCE_CHECKER(read_sequence_checker_);
};

void StorageQueue::PeriodicUpload() {
  // Note: new uploader created every time PeriodicUpload is called.
  StatusOr<std::unique_ptr<UploaderInterface>> uploader =
      start_upload_cb_.Run();
  if (!uploader.ok()) {
    LOG(ERROR) << "Failed to provide the Uploader, status="
               << uploader.status();
    return;
  }
  Start<ReadContext>(std::move(uploader.ValueOrDie()), this);
}

class StorageQueue::WriteContext : public TaskRunnerContext<Status> {
 public:
  WriteContext(base::span<const uint8_t> data,
               base::OnceCallback<void(Status)> write_callback,
               scoped_refptr<StorageQueue> storage_queue)
      : TaskRunnerContext<Status>(std::move(write_callback),
                                  storage_queue->sequenced_task_runner_),
        storage_queue_(storage_queue),
        size_(data.size()) {
    DCHECK(storage_queue_);
    if (size_ > 0) {
      buffer_ = std::make_unique<uint8_t[]>(size_);
      memcpy(buffer_.get(), data.data(), size_);
    }
    DETACH_FROM_SEQUENCE(write_sequence_checker_);
  }

 private:
  // Context can only be deleted by calling Response method.
  ~WriteContext() override {
    // If no uploader is needed, we are done.
    if (!uploader_) {
      return;
    }

    // Otherwise initiate Upload right after writing
    // finished and respond back when reading Upload is done.
    // Note: new uploader created synchronously before scheduling Upload.
    Start<ReadContext>(std::move(uploader_), storage_queue_);
  }

  void OnStart() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(write_sequence_checker_);

    // Prepare uploader, if need to run it after Write.
    if (storage_queue_->options_.upload_period().is_zero()) {
      StatusOr<std::unique_ptr<UploaderInterface>> uploader =
          storage_queue_->start_upload_cb_.Run();
      if (uploader.ok()) {
        uploader_ = std::move(uploader.ValueOrDie());
      } else {
        LOG(ERROR) << "Failed to provide the Uploader, status="
                   << uploader.status();
      }
    }

    StatusOr<scoped_refptr<SingleFile>> assign_result =
        storage_queue_->AssignLastFile(size_);
    if (!assign_result.ok()) {
      Response(assign_result.status());
      return;
    }
    scoped_refptr<SingleFile> last_file = assign_result.ValueOrDie();

    // Write header and block.
    Status write_result = storage_queue_->WriteHeaderAndBlock(
        base::make_span(buffer_.get(), size_), std::move(last_file));
    if (!write_result.ok()) {
      Response(write_result);
      return;
    }

    Response(Status::StatusOK());
  }

  const scoped_refptr<StorageQueue> storage_queue_;

  uint64_t size_;
  std::unique_ptr<uint8_t[]> buffer_;

  // Upload provider (if any).
  std::unique_ptr<UploaderInterface> uploader_;

  SEQUENCE_CHECKER(write_sequence_checker_);
};

void StorageQueue::Write(base::span<const uint8_t> data,
                         base::OnceCallback<void(Status)> completion_cb) {
  Start<WriteContext>(data, std::move(completion_cb), this);
}

Status StorageQueue::SwitchLastFileIfNotEmpty() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  if (files_.empty()) {
    return Status(error::OUT_OF_RANGE,
                  "No files in the queue");  // No files in this queue yet.
  }
  if (files_.rbegin()->second->size() == 0) {
    return Status::StatusOK();  // Already empty.
  }
  files_.rbegin()->second->Close();
  auto insert_result = files_.emplace(
      next_seq_number_,
      base::MakeRefCounted<SingleFile>(
          options_.directory()
              .Append(options_.file_prefix())
              .AddExtensionASCII(base::NumberToString(next_seq_number_)),
          /*size=*/0));
  if (!insert_result.second) {
    return Status(error::ALREADY_EXISTS,
                  base::StrCat({"Sequence number already assigned: '",
                                base::NumberToString(next_seq_number_), "'"}));
  }
  return Status::StatusOK();
}

uint64_t StorageQueue::CollectFilesForUpload(
    uint64_t seq_number,
    std::vector<scoped_refptr<StorageQueue::SingleFile>>* files) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  uint64_t first_file_seq_number = seq_number;
  // Locate the first file based on sequencing number.
  auto file_it = files_.find(seq_number);
  if (file_it == files_.end()) {
    file_it = files_.upper_bound(seq_number);
    if (file_it != files_.begin()) {
      --file_it;
    }
  }
  // Create references to the files that will be uploaded.
  // Exclude the last file (still being written).
  for (; file_it != files_.end() &&
         file_it->second.get() != files_.rbegin()->second.get();
       ++file_it) {
    if (first_file_seq_number > file_it->first) {
      first_file_seq_number = file_it->first;
    }
    files->emplace_back(file_it->second);  // Adding reference.
  }
  return first_file_seq_number;
}

class StorageQueue::ConfirmContext : public TaskRunnerContext<Status> {
 public:
  ConfirmContext(uint64_t seq_number,
                 base::OnceCallback<void(Status)> end_callback,
                 scoped_refptr<StorageQueue> storage_queue)
      : TaskRunnerContext<Status>(std::move(end_callback),
                                  storage_queue->sequenced_task_runner_),
        seq_number_(seq_number),
        storage_queue_(storage_queue) {
    DCHECK(storage_queue_);
    DETACH_FROM_SEQUENCE(confirm_sequence_checker_);
  }

 private:
  // Context can only be deleted by calling Response method.
  ~ConfirmContext() override = default;

  void OnStart() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(confirm_sequence_checker_);
    Response(storage_queue_->RemoveUnusedFiles(seq_number_));
  }

  // Confirmed sequencing number.
  uint64_t seq_number_;

  const scoped_refptr<StorageQueue> storage_queue_;

  SEQUENCE_CHECKER(confirm_sequence_checker_);
};

void StorageQueue::Confirm(uint64_t seq_number,
                           base::OnceCallback<void(Status)> completion_cb) {
  Start<ConfirmContext>(seq_number, std::move(completion_cb), this);
}

Status StorageQueue::RemoveUnusedFiles(uint64_t seq_number) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  if (first_seq_number_ <= seq_number) {
    first_seq_number_ = seq_number + 1;
  }
  if (active_read_operations_ > 0) {
    // If there are read locks registered, bail out
    // (expect to remove unused files later).
    return Status::StatusOK();
  }
  // Remove all files with seq numbers below or equal only.
  // Note: files_ cannot be empty ever (there is always the current
  // file for writing).
  for (;;) {
    DCHECK(!files_.empty()) << "Empty storage queue";
    auto next_it = files_.begin();
    ++next_it;  // Need to consider the next file.
    if (next_it == files_.end()) {
      // We are on the last file, keep it.
      break;
    }
    if (next_it->first > seq_number + 1) {
      // Current file ends with (next_it->first - 1).
      // If it is seq_number >= (next_it->first - 1), we must keep it.
      break;
    }
    // Current file holds only numbers <= seq_number.
    // Delete it.
    files_.begin()->second->Close();
    if (files_.begin()->second->Delete().ok()) {
      files_.erase(files_.begin());
    }
  }
  // Even if there were errors, ignore them.
  return Status::StatusOK();
}

//
// SingleFile implementation
//
StorageQueue::SingleFile::SingleFile(const base::FilePath& filename,
                                     int64_t size)
    : filename_(filename), size_(size) {}

StorageQueue::SingleFile::~SingleFile() {
  handle_.reset();
}

Status StorageQueue::SingleFile::Open(bool read_only) {
  if (handle_) {
    DCHECK_EQ(is_readonly(), read_only);
    // TODO(b/157943192): Restart auto-closing timer.
    return Status::StatusOK();
  }
  handle_ = std::make_unique<base::File>(
      filename_, read_only
                     ? (base::File::FLAG_OPEN | base::File::FLAG_READ)
                     : (base::File::FLAG_CREATE | base::File::FLAG_APPEND));
  if (!handle_ || !handle_->IsValid()) {
    return Status(error::DATA_LOSS,
                  base::StrCat({"Cannot open file=", name(), " for ",
                                read_only ? "read" : "append"}));
  }
  is_readonly_ = read_only;
  if (!read_only) {
    int64_t file_size = handle_->GetLength();
    if (file_size < 0) {
      return Status(error::DATA_LOSS,
                    base::StrCat({"Cannot get size of file=", name()}));
    }
    size_ = static_cast<uint64_t>(file_size);
  }
  return Status::StatusOK();
}

void StorageQueue::SingleFile::Close() {
  if (!handle_) {
    // TODO(b/157943192): Restart auto-closing timer.
    return;
  }
  handle_.reset();
  is_readonly_ = base::nullopt;
  buffer_.reset();
}

Status StorageQueue::SingleFile::Delete() {
  DCHECK(!handle_);
  size_ = 0;
  if (!base::DeleteFile(filename_, /*recursive=*/false)) {
    return Status(error::DATA_LOSS,
                  base::StrCat({"Cannot delete file=", name()}));
  }
  return Status::StatusOK();
}

StatusOr<base::span<const uint8_t>> StorageQueue::SingleFile::Read(
    uint32_t pos,
    uint32_t size) {
  DCHECK(is_readonly());
  if (!handle_) {
    return Status(error::UNAVAILABLE, base::StrCat({"File not open ", name()}));
  }
  if (size > BUFFER_SIZE) {
    return Status(error::RESOURCE_EXHAUSTED, "Too much data to read");
  }
  // If no buffer yet, allocate.
  // TODO(b/157943192): Add buffer management - consider adding an UMA for
  // tracking the average + peak memory the Storage module is consuming.
  if (!buffer_) {
    buffer_ = std::make_unique<uint8_t[]>(BUFFER_SIZE);
    data_start_ = data_end_ = 0;
    file_position_ = 0;
  }
  // If file position does not match, reset buffer.
  if (pos != file_position_) {
    data_start_ = data_end_ = 0;
    file_position_ = pos;
  }
  // If expected data size does not fit into the buffer, move what's left to the
  // start.
  if (data_start_ + size > BUFFER_SIZE) {
    DCHECK_GT(data_start_, 0u);  // Cannot happen if 0.
    memmove(buffer_.get(), buffer_.get() + data_start_,
            data_end_ - data_start_);
    data_end_ -= data_start_;
    data_start_ = 0;
  }
  size_t actual_size = data_end_ - data_start_;
  while (actual_size < size) {
    // Read as much as possible.
    const int32_t result =
        handle_->Read(pos, reinterpret_cast<char*>(buffer_.get() + data_end_),
                      BUFFER_SIZE - data_end_);
    if (result < 0) {
      return Status(
          error::DATA_LOSS,
          base::StrCat({"File read error=",
                        handle_->ErrorToString(handle_->GetLastFileError()),
                        " ", name()}));
    }
    if (result == 0) {
      break;
    }
    pos += result;
    data_end_ += result;
    DCHECK_LE(data_end_, BUFFER_SIZE);
    actual_size += result;
  }
  if (actual_size > size) {
    actual_size = size;
  }
  // If nothing read, report end of file.
  if (actual_size == 0) {
    return Status(error::OUT_OF_RANGE, "End of file");
  }
  // Prepare span of actually loaded data.
  auto read_span = base::make_span(buffer_.get() + data_start_, actual_size);
  // Move start and file position to after that data.
  data_start_ += actual_size;
  file_position_ += actual_size;
  DCHECK_LE(data_start_, data_end_);
  // Return what has been loaded.
  return read_span;
}

StatusOr<uint32_t> StorageQueue::SingleFile::Append(
    base::span<const uint8_t> data) {
  DCHECK(!is_readonly());
  if (!handle_) {
    return Status(error::UNAVAILABLE, base::StrCat({"File not open ", name()}));
  }
  size_t actual_size = 0;
  while (data.size() > 0) {
    const int32_t result = handle_->WriteAtCurrentPos(
        reinterpret_cast<const char*>(data.data()), data.size());
    if (result < 0) {
      return Status(
          error::DATA_LOSS,
          base::StrCat({"File write error=",
                        handle_->ErrorToString(handle_->GetLastFileError()),
                        " ", name()}));
    }
    size_ += result;
    actual_size += result;
    data = data.subspan(result);  // Skip data that has been written.
  }
  return actual_size;
}

}  // namespace reporting
