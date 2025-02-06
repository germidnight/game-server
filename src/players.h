/*
 * Реализует основные прикладные сценарии использования:
 * - вход в игру;
 * - получение списка игроков;
 * - запрос игрового состояния.
 * Хранит информацию об игроках и их токенах авторизации
 * Класс Application реализует паттерн "фасад" для модели игры
 */
#pragma once
#include "collision_detector.h"
#include "tagged.h"
#include "model.h"

#include <algorithm>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <iterator>
#include <list>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>

namespace players {

    namespace detail {
        struct TokenTag {
            TokenTag() = default;

            TokenTag(uint64_t first, uint64_t second) : tag{first, second} {}

            std::string Serialize() const; // длина строки всегда 32 hex цифры (128 бит)

            uint64_t tag[2];
        };
    } // namespace detail

    using Token = util::Tagged<std::string, detail::TokenTag>;

    class Player;

    /* ----------------- Все токены игроков собраны тут ----------------- */
    class PlayerTokens {
    public:
        using TokenHasher = util::TaggedHasher<Token>;
        using TokenToPlayer = std::unordered_map<Token, std::shared_ptr<Player>, TokenHasher>;

        PlayerTokens() = default;

        PlayerTokens(const PlayerTokens&) = delete;
        PlayerTokens(PlayerTokens&& other) noexcept {
            token_to_player_ = std::move(other.token_to_player_);
            other.token_to_player_ = {};
        }

        PlayerTokens& operator=(const PlayerTokens&) = delete;
        PlayerTokens& operator=(PlayerTokens&& other) noexcept {
            if (this != &other) {
                token_to_player_ = std::move(other.token_to_player_);
                other.token_to_player_ = {};
            }
            return *this;
        }
        ~PlayerTokens() = default;

        /* Должны ли имена пользователей (собак) быть уникальными?
         * Нужно ли имена пользователей проверять перед добавлением?
         * На всякий случай (хоть и 128 бит это очень много), но проверяем не сгенерировался ли повторяющийся токен*/
        std::shared_ptr<Token> AddPlayer(std::shared_ptr<Player> player);

        std::shared_ptr<Player> FindPlayerByToken(const Token& token) const noexcept;

        const TokenToPlayer& GetTokenToPlayers() const noexcept {
            return token_to_player_;
        }

        void AddRestoredToken(const Token& token, std::shared_ptr<Player> player);

        void Delete(std::shared_ptr<Player> player);

    private:
        std::random_device random_device_;
        std::mt19937_64 generator1_{[this] {
            std::uniform_int_distribution<std::mt19937_64::result_type> dist;
            return dist(random_device_);
        }()};
        std::mt19937_64 generator2_{[this] {
            std::uniform_int_distribution<std::mt19937_64::result_type> dist;
            return dist(random_device_);
        }()};

        TokenToPlayer token_to_player_;
    };

    /* ------------------------------------------- Игрок ------------------------------------------- */
    class Player {
    public:
        /*
         * Создание пользователя:
         * 1) создаём собаку для пользователя
         * 2) сохраняем игровую сессию пользователя
         */
        explicit Player(model::Dog dog, std::shared_ptr<model::GameSession> game_session)
                        : dog_(std::make_shared<model::Dog>(dog))
                        , session_(std::move(game_session)) {}

        std::shared_ptr<model::Dog> GetDog() const noexcept {
            return dog_;
        }

        size_t GetId() const noexcept {
            return dog_->GetDogId();
        }

        const std::string& GetName() const noexcept {
            return dog_->GetDogName();
        }

        std::shared_ptr<model::GameSession> GetGameSession() const noexcept {
            return session_;
        }

    private:
        std::shared_ptr<model::Dog> dog_;
        std::shared_ptr<model::GameSession> session_;
    };

    /* --------------------------------------- Перечень всех игроков --------------------------------------- */
    /*
     * Здесь храним:
     * 1) перечень всех пользователей в игре
     * 2) перечень токенов пользователей
     * - dog_id - будет уникальный для всех созданных игроков
     */
    class Players {
    public:
        using PlayersAll = std::list<std::shared_ptr<Player>>;
        using PlayerIt = PlayersAll::const_iterator;
        Players() = default;

        explicit Players(PlayersAll players, size_t next_dog_id_);

        /* Добавление пользователя */
        std::shared_ptr<Player> Add(std::string player_name,
                                    std::shared_ptr<model::GameSession> game_session,
                                    bool randomize_spawn_point);

        std::shared_ptr<Player> FindPlayerByDogId(size_t dog_id) const;

        const PlayersAll& GetPlayers() const noexcept {
            return players_;
        }

        void Delete(std::shared_ptr<Player> player);

        size_t GetNextDogId() const noexcept {
            return next_dog_id_;
        }

    private:
        PlayersAll players_;
        size_t next_dog_id_ = 0;
        std::unordered_map<size_t, PlayerIt> map_id_to_it_; // dog_id, iterator_to_player_in_PlayersAll
    };

    /* -------------------- Класс для передачи данных в функцию поиска коллизий -------------------- */
    class ItemGatherer : public collision_detector::ItemGathererProvider {
    public:
        explicit ItemGatherer(size_t ic, const std::vector<std::shared_ptr<model::LostObject>>& items,
                              size_t gc, const std::vector<collision_detector::Gatherer>& ga)
                                : items_count_(ic)
                                , items_(items)
                                , gatherers_count_(gc)
                                , gatherer_(ga) {}

        size_t ItemsCount() const override {
            return items_count_;
        }
        collision_detector::Item GetItem(size_t idx) const override {
            return *(items_[idx]);
        }
        size_t GatherersCount() const override {
            return gatherers_count_;
        }
        collision_detector::Gatherer GetGatherer(size_t idx) const override {
            return gatherer_[idx];
        }

    private:
        size_t items_count_;
        const std::vector<std::shared_ptr<model::LostObject>>& items_;
        size_t gatherers_count_;
        const std::vector<collision_detector::Gatherer>& gatherer_;
    };

    enum class JoinGameErrorCode {
        NONE,
        MAP_NOT_FOUND,
        SESSION_NOT_FOUND,
        INVALID_NAME
    };

    struct JoinGameResult {
        std::shared_ptr<Token> player_token;
        size_t dog_id = 0;
        JoinGameErrorCode error = JoinGameErrorCode::NONE;
    };

    struct GameState {
        size_t dog_id;
        model::DogState state;
        const std::vector<model::PickedObject>& picked_objects;
        size_t scores;
    };

    enum class ActionMove {
        STOP,
        LEFT,
        RIGHT,
        UP,
        DOWN
    };

    struct Champion {
        std::string name; // имя пса (хозяина)
        size_t score;
        double play_time; // в секундах
    };

    using InputArchive = boost::archive::text_iarchive;
    using OutputArchive = boost::archive::text_oarchive;

    /* ------------------- Интерфейс репозитория для хранения результатов игроков ------------------- */

    class ApplicationRepository {
    public:
        virtual void Save(const Champion &result) = 0;
        virtual std::vector<Champion> GetChampions(size_t start, size_t max_items) = 0;

    protected:
        ~ApplicationRepository() = default;
    };

    /* --------------------------------------- Приложение --------------------------------------- */
    class Application {
    public:
        friend std::stringstream SerializeState(const Application &app);
        friend void DeserializeState(std::stringstream& strm, Application& app);

        using Dogs = std::vector<std::shared_ptr<model::Dog>>;

        Application(model::Game &game,
                    bool randomize_spawn_point,
                    bool game_tick_disable,
                    unsigned int tick_period,
                    std::optional<std::string_view> autosave_file,
                    ApplicationRepository& app_repo)
            : game_{game}
            , randomize_spawn_point_(randomize_spawn_point)
            , game_tick_disable_(game_tick_disable)
            , tick_period_(static_cast<double>(tick_period) / 1000.)
            , autosave_file_(autosave_file)
            , app_repo_(app_repo) {}

        const model::Map* FindMap(const model::Map::Id& id) const noexcept {
            return game_.FindMap(id);
        }
        const std::vector<model::Map>& GetMaps() const noexcept {
            return game_.GetMaps();
        }

        JoinGameResult JoinPlayerToGame(model::Map::Id map_id, std::string_view player_name);
        std::shared_ptr<Player> FindPlayerByToken(const Token& token) const noexcept {
            return player_tokens_.FindPlayerByToken(token);
        }
        const GameState GetPlayerGameState(const std::shared_ptr<players::Player> player) const {
            return {player->GetDog()->GetDogId(),
                    player->GetDog()->GetDogState(),
                    player->GetDog()->GetPickedObjects(),
                    player->GetDog()->GetScores()};
        }
        std::shared_ptr<model::Dog> GetDogById(size_t id) const noexcept {
            return players_.FindPlayerByDogId(id)->GetDog();
        }
        Dogs GetDogsInSession(const std::shared_ptr<Player> player) const noexcept {
            Dogs dogs;
            for (const size_t dog_id : player->GetGameSession()->GetDogIds()) {
                dogs.push_back(GetDogById(dog_id));
            }
            return dogs;
        }

        void SetDogAction(std::shared_ptr<Player> player, ActionMove action_move);
        std::vector<std::shared_ptr<Player>> GetPlayersInSession(const std::shared_ptr<Player> player) const;
        void MoveDogs(double time_period);

        double GetTickPeriod() const noexcept {
            return tick_period_;
        }
        bool IsTestMode() const noexcept {
            return game_tick_disable_;
        }
        bool IsRandomSpawnPoint() const noexcept {
            return randomize_spawn_point_;
        }
        const model::GameSession::LostObjects& GetLostObjects(std::shared_ptr<players::Player> player) {
            return player->GetGameSession()->GetLostObjects();
        }
        std::optional<std::string_view> GetAutosaveFile() {
            return autosave_file_;
        }
        std::vector<Champion> GetChampions(size_t start, size_t max_items) const;

    private:
        void PickUpItems(std::shared_ptr<model::GameSession> session,
                         const std::vector<collision_detector::Gatherer>& gatherers,
                         const std::unordered_map<size_t, std::shared_ptr<model::Dog>>& idx_to_dog);
        void BringItemsToOffices(std::shared_ptr<model::GameSession> session,
                         const std::vector<collision_detector::Gatherer>& gatherers,
                         const std::unordered_map<size_t, std::shared_ptr<model::Dog>> &idx_to_dog);
        void DeletePlayer(std::shared_ptr<Player> player);

        model::Game& game_;

        bool randomize_spawn_point_;
        bool game_tick_disable_;
        double tick_period_; // храним в секундах
        std::optional<std::string_view> autosave_file_;

        ApplicationRepository& app_repo_;

        Players players_;
        PlayerTokens player_tokens_;
    };

    std::stringstream SerializeState(const Application &app);
    void DeserializeState(std::stringstream &strm, Application &app);
    void AutosaveState(Application& app, std::string_view file_name);

} // namespace players
