// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_STORAGE_QUEUE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_STORAGE_QUEUE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"

namespace reporting {

// Storage queue represents single queue of data to be collected and stored
// persistently. It allows to add whole data records as necessary,
// flush previously collected records and confirm records up to certain
// sequencing number to be eliminated.
class StorageQueue : public base::RefCountedThreadSafe<StorageQueue> {
 public:
  // Options class allowing to set parameters individually, e.g.:
  // StorageQueue::Create(Options()
  //                  .set_directory("/var/cache/reporting")
  //                  .set_file_prefix(FILE_PATH_LITERAL("p00000001"))
  //                  .set_total_size(128 * 1024LL * 1024LL),
  //                 callback);
  class Options {
   public:
    Options() = default;
    Options(const Options& options) = default;
    Options& operator=(const Options& options) = default;
    Options& set_directory(const base::FilePath& directory) {
      directory_ = directory;
      return *this;
    }
    Options& set_file_prefix(const base::FilePath::StringType& file_prefix) {
      file_prefix_ = file_prefix;
      return *this;
    }
    Options& set_single_file_size(uint64_t single_file_size) {
      single_file_size_ = single_file_size;
      return *this;
    }
    Options& set_total_size(uint64_t total_size) {
      total_size_ = total_size;
      return *this;
    }
    Options& set_upload_period(base::TimeDelta upload_period) {
      upload_period_ = upload_period;
      return *this;
    }
    const base::FilePath& directory() const { return directory_; }
    const base::FilePath::StringType& file_prefix() const {
      return file_prefix_;
    }
    uint64_t single_file_size() const { return single_file_size_; }
    uint64_t total_size() const { return total_size_; }
    base::TimeDelta upload_period() const { return upload_period_; }

   private:
    // Subdirectory of the Storage location assigned for this StorageQueue.
    base::FilePath directory_;
    // Prefix of data files assigned for this StorageQueue.
    base::FilePath::StringType file_prefix_;
    // Cut-off size of an individual file in the set.
    // When file exceeds this size, the new file is created
    // for further records. Note that each file must have at least
    // one record before it is closed, regardless of that record size.
    uint64_t single_file_size_ = 1 * 1024LL * 1024LL;  // 1 MiB
    // Cut-off total size of all files in the set.
    // When the storage queue exceeds this size, oldest records can be
    // dropped.
    uint64_t total_size_ = 256 * 256LL * 256LL;  // 256 MiB
    // Time period the data is uploaded with.
    // If 0, uploaded immediately after a new record is stored
    // (this setting is intended for the immediate priority).
    base::TimeDelta upload_period_;
  };

  // Interface for Upload, which must be implemented by an object returned by
  // |StartUpload| callback (see below).
  // Every time StorageQueue starts an upload (by timer or immediately after
  // Write) it uses this interface to hand available records over to the actual
  // uploader. StorageQueue takes ownership of it and automatically discards
  // after |Completed| returns.
  class UploaderInterface {
   public:
    virtual ~UploaderInterface() = default;

    // Asynchronously processes every record (e.g. serializes and adds to the
    // network message). Expects |processed_cb| to be called after the record
    // or error status has been processed, with true if next record needs to be
    // delivered and false if the Uploader should stop.
    virtual void ProcessBlob(StatusOr<base::span<const uint8_t>> data,
                             base::OnceCallback<void(bool)> processed_cb) = 0;

    // Finalizes the upload (e.g. sends the message to server and gets
    // response). Called always, regardless of whether there were errors.
    virtual void Completed(Status final_status) = 0;
  };

  // Callback type for UploadInterface provider for this queue.
  using StartUploadCb =
      base::RepeatingCallback<StatusOr<std::unique_ptr<UploaderInterface>>()>;

  // Creates StorageQueue instance with the specified options, and returns it
  // with the |completion_cb| callback. |start_upload_cb| is a factory callback
  // that instantiates UploaderInterface every time the queue starts uploading
  // records - periodically or immediately after Write (and in the near future -
  // upon explicit Flush request).
  static void Create(
      const Options& options,
      StartUploadCb start_upload_cb,
      base::OnceCallback<void(StatusOr<scoped_refptr<StorageQueue>>)>
          completion_cb);

  // Writes data blob into the StorageQueue (the last file of it) with the next
  // sequencing number assigned. The write is a non-blocking operation -
  // caller can "fire and forget" it (|completion_cb| allows to verify that
  // record has been successfully enqueued). If file is going to become too
  // large, it is closed and new file is created.
  // Helper methods: AssignLastFile, WriteHeaderAndBlock.
  void Write(base::span<const uint8_t> data,
             base::OnceCallback<void(Status)> completion_cb);

  // Confirms acceptance of the records up to |seq_number| (inclusively).
  // All records with sequencing numbers <= this one can be removed from
  // the StorageQueue, and can no longer be uploaded.
  // Helper methods: RemoveUnusedFiles.
  void Confirm(uint64_t seq_number,
               base::OnceCallback<void(Status)> completion_cb);

  StorageQueue(const StorageQueue& other) = delete;
  StorageQueue& operator=(const StorageQueue& other) = delete;

 protected:
  virtual ~StorageQueue();

 private:
  friend class base::RefCountedThreadSafe<StorageQueue>;

  // Private data structures for Read and Write (need access to the private
  // StorageQueue fields).
  class WriteContext;
  class ReadContext;
  class ConfirmContext;

  // Private envelope class for single file in a StorageQueue.
  class SingleFile : public base::RefCountedThreadSafe<SingleFile> {
   public:
    SingleFile(const base::FilePath& filename, int64_t size);

    Status Open(bool read_only);  // No-op if already opened.
    void Close();                 // No-op if not opened.

    Status Delete();

    // Attempts to read |size| bytes from position |pos| and returns
    // span of data that were actually read (no more than |size|).
    // End of file is indicated by empty span.
    StatusOr<base::span<const uint8_t>> Read(uint32_t pos, uint32_t size);

    // Appends data to the file.
    StatusOr<uint32_t> Append(base::span<const uint8_t> data);

    bool is_opened() const { return handle_.get() != nullptr; }
    bool is_readonly() const {
      DCHECK(is_opened());
      return is_readonly_.value();
    }
    uint64_t size() const { return size_; }
    std::string name() const { return filename_.MaybeAsASCII(); }

   protected:
    virtual ~SingleFile();

   private:
    friend class base::RefCountedThreadSafe<SingleFile>;

    // Flag (valid for opened file only): true if file was opened for reading
    // only, false otherwise.
    base::Optional<bool> is_readonly_;

    const base::FilePath filename_;  // relative to the StorageQueue directory
    uint64_t size_ = 0;  // tracked internally rather than by filesystem

    std::unique_ptr<base::File> handle_;  // Set only when opened/created.

    // When reading the file, this is the buffer and data positions.
    // If the data is read sequentially, buffered portions are reused
    // improving performance. When the sequential order is broken (e.g.
    // we start reading the same file in parallel from different position),
    // the buffer is reset.
    size_t data_start_ = 0;
    size_t data_end_ = 0;
    uint64_t file_position_ = 0;
    std::unique_ptr<uint8_t[]> buffer_;
  };

  // Private constructor, to be called by Create factory method only.
  StorageQueue(const Options& options, StartUploadCb start_upload_cb);

  // Initializes the object by enumerating files in the assigned directory
  // and determines the sequencing information of the last record.
  // Must be called once and only once after construction.
  // Returns OK or error status, if anything failed to initialize.
  // Called once, during initialization. Helper methods: EnumerateDataFiles,
  // ScanLastFile.
  Status Init();

  // Periodically uploads previously stored but not confirmed records.
  // Starts by calling |start_upload_cb_| that instantiates |UploaderInterface
  // uploader|. Then repeatedly reads data blob(s) one by one from the
  // StorageQueue starting from |first_seq_number_|, handing each one over to
  // |uploader|->ProcessBlob (keeping ownership of the buffer) and resuming
  // after result callback returns 'true'. Only files that have been closed are
  // included in reading; |Upload| makes sure to close the last writeable file
  // and create a new one before starting to send records to the |uploader|. If
  // the monotonic order of sequencing is broken, INTERNAL error Status is
  // reported. |Upload| can be stopped after any record by returning 'false' to
  // |processed_cb| callback - in that case |Upload| will behave as if the end
  // of data has been reached. While one or more |Upload|s are active, files can
  // be added to the StorageQueue but cannot be deleted. If processing of the
  // blob takes significant time, |uploader| implementation should be offset to
  // another thread to avoid locking StorageQueue.
  // Called by timer. Helper methods: SwitchLastFileIfNotEmpty,
  // CollectFilesForUpload.
  void PeriodicUpload();

  // Helper method for Init(): enumerates all data files in the directory.
  // Valid file names are <prefix>.<seq_number>, any other names are ignored.
  Status EnumerateDataFiles();

  // Helper method for Init(): scans the last file in StorageQueue, if there are
  // files at all, and learns the latest sequencing number. Otherwise (if there
  // are no files) sets it to 0.
  Status ScanLastFile();

  // Helper method for Write(): increments sequencing number and assigns last
  // file to place record in. |size| parameter indicates the size of data that
  // comprise the record expected to be appended; if appending the record will
  // make the file too large, the current last file will be closed, and a new
  // file will be created and assigned to be the last one.
  StatusOr<scoped_refptr<SingleFile>> AssignLastFile(size_t size);

  // Helper method for Write(): composes record header and writes it to the
  // file, followed by data.
  Status WriteHeaderAndBlock(base::span<const uint8_t> data,
                             scoped_refptr<SingleFile> file);

  // Helper method for Upload: if the last file is not empty (has at least one
  // record), close it and create the new one, so that its records are also
  // included in the reading.
  Status SwitchLastFileIfNotEmpty();

  // Helper method for Upload: collects and sets aside |files| in the
  // StorageQueue that have data for the Upload (all files that have records
  // with sequence numbers equal or higher than |seq_number|). Returns sequence
  // number the first file actually starts from (lower or equal to
  // |seq_number|).
  uint64_t CollectFilesForUpload(
      uint64_t seq_number,
      std::vector<scoped_refptr<SingleFile>>* files) const;

  // Helper method for Confirm: Removes files that only have records with seq
  // numbers below or equal to |seq_number|.
  Status RemoveUnusedFiles(uint64_t seq_number);

  // Immutable options, stored at the time of creation.
  const Options options_;

  // Next sequencing number to store (not assigned yet).
  uint64_t next_seq_number_ = 0;

  // First unconfirmed sequencing number (no records with lower
  // sequencing number are guaranteed to exist in store).
  uint64_t first_seq_number_ = 0;

  // Ordered map of the files by ascending sequence number.
  std::map<uint64_t, scoped_refptr<SingleFile>> files_;

  // Counter of the Read operations. When not 0, none of the files_ can be
  // deleted. Incremented by Upload context OnStart(), decremented by
  // destructor.
  int32_t active_read_operations_ = 0;

  // Upload timer (active only if options_.upload_period() is not 0).
  base::RepeatingTimer upload_timer_;

  // Upload provider callback.
  const StartUploadCb start_upload_cb_;

  // Sequential task runner for all activities in this StorageQueue.
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  SEQUENCE_CHECKER(storage_queue_sequence_checker_);
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_STORAGE_QUEUE_H_
