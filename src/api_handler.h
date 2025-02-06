/*
 * Код обработки запросов к API
 * - добавление нового игрока
 * - получение списка игроков в сессии игрока, который запрашивает
 * - получение состояния игровой сессии игрока, который запрашивает
 * - задания направления движения игровому персонажу
 * - запрос карты с заданным id
 * - запрос списка карт в игре
 * - пересчёт движения всех игровых персонажей по требованию тестирующей системы
 */
#pragma once
#include "model.h"
#include "players.h"
#include "json_loader.h"

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <optional>
#include <utility>

namespace http_handler {

    namespace beast = boost::beast;
    namespace http = beast::http;

    using namespace std::literals;

    using StringRequest = http::request<http::string_body>;

    using StringResponse = http::response<http::string_body>;

    struct ContentType {
        ContentType() = delete;
        constexpr static std::string_view BINARY = "application/octet-stream"sv;
        constexpr static std::string_view JSON = "application/json"sv;
        constexpr static std::string_view XML = "application/xml"sv;
        constexpr static std::string_view MP3 = "audio/mpeg"sv;
        constexpr static std::string_view BMP = "image/bmp"sv;
        constexpr static std::string_view GIF = "image/gif"sv;
        constexpr static std::string_view JPG = "image/jpeg"sv;
        constexpr static std::string_view SVG = "image/svg+xml"sv;
        constexpr static std::string_view PNG = "image/png"sv;
        constexpr static std::string_view TIFF = "image/tiff"sv;
        constexpr static std::string_view ICO = "image/vnd.microsoft.icon"sv;
        constexpr static std::string_view CSS = "text/css"sv;
        constexpr static std::string_view HTML = "text/html"sv;
        constexpr static std::string_view JS = "text/javascript"sv;
        constexpr static std::string_view PLAIN = "text/plain"sv;
    };

    StringResponse MakeStringResponse(http::status status,
                                      std::string_view body,
                                      unsigned http_version,
                                      bool keep_alive,
                                      std::string_view content_type,
                                      size_t length = 0,
                                      std::string allowed_methods = "GET, HEAD, POST");

    /* Получение токена из строки заголовка http::field::authorization. Возвращает:
     * - nullopt если token получить не удалось
     * - Token - если подходящий токен найден */
    std::optional<players::Token> TryToExtractToken(std::string_view auth_header);

    std::optional<StringResponse> AssureMethodIsGetHead(http::verb uri_method, unsigned http_version, bool keep_alive);
    std::optional<StringResponse> AssureMethodIsPOST(http::verb uri_method, unsigned http_version, bool keep_alive);
    std::optional<StringResponse> AssureContentTypeIsJSON(std::string_view ct, unsigned http_version, bool keep_alive);

    std::pair<int64_t, int64_t> LoadGETParams(std::string_view str);

    /* ------------------------------------ Обработчик запросов к API ------------------------------------ */
    class APIHandler {
    public:
        explicit APIHandler(players::Application& app) : app_{app} {}

        APIHandler(const APIHandler&) = delete;
        APIHandler& operator=(const APIHandler&) = delete;

        StringResponse ReturnAPIResponse(const StringRequest&& req, std::string req_str);

    private:
        StringResponse HandleJoining(std::string_view body,
                                            unsigned int version,
                                            bool keep_alive);
        StringResponse HandlePlayersList(std::shared_ptr<players::Player> found_player,
                                         unsigned int version,
                                         bool keep_alive,
                                         bool head_only);
        StringResponse HandleGameState(std::shared_ptr<players::Player> found_player,
                                       unsigned int version,
                                       bool keep_alive,
                                       bool head_only);
        StringResponse HandleAction(std::shared_ptr<players::Player> found_player,
                                    std::string_view body,
                                    unsigned int version,
                                    bool keep_alive);
        StringResponse HandleTick(std::string_view body,
                                  unsigned int version,
                                  bool keep_alive);
        StringResponse HandleChampions(const StringRequest&& req);

        /* Проверяет правильность авторизации и запускает функцию action */
        template <typename Fn>
        StringResponse ExecuteAuthorized(const StringRequest& req, Fn&& action) {
            unsigned int version = req.version();
            bool keep_alive = req.keep_alive();
            bool head_only = (req.method() == http::verb::head);
            auto auth_header = req[http::field::authorization];
            const auto text_response = [version, keep_alive](http::status status, std::string_view text, size_t length = 0) {
                return MakeStringResponse(status, text, version, keep_alive, ContentType::JSON, length);
            };

            if (auto token = TryToExtractToken(auth_header)) {
                auto found_player = app_.FindPlayerByToken(*token);
                if (found_player == nullptr) {
                    return text_response(http::status::unauthorized,
                                         json_loader::MakeErrorString("unknownToken", "Player token has not been found"));
                }
                return action(found_player, version, keep_alive, head_only);
            }
            return text_response(http::status::unauthorized,
                                 json_loader::MakeErrorString("invalidToken", "Authorization header is missing"));
        }

        players::Application& app_;
    };

} // namespace http_handler
