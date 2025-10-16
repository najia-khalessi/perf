#include <stdio.h>
#include <time.h>
#include "../../include/database.h"

int db_create_schema(sqlite3* db) {
    char* err_msg = 0;
    // 确保不存在同名视图导致无法创建表的情况
    const char* drop_view_raw_samples = "DROP TABLE IF EXISTS raw_samples;";
    int rc = sqlite3_exec(db, drop_view_raw_samples, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error dropping raw_samples table: %s\n", err_msg);
        sqlite3_free(err_msg);
        return rc;
    }
    const char* drop_samples = "DROP TABLE IF EXISTS samples;";
    rc = sqlite3_exec(db, drop_samples, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error dropping samples table: %s\n", err_msg);
        sqlite3_free(err_msg);
        return rc;
    }
    const char* sql_raw_samples =
        "CREATE TABLE IF NOT EXISTS raw_samples (" 
        "id INTEGER PRIMARY KEY AUTOINCREMENT," 
        "timestamp INTEGER NOT NULL," 
        "pid INTEGER NOT NULL," 
        "tid INTEGER NOT NULL," 
        "callstack TEXT NOT NULL" 
        ");";
    
    rc = sqlite3_exec(db, sql_raw_samples, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error creating raw_samples table: %s\n", err_msg);
        sqlite3_free(err_msg);
        return rc;
    }

    const char* sql_index = "CREATE INDEX IF NOT EXISTS idx_timestamp ON raw_samples (timestamp);";
    rc = sqlite3_exec(db, sql_index, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error creating index: %s\n", err_msg);
        sqlite3_free(err_msg);
        return rc;
    }

    const char* sql_samples =
        "CREATE TABLE IF NOT EXISTS samples("
        "id INTEGER PRIMARY KEY AUTOINCREMENT," 
        "timestamp INTEGER NOT NULL," 
        "pid INTEGER NOT NULL," 
        "tid INTEGER NOT NULL," 
        "stack TEXT NOT NULL" 
        ");";
    rc = sqlite3_exec(db, sql_samples, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error creating samples table: %s\n", err_msg);
        sqlite3_free(err_msg);
        return rc;
    }

    return SQLITE_OK;
}

sqlite3* db_open(const char* db_name) {
    sqlite3* db;
    int rc = sqlite3_open(db_name, &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return NULL;
    }

    char* err_msg = 0;
    const char* wal_sql = "PRAGMA journal_mode=WAL;";
    rc = sqlite3_exec(db, wal_sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to enable WAL mode: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return NULL;
    }

    if (db_create_schema(db) != SQLITE_OK) {
        sqlite3_close(db);
        return NULL;
    }

    return db;
}

void db_close(sqlite3* db) {
    if (db) {
        // 强制执行 WAL checkpoint，确保所有数据都写入主数据库文件
        // TRUNCATE 是一个温和的模式，但 FULL 或 RESTART 更为彻底
        char* err_msg = 0;
        int rc = sqlite3_exec(db, "PRAGMA wal_checkpoint(FULL);", 0, 0, &err_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Warning: Failed to perform WAL checkpoint: %s\n", err_msg);
            sqlite3_free(err_msg);
        }
        sqlite3_close(db);
    }
}

int db_insert_sample(sqlite3* db, int pid, int tid, const char* stack) {
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO samples (timestamp, pid, tid, stack) VALUES (?, ?, ?, ?);";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return rc;
    }

    sqlite3_bind_int(stmt, 1, time(NULL));
    sqlite3_bind_int(stmt, 2, pid);
    sqlite3_bind_int(stmt, 3, tid);
    sqlite3_bind_text(stmt, 4, stack, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Execution failed: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int db_insert_batch(sqlite3* db, struct buffer_entry* entries, int count) {
    char* err_msg = 0;
    int rc = sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to begin transaction: %s\n", err_msg);
        sqlite3_free(err_msg);
        return rc;
    }

    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO raw_samples (timestamp, pid, tid, callstack) VALUES (?, ?, ?, ?);";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
        return rc;
    }

    for (int i = 0; i < count; i++) {
        sqlite3_bind_int64(stmt, 1, entries[i].timestamp);
        sqlite3_bind_int(stmt, 2, entries[i].pid);
        sqlite3_bind_int(stmt, 3, entries[i].tid);
        sqlite3_bind_text(stmt, 4, entries[i].stack, -1, SQLITE_TRANSIENT);
        
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "Execution failed: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
            return rc;
        }
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);
    rc = sqlite3_exec(db, "COMMIT;", 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to commit transaction: %s\n", err_msg);
        sqlite3_free(err_msg);
        return rc;
    }

    return SQLITE_OK;
}
