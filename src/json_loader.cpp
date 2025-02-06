#include "json_loader.h"

#include <fstream>
#include <memory>
#include <utility>

namespace json_loader {

/* Открыть файл настроек игры и разобрать в объект boost::json
 * Создать объект игры и загрузить модель игры из файла:
 *      1) получить настройки игры (в том числе необязательные)
 *      2) получить массив карт
 *      3) в цикле массива карт:
 *      3.1) создать карту model::Map
 *      3.2) заполнить карту объектами: дорогами, офисами, зданиями, типами вещей
 *      3.3) добавить карту в игру */
model::Game LoadGame(const std::filesystem::path& json_path) {
    const std::string maps_str              = "maps";
    const std::string id_str                = "id";
    const std::string name_str              = "name";
    const std::string roads_str             = "roads";
    const std::string buildings_str         = "buildings";
    const std::string offices_str           = "offices";
    const std::string loot_gen_config_str   = "lootGeneratorConfig";
    const std::string loot_types_str        = "lootTypes";
    // Опциональные параметры:
    const std::string defaultdogspeed_str   = "defaultDogSpeed";
    const std::string dogspeed_str          = "dogSpeed";
    const std::string defaultbagcapacity_str = "defaultBagCapacity";
    const std::string bagcapacity_str       = "bagCapacity";
    const std::string dog_retirement_time_str = "dogRetirementTime";

    // Загрузить содержимое файла json_path, в виде строки
    std::ifstream json_file(json_path, std::ios::in);
    if (!json_file.is_open()) {
        throw std::runtime_error("Can't open file:" + json_path.string());
    }
    std::string json_str{std::istreambuf_iterator<char>(json_file), std::istreambuf_iterator<char>()};
    // Распарсить строку как JSON, используя boost::json::parse
    auto config_data = boost::json::parse(std::forward<std::string>(json_str));

    model::Game game;

    double default_dog_speed = ReadOptionalValue(config_data, defaultdogspeed_str, static_cast<double>(1.));
    size_t default_bag_capacity = ReadOptionalValue(config_data, defaultbagcapacity_str, static_cast<size_t>(3));

    auto loot_settings_obj = config_data.at(loot_gen_config_str).as_object();
    LoadAndSetLootSettings(loot_settings_obj, game);

    game.SetDogRetirementTime(ReadOptionalValue(config_data, dog_retirement_time_str, static_cast<double>(60.)));

    auto maps_arr = config_data.at(maps_str).as_array();
    for (auto it_map = maps_arr.begin(); it_map != maps_arr.end(); ++it_map) {
        double dog_speed = ReadOptionalValue(*it_map, dogspeed_str, default_dog_speed);
        size_t bag_capacity = ReadOptionalValue(*it_map, bagcapacity_str, default_bag_capacity);

        model::Map map(model::Map::Id(std::string{it_map->at(id_str).as_string()}),
                        std::string{it_map->at(name_str).as_string()},
                        dog_speed,
                        bag_capacity);
        auto loot_types_arr = it_map->at(loot_types_str).as_array();
        LoadAndAddLootTypes(loot_types_arr, map);

        auto roads_arr = it_map->at(roads_str).as_array();
        LoadAndAddRoads(roads_arr, map);

        auto buildings_arr = it_map->at(buildings_str).as_array();
        LoadAndAddBuildings(buildings_arr, map);

        auto offices_arr = it_map->at(offices_str).as_array();
        LoadAndAddOffices(offices_arr, map);

        game.AddMap(map);
    }

    return game;
}

void LoadAndSetLootSettings(const boost::json::object& loot_settings, model::Game& game_obj) {
    using namespace std::chrono_literals;
    const std::string ls_period_str = "period";
    const std::string ls_probability_str = "probability";

    double time_period = loot_settings.at(ls_period_str).as_double();
    game_obj.GetLootGenerator().SetLootPeriod(std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::duration<double, std::milli>{time_period * 1s}));
    game_obj.GetLootGenerator().SetLootProbability(loot_settings.at(ls_probability_str).as_double());
}

/* Координаты дорог перед внесением упорядочиваем:
 * начальная координата дороги должна всегда быть меньше конечной */
void LoadAndAddRoads(const boost::json::array& roads_arr, model::Map& map) {
    const std::string road_x0_str = "x0";
    const std::string road_y0_str = "y0";
    const std::string road_x1_str = "x1";
    const std::string road_y1_str = "y1";

    for (auto it_road = roads_arr.begin(); it_road != roads_arr.end(); ++it_road) {
        model::Point start_road = {static_cast<int>(it_road->at(road_x0_str).as_int64()),
                                   static_cast<int>(it_road->at(road_y0_str).as_int64())};
        try {
            map.AddRoad({model::Road::HORIZONTAL, start_road, static_cast<int>(it_road->at(road_x1_str).as_int64())});
        } catch (const std::out_of_range&) {
            map.AddRoad({model::Road::VERTICAL, start_road, static_cast<int>(it_road->at(road_y1_str).as_int64())});
        }
    }
}

void LoadAndAddBuildings(const boost::json::array& buildings_arr, model::Map& map) {
    const std::string building_x_str = "x";
    const std::string building_y_str = "y";
    const std::string building_w_str = "w";
    const std::string building_h_str = "h";

    for (auto it_building = buildings_arr.begin(); it_building != buildings_arr.end(); ++it_building) {
        model::Point point{static_cast<int>(it_building->at(building_x_str).as_int64()),
                            static_cast<int>(it_building->at(building_y_str).as_int64())};
        model::Size size{static_cast<int>(it_building->at(building_w_str).as_int64()),
                          static_cast<int>(it_building->at(building_h_str).as_int64())};
        map.AddBuilding(model::Building{model::Rectangle{point, size}});
    }
}

void LoadAndAddOffices(const boost::json::array& offices_arr, model::Map& map) {
    const std::string office_id_str = "id";
    const std::string office_x_str = "x";
    const std::string office_y_str = "y";
    const std::string office_offsetx_str = "offsetX";
    const std::string office_offsety_str = "offsetY";

    for (auto it_office = offices_arr.begin(); it_office != offices_arr.end(); ++it_office) {
        model::Point position{static_cast<int>(it_office->at(office_x_str).as_int64()),
                                static_cast<int>(it_office->at(office_y_str).as_int64())};
        model::Offset office_offset{static_cast<int>(it_office->at(office_offsetx_str).as_int64()),
                                static_cast<int>(it_office->at(office_offsety_str).as_int64())};
        map.AddOffice({model::Office::Id(std::string{it_office->at(office_id_str).as_string()}),
                        position,
                        office_offset});
    }
}

void LoadAndAddLootTypes(const boost::json::array& loot_types_arr, model::Map& map) {
    const std::string loot_name_str = "name";
    const std::string loot_file_str = "file";
    const std::string loot_type_str = "type";
    const std::string loot_rotation_str = "rotation";
    const std::string loot_color_str = "color";
    const std::string loot_scale_str = "scale";
    const std::string loot_value_str = "value";

    for (auto it_loot = loot_types_arr.begin(); it_loot != loot_types_arr.end(); ++it_loot) {
        std::optional<int> loot_rotation = std::nullopt;
        if (it_loot->as_object().contains(loot_rotation_str)) {
            loot_rotation = static_cast<int>(it_loot->at(loot_rotation_str).as_int64());
        }
        std::optional<std::string_view> loot_color = std::nullopt;
        if (it_loot->as_object().contains(loot_color_str)) {
            loot_color = it_loot->at(loot_color_str).as_string();
        }
        model::LootType loot_type{it_loot->at(loot_name_str).as_string(),
                                    it_loot->at(loot_file_str).as_string(),
                                    it_loot->at(loot_type_str).as_string(),
                                    loot_rotation,
                                    loot_color,
                                    it_loot->at(loot_scale_str).as_double(),
                                    static_cast<size_t>(it_loot->at(loot_value_str).as_int64())};
        map.AddLootType(std::move(loot_type));
    }
}

// Функции для формирования ответа (api_handler)
std::string GetListOfMaps(const players::Application& app) {
    const std::string id_str = "id";
    const std::string name_str = "name";

    boost::json::array arr_json;
    for (auto it = app.GetMaps().begin(); it != app.GetMaps().end(); ++it) {
        boost::json::object temp_obj;
        temp_obj[id_str] = *(it->GetId());
        temp_obj[name_str] = it->GetName();
        arr_json.emplace_back(temp_obj);
    }
    boost::json::value val_json(arr_json);
    return {boost::json::serialize(val_json)};
}

std::optional<std::string> GetMap(const model::Map::Id &map_id, const players::Application& app) {
    const std::string id_str = "id";
    const std::string name_str = "name";
    const std::string roads_str = "roads";
    const std::string buildings_str = "buildings";
    const std::string offices_str = "offices";
    const std::string loot_types_str = "lootTypes";

    const model::Map* map_ptr = app.FindMap(map_id);
    if (map_ptr == nullptr) {
        return std::nullopt;
    }

    boost::json::object res_json;
    res_json[id_str]        = *(map_ptr->GetId());
    res_json[name_str]      = map_ptr->GetName();
    res_json[loot_types_str] = GetLootTypesArray(*map_ptr);
    res_json[roads_str]     = GetRoadsArray(*map_ptr);
    res_json[buildings_str] = GetBuildingsArray(*map_ptr);
    res_json[offices_str]   = GetOfficesArray(*map_ptr);

    boost::json::value val_json(res_json);
    return {boost::json::serialize(val_json)};
}

boost::json::array GetRoadsArray(const model::Map& map) {
    const std::string road_x0_str = "x0";
    const std::string road_y0_str = "y0";
    const std::string road_x1_str = "x1";
    const std::string road_y1_str = "y1";

    boost::json::array roads_arr;
    for (auto it_road = map.GetRoads().begin(); it_road != map.GetRoads().end(); ++it_road) {
        boost::json::object obj_road;
        obj_road[road_x0_str]       = it_road->GetStart().x;
        obj_road[road_y0_str]       = it_road->GetStart().y;
        if (it_road->IsHorizontal()) {
            obj_road[road_x1_str]   = it_road->GetEnd().x;
        } else {
            obj_road[road_y1_str]   = it_road->GetEnd().y;
        }
        roads_arr.emplace_back(obj_road);
    }
    return roads_arr;
}

boost::json::array GetBuildingsArray(const model::Map &map) {
    const std::string building_x_str = "x";
    const std::string building_y_str = "y";
    const std::string building_w_str = "w";
    const std::string building_h_str = "h";

    boost::json::array buildings_arr;
    for (auto it_building = map.GetBuildings().begin(); it_building != map.GetBuildings().end(); ++it_building) {
        boost::json::object obj_building;
        obj_building[building_x_str] = it_building->GetBounds().position.x;
        obj_building[building_y_str] = it_building->GetBounds().position.y;
        obj_building[building_w_str] = it_building->GetBounds().size.width;
        obj_building[building_h_str] = it_building->GetBounds().size.height;
        buildings_arr.emplace_back(obj_building);
    }
    return buildings_arr;
}

boost::json::array GetOfficesArray(const model::Map &map) {
    const std::string office_id_str = "id";
    const std::string office_x_str = "x";
    const std::string office_y_str = "y";
    const std::string office_offsetx_str = "offsetX";
    const std::string office_offsety_str = "offsetY";

    boost::json::array offices_arr;
    for (auto it_office = map.GetOffices().begin(); it_office != map.GetOffices().end(); ++it_office) {
        boost::json::object obj_office;
        obj_office[office_id_str]   = *(it_office->GetId());
        obj_office[office_x_str]    = it_office->GetPosition().x;
        obj_office[office_y_str]    = it_office->GetPosition().y;
        obj_office[office_offsetx_str] = it_office->GetOffset().dx;
        obj_office[office_offsety_str] = it_office->GetOffset().dy;
        offices_arr.emplace_back(obj_office);
    }
    return offices_arr;
}

boost::json::array GetLootTypesArray(const model::Map &map) {
    const std::string loot_name_str = "name";
    const std::string loot_file_str = "file";
    const std::string loot_type_str = "type";
    const std::string loot_rotation_str = "rotation";
    const std::string loot_color_str = "color";
    const std::string loot_scale_str = "scale";
    const std::string loot_value_str = "value";

    boost::json::array loot_types_arr;
    for (auto it_loot = map.GetLootTypes().begin(); it_loot != map.GetLootTypes().end(); ++it_loot) {
        boost::json::object obj_loot;
        obj_loot[loot_name_str] = std::string(it_loot->GetName());
        obj_loot[loot_file_str] = std::string(it_loot->GetFile());
        obj_loot[loot_type_str] = std::string(it_loot->GetType());
        if (it_loot->GetRotation())
            obj_loot[loot_rotation_str] = it_loot->GetRotation().value();
        if (it_loot->GetColor())
            obj_loot[loot_color_str] = std::string(it_loot->GetColor().value());
        obj_loot[loot_scale_str] = it_loot->GetScale();
        obj_loot[loot_value_str] = it_loot->GetScores();
        loot_types_arr.emplace_back(obj_loot);
    }
    return loot_types_arr;
}

std::string GetPlayerAddedAnswer(std::string auth_token, size_t player_id) {
    const std::string auth_token_str = "authToken";
    const std::string player_id_str = "playerId";

    boost::json::object res_obj;
    res_obj[auth_token_str] = auth_token;
    res_obj[player_id_str] = player_id;
    boost::json::value val_json(res_obj);
    return {boost::json::serialize(val_json)};
}

std::string GetSessionPlayers(const players::Application::Dogs& dogs) {
    const std::string player_name_str = "name";

    boost::json::object res_obj;
    for (const auto dog : dogs) {
        boost::json::object player_obj;
        player_obj[player_name_str] = dog->GetDogName(); /* Имя пса и пользователья совпадают (здесб выводится имя пользователя) */
        res_obj[std::to_string(dog->GetDogId())] = player_obj;
    }
    boost::json::value val_json(res_obj);
    return {boost::json::serialize(val_json)};
}

std::string MakeGameStateAnswer(const std::vector<players::GameState>& game_state,
                                const model::GameSession::LostObjects& lost_objects) {
    const std::string players_str   = "players";
    const std::string pos_str       = "pos";
    const std::string speed_str     = "speed";
    const std::string dir_str       = "dir";
    const std::string lost_objects_str = "lostObjects";
    const std::string type_str      = "type";
    const std::string bag_str       = "bag";
    const std::string id_str        = "id";
    const std::string score_str     = "score";

    boost::json::object res_obj;
    // Формируем массив игроков
    boost::json::object players_obj;
    for (const auto& gs : game_state) {
        boost::json::object dog_state_obj;
        dog_state_obj[pos_str] = boost::json::array{gs.state.position.x, gs.state.position.y};
        dog_state_obj[speed_str] = boost::json::array{gs.state.velocity.x, gs.state.velocity.y};
        dog_state_obj[dir_str] = DogDirectionToString(gs.state.direction);
        // содержимое сумки собаки
        boost::json::array dogs_bag_arr;
        for (const auto& obj : gs.picked_objects) {
            boost::json::object pick_obj;
            pick_obj[id_str] = obj.GetId();
            pick_obj[type_str] = obj.GetType();
            dogs_bag_arr.emplace_back(std::move(pick_obj));
        }
        dog_state_obj[bag_str] = dogs_bag_arr;
        // очки игрока
        dog_state_obj[score_str] = gs.scores;

        players_obj[std::to_string(gs.dog_id)] = dog_state_obj;
    }
    res_obj[players_str] = players_obj;
    // Формируем массив потеряных вещей
    boost::json::object lost_objs_obj;
    size_t idx = 0;
    for (const auto& object : lost_objects) {
        boost::json::object obj_obj;
        obj_obj[type_str] = object->GetType();
        obj_obj[pos_str] = boost::json::array{object->GetPosition().x, object->GetPosition().y};
        lost_objs_obj[std::to_string(idx++)] = obj_obj;
    }
    res_obj[lost_objects_str] = lost_objs_obj;

    boost::json::value val_json(res_obj);
    return {boost::json::serialize(val_json)};
}

std::string DogDirectionToString(model::Direction direction) {
    switch (direction) {
    case model::Direction::NORTH :
        return "U";
    case model::Direction::SOUTH :
        return "D";
    case model::Direction::EAST :
        return "R";
    case model::Direction::WEST :
        return "L";
    }
    return "U";
}

std::string MakeChampionsAnswer(std::vector<players::Champion> champions) {
    const std::string name_str = "name";
    const std::string score_str = "score";
    const std::string play_time_str = "playTime";

    boost::json::array res_arr;
    for (const auto& [name, score, play_time] : champions) {
        boost::json::object champion_obj;
        champion_obj[name_str] = std::move(name);
        champion_obj[score_str] = score;
        champion_obj[play_time_str] = play_time;
        res_arr.emplace_back(std::move(champion_obj));
    }
    boost::json::value val_json(res_arr);
    return {boost::json::serialize(val_json)};
}

// Разбор JSON запроса
JoinGame LoadJSONJoinGame(std::string_view request_body) {
    const std::string user_name_str = "userName";
    const std::string map_id_str = "mapId";

    boost::system::error_code ec;
    auto join_data = boost::json::parse(std::forward<std::string>(std::string(request_body)), ec).as_object();

    if (ec) {
        return JoinGame({}, {}, true);
    }
    JoinGame result;
    if (join_data.count(user_name_str) > 0) {
        result.user_name = join_data[user_name_str].as_string();
    } else {
        result.user_name = "";
    }
    if (join_data.count(map_id_str) > 0) {
        result.map_id = join_data[map_id_str].as_string();
    } else {
        result.map_id = "";
    }
    return result;
}

std::optional<std::string> LoadActionMove(std::string_view request_body) {
    const std::string move_str = "move";
    try {
        auto action_data = boost::json::parse(std::string(request_body)).as_object();
        return std::string(action_data.at(move_str).as_string());
    } catch (const boost::system::system_error &) {
        return std::nullopt;
    }
}

std::optional<double> LoadTimeDelta(std::string_view request_body) {
    const std::string timedelta_str = "timeDelta";
    try {
        auto action_data = boost::json::parse(std::string(request_body)).as_object();
        return static_cast<double>(action_data.at(timedelta_str).as_int64()) / 1000.;
    } catch (...) {
        return std::nullopt;
    }
}

// Вывод сообщений об ошибках
std::string MakeErrorString(std::string &&err_code, std::string &&err_text) {
    const std::string err_code_str = "code";
    const std::string err_msg_str = "message";

    boost::json::object res_obj;
    res_obj[err_code_str] = std::forward<std::string>(err_code);
    res_obj[err_msg_str] = std::forward<std::string>(err_text);
    boost::json::value val_json(res_obj);
    return {boost::json::serialize(val_json)};
}

// Функции для логирования - возвращают соответствующие строки для логирования (в формате JSON)
std::string GetLogServerStart(const std::string timestamp,
                              const std::string srv_address,
                              const int port) {
    boost::json::object res_obj;
    res_obj["timestamp"] = timestamp;

    boost::json::object data_obj;
    data_obj["port"] = port;
    data_obj["address"] = srv_address;

    res_obj["data"] = data_obj;
    res_obj["message"] = "server started";
    boost::json::value val_json(res_obj);
    return {boost::json::serialize(val_json)};
}

std::string GetLogServerStop(const std::string timestamp,
                             const int return_code,
                             const std::string exception_what) {
    boost::json::object res_obj;
    res_obj["timestamp"] = timestamp;

    boost::json::object data_obj;
    data_obj["code"] = return_code;
    if (!exception_what.empty()) {
        data_obj["exception"] = exception_what;
    }

    res_obj["data"] = data_obj;
    res_obj["message"] = "server exited";
    boost::json::value val_json(res_obj);
    return {boost::json::serialize(val_json)};
}
std::string GetLogRequest(const std::string timestamp,
                          const std::string client_address,
                          const std::string uri,
                          const std::string http_method) {
    boost::json::object res_obj;
    res_obj["timestamp"] = timestamp;

    boost::json::object data_obj;
    data_obj["ip"] = client_address;
    data_obj["URI"] = uri;
    data_obj["method"] = http_method;

    res_obj["data"] = data_obj;
    res_obj["message"] = "request received";
    boost::json::value val_json(res_obj);
    return {boost::json::serialize(val_json)};
}

std::string GetLogResponse(const std::string timestamp,
                           const std::string client_address,
                           const int response_time_msec,
                           const int response_code,
                           const std::string content_type) {
    boost::json::object res_obj;
    res_obj["timestamp"] = timestamp;

    boost::json::object data_obj;
    data_obj["ip"] = client_address;
    data_obj["response_time"] = response_time_msec;
    data_obj["code"] = response_code;
    data_obj["content_type"] = content_type;

    res_obj["data"] = data_obj;
    res_obj["message"] = "response sent";
    boost::json::value val_json(res_obj);
    return {boost::json::serialize(val_json)};
}

std::string GetLogError(const std::string timestamp,
                        const int error_code,
                        const std::string error_text,
                        const std::string where) {
    boost::json::object res_obj;
    res_obj["timestamp"] = timestamp;

    boost::json::object data_obj;
    data_obj["code"] = error_code;
    data_obj["text"] = error_text;
    data_obj["where"] = where;

    res_obj["data"] = data_obj;
    res_obj["message"] = "error";
    boost::json::value val_json(res_obj);
    return {boost::json::serialize(val_json)};
}

}  // namespace json_loader
