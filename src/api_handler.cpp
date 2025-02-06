#include "api_handler.h"
#include "json_loader.h"
#include "players.h"

namespace http_handler {

    // Создаёт StringResponse с заданными параметрами
    StringResponse MakeStringResponse(http::status status,
                                      std::string_view body,
                                      unsigned http_version,
                                      bool keep_alive,
                                      std::string_view content_type,
                                      size_t length,
                                      std::string allowed_methods) {
        StringResponse response(status, http_version);
        response.set(http::field::content_type, content_type);
        response.set(http::field::cache_control, "no-cache");
        if (status == http::status::method_not_allowed) {
            response.set("Allow"sv, allowed_methods);
        }
        response.body() = body;
        if (length == 0) {
            length = response.body().size();
        }
        response.content_length(length);
        response.keep_alive(keep_alive);
        return response;
    }

    std::optional<players::Token> TryToExtractToken(std::string_view auth_header) {
        constexpr size_t bearer_token_length = 32;

        if (auth_header.empty() || (auth_header.substr(0, 7) != "Bearer ")) {
            return std::nullopt;
        }
        std::string_view bearer_token = auth_header.substr(7);
        if (bearer_token.length() != bearer_token_length) {
            return std::nullopt;
        }
        return players::Token(std::move(std::string(bearer_token)));
    }

    std::optional<StringResponse> AssureMethodIsGetHead(http::verb uri_method, unsigned http_version, bool keep_alive) {
        if ((uri_method != http::verb::get) && (uri_method != http::verb::head)) {
            return MakeStringResponse(http::status::method_not_allowed,
                                      json_loader::MakeErrorString("invalidMethod", "Only GET, HEAD methods are expected"),
                                      http_version, keep_alive, ContentType::JSON, 0, "GET, HEAD");
        }
        return std::nullopt;
    }

    std::optional<StringResponse> AssureMethodIsPOST(http::verb uri_method, unsigned http_version, bool keep_alive) {
        if (uri_method != http::verb::post) {
            return MakeStringResponse(http::status::method_not_allowed,
                                      json_loader::MakeErrorString("invalidMethod", "Only POST method is expected"),
                                      http_version, keep_alive, ContentType::JSON, 0, "POST");
        }
        return std::nullopt;
    }

    std::optional<StringResponse> AssureContentTypeIsJSON(std::string_view ct, unsigned http_version, bool keep_alive) {
        if (ContentType::JSON.compare(ct) != 0) {
            return MakeStringResponse(http::status::bad_request,
                                      json_loader::MakeErrorString("invalidArgument", "Invalid content type"),
                                      http_version, keep_alive, ContentType::JSON, 0, "GET, HEAD, POST");
        }
        return std::nullopt;
    }

    /* ------------------------------------ Обработчик запросов к API ------------------------------------ */

    StringResponse APIHandler::ReturnAPIResponse(const StringRequest&& req, std::string req_str) {
        const std::string command_maps1_str             = "/api/v1/maps";
        const std::string command_map2_str              = "/api/v1/maps/";
        const size_t command_map2_symbols = command_map2_str.length();

        const std::string command_join_str              = "/api/v1/game/join";
        const std::string command_session_players_str   = "/api/v1/game/players";

        const std::string command_get_game_state_str    = "/api/v1/game/state";
        const std::string command_action_str            = "/api/v1/game/player/action";
        const std::string command_tick_str              = "/api/v1/game/tick";
        const std::string command_records_str           = "/api/v1/game/records";

        unsigned int version = req.version();
        bool keep_alive = req.keep_alive();

        const auto text_response = [version, keep_alive](http::status status, std::string_view text, size_t length = 0,
                                                         std::string allowed_methods = "GET, HEAD, POST"s) {
            return MakeStringResponse(status, text, version, keep_alive, ContentType::JSON, length, allowed_methods);
        };
        /* ------------------------------------------- вход в игру ------------------------------------------- */
        if (req_str.starts_with(command_join_str)) {
            auto post = AssureMethodIsPOST(req.method(), version, keep_alive);
            if (post) {
                return post.value();
            }
            auto ct_json = AssureContentTypeIsJSON(req[http::field::content_type], version, keep_alive);
            if (ct_json) {
                return ct_json.value();
            }
            return HandleJoining(req.body(), version, keep_alive);

        /* ----------------------------------- запрашивается карта с заданным id ----------------------------------- */
        } else if (req_str.starts_with(command_map2_str)) {
            auto get_head = AssureMethodIsGetHead(req.method(), version, keep_alive);
            if (get_head) {
                return get_head.value();
            }
            std::string_view map_id = req_str.substr(command_map2_symbols);
            std::optional<std::string> map_result = json_loader::GetMap(model::Map::Id{std::string(map_id)}, app_);
            if (map_result.has_value()) {
                if (req.method() == http::verb::get) {
                    return text_response(http::status::ok, map_result.value());
                } else { // HEAD method
                    return text_response(http::status::ok, "",map_result.value().length());
                }
            } else { // Запрашиваемый id карты не найден
                return text_response(http::status::not_found, json_loader::MakeErrorString("mapNotFound", "Map not found"));
            }
        /* ----------------------------------- запрашивается список карт ----------------------------------- */
        } else if (req_str.compare(command_maps1_str) == 0) {
            auto get_head = AssureMethodIsGetHead(req.method(), version, keep_alive);
            if (get_head) {
                return get_head.value();
            }
            if (req.method() == http::verb::get) {
                return text_response(http::status::ok, json_loader::GetListOfMaps(app_));
            } else { // HEAD method
                return text_response(http::status::ok, "", json_loader::GetListOfMaps(app_).length());
            }

        /* ----------------------------------- запрашивается список игроков в сессии ----------------------------------- */
        } else if (req_str.starts_with(command_session_players_str)) {
            auto get_head = AssureMethodIsGetHead(req.method(), version, keep_alive);
            if (get_head) {
                return get_head.value();
            }
            return ExecuteAuthorized(req, [this](std::shared_ptr<players::Player> found_player,
                                                     unsigned int version,
                                                     bool keep_alive,
                                                     bool head_only) {
                    return this->HandlePlayersList(found_player, version, keep_alive, head_only);
                });

        /* ----------------------------------- запрос игрового состояния ----------------------------------- */
        } else if (req_str.starts_with(command_get_game_state_str)) {
            auto get_head = AssureMethodIsGetHead(req.method(), version, keep_alive);
            if (get_head) {
                return get_head.value();
            }
            return ExecuteAuthorized(req, [this](std::shared_ptr<players::Player> found_player,
                                                     unsigned int version,
                                                     bool keep_alive,
                                                     bool head_only) {
                    return this->HandleGameState(found_player, version, keep_alive, head_only);
                });

        /* ----------------------------------- запрос на управление персонажем ----------------------------------- */
        } else if (req_str.starts_with(command_action_str)) {
            auto post = AssureMethodIsPOST(req.method(), version, keep_alive);
            if (post) {
                return post.value();
            }
            auto ct_json = AssureContentTypeIsJSON(req[http::field::content_type], version, keep_alive);
            if (ct_json) {
                return ct_json.value();
            }
            return ExecuteAuthorized(req, [this, &req](std::shared_ptr<players::Player> found_player,
                                                           unsigned int version, bool keep_alive,
                                                           [[maybe_unused]] bool head_only) {
                    return HandleAction(found_player, req.body(),
                                        version, keep_alive);
                });

        /* ----------------------------------- запрос на управление временем на карте ----------------------------------- */
        } else if (app_.IsTestMode() && req_str.starts_with(command_tick_str)) {
            auto post = AssureMethodIsPOST(req.method(), version, keep_alive);
            if (post) {
                return post.value();
            }
            auto ct_json = AssureContentTypeIsJSON(req[http::field::content_type], version, keep_alive);
            if (ct_json) {
                return ct_json.value();
            }
            return HandleTick(req.body(), version, keep_alive);

        /* ----------------------------------- запрос на получения списка рекордсменов ----------------------------------- */
        } else if (req_str.starts_with(command_records_str)) {
            auto get_head = AssureMethodIsGetHead(req.method(), version, keep_alive);
            if (get_head) {
                return get_head.value();
            }
            return HandleChampions(std::move(req));
        } else {
            // Неправильный запрос
            return text_response(http::status::bad_request, json_loader::MakeErrorString("badRequest", "Invalid endpoint"));
        }
    }

    /*
     * Обработка запроса на подключение к игре
     */
    StringResponse APIHandler::HandleJoining(std::string_view body,
                                            unsigned int version,
                                            bool keep_alive) {
        const auto text_response = [version, keep_alive](http::status status, std::string_view text, size_t length = 0) {
            return MakeStringResponse(status, text, version, keep_alive, ContentType::JSON, length);
        };

        auto join_data = json_loader::LoadJSONJoinGame(body);

        if (join_data.error) {
            return text_response(http::status::bad_request, json_loader::MakeErrorString("invalidArgument", "Join game request parse error"));
        }

        if (join_data.user_name.empty()) {
            return text_response(http::status::bad_request, json_loader::MakeErrorString("invalidArgument", "Invalid name"));
        }

        if (join_data.map_id.empty()) {
            return text_response(http::status::bad_request, json_loader::MakeErrorString("invalidArgument", "Invalid map"));
        }

        players::JoinGameResult result = app_.JoinPlayerToGame(model::Map::Id{join_data.map_id}, join_data.user_name);

        if (result.error != players::JoinGameErrorCode::NONE) {
            switch (result.error) {
            case players::JoinGameErrorCode::MAP_NOT_FOUND :
                return text_response(http::status::not_found, json_loader::MakeErrorString("mapNotFound", "Map not found"));
            case players::JoinGameErrorCode::SESSION_NOT_FOUND :
                return text_response(http::status::not_found, json_loader::MakeErrorString("mapNotFound", "Session not found"));
            case players::JoinGameErrorCode::INVALID_NAME :
                return text_response(http::status::not_found, json_loader::MakeErrorString("invalidArgument", "Invalid name"));
            }
        }

        return text_response(http::status::ok, json_loader::GetPlayerAddedAnswer(**(result.player_token), result.dog_id));
    }

    /*
     * Обработка запроса на получение списка игроков в сессии игрока (кто делает запрос)
     */
    StringResponse APIHandler::HandlePlayersList(std::shared_ptr<players::Player> found_player,
                                                 unsigned int version,
                                                 bool keep_alive,
                                                 bool head_only) {
        const auto text_response = [version, keep_alive](http::status status, std::string_view text, size_t length = 0,
                                                         std::string allowed_methods = "GET, HEAD"s) {
            return MakeStringResponse(status, text, version, keep_alive, ContentType::JSON, length, allowed_methods);
        };
       players::Application::Dogs dogs = app_.GetDogsInSession(found_player);
        if (head_only) {
            return text_response(http::status::ok, "", json_loader::GetSessionPlayers(dogs).length());
        }
        return text_response(http::status::ok, json_loader::GetSessionPlayers(dogs));
    }

    /*
     * Обработка запроса на получение игрового состояния:
     * 1) получаем список всех игроков в сессии игрока
     * 2) формируем вектор состояний собак
     * 3) формируем вектор потерянных объектов на карте
     * 4) формируем ответ
     */
    StringResponse APIHandler::HandleGameState(std::shared_ptr<players::Player> found_player,
                                               unsigned int version,
                                               bool keep_alive,
                                               bool head_only) {
        const auto text_response = [version, keep_alive](http::status status, std::string_view text, size_t length = 0,
                                                         std::string allowed_methods = "GET, HEAD"s) {
            return MakeStringResponse(status, text, version, keep_alive, ContentType::JSON, length, allowed_methods);
        };
        std::vector<players::GameState> game_state;

        for (const auto next_player : app_.GetPlayersInSession(found_player)) {
            game_state.push_back(app_.GetPlayerGameState(next_player));
        }
        if (head_only) {
            return text_response(http::status::ok, "", json_loader::MakeGameStateAnswer(game_state, app_.GetLostObjects(found_player)).length());
        }
        return text_response(http::status::ok, json_loader::MakeGameStateAnswer(game_state, app_.GetLostObjects(found_player)));
    }

    /*
     * Обработка запроса на задание действия игровому персонажу
     */
    StringResponse APIHandler::HandleAction(std::shared_ptr<players::Player> found_player,
                                            std::string_view body,
                                            unsigned int version,
                                            bool keep_alive) {
        const auto text_response = [version, keep_alive](http::status status, std::string_view text, size_t length = 0,
                                                         std::string allowed_methods = "POST"s) {
            return MakeStringResponse(status, text, version, keep_alive, ContentType::JSON, length, allowed_methods);
        };

        auto action_data = json_loader::LoadActionMove(body);
        if (action_data) {
            if (*action_data == "L") {
                app_.SetDogAction(found_player, players::ActionMove::LEFT);
            } else if (*action_data == "R") {
                app_.SetDogAction(found_player, players::ActionMove::RIGHT);
            } else if (*action_data == "U") {
                app_.SetDogAction(found_player, players::ActionMove::UP);
            } else if (*action_data == "D") {
                app_.SetDogAction(found_player, players::ActionMove::DOWN);
            } else {
                app_.SetDogAction(found_player, players::ActionMove::STOP);
            }
        } else {
            return text_response(http::status::bad_request, json_loader::MakeErrorString("invalidArgument",
                                                                                        "Failed to parse action"));
        }
        return text_response(http::status::ok, "{}");
    }

    /*
     * Обработка запроса на управление временем на карте (для запуска в режиме тестирования)
     */
    StringResponse APIHandler::HandleTick(std::string_view body,
                                            unsigned int version,
                                            bool keep_alive) {
        const auto text_response = [version, keep_alive](http::status status, std::string_view text, size_t length = 0,
                                                         std::string allowed_methods = "POST"s) {
            return MakeStringResponse(status, text, version, keep_alive, ContentType::JSON, length, allowed_methods);
        };

        auto action_data = json_loader::LoadTimeDelta(body);
        if (action_data) {
            /* все персонажи должны переместиться по правилам перемещения персонажей.
             * Последующие запросы игрового состояния должны возвращать новые координаты персонажей. */
            app_.MoveDogs(*action_data);
        } else {
            return text_response(http::status::bad_request, json_loader::MakeErrorString("invalidArgument",
                                                                            "Failed to parse tick request JSON"));
        }
        if (app_.GetAutosaveFile().has_value()) {
            players::AutosaveState(app_, app_.GetAutosaveFile().value());
        }
        return text_response(http::status::ok, "{}");
    }

    /*
     * Обработка запроса на получение списка рекордсменов
     */
    StringResponse APIHandler::HandleChampions(const StringRequest&& req) {
        unsigned int version = req.version();
        bool keep_alive = req.keep_alive();
        bool head_only = req.method() == http::verb::head;
        const auto text_response = [version, keep_alive](http::status status, std::string_view text, size_t length = 0,
                                                         std::string allowed_methods = "GET, HEAD"s) {
            return MakeStringResponse(status, text, version, keep_alive, ContentType::JSON, length, allowed_methods);
        };
        constexpr size_t MAXITEMS = 100;

        auto params = LoadGETParams(req.target());
        if ((params.first < 0) ||
            (params.second < 0) ||
            (params.second > MAXITEMS)) {
            return text_response(http::status::bad_request, json_loader::MakeErrorString("invalidArgument",
                                                                            "Invalid parameter values"));
        }
        size_t start = static_cast<size_t>(params.first);
        size_t max_items = static_cast<size_t>(params.second);

        if (head_only) {
            return text_response(http::status::ok, "", json_loader::MakeChampionsAnswer(
                                app_.GetChampions(start, max_items)).length());
        }
        return text_response(http::status::ok, json_loader::MakeChampionsAnswer(
                                app_.GetChampions(start, max_items)));
    }

    std::pair<int64_t, int64_t> LoadGETParams(std::string_view str) {
        const std::string start_str = "start"s;
        const std::string max_items_str = "maxItems"s;
        const std::string_view numbers = "0123456789"sv;
        int64_t start, max_items;

        size_t pos_start = str.find(start_str);
        if (pos_start != str.npos) {
            pos_start += (start_str.length() + 1);
            size_t pos_end = str.find_first_not_of(numbers, pos_start);
            if (pos_end == str.npos) {
                pos_end = str.length();
            }
            std::string_view start_str = str.substr(pos_start, pos_end - pos_start);
            try {
                start = std::stoi(std::string(start_str));
            } catch (const std::exception &) {
                start = 0;
            }
        } else {
            start = 0;
        }

        size_t pos_items = str.find(max_items_str);
        if (pos_items != str.npos) {
            pos_items += (max_items_str.length() + 1);
            size_t pos_end = str.find_first_not_of(numbers, pos_items);
            if (pos_end == str.npos) {
                pos_end = str.length();
            }
            std::string_view items_str = str.substr(pos_items, pos_end - pos_items);
            try {
                max_items = std::stoi(std::string(items_str));
            } catch (const std::exception &) {
                max_items = 100;
            }
        } else {
            max_items = 100;
        }

        return std::make_pair(start, max_items);
    }

} // namespace http_handler
