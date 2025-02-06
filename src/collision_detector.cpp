#include "collision_detector.h"
#include <cassert>

namespace collision_detector {

    /* Собака двигается из точки А в точку В, предмет лежит в точке С.
     * Проверим, что перемещение ненулевое.
     * Тут приходится использовать строгое равенство, а не приближённое,
     * поскольку при сборе заказов придётся учитывать перемещение даже на небольшое
     * расстояние. */
    CollectionResult TryCollectPoint(model::Position a, model::Position b, model::Position c) {
        assert(b.x != a.x || b.y != a.y);
        const double u_x = c.x - a.x;
        const double u_y = c.y - a.y;
        const double v_x = b.x - a.x;
        const double v_y = b.y - a.y;
        const double u_dot_v = u_x * v_x + u_y * v_y;
        const double u_len2 = u_x * u_x + u_y * u_y;
        const double v_len2 = v_x * v_x + v_y * v_y;
        const double proj_ratio = u_dot_v / v_len2;
        const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

        return CollectionResult(sq_distance, proj_ratio);
    }

    std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider) {
        std::vector<GatheringEvent> events;

        for (size_t g_idx = 0; g_idx != provider.GatherersCount(); ++g_idx) {
            const auto &gatherer = provider.GetGatherer(g_idx);
            if ((gatherer.start_pos.x == gatherer.end_pos.x) &&
                (gatherer.start_pos.y == gatherer.end_pos.y)) {
                continue;
            }
            for (size_t i_idx = 0; i_idx != provider.ItemsCount(); ++i_idx) {
                auto coll_res = TryCollectPoint(gatherer.start_pos,
                                                gatherer.end_pos,
                                                provider.GetItem(i_idx).GetPosition());
                if (coll_res.IsCollected(gatherer.width + provider.GetItem(i_idx).GetWidth())) {
                    events.emplace_back(GatheringEvent(i_idx,
                                                       g_idx,
                                                       coll_res.sq_distance,
                                                       coll_res.proj_ratio));
                }
            }
        }
        std::sort(events.begin(), events.end(), [](const GatheringEvent &left, const GatheringEvent &right) {
            return left.time < right.time;
        });
        return events;
    }

} // namespace collision_detector