/*
 * Модель игры
 * Хранит состояния игры.
 * Реализует базовые правила предметной области:
 * - перемещение;
 * - обработка коллизий;
 * - начисление баллов;
 * - ограничения на перемещения персонажей.
 * Основная работа по вычислению состояния игрового мира происходит здесь
 */
#pragma once
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "loot_generator.h"
#include "tagged.h"
#include "game_session.h"

using namespace std::chrono_literals;

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class LootType {
public:
    explicit LootType(std::string_view name, std::string_view file, std::string_view type,
                  std::optional<int> rotation, std::optional<std::string_view> color,
                  double scale, size_t scores)
                  : name_{name}
                  , file_{file}
                  , type_{type}
                  , rotation_(rotation)
                  , color_{color}
                  , scale_(scale)
                  , scores_cost_(scores) {}

    std::string_view GetName() const noexcept {
        return name_;
    }
    std::string_view GetFile() const noexcept {
        return file_;
    }
    std::string_view GetType() const noexcept {
        return type_;
    }
    std::optional<std::string_view> GetColor() const noexcept {
        return color_;
    }
    const std::optional<int> GetRotation() const noexcept {
        return rotation_;
    }
    const double GetScale() const noexcept {
        return scale_;
    }
    const size_t GetScores() const noexcept {
        return scores_cost_;
    }

private:
    std::string name_;
    std::string file_;
    std::string type_;
    std::optional<int> rotation_;
    std::optional<std::string> color_;
    double scale_;
    size_t scores_cost_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name,
            double speed = 1., size_t bag_capacity = 3) noexcept
        : id_(std::move(id))
        , name_(std::move(name))
        , speed_(speed)
        , bag_capacity_(bag_capacity) {}

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    double GetSpeed() const noexcept {
        return speed_;
    }

    void AddRoad(const Road& road);

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

    Position GetRandomPositionOnRoads() const;
    Position GetTestPositionOnRoads() const noexcept;

    DogState MoveDog(const std::shared_ptr<Dog> dog, double time) const;

    void AddLootType(LootType loot_type);

    size_t GetLootTypesCount() const noexcept {
        return loot_types_.size();
    }

    const std::vector<LootType>& GetLootTypes() const noexcept {
        return loot_types_;
    }

    const LootType& GetLootByIndex(size_t index) const {
        if (index >= loot_types_.size()) {
            throw std::out_of_range("Index too big");
        }
        return loot_types_[index];
    }

    const size_t GetBagCapacity() const noexcept {
        return bag_capacity_;
    }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;
    std::vector<size_t> GetRoadByPosition(const Position& pos) const; // возвращает массив индексов дорог в массиве

    Id id_;
    std::string name_;
    double speed_ = 1.;
    size_t bag_capacity_ = 3;

    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    static constexpr double HALF_ROAD_WIDE = 0.4; // она есть также в detail (model.cpp)
    Roads normal_roads_; /* массив дорог в котором координаты начала дороги всегда меньше координат конца */
    /* все горизонтальные дороги ключ - y-координата, значение - индекс в normal_roads_ */
    std::unordered_multimap<Coord, size_t> hor_roads_;
    /* все вертикальные дороги ключ - x-координата, значение - индекс в normal_roads_ */
    std::unordered_multimap<Coord, size_t> vert_roads_;

    std::vector<LootType> loot_types_; // типы потерянных вещей на карте
};

class Game {
public:
    static constexpr size_t MAX_DOGS_ON_MAP = std::numeric_limits<size_t>::max();

    using Maps = std::vector<Map>;
    using Sessions = std::shared_ptr<GameSession>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    std::vector<Sessions>& GetSessions() {
        return sessions_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

    std::shared_ptr<GameSession> PlacePlayerOnMap(const Map::Id& map_id);

    void SetLootGenerator(loot_gen::LootGenerator::TimeInterval base_interval, double probability) {
        loot_generator_ = loot_gen::LootGenerator(base_interval, probability);
    }

    loot_gen::LootGenerator& GetLootGenerator() {
        return loot_generator_;
    }

    void RestoreSessions(std::vector<Sessions> sessions_vec) {
        for (Sessions& session : sessions_vec) {
            Map::Id map_id_ = session->GetMap()->GetId();
            if (map_id_to_index_.count(map_id_) == 0) {
                throw std::domain_error("Restore Session failed, no such map_id");
            }
            sessions_[map_id_to_index_.at(map_id_)] = session;
        }
    }

    double GetDogRetirementTime() const noexcept {
        return dog_retirement_time_;
    }

    void SetDogRetirementTime(double inactive_time) {
        dog_retirement_time_ = inactive_time;
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    Maps maps_;
    MapIdToIndex map_id_to_index_;

    std::vector<Sessions> sessions_; /* Индексы соответствуют индексам карт в maps_ */

    loot_gen::LootGenerator loot_generator_{1s, 0.};
    double dog_retirement_time_ = 60.; // в секундах
};

}  // namespace model
