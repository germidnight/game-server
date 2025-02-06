/*
 * Модель игры
 * - игровой персонаж
 * - потерянные предметы
 * - игровые сессии
 */
#pragma once
#include "loot_generator.h"
#include "tagged.h"

#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace model {

    class Map; // описан в model.h

    enum class Direction {
        NORTH,
        SOUTH,
        WEST,
        EAST
    };

    struct Position {
        double x = 0.;
        double y = 0.;
    };
    bool operator==(const Position& left, const Position& right);

    struct Velocity {
        double x = 0.;
        double y = 0.;
        bool IsZero() const noexcept;
    };
    bool operator==(const Velocity& left, const Velocity& right);

    struct DogState {
        Position position;
        Velocity velocity;
        Direction direction = Direction::NORTH;
    };
    bool operator==(const DogState& left, const DogState& right);

    /* --------------------------------------- Найденные вещи --------------------------------------- */
    /* id_ - идентификатор, который при сборе копируется из LostObject::id_
     * type_ - индекс в векторе model::Map::LootTypes */
    class PickedObject {
    public:
        explicit PickedObject(const size_t obj_id, const size_t obj_type) noexcept
                    : id_(obj_id)
                    , type_(obj_type) {}
        const size_t GetId() const noexcept {
            return id_;
        }
        const size_t GetType() const noexcept {
            return type_;
        }
    private:
        size_t id_ = 0;
        size_t type_ = 0;
    };

    /* --------------------------------------- Собака --------------------------------------- */
    class Dog {
    public:
        Dog(size_t id, std::string name, const Position& pos)
                : id_(id)
                , name_(name) {
            state_.position = pos;
        }
        Dog(size_t id, std::string name, DogState state,
                std::vector<PickedObject> objects, size_t scores,
                double inactive_time, double total_time)
                : id_(id)
                , name_(std::move(name))
                , state_(std::move(state))
                , objects_(std::move(objects))
                , scores_(scores)
                , inactive_time_(inactive_time)
                , total_time_(total_time) {}

        size_t GetDogId() const noexcept {
            return id_;
        }
        const std::string& GetDogName() const noexcept {
            return name_;
        }
        const DogState& GetDogState() const noexcept {
            return state_;
        }
        void SetPosition(const Position& pos) {
            state_.position = pos;
        }
        void SetVelocity(const Velocity& vel) {
            state_.velocity = vel;
        }
        void SetDirection(const Direction& dir) {
            state_.direction = dir;
        }
        void SetState(const DogState& state) {
            state_ = state;
        }
        bool AddPickedObject(const PickedObject object, size_t bag_capacity);

        const std::vector<PickedObject>& GetPickedObjects() const noexcept {
            return objects_;
        }
        const bool IsBagEmpty() const noexcept {
            return objects_.empty();
        }
        std::vector<PickedObject> FlushPickedObjects() {
            std::vector<PickedObject> result(objects_.begin(), objects_.end());
            objects_.clear();
            return result;
        }
        const size_t GetScores() const noexcept {
            return scores_;
        }
        void AddScores(size_t scores) {
            scores_ += scores;
        }
        void IncInactiveTime(double time_delta_) { // время в секундах
            inactive_time_ += time_delta_;
        }
        void ResetInactiveTime() noexcept {
            inactive_time_ = 0.;
        }
        double GetInactiveTime() const noexcept {
            return inactive_time_;
        }
        void IncTotalTime(double time_delta) {
            total_time_ += time_delta;
        }
        double GetTotalTime() const noexcept {
            return total_time_;
        }

    private:
        size_t id_;
        std::string name_;
        DogState state_;
        std::vector<PickedObject> objects_;
        size_t scores_ = 0;
        double inactive_time_ = 0.; // время в секундах
        double total_time_ = 0.;  // время в игре в секундах
    };

    /* --------------------------------------- Потерянные вещи --------------------------------------- */
    /* type_ - индекс в векторе model::Map::LootTypes
     * position_ - положение на одной из дорог на карте
     * id_ - идентификатор, который при сборе копируется в PickedObject::id_
     * width_ - радиус вещи (половина ширины) */
    class LostObject {
    public:
        // половина ширины объектов
        static constexpr double ITEM_HALF_WIDTH = 0.;
        static constexpr double GATHERER_HALF_WIDTH = 0.3;
        static constexpr double OFFICE_HALF_WIDTH = 0.25;

        explicit LostObject(size_t type, Position position, size_t id, double width = ITEM_HALF_WIDTH)
                    : type_(type)
                    , position_(std::move(position))
                    , id_(id)
                    , width_(width) {}

        size_t GetType() const noexcept{
            return type_;
        }
        const Position& GetPosition() const noexcept {
            return position_;
        }
        const size_t GetId() const noexcept {
            return id_;
        }
        const double GetWidth() const noexcept {
            return width_;
        }

    private:
        size_t type_ = 0;
        Position position_;
        size_t id_; // присваивается автоматически при создании
        double width_;
    };

    /* --------------------------------------- Игровая сессия --------------------------------------- */
    class GameSession {
    public:
        using DogIds = std::list<size_t>;
        using DogIdsIt = DogIds::const_iterator;
        using LostObjects = std::list<std::shared_ptr<LostObject>>; // Одна сессия на одну карту!!!!!!!!!!!

	    explicit GameSession(model::Map* map) : map_{map} {}

        void AddDog(size_t dog_id) {
            map_id_to_it_[dog_id] = dog_ids_.emplace(std::cend(dog_ids_), dog_id);
        }

        const DogIds& GetDogIds() const noexcept {
            return dog_ids_;
        }

        const size_t CountDogsInSession() const noexcept {
            return dog_ids_.size();
        }

        Map* GetMap() const noexcept {
            return map_;
        }

        const size_t CountLostObjects() const noexcept {
            return lost_objects_.size();
        }

        const LostObjects& GetLostObjects() const noexcept {
            return lost_objects_;
        }
        /* Удаление всех элементов списка, индексы которых отмечены true */
        void RemoveObjectsFromLost(const std::vector<bool>& idxs_to_remove);

        /* 1) Получаем от генератора количество новых потерянных предметов случайным образом.
         * 2) Генерируем для каждого из них:
         *      - Тип предмета — целое число от 0 до K−1 включительно, где K — количество элементов в массиве lootTypes
         *      - Объект генерируется в случайно выбранной точке на случайно выбранной дороге карты. */
        void AddLostObjectsOnSession(loot_gen::LootGenerator& loot_generator,
                                    loot_gen::LootGenerator::TimeInterval time_delta);

        size_t GetLastObjectId() const noexcept {
            return last_object_id_;
        }

        void RestoreLostObjects(LostObjects objects, size_t last_obj_id) {
            lost_objects_ = std::move(objects);
            last_object_id_ = last_obj_id;
        }

        void DeleteDog(size_t dog_id);

    private:
        model::Map* map_;
        DogIds dog_ids_;
        std::unordered_map<size_t, DogIdsIt> map_id_to_it_; // <dog_id, iterator_to_dog_ids_>
        LostObjects lost_objects_;
        size_t last_object_id_ = 0;
    };

} // namespace model
