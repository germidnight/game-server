#include "request_handler.h"

#include <optional>

namespace http_handler {

    using namespace std::literals;

    SomeResponse RequestHandler::ReturnFile(std::filesystem::path file_path,
                                                                        unsigned int version, bool keep_alive) {
        const auto error_response = [version, keep_alive](http::status status, std::string_view text) {
            return MakeStringResponse(status, text, version, keep_alive, ContentType::PLAIN);
        };
        FileResponse response(http::status::ok, version);
        size_t file_size = 0;

        if (file_path.string().back() == '/') {
            file_path.append("index.html");
        }
        file_path = root_path_ / file_path.relative_path();

        switch (isSafePath(root_path_, file_path)) {
        case 0: {
            http::file_body::value_type file;
            boost::system::error_code ec;
            file.open(file_path.string().c_str(), beast::file_mode::read, ec);
            if (ec) {
                return error_response(http::status::not_found, "There is no access to the specified file"sv);
            }
            response.body() = std::move(file);
            break;
        }
        case 1: // ошибка - искомый файл не найден
            return error_response(http::status::not_found, "The specified file was not found"sv);
        case 2: // ошибка - выход за пределы разрешённой директории
            return error_response(http::status::bad_request, "Invalid request"sv);
        }
        // возвращаем файл
        response.prepare_payload(); // заполняет заголовки Content-Length и Transfer-Encoding
        response.set(http::field::content_type, GetContentType(file_path.extension().string()));
        response.keep_alive(keep_alive);
        return response;
    }

    // Возвращает Content-Type по расширению файла
    std::string_view GetContentType(std::string file_extension) {
        if (!file_extension.empty()) {
            file_extension = file_extension.substr(1);
        }
        FileExt f_ext;
        if (f_ext.extensions.count(file_extension) == 1) {
            return f_ext.extensions.at(file_extension);
        }
        return ContentType::BINARY;
    }

    /* Декодирует URI строку, при запуске необходимо отлавливать исключение (std::stoul):
     * - std::invalid_argument - невозможно сконвертировать  */
    std::string DecodeURI(std::string_view uri_string) {
        std::string result = "";
        for (size_t i = 0; i < uri_string.length(); ++i) {
            if (uri_string[i] == '%') {
                uint64_t x = std::stoul(std::string("0x").append(uri_string.substr(i + 1, 2)), nullptr, 16);
                result += static_cast<char>(x);
                i += 2;
            } else {
                result += uri_string[i];
            }
        }
        return result;
    }

    /* Проверка пути запрашиваемого файла, возвращаемые значения:
     * - 0 - всё хорошо, файл найден,
     * - 1 - ошибка - искомый файл не найден,
     * - 2 - ошибка - выход за пределы разрешённой директории */
    int isSafePath(const std::filesystem::path& norm_root, const std::filesystem::path& child) {
        std::error_code ec;
        auto const norm_child = std::filesystem::canonical(child, ec);
        if (ec) {
            return 1;
        }

        auto it = std::search(norm_child.begin(), norm_child.end(),
                               norm_root.begin(), norm_root.end());

        if (it == norm_child.begin()) {
            return 0;
        }
        return 2;
    }

}  // namespace http_handler
