#pragma once

#include <string>

#include "cabinos/service_broker.hpp"

namespace cabinos {

class SqliteStateStore {
public:
    explicit SqliteStateStore(std::string db_path) : db_path_(std::move(db_path)) {}

    bool Load(RuntimeSnapshot* snapshot_out, std::string* status_out) const;
    bool Save(const RuntimeSnapshot& snapshot, std::string* status_out) const;

private:
    std::string db_path_;
};

}  // namespace cabinos
