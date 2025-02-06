#include "game_session.h"
#include "loot_generator.h"
#include "model.h"

#include <cassert>
#include <iterator>
#include <random>

namespace model {

    /* 1) Получаем от генератора количество новых потерянных предметов случайным образом.
     * 2) Генерируем для каждого из них:
     *      - Тип предмета — целое число от 0 до K−1 включительно, где K — количество элементов в массиве lootTypes
     *      - Объект генерируется в случайно выбранной точке на случайно выбранной дороге карты. */
    void GameSession::AddLostObjectsOnSession(loot_gen::LootGenerator& loot_generator,
                                  		loot_gen::LootGenerator::TimeInterval time_delta) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> random_gen(0, map_->GetLootTypesCount() - 1);

        size_t lost_obj_count = loot_generator.Generate(time_delta, lost_objects_.size(), dog_ids_.size());
        for (size_t i = 0; i < lost_obj_count; ++i) {
            lost_objects_.emplace_back(std::make_shared<LostObject>(random_gen(gen),
                                                        map_->GetRandomPositionOnRoads(),
                                                        last_object_id_++));
        }
    }

    /* Удаление всех элементов списка, индексы которых отмечены true */
    void GameSession::RemoveObjectsFromLost(const std::vector<bool>& idxs_to_remove) {
        assert(lost_objects_.size() == idxs_to_remove.size());

        std::vector<LostObjects::iterator> its_to_erase;
        for (size_t idx = 0; idx != idxs_to_remove.size(); ++idx) {
            if (idxs_to_remove[idx]) {
                auto it = lost_objects_.begin();
                std::advance(it, idx);
                its_to_erase.push_back(it);
            }
        }
        for (auto& it : its_to_erase) {
            lost_objects_.erase(it);
        }
    }

    void GameSession::DeleteDog(size_t dog_id) {
        if (map_id_to_it_.count(dog_id) > 0) {
            dog_ids_.erase(map_id_to_it_.at(dog_id));
            map_id_to_it_.erase(dog_id);
        }
    }

    /* Добавляет подобранный предмет в сумку собаки и возвращает true,
    * возвращает false если сумка полна*/
    bool Dog::AddPickedObject(const PickedObject object, size_t bag_capacity) {
        if (objects_.size() < bag_capacity) {
            objects_.emplace_back(std::move(object));
        } else {
            return false;
        }
        return true;
    }

    bool operator==(const Position &left, const Position &right) {
        return (left.x == right.x) && (left.y == right.y);
    }
    bool operator==(const Velocity &left, const Velocity &right) {
        return (left.x == right.x) && (left.y == right.y);
    }
    bool operator==(const DogState &left, const DogState &right) {
        return (left.position == right.position) &&
               (left.velocity == right.velocity) &&
               (left.direction == right.direction);
    }
    bool Velocity::IsZero() const noexcept {
        constexpr double DELTA = 0.000001;
        if ((x > -DELTA) && (x < DELTA) &&
            (y > -DELTA) && (y < DELTA)) {
            return true;
        }
        return false;
    }

} // namespace model
