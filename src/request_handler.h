/*
 * Код обработки запросов
 * - первичный разбор запроса
 * - отдача статических файлов
 * - перенаправление запросов к API в APIHandler (выполняется в strand последовательно для избежания гонок)
 */
#pragma once
#include "api_handler.h"

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <map>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

using namespace std::literals;

using FileResponse = http::response<http::file_body>;
using EmptyResponse = http::response<http::empty_body>;
using SomeResponse = std::variant<EmptyResponse, StringResponse, FileResponse>;

struct ContentType; // В файле api_handler.h

struct FileExt {
    const std::map<std::string, std::string_view> extensions = {
        {"htm", ContentType::HTML}, {"html", ContentType::HTML},
        {"css", ContentType::CSS}, {"txt", ContentType::PLAIN},
        {"js", ContentType::JS}, {"json", ContentType::JSON},
        {"xml", ContentType::XML}, {"png", ContentType::PNG},
        {"jpg", ContentType::JPG}, {"jpe", ContentType::JPG}, {"jpeg", ContentType::JPG},
        {"gif", ContentType::GIF}, {"bmp", ContentType::BMP}, {"ico", ContentType::ICO},
        {"tif", ContentType::TIFF}, {"tiff", ContentType::TIFF},
        {"svg", ContentType::SVG}, {"svgz", ContentType::SVG},
        {"mp3", ContentType::MP3}};
};

std::string_view GetContentType(std::string file_extension);
std::string DecodeURI(const std::string_view uri_string);
int isSafePath(const std::filesystem::path& norm_root, const std::filesystem::path& child);

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;

    explicit RequestHandler(players::Application& app,
                            std::filesystem::path root_dir,
                            Strand api_strand)
                : api_handler_{std::make_unique<APIHandler>(app)}
                , root_path_(root_dir)
                , api_strand_(api_strand) {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    /* Обработка запроса
     * Функция принимает параметры:
     * - rec - запрос серверу (тело запроса)
     * - send - функция для отправки готового ответа клиенту
     * - log_function - функция для создания записи в лог-файл */
    template <typename Body, typename Allocator, typename Send, typename LogFunc>
    void operator()(http::request<Body, http::basic_fields<Allocator>> &&req,
                    Send &&send,
                    LogFunc&& log_function) {
        const std::string api_base_str = "/api/";
        unsigned int version = req.version();
        bool keep_alive = req.keep_alive();

        try {
            std::string req_str{DecodeURI(req.target())};

            if (req_str.starts_with(api_base_str)) {
                // запрашивается REST API
                auto handle = [self = shared_from_this(), send, log_function,
                               req = std::forward<decltype(req)>(req), req_str, version, keep_alive]() {
                    try { // лямбда-функция будет выполняться внутри strand
                        StringResponse answer = self->api_handler_->ReturnAPIResponse(std::forward<decltype(req)>(req),
                                                                                      std::move(req_str));
                        log_function(answer.result_int(), std::string(answer[http::field::content_type]));
                        send(std::move(answer));
                    } catch (...) {
                        StringResponse answer = MakeStringResponse(http::status::internal_server_error,
                                                                   "Internal server error in lambda of HandleRequest"sv,
                                                                   version, keep_alive, ContentType::JSON);
                        log_function(answer.result_int(), std::string(answer[http::field::content_type]));
                        send(answer);
                    }
                };
                return boost::asio::dispatch(api_strand_, handle);
            } else {
                // Запрашивается файл
                std::filesystem::path path{req_str};
                path = path.lexically_normal();
                auto answer = ReturnFile(path, version, keep_alive);
                if (std::holds_alternative<StringResponse>(answer)) {
                    StringResponse response(std::move(std::get<StringResponse>(answer)));
                    log_function(response.result_int(), std::string(response[http::field::content_type]));
                    send(std::move(response));
                } else if (std::holds_alternative<FileResponse>(answer)) {
                    FileResponse response(std::move(std::get<FileResponse>(answer)));
                    log_function(response.result_int(), std::string(response[http::field::content_type]));
                    send(std::move(response));
                } else if (std::holds_alternative<EmptyResponse>(answer)) {
                    EmptyResponse response(std::move(std::get<EmptyResponse>(answer)));
                    log_function(response.result_int(), std::string(response[http::field::content_type]));
                    send(std::move(response));
                }
            }
        } catch (...) {
            StringResponse answer = MakeStringResponse(http::status::internal_server_error,
                                                       "Internal server error in HandleRequest"sv,
                                                       version, keep_alive, ContentType::JSON);
            log_function(answer.result_int(), std::string(answer[http::field::content_type]));
            send(answer);
        }
    }

private:
    SomeResponse ReturnFile(std::filesystem::path file_path, unsigned int version, bool keep_alive);

    std::unique_ptr<APIHandler> api_handler_;
    std::filesystem::path root_path_;
    Strand api_strand_;
};

}  // namespace http_handler
