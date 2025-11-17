#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <SFML/Network.hpp>
#include <chrono>

// Message types for different kinds of network communication
enum class NetMessageType : uint8_t {
    PLAYER_JOIN = 1, 
    PLAYER_LEAVE = 2,
    PLAYER_UPDATE = 3,
    GAME_STATE = 4,
    PLAYER_LIST = 5,
    PLAYER_ID_ASSIGNMENT = 6,
    PING = 7,
    PONG = 8,
    PLAYER_INPUT = 9,          //  Lightweight input-only message
    INPUT_ACKNOWLEDGMENT = 10, //    2.3: Server acknowledges received input
    BULLET_SPAWN = 11,        //    Client requests bullet spawn
    BULLET_UPDATE = 12,       //    Server sends bullet state
    BULLET_DESTROY = 13,      //    Server notifies bullet destruction
    PLAYER_DEATH = 14,        //    Player died
    PLAYER_RESPAWN = 15       //    Player respawned
};
struct EnemyData {
    uint32_t enemyId;           // Unique enemy ID (starts at 1000)
    uint8_t enemyType;          // Enemy type (0=Red, 1=Black, 2=Purple, 3=Orange, 4=Teal)
    float x, y;                 // Position
    float bodyRotation;         // Body rotation in degrees
    float barrelRotation;       // Barrel rotation in degrees
    float health;               // Current health
    float maxHealth;            // Maximum health

    EnemyData() : enemyId(0), enemyType(0), x(0), y(0),
        bodyRotation(0), barrelRotation(0),
        health(100.0f), maxHealth(100.0f) {
    }
};
struct BulletData {
    uint32_t bulletId;          // Unique bullet ID (server-assigned)
    uint32_t ownerId;           // Tank that fired this bullet
    uint8_t bulletType;         // Bullet type (0=Player, 1=Enemy, 2=Shell, 3=Tracer)
    float x, y;                 // Current position
    float velocityX, velocityY; // Velocity vector
    float rotation;             // Visual rotation (degrees)
    float damage;               // Damage value
    float lifetime;             // Remaining lifetime (seconds)
    int64_t spawnTime;          // When bullet was created (timestamp)

    BulletData()
        : bulletId(0), ownerId(0), bulletType(0),
        x(0.0f), y(0.0f), velocityX(0.0f), velocityY(0.0f),
        rotation(0.0f), damage(25.0f), lifetime(3.0f),
        spawnTime(0) {
    }
};

// Network statistics for monitoring connection quality
struct NetworkStats {
    float averageRTT;           // Round-trip time in milliseconds
    float packetLoss;           // Percentage of lost packets
    float jitter;               // Variation in packet delay (milliseconds)
    uint32_t totalPacketsSent;
    uint32_t totalPacketsReceived;
    uint32_t packetsLost;
    float averageLatency;       // One-way latency (RTT/2)

    // Jitter calculation
    float lastPacketDelay;
    float minRTT;
    float maxRTT;

    NetworkStats() : averageRTT(0), packetLoss(0), jitter(0),
        totalPacketsSent(0), totalPacketsReceived(0), packetsLost(0),
        averageLatency(0), lastPacketDelay(0), minRTT(999999.0f), maxRTT(0) {
    }

    void Reset() {
        averageRTT = 0;
        packetLoss = 0;
        jitter = 0;
        totalPacketsSent = 0;
        totalPacketsReceived = 0;
        packetsLost = 0;
        averageLatency = 0;
        lastPacketDelay = 0;
        minRTT = 999999.0f;
        maxRTT = 0;
    }
};

// Timestamp helper using high-resolution clock
inline int64_t GetCurrentTimestamp() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

// Individual player data
struct PlayerData {
    uint32_t playerId;
    std::string playerName;  // Added player name for labels
    float x, y;
    float bodyRotation;  // in degrees
    float barrelRotation; // in degrees
    std::string color;
    bool isMoving_forward;
    bool isMoving_backward;
    bool isMoving_left;
    bool isMoving_right;
    float health;          // Current health
    float maxHealth;       // Maximum health
    int32_t score;         // Player score
    bool isDead;           // Whether player is currently dead

    PlayerData() : playerId(0), playerName(""), x(0), y(0), bodyRotation(0), barrelRotation(0),
        color("green"), isMoving_forward(false), isMoving_backward(false),
        isMoving_left(false), isMoving_right(false), health(100.0f), maxHealth(100.0f),
        score(0), isDead(false) {
    }
};
struct PlayerDeathMessage {
    NetMessageType type = NetMessageType::PLAYER_DEATH;
    uint32_t playerId;          // Who died
    uint32_t killerId;          // Who killed them (0 if killed by enemy)
    float deathX, deathY;       // Where they died
    int32_t scorePenalty;       // Score lost (150)
    int64_t timestamp;
    uint32_t sequenceNumber;

    PlayerDeathMessage()
        : playerId(0), killerId(0), deathX(0.0f), deathY(0.0f),
        scorePenalty(150), timestamp(0), sequenceNumber(0) {
    }
};

//  Player respawn message (server -> all clients)
struct PlayerRespawnMessage {
    NetMessageType type = NetMessageType::PLAYER_RESPAWN;
    uint32_t playerId;          // Who respawned
    float spawnX, spawnY;       // Spawn position
    float health;               // Respawn health (100.0f)
    int64_t timestamp;
    uint32_t sequenceNumber;

    PlayerRespawnMessage()
        : playerId(0), spawnX(0.0f), spawnY(0.0f), health(100.0f),
        timestamp(0), sequenceNumber(0) {
    }
};
// Network message for player joining
struct JoinMessage {
    NetMessageType type = NetMessageType::PLAYER_JOIN;
    std::string playerName;
    std::string preferredColor;
    int64_t timestamp;              //   : Message timestamp
    uint32_t sequenceNumber;        //   : Sequence number for ordering

    JoinMessage() : timestamp(0), sequenceNumber(0) {}
};

// Network message for player updates (sent from client to server)
struct PlayerUpdateMessage {
    NetMessageType type = NetMessageType::PLAYER_UPDATE;
    uint32_t playerId;
    float x, y;
    float bodyRotation;
    float barrelRotation;
    bool isMoving_forward;
    bool isMoving_backward;
    bool isMoving_left;
    bool isMoving_right;
    int64_t timestamp;              //   Message timestamp
    uint32_t sequenceNumber;        //   Sequence number for ordering

    PlayerUpdateMessage() : playerId(0), x(0), y(0), bodyRotation(0), barrelRotation(0),
        isMoving_forward(false), isMoving_backward(false), isMoving_left(false),
        isMoving_right(false), timestamp(0), sequenceNumber(0) {
    }
};

//  Lightweight input-only message (reduces bandwidth by ~40%)
struct PlayerInputMessage {
    NetMessageType type = NetMessageType::PLAYER_INPUT;
    uint32_t playerId;
    bool isMoving_forward;
    bool isMoving_backward;
    bool isMoving_left;
    bool isMoving_right;
    float barrelRotation;  //   Client's barrel rotation (mouse-driven)
    int64_t timestamp;
    uint32_t sequenceNumber;

    PlayerInputMessage() : playerId(0),
        isMoving_forward(false), isMoving_backward(false),
        isMoving_left(false), isMoving_right(false),
        barrelRotation(0.0f),  //   
        timestamp(0), sequenceNumber(0) {
    }
};

// Input acknowledgment message (server -> client)
struct InputAcknowledgmentMessage {
    NetMessageType type = NetMessageType::INPUT_ACKNOWLEDGMENT;
    uint32_t playerId;              // Which player's input is being acknowledged
    uint32_t acknowledgedSequence;  // Last input sequence server processed
    int64_t serverTimestamp;        // When server processed this input

    InputAcknowledgmentMessage() : playerId(0), acknowledgedSequence(0), serverTimestamp(0) {}
};
struct BulletSpawnMessage {
    NetMessageType type = NetMessageType::BULLET_SPAWN;
    uint32_t playerId;          // Who is shooting
    float spawnX, spawnY;       // Where bullet spawns (barrel end)
    float directionX, directionY; // Direction vector (normalized)
    float barrelRotation;       // Visual rotation for bullet sprite
    int64_t timestamp;          // When shot was fired
    uint32_t sequenceNumber;    // For packet ordering

    BulletSpawnMessage()
        : playerId(0), spawnX(0.0f), spawnY(0.0f),
        directionX(1.0f), directionY(0.0f), barrelRotation(0.0f),
        timestamp(0), sequenceNumber(0) {
    }
};
struct BulletUpdateMessage {
    NetMessageType type = NetMessageType::BULLET_UPDATE;
    std::vector<BulletData> bullets; // All active bullets
    int64_t timestamp;               // Server timestamp
    uint32_t sequenceNumber;         // For packet ordering

    BulletUpdateMessage() : timestamp(0), sequenceNumber(0) {
    }
};
struct BulletDestroyMessage {
    NetMessageType type = NetMessageType::BULLET_DESTROY;
    uint32_t bulletId;          // Which bullet was destroyed
    uint8_t destroyReason;      // 0=Expired, 1=HitPlayer, 2=HitEnemy, 3=HitBorder
    uint32_t hitTargetId;       // ID of what was hit (0 if none)
    float hitX, hitY;           // Where impact occurred
    int64_t timestamp;          // When destruction occurred
    uint32_t sequenceNumber;    // For packet ordering

    BulletDestroyMessage()
        : bulletId(0), destroyReason(0), hitTargetId(0),
        hitX(0.0f), hitY(0.0f), timestamp(0), sequenceNumber(0) {
    }
};

// Network message for full game state (sent from server to clients)
struct GameStateMessage {
    NetMessageType type = NetMessageType::GAME_STATE;
    std::vector<PlayerData> players;
    int64_t timestamp;              //   : Message timestamp
    uint32_t sequenceNumber;        //   : Sequence number for ordering
    uint32_t lastAckedInput;        //    2.3: Last acknowledged input sequence
    std::vector<EnemyData> enemies;  //   : Add enemy list


    GameStateMessage() : timestamp(0), sequenceNumber(0), lastAckedInput(0) {}
};

// Network message for player list updates
struct PlayerListMessage {
    NetMessageType type = NetMessageType::PLAYER_LIST;
    std::vector<PlayerData> players;
    int64_t timestamp;              //   : Message timestamp
    uint32_t sequenceNumber;        //   : Sequence number for ordering

    PlayerListMessage() : timestamp(0), sequenceNumber(0) {}
};

//  Ping message for RTT measurement
struct PingMessage {
    NetMessageType type = NetMessageType::PING;
    int64_t timestamp;
    uint32_t sequenceNumber;

    PingMessage() : timestamp(GetCurrentTimestamp()), sequenceNumber(0) {}
};

//  Pong message response
struct PongMessage {
    NetMessageType type = NetMessageType::PONG;
    int64_t originalTimestamp;      // Echo back the ping timestamp
    uint32_t sequenceNumber;        // Echo back sequence number

    PongMessage() : originalTimestamp(0), sequenceNumber(0) {}
};


// Helper functions for packet serialization/deserialization
namespace NetworkUtils {
    // Serialize PlayerData to packet
    sf::Packet& operator<<(sf::Packet& packet, const PlayerData& player);
    sf::Packet& operator>>(sf::Packet& packet, PlayerData& player);

    // Serialize messages to packets
    sf::Packet& operator<<(sf::Packet& packet, const JoinMessage& msg);
    sf::Packet& operator>>(sf::Packet& packet, JoinMessage& msg);

    sf::Packet& operator<<(sf::Packet& packet, const PlayerUpdateMessage& msg);
    sf::Packet& operator>>(sf::Packet& packet, PlayerUpdateMessage& msg);

    // Step 1.3: Input-only message serialization
    sf::Packet& operator<<(sf::Packet& packet, const PlayerInputMessage& msg);
    sf::Packet& operator>>(sf::Packet& packet, PlayerInputMessage& msg);

    sf::Packet& operator<<(sf::Packet& packet, const GameStateMessage& msg);
    sf::Packet& operator>>(sf::Packet& packet, GameStateMessage& msg);

    //   Ping/Pong serialization
    sf::Packet& operator<<(sf::Packet& packet, const PingMessage& msg);
    sf::Packet& operator>>(sf::Packet& packet, PingMessage& msg);

    sf::Packet& operator<<(sf::Packet& packet, const PongMessage& msg);
    sf::Packet& operator>>(sf::Packet& packet, PongMessage& msg);

    //    Input acknowledgment serialization
    sf::Packet& operator<<(sf::Packet& packet, const InputAcknowledgmentMessage& msg);
    sf::Packet& operator>>(sf::Packet& packet, InputAcknowledgmentMessage& msg);
    sf::Packet& operator<<(sf::Packet& packet, const PlayerInputMessage& msg);

    sf::Packet& operator>>(sf::Packet& packet, PlayerInputMessage& msg);
    sf::Packet& operator<<(sf::Packet& packet, const BulletData& bullet);

    sf::Packet& operator>>(sf::Packet& packet, BulletData& bullet);


    // BulletSpawnMessage serialization
    sf::Packet& operator<<(sf::Packet& packet, const BulletSpawnMessage& msg);

    sf::Packet& operator>>(sf::Packet& packet, BulletSpawnMessage& msg);
    // BulletUpdateMessage serialization
    sf::Packet& operator<<(sf::Packet& packet, const BulletUpdateMessage& msg);
    sf::Packet& operator>>(sf::Packet& packet, BulletUpdateMessage& msg);
    // BulletDestroyMessage serialization
    sf::Packet& operator<<(sf::Packet& packet, const BulletDestroyMessage& msg);

    sf::Packet& operator>>(sf::Packet& packet, BulletDestroyMessage& msg);
    sf::Packet& operator<<(sf::Packet& packet, const PlayerDeathMessage& msg);
    sf::Packet& operator>>(sf::Packet& packet, PlayerDeathMessage& msg);

    sf::Packet& operator<<(sf::Packet& packet, const PlayerRespawnMessage& msg);
    sf::Packet& operator>>(sf::Packet& packet, PlayerRespawnMessage& msg);
}
using NetworkUtils::operator<<;
using NetworkUtils::operator>>;