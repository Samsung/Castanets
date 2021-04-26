// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/access_context_audit_database.h"

#include "base/logging.h"
#include "sql/database.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace {

const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("AccessContextAudit");
const char kCookieTableName[] = "cookies";
const char kStorageAPITableName[] = "originStorageAPIs";

// Callback that is fired upon an SQLite error, attempts to automatically
// recover the database if it appears possible to do so.
// TODO(crbug.com/1087272): Remove duplication of this function in the codebase.
void DatabaseErrorCallback(sql::Database* db,
                           const base::FilePath& db_path,
                           int extended_error,
                           sql::Statement* stmt) {
  if (sql::Recovery::ShouldRecover(extended_error)) {
    // Prevent reentrant calls.
    db->reset_error_callback();

    // After this call, the |db| handle is poisoned so that future calls will
    // return errors until the handle is re-opened.
    sql::Recovery::RecoverDatabase(db, db_path);

    // The DLOG(WARNING) below is intended to draw immediate attention to errors
    // in newly-written code.  Database corruption is generally a result of OS
    // or hardware issues, not coding errors at the client level, so displaying
    // the error would probably lead to confusion.  The ignored call signals the
    // test-expectation framework that the error was handled.
    ignore_result(sql::Database::IsExpectedSqliteError(extended_error));
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error))
    DLOG(FATAL) << db->GetErrorMessage();
}

}  // namespace

AccessContextAuditDatabase::AccessRecord::AccessRecord(
    const GURL& top_frame_origin,
    const std::string& name,
    const std::string& domain,
    const std::string& path,
    const base::Time& last_access_time)
    : top_frame_origin(top_frame_origin),
      type(StorageAPIType::kCookie),
      name(name),
      domain(domain),
      path(path),
      last_access_time(last_access_time) {}

AccessContextAuditDatabase::AccessRecord::AccessRecord(
    const GURL& top_frame_origin,
    const StorageAPIType& type,
    const GURL& origin,
    const base::Time& last_access_time)
    : top_frame_origin(top_frame_origin),
      type(type),
      origin(origin),
      last_access_time(last_access_time) {
  DCHECK(type != StorageAPIType::kCookie);
}

AccessContextAuditDatabase::AccessRecord::AccessRecord(
    const AccessRecord& other) = default;

AccessContextAuditDatabase::AccessRecord::~AccessRecord() = default;

AccessContextAuditDatabase::AccessContextAuditDatabase(
    const base::FilePath& path_to_database_dir)
    : db_file_path_(path_to_database_dir.Append(kDatabaseName)) {}

void AccessContextAuditDatabase::Init() {
  db_.set_histogram_tag("Access Context Audit");

  db_.set_error_callback(
      base::BindRepeating(&DatabaseErrorCallback, &db_, db_file_path_));

  // Cache values generated assuming ~5000 individual pieces of client storage
  // API data, each accessed in an average of 3 different contexts (complete
  // speculation, most will be 1, some will be >50), with an average of 40bytes
  // per audit entry.
  // TODO(crbug.com/1083384): Revist these numbers.
  db_.set_page_size(4096);
  db_.set_cache_size(128);

  db_.set_exclusive_locking();

  if (db_.Open(db_file_path_))
    InitializeSchema();
}

bool AccessContextAuditDatabase::InitializeSchema() {
  std::string create_table;
  create_table.append("CREATE TABLE IF NOT EXISTS ");
  create_table.append(kCookieTableName);
  create_table.append(
      "(top_frame_origin TEXT NOT NULL,"
      "name TEXT NOT NULL,"
      "domain TEXT NOT NULL,"
      "path TEXT NOT NULL,"
      "access_utc INTEGER NOT NULL,"
      "PRIMARY KEY (top_frame_origin, name, domain, path))");

  if (!db_.Execute(create_table.c_str()))
    return false;

  create_table.clear();
  create_table.append("CREATE TABLE IF NOT EXISTS ");
  create_table.append(kStorageAPITableName);
  create_table.append(
      "(top_frame_origin TEXT NOT NULL,"
      "type INTEGER NOT NULL,"
      "origin TEXT NOT NULL,"
      "access_utc INTEGER NOT NULL,"
      "PRIMARY KEY (top_frame_origin, origin, type))");

  return db_.Execute(create_table.c_str());
}

void AccessContextAuditDatabase::AddRecords(
    const std::vector<AccessContextAuditDatabase::AccessRecord>& records) {
  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  // Create both insert statements ahead of iterating over records. These are
  // highly likely to both be used, and should be in the statement cache.
  std::string insert;
  insert.append("INSERT OR REPLACE INTO ");
  insert.append(kCookieTableName);
  insert.append(
      "(top_frame_origin, name, domain, path, access_utc) "
      "VALUES (?, ?, ?, ?, ?)");
  sql::Statement insert_cookie(
      db_.GetCachedStatement(SQL_FROM_HERE, insert.c_str()));

  insert.clear();
  insert.append("INSERT OR REPLACE INTO ");
  insert.append(kStorageAPITableName);
  insert.append(
      "(top_frame_origin, type, origin, access_utc) "
      "VALUES (?, ?, ?, ?)");
  sql::Statement insert_storage_api(
      db_.GetCachedStatement(SQL_FROM_HERE, insert.c_str()));

  for (const auto& record : records) {
    if (record.type == StorageAPIType::kCookie) {
      insert_cookie.BindString(0, record.top_frame_origin.GetOrigin().spec());
      insert_cookie.BindString(1, record.name);
      insert_cookie.BindString(2, record.domain);
      insert_cookie.BindString(3, record.path);
      insert_cookie.BindInt64(
          4,
          record.last_access_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

      if (!insert_cookie.Run())
        return;

      insert_cookie.Reset(true);
    } else {
      insert_storage_api.BindString(0,
                                    record.top_frame_origin.GetOrigin().spec());
      insert_storage_api.BindInt(1, static_cast<int>(record.type));
      insert_storage_api.BindString(2, record.origin.GetOrigin().spec());
      insert_storage_api.BindInt64(
          3,
          record.last_access_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

      if (!insert_storage_api.Run())
        return;

      insert_storage_api.Reset(true);
    }
  }

  transaction.Commit();
}

void AccessContextAuditDatabase::RemoveRecord(const AccessRecord& record) {
  sql::Statement remove_statement;

  std::string remove;
  remove.append("DELETE FROM ");
  if (record.type == StorageAPIType::kCookie) {
    remove.append(kCookieTableName);
    remove.append(
        " WHERE top_frame_origin = ? AND name = ? AND domain = ? AND path = ?");
    remove_statement.Assign(
        db_.GetCachedStatement(SQL_FROM_HERE, remove.c_str()));
    remove_statement.BindString(0, record.top_frame_origin.GetOrigin().spec());
    remove_statement.BindString(1, record.name);
    remove_statement.BindString(2, record.domain);
    remove_statement.BindString(3, record.path);
  } else {
    remove.append(kStorageAPITableName);
    remove.append(" WHERE top_frame_origin = ? AND type = ? AND origin = ?");
    remove_statement.Assign(
        db_.GetCachedStatement(SQL_FROM_HERE, remove.c_str()));
    remove_statement.BindString(0, record.top_frame_origin.GetOrigin().spec());
    remove_statement.BindInt(1, static_cast<int>(record.type));
    remove_statement.BindString(2, record.origin.GetOrigin().spec());
  }
  remove_statement.Run();
}

void AccessContextAuditDatabase::RemoveSessionOnlyRecords(
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    const ContentSettingsForOneType& content_settings) {
  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  // Extract the set of all domains from the cookies table.
  std::string select = "SELECT DISTINCT domain FROM ";
  select.append(kCookieTableName);
  sql::Statement select_cookie_domains(
      db_.GetCachedStatement(SQL_FROM_HERE, select.c_str()));

  std::vector<std::string> cookie_domains;
  while (select_cookie_domains.Step()) {
    cookie_domains.emplace_back(select_cookie_domains.ColumnString(0));
  }

  // Extract the set of all origins from the storage API table.
  select = "SELECT DISTINCT origin FROM ";
  select.append(kStorageAPITableName);
  sql::Statement select_storage_origins(
      db_.GetCachedStatement(SQL_FROM_HERE, select.c_str()));

  std::vector<GURL> storage_origins;
  while (select_storage_origins.Step()) {
    storage_origins.emplace_back(GURL(select_storage_origins.ColumnString(0)));
  }

  // Remove records for all cookie domains and storage origins for which the
  // provided settings indicate should be cleared on exit.
  std::string remove = "DELETE FROM ";
  remove.append(kCookieTableName);
  remove.append(" WHERE domain = ?");
  sql::Statement remove_cookies(
      db_.GetCachedStatement(SQL_FROM_HERE, remove.c_str()));

  for (const auto& domain : cookie_domains) {
    if (!cookie_settings->ShouldDeleteCookieOnExit(content_settings, domain,
                                                   true) &&
        !cookie_settings->ShouldDeleteCookieOnExit(content_settings, domain,
                                                   false)) {
      continue;
    }

    remove_cookies.BindString(0, domain);
    if (!remove_cookies.Run())
      return;
    remove_cookies.Reset(true);
  }

  remove = "DELETE FROM ";
  remove.append(kStorageAPITableName);
  remove.append(" WHERE origin = ?");
  sql::Statement remove_storage_apis(
      db_.GetCachedStatement(SQL_FROM_HERE, remove.c_str()));

  for (const auto& origin : storage_origins) {
    // TODO(crbug.com/1099164): Rename IsCookieSessionOnly to better convey
    //                          its actual functionality.
    if (!cookie_settings->IsCookieSessionOnly(origin))
      continue;

    remove_storage_apis.BindString(0, origin.spec());
    if (!remove_storage_apis.Run())
      return;
    remove_storage_apis.Reset(true);
  }

  transaction.Commit();
}

void AccessContextAuditDatabase::RemoveAllRecordsForCookie(
    const std::string& name,
    const std::string& domain,
    const std::string& path) {
  std::string remove;
  remove.append("DELETE FROM ");
  remove.append(kCookieTableName);
  remove.append(" WHERE name = ? AND domain = ? AND path = ?");
  sql::Statement remove_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, remove.c_str()));
  remove_statement.BindString(0, name);
  remove_statement.BindString(1, domain);
  remove_statement.BindString(2, path);
  remove_statement.Run();
}

void AccessContextAuditDatabase::RemoveAllRecordsForOriginStorage(
    const GURL& origin,
    StorageAPIType type) {
  std::string remove;
  remove.append("DELETE FROM ");
  remove.append(kStorageAPITableName);
  remove.append(" WHERE origin = ? AND type = ?");
  sql::Statement remove_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, remove.c_str()));
  remove_statement.BindString(0, origin.GetOrigin().spec());
  remove_statement.BindInt(1, static_cast<int>(type));
  remove_statement.Run();
}

std::vector<AccessContextAuditDatabase::AccessRecord>
AccessContextAuditDatabase::GetAllRecords() {
  std::vector<AccessContextAuditDatabase::AccessRecord> records;

  std::string select;
  select.append(
      "SELECT top_frame_origin, name, domain, path, access_utc FROM ");
  select.append(kCookieTableName);
  sql::Statement select_cookies(
      db_.GetCachedStatement(SQL_FROM_HERE, select.c_str()));

  while (select_cookies.Step()) {
    records.emplace_back(
        GURL(select_cookies.ColumnString(0)), select_cookies.ColumnString(1),
        select_cookies.ColumnString(2), select_cookies.ColumnString(3),
        base::Time::FromDeltaSinceWindowsEpoch(
            base::TimeDelta::FromMicroseconds(select_cookies.ColumnInt64(4))));
  }

  select.clear();
  select.append("SELECT top_frame_origin, type, origin, access_utc FROM ");
  select.append(kStorageAPITableName);
  sql::Statement select_storage_api(
      db_.GetCachedStatement(SQL_FROM_HERE, select.c_str()));

  while (select_storage_api.Step()) {
    records.emplace_back(
        GURL(select_storage_api.ColumnString(0)),
        static_cast<StorageAPIType>(select_storage_api.ColumnInt(1)),
        GURL(select_storage_api.ColumnString(2)),
        base::Time::FromDeltaSinceWindowsEpoch(
            base::TimeDelta::FromMicroseconds(
                select_storage_api.ColumnInt64(3))));
  }

  return records;
}
