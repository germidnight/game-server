#include "postgres.h"

#include <pqxx/pqxx>
#include <pqxx/result.hxx>
#include <pqxx/zview.hxx>

namespace postgres {
using namespace std::literals;
using pqxx::operator"" _zv;

/* ---------------------------- Database ---------------------------- */

Database::Database(const std::string &db_url, size_t num_threads)
                    : pool_{num_threads, [db_url]() {
                                    return std::make_shared<pqxx::connection>(db_url);
                                }} {
    auto conn = pool_.GetConnection();
    pqxx::work work{*conn};

    work.exec(R"(CREATE EXTENSION IF NOT EXISTS pgcrypto;)"_zv);
    work.exec(R"(
CREATE TABLE IF NOT EXISTS retired_players (
    id UUID PRIMARY KEY,
    name varchar(100) NOT NULL,
    score integer CHECK (score >= 0) NOT NULL,
    play_time_ms integer CHECK (play_time_ms >= 0) NOT NULL);
    )"_zv);
    work.exec(R"(
CREATE INDEX IF NOT EXISTS results_show
ON retired_players (score DESC, play_time_ms, name);
    )"_zv);
    work.commit();
}

/* ---------------------------- AppRepoImpl ---------------------------- */

void AppRepoImpl::Save(const players::Champion& result) {
    unit_of_work_.SaveChampion(result);
}
std::vector<players::Champion> AppRepoImpl::GetChampions(size_t start, size_t max_items) {
    return unit_of_work_.GetChampions(start, max_items);
}

/* ---------------------------- Unit Of Work ---------------------------- */

void UnitOfWork::SaveChampion(const players::Champion& result) {
    auto conn = pool_.GetConnection();
    pqxx::work work{*conn};
    work.exec_params(
        R"(
INSERT INTO retired_players (id, name, score, play_time_ms)
VALUES (gen_random_uuid(), $1, $2, $3);
        )"_zv, result.name, result.score, static_cast<int64_t>(result.play_time * 1000.)
    );
    work.commit();
}
std::vector<players::Champion> UnitOfWork::GetChampions(size_t start, size_t max_items){
    std::vector<players::Champion> champions;
    auto conn = pool_.GetConnection();
    pqxx::read_transaction r(*conn);
    std::string query_str = "\
SELECT name, score, play_time_ms \
FROM retired_players \
ORDER BY score DESC, play_time_ms, name \
LIMIT " + std::to_string(max_items) + " OFFSET " + std::to_string(start) + ";";

    for (auto [name, score, time_ms] :
            r.query<std::string, size_t, size_t>(pqxx::zview(query_str))) {
        champions.emplace_back(std::move(name),
                               score,
                               static_cast<double>(time_ms) / 1000.);
    }
    return champions;
}

} // namespace postgres