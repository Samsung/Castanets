// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_DATABASE_H_
#define CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_DATABASE_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "net/cookies/canonical_cookie.h"
#include "sql/database.h"
#include "sql/init_status.h"
#include "sql/test/test_helpers.h"
#include "url/gurl.h"

// Provides the backend SQLite storage to support access context auditing. This
// requires storing information associating individual client-side storage API
// accesses (e.g. cookies, indexedDBs, etc.) with the top level frame origins
// at the time of their access.
class AccessContextAuditDatabase
    : public base::RefCountedThreadSafe<AccessContextAuditDatabase> {
 public:
  // All client-side storage API types supported by the database.
  enum class StorageAPIType : int {
    kCookie = 0,
    kLocalStorage = 1,
    kSessionStorage,
    kFileSystem,
    kWebDatabase,
    kServiceWorker,
    kCacheStorage,
    kIndexedDB,
    kAppCache,
  };

  // An individual record of a Storage API access, associating the individual
  // API usage with a top level frame origin.
  struct AccessRecord {
    AccessRecord(const GURL& top_frame_origin,
                 const std::string& name,
                 const std::string& domain,
                 const std::string& path,
                 const base::Time& last_access_time);
    AccessRecord(const GURL& top_frame_origin,
                 const StorageAPIType& type,
                 const GURL& origin,
                 const base::Time& last_access_time);
    AccessRecord(const AccessRecord& other);
    ~AccessRecord();
    GURL top_frame_origin;
    StorageAPIType type;

    // Identifies a canonical cookie, only used when |type| is kCookie.
    std::string name;
    std::string domain;
    std::string path;

    // Identifies an origin-keyed storage API, used when |type| is NOT kCookie.
    GURL origin;

    base::Time last_access_time;
  };

  explicit AccessContextAuditDatabase(
      const base::FilePath& path_to_database_dir);

  // Initialises internal database. Must be called prior to any other usage.
  void Init();

  // Persists the provided list of |records| in the database.
  void AddRecords(const std::vector<AccessRecord>& records);

  // Returns all entries in the database. No ordering is enforced.
  std::vector<AccessRecord> GetAllRecords();

  // Removes a record from the database and from future calls to GetAllRecords.
  void RemoveRecord(const AccessRecord& record);

  // Removes all records that match the provided cookie details.
  void RemoveAllRecordsForCookie(const std::string& name,
                                 const std::string& domain,
                                 const std::string& path);

  // Remove all records of access to |origin|'s storage API of |type|.
  void RemoveAllRecordsForOriginStorage(const GURL& origin,
                                        StorageAPIType type);

  // Removes all records for cookie domains and API origins that match session
  // only entries in |settings|
  void RemoveSessionOnlyRecords(
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      const ContentSettingsForOneType& content_settings);

 protected:
  virtual ~AccessContextAuditDatabase() = default;

 private:
  friend class base::RefCountedThreadSafe<AccessContextAuditDatabase>;
  bool InitializeSchema();

  sql::Database db_;
  base::FilePath db_file_path_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_DATABASE_H_
