#pragma once
#include <SFML/Network.hpp>
#include <unordered_map>
#include <vector>
#include <string>
#include "network_messages.h"
#include "EnemyTank.h"
#include <random>
#include "Bullet.h"
struct ClientInfo {
    sf::IpAddress address;
    unsigned short port;
    PlayerData playerData;
    float lastUpdateTime;
    bool isActive;

    // Sequence number tracking for this client
    uint32_t lastReceivedSequenceNumber;
    std::unordered_map<uint32_t, bool> receivedSequenceNumbers;

    // Track last acknowledged input for this client
    uint32_t lastAcknowledgedInputSeq;

    // Death and respawn system
    int32_t score;                  // Player score (starts at 0, penalty -100 on death)
    bool isDead;                    // Whether player is currently dead
    float deathTimer;               // Countdown timer for respawn (5 seconds)
    static constexpr float RESPAWN_COOLDOWN = 5.0f;
    static constexpr int32_t DEATH_PENALTY = 100;

    ClientInfo() : address(sf::IpAddress::LocalHost), port(0), lastUpdateTime(0),
        isActive(false), lastReceivedSequenceNumber(0), lastAcknowledgedInputSeq(0),
        score(0), isDead(false), deathTimer(0.0f) {
    }
    ClientInfo(sf::IpAddress addr, unsigned short p)
        : address(addr), port(p), lastUpdateTime(0), isActive(true),
        lastReceivedSequenceNumber(0), lastAcknowledgedInputSeq(0),
        score(0), isDead(false), deathTimer(0.0f) {
    }
};

class GameServer {
public:
    GameServer(unsigned short port = 53000);
    ~GameServer();
    bool Initialize();
    void Update(float deltaTime);
    void Shutdown();

    // Server management
    bool IsRunning() const { return isRunning; }
    size_t GetPlayerCount() const { return clients.size(); }

private:
    sf::UdpSocket socket;
    unsigned short serverPort;
    bool isRunning;
    // Enemy management
    std::unordered_map<uint32_t, std::unique_ptr<EnemyTank>> enemies;
    uint32_t nextEnemyId;
    // Client management
    std::unordered_map<uint32_t, ClientInfo> clients;
    uint32_t nextPlayerId;
    std::unordered_map<uint32_t, std::unique_ptr<Bullet>> bullets;  // All active bullets
    uint32_t nextBulletId;           // Counter for bullet IDs (starts at 10000)
    float bulletUpdateRate;          // How often to send bullet updates (0.033f = 30Hz)
    float bulletUpdateTimer;         // Timer for bullet updates

    // Server-side sequence numbering for outgoing messages
    uint32_t outgoingSequenceNumber;

    // Timing
    float gameStateUpdateRate;  // How often to send full game state (seconds)
    float gameStateUpdateTimer;
    float clientTimeoutDuration; // How long before disconnecting inactive clients

    // Available colors for players
    std::vector<std::string> availableColors;

    // Enemy spawning
    float enemySpawnTimer;
    float enemySpawnInterval;  // How often to spawn enemies (seconds)
    // Note: Max enemies is now calculated dynamically: 3 * (PlayerCount > 0) + PlayerCount

    // Random number generation for enemy spawning
    std::random_device randomDevice;
    std::mt19937 randomGenerator;

    // Enemy management methods
    void UpdateEnemies(float deltaTime);
    void SpawnEnemy();
    sf::Vector2f GetRandomSpawnPosition();
    EnemyTank::EnemyType GetRandomEnemyType();
    void RemoveDeadEnemies();


    // Network Handling
    void ProcessIncomingMessages();
    void ProcessPacket(sf::Packet& packet, sf::IpAddress clientIP, unsigned short clientPort);
    void HandleJoinRequest(const JoinMessage& msg, sf::IpAddress clientIP, unsigned short clientPort);
    void HandlePlayerUpdate(const PlayerUpdateMessage& msg, sf::IpAddress clientIP, unsigned short clientPort);
    void HandlePlayerInput(const PlayerInputMessage& msg, sf::IpAddress clientIP, unsigned short clientPort);  // Step 1.3
    void SendGameStateToAll();
    void SendGameStateToClient(uint32_t playerId);
    void RemoveInactiveClients(float deltaTime);
    void BroadcastPlayerLeft(uint32_t playerId);
    void SendPlayerIdAssignment(uint32_t playerId, sf::IpAddress clientIP, unsigned short clientPort);

    // Server-side movement simulation
    void SimulatePlayerMovement(float deltaTime);
    void CheckServerSideCollisions(float deltaTime);  // NEW: Authoritative collision checking

    // Ping/Pong handling
    void HandlePing(const PingMessage& msg, sf::IpAddress clientIP, unsigned short clientPort);

    // Sequence number validation
    bool ValidateSequenceNumber(ClientInfo& client, uint32_t sequenceNumber);
    void RecordReceivedSequence(ClientInfo& client, uint32_t sequenceNumber);

    // Error handling helpers
    void CleanupSocketResources();
    std::string SocketStatusToString(sf::Socket::Status status) const;

    // Helper methods
    uint32_t FindPlayerByAddress(sf::IpAddress address, unsigned short port);
    std::string AssignColor();
    void PrintServerStats();
    void DetectAndReportPacketLoss();

    // Input acknowledgment
    void SendInputAcknowledgment(uint32_t playerId, uint32_t acknowledgedSeq,
        sf::IpAddress clientIP, unsigned short clientPort);
    
    // Bullet Mechanics 
    void HandleBulletSpawn(const BulletSpawnMessage& msg, sf::IpAddress clientIP, unsigned short clientPort);
    void UpdateBullets(float deltaTime);
    void SendBulletUpdates();
    void BroadcastBulletDestruction(uint32_t bulletId, uint8_t reason, uint32_t hitTargetId, sf::Vector2f hitPos);
    void CheckBulletCollisions();
    void RemoveDeadBullets();
    BulletData BulletToBulletData(const Bullet& bullet, uint32_t bulletId) const;
    bool ValidateBulletSpawnRequest(const BulletSpawnMessage& msg, uint32_t playerId) const;
    //Enemy Targeting AI
    uint32_t SelectTargetForEnemy(EnemyTank* enemy);
    void UpdateEnemyTargetPosition(EnemyTank* enemy);
    void SpawnEnemyBullet(uint32_t enemyId, EnemyTank* enemy);
    void BroadcastEnemyBulletSpawn(uint32_t bulletId, sf::Vector2f position, sf::Vector2f direction, uint32_t ownerId);
    uint64_t GetCurrentTimestamp() const;
    void DiagnoseEnemyShooting(uint32_t enemyId, EnemyTank* enemy);

    // Death and respawn system
    void CheckPlayerDeaths();
    void HandlePlayerDeath(uint32_t playerId, uint32_t killerId);
    void RespawnPlayer(uint32_t playerId);
    void UpdateDeadPlayers(float deltaTime);
    void BroadcastPlayerDeath(uint32_t playerId, uint32_t killerId, sf::Vector2f deathPos, int32_t scorePenalty);
    void BroadcastPlayerRespawn(uint32_t playerId, sf::Vector2f spawnPos, float health);
    sf::Vector2f GetRandomRespawnPosition();
};