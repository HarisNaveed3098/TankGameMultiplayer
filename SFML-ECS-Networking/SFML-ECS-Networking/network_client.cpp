#include "network_client.h"
#include "utils.h"
#include <algorithm>
#include "network_validation.h"
#include "client_prediction.h"
#include <cmath>

NetworkClient::NetworkClient()
    : isConnected(false), localPlayerId(0), updateRate(0.0167f), updateTimer(0),
    serverAddress(sf::IpAddress::LocalHost), outgoingSequenceNumber(0),
    lastReceivedSequenceNumber(0), pingTimer(0), pingInterval(1.0f),
    consecutiveErrors(0), maxConsecutiveErrors(5),
    prediction(std::make_unique<ClientPrediction>()),
    lastServerAckedSequence(0),
    predictionEnabled(true),
    serverAuthoritativePosition(0.0f, 0.0f),
    serverAuthoritativeBodyRotation(0.0f),
    serverAuthoritativeBarrelRotation(0.0f),
    serverAuthoritativeHealth(100.0f),
    serverAuthoritativeMaxHealth(100.0f),
    serverAuthoritativeScore(0),
    serverAuthoritativeIsDead(false),
    hasServerAuthoritativeState(false),
    reconciliationTargetPosition(0.0f, 0.0f),
    reconciliationTargetRotation(0.0f),
    isReconciling(false),
    lastInputAckTime(0),
    lastServerTimestamp(0),
    lastAcknowledgedInputSeq(0) {
}

NetworkClient::~NetworkClient() {
    Disconnect();
}

void NetworkClient::ApplyLocalInputWithPrediction(Tank& localPlayer, float deltaTime, sf::Vector2f mousePos) {
    if (!predictionEnabled || !isConnected || localPlayerId == 0) {
        return;
    }

    // Store mouse position for potential replay during reconciliation
    lastMousePosition = mousePos;

    // Create input state from current player input
    InputState input;
    input.timestamp = GetCurrentTimestamp();
    input.moveForward = localPlayer.isMoving.forward;
    input.moveBackward = localPlayer.isMoving.backward;
    input.turnLeft = localPlayer.isMoving.left;
    input.turnRight = localPlayer.isMoving.right;
    input.deltaTime = deltaTime;

    // Store input and get sequence number
    uint32_t sequenceNumber = prediction->StoreInput(input);

    // Apply input IMMEDIATELY with mouse position for barrel (CLIENT-SIDE PREDICTION)
    ApplyInputToTank(localPlayer, input, mousePos);

    // Calculate barrel rotation from mouse position
    float deltaX = mousePos.x - localPlayer.position.x;
    float deltaY = mousePos.y - localPlayer.position.y;
    float barrelRotationDegrees = std::atan2(deltaY, deltaX) * 180.0f / 3.14159f;
    static int logCounter1 = 0;
    if (++logCounter1 % 30 == 0) {  // Log every 30 frames (~0.5 seconds)
        Utils::printMsg(" CLIENT SENDING: Barrel=" + std::to_string(barrelRotationDegrees) + "°", debug);
    }
    // Store predicted state (including barrel rotation)
    PredictedState predicted(
        sequenceNumber,
        input.timestamp,
        localPlayer.position,
        localPlayer.bodyRotation,
        sf::degrees(barrelRotationDegrees)
    );
    prediction->StorePredictedState(predicted);

    // Send input to server with barrel rotation
    SendInputWithSequence(sequenceNumber, input, barrelRotationDegrees);

    // Clean up old history periodically
    static uint32_t lastCleanupSequence = 0;
    if (sequenceNumber - lastCleanupSequence > 30) {
        prediction->CleanupOldHistory(lastAcknowledgedInputSeq);
        lastCleanupSequence = sequenceNumber;
    }
}

void NetworkClient::ApplyInputToTank(Tank& tank, const InputState& input, sf::Vector2f mousePos) {
    // Apply rotation (body only - A/D keys control body rotation)
    if (input.turnLeft) {
        tank.bodyRotation -= sf::degrees(ROTATION_SPEED * input.deltaTime);
    }
    else if (input.turnRight) {
        tank.bodyRotation += sf::degrees(ROTATION_SPEED * input.deltaTime);
    }

    //  Normalize rotation to 0-360 degrees EXACTLY like server
    // This ensures client prediction matches server simulation
    float bodyRotDegrees = tank.bodyRotation.asDegrees();
    while (bodyRotDegrees < 0.0f) {
        bodyRotDegrees += 360.0f;
    }
    while (bodyRotDegrees >= 360.0f) {
        bodyRotDegrees -= 360.0f;
    }
    tank.bodyRotation = sf::degrees(bodyRotDegrees);

    // Calculate movement direction from BODY rotation (not barrel!)
    // Tank moves in the direction the body is facing
    float radians = bodyRotDegrees * 3.14159f / 180.0f;
    sf::Vector2f direction(std::cos(radians), std::sin(radians));

    // Apply position change based on W/S keys
    if (input.moveForward) {
        tank.position += direction * MOVEMENT_SPEED * input.deltaTime;
    }
    else if (input.moveBackward) {
        tank.position -= direction * MOVEMENT_SPEED * input.deltaTime;
    }

    //  Calculate barrel rotation from mouse position (independent of body)
    // Barrel aims at mouse cursor, not in the direction of movement
    float deltaX = mousePos.x - tank.position.x;
    float deltaY = mousePos.y - tank.position.y;

    // Validate mouse position is finite (safety check)
    if (std::isfinite(deltaX) && std::isfinite(deltaY)) {
        // Calculate angle from tank to mouse using atan2
        // atan2(y, x) returns angle in radians, convert to degrees
        float barrelAngle = std::atan2(deltaY, deltaX) * 180.0f / 3.14159f;
        tank.barrelRotation = sf::degrees(barrelAngle);
    }
    else {
        // Fallback: if mouse position is invalid, keep current barrel rotation
        // This prevents NaN or Inf values from breaking the game
    }

    // Update sprites to reflect new position and rotations
    tank.UpdateSprites();
}

void NetworkClient::ApplyServerReconciliation(Tank& localPlayer) {
    if (!predictionEnabled || !isConnected || localPlayerId == 0) {
        // If we're already reconciling, continue smooth interpolation
        if (isReconciling) {
            float lerpFactor = RECONCILIATION_RATE * 0.016f;

            sf::Vector2f currentPos = localPlayer.position;
            sf::Vector2f targetPos = reconciliationTargetPosition;
            localPlayer.position.x = currentPos.x + (targetPos.x - currentPos.x) * lerpFactor;
            localPlayer.position.y = currentPos.y + (targetPos.y - currentPos.y) * lerpFactor;

            float currentRot = localPlayer.bodyRotation.asDegrees();
            float targetRot = reconciliationTargetRotation;
            float rotDiff = targetRot - currentRot;
            if (rotDiff > 180.0f) rotDiff -= 360.0f;
            if (rotDiff < -180.0f) rotDiff += 360.0f;

            localPlayer.bodyRotation = sf::degrees(currentRot + rotDiff * lerpFactor);
            // NOTE: Don't sync barrel with body - barrel is mouse-driven
            // localPlayer.barrelRotation = localPlayer.bodyRotation; // OLD - REMOVED

            float distToTarget = std::sqrt(
                std::pow(targetPos.x - localPlayer.position.x, 2) +
                std::pow(targetPos.y - localPlayer.position.y, 2)
            );

            if (distToTarget < 2.0f) {
                isReconciling = false;
            }

            localPlayer.UpdateSprites();
        }
        return;
    }

    // Check if we have fresh server data
    if (!hasServerAuthoritativeState) {
        if (isReconciling) {
            // Continue smooth interpolation
            float lerpFactor = RECONCILIATION_RATE * 0.016f;

            sf::Vector2f currentPos = localPlayer.position;
            sf::Vector2f targetPos = reconciliationTargetPosition;
            localPlayer.position.x = currentPos.x + (targetPos.x - currentPos.x) * lerpFactor;
            localPlayer.position.y = currentPos.y + (targetPos.y - currentPos.y) * lerpFactor;

            float currentRot = localPlayer.bodyRotation.asDegrees();
            float targetRot = reconciliationTargetRotation;
            float rotDiff = targetRot - currentRot;
            if (rotDiff > 180.0f) rotDiff -= 360.0f;
            if (rotDiff < -180.0f) rotDiff += 360.0f;

            localPlayer.bodyRotation = sf::degrees(currentRot + rotDiff * lerpFactor);
            // NOTE: Don't sync barrel with body - barrel is mouse-driven
            // localPlayer.barrelRotation = localPlayer.bodyRotation; // OLD - REMOVED

            float distToTarget = std::sqrt(
                std::pow(targetPos.x - localPlayer.position.x, 2) +
                std::pow(targetPos.y - localPlayer.position.y, 2)
            );

            if (distToTarget < 2.0f) {
                isReconciling = false;
            }

            localPlayer.UpdateSprites();
        }
        return;
    }

    // We have new server data - analyze the error
    sf::Vector2f serverPos = serverAuthoritativePosition;
    float serverRot = serverAuthoritativeBodyRotation;

    float errorX = localPlayer.position.x - serverPos.x;
    float errorY = localPlayer.position.y - serverPos.y;
    float error = std::sqrt(errorX * errorX + errorY * errorY);

    // Four-tier reconciliation strategy:
    if (error < 5.0f) {
        // TIER 1: Tiny error - Ignore
    }
    else if (error < SMOOTH_CORRECTION_THRESHOLD) {
        // TIER 2: Small error - Smooth interpolation
        reconciliationTargetPosition = serverPos;
        reconciliationTargetRotation = serverRot;
        isReconciling = true;
    }
    else if (error < SNAP_CORRECTION_THRESHOLD) {
        // TIER 3: Medium error - Partial correction + smooth
        sf::Vector2f halfway = localPlayer.position + (serverPos - localPlayer.position) * 0.5f;
        localPlayer.position = halfway;

        reconciliationTargetPosition = serverPos;
        reconciliationTargetRotation = serverRot;
        isReconciling = true;

        localPlayer.bodyRotation = sf::degrees(serverRot);
        // NOTE: Don't sync barrel with body - barrel is mouse-driven
        // localPlayer.barrelRotation = localPlayer.bodyRotation; // OLD - REMOVED
        localPlayer.UpdateSprites();

        Utils::printMsg("Medium correction: " + std::to_string(error) + "px error", debug);

        // Mark unacknowledged inputs for replay
        prediction->MarkInputsForReplay(lastAcknowledgedInputSeq + 1);
    }
    else {
        // TIER 4: Large error - Snap immediately
        Utils::printMsg("SNAP correction: Error: " + std::to_string(error) + "px", warning);

        localPlayer.position = serverPos;
        localPlayer.bodyRotation = sf::degrees(serverRot);
        // NOTE: Don't sync barrel with body - barrel is mouse-driven
        // localPlayer.barrelRotation = localPlayer.bodyRotation; // OLD - REMOVED
        localPlayer.UpdateSprites();

        isReconciling = false;

        // Get current mouse position for replay
        // We need to get mouse position from MultiplayerGame
        // Since we don't have direct access, use a fallback approach:
        // Use the current barrel rotation as reference (already set by prediction)
        sf::Vector2f mousePos = localPlayer.position; // Fallback

        // Calculate approximate mouse position from current barrel rotation
        float barrelDeg = localPlayer.barrelRotation.asDegrees();
        float barrelRad = barrelDeg * 3.14159f / 180.0f;
        float mouseDistance = 100.0f; // Approximate distance
        mousePos.x = localPlayer.position.x + std::cos(barrelRad) * mouseDistance;
        mousePos.y = localPlayer.position.y + std::sin(barrelRad) * mouseDistance;

        // Replay all unacknowledged inputs after snap
        ReplayInputsAfterCorrection(localPlayer, lastAcknowledgedInputSeq + 1, mousePos);
    }

    // NOTE: Do NOT clear hasServerAuthoritativeState here!
    // The flag needs to remain true so that health updates can be processed
    // in multiplayer_game.cpp after this function returns.
    // The flag will be cleared there after health sync is complete.
}

void NetworkClient::SendInputWithSequence(uint32_t sequenceNumber, const InputState& input, float barrelRotation) {
    if (!isConnected) return;

    try {
        sf::Packet packet;

        PlayerInputMessage inputMsg;
        inputMsg.type = NetMessageType::PLAYER_INPUT;
        inputMsg.playerId = localPlayerId;
        inputMsg.isMoving_forward = input.moveForward;
        inputMsg.isMoving_backward = input.moveBackward;
        inputMsg.isMoving_left = input.turnLeft;
        inputMsg.isMoving_right = input.turnRight;
        inputMsg.timestamp = input.timestamp;
        inputMsg.sequenceNumber = sequenceNumber;
        inputMsg.barrelRotation = barrelRotation;  // Include barrel rotation

        // Debug log (optional - comment out after testing)
        // Utils::printMsg("Sending barrel: " + std::to_string(barrelRotation) + "°", debug);

        // Serialize message - CRITICAL: barrel must be included
        packet << static_cast<uint8_t>(inputMsg.type) << inputMsg.playerId
            << inputMsg.isMoving_forward << inputMsg.isMoving_backward
            << inputMsg.isMoving_left << inputMsg.isMoving_right
            << inputMsg.timestamp << inputMsg.sequenceNumber
            << inputMsg.barrelRotation;  // Serialize barrel rotation

        // Track sent packet for RTT calculation
        SentPacket sentPacket;
        sentPacket.sequenceNumber = sequenceNumber;
        sentPacket.sentTime = input.timestamp;
        sentPackets.push_back(sentPacket);

        // Limit history size
        if (sentPackets.size() > MAX_SENT_PACKETS_HISTORY) {
            sentPackets.pop_front();
        }

        networkStats.totalPacketsSent++;

        // Send packet with error handling
        sf::Socket::Status sendStatus = socket.send(packet, serverAddress, serverPort);

        if (sendStatus == sf::Socket::Status::Done) {
            consecutiveErrors = 0;
        }
        else if (sendStatus != sf::Socket::Status::NotReady) {
            Utils::printMsg("Failed to send input - Status: " +
                SocketStatusToString(sendStatus), warning);
            consecutiveErrors++;

            if (consecutiveErrors >= maxConsecutiveErrors) {
                isConnected = false;
            }
        }

    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in SendInputWithSequence: " + std::string(e.what()), error);
        consecutiveErrors++;
    }
}

bool NetworkClient::Connect(const std::string& serverIP, unsigned short serverPort,
    const std::string& playerName, const std::string& preferredColor) {

    Utils::printMsg("Attempting to connect to server " + serverIP + ":" + std::to_string(serverPort));

    try {
        // Parse server IP - use resolve for string to IpAddress conversion
        auto resolvedIP = sf::IpAddress::resolve(serverIP);
        if (!resolvedIP) {
            Utils::printMsg("Failed to resolve server IP: " + serverIP, error);
            CleanupSocketResources();
            return false;
        }
        serverAddress = resolvedIP.value();

        this->serverPort = serverPort;

        // Bind client socket to any available port with error handling
        sf::Socket::Status bindStatus = socket.bind(sf::Socket::AnyPort);
        if (bindStatus != sf::Socket::Status::Done) {
            Utils::printMsg("Failed to bind client socket - Status: " +
                SocketStatusToString(bindStatus), error);
            CleanupSocketResources();
            return false;
        }

        // Set socket to non-blocking mode
        socket.setBlocking(false);

        Utils::printMsg("Client socket bound to port " + std::to_string(socket.getLocalPort()));

        // Reset network statistics
        networkStats.Reset();
        outgoingSequenceNumber = 0;
        lastReceivedSequenceNumber = 0;
        sentPackets.clear();
        rttHistory.clear();
        receivedSequenceNumbers.clear();
        consecutiveErrors = 0;

        // Send join request to server with retry logic
        const int MAX_JOIN_ATTEMPTS = 3;
        bool joinSuccess = false;

        for (int attempt = 1; attempt <= MAX_JOIN_ATTEMPTS && !joinSuccess; ++attempt) {
            if (attempt > 1) {
                Utils::printMsg("Join request attempt " + std::to_string(attempt) +
                    " of " + std::to_string(MAX_JOIN_ATTEMPTS), warning);
            }

            joinSuccess = SendJoinRequest(playerName, preferredColor);

            if (!joinSuccess && attempt < MAX_JOIN_ATTEMPTS) {
                sf::sleep(sf::milliseconds(500)); // Wait before retry
            }
        }

        if (!joinSuccess) {
            Utils::printMsg("Failed to send join request after " +
                std::to_string(MAX_JOIN_ATTEMPTS) + " attempts", error);
            CleanupSocketResources();
            return false;
        }

        isConnected = true;
        Utils::printMsg("Connected to server successfully", success);

        return true;
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception during connection: " + std::string(e.what()), error);
        CleanupSocketResources();
        return false;
    }
    catch (...) {
        Utils::printMsg("Unknown exception during connection", error);
        CleanupSocketResources();
        return false;
    }
}

void NetworkClient::Disconnect() {
    try {
        if (isConnected) {
            Utils::printMsg("Disconnecting from server...", warning);

            // Clean up socket resources
            CleanupSocketResources();

            isConnected = false;
            localPlayerId = 0;
            otherPlayers.clear();
            sentPackets.clear();
            rttHistory.clear();
            receivedSequenceNumbers.clear();
            bulletData.clear();

            consecutiveErrors = 0;

            Utils::printMsg("Disconnected from server", success);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception during disconnect: " + std::string(e.what()), error);
        // Force cleanup even if exception occurred
        isConnected = false;
    }
    catch (...) {
        Utils::printMsg("Unknown exception during disconnect", error);
        isConnected = false;
    }
}

void NetworkClient::Update(float deltaTime) {
    if (!isConnected) return;

    try {
        // Process incoming messages from server
        ProcessIncomingMessages();

        // Update timers
        updateTimer += deltaTime;
        pingTimer += deltaTime;

        // NEW: Process input buffer (update timers, cleanup timeouts)
        ProcessInputBuffer(deltaTime);

        // Send periodic ping for RTT measurement
        if (pingTimer >= pingInterval) {
            SendPing();
            pingTimer = 0;
        }

        // Check for connection health
        if (consecutiveErrors >= maxConsecutiveErrors) {
            Utils::printMsg("Too many consecutive errors (" +
                std::to_string(consecutiveErrors) + "), connection may be unstable", error);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in Update: " + std::string(e.what()), error);
        consecutiveErrors++;
    }
    catch (...) {
        Utils::printMsg("Unknown exception in Update", error);
        consecutiveErrors++;
    }
}

void NetworkClient::SendPlayerUpdate(const Tank& localPlayer) {
    if (!isConnected || localPlayerId == 0) return;

    // Only send updates at the specified rate
    if (updateTimer < updateRate) return;

    try {
        sf::Packet packet;

        // Create update message with timestamp and sequence number
        PlayerUpdateMessage updateMsg;
        updateMsg.type = NetMessageType::PLAYER_UPDATE;
        updateMsg.playerId = localPlayerId;

        // Validate data before sending
        updateMsg.x = NetworkValidation::ClampPositionX(localPlayer.position.x);
        updateMsg.y = NetworkValidation::ClampPositionY(localPlayer.position.y);
        updateMsg.bodyRotation = NetworkValidation::NormalizeRotation(localPlayer.bodyRotation.asDegrees());
        updateMsg.barrelRotation = NetworkValidation::NormalizeRotation(localPlayer.barrelRotation.asDegrees());

        updateMsg.isMoving_forward = localPlayer.isMoving.forward;
        updateMsg.isMoving_backward = localPlayer.isMoving.backward;
        updateMsg.isMoving_left = localPlayer.isMoving.left;
        updateMsg.isMoving_right = localPlayer.isMoving.right;
        updateMsg.timestamp = GetCurrentTimestamp();
        updateMsg.sequenceNumber = outgoingSequenceNumber++;

        // Manual serialization to maintain compatibility
        packet << static_cast<uint8_t>(updateMsg.type) << updateMsg.playerId
            << updateMsg.x << updateMsg.y
            << updateMsg.bodyRotation << updateMsg.barrelRotation
            << updateMsg.isMoving_forward << updateMsg.isMoving_backward
            << updateMsg.isMoving_left << updateMsg.isMoving_right
            << updateMsg.timestamp << updateMsg.sequenceNumber;

        // Track sent packet for potential RTT calculation
        SentPacket sentPacket;
        sentPacket.sequenceNumber = updateMsg.sequenceNumber;
        sentPacket.sentTime = updateMsg.timestamp;
        sentPackets.push_back(sentPacket);

        // Limit history size
        if (sentPackets.size() > MAX_SENT_PACKETS_HISTORY) {
            sentPackets.pop_front();
        }

        networkStats.totalPacketsSent++;

        // Send with error handling
        sf::Socket::Status sendStatus = socket.send(packet, serverAddress, serverPort);

        if (sendStatus == sf::Socket::Status::Done) {
            consecutiveErrors = 0; // Reset error counter on success
        }
        else if (sendStatus == sf::Socket::Status::NotReady) {
            Utils::printMsg("Socket not ready for sending player update", debug);
        }
        else {
            Utils::printMsg("Failed to send player update - Status: " +
                SocketStatusToString(sendStatus), warning);
            consecutiveErrors++;

            if (consecutiveErrors >= maxConsecutiveErrors) {
                Utils::printMsg("Max consecutive errors reached, connection may be lost", error);
                isConnected = false;
            }
        }

        updateTimer = 0; // Reset timer
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in SendPlayerUpdate: " + std::string(e.what()), error);
        consecutiveErrors++;
    }
    catch (...) {
        Utils::printMsg("Unknown exception in SendPlayerUpdate", error);
        consecutiveErrors++;
    }
}

void NetworkClient::SendPlayerInput(const Tank& localPlayer) {
    if (!isConnected || localPlayerId == 0) return;

    // Only send at the specified rate
    if (updateTimer < updateRate) return;

    try {
        sf::Packet packet;

        // Create lightweight input message
        PlayerInputMessage inputMsg;
        inputMsg.type = NetMessageType::PLAYER_INPUT;
        inputMsg.playerId = localPlayerId;
        inputMsg.isMoving_forward = localPlayer.isMoving.forward;
        inputMsg.isMoving_backward = localPlayer.isMoving.backward;
        inputMsg.isMoving_left = localPlayer.isMoving.left;
        inputMsg.isMoving_right = localPlayer.isMoving.right;
        inputMsg.timestamp = GetCurrentTimestamp();
        inputMsg.sequenceNumber = outgoingSequenceNumber++;

        // Serialize input message
        packet << static_cast<uint8_t>(inputMsg.type) << inputMsg.playerId
            << inputMsg.isMoving_forward << inputMsg.isMoving_backward
            << inputMsg.isMoving_left << inputMsg.isMoving_right
            << inputMsg.timestamp << inputMsg.sequenceNumber;

        // Track sent packet for RTT calculation
        SentPacket sentPacket;
        sentPacket.sequenceNumber = inputMsg.sequenceNumber;
        sentPacket.sentTime = inputMsg.timestamp;
        sentPackets.push_back(sentPacket);

        if (sentPackets.size() > MAX_SENT_PACKETS_HISTORY) {
            sentPackets.pop_front();
        }

        networkStats.totalPacketsSent++;

        // Send with error handling
        sf::Socket::Status sendStatus = socket.send(packet, serverAddress, serverPort);

        if (sendStatus == sf::Socket::Status::Done) {
            consecutiveErrors = 0;
        }
        else if (sendStatus == sf::Socket::Status::NotReady) {
            Utils::printMsg("Socket not ready for sending input", debug);
        }
        else {
            Utils::printMsg("Failed to send input - Status: " +
                SocketStatusToString(sendStatus), warning);
            consecutiveErrors++;

            if (consecutiveErrors >= maxConsecutiveErrors) {
                Utils::printMsg("Max consecutive errors reached, connection may be lost", error);
                isConnected = false;
            }
        }

        updateTimer = 0;
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in SendPlayerInput: " + std::string(e.what()), error);
        consecutiveErrors++;
    }
    catch (...) {
        Utils::printMsg("Unknown exception in SendPlayerInput", error);
        consecutiveErrors++;
    }
}

void NetworkClient::SendPing() {
    if (!isConnected) return;

    try {
        sf::Packet packet;
        PingMessage pingMsg;
        pingMsg.timestamp = GetCurrentTimestamp();
        pingMsg.sequenceNumber = outgoingSequenceNumber++;

        packet << static_cast<uint8_t>(pingMsg.type) << pingMsg.timestamp << pingMsg.sequenceNumber;

        // Track ping for RTT calculation
        SentPacket sentPacket;
        sentPacket.sequenceNumber = pingMsg.sequenceNumber;
        sentPacket.sentTime = pingMsg.timestamp;
        sentPackets.push_back(sentPacket);

        if (sentPackets.size() > MAX_SENT_PACKETS_HISTORY) {
            sentPackets.pop_front();
        }

        networkStats.totalPacketsSent++;

        // Send with error handling
        sf::Socket::Status sendStatus = socket.send(packet, serverAddress, serverPort);

        if (sendStatus == sf::Socket::Status::Done) {
            consecutiveErrors = 0;
        }
        else if (sendStatus == sf::Socket::Status::NotReady) {
            Utils::printMsg("Socket not ready for sending ping", debug);
        }
        else {
            Utils::printMsg("Failed to send ping - Status: " +
                SocketStatusToString(sendStatus), warning);
            consecutiveErrors++;
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in SendPing: " + std::string(e.what()), error);
        consecutiveErrors++;
    }
    catch (...) {
        Utils::printMsg("Unknown exception in SendPing", error);
        consecutiveErrors++;
    }
}

void NetworkClient::ProcessIncomingMessages() {
    try {
        sf::Packet packet;
        std::optional<sf::IpAddress> senderIP;
        unsigned short senderPort;

        // Process all pending messages with explicit NotReady handling
        int messagesProcessed = 0;
        const int MAX_MESSAGES_PER_FRAME = 100; // Prevent infinite loop

        while (messagesProcessed < MAX_MESSAGES_PER_FRAME) {
            sf::Socket::Status receiveStatus = socket.receive(packet, senderIP, senderPort);

            if (receiveStatus == sf::Socket::Status::Done) {
                // Successfully received a packet
                networkStats.totalPacketsReceived++;
                consecutiveErrors = 0; // Reset on successful receive

                // Validate sender
                if (!senderIP.has_value()) {
                    Utils::printMsg("Received packet from invalid sender", warning);
                    messagesProcessed++;
                    continue;
                }

                // Process the packet
                ProcessPacket(packet, senderIP.value(), senderPort);
                messagesProcessed++;
            }
            else if (receiveStatus == sf::Socket::Status::NotReady) {
                // No more data available - this is normal for non-blocking sockets
                break;
            }
            else if (receiveStatus == sf::Socket::Status::Disconnected) {
                // Server disconnected or unreachable
                Utils::printMsg("Server disconnected", error);
                isConnected = false;
                break;
            }
            else if (receiveStatus == sf::Socket::Status::Error) {
                Utils::printMsg("Socket error while receiving", error);
                consecutiveErrors++;

                // After multiple consecutive errors, consider connection lost
                if (consecutiveErrors >= maxConsecutiveErrors) {
                    Utils::printMsg("Too many consecutive errors, connection lost", error);
                    isConnected = false;
                }
                break;
            }
            else if (receiveStatus == sf::Socket::Status::Partial) {
                // Partial receive shouldn't happen with UDP
                Utils::printMsg("Partial packet received (unusual for UDP)", debug);
                break;
            }
            else {
                Utils::printMsg("Unknown socket status while receiving: " +
                    SocketStatusToString(receiveStatus), warning);
                consecutiveErrors++;
                break;
            }
        }

        if (messagesProcessed >= MAX_MESSAGES_PER_FRAME) {
            Utils::printMsg("Warning: Hit max messages per frame limit", warning);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in ProcessIncomingMessages: " + std::string(e.what()), error);
        consecutiveErrors++;
    }
    catch (...) {
        Utils::printMsg("Unknown exception in ProcessIncomingMessages", error);
        consecutiveErrors++;
    }
}

void NetworkClient::ProcessPacket(sf::Packet& packet, sf::IpAddress senderIP, unsigned short senderPort) {
    try {
        uint8_t messageTypeRaw;
        if (!(packet >> messageTypeRaw)) {
            Utils::printMsg("Failed to extract message type from packet", warning);
            consecutiveErrors++;
            return;
        }
        NetMessageType msgType = static_cast<NetMessageType>(messageTypeRaw);
        if (msgType == NetMessageType::GAME_STATE) {
            uint32_t playerCount;
            if (!(packet >> playerCount)) {
                Utils::printMsg("Failed to extract player count from game state", warning);
                consecutiveErrors++;
                return;
            }
            // Validate player count is reasonable
            if (!NetworkValidation::IsValidPlayerCount(playerCount)) {
                Utils::printMsg("Invalid player count received: " + std::to_string(playerCount), error);
                consecutiveErrors++;
                return;
            }
            // Clear current other players
            otherPlayers.clear();
            uint32_t successfullyParsed = 0;
            for (uint32_t i = 0; i < playerCount; ++i) {
                PlayerData player;
                // Extract basic data
                if (!(packet >> player.playerId >> player.playerName >> player.x >> player.y)) {
                    Utils::printMsg("Failed to extract player " + std::to_string(i) + " basic data", warning);
                    break;
                }
                // Validate player ID
                if (!NetworkValidation::IsValidPlayerId(player.playerId)) {
                    Utils::printMsg("Invalid player ID in game state: " + std::to_string(player.playerId), warning);
                    continue;
                }
                // Validate player name
                if (!NetworkValidation::IsValidPlayerName(player.playerName)) {
                    Utils::printMsg("Invalid player name in game state (length: " +
                        std::to_string(player.playerName.length()) + ")", warning);
                    player.playerName = "Player" + std::to_string(player.playerId);
                }
                // Validate position
                if (!NetworkValidation::IsValidPosition(player.x, player.y)) {
                    Utils::printMsg("Invalid position for player " + std::to_string(player.playerId) +
                        " (" + std::to_string(player.x) + ", " + std::to_string(player.y) + ")", warning);
                    player.x = NetworkValidation::ClampPositionX(player.x);
                    player.y = NetworkValidation::ClampPositionY(player.y);
                }
                // Extract rotations including barrel rotation
                if (!(packet >> player.bodyRotation >> player.barrelRotation)) {
                    Utils::printMsg("Failed to extract player rotations", warning);
                    break;
                }
                if (player.playerId != localPlayerId) { // Only log other players
                    static std::unordered_map<uint32_t, int> logCounters;
                    if (++logCounters[player.playerId] % 30 == 0) {
                    //    Utils::printMsg(" CLIENT RECV: Player=" + std::to_string(player.playerId) +
                     //       " Body=" + std::to_string(player.bodyRotation) + "° " +
                      //      "Barrel=" + std::to_string(player.barrelRotation) + "°", debug);
                    }
                }
                // Validate rotations
                if (!NetworkValidation::IsValidRotation(player.bodyRotation)) {
                    Utils::printMsg("Invalid body rotation for player " + std::to_string(player.playerId) +
                        ": " + std::to_string(player.bodyRotation), debug);
                    player.bodyRotation = NetworkValidation::NormalizeRotation(player.bodyRotation);
                }
                if (!NetworkValidation::IsValidRotation(player.barrelRotation)) {
                    Utils::printMsg("Invalid barrel rotation for player " + std::to_string(player.playerId) +
                        ": " + std::to_string(player.barrelRotation), debug);
                    player.barrelRotation = NetworkValidation::NormalizeRotation(player.barrelRotation);
                }
                // Extract remaining data
                if (!(packet >> player.color >> player.isMoving_forward)) {
                    Utils::printMsg("Failed to extract player movement data", warning);
                    break;
                }
                // Validate color
                if (!NetworkValidation::IsValidColor(player.color)) {
                    Utils::printMsg("Invalid color for player " + std::to_string(player.playerId), debug);
                    player.color = "green";
                }
                if (!(packet >> player.isMoving_backward >> player.isMoving_left >> player.isMoving_right)) {
                    Utils::printMsg("Failed to extract player directional data", warning);
                    break;
                }

                // CRITICAL: Extract health data (added for health bar system)
                if (!(packet >> player.health >> player.maxHealth)) {
                    Utils::printMsg("Failed to extract player health data", warning);
                    break;
                }

                // Extract score and death state
                if (!(packet >> player.score >> player.isDead)) {
                    Utils::printMsg("Failed to extract player score/death data", warning);
                    break;
                }

                // Process local player for reconciliation
                if (player.playerId == localPlayerId && localPlayerId != 0) {
                    serverAuthoritativePosition = sf::Vector2f(player.x, player.y);
                    serverAuthoritativeBodyRotation = player.bodyRotation;
                    serverAuthoritativeBarrelRotation = player.barrelRotation;

                    //Store health/maxHealth/score/isDead for local player synchronization
                    serverAuthoritativeHealth = player.health;
                    serverAuthoritativeMaxHealth = player.maxHealth;
                    serverAuthoritativeScore = player.score;
                    serverAuthoritativeIsDead = player.isDead;

                    hasServerAuthoritativeState = true;
                    successfullyParsed++;
                }
                // Add other players to map
                else if (player.playerId != localPlayerId && localPlayerId != 0) {
                    otherPlayers[player.playerId] = player;
                    successfullyParsed++;
                }
            }
            // Verify we parsed expected number of players
            uint32_t expectedOtherPlayers = (localPlayerId == 0) ? playerCount : playerCount - 1;
            if (successfullyParsed < expectedOtherPlayers) {
                Utils::printMsg("Warning: Only parsed " + std::to_string(successfullyParsed) +
                    " of " + std::to_string(expectedOtherPlayers) + " expected players", warning);
            }

            // NEW: Parse enemy data from packet
            uint32_t enemyCount;
            if (packet >> enemyCount) {
                // Clear existing enemy data
                enemyData.clear();
                // Parse each enemy
                for (uint32_t i = 0; i < enemyCount; ++i) {
                    EnemyData enemy;
                    if (packet >> enemy.enemyId >> enemy.enemyType >> enemy.x >> enemy.y >>
                        enemy.bodyRotation >> enemy.barrelRotation >> enemy.health >> enemy.maxHealth) {
                        // Store enemy data
                        enemyData[enemy.enemyId] = enemy;
                    }
                    else {
                        Utils::printMsg("Failed to parse enemy " + std::to_string(i), warning);
                        break;
                    }
                }
                // Debug log enemy reception
                static int receiveCounter = 0;
                if (++receiveCounter % 100 == 0) {
                    Utils::printMsg("Client received " + std::to_string(enemyCount) + " enemies", debug);
                }
            }
            else {
                Utils::printMsg("Failed to extract enemy count (may be old server version)", debug);
                // Not a critical error - continue without enemies
            }

            // Extract timestamp and sequence number
            int64_t timestamp;
            uint32_t sequenceNumber;
            uint32_t lastAckedInput;
            if (packet >> timestamp >> sequenceNumber >> lastAckedInput) {
                int64_t currentTime = GetCurrentTimestamp();
                if (NetworkValidation::IsValidTimestamp(timestamp, currentTime)) {
                    RecordReceivedPacket(sequenceNumber);
                    lastServerTimestamp = timestamp;
                    // Process acknowledged input
                    if (lastAckedInput > 0 && lastAckedInput > lastAcknowledgedInputSeq) {
                        prediction->AcknowledgeInput(lastAckedInput);
                        lastAcknowledgedInputSeq = lastAckedInput;
                        lastInputAckTime = currentTime;
                    }
                    consecutiveErrors = 0;
                }
                else {
                    Utils::printMsg("Invalid timestamp in game state (delta: " +
                        std::to_string(std::abs(currentTime - timestamp)) + "ms)", debug);
                }
            }
        }
        else if (msgType == NetMessageType::PLAYER_ID_ASSIGNMENT) {
            uint32_t assignedId;
            if (packet >> assignedId) {
                //  Validate assigned player ID
                if (!NetworkValidation::IsValidPlayerId(assignedId)) {
                    Utils::printMsg("Invalid player ID assignment: " + std::to_string(assignedId), error);
                    consecutiveErrors++;
                    return;
                }
                localPlayerId = assignedId;
                Utils::printMsg("Assigned player ID: " + std::to_string(localPlayerId));
                consecutiveErrors = 0;
            }
            else {
                Utils::printMsg("Failed to extract player ID from assignment", warning);
                consecutiveErrors++;
            }
        }
        else if (msgType == NetMessageType::PONG) {
            PongMessage pongMsg;
            if (packet >> pongMsg.originalTimestamp >> pongMsg.sequenceNumber) {
                //  Validate pong timestamp
                if (pongMsg.originalTimestamp <= 0) {
                    Utils::printMsg("Invalid pong timestamp: " + std::to_string(pongMsg.originalTimestamp), warning);
                    return;
                }
                HandlePong(pongMsg);
                consecutiveErrors = 0;
            }
            else {
                Utils::printMsg("Failed to extract pong message data", warning);
                consecutiveErrors++;
            }
        }
        else if (msgType == NetMessageType::BULLET_UPDATE) {
            BulletUpdateMessage updateMsg;
            updateMsg.type = msgType;

            uint32_t bulletCount;
            if (!(packet >> bulletCount)) {
                Utils::printMsg("Failed to extract bullet count", warning);
                return;
            }

            updateMsg.bullets.clear();
            updateMsg.bullets.reserve(bulletCount);

            for (uint32_t i = 0; i < bulletCount; ++i) {
                BulletData bullet;
                if (packet >> bullet) {
                    updateMsg.bullets.push_back(bullet);
                }
                else {
                    Utils::printMsg("Failed to parse bullet " + std::to_string(i), warning);
                    break;
                }
            }

            if (packet >> updateMsg.timestamp >> updateMsg.sequenceNumber) {
                HandleBulletUpdate(updateMsg);
            }
            else {
                Utils::printMsg("Failed to extract bullet update metadata", warning);
            }
        }

        // Handle bullet destroy
        else if (msgType == NetMessageType::BULLET_DESTROY) {
            BulletDestroyMessage destroyMsg;
            destroyMsg.type = msgType;

            if (packet >> destroyMsg.bulletId >> destroyMsg.destroyReason
                >> destroyMsg.hitTargetId >> destroyMsg.hitX >> destroyMsg.hitY
                >> destroyMsg.timestamp >> destroyMsg.sequenceNumber) {

                HandleBulletDestroy(destroyMsg);
            }
            else {
                Utils::printMsg("Failed to parse bullet destroy message", warning);
            }
        }
        else if (msgType == NetMessageType::INPUT_ACKNOWLEDGMENT) {
            // Handle explicit input acknowledgment
            InputAcknowledgmentMessage ackMsg;
            if (packet >> ackMsg.playerId >> ackMsg.acknowledgedSequence >> ackMsg.serverTimestamp) {
                HandleInputAcknowledgment(ackMsg);
            }
            else {
                Utils::printMsg("Failed to extract input acknowledgment data", warning);
            }
        }
        else if (msgType == NetMessageType::PLAYER_DEATH) {
            // Handle player death message
            PlayerDeathMessage deathMsg;
            if (packet >> deathMsg.playerId >> deathMsg.killerId >> deathMsg.deathX >> deathMsg.deathY
                >> deathMsg.scorePenalty >> deathMsg.timestamp >> deathMsg.sequenceNumber) {

                Utils::printMsg(" DEATH MESSAGE: Player " + std::to_string(deathMsg.playerId) +
                    " died | Penalty: " + std::to_string(deathMsg.scorePenalty) + " points", error);

                // If this is the local player, notify them
                if (deathMsg.playerId == localPlayerId) {
                    Utils::printMsg("YOU DIED! You will respawn in 5 seconds...", error);
                }
            }
            else {
                Utils::printMsg("Failed to extract player death data", warning);
            }
        }
        else if (msgType == NetMessageType::PLAYER_RESPAWN) {
            // Handle player respawn message
            PlayerRespawnMessage respawnMsg;
            if (packet >> respawnMsg.playerId >> respawnMsg.spawnX >> respawnMsg.spawnY
                >> respawnMsg.health >> respawnMsg.timestamp >> respawnMsg.sequenceNumber) {

                Utils::printMsg("RESPAWN MESSAGE: Player " + std::to_string(respawnMsg.playerId) +
                    " respawned at (" + std::to_string(respawnMsg.spawnX) + ", " +
                    std::to_string(respawnMsg.spawnY) + ")", success);

                // If this is the local player, notify them
                if (respawnMsg.playerId == localPlayerId) {
                    Utils::printMsg("YOU RESPAWNED! Back in action!", success);
                }
            }
            else {
                Utils::printMsg("Failed to extract player respawn data", warning);
            }
        }
        else {
            Utils::printMsg("Received unknown message type: " + std::to_string(static_cast<int>(msgType)), debug);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in ProcessPacket: " + std::string(e.what()), error);
        consecutiveErrors++;
    }
    catch (...) {
        Utils::printMsg("Unknown exception in ProcessPacket", error);
        consecutiveErrors++;
    }
}

void NetworkClient::ValidateAndClampLocalPlayerData(Tank& localPlayer) {
    //  Validate and clamp local player position before sending
    if (!NetworkValidation::IsValidPosition(localPlayer.position.x, localPlayer.position.y)) {
        float oldX = localPlayer.position.x;
        float oldY = localPlayer.position.y;

        localPlayer.position.x = NetworkValidation::ClampPositionX(localPlayer.position.x);
        localPlayer.position.y = NetworkValidation::ClampPositionY(localPlayer.position.y);

        Utils::printMsg("Clamped local player position from (" +
            std::to_string(oldX) + ", " + std::to_string(oldY) + ") to (" +
            std::to_string(localPlayer.position.x) + ", " +
            std::to_string(localPlayer.position.y) + ")", debug);
    }

    //  Normalize rotations
    float bodyRotDegrees = localPlayer.bodyRotation.asDegrees();
    float barrelRotDegrees = localPlayer.barrelRotation.asDegrees();

    if (!NetworkValidation::IsValidRotation(bodyRotDegrees)) {
        bodyRotDegrees = NetworkValidation::NormalizeRotation(bodyRotDegrees);
        localPlayer.bodyRotation = sf::degrees(bodyRotDegrees);
    }

    if (!NetworkValidation::IsValidRotation(barrelRotDegrees)) {
        barrelRotDegrees = NetworkValidation::NormalizeRotation(barrelRotDegrees);
        localPlayer.barrelRotation = sf::degrees(barrelRotDegrees);
    }
}

void NetworkClient::HandlePong(const PongMessage& msg) {
    try {
        // Calculate RTT
        int64_t currentTime = GetCurrentTimestamp();
        float rtt = static_cast<float>(currentTime - msg.originalTimestamp);

        // Validate RTT is reasonable (< 10 seconds)
        if (rtt < 0 || rtt > 10000) {
            Utils::printMsg("Invalid RTT calculated: " + std::to_string(rtt), warning);
            return;
        }

        // Update network statistics
        UpdateNetworkStatistics(rtt);

        // Remove this ping from sent packets tracking
        auto it = std::find_if(sentPackets.begin(), sentPackets.end(),
            [&msg](const SentPacket& sp) { return sp.sequenceNumber == msg.sequenceNumber; });

        if (it != sentPackets.end()) {
            sentPackets.erase(it);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in HandlePong: " + std::string(e.what()), error);
    }
    catch (...) {
        Utils::printMsg("Unknown exception in HandlePong", error);
    }
}

void NetworkClient::UpdateNetworkStatistics(float rtt) {
    // Add to RTT history
    rttHistory.push_back(rtt);
    if (rttHistory.size() > RTT_HISTORY_SIZE) {
        rttHistory.pop_front();
    }

    // Update min/max RTT
    networkStats.minRTT = std::min(networkStats.minRTT, rtt);
    networkStats.maxRTT = std::max(networkStats.maxRTT, rtt);

    // Calculate average RTT
    float totalRTT = 0;
    for (float r : rttHistory) {
        totalRTT += r;
    }
    networkStats.averageRTT = totalRTT / rttHistory.size();
    networkStats.averageLatency = networkStats.averageRTT / 2.0f;

    // Calculate jitter (variation in delay)
    if (rttHistory.size() > 1) {
        float variance = 0;
        for (float r : rttHistory) {
            float diff = r - networkStats.averageRTT;
            variance += diff * diff;
        }
        networkStats.jitter = std::sqrt(variance / rttHistory.size());
    }

    // Calculate packet loss percentage
    if (networkStats.totalPacketsSent > 0) {
        networkStats.packetsLost = networkStats.totalPacketsSent - networkStats.totalPacketsReceived;
        networkStats.packetLoss = (static_cast<float>(networkStats.packetsLost) /
            static_cast<float>(networkStats.totalPacketsSent)) * 100.0f;
    }
}

void NetworkClient::RecordReceivedPacket(uint32_t sequenceNumber) {
    // Check for out-of-order packets
    if (IsPacketOutOfOrder(sequenceNumber)) {
        Utils::printMsg("Out-of-order packet detected: " + std::to_string(sequenceNumber), debug);
    }

    // Record this sequence number
    receivedSequenceNumbers[sequenceNumber] = true;

    // Update last received sequence number if this is newer
    if (sequenceNumber > lastReceivedSequenceNumber) {
        lastReceivedSequenceNumber = sequenceNumber;
    }

    // Cleanup old sequence numbers periodically
    CleanupOldSequenceNumbers();
}

bool NetworkClient::IsPacketOutOfOrder(uint32_t sequenceNumber) const {
    // If we've already received this packet, it's a duplicate
    if (receivedSequenceNumbers.find(sequenceNumber) != receivedSequenceNumbers.end()) {
        return true;
    }

    // If this sequence is less than the last received, it's out of order
    if (sequenceNumber < lastReceivedSequenceNumber) {
        return true;
    }

    return false;
}

void NetworkClient::CleanupOldSequenceNumbers() {
    if (receivedSequenceNumbers.size() > MAX_SEQUENCE_HISTORY) {
        // Find oldest sequence numbers and remove them
        uint32_t minSequence = lastReceivedSequenceNumber > MAX_SEQUENCE_HISTORY ?
            lastReceivedSequenceNumber - MAX_SEQUENCE_HISTORY : 0;

        auto it = receivedSequenceNumbers.begin();
        while (it != receivedSequenceNumbers.end()) {
            if (it->first < minSequence) {
                it = receivedSequenceNumbers.erase(it);
            }
            else {
                ++it;
            }
        }
    }
}
void NetworkClient::SetOnFirstGameStateCallback(OnFirstGameStateCallback cb) {
    onFirstGameState = std::move(cb);
}

int64_t NetworkClient::GetLastGameStateTimestamp() const {
    return lastGameStateTimestamp;
}
void NetworkClient::HandleGameState(const GameStateMessage& msg) {
    // Clear current other players
    otherPlayers.clear();
    lastGameStateTimestamp = msg.timestamp;
    if (!interpolationInitialized && onFirstGameState) {
        onFirstGameState(msg.timestamp);
        interpolationInitialized = true;
    }
    // Process all players in the game state
    for (const PlayerData& player : msg.players) {
        if (localPlayerId == 0) {
            localPlayerId = player.playerId;
            Utils::printMsg("Assigned player ID: " + std::to_string(localPlayerId));
        }

        if (player.playerId != localPlayerId) {
            otherPlayers[player.playerId] = player;
        }
    }
    lastGameStateTimestamp = msg.timestamp;
    // Record received packet for statistics
    RecordReceivedPacket(msg.sequenceNumber);
}

bool NetworkClient::SendJoinRequest(const std::string& playerName, const std::string& preferredColor) {
    try {
        sf::Packet packet;

        JoinMessage joinMsg;
        joinMsg.playerName = playerName;
        joinMsg.preferredColor = preferredColor;
        joinMsg.timestamp = GetCurrentTimestamp();
        joinMsg.sequenceNumber = outgoingSequenceNumber++;

        packet << static_cast<uint8_t>(joinMsg.type) << joinMsg.playerName << joinMsg.preferredColor
            << joinMsg.timestamp << joinMsg.sequenceNumber;

        networkStats.totalPacketsSent++;

        sf::Socket::Status sendStatus = socket.send(packet, serverAddress, serverPort);

        if (sendStatus == sf::Socket::Status::Done) {
            Utils::printMsg("Join request sent to server");
            consecutiveErrors = 0;
            return true;
        }
        else if (sendStatus == sf::Socket::Status::NotReady) {
            Utils::printMsg("Socket not ready for join request", warning);
            return false;
        }
        else {
            Utils::printMsg("Failed to send join request - Status: " +
                SocketStatusToString(sendStatus), error);
            consecutiveErrors++;
            return false;
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in SendJoinRequest: " + std::string(e.what()), error);
        consecutiveErrors++;
        return false;
    }
    catch (...) {
        Utils::printMsg("Unknown exception in SendJoinRequest", error);
        consecutiveErrors++;
        return false;
    }
}

void NetworkClient::CleanupSocketResources() {
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

std::string NetworkClient::SocketStatusToString(sf::Socket::Status status) const {
    switch (status) {
    case sf::Socket::Status::Done:         return "Done";
    case sf::Socket::Status::NotReady:     return "NotReady";
    case sf::Socket::Status::Partial:      return "Partial";
    case sf::Socket::Status::Disconnected: return "Disconnected";
    case sf::Socket::Status::Error:        return "Error";
    default:                                return "Unknown";
    }
}

void NetworkClient::DetectPacketLoss() {
    // Calculate packet loss from network statistics
    if (networkStats.totalPacketsSent > NetworkValidation::SEQUENCE_WINDOW_SIZE) {
        float lossPercentage = networkStats.packetLoss;

        if (lossPercentage >= NetworkValidation::PACKET_LOSS_THRESHOLD) {
            Utils::printMsg("High packet loss detected: " + std::to_string(lossPercentage) +
                "% (Sent: " + std::to_string(networkStats.totalPacketsSent) +
                ", Received: " + std::to_string(networkStats.totalPacketsReceived) + ")",
                warning);
        }
    }
}

// INPUT BUFFERING SYSTEM METHODS

void NetworkClient::HandleInputAcknowledgment(const InputAcknowledgmentMessage& msg) {
    if (msg.playerId != localPlayerId) {
        return;  // Not for us
    }
    // Track acknowledgments received
    static uint32_t acksReceived = 0;
    if (++acksReceived % 100 == 0) {
        Utils::printMsg("Client received 100 input acks (total: " + std::to_string(acksReceived) + ")", debug);
    }

    // Acknowledge the input in our buffer
    prediction->AcknowledgeInput(msg.acknowledgedSequence);

    // Update tracking
    if (msg.acknowledgedSequence > lastAcknowledgedInputSeq) {
        lastAcknowledgedInputSeq = msg.acknowledgedSequence;
        lastInputAckTime = GetCurrentTimestamp();

        //         Utils::printMsg("Input acknowledged: " + std::to_string(msg.acknowledgedSequence), debug);
    }
}

void NetworkClient::ReplayInputsAfterCorrection(Tank& localPlayer, uint32_t fromSequence, sf::Vector2f mousePos) {
    // Get inputs to replay
    std::vector<InputState> inputsToReplay;
    prediction->GetInputsToReplay(inputsToReplay);

    if (inputsToReplay.empty()) {
        return;
    }

    Utils::printMsg("Replaying " + std::to_string(inputsToReplay.size()) +
        " inputs after correction", debug);

    // Replay each input in order
    for (const auto& input : inputsToReplay) {
        ApplyInputToTank(localPlayer, input, mousePos);
    }

    // Clear replay flags
    prediction->ClearReplayFlags();

    // Update sprites after replay
    localPlayer.UpdateSprites();
}

void NetworkClient::ProcessInputBuffer(float deltaTime) {
    if (!prediction) return;

    // Update buffer timers
    prediction->UpdateBufferTimers(deltaTime);

    // Cleanup timed-out inputs
    prediction->CleanupTimedOutInputs();

    // Optional: Log buffer stats periodically for debugging
    static float statsTimer = 0;
    statsTimer += deltaTime;
    if (statsTimer >= 5.0f) {  // Every 5 seconds
        auto stats = prediction->GetBufferStats();
        float currentRTT = networkStats.averageRTT;

        // Always log stats, even if buffer is empty (shows fix is working!)
        Utils::printMsg("Input Buffer: " + std::to_string(stats.totalBuffered) +
            " buffered, " + std::to_string(stats.needingReplay) +
            " need replay, avg time: " +
            std::to_string(stats.averageBufferTime) + "ms | RTT: " +
            std::to_string(currentRTT) + "ms", debug);

        // Diagnostic: buffer time should be close to RTT (only check if buffer has items)
        if (stats.totalBuffered > 0 && stats.averageBufferTime > currentRTT * 3.0f) {
            Utils::printMsg("WARNING: Buffer time (" + std::to_string(stats.averageBufferTime) +
                "ms) is much higher than RTT (" + std::to_string(currentRTT) +
                "ms) - acknowledgments may be delayed!", warning);
        }

        statsTimer = 0;
    }
}
void NetworkClient::SendBulletSpawn(const Tank& localPlayer) {
    if (!isConnected || localPlayerId == 0) {
        return;
    }

    try {
        sf::Packet packet;
        BulletSpawnMessage spawnMsg;
        spawnMsg.type = NetMessageType::BULLET_SPAWN;
        spawnMsg.playerId = localPlayerId;

        // Get spawn position (barrel end)
        sf::Vector2f spawnPos = localPlayer.GetBarrelEndPosition();
        spawnMsg.spawnX = spawnPos.x;
        spawnMsg.spawnY = spawnPos.y;

        // Calculate direction from barrel rotation
        float barrelRad = localPlayer.barrelRotation.asDegrees() * 3.14159f / 180.0f;
        spawnMsg.directionX = std::cos(barrelRad);
        spawnMsg.directionY = std::sin(barrelRad);

        spawnMsg.barrelRotation = localPlayer.barrelRotation.asDegrees();
        spawnMsg.timestamp = GetCurrentTimestamp();
        spawnMsg.sequenceNumber = outgoingSequenceNumber++;

        // Serialize
        packet << static_cast<uint8_t>(spawnMsg.type)
            << spawnMsg.playerId
            << spawnMsg.spawnX
            << spawnMsg.spawnY
            << spawnMsg.directionX
            << spawnMsg.directionY
            << spawnMsg.barrelRotation
            << spawnMsg.timestamp
            << spawnMsg.sequenceNumber;

        networkStats.totalPacketsSent++;

        // Send to server
        sf::Socket::Status sendStatus = socket.send(packet, serverAddress, serverPort);

        if (sendStatus == sf::Socket::Status::Done) {
            Utils::printMsg("Sent bullet spawn request (seq: " +
                std::to_string(spawnMsg.sequenceNumber) + ")", debug);
            consecutiveErrors = 0;
        }
        else if (sendStatus != sf::Socket::Status::NotReady) {
            Utils::printMsg("Failed to send bullet spawn - Status: " +
                SocketStatusToString(sendStatus), warning);
            consecutiveErrors++;
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in SendBulletSpawn: " + std::string(e.what()), error);
        consecutiveErrors++;
    }
}

/**
 * Handles bullet update from server
 */
void NetworkClient::HandleBulletUpdate(const BulletUpdateMessage& msg) {
    try {
        // Clear existing bullets
        bulletData.clear();

        // Update with new bullet data
        for (const auto& bullet : msg.bullets) {
            bulletData[bullet.bulletId] = bullet;
        }

        // Debug log periodically
        static int updateCounter = 0;
        if (++updateCounter % 30 == 0) {
            Utils::printMsg("Client received bullet update: " +
                std::to_string(bulletData.size()) + " bullets", debug);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in HandleBulletUpdate: " + std::string(e.what()), error);
    }
}

/**
 * Handles bullet destruction from server
 */
void NetworkClient::HandleBulletDestroy(const BulletDestroyMessage& msg) {
    try {
        // Remove bullet from tracked bullets
        auto it = bulletData.find(msg.bulletId);
        if (it != bulletData.end()) {
            bulletData.erase(it);
        }

        // Log destruction
        std::string reasonStr;
        switch (msg.destroyReason) {
        case 0: reasonStr = "Expired"; break;
        case 1: reasonStr = "Hit Player"; break;
        case 2: reasonStr = "Hit Enemy"; break;
        case 3: reasonStr = "Hit Border"; break;
        default: reasonStr = "Unknown"; break;
        }

        Utils::printMsg("Bullet " + std::to_string(msg.bulletId) +
            " destroyed: " + reasonStr +
            " at (" + std::to_string(msg.hitX) + ", " +
            std::to_string(msg.hitY) + ")", debug);


    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in HandleBulletDestroy: " + std::string(e.what()), error);
    }
}