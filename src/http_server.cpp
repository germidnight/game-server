#include "http_server.h"

#include <boost/asio/dispatch.hpp>
#include <iostream>

namespace http_server {  // namespace http_server

using namespace std::literals;

// ---------------------------------------- SessionBase ----------------------------------------
void SessionBase::Run() {
    /* Вызываем метод Read, используя executor объекта stream_.
     * Таким образом вся работа со stream_ будет выполняться, используя его executor */
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

void SessionBase::Read() { /* Асинхронное чтение запроса */
    // Очищаем запрос от прежнего значения (метод Read может быть вызван несколько раз)
    request_ = {};
    stream_.expires_after(30s);
    // Считываем request_ из stream_, используя buffer_ для хранения считанных данных
    beast::http::async_read(stream_, buffer_, request_,
                            // По окончании операции будет вызван метод OnRead
                            beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
}

/*
 * В OnRead в возможны три ситуации:
 * - Если клиент закрыл соединение, то сервер должен завершить сеанс.
 * - Если произошла ошибка чтения, выведите её в stdout.
 * - Если запрос прочитан без ошибок, делегируйте его обработку классу-наследнику.
 */
void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
    if (ec == beast::http::error::end_of_stream) {
        // Нормальная ситуация - клиент закрыл соединение
        return Close();
    }
    if (ec) {
        return logging_handler::LogNetworkError(ec.value(), ec.message(), "read"sv);
    }
    HandleRequest(std::move(request_));
}

void SessionBase::Close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    if (ec) {
        logging_handler::LogNetworkError(ec.value(), ec.message(), "close"sv);
    }
}

/* Если при отправке ответа возникла ошибка, завершаем обмен данными с клиентом.
 * В противном случае в зависимости от параметра close:
 * - закроем соединение
 * - продолжим обмен данными с клиентом. */
void SessionBase::OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
    if (ec) {
        logging_handler::LogNetworkError(ec.value(), ec.message(), "write"sv);
    }
    if (close) {
        return Close();
    }
    // Считываем следующий запрос
    Read();
}

} // namespace http_server