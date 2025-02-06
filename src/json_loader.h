#pragma once

#include <boost/json.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "model.h"
#include "players.h"

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path);

template <typename BoostJSONType, typename ValueType>
ValueType ReadOptionalValue(const BoostJSONType& obj, std::string_view optional_str, ValueType default_value) {
    ValueType result = default_value;
    try {
        result = boost::json::value_to<ValueType>(obj.at(std::string(optional_str)));
    } catch (...) {
    }
    return result;
}

void LoadAndAddRoads(const boost::json::array& road_value, model::Map& map);
void LoadAndAddBuildings(const boost::json::array& building_value, model::Map& map);
void LoadAndAddOffices(const boost::json::array& office_value, model::Map& map);
void LoadAndAddLootTypes(const boost::json::array& loot_type_value, model::Map& map);
void LoadAndSetLootSettings(const boost::json::object& loot_settings, model::Game& game_obj);

// Функции для формирования ответа (api_handler)
std::string GetListOfMaps(const players::Application& app);
std::optional<std::string> GetMap(const model::Map::Id& map_id, const players::Application& app);

boost::json::array GetRoadsArray(const model::Map& map);
boost::json::array GetBuildingsArray(const model::Map& map);
boost::json::array GetOfficesArray(const model::Map& map);
boost::json::array GetLootTypesArray(const model::Map &map);

std::string GetPlayerAddedAnswer(std::string auth_token, size_t player_id);
std::string GetSessionPlayers(const players::Application::Dogs& dogs);

std::string MakeGameStateAnswer(const std::vector<players::GameState>& game_state,
                                const model::GameSession::LostObjects& lost_objects);
std::string DogDirectionToString(model::Direction direction);

std::string MakeChampionsAnswer(std::vector<players::Champion> champions);

// Разбор JSON запросов
struct JoinGame {
    std::string user_name;
    std::string map_id;
    bool error = false;
};
JoinGame LoadJSONJoinGame(std::string_view request_body);

std::optional<std::string> LoadActionMove(std::string_view request_body);
std::optional<double> LoadTimeDelta(std::string_view request_body); // значение времени в секундах

// Вывод сообщений об ошибках
std::string MakeErrorString(std::string&& err_code, std::string&& err_text);

// Функции для логирования - возвращают соответствующие строки для логирования (в формате JSON)
std::string GetLogServerStart(const std::string timestamp,
                            const std::string srv_address,
                            const int port);
std::string GetLogServerStop(const std::string timestamp,
                            const int return_code,
                            const std::string exception_what);
std::string GetLogRequest(const std::string timestamp,
                            const std::string client_address,
                            const std::string uri,
                            const std::string http_method);
std::string GetLogResponse(const std::string timestamp,
                            const std::string client_address,
                            const int response_time_msec,
                            const int response_code,
                            const std::string content_type);
std::string GetLogError(const std::string timestamp,
                            const int error_code,
                            const std::string error_text,
                            const std::string where);

}  // namespace json_loader
