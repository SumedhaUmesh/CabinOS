#include "cabinos/state_store.hpp"

#include <sqlite3.h>

#include <string>

namespace cabinos {
namespace {

bool ExecSql(sqlite3* db, const char* sql, std::string* status_out) {
    char* err_msg = nullptr;
    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        *status_out = "SQLite exec failed: " + std::string(err_msg ? err_msg : "unknown");
        if (err_msg != nullptr) {
            sqlite3_free(err_msg);
        }
        return false;
    }
    return true;
}

}  // namespace

bool SqliteStateStore::Load(RuntimeSnapshot* snapshot_out, std::string* status_out) const {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) {
        *status_out = "Failed to open DB for load.";
        return false;
    }

    if (!ExecSql(db,
                 "CREATE TABLE IF NOT EXISTS runtime_state ("
                 "id INTEGER PRIMARY KEY CHECK(id = 1),"
                 "cabin_temperature_c INTEGER NOT NULL,"
                 "cabin_lights_level INTEGER NOT NULL,"
                 "hazards_on INTEGER NOT NULL,"
                 "battery_soc_percent INTEGER NOT NULL"
                 ");",
                 status_out)) {
        sqlite3_close(db);
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* query =
        "SELECT cabin_temperature_c, cabin_lights_level, hazards_on, battery_soc_percent "
        "FROM runtime_state WHERE id = 1;";
    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        *status_out = "Failed to prepare state load query.";
        sqlite3_close(db);
        return false;
    }

    const int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        snapshot_out->cabin_temperature_c = sqlite3_column_int(stmt, 0);
        snapshot_out->cabin_lights_level = sqlite3_column_int(stmt, 1);
        snapshot_out->hazards_on = (sqlite3_column_int(stmt, 2) != 0);
        snapshot_out->battery_soc_percent = sqlite3_column_int(stmt, 3);
        *status_out = "State restored from SQLite.";
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return true;
    }

    *status_out = "No persisted state found, using defaults.";
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return false;
}

bool SqliteStateStore::Save(const RuntimeSnapshot& snapshot, std::string* status_out) const {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) {
        *status_out = "Failed to open DB for save.";
        return false;
    }

    if (!ExecSql(db,
                 "CREATE TABLE IF NOT EXISTS runtime_state ("
                 "id INTEGER PRIMARY KEY CHECK(id = 1),"
                 "cabin_temperature_c INTEGER NOT NULL,"
                 "cabin_lights_level INTEGER NOT NULL,"
                 "hazards_on INTEGER NOT NULL,"
                 "battery_soc_percent INTEGER NOT NULL"
                 ");",
                 status_out)) {
        sqlite3_close(db);
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* upsert =
        "INSERT INTO runtime_state(id, cabin_temperature_c, cabin_lights_level, hazards_on, battery_soc_percent) "
        "VALUES (1, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET "
        "cabin_temperature_c=excluded.cabin_temperature_c, "
        "cabin_lights_level=excluded.cabin_lights_level, "
        "hazards_on=excluded.hazards_on, "
        "battery_soc_percent=excluded.battery_soc_percent;";

    if (sqlite3_prepare_v2(db, upsert, -1, &stmt, nullptr) != SQLITE_OK) {
        *status_out = "Failed to prepare state save query.";
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_int(stmt, 1, snapshot.cabin_temperature_c);
    sqlite3_bind_int(stmt, 2, snapshot.cabin_lights_level);
    sqlite3_bind_int(stmt, 3, snapshot.hazards_on ? 1 : 0);
    sqlite3_bind_int(stmt, 4, snapshot.battery_soc_percent);

    const int rc = sqlite3_step(stmt);
    const bool ok = (rc == SQLITE_DONE);
    *status_out = ok ? "State persisted to SQLite." : "Failed to persist state.";

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

}  // namespace cabinos
