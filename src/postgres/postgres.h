/*
 * Модуль хранения. Отвечает за запись и чтение данных в СУБД PostgreSQL
 */
#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>
#include <memory>
#include <string>
#include <vector>

#include "connection_pool.h"
#include "../players.h"

namespace postgres {

class UnitOfWork {
public:
    explicit UnitOfWork(ConnectionPool& pool)
        : pool_(pool) {}

    void SaveChampion(const players::Champion& result);
    std::vector<players::Champion> GetChampions(size_t start, size_t max_items);

private:
    ConnectionPool& pool_;
};

class AppRepoImpl : public players::ApplicationRepository {
public:
    explicit AppRepoImpl(ConnectionPool& pool)
                : unit_of_work_{pool} {}

    void Save(const players::Champion& result) override;
    std::vector<players::Champion> GetChampions(size_t start, size_t max_items) override;

private:
    UnitOfWork unit_of_work_;
};

class Database {
public:
    explicit Database(const std::string& db_url, size_t num_threads);

    AppRepoImpl& GetApplicationRepository() & {
        return app_repo_impl_;
    }

private:
    ConnectionPool pool_;
    AppRepoImpl app_repo_impl_{pool_};
};

} // namespace postgres