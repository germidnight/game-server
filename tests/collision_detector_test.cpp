#define _USE_MATH_DEFINES
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../src/collision_detector.h"

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

using namespace collision_detector;
using namespace std::literals;

namespace Catch {
    template <>
    struct StringMaker<GatheringEvent> {
        static std::string convert(GatheringEvent const &value) {
            std::ostringstream tmp;
            tmp << "(" << value.gatherer_id << "," << value.item_id << "," << value.sq_distance << "," << value.time << ")";

            return tmp.str();
        }
    };

} // namespace Catch

bool operator==(const collision_detector::GatheringEvent &first,
                const collision_detector::GatheringEvent &second) {
    return ((first.gatherer_id == second.gatherer_id) &&
            (first.item_id == second.item_id) &&
            (std::fabs(first.sq_distance - second.sq_distance) < 1e-10) &&
            (std::fabs(first.time - second.time) < 1e-10));
}

bool operator==(const std::vector<GatheringEvent> &left,
                const std::vector<GatheringEvent> &right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (size_t i = 0; i != left.size(); ++i) {
        if (!(left[i] == right[i])) {
            return false;
        }
    }
    return true;
}
/*
 * Будет возвращать собак в виде собирателей, а потерянные вещи — в виде предметов.
 * Событие столкновения характеризуется:
 * - индексом собирателя,
 * - индексом предмета,
 * - квадратом расстояния до предмета,
 * - относительным временем столкновения собирателя с предметом (число от 0 до 1).
 * Мы используем квадрат расстояния, чтобы избежать ненужного извлечения корней.
 */
class TestFindGatherEvents : public ItemGathererProvider {
public:
    explicit TestFindGatherEvents(size_t ic, std::vector<Item> items,
                                  size_t gc, std::vector<Gatherer> ga)
        : items_count_(ic), items_(std::move(items)), gatherers_count_(gc), gatherer_(std::move(ga)) {}
    size_t ItemsCount() const override {
        return items_count_;
    }
    Item GetItem(size_t idx) const override {
        return items_[idx];
    }
    size_t GatherersCount() const override {
        return gatherers_count_;
    }
    Gatherer GetGatherer(size_t idx) const override {
        return gatherer_[idx];
    }

private:
    size_t items_count_;
    std::vector<Item> items_;
    size_t gatherers_count_;
    std::vector<Gatherer> gatherer_;
};

/* Тестируем функцию std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider)
 *
 * Функция возвращает вектор событий, идущих в хронологическом порядке.
 * Считается, что предметы остаются после столкновений на своём месте — событие добавляется в вектор,
 * даже если другой собиратель уже сталкивался с этим предметом.
 * Порядок одновременно произошедших событий может быть любым.
 *
 * Столкновение засчитывается при совпадении двух факторов:
 * 1) расстояние от предмета до прямой перемещения собирателя не превышает величины w + W,
 * где w — радиус предмета, а W — радиус собирателя,
 * 2) проекция предмета на прямую перемещения собирателя попадает на отрезок перемещения.
 * Если объект не переместился, считайте, что он не совершил столкновений.
 * При этом учитывайте перемещение на любое ненулевое расстояние — погрешностью можно пренебречь.*/

SCENARIO("Check FindGatheringEvents() function") {
    using Catch::Matchers::WithinAbs;

    GIVEN("One item, One gatherer") {
        WHEN("check gathering on position Item.Position == GATHERER.Position") {
            const double ga_width = 0.8;
            const double item_width = 0.6;
            const std::vector<double> step = {0.01, 0.6, 30.};
            const std::vector<std::string> step_str = {"little"s, "medium"s, "big"s};
            const std::vector<model::Position> pos = {{0., 0.}, {5.1, 0.}, {0., 6.2}, {10., 10.}, {-0.4, -0.4}, {-0.4, 0.}, {12.9, -0.4}};
            for (size_t j = 0; j != pos.size(); ++j) {
                for (size_t i = 0; i != step.size(); ++i) {
                    TestFindGatherEvents tfge(1, {Item(0, pos[j], 0, item_width)},
                                              1, {Gatherer(pos[j], {pos[j].x + step[i], pos[j].y}, ga_width)});
                    THEN("item must be gathered immediately at {" + std::to_string(pos[j].x) + ", " +
                         std::to_string(pos[j].y) + "} " + step_str[i] + "step") {
                        auto result = FindGatherEvents(tfge);
                        REQUIRE(result.size() > 0);
                        CHECK(result[0].gatherer_id == 0);
                        CHECK(result[0].item_id == 0);
                        CHECK_THAT(result[0].sq_distance, WithinAbs(0., 1e-10));
                        CHECK_THAT(result[0].time, WithinAbs(0., 1e-10));
                    }
                }
            }
        }
        WHEN("check gathering on coming to (GATHERER.Position ==> Item.Position)") {
            const double ga_width = 0.8;
            const double item_width = 0.6;
            const std::vector<double> step = {0.01, 0.6, 30.};
            const std::vector<std::string> step_str = {"little"s, "medium"s, "big"s};
            const std::vector<model::Position> pos = {{0., 0.}, {5.1, 0.}, {0., 6.2}, {10., 10.}, {-0.4, -0.4}, {-0.4, 0.}, {12.9, -0.4}};
            for (size_t j = 0; j != pos.size(); ++j) {
                for (size_t i = 0; i != step.size(); ++i) {
                    TestFindGatherEvents tfge(1, {Item(0, pos[j], 0, item_width)},
                                              1, {Gatherer({pos[j].x, pos[j].y - step[i]}, pos[j], ga_width)});
                    THEN("item must be gathered immediately at {" + std::to_string(pos[j].x) + ", " +
                         std::to_string(pos[j].y) + "} " + step_str[i] + "step") {
                        auto result = FindGatherEvents(tfge);
                        REQUIRE(result.size() > 0);
                        CHECK(result[0].gatherer_id == 0);
                        CHECK(result[0].item_id == 0);
                        CHECK(result[0].time <= 1.);
                    }
                }
            }
        }
        WHEN("check NOT gathering on position Item.Position != GATHERER.Position") {
            const double ga_width = 0.8;
            const double item_width = 0.6;
            const std::vector<double> step = {0.01, 0.6, 30.};
            const std::vector<std::string> step_str = {"little"s, "medium"s, "big"s};
            const std::vector<model::Position> pos = {{0., 0.}, {5.1, 0.}, {0., 6.2}, {10., 10.}, {-0.4, -0.4}, {-0.4, 0.}, {12.9, -0.4}};
            for (size_t j = 0; j != pos.size(); ++j) {
                for (size_t i = 0; i != step.size(); ++i) {
                    TestFindGatherEvents tfge(1, {Item(0, { pos[j].x + 1000., pos[j].y}, 0, item_width)},
                                              1, {Gatherer(pos[j], {pos[j].x + step[i], pos[j].y}, ga_width)});
                    THEN("item must be gathered immediately at {" + std::to_string(pos[j].x) + ", " +
                         std::to_string(pos[j].y) + "} " + step_str[i] + "step") {
                        auto result = FindGatherEvents(tfge);
                        CHECK(result.size() == 0);
                    }
                }
            }
        }
    }

    GIVEN("One item, many gatherers") {
        const double ga_width = 0.5;
        const double item_width = 0.5;

        WHEN("many gatherers get item at one time") {
            const std::vector<Gatherer> gas = {
                Gatherer({10.0, 3.9}, {10.2, 3.9}, ga_width),
                Gatherer({10.5, 3.9}, {10.0, 3.9}, ga_width),
                Gatherer({10.2, 3.9}, {10.0, 3.9}, ga_width),
                Gatherer({10.0, 3.4}, {10.2, 3.9}, ga_width)};
            THEN("check all gatherers can gather item") {
                const std::vector<Item> items = {Item(0, {10.0, 3.9}, 0, item_width)};
                TestFindGatherEvents tfge(items.size(), items, gas.size(), gas);
                auto result = FindGatherEvents(tfge);
                CHECK(result.size() == gas.size());
            }
        }
        WHEN("many gatherers gathering one item") {
            const std::vector<Gatherer> gas = {
                Gatherer({10.0, 3.9}, {10.2, 3.9}, ga_width),
                Gatherer({12.0, 3.9}, {10.0, 3.9}, ga_width),
                Gatherer({14.0, 3.9}, {10.0, 3.9}, ga_width),
                Gatherer({10.0, 13.9}, {10.0, 3.9}, ga_width)};
            THEN("check by first order") {
                const std::vector<Item> items = {Item(0, {10.0, 3.9}, 0)};
                const std::vector<GatheringEvent> gae = {
                    GatheringEvent(0, 0, 0., 0.),
                    GatheringEvent(0, 1, 0., 1.),
                    GatheringEvent(0, 2, 0., 1.),
                    GatheringEvent(0, 3, 0., 1.)};
                TestFindGatherEvents tfge(items.size(), items, gas.size(), gas);
                const std::vector<GatheringEvent> result = FindGatherEvents(tfge);
                CHECK(result == gae);
            }
        }
        WHEN("one of gatherer has no movement") {
            const std::vector<Gatherer> gas = {
                Gatherer({10.0, 3.9}, {10.0, 3.9}, ga_width),
                Gatherer({12.0, 3.9}, {10.0, 3.9}, ga_width),
                Gatherer({14.0, 3.9}, {10.0, 3.9}, ga_width),
                Gatherer({10.0, 13.9}, {10.0, 3.9}, ga_width)};
            THEN("check that zero gatherer has not got item") {
                const std::vector<Item> items = {Item(0, {10.0, 3.9}, 0)};
                const std::vector<GatheringEvent> gae = {
                    GatheringEvent(0, 1, 0., 1.),
                    GatheringEvent(0, 2, 0., 1.),
                    GatheringEvent(0, 3, 0., 1.)};
                TestFindGatherEvents tfge(items.size(), items, gas.size(), gas);
                const std::vector<GatheringEvent> result = FindGatherEvents(tfge);
                CHECK(result == gae);
            }
        }
    }

    GIVEN("Many items, one gatherer") {
        const double ga_width = 1.;
        const std::vector<Gatherer> gas = {Gatherer({8.0, 3.0}, {10.0, 3.0}, ga_width)};

        WHEN("getting one item") {
            const std::vector<Item> items = {Item(0, {1., 3.}, 0),
                                             Item(0, {10., 3.}, 1),
                                             Item(0, {1., 13.}, 2),
                                             Item(0, {16., 30.}, 3)};
            THEN("check getting item id = 1") {
                TestFindGatherEvents tfge(items.size(), items, gas.size(), gas);
                const std::vector<GatheringEvent> result = FindGatherEvents(tfge);
                REQUIRE(result.size() > 0);
                CHECK(result[0].item_id == 1);
            }
        }
        WHEN("getting many items") {
            const std::vector<Gatherer> gas = {Gatherer({8.0, 3.0}, {10.0, 3.0}, ga_width)};
            THEN("check getting 3 items") {
                const std::vector<Item> items = {Item(0, {9.6, 3.}, 0),
                                                 Item(0, {10., 3.}, 1),
                                                 Item(0, {10., 3.4}, 2),
                                                 Item(0, {16., 30.}, 3)};
                TestFindGatherEvents tfge(items.size(), items, gas.size(), gas);
                const std::vector<GatheringEvent> result = FindGatherEvents(tfge);
                CHECK(result.size() == 3);
                for (const auto &res : result) {
                    INFO("(" << res.gatherer_id << "," << res.item_id << "," << res.sq_distance << "," << res.time << ")");
                }
            }
        }
    }

    GIVEN("Many items, many gatherers") {
        const double ga_width = 1.;
        WHEN("getting many items with many gatherers") {
            const std::vector<Item> items = {Item(0, {9.6, 3.}, 0),
                                             Item(0, {10., 3.}, 1),
                                             Item(0, {12., 8.4}, 2),
                                             Item(0, {16., 30.}, 3)};
            const std::vector<Gatherer> gas = {
                Gatherer({16.0, 35.9}, {16.0, 30.}, ga_width),
                Gatherer({12.0, 8.9}, {12.0, 7.9}, ga_width),
                Gatherer({14.0, 3.4}, {10.0, 3.2}, ga_width),
                Gatherer({9.5, 6.9}, {9.5, 6.9}, ga_width)};
            TestFindGatherEvents tfge(items.size(), items, gas.size(), gas);
            THEN("check getting 3 items, one gatherer is not moving") {
                const std::vector<GatheringEvent> result = FindGatherEvents(tfge);
                CHECK(result.size() == 2);
                for (const auto &res : result) {
                    INFO("(" << res.gatherer_id << "," << res.item_id << "," << res.sq_distance << "," << res.time << ")");
                }
            }
        }
    }
}