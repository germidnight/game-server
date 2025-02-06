cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Добавляем библиотеку, указывая, что она статическая.
add_library(ModelLib STATIC
	src/collision_detector.h
	src/collision_detector.cpp
	src/game_session.h
	src/game_session.cpp
	src/loot_generator.h
	src/loot_generator.cpp
	src/model.h
	src/model.cpp
	src/model_serialization.h
	src/tagged.h
	src/players.h
	src/players.cpp)

target_link_libraries(ModelLib PUBLIC CONAN_PKG::boost Threads::Threads)

add_executable(game_server
	src/main.cpp
	src/command_line.h
	src/logging_handler.cpp
	src/logging_handler.h
	src/http_server.cpp
	src/http_server.h
	src/sdk.h
	src/boost_json.cpp
	src/json_loader.h
	src/json_loader.cpp
	src/request_handler.cpp
	src/request_handler.h
	src/ticker.h
	src/api_handler.h
	src/api_handler.cpp
	src/postgres/connection_pool.h
	src/postgres/postgres.h
	src/postgres/postgres.cpp
)
# Добавляем зависимость целей от статической библиотеки.
target_link_libraries(game_server PRIVATE Threads::Threads
						CONAN_PKG::boost
						CONAN_PKG::libpq CONAN_PKG::libpqxx
						ModelLib)

add_executable(game_server_tests
	tests/model-tests.cpp
	tests/loot_generator_tests.cpp
	tests/collision_detector_test.cpp
	tests/state-serialization-tests.cpp
)
target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2
						CONAN_PKG::boost
						Threads::Threads
						ModelLib)
