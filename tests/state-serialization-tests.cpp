#include "../src/game_session.h"
#include "../src/model.h"
#include "../src/model_serialization.h"
#include "../src/players.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <catch2/catch_test_macros.hpp>
#include <iterator>
#include <sstream>

using namespace model;
using namespace std::literals;
namespace {

using InputArchive = boost::archive::text_iarchive;
using OutputArchive = boost::archive::text_oarchive;

struct Fixture {
    std::stringstream strm;
    OutputArchive output_archive{strm};
};

}  // namespace

namespace model {
bool operator==(const PickedObject& left, const PickedObject& right) {
    return (left.GetId() == right.GetId()) &&
           (left.GetType() == right.GetType());
}
} // namespace model

SCENARIO_METHOD(Fixture, "Position serialization") {
    GIVEN("A point") {
        const Position p{10.6, 20.};
        WHEN("position is serialized") {
            output_archive << p;

            THEN("it is equal to point position after serialization") {
                InputArchive input_archive{strm};
                Position restored_point;
                input_archive >> restored_point;
                CHECK(p == restored_point);
            }
        }
    }
}
SCENARIO_METHOD(Fixture, "Velocity serialization") {
    GIVEN("A point") {
        const Velocity p{-10, 20.9};
        WHEN("velocity is serialized") {
            output_archive << p;

            THEN("it is equal to point velocity after serialization") {
                InputArchive input_archive{strm};
                Velocity restored_point;
                input_archive >> restored_point;
                CHECK(p == restored_point);
            }
        }
    }
}
SCENARIO_METHOD(Fixture, "DogState serialization") {
    GIVEN("A dog state") {
        const DogState dog_state{{7., 15.3},{0., -2.5}, Direction::NORTH};
        WHEN("dog state is serialized") {
            output_archive << dog_state;

            THEN("it is equal to dog state after serialization") {
                InputArchive input_archive{strm};
                DogState restored_dog_state;
                input_archive >> restored_dog_state;
                CHECK(dog_state == restored_dog_state);
            }
        }
    }
}
SCENARIO_METHOD(Fixture, "TokenTag serialization") {
    GIVEN("A token tage") {
        const players::detail::TokenTag token_tag{12345678901234567890ul, 9876543210ul};
        WHEN("token tag is serialized") {
            output_archive << token_tag;

            THEN("it is equal to token_tag after serialization") {
                InputArchive input_archive{strm};
                players::detail::TokenTag restored_token_tag;
                input_archive >> restored_token_tag;
                CHECK(token_tag.tag[0] == restored_token_tag.tag[0]);
                CHECK(token_tag.tag[1] == restored_token_tag.tag[1]);
            }
        }
    }
}
SCENARIO_METHOD(Fixture, "Token serialization") {
    GIVEN("A Token") {
        const players::Token token{"145090b296f9e0079a15b166b797e479"};
        WHEN("Token is serialized") {
            {
                serialization::TokenRepr repr{token};
                output_archive << repr;
            }

            THEN("it is equal token after serialization") {
                InputArchive input_archive{strm};
                serialization::TokenRepr restored;
                input_archive >> restored;
                players::Token restored_token = restored.Restore();
                CHECK(token == restored_token);
            }
        }
    }
}
SCENARIO_METHOD(Fixture, "Picked object serialization") {
    GIVEN("A picked object") {
        const PickedObject picked_obj{123, 2};
        WHEN("Picked object is serialized") {
            {
                serialization::PickedObjectRepr repr{picked_obj};
                output_archive << repr;
            }

            THEN("it is equal picked object after serialization") {
                InputArchive input_archive{strm};
                serialization::PickedObjectRepr restored;
                input_archive >> restored;
                PickedObject restored_picked_obj = restored.Restore();
                CHECK(picked_obj.GetId() == restored_picked_obj.GetId());
                CHECK(picked_obj.GetType() == restored_picked_obj.GetType());
            }
        }
    }
}
SCENARIO_METHOD(Fixture, "Lost object serialization") {
    GIVEN("A lost object") {
        const LostObject lost_obj{1, {20.8, 10.}, 14700, 0.8};
        WHEN("Lost object is serialized") {
            {
                serialization::LostObjectRepr repr{lost_obj};
                output_archive << repr;
            }

            THEN("it is equal lost object after serialization") {
                InputArchive input_archive{strm};
                serialization::LostObjectRepr restored;
                input_archive >> restored;
                LostObject restored_lost_obj = restored.Restore();
                CHECK(lost_obj.GetType() == restored_lost_obj.GetType());
                CHECK(lost_obj.GetPosition() == restored_lost_obj.GetPosition());
                CHECK(lost_obj.GetId() == restored_lost_obj.GetId());
                CHECK(lost_obj.GetWidth() == restored_lost_obj.GetWidth());
            }
        }
    }
}
SCENARIO_METHOD(Fixture, "Dog Serialization") {
    GIVEN("a dog") {
        const auto dog = [] {
            Dog dog{42, "Pluto"s, {42.2, 12.5}};
            dog.AddPickedObject(PickedObject{123, 2}, 4);
            dog.AddPickedObject(PickedObject{1, 0}, 4);
            dog.AddScores(42);
            dog.SetDirection(Direction::EAST);
            dog.SetState({{7., 15.3}, {0., -2.5}, Direction::NORTH});
            return dog;
        }();

        WHEN("dog is serialized") {
            {
                serialization::DogRepr repr{dog};
                output_archive << repr;
            }

            THEN("dog can be deserialized") {
                InputArchive input_archive{strm};
                serialization::DogRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore();

                CHECK(dog.GetDogId() == restored.GetDogId());
                CHECK(dog.GetDogName() == restored.GetDogName());
                CHECK(dog.GetDogState() == restored.GetDogState());
                CHECK(dog.GetPickedObjects() == restored.GetPickedObjects());
                CHECK(dog.GetScores() == restored.GetScores());
            }
        }
    }
}
SCENARIO_METHOD(Fixture, "Player, players Serialization") {
    GIVEN("a player, players") {
        const auto dog = [] {
            Dog dog{42, "Pluto"s, {42.2, 12.5}};
            dog.AddPickedObject(PickedObject{123, 2}, 4);
            dog.AddPickedObject(PickedObject{1, 0}, 4);
            dog.AddScores(42);
            dog.SetDirection(Direction::EAST);
            dog.SetState({{7., 15.3}, {0., -2.5}, Direction::NORTH});
            return dog;
        }();
        Map map{Map::Id{"town"}, "Town map", 4., 3};
        map.AddRoad({model::Road::HORIZONTAL, {0, 0}, 100});
        std::shared_ptr<GameSession> game_session = std::make_shared<GameSession>(&map);
        std::vector<model::Game::Sessions> sessions;
        sessions.emplace_back(game_session);
        const players::Player player(dog, game_session);

        WHEN("player is serialized") {
            {
                serialization::PlayerRepr repr{player};
                output_archive << repr;
            }

            THEN("player can be deserialized") {
                InputArchive input_archive{strm};
                serialization::PlayerRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore(sessions);

                CHECK(player.GetDog()->GetDogId() == restored.GetDog()->GetDogId());
                CHECK(player.GetGameSession()->GetMap()->GetId() == restored.GetGameSession()->GetMap()->GetId());
            }
        }

        players::Players game_players;
        game_players.Add("Pluto"s, game_session, false);
        game_players.Add("Meeto"s, game_session, false);
        game_players.Add("r1234"s, game_session, false);
        WHEN("players are serialized") {
            {
                serialization::PlayersRepr repr{game_players};
                output_archive << repr;
            }

            THEN("players can be deserialized") {
                InputArchive input_archive{strm};
                serialization::PlayersRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore(sessions);

                CHECK(game_players.GetNextDogId() == restored.GetNextDogId());
                REQUIRE(game_players.GetPlayers().size() == restored.GetPlayers().size());
                for (auto it1 = game_players.GetPlayers().begin(), it2 = restored.GetPlayers().begin();
                        it1 != game_players.GetPlayers().end(); ++it1, ++it2) {
                    CHECK((*it1)->GetName() == (*it2)->GetName());
                    CHECK((*it1)->GetGameSession()->GetMap()->GetId() ==
                          (*it2)->GetGameSession()->GetMap()->GetId());
                }
            }
        }
    }
}
SCENARIO_METHOD(Fixture, "PlayerTokens Serialization") {
    GIVEN("a PlayerTokens session") {
        Map map{Map::Id{"town"}, "Town map", 4., 3};
        std::shared_ptr<GameSession> game_session = std::make_shared<GameSession>(&map);
        std::vector<model::Dog> dog;
        players::Players::PlayersAll game_players_cntnr;
        players::PlayerTokens player_tokens;
        for (size_t i = 0; i != 4; ++i) {
            dog.emplace_back([&i] {
                Dog dog{42 + i, "Pluto"s + std::to_string(i), {42.2, 12.5}};
                dog.AddPickedObject(PickedObject{123, 2}, 4);
                dog.AddPickedObject(PickedObject{1, 0}, 4);
                dog.AddScores(42);
                dog.SetDirection(Direction::EAST);
                dog.SetState({{7., 15.3}, {0., -2.5}, Direction::NORTH});
                return dog;
            }());
            auto it = game_players_cntnr.emplace(std::cend(game_players_cntnr), std::make_shared<players::Player>(
                                        dog[i], game_session));
            player_tokens.AddPlayer(*it);
        }
        std::vector<Game::Sessions> sessions;
        sessions.emplace_back(game_session);
        players::Players ps(game_players_cntnr, game_players_cntnr.size());

        WHEN("PlayerTokens is serialized") {
            {
                serialization::PlayerTokensRepr repr{player_tokens};
                output_archive << repr;
            }

            THEN("game session can be deserialized") {
                InputArchive input_archive{strm};
                serialization::PlayerTokensRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore(ps);

                REQUIRE(player_tokens.GetTokenToPlayers().size() ==
                        restored.GetTokenToPlayers().size());
                for (const auto &[token, player_ptr] : player_tokens.GetTokenToPlayers()) {
                    CHECK(player_ptr->GetId() == restored.FindPlayerByToken(token)->GetId());
                }
            }
        }
    }
}
SCENARIO_METHOD(Fixture, "GameSession Serialization") {
    GIVEN("a game session") {
        Map map{Map::Id("town"), "Some Town", 5.5, 2u};
        Game game;
        game.AddMap(map);
        auto session_ptr = game.PlacePlayerOnMap(map.GetId());
        GameSession::LostObjects lost_objects;
        for (size_t i = 0; i != 4; ++i) {
            Dog dog{42 + i, "Pluto"s + std::to_string(i), {42.2, 12.5}};
            session_ptr->AddDog(dog.GetDogId());
            lost_objects.emplace_back(std::make_shared<LostObject>(LostObject(1, {20.8, 10. + i}, i, 0.8)));
        }
        session_ptr->RestoreLostObjects(lost_objects, 4);

        WHEN("game session is serialized") {
            {
                serialization::GameSessionRepr repr{*session_ptr};
                output_archive << repr;
            }

            THEN("game session can be deserialized") {
                InputArchive input_archive{strm};
                serialization::GameSessionRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore(game);

                REQUIRE(session_ptr->GetDogIds().size() == restored.GetDogIds().size());
                REQUIRE(session_ptr->GetLostObjects().size() == restored.GetLostObjects().size());
                CHECK(session_ptr->GetLastObjectId() == restored.GetLastObjectId());
                auto s_it = session_ptr->GetLostObjects().begin();
                auto r_it = restored.GetLostObjects().begin();
                for (auto it1 = session_ptr->GetDogIds().begin(), it2 = restored.GetDogIds().begin();
                        it1 != session_ptr->GetDogIds().end(); ++it1, ++it2, ++s_it, ++r_it) {
                    CHECK(*it1 == *it2);
                    CHECK((*s_it)->GetId() == (*r_it)->GetId());
                }
            }
        }
    }
}
