#include <catch2/catch_test_macros.hpp>

#include "../src/model.h"
#include "../src/game_session.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace std::literals;

model::Map PrepareMap(size_t num_loot_types = 1) {
    const double map_speed = 4.5;
    model::Map map(model::Map::Id{"map1"}, "Map 1"s, map_speed);
    map.AddRoad(model::Road{model::Road::HORIZONTAL, {0, 0}, 40});
    map.AddRoad(model::Road{model::Road::VERTICAL, {40, 0}, 30});
    map.AddRoad(model::Road{model::Road::HORIZONTAL, {40, 30}, 0});
    map.AddRoad(model::Road{model::Road::VERTICAL, {0, 30}, 0});

    for (size_t i = 0; i < num_loot_types; ++i) {
        std::string name = std::to_string(i);
        std::string file = std::to_string(i) + std::to_string(i);
        std::string type = std::to_string(i) + std::to_string(i) + std::to_string(i);
        map.AddLootType(model::LootType(name, file, type,
                         static_cast<int>((90 * i) % 360),
                         "#338844"sv,
                         0.07,
                         20));
    }
    return std::move(map);
}

SCENARIO("Lost object generation") {

    GIVEN("loot generator") {
        loot_gen::LootGenerator gen{1s, 1.0};

        WHEN("loot types added on map") {
            model::Map map = PrepareMap(1);
            THEN("if 1 loot type added") {
                CHECK(map.GetLootTypesCount() == 1);
            }
            THEN("check GetLootByIndex() if index is in good range and check data") {
                CHECK(map.GetLootByIndex(0).GetName() == "0"s);
                CHECK(map.GetLootByIndex(0).GetFile() == "00"s);
                CHECK(map.GetLootByIndex(0).GetType() == "000"s);
                CHECK(map.GetLootByIndex(0).GetRotation() == 0);
                CHECK(map.GetLootByIndex(0).GetColor() == "#338844"s);
                CHECK(map.GetLootByIndex(0).GetScale() == 0.07);
            }
            THEN("check GetLootByIndex() if index is out of range") {
                CHECK_THROWS_AS(map.GetLootByIndex(1), std::out_of_range);
                CHECK_THROWS_AS(map.GetLootByIndex(10), std::out_of_range);
            }
        }

        WHEN("add lost object on game session") {
            model::Map map = PrepareMap(1);
            model::GameSession game_session{&map};
            THEN("check add lost object") {
                REQUIRE_NOTHROW(game_session.AddLostObjectsOnSession(gen, 10s));
            }
        }

        WHEN("added lost object") {
            model::Map map = PrepareMap(10);
            model::GameSession game_session{&map};
	    model::Dog dog{1, "user 1"s, {0., 0.}};
            auto dog_ptr = std::make_shared<model::Dog>(dog);
            game_session.AddDog(dog_ptr->GetDogId());

            THEN("check added lost object") {
                game_session.AddLostObjectsOnSession(gen, 10s);
                const model::GameSession::LostObjects &lost_objects = game_session.GetLostObjects();
                REQUIRE(lost_objects.size() >= 1);
                CHECK((((lost_objects.back()->GetPosition().x - 0.) > 10e-6) ||
                        ((lost_objects.back()->GetPosition().y - 0.) > 10e-6 )));
            }
        }
    }

}
