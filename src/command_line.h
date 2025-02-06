/*
 * Разбор опций запуска game_server:
 * --tick-period (-t) задаёт период автоматического обновления игрового состояния в миллисекундах.
 * Если этот параметр указан, каждые N миллисекунд сервер должен обновлять координаты объектов.
 * Если этот параметр не указан, время в игре должно управляться с помощью запроса /api/v1/game/tick к REST API.
 * --config-file (-c) задаёт путь к конфигурационному JSON-файлу игры.
 * --www-root (-w) задаёт путь к каталогу со статическими файлами игры.
 * --randomize-spawn-points включает режим, при котором пёс игрока появляется в случайной точке случайно выбранной дороги карты.
 * --save-state-period <игровое-время-в-миллисекундах> задаёт период автоматического сохранения состояния сервера.
 * --state-file <путь-к-файлу> задаёт путь к файлу, в который приложение должно сохранять своё состояние в процессе работы,
 * а при старте — восстанавливать.
 * --help (-h) должен выводить информацию о параметрах командной строки.
 */
#pragma once
#include <boost/program_options.hpp>

#include <iostream>
#include <optional>
#include <string>

namespace start_options {

using namespace std::literals;

struct Args{
    unsigned int tick_period = 0;   // получаем в миллисекундах
    std::string config_file;        // файл JSON с настройками игры (карты...)
    std::string www_root;           // каталог с файлами игры (веб-странички, скрипты, картинки...)
    bool randomize_spawn_points = false;    // true - персонажи на карте появляются в случайных местах
    bool test_mode = false;         // если true, то tick_period не задан
    unsigned int autosave_period = 0;   // получаем в миллисекундах
    std::string state_file;        // файл с сохранённым состоянием игры
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    Args args;
    po::options_description desc{"Allowed options"};

    desc.add_options()
        // опция --help и её короткая версия -h
        ("help,h", "produce help message")
        // задаёт период автоматического обновления игрового состояния в миллисекундах
        ("tick-period,t", po::value<unsigned int>(&args.tick_period)->value_name("milliseconds"s), "set tick period")
        // задаёт путь к конфигурационному JSON-файлу игры
        ("config-file,c", po::value(&args.config_file)->value_name("file"s), "set config file path")
        // задаёт путь к каталогу со статическими файлами игры
        ("www-root,w", po::value(&args.www_root)->value_name("dir"s), "set static files root")
        // включает режим, при котором пёс игрока появляется в случайной точке случайно выбранной дороги карты
        ("randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points), "spawn dogs at random positions")
        ("save-state-period", po::value<unsigned int>(&args.autosave_period)->value_name("milliseconds"s), "set autosave period")
        ("state-file", po::value(&args.state_file)->value_name("file"s), "set autosave file path");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        // Выводим описание параметров программы
        std::cout << desc;
        return std::nullopt;
    }
    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("config-file is not specified"s);
    }
    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("www-root directory path is not specified"s);
    }
    if (!vm.contains("tick-period"s)) {
        args.test_mode = true;
    }
    if (!vm.contains("state-file")) {
        args.autosave_period = 0;
    }
    return args;
}

} // namespace start_options