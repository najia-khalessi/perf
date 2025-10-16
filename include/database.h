#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include "buffer.h"

sqlite3* db_open(const char* db_name);
int db_create_schema(sqlite3* db);
void db_close(sqlite3* db);
int db_insert_sample(sqlite3* db, int pid, int tid, const char* stack);
int db_insert_batch(sqlite3* db, struct buffer_entry* entries, int count);

#endif // DATABASE_H