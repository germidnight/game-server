/*
 * Расчёт столкновений объектов, основная функция FindGatherEvents()
 * Для её запуска нужно создать объект класса унаследованного от ItemGathererProvider.
 * Возвращаемое значение: вектор событий столкновения собак (gatherer) и потерянных вещей (item),
 * идущих в хронологическом порядке от первого к последнему
 */
#pragma once

#include "game_session.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace collision_detector {

    struct CollectionResult {
        bool IsCollected(double collect_radius) const {
            return proj_ratio >= 0 && proj_ratio <= 1 && sq_distance <= collect_radius * collect_radius;
        }

        double sq_distance; // квадрат расстояния до точки
        double proj_ratio;  // доля пройденного отрезка
    };

    /* Движемся из точки a в точку b и пытаемся подобрать точку c (предмет).
     * Функция корректно работает только при условии ненулевого перемещения. */
    CollectionResult TryCollectPoint(model::Position a, model::Position b, model::Position c);

    using Item = model::LostObject;

    struct Gatherer {
        model::Position start_pos;
        model::Position end_pos;
        double width;
    };

    class ItemGathererProvider {
    protected:
        ~ItemGathererProvider() = default;

    public:
        virtual size_t ItemsCount() const = 0;
        virtual Item GetItem(size_t idx) const = 0;
        virtual size_t GatherersCount() const = 0;
        virtual Gatherer GetGatherer(size_t idx) const = 0;
    };

    struct GatheringEvent {
        size_t item_id;
        size_t gatherer_id;
        double sq_distance;
        double time;
    };

    /* Функция возвращает вектор событий, идущих в хронологическом порядке.
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
    std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider &provider);

} // namespace collision_detector