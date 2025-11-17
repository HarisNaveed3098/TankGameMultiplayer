#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include <unordered_map>
#include <optional>
#include <cstdint>
#include <string>

class NetworkClient;
class Tank;
class BorderManager;
class InterpolationManager;

#include "tank.h"
#include "BorderManager.h"
#include "entity_interpolation.h"
#include "network_messages.h"  // For PlayerData
#include "EnemyTank.h"
#include "Bullet.h"  

class MultiplayerGame {
public:
    MultiplayerGame();
    ~MultiplayerGame();

    bool Initialize(const std::string& playerName, const std::string& preferredColor);
    bool ConnectToServer(const std::string& serverIP, unsigned short serverPort);
    void Shutdown();
    void HandleEvents(const std::optional<sf::Event> event);
    void Update(float dt);
    void Render(sf::RenderWindow& window);
    bool IsConnected() const;
    size_t GetPlayerCount() const;

    float GetAverageRTT() const;

    //    Set window reference for mouse tracking
    void SetWindow(sf::RenderWindow* window) { this->window = window; }
    int GetPlayerScore() const { return playerScore; }

private:
    std::unique_ptr<NetworkClient>      networkClient;
    std::unique_ptr<Tank>               localTank;
    std::unordered_map<uint32_t, std::unique_ptr<Tank>> otherTanks;
    std::vector<std::unique_ptr<Bullet>> bullets;
    sf::Font scoreFont;
    sf::Text* scoreText;  // SFML 3.0: Use pointer for sf::Text
    bool scoreFontLoaded = false;
    std::string playerName;
    std::string playerColor;
    void SynchronizeBulletsFromServer();
    void CreateBulletFromServerData(const BulletData& data);
    Bullet::BulletType ConvertBulletType(uint8_t typeValue);
    std::unique_ptr<BorderManager>      borderManager;
    sf::Texture                         backgroundTexture;
    std::unique_ptr<sf::Sprite>         background;

    std::unique_ptr<InterpolationManager> interpolationManager;
    std::unordered_map<uint32_t, std::unique_ptr<EnemyTank>> enemies;

    // Track how many GameState messages we've received to delay interpolation start
    int snapshotCountForInterpolation = 0;
    int64_t gameStartTime = 0;

    //    Window reference for mouse position tracking
    sf::RenderWindow* window = nullptr;

    //    Get mouse position in world coordinates
    sf::Vector2f GetMouseWorldPosition() const;

    void UpdateOtherPlayers(int64_t serverTimestamp);
    void CreateTankForPlayer(uint32_t playerId, const PlayerData& playerData);
    void UpdateTankFromPlayerData(Tank& tank, const PlayerData& playerData);
    void UpdateTankFromInterpolatedState(Tank& tank, const InterpolatedState& state);
    void EnforceBorderCollision(Tank& tank);
    void UpdateEnemies(const std::unordered_map<uint32_t, EnemyData>& enemyData);
    void CreateEnemyFromData(uint32_t enemyId, const EnemyData& data);
    void UpdateEnemyFromData(EnemyTank& enemy, const EnemyData& data);
    EnemyTank::EnemyType ConvertEnemyType(uint8_t typeValue);
    void CheckTankCollisions();
    bool CheckCircleCollision(sf::Vector2f pos1, float radius1,
        sf::Vector2f pos2, float radius2);
       // sf::Vector2f obstaclePos, float tankRadius, float obstacleRadius);
    int playerScore;

    // Bullet collision methods
    void CheckBulletCollisions();
    void CheckBulletEnemyCollisions();
    void CheckBulletBorderCollisions();
};