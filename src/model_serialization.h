#pragma once
#include <boost/serialization/list.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>
#include <algorithm>
#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

#include "game_session.h"
#include "model.h"
#include "tagged.h"
#include "players.h"

namespace model {

template <typename Archive>
void serialize(Archive& ar, Position& point, [[maybe_unused]] const unsigned version) {
    ar& point.x;
    ar& point.y;
}

template <typename Archive>
void serialize(Archive& ar, Velocity& vec, [[maybe_unused]] const unsigned version) {
    ar& vec.x;
    ar& vec.y;
}

template <typename Archive>
void serialize(Archive& ar, DogState& state, [[maybe_unused]] const unsigned version) {
    ar&(state.position);
    ar&(state.velocity);
    ar&(state.direction);
}

} // namespace model

namespace players {
    namespace detail {
        template <typename Archive>
        void serialize(Archive& ar, players::detail::TokenTag& token_tag, [[maybe_unused]] const unsigned version) {
            ar &(token_tag.tag);
        }
    } // namespace detail
} // namespace players

namespace serialization {

class TokenRepr {
public:
    TokenRepr() = default;

    explicit TokenRepr(const players::Token& token)
        : value_(*token) {}

    [[nodiscard]] players::Token Restore() const {
        players::Token token{value_};
        return token;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& value_;
    }

private:
    std::string value_;
};
class PickedObjectRepr {
public:
    PickedObjectRepr() = default;

    explicit PickedObjectRepr(const model::PickedObject& obj)
                : id_(obj.GetId())
                , type_(obj.GetType()) {}

    [[nodiscard]] model::PickedObject Restore() const {
        model::PickedObject obj{id_, type_};
        return obj;
    }
    template <typename Archive>
    void serialize(Archive &ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& type_;
    }

private:
    size_t id_;
    size_t type_;
};
class LostObjectRepr {
public:
    LostObjectRepr() = default;

    explicit LostObjectRepr(const model::LostObject& obj)
                : type_(obj.GetType())
                , position_(obj.GetPosition())
                , id_(obj.GetId())
                , width_(obj.GetWidth()) {}

    [[nodiscard]] model::LostObject Restore() const {
        model::LostObject obj{type_,
                              position_,
                              id_,
                              width_};
        return obj;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& type_;
        ar& position_;
        ar& id_;
        ar& width_;
    }

private:
    size_t type_;
    model::Position position_;
    size_t id_;
    double width_;
};
class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const model::Dog& dog)
                : id_(dog.GetDogId())
                , name_(dog.GetDogName())
                , state_(dog.GetDogState())
                , scores_(dog.GetScores()) {
        for (const auto&  obj : dog.GetPickedObjects()) {
            objects_repr_.emplace_back(PickedObjectRepr(obj));
        }
    }

    [[nodiscard]] model::Dog Restore() const {
        std::vector<model::PickedObject> objects;
        for (auto& obj_repr : objects_repr_) {
            objects.emplace_back(obj_repr.Restore());
        }
        model::Dog dog{id_,
                       std::move(name_),
                       std::move(state_),
                       std::move(objects),
                       scores_,
                       inactive_time_,
                       total_time_};
        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& name_;
        ar& state_;
        ar& scores_;
        ar& objects_repr_;
        ar& inactive_time_;
        ar& total_time_;
    }

private:
    size_t id_;
    std::string name_;
    model::DogState state_;
    size_t scores_;
    std::vector<PickedObjectRepr> objects_repr_;
    double inactive_time_;
    double total_time_;
};

/* В игроках сохранены:
 * - собака
 * - идентификатор карты сессии (одна сессия на одну карту) */
class PlayerRepr {
public:
    PlayerRepr() = default;

    explicit PlayerRepr(const players::Player& player)
                : dog_repr_(DogRepr(*(player.GetDog())))
                , map_id_string_(*(player.GetGameSession()->GetMap()->GetId())) {}

    [[nodiscard]] players::Player Restore(const std::vector<model::Game::Sessions>& sessions) const {
        auto session_it = std::find_if(sessions.begin(), sessions.end(),
                            [this](const model::Game::Sessions& s) {
                                if (s != nullptr) {
                                    return (*(s->GetMap()->GetId()) == this->map_id_string_);
                                }
                                return false;
                            });
        if (session_it == sessions.end()) {
            throw std::domain_error("Restore Player failed, no such session");
        }
        players::Player player{dog_repr_.Restore(), *session_it};
        return player;
    }

    template <typename Archive>
    void serialize(Archive &ar, [[maybe_unused]] const unsigned version) {
        ar& dog_repr_;
        ar& map_id_string_;
    }

private:
    DogRepr dog_repr_;
    std::string map_id_string_;
};

class PlayersRepr {
public:
    PlayersRepr() = default;

    explicit PlayersRepr(const players::Players& game_players)
        : next_dog_id_(game_players.GetNextDogId()) {
        for (const auto& player : game_players.GetPlayers()){
            players_.emplace_back(PlayerRepr(*player));
        }
    }

    [[nodiscard]] players::Players Restore(const std::vector<model::Game::Sessions>& sessions) const {
        players::Players::PlayersAll game_players;
        for (auto& player_repr : players_) {
            game_players.emplace_back(std::make_shared<players::Player>(player_repr.Restore(sessions)));
        }
        return players::Players(std::move(game_players), next_dog_id_);
    }

    template <typename Archive>
    void serialize(Archive &ar, [[maybe_unused]] const unsigned version) {
        ar& players_;
        ar& next_dog_id_;
    }

private:
    std::vector<PlayerRepr> players_;
    size_t next_dog_id_;
};

class PlayerTokensRepr {
public:
    PlayerTokensRepr() = default;

    explicit PlayerTokensRepr(const players::PlayerTokens& tokens) {
        for (const auto& [token, player_ptr] : tokens.GetTokenToPlayers()) {
            player_id_to_token_str_.emplace(player_ptr->GetId(), TokenRepr(token));
        }
    }

    [[nodiscard]] players::PlayerTokens Restore(const players::Players& game_players) const {
        players::PlayerTokens player_tokens;
        for (const auto& player : game_players.GetPlayers()) {
            if (player_id_to_token_str_.count(player->GetId()) == 0) {
                throw std::domain_error("Restore PlayerTokens failed, no such player_id in file");
            }
            player_tokens.AddRestoredToken(player_id_to_token_str_.at(player->GetId()).Restore(),
                                            player);
        }
        return player_tokens;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& player_id_to_token_str_;
    }

private:
    std::unordered_map<size_t, TokenRepr> player_id_to_token_str_; // dog_id; token
};

class GameSessionRepr {
public:
    GameSessionRepr() = default;

    explicit GameSessionRepr(const model::GameSession& session)
                : map_id_str_(*(session.GetMap()->GetId()))
                , last_object_id_(session.GetLastObjectId())
                , dog_ids_(session.GetDogIds()) {
        for (const auto& obj_ptr : session.GetLostObjects()) {
            lost_objects_repr_.emplace_back(LostObjectRepr(*obj_ptr));
        }
    }

    [[nodiscard]] model::GameSession Restore(const model::Game& game) const {
        model::GameSession session(const_cast<model::Map*>(game.FindMap(model::Map::Id{map_id_str_})));
        for (const size_t dog_id : dog_ids_) {
            session.AddDog(dog_id);
        }
        model::GameSession::LostObjects lost_objects;
        for (auto& obj_repr : lost_objects_repr_) {
            lost_objects.emplace_back(std::make_shared<model::LostObject>(obj_repr.Restore()));
        }
        session.RestoreLostObjects(std::move(lost_objects), last_object_id_);
        return session;
    }

    template <typename Archive>
    void serialize(Archive &ar, [[maybe_unused]] const unsigned version) {
        ar& map_id_str_;
        ar& last_object_id_;
        ar& dog_ids_;
        ar& lost_objects_repr_;
    }

private:
    std::string map_id_str_;
    size_t last_object_id_;
    model::GameSession::DogIds dog_ids_;
    std::vector<LostObjectRepr> lost_objects_repr_;
};

} // namespace serialization
