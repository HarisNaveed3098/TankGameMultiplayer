#pragma once
#include <SFML/Network.hpp>
#include <string>
#include <unordered_map>
#include <queue>
#include <deque>
#include "network_messages.h"
#include "Tank.h"
#include "client_prediction.h"
#include <memory>
#include "multiplayer_game.h"
#include <functional>
#include "Bullet.h"  

class MultiplayerGame;

// Structure to track sent packets for RTT calculation 
struct SentPacket {
    uint32_t sequenceNumber;
    int64_t sentTime;
};

class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();
    using OnFirstGameStateCallback = std::function<void(int64_t)>;
    void SetOnFirstGameStateCallback(OnFirstGameStateCallback cb);
    int64_t GetLastGameStateTimestamp() const;

    // Connection management
    bool Connect(const std::string& serverIP, unsigned short serverPort,
        const std::string& playerName, const std::string& preferredColor = "");
    void Disconnect();
    bool IsConnected() const { return isConnected; }

    // Network communication
    void Update(float deltaTime);
    void SendPlayerUpdate(const Tank& localPlayer);  // Full update (kept for compatibility)
    void SendPlayerInput(const Tank& localPlayer);    // Lightweight input-only update

    // Game state access
    const std::unordered_map<uint32_t, PlayerData>& GetOtherPlayers() const { return otherPlayers; }
    uint32_t GetLocalPlayerId() const { return localPlayerId; }

    // Step 1.1: Network statistics access
    const NetworkStats& GetNetworkStats() const { return networkStats; }
    float GetAverageRTT() const { return networkStats.averageRTT; }
    float GetPacketLoss() const { return networkStats.packetLoss; }
    float GetJitter() const { return networkStats.jitter; }

    // Step 1.2: Error monitoring
    int GetConsecutiveErrors() const { return consecutiveErrors; }
    bool HasNetworkErrors() const { return consecutiveErrors > 0; }

    // Client-side prediction - NEW: Support mouse position for barrel rotation
    void ApplyLocalInputWithPrediction(Tank& localPlayer, float deltaTime, sf::Vector2f mousePos);
    void SetPredictionEnabled(bool enabled) { predictionEnabled = enabled; }
    bool IsPredictionEnabled() const { return predictionEnabled; }

    // Get prediction stats (for debugging)
    size_t GetPredictionHistorySize() const {
        return prediction ? prediction->GetHistorySize() : 0;
    }
    uint32_t GetPredictionSequenceNumber() const {
        return prediction ? prediction->GetLatestSequenceNumber() : 0;
    }

    MultiplayerGame* multiplayerGame;

    // Server reconciliation
    bool HasServerAuthoritativeState() const { return hasServerAuthoritativeState; }
    void ClearServerAuthoritativeState() { hasServerAuthoritativeState = false; }
    sf::Vector2f GetServerAuthoritativePosition() const { return serverAuthoritativePosition; }
    float GetServerAuthoritativeBodyRotation() const { return serverAuthoritativeBodyRotation; }
    void ApplyServerReconciliation(Tank& localPlayer);

    // Input buffering system access
    size_t GetUnacknowledgedInputCount() const {
        return prediction ? prediction->GetUnacknowledgedCount() : 0;
    }
    int64_t GetOldestUnacknowledgedTimestamp() const {
        return prediction ? prediction->GetOldestUnacknowledgedTimestamp() : 0;
    }
    uint32_t GetLastAcknowledgedInputSeq() const { return lastAcknowledgedInputSeq; }

    // Get server timestamp from last received game state
    int64_t GetLastServerTimestamp() const { return lastServerTimestamp; }
    bool HasServerTimestamp() const { return lastServerTimestamp > 0; }
    const std::unordered_map<uint32_t, EnemyData>& GetEnemies() const { return enemyData; }
    void SendBulletSpawn(const Tank& localPlayer);
    const std::unordered_map<uint32_t, BulletData>& GetBullets() const { return bulletData; }
    float GetServerAuthoritativeHealth() const { return serverAuthoritativeHealth; }
    float GetServerAuthoritativeMaxHealth() const { return serverAuthoritativeMaxHealth; }
    int32_t GetServerAuthoritativeScore() const { return serverAuthoritativeScore; }
    bool GetServerAuthoritativeIsDead() const { return serverAuthoritativeIsDead; }
private:
    sf::UdpSocket socket;
    sf::IpAddress serverAddress;
    unsigned short serverPort;
    bool isConnected;
    std::unordered_map<uint32_t, EnemyData> enemyData;
    float serverAuthoritativeHealth;
    float serverAuthoritativeMaxHealth;
    int32_t serverAuthoritativeScore;
    bool serverAuthoritativeIsDead;
    // Player management
    uint32_t localPlayerId;
    std::unordered_map<uint32_t, PlayerData> otherPlayers;
    std::unordered_map<uint32_t, BulletData> bulletData;  // Bullets from server
    void HandleBulletUpdate(const BulletUpdateMessage& msg);
    void HandleBulletDestroy(const BulletDestroyMessage& msg);

    // Timing
    float updateRate; // How often to send updates to server
    float updateTimer;
    OnFirstGameStateCallback onFirstGameState;
    bool interpolationInitialized = false;
    int64_t lastGameStateTimestamp = 0;

    // Sequence number tracking
    uint32_t outgoingSequenceNumber;
    uint32_t lastReceivedSequenceNumber;
    int64_t lastServerTimestamp;  // Timestamp from last received game state

    // RTT and ping tracking
    float pingTimer;
    float pingInterval;  // How often to send ping (e.g., 1.0 seconds)
    std::deque<SentPacket> sentPackets;  // Track sent packets for RTT calculation
    static const size_t MAX_SENT_PACKETS_HISTORY = 100;

    //  Network statistics
    NetworkStats networkStats;
    std::deque<float> rttHistory;  // For calculating average and jitter
    static const size_t RTT_HISTORY_SIZE = 30;

    // Out-of-order packet detection
    std::unordered_map<uint32_t, bool> receivedSequenceNumbers;  // Track received sequences
    static const size_t MAX_SEQUENCE_HISTORY = 200;

    //  Error tracking
    int consecutiveErrors;
    const int maxConsecutiveErrors;

    // Client prediction system
    std::unique_ptr<ClientPrediction> prediction;
    uint32_t lastServerAckedSequence;
    bool predictionEnabled;
    static constexpr float MOVEMENT_SPEED = 150.0f;
    static constexpr float ROTATION_SPEED = 200.0f;

    // Server authoritative state for reconciliation
    sf::Vector2f serverAuthoritativePosition;
    float serverAuthoritativeBodyRotation;
    float serverAuthoritativeBarrelRotation;
    bool hasServerAuthoritativeState;

    // Smooth reconciliation system (prevents jerky corrections)
    sf::Vector2f reconciliationTargetPosition;
    float reconciliationTargetRotation;
    bool isReconciling;
    static constexpr float SMOOTH_CORRECTION_THRESHOLD = 30.0f;  // Below this, use smooth correction
    static constexpr float SNAP_CORRECTION_THRESHOLD = 50.0f;     // Above this, snap immediately
    static constexpr float RECONCILIATION_RATE = 6.0f;           // Lerp speed (higher = faster)

    // Input buffering system tracking
    int64_t lastInputAckTime;              // Last time we received input ack
    uint32_t lastAcknowledgedInputSeq;     // Last acknowledged input sequence
    sf::Vector2f lastMousePosition;

    // Private methods
    void ProcessIncomingMessages();
    void ProcessPacket(sf::Packet& packet, sf::IpAddress senderIP, unsigned short senderPort);
    void HandleGameState(const GameStateMessage& msg);
    bool SendJoinRequest(const std::string& playerName, const std::string& preferredColor);

    //  RTT and statistics methods
    void SendPing();
    void HandlePong(const PongMessage& msg);
    void UpdateNetworkStatistics(float rtt);
    void RecordReceivedPacket(uint32_t sequenceNumber);
    bool IsPacketOutOfOrder(uint32_t sequenceNumber) const;
    void CleanupOldSequenceNumbers();

    //  Error handling helpers
    void CleanupSocketResources();
    std::string SocketStatusToString(sf::Socket::Status status) const;
    void ValidateAndClampLocalPlayerData(Tank& localPlayer);
    void DetectPacketLoss();

    // Prediction helpers  Mouse position parameter
    void ApplyInputToTank(Tank& tank, const InputState& input, sf::Vector2f mousePos);
    void SendInputWithSequence(uint32_t sequenceNumber, const InputState& input, float barrelRotation);
    //  Input buffering system methods
    void HandleInputAcknowledgment(const InputAcknowledgmentMessage& msg);
    void ReplayInputsAfterCorrection(Tank& localPlayer, uint32_t fromSequence, sf::Vector2f mousePos); // ADD mousePos HERE!
    void ProcessInputBuffer(float deltaTime);
};