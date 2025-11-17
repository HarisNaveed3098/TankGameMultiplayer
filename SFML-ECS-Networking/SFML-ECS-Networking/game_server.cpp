#include "game_server.h"
#include "utils.h"
#include <algorithm>
#include <random>
#include <SFML/Network.hpp>
#include "network_validation.h"
#include "world_constants.h"
#include <unordered_set>
GameServer::GameServer(unsigned short port)
    : serverPort(port), isRunning(false), nextPlayerId(1),
    gameStateUpdateRate(0.022f), gameStateUpdateTimer(0),
    clientTimeoutDuration(15.0f), outgoingSequenceNumber(0),
    nextEnemyId(1000),
    enemySpawnTimer(0),
    enemySpawnInterval(5.0f),
    randomGenerator(randomDevice()),
    nextBulletId(10000),
    bulletUpdateRate(0.033f),
    bulletUpdateTimer(0)
{
    availableColors = { "red", "blue", "green", "black" };
}

GameServer::~GameServer() {
    Shutdown();
}

bool GameServer::Initialize() {
    Utils::printMsg("Initializing game server on port " + std::to_string(serverPort) + "...");

    try {
        sf::Socket::Status bindStatus = socket.bind(serverPort);

        if (bindStatus != sf::Socket::Status::Done) {
            Utils::printMsg("Failed to bind server socket to port " + std::to_string(serverPort) +
                " - Status: " + SocketStatusToString(bindStatus), error);
            CleanupSocketResources();
            return false;
        }

        socket.setBlocking(false);
        isRunning = true;
        outgoingSequenceNumber = 0;
        Utils::printMsg("Game server initialized successfully", success);
        Utils::printMsg("Server listening on port " + std::to_string(socket.getLocalPort()));

        return true;
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception during server initialization: " + std::string(e.what()), error);
        CleanupSocketResources();
        return false;
    }
    catch (...) {
        Utils::printMsg("Unknown exception during server initialization", error);
        CleanupSocketResources();
        return false;
    }
}

void GameServer::Update(float deltaTime) {
    if (!isRunning) return;

    try {
        ProcessIncomingMessages();
        SimulatePlayerMovement(deltaTime);
        UpdateEnemies(deltaTime);
        UpdateBullets(deltaTime);
        CheckServerSideCollisions(deltaTime);
        CheckPlayerDeaths();
        UpdateDeadPlayers(deltaTime);

        gameStateUpdateTimer += deltaTime;
        bulletUpdateTimer += deltaTime;

        if (gameStateUpdateTimer >= gameStateUpdateRate) {
            SendGameStateToAll();
            gameStateUpdateTimer = 0;
        }

        if (bulletUpdateTimer >= bulletUpdateRate) {
            SendBulletUpdates();
            bulletUpdateTimer = 0;
        }

        RemoveInactiveClients(deltaTime);

        static float statsTimer = 0;
        statsTimer += deltaTime;
        if (statsTimer >= 5.0f) {
            PrintServerStats();
            statsTimer = 0;
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in server Update: " + std::string(e.what()), error);
    }
    catch (...) {
        Utils::printMsg("Unknown exception in server Update", error);
    }
}

void GameServer::ProcessIncomingMessages() {
    try {
        sf::Packet packet;
        std::optional<sf::IpAddress> clientIP;
        unsigned short clientPort;

        int messagesProcessed = 0;
        const int MAX_MESSAGES_PER_FRAME = 200;

        while (messagesProcessed < MAX_MESSAGES_PER_FRAME) {
            sf::Socket::Status receiveStatus = socket.receive(packet, clientIP, clientPort);

            if (receiveStatus == sf::Socket::Status::Done) {
                if (!clientIP.has_value()) {
                    Utils::printMsg("Received packet from invalid sender", warning);
                    messagesProcessed++;
                    continue;
                }

                ProcessPacket(packet, clientIP.value(), clientPort);
                messagesProcessed++;
            }
            else if (receiveStatus == sf::Socket::Status::NotReady) {
                break;
            }
            else if (receiveStatus == sf::Socket::Status::Disconnected) {
                break;
            }
            else if (receiveStatus == sf::Socket::Status::Error) {
                Utils::printMsg("Socket error while receiving on server", error);
                break;
            }
            else if (receiveStatus == sf::Socket::Status::Partial) {
                Utils::printMsg("Partial packet received (unusual for UDP)", debug);
                break;
            }
            else {
                Utils::printMsg("Unknown socket status while receiving: " +
                    SocketStatusToString(receiveStatus), warning);
                break;
            }
        }

        if (messagesProcessed >= MAX_MESSAGES_PER_FRAME) {
            Utils::printMsg("Warning: Server hit max messages per frame limit", warning);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in ProcessIncomingMessages: " + std::string(e.what()), error);
    }
    catch (...) {
        Utils::printMsg("Unknown exception in ProcessIncomingMessages", error);
    }
}

void GameServer::ProcessPacket(sf::Packet& packet, sf::IpAddress clientIP, unsigned short clientPort) {
    try {
        uint8_t messageTypeRaw;
        if (!(packet >> messageTypeRaw)) {
            return;
        }

        NetMessageType msgType = static_cast<NetMessageType>(messageTypeRaw);

        if (msgType == NetMessageType::PLAYER_JOIN) {
            JoinMessage joinMsg;
            joinMsg.type = msgType;

            if (packet >> joinMsg.playerName >> joinMsg.preferredColor
                >> joinMsg.timestamp >> joinMsg.sequenceNumber) {
                HandleJoinRequest(joinMsg, clientIP, clientPort);
            }
            else {
                Utils::printMsg("Failed to parse join message", warning);
            }
        }
        else if (msgType == NetMessageType::PLAYER_UPDATE) {
            PlayerUpdateMessage updateMsg;
            updateMsg.type = msgType;

            if (packet >> updateMsg.playerId >> updateMsg.x >> updateMsg.y
                >> updateMsg.bodyRotation >> updateMsg.barrelRotation
                >> updateMsg.isMoving_forward >> updateMsg.isMoving_backward
                >> updateMsg.isMoving_left >> updateMsg.isMoving_right
                >> updateMsg.timestamp >> updateMsg.sequenceNumber) {

                HandlePlayerUpdate(updateMsg, clientIP, clientPort);
            }
            else {
                Utils::printMsg("Failed to parse player update message", warning);
            }
        }
        else if (msgType == NetMessageType::PLAYER_INPUT) {
            PlayerInputMessage inputMsg;
            inputMsg.type = msgType;

            if (packet >> inputMsg.playerId
                >> inputMsg.isMoving_forward
                >> inputMsg.isMoving_backward
                >> inputMsg.isMoving_left
                >> inputMsg.isMoving_right
                >> inputMsg.timestamp
                >> inputMsg.sequenceNumber
                >> inputMsg.barrelRotation) {

                HandlePlayerInput(inputMsg, clientIP, clientPort);
            }
            else {
                static int failCount = 0;
                if (++failCount % 100 == 0) {
                    Utils::printMsg("PlayerInput parse failures: " +
                        std::to_string(failCount) + " (intermittent packet corruption)", debug);
                }
            }
        }
        else if (msgType == NetMessageType::BULLET_SPAWN) {
            BulletSpawnMessage spawnMsg;
            spawnMsg.type = msgType;

            if (packet >> spawnMsg.playerId >> spawnMsg.spawnX >> spawnMsg.spawnY
                >> spawnMsg.directionX >> spawnMsg.directionY >> spawnMsg.barrelRotation
                >> spawnMsg.timestamp >> spawnMsg.sequenceNumber) {

                HandleBulletSpawn(spawnMsg, clientIP, clientPort);
            }
            else {
                Utils::printMsg("Failed to parse bullet spawn message", warning);
            }
        }
        else if (msgType == NetMessageType::PING) {
            PingMessage pingMsg;
            pingMsg.type = msgType;

            if (packet >> pingMsg.timestamp >> pingMsg.sequenceNumber) {
                HandlePing(pingMsg, clientIP, clientPort);
            }
            else {
                Utils::printMsg("Failed to parse ping message", warning);
            }
        }
        else {
            static std::unordered_map<uint8_t, int> unknownTypes;
            if (++unknownTypes[messageTypeRaw] % 100 == 0) {
                Utils::printMsg("Unknown message type " + std::to_string(static_cast<int>(msgType)) +
                    " (count: " + std::to_string(unknownTypes[messageTypeRaw]) + ")", debug);
            }
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in ProcessPacket: " + std::string(e.what()), error);
    }
    catch (...) {
        Utils::printMsg("Unknown exception in ProcessPacket", error);
    }
}

void GameServer::HandleJoinRequest(const JoinMessage& msg, sf::IpAddress clientIP, unsigned short clientPort) {
    try {
        if (!NetworkValidation::IsValidPlayerName(msg.playerName)) {
            Utils::printMsg("Invalid player name from " + clientIP.toString() +
                " (length: " + std::to_string(msg.playerName.length()) + ")", warning);
            return;
        }

        if (!msg.preferredColor.empty() && !NetworkValidation::IsValidColor(msg.preferredColor)) {
            Utils::printMsg("Invalid color name from " + clientIP.toString() +
                " (length: " + std::to_string(msg.preferredColor.length()) + ")", warning);
        }

        int64_t currentTime = GetCurrentTimestamp();
        if (!NetworkValidation::IsValidTimestamp(msg.timestamp, currentTime)) {
            Utils::printMsg("Invalid timestamp from " + clientIP.toString() +
                " (delta: " + std::to_string(std::abs(currentTime - msg.timestamp)) + "ms)", warning);
        }

        Utils::printMsg("Player join request from " + clientIP.toString() + ":" + std::to_string(clientPort) +
            " Name: " + msg.playerName + " (Seq: " + std::to_string(msg.sequenceNumber) + ")");

        uint32_t existingPlayerId = FindPlayerByAddress(clientIP, clientPort);
        if (existingPlayerId != 0) {
            Utils::printMsg("Player already connected, updating info", warning);
            clients[existingPlayerId].isActive = true;
            clients[existingPlayerId].lastUpdateTime = 0;
            clients[existingPlayerId].playerData.playerName = msg.playerName;
            SendPlayerIdAssignment(existingPlayerId, clientIP, clientPort);
            SendGameStateToClient(existingPlayerId);
            return;
        }

        ClientInfo newClient(clientIP, clientPort);
        newClient.playerData.playerId = nextPlayerId++;
        newClient.playerData.playerName = msg.playerName;
        newClient.playerData.color = !msg.preferredColor.empty() ? msg.preferredColor : AssignColor();

        newClient.playerData.x = WorldConstants::CENTER_X;
        newClient.playerData.y = WorldConstants::CENTER_Y;
        newClient.playerData.health = 100.0f;
        newClient.playerData.maxHealth = 100.0f;

        RecordReceivedSequence(newClient, msg.sequenceNumber);

        clients[newClient.playerData.playerId] = newClient;

        Utils::printMsg("Player " + std::to_string(newClient.playerData.playerId) +
            " (" + msg.playerName + ") joined with color " + newClient.playerData.color, success);

        SendPlayerIdAssignment(newClient.playerData.playerId, clientIP, clientPort);
        SendGameStateToClient(newClient.playerData.playerId);
        SendGameStateToAll();
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in HandleJoinRequest: " + std::string(e.what()), error);
    }
    catch (...) {
        Utils::printMsg("Unknown exception in HandleJoinRequest", error);
    }
}

void GameServer::HandlePlayerUpdate(const PlayerUpdateMessage& msg, sf::IpAddress clientIP, unsigned short clientPort) {
    try {
        if (!NetworkValidation::IsValidPlayerId(msg.playerId)) {
           // Utils::printMsg("Invalid player ID received: " + std::to_string(msg.playerId), warning);
            return;
        }

        if (!NetworkValidation::IsValidPosition(msg.x, msg.y)) {
           // Utils::printMsg("Invalid position from player " + std::to_string(msg.playerId) +
             //   " (" + std::to_string(msg.x) + ", " + std::to_string(msg.y) + ")", warning);
            return;
        }

        if (!NetworkValidation::IsValidRotation(msg.bodyRotation) ||
            !NetworkValidation::IsValidRotation(msg.barrelRotation)) {
           // Utils::printMsg("Invalid rotation from player " + std::to_string(msg.playerId) +
            //    " (body: " + std::to_string(msg.bodyRotation) +
             //   ", barrel: " + std::to_string(msg.barrelRotation) + ")", warning);
            return;
        }

        int64_t currentTime = GetCurrentTimestamp();
        if (!NetworkValidation::IsValidTimestamp(msg.timestamp, currentTime)) {
          //  Utils::printMsg("Invalid timestamp from player " + std::to_string(msg.playerId) +
           //     " (delta: " + std::to_string(std::abs(currentTime - msg.timestamp)) + "ms)", debug);
        }

        auto it = clients.find(msg.playerId);
        if (it != clients.end()) {
            if (it->second.address == clientIP && it->second.port == clientPort) {
                if (!ValidateSequenceNumber(it->second, msg.sequenceNumber)) {
                    Utils::printMsg("Out-of-order or duplicate packet from player " +
                        std::to_string(msg.playerId) + " (Seq: " +
                        std::to_string(msg.sequenceNumber) + ")", debug);
                }

                float clampedX = NetworkValidation::ClampPositionX(msg.x);
                float clampedY = NetworkValidation::ClampPositionY(msg.y);

                if (clampedX != msg.x || clampedY != msg.y) {
                  //  Utils::printMsg("Clamped position for player " + std::to_string(msg.playerId) +
                    //    " from (" + std::to_string(msg.x) + ", " + std::to_string(msg.y) +
                    //    ") to (" + std::to_string(clampedX) + ", " + std::to_string(clampedY) + ")",
                   //    debug);
                }

                it->second.playerData.x = clampedX;
                it->second.playerData.y = clampedY;
                it->second.playerData.bodyRotation = NetworkValidation::NormalizeRotation(msg.bodyRotation);
                it->second.playerData.barrelRotation = NetworkValidation::NormalizeRotation(msg.barrelRotation);
                it->second.playerData.isMoving_forward = msg.isMoving_forward;
                it->second.playerData.isMoving_backward = msg.isMoving_backward;
                it->second.playerData.isMoving_left = msg.isMoving_left;
                it->second.playerData.isMoving_right = msg.isMoving_right;
                it->second.lastUpdateTime = 0;

                RecordReceivedSequence(it->second, msg.sequenceNumber);
            }
            else {
              //  Utils::printMsg("Player update from incorrect address for ID " +
              //      std::to_string(msg.playerId), warning);
            }
        }
        else {
          //  Utils::printMsg("Received update for unknown player ID: " + std::to_string(msg.playerId), warning);
        }
    }
    catch (const std::exception& e) {
      //  Utils::printMsg("Exception in HandlePlayerUpdate: " + std::string(e.what()), error);
    }
    catch (...) {
      //  Utils::printMsg("Unknown exception in HandlePlayerUpdate", error);
    }
}

void GameServer::HandlePlayerInput(const PlayerInputMessage& msg, sf::IpAddress clientIP, unsigned short clientPort) {
    try {
        if (!NetworkValidation::IsValidPlayerId(msg.playerId)) {
            Utils::printMsg("Invalid player ID in input: " + std::to_string(msg.playerId), warning);
            return;
        }

        int64_t currentTime = GetCurrentTimestamp();
        if (!NetworkValidation::IsValidTimestamp(msg.timestamp, currentTime)) {
            Utils::printMsg("Invalid timestamp from player " + std::to_string(msg.playerId) +
                " (delta: " + std::to_string(std::abs(currentTime - msg.timestamp)) + "ms)", debug);
        }

        if (!NetworkValidation::IsValidRotation(msg.barrelRotation)) {
            Utils::printMsg("Invalid barrel rotation from player " + std::to_string(msg.playerId) +
                ": " + std::to_string(msg.barrelRotation), debug);
        }

        auto it = clients.find(msg.playerId);
        if (it != clients.end()) {
            if (it->second.address == clientIP && it->second.port == clientPort) {
                if (!ValidateSequenceNumber(it->second, msg.sequenceNumber)) {
                    Utils::printMsg("Out-of-order input from player " +
                        std::to_string(msg.playerId) + " (Seq: " +
                        std::to_string(msg.sequenceNumber) + ")", debug);
                }

                it->second.playerData.isMoving_forward = msg.isMoving_forward;
                it->second.playerData.isMoving_backward = msg.isMoving_backward;
                it->second.playerData.isMoving_left = msg.isMoving_left;
                it->second.playerData.isMoving_right = msg.isMoving_right;
                it->second.playerData.barrelRotation = NetworkValidation::NormalizeRotation(msg.barrelRotation);

               // static int logCounter2 = 0;
               // if (++logCounter2 % 30 == 0) {
                 //   Utils::printMsg(" SERVER RECV: Player=" + std::to_string(msg.playerId) +
                 //       " Barrel=" + std::to_string(msg.barrelRotation) + "° → Stored=" +
                 //       std::to_string(it->second.playerData.barrelRotation) + "°", debug);
              //  }

                it->second.lastUpdateTime = 0;
                RecordReceivedSequence(it->second, msg.sequenceNumber);

                it->second.lastAcknowledgedInputSeq = msg.sequenceNumber;
                SendInputAcknowledgment(msg.playerId, msg.sequenceNumber, clientIP, clientPort);
            }
            else {
                Utils::printMsg("Input from incorrect address for player " +
                    std::to_string(msg.playerId), warning);
            }
        }
        else {
          //  Utils::printMsg("Received input for unknown player ID: " + std::to_string(msg.playerId), warning);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in HandlePlayerInput: " + std::string(e.what()), error);
    }
    catch (...) {
        Utils::printMsg("Unknown exception in HandlePlayerInput", error);
    }
}

void GameServer::HandlePing(const PingMessage& msg, sf::IpAddress clientIP, unsigned short clientPort) {
    try {
        sf::Packet packet;
        PongMessage pongMsg;
        pongMsg.originalTimestamp = msg.timestamp;
        pongMsg.sequenceNumber = msg.sequenceNumber;

        packet << static_cast<uint8_t>(pongMsg.type) << pongMsg.originalTimestamp << pongMsg.sequenceNumber;

        sf::Socket::Status sendStatus = socket.send(packet, clientIP, clientPort);

        if (sendStatus != sf::Socket::Status::Done && sendStatus != sf::Socket::Status::NotReady) {
            Utils::printMsg("Failed to send pong to " + clientIP.toString() +
                " - Status: " + SocketStatusToString(sendStatus), warning);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in HandlePing: " + std::string(e.what()), error);
    }
    catch (...) {
        Utils::printMsg("Unknown exception in HandlePing", error);
    }
}

bool GameServer::ValidateSequenceNumber(ClientInfo& client, uint32_t sequenceNumber) {
    if (client.receivedSequenceNumbers.find(sequenceNumber) != client.receivedSequenceNumbers.end()) {
        return false;
    }

    const uint32_t OUT_OF_ORDER_THRESHOLD = 50;
    if (sequenceNumber + OUT_OF_ORDER_THRESHOLD < client.lastReceivedSequenceNumber) {
        return false;
    }

    return true;
}

void GameServer::RecordReceivedSequence(ClientInfo& client, uint32_t sequenceNumber) {
    client.receivedSequenceNumbers[sequenceNumber] = true;

    if (sequenceNumber > client.lastReceivedSequenceNumber) {
        client.lastReceivedSequenceNumber = sequenceNumber;
    }

    const size_t MAX_SEQUENCE_HISTORY = 200;
    if (client.receivedSequenceNumbers.size() > MAX_SEQUENCE_HISTORY) {
        uint32_t minSequence = client.lastReceivedSequenceNumber > MAX_SEQUENCE_HISTORY ?
            client.lastReceivedSequenceNumber - MAX_SEQUENCE_HISTORY : 0;

        auto it = client.receivedSequenceNumbers.begin();
        while (it != client.receivedSequenceNumbers.end()) {
            if (it->first < minSequence) {
                it = client.receivedSequenceNumbers.erase(it);
            }
            else {
                ++it;
            }
        }
    }
}

void GameServer::SendPlayerIdAssignment(uint32_t playerId, sf::IpAddress clientIP, unsigned short clientPort) {
    try {
        sf::Packet packet;
        packet << static_cast<uint8_t>(NetMessageType::PLAYER_ID_ASSIGNMENT);
        packet << playerId;

        sf::Socket::Status sendStatus = socket.send(packet, clientIP, clientPort);

        if (sendStatus != sf::Socket::Status::Done && sendStatus != sf::Socket::Status::NotReady) {
            Utils::printMsg("Failed to send player ID to player " + std::to_string(playerId) +
                " - Status: " + SocketStatusToString(sendStatus), warning);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in SendPlayerIdAssignment: " + std::string(e.what()), error);
    }
    catch (...) {
        Utils::printMsg("Unknown exception in SendPlayerIdAssignment", error);
    }
}

void GameServer::SendGameStateToAll() {
    if (clients.empty()) return;

    try {
        sf::Packet packet;
        packet << static_cast<uint8_t>(NetMessageType::GAME_STATE);

        uint32_t activeClientCount = 0;
        for (const auto& [playerId, client] : clients) {
            if (client.isActive) {
                activeClientCount++;
            }
        }
        packet << activeClientCount;

        for (auto& [playerId, client] : clients) {  // Changed to non-const to allow modification
            if (client.isActive) {
                // CRITICAL: Sync client.score to client.playerData.score before sending
                client.playerData.score = client.score;
                client.playerData.isDead = client.isDead;

                packet << client.playerData.playerId
                    << client.playerData.playerName
                    << client.playerData.x
                    << client.playerData.y
                    << client.playerData.bodyRotation
                    << client.playerData.barrelRotation
                    << client.playerData.color
                    << client.playerData.isMoving_forward
                    << client.playerData.isMoving_backward
                    << client.playerData.isMoving_left
                    << client.playerData.isMoving_right
                    << client.playerData.health
                    << client.playerData.maxHealth
                    << client.playerData.score
                    << client.playerData.isDead;
            }
        }

        uint32_t enemyCount = static_cast<uint32_t>(enemies.size());
        packet << enemyCount;

        for (const auto& [enemyId, enemy] : enemies) {
            if (enemy) {
                EnemyData enemyData;
                enemyData.enemyId = enemyId;
                enemyData.enemyType = static_cast<uint8_t>(enemy->GetEnemyType());
                enemyData.x = enemy->GetPosition().x;
                enemyData.y = enemy->GetPosition().y;
                enemyData.bodyRotation = enemy->GetBodyRotation().asDegrees();
                enemyData.barrelRotation = enemy->GetBarrelRotation().asDegrees();
                enemyData.health = enemy->GetHealth();
                enemyData.maxHealth = enemy->GetMaxHealth();

                packet << enemyData.enemyId
                    << enemyData.enemyType
                    << enemyData.x
                    << enemyData.y
                    << enemyData.bodyRotation
                    << enemyData.barrelRotation
                    << enemyData.health
                    << enemyData.maxHealth;
            }
        }

        int64_t timestamp = GetCurrentTimestamp();
        uint32_t lastAckedInput = 0;
        packet << timestamp << outgoingSequenceNumber++ << lastAckedInput;

        for (const auto& [playerId, client] : clients) {
            if (client.isActive) {
                sf::Socket::Status sendStatus = socket.send(packet, client.address, client.port);

                if (sendStatus != sf::Socket::Status::Done && sendStatus != sf::Socket::Status::NotReady) {
                    Utils::printMsg("Failed to send game state to player " + std::to_string(playerId) +
                        " - Status: " + SocketStatusToString(sendStatus), warning);
                }
            }
        }

        static int syncCounter = 0;
        if (++syncCounter % 100 == 0) {
            Utils::printMsg("Synced " + std::to_string(enemyCount) + " enemies to " +
                std::to_string(activeClientCount) + " clients", debug);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in SendGameStateToAll: " + std::string(e.what()), error);
    }
    catch (...) {
        Utils::printMsg("Unknown exception in SendGameStateToAll", error);
    }
}

void GameServer::SendGameStateToClient(uint32_t playerId) {
    auto it = clients.find(playerId);
    if (it == clients.end() || !it->second.isActive) return;

    try {
        sf::Packet packet;
        packet << static_cast<uint8_t>(NetMessageType::GAME_STATE);

        uint32_t activeClientCount = 0;
        for (const auto& [id, client] : clients) {
            if (client.isActive) {
                activeClientCount++;
            }
        }
        packet << activeClientCount;

        for (auto& [id, client] : clients) {  // Changed to non-const
            if (client.isActive) {
                //  Sync client.score to client.playerData.score before sending
                client.playerData.score = client.score;
                client.playerData.isDead = client.isDead;

                packet << client.playerData.playerId << client.playerData.playerName
                    << client.playerData.x << client.playerData.y
                    << client.playerData.bodyRotation << client.playerData.barrelRotation
                    << client.playerData.color << client.playerData.isMoving_forward
                    << client.playerData.isMoving_backward << client.playerData.isMoving_left
                    << client.playerData.isMoving_right << client.playerData.health
                    << client.playerData.maxHealth << client.playerData.score
                    << client.playerData.isDead;
            }
        }

        uint32_t enemyCount = static_cast<uint32_t>(enemies.size());
        packet << enemyCount;

        for (const auto& [enemyId, enemy] : enemies) {
            if (enemy) {
                EnemyData enemyData;
                enemyData.enemyId = enemyId;
                enemyData.enemyType = static_cast<uint8_t>(enemy->GetEnemyType());
                enemyData.x = enemy->GetPosition().x;
                enemyData.y = enemy->GetPosition().y;
                enemyData.bodyRotation = enemy->GetBodyRotation().asDegrees();
                enemyData.barrelRotation = enemy->GetBarrelRotation().asDegrees();
                enemyData.health = enemy->GetHealth();
                enemyData.maxHealth = enemy->GetMaxHealth();

                packet << enemyData.enemyId
                    << enemyData.enemyType
                    << enemyData.x
                    << enemyData.y
                    << enemyData.bodyRotation
                    << enemyData.barrelRotation
                    << enemyData.health
                    << enemyData.maxHealth;
            }
        }

        int64_t timestamp = GetCurrentTimestamp();
        uint32_t lastAckedInput = 0;
        packet << timestamp << outgoingSequenceNumber++ << lastAckedInput;

        sf::Socket::Status sendStatus = socket.send(packet, it->second.address, it->second.port);

        if (sendStatus != sf::Socket::Status::Done && sendStatus != sf::Socket::Status::NotReady) {
            Utils::printMsg("Failed to send initial game state to player " + std::to_string(playerId) +
                " - Status: " + SocketStatusToString(sendStatus), warning);
        }
        else {
            Utils::printMsg("Sent initial game state with " + std::to_string(enemyCount) +
                " enemies to player " + std::to_string(playerId), debug);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in SendGameStateToClient: " + std::string(e.what()), error);
    }
    catch (...) {
        Utils::printMsg("Unknown exception in SendGameStateToClient", error);
    }
}

void GameServer::RemoveInactiveClients(float deltaTime) {
    try {
        std::vector<uint32_t> toRemove;

        for (auto& [playerId, client] : clients) {
            if (client.isActive) {
                client.lastUpdateTime += deltaTime;
                if (client.lastUpdateTime > clientTimeoutDuration) {
                    Utils::printMsg("Player " + std::to_string(playerId) + " (" + client.playerData.playerName + ") timed out", warning);
                    client.isActive = false;
                    toRemove.push_back(playerId);
                }
            }
        }

        for (uint32_t playerId : toRemove) {
            BroadcastPlayerLeft(playerId);
            clients.erase(playerId);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in RemoveInactiveClients: " + std::string(e.what()), error);
    }
    catch (...) {
        Utils::printMsg("Unknown exception in RemoveInactiveClients", error);
    }
}

void GameServer::BroadcastPlayerLeft(uint32_t playerId) {
    SendGameStateToAll();
}

uint32_t GameServer::FindPlayerByAddress(sf::IpAddress address, unsigned short port) {
    for (const auto& [playerId, client] : clients) {
        if (client.address == address && client.port == port) {
            return playerId;
        }
    }
    return 0;
}

std::string GameServer::AssignColor() {
    std::vector<std::string> usedColors;
    for (const auto& [playerId, client] : clients) {
        usedColors.push_back(client.playerData.color);
    }

    for (const std::string& color : availableColors) {
        if (std::find(usedColors.begin(), usedColors.end(), color) == usedColors.end()) {
            return color;
        }
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, static_cast<int>(availableColors.size()) - 1);
    return availableColors[dist(gen)];
}

void GameServer::PrintServerStats() {
    if (clients.empty()) {
        Utils::printMsg("Server running - No players connected - Enemies: " +
            std::to_string(enemies.size()));
    }
    else {
        std::string playerList = "Server running - " + std::to_string(clients.size()) +
            " players connected - Enemies: " + std::to_string(enemies.size()) +
            " - Players: ";
        for (const auto& [playerId, client] : clients) {
            if (client.isActive) {
                playerList += client.playerData.playerName + " ";
            }
        }
        Utils::printMsg(playerList);
    }
}

void GameServer::SimulatePlayerMovement(float deltaTime) {
    const float MOVEMENT_SPEED = 150.0f;
    const float ROTATION_SPEED = 200.0f;

    for (auto& [playerId, client] : clients) {
        if (!client.isActive) continue;

        PlayerData& player = client.playerData;

        if (player.isMoving_left) {
            player.bodyRotation -= ROTATION_SPEED * deltaTime;
        }
        else if (player.isMoving_right) {
            player.bodyRotation += ROTATION_SPEED * deltaTime;
        }

        while (player.bodyRotation < 0.0f) {
            player.bodyRotation += 360.0f;
        }
        while (player.bodyRotation >= 360.0f) {
            player.bodyRotation -= 360.0f;
        }

        float radians = player.bodyRotation * 3.14159f / 180.0f;
        float dirX = std::cos(radians);
        float dirY = std::sin(radians);

        if (player.isMoving_forward) {
            player.x += dirX * MOVEMENT_SPEED * deltaTime;
            player.y += dirY * MOVEMENT_SPEED * deltaTime;
        }
        else if (player.isMoving_backward) {
            player.x -= dirX * MOVEMENT_SPEED * deltaTime;
            player.y -= dirY * MOVEMENT_SPEED * deltaTime;
        }

        player.x = std::max(WorldConstants::MOVEMENT_MIN_X,
            std::min(WorldConstants::MOVEMENT_MAX_X, player.x));
        player.y = std::max(WorldConstants::MOVEMENT_MIN_Y,
            std::min(WorldConstants::MOVEMENT_MAX_Y, player.y));
    }
}

void GameServer::Shutdown() {
    try {
        if (isRunning) {
            Utils::printMsg("Shutting down game server...", warning);
            CleanupSocketResources();
            clients.clear();
            enemies.clear();
            bullets.clear();

            isRunning = false;
            Utils::printMsg("Game server shut down", success);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception during server shutdown: " + std::string(e.what()), error);
        isRunning = false;
    }
    catch (...) {
        Utils::printMsg("Unknown exception during server shutdown", error);
        isRunning = false;
    }
}

void GameServer::CleanupSocketResources() {
    try {
        socket.unbind();
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception during socket cleanup: " + std::string(e.what()), warning);
    }
    catch (...) {
        Utils::printMsg("Unknown exception during socket cleanup", warning);
    }
}

std::string GameServer::SocketStatusToString(sf::Socket::Status status) const {
    switch (status) {
    case sf::Socket::Status::Done:         return "Done";
    case sf::Socket::Status::NotReady:     return "NotReady";
    case sf::Socket::Status::Partial:      return "Partial";
    case sf::Socket::Status::Disconnected: return "Disconnected";
    case sf::Socket::Status::Error:        return "Error";
    default:                                return "Unknown";
    }
}

void GameServer::DetectAndReportPacketLoss() {
    for (auto& [playerId, client] : clients) {
        if (!client.isActive) continue;

        if (client.lastReceivedSequenceNumber > NetworkValidation::SEQUENCE_WINDOW_SIZE) {
            uint32_t expectedPackets = NetworkValidation::SEQUENCE_WINDOW_SIZE;
            uint32_t receivedPackets = static_cast<uint32_t>(client.receivedSequenceNumbers.size());

            if (receivedPackets < expectedPackets) {
                float lossPercentage = ((expectedPackets - receivedPackets) /
                    static_cast<float>(expectedPackets)) * 100.0f;

                if (lossPercentage >= NetworkValidation::PACKET_LOSS_THRESHOLD) {
                    Utils::printMsg("High packet loss detected for player " +
                        std::to_string(playerId) + " (" +
                        client.playerData.playerName + "): " +
                        std::to_string(lossPercentage) + "%", warning);
                }
            }
        }
    }
}

void GameServer::SendInputAcknowledgment(uint32_t playerId, uint32_t acknowledgedSeq,
    sf::IpAddress clientIP, unsigned short clientPort) {
    try {
        sf::Packet packet;
        InputAcknowledgmentMessage ackMsg;
        ackMsg.type = NetMessageType::INPUT_ACKNOWLEDGMENT;
        ackMsg.playerId = playerId;
        ackMsg.acknowledgedSequence = acknowledgedSeq;
        ackMsg.serverTimestamp = GetCurrentTimestamp();

        packet << static_cast<uint8_t>(ackMsg.type)
            << ackMsg.playerId
            << ackMsg.acknowledgedSequence
            << ackMsg.serverTimestamp;

        sf::Socket::Status sendStatus = socket.send(packet, clientIP, clientPort);

        if (sendStatus != sf::Socket::Status::Done && sendStatus != sf::Socket::Status::NotReady) {
            static int failCount = 0;
            if (++failCount % 100 == 0) {
                Utils::printMsg("Failed to send input acks (count: " +
                    std::to_string(failCount) + ")", warning);
            }
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in SendInputAcknowledgment: " + std::string(e.what()), error);
    }
}

uint64_t GameServer::GetCurrentTimestamp() const {
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return static_cast<uint64_t>(ms.count());
}
void GameServer::DiagnoseEnemyShooting(uint32_t enemyId, EnemyTank* enemy) {
    if (!enemy) return;

    float distance = 0.0f;
    if (enemy->HasTarget()) {
        auto it = clients.find(enemy->GetTargetPlayerId());
        if (it != clients.end()) {
            sf::Vector2f targetPos(it->second.playerData.x, it->second.playerData.y);
            float dx = targetPos.x - enemy->GetPosition().x;
            float dy = targetPos.y - enemy->GetPosition().y;
            distance = std::sqrt(dx * dx + dy * dy);
        }
    }

   /* Utils::printMsg("=== SHOOTING DIAGNOSTIC: Enemy " + std::to_string(enemyId) + " ===", warning);
    Utils::printMsg("  Type: " + enemy->GetEnemyTypeName(), info);
    Utils::printMsg("  AI State: " + enemy->GetAIStateName(), info);
    Utils::printMsg("  Has Target: " + std::string(enemy->HasTarget() ? "YES" : "NO"), info);

    if (enemy->HasTarget()) {
        Utils::printMsg("  Target Player ID: " + std::to_string(enemy->GetTargetPlayerId()), info);
        Utils::printMsg("  Distance to Target: " + std::to_string(distance), info);
        Utils::printMsg("  Attack Range: " + std::to_string(enemy->GetAttackRange()), info);
        Utils::printMsg("  In Attack Range: " + std::string(distance <= enemy->GetAttackRange() ? "YES" : "NO"), info);
    }

    Utils::printMsg("  Can Shoot (cooldown ready): " + std::string(enemy->CanShoot() ? "YES" : "NO"), info);
    Utils::printMsg("  Cooldown Remaining: " + std::to_string(enemy->GetShootCooldown()) + "s", info);
    Utils::printMsg("======================================", warning);*/
}
void GameServer::UpdateEnemies(float deltaTime) {
    try {
        enemySpawnTimer += deltaTime;

        // Calculate dynamic max enemies based on player count
        // Formula: 3 * (PlayerCount > 0) + PlayerCount
        int activePlayerCount = 0;
        for (const auto& [playerId, client] : clients) {
            if (client.isActive && !client.isDead) {
                activePlayerCount++;
            }
        }
        int dynamicMaxEnemies = 3 * (activePlayerCount > 0 ? 1 : 0) + activePlayerCount;

        // Spawn enemy if timer ready and below max
        if (enemySpawnTimer >= enemySpawnInterval &&
            enemies.size() < static_cast<size_t>(dynamicMaxEnemies)) {

            SpawnEnemy();
            enemySpawnTimer = 0;

            // Log dynamic max for debugging
            static int spawnLogCounter = 0;
            if (++spawnLogCounter % 5 == 0) {
                Utils::printMsg("🎮 Enemy spawned | Active Players: " +
                    std::to_string(activePlayerCount) +
                    " | Max Enemies: " + std::to_string(dynamicMaxEnemies) +
                    " | Current: " + std::to_string(enemies.size()), info);
            }
        }

        //Track which enemies shot THIS frame to prevent multi-spawn
        std::unordered_set<uint32_t> enemiesWhoShotThisFrame;

        for (auto& [enemyId, enemy] : enemies) {
            if (!enemy) continue;

            // TARGET ACQUISITION
            if (!enemy->HasTarget()) {
                uint32_t targetId = SelectTargetForEnemy(enemy.get());
                if (targetId != 0) {
                    auto it = clients.find(targetId);
                    if (it != clients.end() && it->second.isActive) {
                        sf::Vector2f targetPos(it->second.playerData.x, it->second.playerData.y);
                        enemy->SelectNewTarget(targetId, targetPos);

                        float distance = std::sqrt(
                            std::pow(targetPos.x - enemy->GetPosition().x, 2) +
                            std::pow(targetPos.y - enemy->GetPosition().y, 2)
                        );

                      //  Utils::printMsg(enemy->GetEnemyTypeName() + " (ID: " +
                        //    std::to_string(enemyId) + ") NOW TARGETING player " +
                         //   std::to_string(targetId) + " at distance " +
                          //  std::to_string(distance), success);
                    }
                }
            }
            else {
                UpdateEnemyTargetPosition(enemy.get());
            }

            //  Store cooldown BEFORE update
            float cooldownBefore = enemy->GetShootCooldown();

            // AI BEHAVIOR UPDATE
            enemy->Update(deltaTime);

            //  Store cooldown AFTER update
            float cooldownAfter = enemy->GetShootCooldown();

            //  DEBUG: Log attack state enemies
            if (enemy->GetAIState() == EnemyTank::AIState::ATTACK) {
                static std::unordered_map<uint32_t, float> attackLogTimers;
                attackLogTimers[enemyId] += deltaTime;

                if (attackLogTimers[enemyId] >= 2.0f) {
                   // Utils::printMsg(" ATTACKING: " + enemy->GetEnemyTypeName() +
                    //    " (ID: " + std::to_string(enemyId) + ") | " +
                     //   "Cooldown: " + std::to_string(cooldownAfter) + "s | " +
                      //  "Target: " + std::to_string(enemy->GetTargetPlayerId()), warning);
                    attackLogTimers[enemyId] = 0.0f;
                }
            }

            //  SHOOTING DETECTION - Check if cooldown INCREASED
            bool enemyJustShot = false;

            if (cooldownAfter > cooldownBefore) {
                // Cooldown increased - enemy just shot!
                float cooldownJump = cooldownAfter - cooldownBefore;

                // Verify it's a real shot (not just floating point error)
                if (cooldownJump > 0.5f) {
                    enemyJustShot = true;

                    //  DEBUG: Confirm shot detection
                   /* Utils::printMsg(" SHOT DETECTED: " + enemy->GetEnemyTypeName() +
                        " (ID: " + std::to_string(enemyId) + ") | " +
                        "Cooldown jump: " + std::to_string(cooldownBefore) + " → " +
                        std::to_string(cooldownAfter), success);*/
                }
            }

            // Spawn server bullet if enemy shot AND we haven't already spawned for this enemy
            if (enemyJustShot && enemiesWhoShotThisFrame.find(enemyId) == enemiesWhoShotThisFrame.end()) {
                SpawnEnemyBullet(enemyId, enemy.get());
                enemiesWhoShotThisFrame.insert(enemyId);

              //  Utils::printMsg(" Server confirmed bullet spawn for enemy " +
                   // std::to_string(enemyId), success);
            }
        }

        RemoveDeadEnemies();
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in UpdateEnemies: " + std::string(e.what()), error);
    }
}
// SelectTargetForEnemy - Add scoring logic for best target

uint32_t GameServer::SelectTargetForEnemy(EnemyTank* enemy) {
    if (!enemy) return 0;

    uint32_t bestTarget = 0;
    float bestScore = -1.0f;

    sf::Vector2f enemyPos = enemy->GetPosition();
    float detectionRange = enemy->GetDetectionRange();

    for (const auto& [playerId, client] : clients) {
        if (!client.isActive) continue;

        sf::Vector2f playerPos(client.playerData.x, client.playerData.y);

        // Calculate distance
        float dx = playerPos.x - enemyPos.x;
        float dy = playerPos.y - enemyPos.y;
        float distance = std::sqrt(dx * dx + dy * dy);

        // Skip if out of detection range
        if (distance > detectionRange) continue;

        // Calculate threat score (closer = higher priority)
        float proximityScore = 1.0f - (distance / detectionRange);
        float score = proximityScore * 100.0f;

        // Optional: Add health-based scoring (target low health players)
        float healthFactor = 1.0f - (client.playerData.health / client.playerData.maxHealth);
        score += healthFactor * 20.0f;

        // Check if this is the best target so far
        if (score > bestScore) {
            bestScore = score;
            bestTarget = playerId;
        }
    }

    // Log targeting decision
    if (bestTarget != 0) {
      //  Utils::printMsg(enemy->GetEnemyTypeName() + " selected target " +
       //     std::to_string(bestTarget) + " (score: " +
       //     std::to_string(bestScore) + ")", debug);
    }

    return bestTarget;
}

void GameServer::UpdateEnemyTargetPosition(EnemyTank* enemy) {
    if (!enemy || !enemy->HasTarget()) return;

    uint32_t targetId = enemy->GetTargetPlayerId();

    auto it = clients.find(targetId);
    if (it == clients.end() || !it->second.isActive) {
        // Target disconnected or became inactive
        enemy->ClearTarget();
      //  Utils::printMsg(enemy->GetEnemyTypeName() + " lost target " +
       //     std::to_string(targetId) + " (disconnected)", debug);
        return;
    }

    // Update enemy with fresh player position
    sf::Vector2f playerPos(it->second.playerData.x, it->second.playerData.y);

    // Calculate distance
    float distance = std::sqrt(
        std::pow(playerPos.x - enemy->GetPosition().x, 2) +
        std::pow(playerPos.y - enemy->GetPosition().y, 2)
    );

    // Check if target escaped detection range (with hysteresis)
    float detectionRange = enemy->GetDetectionRange();
    if (distance > detectionRange * 2.0f) {
        enemy->ClearTarget();
      //  Utils::printMsg(enemy->GetEnemyTypeName() + " lost target " +
       //     std::to_string(targetId) + " (out of range: " +
       //     std::to_string(distance) + ")", debug);
        return;
    }

    // Update target position for AI to use
    enemy->SelectNewTarget(targetId, playerPos);
}

void GameServer::SpawnEnemy() {
    try {
        sf::Vector2f spawnPos = GetRandomSpawnPosition();
        EnemyTank::EnemyType enemyType = GetRandomEnemyType();

        auto newEnemy = std::make_unique<EnemyTank>(enemyType, spawnPos);

        uint32_t enemyId = nextEnemyId++;

        enemies[enemyId] = std::move(newEnemy);

        Utils::printMsg("Spawned " + enemies[enemyId]->GetEnemyTypeName() +
            " (ID: " + std::to_string(enemyId) + ") at (" +
            std::to_string(spawnPos.x) + ", " + std::to_string(spawnPos.y) + ")",
            success);
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in SpawnEnemy: " + std::string(e.what()), error);
    }
}

sf::Vector2f GameServer::GetRandomSpawnPosition() {
    const float SPAWN_MIN_X = WorldConstants::SPAWN_MIN_X;
    const float SPAWN_MAX_X = WorldConstants::SPAWN_MAX_X;
    const float SPAWN_MIN_Y = WorldConstants::SPAWN_MIN_Y;
    const float SPAWN_MAX_Y = WorldConstants::SPAWN_MAX_Y;

    std::uniform_real_distribution<float> distX(SPAWN_MIN_X, SPAWN_MAX_X);
    std::uniform_real_distribution<float> distY(SPAWN_MIN_Y, SPAWN_MAX_Y);

    float x = distX(randomGenerator);
    float y = distY(randomGenerator);

    return sf::Vector2f(x, y);
}

EnemyTank::EnemyType GameServer::GetRandomEnemyType() {
    std::uniform_int_distribution<int> dist(1, 100);
    int roll = dist(randomGenerator);

    if (roll <= 40) {
        return EnemyTank::EnemyType::RED;
    }
    else if (roll <= 60) {
        return EnemyTank::EnemyType::BLACK;
    }
    else if (roll <= 80) {
        return EnemyTank::EnemyType::PURPLE;
    }
    else if (roll <= 95) {
        return EnemyTank::EnemyType::TEAL;
    }
    else {
        return EnemyTank::EnemyType::ORANGE;
    }
}

void GameServer::RemoveDeadEnemies() {
    auto it = enemies.begin();
    while (it != enemies.end()) {
        if (it->second && it->second->IsDead()) {
            Utils::printMsg("Removing dead enemy (ID: " + std::to_string(it->first) + ")",
                debug);
            it = enemies.erase(it);
        }
        else {
            ++it;
        }
    }
}

void GameServer::SpawnEnemyBullet(uint32_t enemyId, EnemyTank* enemy) {
    if (!enemy) return;

    try {
        sf::Vector2f spawnPos = enemy->GetBarrelEndPosition();
        sf::Vector2f direction = enemy->GetAimDirection();
        sf::Vector2f finalDirection = enemy->ApplyAccuracySpread(direction);

        // DEBUG: Detailed bullet spawn info
      /*  Utils::printMsg("ENEMY BULLET SPAWN: Enemy " + std::to_string(enemyId) +
            " (" + enemy->GetEnemyTypeName() + ") at (" +
            std::to_string(spawnPos.x) + ", " + std::to_string(spawnPos.y) +
            ") direction=(" + std::to_string(finalDirection.x) + ", " +
            std::to_string(finalDirection.y) + ")", success);*/

        auto bullet = std::make_unique<Bullet>(
            Bullet::BulletType::ENEMY_STANDARD,
            spawnPos,
            finalDirection,
            enemyId  // This is the owner ID
        );

        uint32_t bulletId = nextBulletId++;
        bullet->SetBulletId(bulletId);

        // DEBUG: Verify owner ID
     /*   Utils::printMsg("  Bullet ID: " + std::to_string(bulletId) +
            " | Owner ID: " + std::to_string(bullet->GetOwnerId()) +
            " | Is Enemy Bullet: " + std::string(bullet->GetOwnerId() >= 1000 ? "YES" : "NO"),
            info);*/

        bullets[bulletId] = std::move(bullet);

        BroadcastEnemyBulletSpawn(bulletId, spawnPos, finalDirection, enemyId);

     /*   Utils::printMsg("  Bullet " + std::to_string(bulletId) + " added to bullets map (total: " +
            std::to_string(bullets.size()) + ")", success);*/
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in SpawnEnemyBullet: " + std::string(e.what()), error);
    }
}

void GameServer::BroadcastEnemyBulletSpawn(uint32_t bulletId, sf::Vector2f position,
    sf::Vector2f direction, uint32_t ownerId) {
    try {
        sf::Packet packet;

        BulletSpawnMessage msg;
        msg.type = NetMessageType::BULLET_SPAWN;
        //  Use correct field names
        msg.playerId = ownerId;  // NOTE: For enemy bullets, this is enemyId (owner)
        msg.spawnX = position.x;
        msg.spawnY = position.y;
        msg.directionX = direction.x;
        msg.directionY = direction.y;
        msg.barrelRotation = std::atan2(direction.y, direction.x) * 180.0f / 3.14159f;
        msg.timestamp = GetCurrentTimestamp();
        msg.sequenceNumber = outgoingSequenceNumber++;

        packet << static_cast<uint8_t>(msg.type)
            << msg.playerId
            << msg.spawnX
            << msg.spawnY
            << msg.directionX
            << msg.directionY
            << msg.barrelRotation
            << msg.timestamp
            << msg.sequenceNumber;

        for (const auto& [playerId, client] : clients) {
            if (client.isActive) {
                socket.send(packet, client.address, client.port);
            }
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in BroadcastEnemyBulletSpawn: " + std::string(e.what()), error);
    }
}

void GameServer::HandleBulletSpawn(const BulletSpawnMessage& msg,
    sf::IpAddress clientIP, unsigned short clientPort) {
    try {
        auto clientIt = clients.find(msg.playerId);
        if (clientIt == clients.end()) {
            Utils::printMsg("Bullet spawn from unknown player: " + std::to_string(msg.playerId), warning);
            return;
        }

        if (clientIt->second.address != clientIP || clientIt->second.port != clientPort) {
            Utils::printMsg("Bullet spawn from incorrect address", warning);
            return;
        }

        if (!ValidateBulletSpawnRequest(msg, msg.playerId)) {
            Utils::printMsg("Invalid bullet spawn request from player " + std::to_string(msg.playerId), warning);
            return;
        }

        sf::Vector2f spawnPos(msg.spawnX, msg.spawnY);
        sf::Vector2f direction(msg.directionX, msg.directionY);

        float dirLength = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        if (dirLength > 0.001f) {
            direction /= dirLength;
        }

        auto bullet = std::make_unique<Bullet>(
            Bullet::BulletType::PLAYER_STANDARD,
            spawnPos,
            direction,
            msg.playerId
        );

        uint32_t bulletId = nextBulletId++;
        bullet->SetBulletId(bulletId);

        bullets[bulletId] = std::move(bullet);

        Utils::printMsg("Player " + std::to_string(msg.playerId) +
            " spawned bullet " + std::to_string(bulletId), success);

        SendBulletUpdates();
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in HandleBulletSpawn: " + std::string(e.what()), error);
    }
}

void GameServer::UpdateBullets(float deltaTime) {
    try {
        for (auto& [bulletId, bullet] : bullets) {
            if (bullet) {
                bullet->Update(deltaTime);
            }
        }

        CheckBulletCollisions();
        RemoveDeadBullets();
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in UpdateBullets: " + std::string(e.what()), error);
    }
}

void GameServer::SendBulletUpdates() {
    if (bullets.empty() || clients.empty()) {
        return;
    }

    try {
        sf::Packet packet;
        BulletUpdateMessage updateMsg;
        updateMsg.type = NetMessageType::BULLET_UPDATE;
        updateMsg.timestamp = GetCurrentTimestamp();
        updateMsg.sequenceNumber = outgoingSequenceNumber++;

        for (const auto& [bulletId, bullet] : bullets) {
            if (bullet && !bullet->IsDestroyed()) {
                BulletData data = BulletToBulletData(*bullet, bulletId);
                updateMsg.bullets.push_back(data);
            }
        }

        packet << static_cast<uint8_t>(updateMsg.type);

        uint32_t bulletCount = static_cast<uint32_t>(updateMsg.bullets.size());
        packet << bulletCount;

        for (const auto& bulletData : updateMsg.bullets) {
            packet << bulletData;
        }

        packet << updateMsg.timestamp << updateMsg.sequenceNumber;

        for (const auto& [playerId, client] : clients) {
            if (client.isActive) {
                sf::Socket::Status sendStatus = socket.send(packet, client.address, client.port);

                if (sendStatus != sf::Socket::Status::Done && sendStatus != sf::Socket::Status::NotReady) {
                    Utils::printMsg("Failed to send bullet update to player " +
                        std::to_string(playerId), debug);
                }
            }
        }

        static int updateCounter = 0;
        if (++updateCounter % 30 == 0) {
           // Utils::printMsg("Sent bullet update: " + std::to_string(bulletCount) +
             //   " bullets to " + std::to_string(clients.size()) + " clients", debug);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in SendBulletUpdates: " + std::string(e.what()), error);
    }
}

void GameServer::BroadcastBulletDestruction(uint32_t bulletId, uint8_t reason,
    uint32_t hitTargetId, sf::Vector2f hitPos) {
    if (clients.empty()) {
        return;
    }

    try {
        sf::Packet packet;
        BulletDestroyMessage destroyMsg;
        destroyMsg.type = NetMessageType::BULLET_DESTROY;
        destroyMsg.bulletId = bulletId;
        destroyMsg.destroyReason = reason;
        destroyMsg.hitTargetId = hitTargetId;
        destroyMsg.hitX = hitPos.x;
        destroyMsg.hitY = hitPos.y;
        destroyMsg.timestamp = GetCurrentTimestamp();
        destroyMsg.sequenceNumber = outgoingSequenceNumber++;

        packet << destroyMsg;

        for (const auto& [playerId, client] : clients) {
            if (client.isActive) {
                socket.send(packet, client.address, client.port);
            }
        }

      //  Utils::printMsg("Broadcast bullet " + std::to_string(bulletId) +
       //     " destruction (reason: " + std::to_string(reason) + ")", debug);
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in BroadcastBulletDestruction: " + std::string(e.what()), error);
    }
}


void GameServer::CheckBulletCollisions() {
    try {
        //  DEBUG: Log collision check start
        static int checkCounter = 0;
        if (++checkCounter % 60 == 0) {  // Every ~2 seconds at 30Hz
         //   Utils::printMsg("COLLISION CHECK: " + std::to_string(bullets.size()) +
           //     " bullets vs " + std::to_string(clients.size()) + " players vs " +
           //     std::to_string(enemies.size()) + " enemies", debug);
        }

        for (auto& [bulletId, bullet] : bullets) {
            if (!bullet || bullet->IsDestroyed()) {
                continue;
            }

            sf::Vector2f bulletPos = bullet->GetPosition();
            float bulletRadius = bullet->GetRadius();
            uint32_t ownerId = bullet->GetOwnerId();

            //  Determine bullet faction ONCE at the start
            bool isEnemyBullet = (ownerId >= 1000);  // Enemy IDs start at 1000
            bool isPlayerBullet = (ownerId < 1000);  // Player IDs are < 1000

            // : Log bullet info periodically
            static std::unordered_map<uint32_t, int> bulletLogCounters;
            if (++bulletLogCounters[bulletId] % 30 == 0) {
                std::string bulletType = isEnemyBullet ? "ENEMY" : "PLAYER";
               // Utils::printMsg("  Bullet " + std::to_string(bulletId) +
                //    " [" + bulletType + "] at (" +
                 //   std::to_string(bulletPos.x) + ", " + std::to_string(bulletPos.y) +
                  //  ") owner=" + std::to_string(ownerId), debug);
            }

            // CHECK: Bullet vs Enemies
            if (isPlayerBullet) {
                for (auto& [enemyId, enemy] : enemies) {
                    if (!enemy || enemy->IsDead()) {
                        continue;
                    }

                    sf::Vector2f enemyPos = enemy->GetPosition();
                    float enemyRadius = enemy->GetRadius();

                    float dx = bulletPos.x - enemyPos.x;
                    float dy = bulletPos.y - enemyPos.y;
                    float distSq = dx * dx + dy * dy;
                    float radiusSum = bulletRadius + enemyRadius;

                    if (distSq < radiusSum * radiusSum) {
                        float damage = bullet->GetDamage();
                        float oldHealth = enemy->GetHealth();
                        enemy->TakeDamage(damage);
                        float newHealth = enemy->GetHealth();

                     /*   Utils::printMsg("Player bullet " + std::to_string(bulletId) +
                            " (owner: " + std::to_string(ownerId) + ") hit enemy " +
                            std::to_string(enemyId) + " for " + std::to_string(damage) +
                            " damage (Health: " + std::to_string(oldHealth) + " → " +
                            std::to_string(newHealth) + ")", success);*/

                        // Award score if enemy died
                        if (enemy->IsDead() && oldHealth > 0.0f) {
                            auto ownerIt = clients.find(ownerId);
                            if (ownerIt != clients.end()) {
                                int scoreValue = enemy->GetScoreValue();
                                ownerIt->second.score += scoreValue;
                                ownerIt->second.playerData.score = ownerIt->second.score;

                                Utils::printMsg("Player " + std::to_string(ownerId) +
                                    " killed enemy " + std::to_string(enemyId) +
                                    "! +" + std::to_string(scoreValue) + " points | " +
                                    "Total: " + std::to_string(ownerIt->second.score), success);
                            }
                        }

                        bullet->Destroy();
                        BroadcastBulletDestruction(bulletId, 2, enemyId, bulletPos);
                        break;
                    }
                }

                if (bullet->IsDestroyed()) {
                    continue;
                }
            }

            // CHECK: Bullet vs Players 
      
            if (isEnemyBullet) {
                // : Log enemy bullet checking players
                static std::unordered_map<uint32_t, int> enemyBulletLogCounters;
                if (++enemyBulletLogCounters[bulletId] % 30 == 0) {
                    Utils::printMsg("  Enemy bullet " + std::to_string(bulletId) +
                        " checking " + std::to_string(clients.size()) + " players...", debug);
                }

                for (auto& [playerId, client] : clients) {
                    if (!client.isActive) {
                        continue;
                    }

                    // Log player positions
              /*      static std::unordered_map<uint32_t, int> playerPosLogCounters;*/
                    //if (++playerPosLogCounters[playerId] % 60 == 0) {
                    //    Utils::printMsg("    Player " + std::to_string(playerId) +
                    //        " at (" + std::to_string(client.playerData.x) + ", " +
                    //        std::to_string(client.playerData.y) + ")", debug);
                    //}

                    float dx = bulletPos.x - client.playerData.x;
                    float dy = bulletPos.y - client.playerData.y;
                    float distSq = dx * dx + dy * dy;
                    float radiusSum = bulletRadius + WorldConstants::TANK_RADIUS;
                    float distance = std::sqrt(distSq);

                    //  Log close calls
                    //if (distance < radiusSum * 2.0f) {  // Within 2x collision distance
                    //    Utils::printMsg("CLOSE: Bullet " + std::to_string(bulletId) +
                    //        " vs Player " + std::to_string(playerId) +
                    //        " dist=" + std::to_string(distance) +
                    //        " (need <" + std::to_string(radiusSum) + ")", warning);
                    //}

                    if (distSq < radiusSum * radiusSum) {
                        float damage = bullet->GetDamage();
                        float oldHealth = client.playerData.health;
                        client.playerData.health -= damage;

                        if (client.playerData.health < 0.0f) {
                            client.playerData.health = 0.0f;
                        }

                        Utils::printMsg("HIT CONFIRMED Enemy bullet " + std::to_string(bulletId) +
                            " (owner: " + std::to_string(ownerId) + ") hit player " +
                            std::to_string(playerId) + " for " + std::to_string(damage) +
                            " damage | Health: " + std::to_string(oldHealth) + " → " +
                            std::to_string(client.playerData.health), error);  // Use ERROR to make it stand out

                        bullet->Destroy();
                        BroadcastBulletDestruction(bulletId, 1, playerId, bulletPos);
                        break;
                    }
                }

                if (bullet->IsDestroyed()) {
                    continue;
                }
            }

            // CHECK: Bullet vs World Boundaries

            sf::FloatRect worldBounds(
                sf::Vector2f(WorldConstants::PLAYABLE_MIN_X, WorldConstants::PLAYABLE_MIN_Y),
                sf::Vector2f(WorldConstants::PLAYABLE_WIDTH, WorldConstants::PLAYABLE_HEIGHT)
            );

            if (bulletPos.x < worldBounds.position.x ||
                bulletPos.x > worldBounds.position.x + worldBounds.size.x ||
                bulletPos.y < worldBounds.position.y ||
                bulletPos.y > worldBounds.position.y + worldBounds.size.y) {

                Utils::printMsg("Bullet " + std::to_string(bulletId) + " hit border", debug);
                bullet->Destroy();
                BroadcastBulletDestruction(bulletId, 3, 0, bulletPos);
            }
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in CheckBulletCollisions: " + std::string(e.what()), error);
    }
}
void GameServer::RemoveDeadBullets() {
    auto it = bullets.begin();
    while (it != bullets.end()) {
        if (it->second && it->second->IsExpired()) {
            uint32_t bulletId = it->first;
            sf::Vector2f pos = it->second->GetPosition();

            // Only broadcast destruction if bullet expired naturally (lifetime)
            // Don't broadcast if bullet was already destroyed by collision
            // (collision already sent a destruction message)

            // Check if bullet was destroyed by collision (not just expired)
            // If destroyed but still has lifetime > 0, it was destroyed by collision
            bool destroyedByCollision = it->second->IsDestroyed();

            if (!destroyedByCollision) {
                // Bullet expired naturally (ran out of lifetime) - broadcast
                BroadcastBulletDestruction(bulletId, 0, 0, pos);
                //Utils::printMsg("Bullet " + std::to_string(bulletId) +
                //    " expired naturally", debug);
            }
            // else: Already destroyed by collision, destruction already broadcasted

            it = bullets.erase(it);
        }
        else {
            ++it;
        }
    }
}

BulletData GameServer::BulletToBulletData(const Bullet& bullet, uint32_t bulletId) const {
    BulletData data;
    data.bulletId = bulletId;
    data.ownerId = bullet.GetOwnerId();
    data.bulletType = static_cast<uint8_t>(bullet.GetBulletType());
    data.x = bullet.GetPosition().x;
    data.y = bullet.GetPosition().y;
    data.velocityX = bullet.GetVelocity().x;
    data.velocityY = bullet.GetVelocity().y;
    data.rotation = bullet.rotation;
    data.damage = bullet.GetDamage();
    data.lifetime = 0;
    data.spawnTime = GetCurrentTimestamp();

    return data;
}

bool GameServer::ValidateBulletSpawnRequest(const BulletSpawnMessage& msg, uint32_t playerId) const {
    if (!NetworkValidation::IsValidPosition(msg.spawnX, msg.spawnY)) {
        return false;
    }

    float dirLength = std::sqrt(msg.directionX * msg.directionX +
        msg.directionY * msg.directionY);
    if (dirLength < 0.001f || dirLength > 2.0f) {
        return false;
    }

    int64_t currentTime = GetCurrentTimestamp();
    if (!NetworkValidation::IsValidTimestamp(msg.timestamp, currentTime)) {
        return false;
    }

    return true;
}

void GameServer::CheckServerSideCollisions(float deltaTime) {
    try {
        const float TANK_RADIUS = WorldConstants::TANK_RADIUS;
        const float ENEMY_RADIUS = WorldConstants::ENEMY_TANK_RADIUS;
        const float SEPARATION_SPEED = 200.0f;
        const float MIN_SEPARATION = 2.0f;

        for (auto& [playerId, client] : clients) {
            if (!client.isActive) continue;

            sf::Vector2f playerPos(client.playerData.x, client.playerData.y);

            for (const auto& [enemyId, enemy] : enemies) {
                if (!enemy || enemy->IsDead()) continue;

                sf::Vector2f enemyPos = enemy->GetPosition();

                float dx = playerPos.x - enemyPos.x;
                float dy = playerPos.y - enemyPos.y;
                float distSq = dx * dx + dy * dy;
                float minDist = TANK_RADIUS + ENEMY_RADIUS + MIN_SEPARATION;
                float minDistSq = minDist * minDist;

                if (distSq < minDistSq) {
                    float currentDist = std::sqrt(distSq);

                    if (currentDist < 0.001f) {
                        playerPos.x += minDist;
                    }
                    else {
                        float overlap = minDist - currentDist;
                        float maxSeparation = SEPARATION_SPEED * deltaTime;
                        float separationAmount = std::min(overlap, maxSeparation);

                        sf::Vector2f pushDir(dx / currentDist, dy / currentDist);

                        playerPos = playerPos + pushDir * separationAmount;
                    }

                    client.playerData.x = playerPos.x;
                    client.playerData.y = playerPos.y;

                    client.playerData.x = std::max(WorldConstants::MOVEMENT_MIN_X,
                        std::min(WorldConstants::MOVEMENT_MAX_X, client.playerData.x));
                    client.playerData.y = std::max(WorldConstants::MOVEMENT_MIN_Y,
                        std::min(WorldConstants::MOVEMENT_MAX_Y, client.playerData.y));
                }
            }
        }

        std::vector<uint32_t> playerIds;
        for (const auto& [id, client] : clients) {
            if (client.isActive) {
                playerIds.push_back(id);
            }
        }

        for (size_t i = 0; i < playerIds.size(); ++i) {
            for (size_t j = i + 1; j < playerIds.size(); ++j) {
                uint32_t id1 = playerIds[i];
                uint32_t id2 = playerIds[j];

                auto& client1 = clients[id1];
                auto& client2 = clients[id2];

                sf::Vector2f pos1(client1.playerData.x, client1.playerData.y);
                sf::Vector2f pos2(client2.playerData.x, client2.playerData.y);

                float dx = pos2.x - pos1.x;
                float dy = pos2.y - pos1.y;
                float distSq = dx * dx + dy * dy;
                float minDist = TANK_RADIUS * 2.0f + MIN_SEPARATION;
                float minDistSq = minDist * minDist;

                if (distSq < minDistSq) {
                    float currentDist = std::sqrt(distSq);

                    if (currentDist < 0.001f) {
                        client1.playerData.x -= minDist / 2.0f;
                        client2.playerData.x += minDist / 2.0f;
                    }
                    else {
                        float overlap = minDist - currentDist;
                        float maxSeparation = (SEPARATION_SPEED * deltaTime) / 2.0f;
                        float separationAmount = std::min(overlap / 2.0f, maxSeparation);

                        sf::Vector2f pushDir(dx / currentDist, dy / currentDist);

                        client1.playerData.x -= pushDir.x * separationAmount;
                        client1.playerData.y -= pushDir.y * separationAmount;

                        client2.playerData.x += pushDir.x * separationAmount;
                        client2.playerData.y += pushDir.y * separationAmount;
                    }

                    client1.playerData.x = std::max(WorldConstants::MOVEMENT_MIN_X,
                        std::min(WorldConstants::MOVEMENT_MAX_X, client1.playerData.x));
                    client1.playerData.y = std::max(WorldConstants::MOVEMENT_MIN_Y,
                        std::min(WorldConstants::MOVEMENT_MAX_Y, client1.playerData.y));

                    client2.playerData.x = std::max(WorldConstants::MOVEMENT_MIN_X,
                        std::min(WorldConstants::MOVEMENT_MAX_X, client2.playerData.x));
                    client2.playerData.y = std::max(WorldConstants::MOVEMENT_MIN_Y,
                        std::min(WorldConstants::MOVEMENT_MAX_Y, client2.playerData.y));
                }
            }
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in CheckServerSideCollisions: " + std::string(e.what()), error);
    }
}
// DEATH AND RESPAWN SYSTEM IMPLEMENTATION

/**
 * Check all players for death condition (health <= 0)
 * Triggered every frame after collision detection
 */
void GameServer::CheckPlayerDeaths() {
    try {
        for (auto& [playerId, client] : clients) {
            if (!client.isActive || client.isDead) {
                continue; // Skip inactive or already dead players
            }

            // Check if player's health dropped to zero or below
            if (client.playerData.health <= 0.0f) {
                // Determine killer (0 = killed by enemy)
                uint32_t killerId = 0; 

                Utils::printMsg("PLAYER DEATH DETECTED: Player " +
                    std::to_string(playerId) + " (" + client.playerData.playerName +
                    ") died at (" + std::to_string(client.playerData.x) + ", " +
                    std::to_string(client.playerData.y) + ")", error);

                HandlePlayerDeath(playerId, killerId);
            }
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in CheckPlayerDeaths: " + std::string(e.what()), error);
    }
}

/**
 * Handle player death - apply penalties, set death state, broadcast death message
 * @param playerId The player who died
 * @param killerId The player/enemy who killed them (0 = killed by enemy)
 */
void GameServer::HandlePlayerDeath(uint32_t playerId, uint32_t killerId) {
    auto it = clients.find(playerId);
    if (it == clients.end() || !it->second.isActive) {
        return;
    }

    ClientInfo& client = it->second;

    // Mark player as dead
    client.isDead = true;
    client.playerData.isDead = true;
    client.deathTimer = ClientInfo::RESPAWN_COOLDOWN; // 5 seconds

    // Store death position for broadcast
    sf::Vector2f deathPos(client.playerData.x, client.playerData.y);

    // Apply score penalty (minimum 0)
    int32_t oldScore = client.score;
    client.score -= ClientInfo::DEATH_PENALTY;
    if (client.score < 0) {
        client.score = 0;
    }
    client.playerData.score = client.score;

    int32_t actualPenalty = oldScore - client.score;

    Utils::printMsg("DEATH PENALTY: Player " + std::to_string(playerId) +
        " lost " + std::to_string(actualPenalty) + " points | " +
        "Score: " + std::to_string(oldScore) + " → " + std::to_string(client.score),
        warning);

    // Set health to 0 (in case it went negative)
    client.playerData.health = 0.0f;

    // Broadcast death to all clients
    BroadcastPlayerDeath(playerId, killerId, deathPos, actualPenalty);

    Utils::printMsg("Player " + std::to_string(playerId) +
        " will respawn in " + std::to_string(ClientInfo::RESPAWN_COOLDOWN) + " seconds",
        info);
}

/**
 * Update all dead players' respawn timers and respawn them when ready
 * @param deltaTime Time elapsed since last frame
 */
void GameServer::UpdateDeadPlayers(float deltaTime) {
    try {
        for (auto& [playerId, client] : clients) {
            if (!client.isActive || !client.isDead) {
                continue; // Only process active, dead players
            }

            // Countdown respawn timer
            client.deathTimer -= deltaTime;

            // Log countdown periodically (every second)
            static std::unordered_map<uint32_t, float> logTimers;
            logTimers[playerId] += deltaTime;
            if (logTimers[playerId] >= 1.0f && client.deathTimer > 0) {
                Utils::printMsg("Player " + std::to_string(playerId) +
                    " respawns in " + std::to_string((int)std::ceil(client.deathTimer)) +
                    " seconds", debug);
                logTimers[playerId] = 0.0f;
            }

            // Check if respawn timer expired
            if (client.deathTimer <= 0.0f) {
                RespawnPlayer(playerId);
                logTimers.erase(playerId); // Clear log timer
            }
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in UpdateDeadPlayers: " + std::string(e.what()), error);
    }
}

/**
 * Respawn a player at a random position with full health
 * @param playerId The player to respawn
 */
void GameServer::RespawnPlayer(uint32_t playerId) {
    auto it = clients.find(playerId);
    if (it == clients.end() || !it->second.isActive) {
        return;
    }

    ClientInfo& client = it->second;

    // Get random spawn position
    sf::Vector2f spawnPos = GetRandomRespawnPosition();

    // Reset player state
    client.isDead = false;
    client.playerData.isDead = false;
    client.deathTimer = 0.0f;
    client.playerData.health = client.playerData.maxHealth; // Full health
    client.playerData.x = spawnPos.x;
    client.playerData.y = spawnPos.y;
    client.playerData.bodyRotation = 0.0f;
    client.playerData.barrelRotation = 0.0f;

    // Clear movement flags
    client.playerData.isMoving_forward = false;
    client.playerData.isMoving_backward = false;
    client.playerData.isMoving_left = false;
    client.playerData.isMoving_right = false;

    Utils::printMsg("RESPAWN: Player " + std::to_string(playerId) + " (" +
        client.playerData.playerName + ") respawned at (" +
        std::to_string(spawnPos.x) + ", " + std::to_string(spawnPos.y) + ") | " +
        "Score: " + std::to_string(client.score) + " | Health: " +
        std::to_string(client.playerData.health), success);

    // Broadcast respawn to all clients
    BroadcastPlayerRespawn(playerId, spawnPos, client.playerData.health);
}

/**
 * Broadcast player death message to all clients
 * @param playerId The player who died
 * @param killerId The killer (0 = enemy)
 * @param deathPos Where the player died
 * @param scorePenalty How much score was lost
 */
void GameServer::BroadcastPlayerDeath(uint32_t playerId, uint32_t killerId,
    sf::Vector2f deathPos, int32_t scorePenalty) {
    if (clients.empty()) {
        return;
    }

    try {
        sf::Packet packet;
        PlayerDeathMessage deathMsg;
        deathMsg.type = NetMessageType::PLAYER_DEATH;
        deathMsg.playerId = playerId;
        deathMsg.killerId = killerId;
        deathMsg.deathX = deathPos.x;
        deathMsg.deathY = deathPos.y;
        deathMsg.scorePenalty = scorePenalty;
        deathMsg.timestamp = GetCurrentTimestamp();
        deathMsg.sequenceNumber = outgoingSequenceNumber++;

        // Serialize death message
        packet << static_cast<uint8_t>(deathMsg.type)
            << deathMsg.playerId
            << deathMsg.killerId
            << deathMsg.deathX
            << deathMsg.deathY
            << deathMsg.scorePenalty
            << deathMsg.timestamp
            << deathMsg.sequenceNumber;

        // Send to all active clients
        for (const auto& [id, client] : clients) {
            if (client.isActive) {
                socket.send(packet, client.address, client.port);
            }
        }

        Utils::printMsg("Broadcasted death message for player " + std::to_string(playerId), debug);
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in BroadcastPlayerDeath: " + std::string(e.what()), error);
    }
}

/**
 * Broadcast player respawn message to all clients
 * @param playerId The player who respawned
 * @param spawnPos Where they respawned
 * @param health Their respawn health (should be maxHealth)
 */
void GameServer::BroadcastPlayerRespawn(uint32_t playerId, sf::Vector2f spawnPos, float health) {
    if (clients.empty()) {
        return;
    }

    try {
        sf::Packet packet;
        PlayerRespawnMessage respawnMsg;
        respawnMsg.type = NetMessageType::PLAYER_RESPAWN;
        respawnMsg.playerId = playerId;
        respawnMsg.spawnX = spawnPos.x;
        respawnMsg.spawnY = spawnPos.y;
        respawnMsg.health = health;
        respawnMsg.timestamp = GetCurrentTimestamp();
        respawnMsg.sequenceNumber = outgoingSequenceNumber++;

        // Serialize respawn message
        packet << static_cast<uint8_t>(respawnMsg.type)
            << respawnMsg.playerId
            << respawnMsg.spawnX
            << respawnMsg.spawnY
            << respawnMsg.health
            << respawnMsg.timestamp
            << respawnMsg.sequenceNumber;

        // Send to all active clients
        for (const auto& [id, client] : clients) {
            if (client.isActive) {
                socket.send(packet, client.address, client.port);
            }
        }

        Utils::printMsg("Broadcasted respawn message for player " + std::to_string(playerId), debug);
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in BroadcastPlayerRespawn: " + std::string(e.what()), error);
    }
}

/**
 * Get a random respawn position that's safe (not near enemies or other players)
 * @return Random spawn position within world bounds
 */
sf::Vector2f GameServer::GetRandomRespawnPosition() {
    // Use the same spawn logic as GetRandomSpawnPosition() but with safety checks
    std::uniform_real_distribution<float> distX(
        WorldConstants::PLAYABLE_MIN_X + WorldConstants::TANK_RADIUS + 50.0f,
        WorldConstants::PLAYABLE_MAX_X - WorldConstants::TANK_RADIUS - 50.0f
    );

    std::uniform_real_distribution<float> distY(
        WorldConstants::PLAYABLE_MIN_Y + WorldConstants::TANK_RADIUS + 50.0f,
        WorldConstants::PLAYABLE_MAX_Y - WorldConstants::TANK_RADIUS - 50.0f
    );

    // Try to find a safe spawn position (max 10 attempts)
    const int MAX_ATTEMPTS = 10;
    const float MIN_SAFE_DISTANCE = 200.0f; // Minimum distance from enemies/players

    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        sf::Vector2f candidatePos(distX(randomGenerator), distY(randomGenerator));
        bool isSafe = true;

        // Check distance from all enemies
        for (const auto& [enemyId, enemy] : enemies) {
            if (!enemy || enemy->IsDead()) continue;

            sf::Vector2f enemyPos = enemy->GetPosition();
            float dx = candidatePos.x - enemyPos.x;
            float dy = candidatePos.y - enemyPos.y;
            float distSq = dx * dx + dy * dy;

            if (distSq < MIN_SAFE_DISTANCE * MIN_SAFE_DISTANCE) {
                isSafe = false;
                break;
            }
        }

        if (!isSafe) continue;

        // Check distance from all active players
        for (const auto& [playerId, client] : clients) {
            if (!client.isActive || client.isDead) continue;

            float dx = candidatePos.x - client.playerData.x;
            float dy = candidatePos.y - client.playerData.y;
            float distSq = dx * dx + dy * dy;

            if (distSq < MIN_SAFE_DISTANCE * MIN_SAFE_DISTANCE) {
                isSafe = false;
                break;
            }
        }

        if (isSafe) {
            Utils::printMsg("Found safe respawn position at (" +
                std::to_string(candidatePos.x) + ", " +
                std::to_string(candidatePos.y) + ") on attempt " +
                std::to_string(attempt + 1), debug);
            return candidatePos;
        }
    }

    // If no safe position found, return center position
    Utils::printMsg(" Could not find safe respawn position, using center", warning);
    return sf::Vector2f(WorldConstants::CENTER_X, WorldConstants::CENTER_Y);
}