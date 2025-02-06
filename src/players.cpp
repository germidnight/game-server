#include "players.h"
#include "model_serialization.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace players {

namespace detail {
    // длина строки всегда 32 hex цифры (128 бит)
    std::string TokenTag::Serialize() const {
        const size_t len_of_64bit_word_in_hex = 16;
        std::ostringstream os;
        for (int i = 0; i < 2; ++i) {
            os << std::hex << std::setfill('0') << std::setw(len_of_64bit_word_in_hex) << tag[i];
        }
        return os.str();
    }

} // namespace detail

    /* ----------------------------------- PlayerTokens ----------------------------------- */

    std::shared_ptr<Token> PlayerTokens::AddPlayer(std::shared_ptr<Player> player) {
        Token token(detail::TokenTag{generator1_(), generator2_()}.Serialize());
        while (token_to_player_.count(token) > 0) {
            token = Token(detail::TokenTag{generator1_(), generator2_()}.Serialize());
        }
        token_to_player_[token] = std::move(player);
        return std::make_shared<Token>(token);
    }

    std::shared_ptr<Player> PlayerTokens::FindPlayerByToken(const Token& token) const noexcept {
        if (token_to_player_.count(token) > 0) {
            return token_to_player_.at(token);
        }
        return nullptr;
    }

    void PlayerTokens::AddRestoredToken(const Token& token, std::shared_ptr<Player> player) {
        token_to_player_[token] = std::move(player);
    }

    void PlayerTokens::Delete(std::shared_ptr<Player> player) {
        auto it = std::find_if(token_to_player_.begin(), token_to_player_.end(),
                                [&player](const auto& val) {
                                    return val.second == player;
                                });
        if (it != token_to_player_.end()) {
            token_to_player_.erase(it);
        }
    }

    /* ----------------------------------- Players ----------------------------------- */

    Players::Players(PlayersAll game_players, size_t next_dog_id)
        : next_dog_id_(next_dog_id) {
        for (auto player : game_players) {
            auto it = players_.emplace(std::cend(players_), player);
            map_id_to_it_.emplace((*it)->GetDog()->GetDogId(), it);
        }
    }

    /* Создание и добавление пользователя:
     * 1) создаём и добавляем игрока с собакой в перечень игроков
     * 2) в выбранной игровой сессии добавляем собаку нового игрока */
    std::shared_ptr<Player> Players::Add(std::string player_name,
                                         std::shared_ptr<model::GameSession> game_session,
                                         bool randomize_spawn_point) {
        model::Position position;
        if (randomize_spawn_point) {
            position = game_session->GetMap()->GetRandomPositionOnRoads();
        } else {
            position = game_session->GetMap()->GetTestPositionOnRoads();
        }
        auto it = players_.emplace(std::cend(players_), std::make_shared<Player>(model::Dog(++next_dog_id_,
                                                                                            player_name,
                                                                                            position),
                                                                                game_session));
        game_session->AddDog((*it)->GetDog()->GetDogId());
        map_id_to_it_.emplace((*it)->GetDog()->GetDogId(), it);
        return *it;
    }

    std::shared_ptr<Player> Players::FindPlayerByDogId(size_t dog_id) const {
        if (map_id_to_it_.count(dog_id) > 0) {
            return *(map_id_to_it_.at(dog_id));
        }
        return nullptr;
    }

    void Players::Delete(std::shared_ptr<Player> player) {
        auto id = player->GetId();
        if (map_id_to_it_.count(id) > 0) {
            auto it = map_id_to_it_.at(id);
            players_.erase(it);
            map_id_to_it_.erase(id);
        }
    }

    /* ----------------------------------- Application ----------------------------------- */

    /* Добавляем нового пользователя в игру
     * 1) находим карту
     * 2) получаем игровую сессию
     * 3) добавляем пользователя
     * 4) получаем токен пользователя */
    JoinGameResult Application::JoinPlayerToGame(model::Map::Id map_id, std::string_view player_name) {
        const model::Map* map = game_.FindMap(map_id);
        if (map == nullptr) {
            return JoinGameResult(nullptr, 0, JoinGameErrorCode::MAP_NOT_FOUND);
        }

        auto game_session = game_.PlacePlayerOnMap(map->GetId());
        if (game_session == nullptr) {
            return JoinGameResult(nullptr, 0, JoinGameErrorCode::SESSION_NOT_FOUND);
        }

        auto player = players_.Add(std::string(player_name), game_session, IsRandomSpawnPoint());
        return JoinGameResult(player_tokens_.AddPlayer(player), player->GetDog()->GetDogId(), JoinGameErrorCode::NONE);
    }

    std::vector<std::shared_ptr<Player>> Application::GetPlayersInSession(const std::shared_ptr<Player> player) const {
        std::vector<std::shared_ptr<Player>> res_players;

        auto session = player->GetGameSession();
        for (const auto next_dog : session->GetDogIds()) {
            auto next_player = players_.FindPlayerByDogId(next_dog);
            if (next_player != nullptr) {
                res_players.push_back(next_player);
            }
        }
        return res_players;
    }

    void Application::SetDogAction(std::shared_ptr<Player> player, ActionMove action_move) {
        auto dog_speed = player->GetGameSession()->GetMap()->GetSpeed();
        auto dog = player->GetDog();
        switch (action_move) {
        case ActionMove::LEFT : {
            dog->SetVelocity({-dog_speed, 0.});
            dog->SetDirection(model::Direction::WEST);
            break;
        }
        case ActionMove::RIGHT : {
            dog->SetVelocity({dog_speed, 0.});
            dog->SetDirection(model::Direction::EAST);
            break;
        }
        case ActionMove::UP : {
            dog->SetVelocity({0., -dog_speed});
            dog->SetDirection(model::Direction::NORTH);
            break;
        }
        case ActionMove::DOWN : {
            dog->SetVelocity({0., dog_speed});
            dog->SetDirection(model::Direction::SOUTH);
            break;
        }
        default: {
            dog->SetVelocity({0., 0.});
            break;
        }
        }
    }

    /* Пересчёт событий на карте:
     * 1) двигаем собак по картам
     * 1.1) учитываем общее время в игре
     * 1.2) учитываем время неактивности игрока
     * 1.3) помечаем игрока на удаление при неактивности
     * 2) размещаем потерянные объекты в сессиях
     * 3) отдаём находки в офис
     * 4) подбираем предметы
     * 5) удаляем неактивных игроков */
    void Application::MoveDogs(double time_period) {
        using namespace std::chrono_literals;
        loot_gen::LootGenerator::TimeInterval duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                std::chrono::duration<double, std::milli>{time_period * 1s});

        std::unordered_map<size_t, collision_detector::Gatherer> all_gatherers;
        std::vector<std::shared_ptr<Player>> delete_this;
        // перебор всех игроков
        for (auto player : players_.GetPlayers()) {
            model::DogState state = player->GetGameSession()->GetMap()->MoveDog(player->GetDog(), time_period);

            all_gatherers[player->GetDog()->GetDogId()] = collision_detector::Gatherer{
                                        player->GetDog()->GetDogState().position,
                                        state.position,
                                        model::LostObject::GATHERER_HALF_WIDTH};

            player->GetDog()->IncTotalTime(time_period);
            if (state == player->GetDog()->GetDogState()) {
                player->GetDog()->IncInactiveTime(time_period);
            } else {
                player->GetDog()->ResetInactiveTime();
            }
            player->GetDog()->SetState(state);
            if (player->GetDog()->GetInactiveTime() >= game_.GetDogRetirementTime()) {
                delete_this.push_back(player);
            }
        }
        // перебор всех сессий
        for (auto& session : game_.GetSessions()) {
            if(session != nullptr) {
                // размещаем потерянные объекты в сессии
                session->AddLostObjectsOnSession(game_.GetLootGenerator(), duration);

                std::vector<collision_detector::Gatherer> gatherers;
                std::unordered_map<size_t, std::shared_ptr<model::Dog>> idx_to_dog;
                for (const auto& dog_id : session->GetDogIds()) {
                    gatherers.emplace_back(all_gatherers.at(dog_id));
                    idx_to_dog[gatherers.size() - 1] = GetDogById(dog_id);
                }
                BringItemsToOffices(session, gatherers, idx_to_dog);
                PickUpItems(session, gatherers, idx_to_dog);
            }
        }
        // Удаляем неактивных игроков
        for (auto& player : delete_this) {
            Champion player_result{player->GetName(),
                                   player->GetDog()->GetScores(),
                                   player->GetDog()->GetTotalTime()};
            app_repo_.Save(player_result);
            DeletePlayer(player);
        }
    }

    /* подбираем предметы:
     * 1) формируем вектор items (gatherers сформирован выше) и передаём их провайдеру
     * 2) получаем вектор событий подбора вещей собаками
     * 3) для каждого события:
     *      - подбираем собаками ещё не подобранные вещи, не забывая пометить подобранные вещи
     * 4) удаляем подобранные вещи из списка потерянных */
    void Application::PickUpItems(std::shared_ptr<model::GameSession> session,
                                  const std::vector<collision_detector::Gatherer>& gatherers,
                                  const std::unordered_map<size_t, std::shared_ptr<model::Dog>>& idx_to_dog) {
        std::vector<std::shared_ptr<model::LostObject>> items(session->GetLostObjects().begin(),
                                                              session->GetLostObjects().end());
        ItemGatherer ig(items.size(), items, gatherers.size(), gatherers);
        std::vector<bool> item_picked(session->CountLostObjects(), false);

        for (const auto &event : collision_detector::FindGatherEvents(ig)) {
            if (!item_picked[event.item_id]) {
                item_picked[event.item_id] = idx_to_dog.at(event.gatherer_id)->AddPickedObject(
                                                        model::PickedObject(items[event.item_id]->GetId(),
                                                                            items[event.item_id]->GetType()),
                                                        session->GetMap()->GetBagCapacity());
            }
        }
        session->RemoveObjectsFromLost(item_picked); // удаляем только подобранные вещи
    }

    /* отдаём находки в офис
     * 1) формируем вектор offices (gatherers сформирован выше) и передаём их провайдеру
     * 2) получаем вектор событий посещения собаками офисов
     * 3) для каждого события:
     *      - сбрасываем все подобранные вещи */
    void Application::BringItemsToOffices(std::shared_ptr<model::GameSession> session,
                         const std::vector<collision_detector::Gatherer>& gatherers,
                         const std::unordered_map<size_t, std::shared_ptr<model::Dog>>& idx_to_dog) {
        std::vector<std::shared_ptr<model::LostObject>> offices;
        for (const auto& office : session->GetMap()->GetOffices()) {
            offices.emplace_back(std::make_shared<model::LostObject>(model::LostObject(0,
                                                        {static_cast<double>(office.GetPosition().x),
                                                        static_cast<double>(office.GetPosition().y)},
                                                   offices.size(),
                                                   model::LostObject::OFFICE_HALF_WIDTH)));
        }
        ItemGatherer og(offices.size(), offices, gatherers.size(), gatherers);
        for (const auto& event : collision_detector::FindGatherEvents(og)) {
            auto dog = idx_to_dog.at(event.gatherer_id);
            if (dog->IsBagEmpty()) {
                continue;
            }
            for(const auto& obj : dog->FlushPickedObjects()) {
                dog->AddScores(session->GetMap()->GetLootByIndex(obj.GetType()).GetScores());
            }
        }
    }

    /* Удаляем игрока:
     * 1) Удаляем из Players
     * 2) Удаляем из PlayerTokens
     * 3) Удаляем из GameSessions
     * 4) Удаляем пользователя (собака и подобранные объекты удалятся сами) */
    void Application::DeletePlayer(std::shared_ptr<Player> player) {
        players_.Delete(player);
        player_tokens_.Delete(player);
        player->GetGameSession()->DeleteDog(player->GetDog()->GetDogId());
        //player.reset();
    }

    /* Считывает из БД результаты игроков от номера start в количестве не более max_items */
    std::vector<Champion> Application::GetChampions(size_t start, size_t max_items) const {
        return app_repo_.GetChampions(start, max_items);
    }

std::stringstream SerializeState(const Application& app) {
    std::stringstream strm;
    OutputArchive output_archive{strm};

    std::vector<serialization::GameSessionRepr> session_repr_vec;
    for (const auto &session_ptr : app.game_.GetSessions()) {
        if (session_ptr != nullptr) {
            session_repr_vec.emplace_back(*session_ptr);
        }
    }
    output_archive << session_repr_vec;
    output_archive << serialization::PlayersRepr(app.players_);
    output_archive << serialization::PlayerTokensRepr(app.player_tokens_);

    return strm;
}

void DeserializeState(std::stringstream& strm, Application& app) {
    InputArchive input_archive{strm};

    std::vector<serialization::GameSessionRepr> sessions_repr_vec;
    input_archive >> sessions_repr_vec;
    std::vector<model::Game::Sessions> sessions;
    for (auto& session_repr : sessions_repr_vec) {
        sessions.emplace_back(std::make_shared<model::GameSession>(session_repr.Restore(app.game_)));
    }
    app.game_.RestoreSessions(std::move(sessions));

    serialization::PlayersRepr players_repr;
    input_archive >> players_repr;
    app.players_ = std::move(players_repr.Restore(app.game_.GetSessions()));

    serialization::PlayerTokensRepr tokens_repr;
    input_archive >> tokens_repr;
    app.player_tokens_ = std::move(tokens_repr.Restore(app.players_));
}

void AutosaveState(Application& app, std::string_view file_name) {
    std::filesystem::path temporary_file = std::filesystem::path(file_name).parent_path() / "temporary";

    std::ofstream autosave_file(temporary_file, std::ios::trunc);
    autosave_file << players::SerializeState(app).str();
    autosave_file.close();
    std::filesystem::rename(temporary_file, {file_name});
}

} // namespace players
