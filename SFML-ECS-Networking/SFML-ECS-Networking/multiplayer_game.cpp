#include "multiplayer_game.h"
#include "utils.h"
#include "world_constants.h"
#include "network_client.h"
#include <unordered_set>
MultiplayerGame::MultiplayerGame()
    : background(nullptr), window(nullptr), playerScore(0),          
    scoreText(nullptr),       //  Initialize text pointer
    scoreFontLoaded(false)    // Initialize font flag
{
    networkClient = std::make_unique<NetworkClient>();
    borderManager = std::make_unique<BorderManager>();
    interpolationManager = std::make_unique<InterpolationManager>();
}

MultiplayerGame::~MultiplayerGame() {
    Shutdown();
    if (scoreText) {
        delete scoreText;
        scoreText = nullptr;
    }
}

bool MultiplayerGame::Initialize(const std::string& playerName, const std::string& preferredColor) {
    this->playerName = playerName;
    this->playerColor = preferredColor;
    playerScore = 0;  // Initialize score to 0

    if (!backgroundTexture.loadFromFile("Assets/background_snow.png")) {
        Utils::printMsg("Warning: Could not load background texture", warning);
        sf::Image fallbackImage;
        fallbackImage.resize(sf::Vector2u(1, 1), sf::Color::White);
        if (!backgroundTexture.loadFromImage(fallbackImage)) {
            Utils::printMsg("Error: Failed to create fallback texture", error);
            return false;
        }
    }
    backgroundTexture.setRepeated(true);
    background = std::make_unique<sf::Sprite>(backgroundTexture);
    background->setTextureRect(sf::IntRect({ 0, 0 }, { 1280, 960 }));

    if (!borderManager->Initialize(1280.0f, 960.0f, 48.0f)) {
        Utils::printMsg("Warning: Border system initialization had issues", warning);
    }
    try {
        scoreFontLoaded = scoreFont.openFromFile("C:/Windows/Fonts/arial.ttf");
        if (!scoreFontLoaded) {
            scoreFontLoaded = scoreFont.openFromFile("C:/Windows/Fonts/calibri.ttf");
        }

        if (scoreFontLoaded) {
            //  Create text with font in constructor
            scoreText = new sf::Text(scoreFont);
            scoreText->setString("Score: 0");
            scoreText->setCharacterSize(24);
            scoreText->setFillColor(sf::Color::White);
            scoreText->setOutlineColor(sf::Color::Black);
            scoreText->setOutlineThickness(2.0f);
            scoreText->setPosition(sf::Vector2f(10.0f, 10.0f)); 
            Utils::printMsg("✓ Score display initialized");
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Warning: Could not load font for score display - " +
            std::string(e.what()), warning);
        scoreFontLoaded = false;
    }
    localTank = std::make_unique<Tank>(preferredColor, playerName);
    localTank->position = { WorldConstants::CENTER_X, WorldConstants::CENTER_Y };

    Utils::printMsg("Multiplayer game initialized for player: " + playerName);
    return true;
}

bool MultiplayerGame::ConnectToServer(const std::string& serverIP, unsigned short serverPort) {
    if (!localTank) {
        Utils::printMsg("Game not initialized before connecting to server", error);
        return false;
    }

    bool connected = networkClient->Connect(serverIP, serverPort, playerName, playerColor);

    if (connected && interpolationManager) {
        snapshotCountForInterpolation = 0;
        gameStartTime = 0;

        networkClient->SetOnFirstGameStateCallback([this](int64_t serverTime) {
            snapshotCountForInterpolation++;

            if (snapshotCountForInterpolation == 1) {
                gameStartTime = GetCurrentTimestamp();
            }

            if (snapshotCountForInterpolation >= 2) {
                float rtt = networkClient->GetAverageRTT();

                // CHANGED: Use 2x RTT instead of 0.5x for smoother interpolation
                // Higher delay = more buffering = smoother movement
                int64_t delay = static_cast<int64_t>(std::max(100.0f, rtt * 2.0f));  // Was rtt * 0.5f
                int64_t renderTime = GetCurrentTimestamp() - delay;

                interpolationManager->Initialize(renderTime);
                interpolationManager->SetInterpolationDelay(delay);

                Utils::printMsg("Interpolation STARTED: renderTime=" + std::to_string(renderTime) +
                    " delay=" + std::to_string(delay) + "ms (RTT=" + std::to_string(rtt) + "ms)", success);
            }
            });
    }

    return connected;
}

void MultiplayerGame::Shutdown() {
    if (networkClient) {
        networkClient->Disconnect();
    }
    if (interpolationManager) {
        interpolationManager->Clear();
    }
    localTank.reset();
    otherTanks.clear();
    enemies.clear();  //  Clear all enemies
    bullets.clear();  //  Clear all bullets
    borderManager.reset();
    background.reset();
    window = nullptr;  // Clear window reference
    snapshotCountForInterpolation = 0;
    playerScore = 0;  // Reset score

}

void MultiplayerGame::HandleEvents(const std::optional<sf::Event> event) {
    if (!localTank || !event) return;

    if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
        if (keyPressed->scancode == sf::Keyboard::Scancode::W) {
            localTank->isMoving.forward = true;
            localTank->isMoving.backward = false;
        }
        else if (keyPressed->scancode == sf::Keyboard::Scancode::S) {
            localTank->isMoving.forward = false;
            localTank->isMoving.backward = true;
        }
        if (keyPressed->scancode == sf::Keyboard::Scancode::A) {
            localTank->isMoving.left = true;
            localTank->isMoving.right = false;
        }
        else if (keyPressed->scancode == sf::Keyboard::Scancode::D) {
            localTank->isMoving.left = false;
            localTank->isMoving.right = true;
        }
        if (keyPressed->scancode == sf::Keyboard::Scancode::Space) {
            if (localTank->CanShoot() && networkClient && networkClient->IsConnected()) {
                // Send bullet spawn request to server
                networkClient->SendBulletSpawn(*localTank);

                // Start local cooldown immediately (client prediction)
                localTank->Shoot(bullets);  // This starts cooldown + spawns local bullet

                Utils::printMsg("Requested bullet spawn from server", debug);
            }
        }
    }
    else if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>()) {
        if (keyReleased->scancode == sf::Keyboard::Scancode::W) localTank->isMoving.forward = false;
        if (keyReleased->scancode == sf::Keyboard::Scancode::S) localTank->isMoving.backward = false;
        if (keyReleased->scancode == sf::Keyboard::Scancode::A) localTank->isMoving.left = false;
        if (keyReleased->scancode == sf::Keyboard::Scancode::D) localTank->isMoving.right = false;
    }
    else if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
        if (mousePressed->button == sf::Mouse::Button::Left) {
            if (localTank->CanShoot() && networkClient && networkClient->IsConnected()) {
                networkClient->SendBulletSpawn(*localTank);
                localTank->Shoot(bullets);  // Start cooldown + local spawn
                Utils::printMsg("Requested bullet spawn from server (mouse)", debug);
            }
        }
    }
}

void MultiplayerGame::Update(float dt) {
    if (!networkClient) return;

    networkClient->Update(dt);

    if (interpolationManager) {
        interpolationManager->Update(dt);
    }

    if (localTank) {
        // Check if player is dead - if so, skip movement/input
        bool isDead = networkClient->GetServerAuthoritativeIsDead();

        if (!isDead) {
            localTank->UpdateCooldown(dt);

            sf::Vector2f mousePos = GetMouseWorldPosition();

            if (networkClient->IsPredictionEnabled()) {
                networkClient->ApplyLocalInputWithPrediction(*localTank, dt, mousePos);
                networkClient->ApplyServerReconciliation(*localTank);
            }
            else {
                localTank->Update(dt, mousePos, true);
                if (IsConnected()) {
                    networkClient->SendPlayerInput(*localTank);
                }
            }

            EnforceBorderCollision(*localTank);
            CheckTankCollisions();
        }
        else {
            // Player is dead - clear all movement flags to stop any motion
            localTank->isMoving.forward = false;
            localTank->isMoving.backward = false;
            localTank->isMoving.left = false;
            localTank->isMoving.right = false;
        }
    }
    if (networkClient && networkClient->HasServerAuthoritativeState()) {
        float serverHealth = networkClient->GetServerAuthoritativeHealth();
        float serverMaxHealth = networkClient->GetServerAuthoritativeMaxHealth();

        float oldHealth = localTank->GetHealth();

        // Apply server's authoritative health
        localTank->SetHealth(serverHealth);
        localTank->SetMaxHealth(serverMaxHealth);

        // Log health changes (helps with debugging)
        if (std::abs(oldHealth - serverHealth) > 0.1f) {
            //Utils::printMsg("LOCAL HEALTH SYNC: " +
            //    std::to_string(oldHealth) + " → " +
            //    std::to_string(serverHealth) + " (" +
            //    std::to_string((serverHealth / serverMaxHealth) * 100.0f) + "%)",
            //    success);
        }

        // Alert player when they die
        if (serverHealth <= 0.0f && oldHealth > 0.0f) {
            Utils::printMsg("YOU DIED! Health reached 0", error);
        }

        //  Clear the flag AFTER health sync is complete
        networkClient->ClearServerAuthoritativeState();
    }

    // Sync player score from server EVERY frame (separate from health sync)
    // Score changes happen independently (killing enemies, dying, etc.)
    if (networkClient && IsConnected()) {
        int32_t serverScore = networkClient->GetServerAuthoritativeScore();

        if (playerScore != serverScore) {
            int32_t scoreDiff = serverScore - playerScore;
        /*    Utils::printMsg("SCORE SYNC: " + std::to_string(playerScore) +
                " → " + std::to_string(serverScore) +
                " (" + (scoreDiff >= 0 ? "+" : "") + std::to_string(scoreDiff) + ")",
                info);*/
        }
        playerScore = serverScore;
    }
    int64_t serverTimestamp = networkClient->GetLastGameStateTimestamp();
    if (serverTimestamp == 0) {
        serverTimestamp = GetCurrentTimestamp();
    }

    UpdateOtherPlayers(serverTimestamp);

    if (networkClient && IsConnected()) {
        UpdateEnemies(networkClient->GetEnemies());
    }

    if (networkClient && IsConnected()) {
        SynchronizeBulletsFromServer();
    }

    // Update all bullets (both local prediction and server-confirmed)
    for (auto& bullet : bullets) {
        if (bullet) {
            bullet->Update(dt);
        }
    }

    // Check bullet collisions (client-side for immediate feedback)
    CheckBulletCollisions();

    // Remove expired bullets
    bullets.erase(
        std::remove_if(bullets.begin(), bullets.end(),
            [](const std::unique_ptr<Bullet>& bullet) {
                return bullet->IsExpired();
            }),
        bullets.end()
    );
    // Debug logging
    static float logTimer = 0;
    logTimer += dt;
    if (logTimer > 1.0f && interpolationManager && interpolationManager->GetRenderTime() > 0) {
        int64_t render = interpolationManager->GetRenderTime();
        //Utils::printMsg("RENDER: " + std::to_string(render) +
        //    " | Bullets: " + std::to_string(bullets.size()) +
        //    " | Enemies: " + std::to_string(enemies.size()) +
        //    " | Score: " + std::to_string(playerScore));
        logTimer = 0;
    }
}

// NEW: Get mouse position in world coordinates
sf::Vector2f MultiplayerGame::GetMouseWorldPosition() const {
    if (!window) {
        // Fallback: return center of screen if window not set
        Utils::printMsg("Warning: Window not set, using center position for mouse", debug);
        return sf::Vector2f(WorldConstants::CENTER_X, WorldConstants::CENTER_Y);
    }

    try {
        // Get mouse position relative to window
        sf::Vector2i pixelPos = sf::Mouse::getPosition(*window);

        // Convert to world coordinates (no view/camera transform needed for now)
        sf::Vector2f worldPos = window->mapPixelToCoords(pixelPos);

        return worldPos;
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception getting mouse position - " + std::string(e.what()), error);
        return sf::Vector2f(WorldConstants::CENTER_X, WorldConstants::CENTER_Y);
    }
}

void MultiplayerGame::EnforceBorderCollision(Tank& tank) {
    if (!borderManager) return;
    const float tankRadius = WorldConstants::TANK_RADIUS;
    if (!borderManager->IsPositionInBounds(tank.position, tankRadius)) {
        sf::Vector2f clamped = borderManager->ClampPositionToBounds(tank.position, tankRadius);
        if (tank.position != clamped) {
            tank.position = clamped;
            tank.UpdateSprites();
        }
    }
}

void MultiplayerGame::UpdateOtherPlayers(int64_t serverTimestamp) {
    if (!networkClient || !IsConnected()) return;

    const auto& otherPlayersData = networkClient->GetOtherPlayers();

    //  Remove players who left the game
    for (auto tankIt = otherTanks.begin(); tankIt != otherTanks.end(); ) {
        uint32_t playerId = tankIt->first;
        if (otherPlayersData.find(playerId) == otherPlayersData.end()) {
            Utils::printMsg("Player " + std::to_string(playerId) + " left the game");
            if (interpolationManager) {
                interpolationManager->RemoveEntity(playerId);
            }
            tankIt = otherTanks.erase(tankIt);
        }
        else {
            ++tankIt;
        }
    }

    //  Update or create tanks for current players 
    for (auto dataIt = otherPlayersData.begin(); dataIt != otherPlayersData.end(); ++dataIt) {
        uint32_t playerId = dataIt->first;
        const PlayerData& playerData = dataIt->second;

        // Find or create tank
        auto tankIt = otherTanks.find(playerId);
        if (tankIt == otherTanks.end()) {
            CreateTankForPlayer(playerId, playerData);
            Utils::printMsg("Player " + playerData.playerName + " (" + std::to_string(playerId) + ") joined");
            tankIt = otherTanks.find(playerId);
        }

        // === 3. Use interpolation if enabled and render time > 0 ===
        if (interpolationManager && interpolationManager->GetRenderTime() > 0 && gameStartTime != 0) {
            // Use RELATIVE time
            int64_t relativeTime = GetCurrentTimestamp() - gameStartTime;

            EntitySnapshot snapshot;
            snapshot.timestamp = relativeTime;
            snapshot.position = sf::Vector2f(playerData.x, playerData.y);
            snapshot.bodyRotation = sf::degrees(playerData.bodyRotation);
            snapshot.barrelRotation = sf::degrees(playerData.barrelRotation);
            snapshot.isMoving_forward = playerData.isMoving_forward;
            snapshot.isMoving_backward = playerData.isMoving_backward;
            snapshot.isMoving_left = playerData.isMoving_left;
            snapshot.isMoving_right = playerData.isMoving_right;

            static std::unordered_map<uint32_t, int> logCounters;
            if (++logCounters[playerId] % 30 == 0) {
                Utils::printMsg(" SNAPSHOT: Player=" + std::to_string(playerId) +
                    " Body=" + std::to_string(playerData.bodyRotation) + "° " +
                    "Barrel=" + std::to_string(playerData.barrelRotation) + "°", debug);
            }

            // Add snapshot to interpolation manager
            interpolationManager->AddEntitySnapshot(playerId, snapshot);

            // Get interpolated state
            InterpolatedState interpState;
            if (interpolationManager->GetEntityState(playerId, interpState)) {
                UpdateTankFromInterpolatedState(*tankIt->second, interpState);

                //  Sync health separately (not part of interpolation)
                tankIt->second->SetHealth(playerData.health);
                tankIt->second->SetMaxHealth(playerData.maxHealth);

                EnforceBorderCollision(*tankIt->second);
            }
        }
        //  Fallback: use raw server data (before interpolation starts)
        else {
            UpdateTankFromPlayerData(*tankIt->second, playerData);
            EnforceBorderCollision(*tankIt->second);
        }
    }
    // The local player's health must be synchronized from server data because:
    // 1. Server is authoritative for health (it processes bullet collisions)
    // 2. Without this, enemy bullets appear to do no damage to the local player
    // 3. Position/rotation use prediction, but health must come from server

    if (localTank && networkClient->GetLocalPlayerId() != 0) {
        // Search for local player in the server's game state
        uint32_t localPlayerId = networkClient->GetLocalPlayerId();

        // Check if we're in the "other players" list (shouldn't be, but check anyway)
        auto localPlayerDataIt = otherPlayersData.find(localPlayerId);

        if (localPlayerDataIt != otherPlayersData.end()) {
            // Found local player data - sync health
            const PlayerData& localPlayerData = localPlayerDataIt->second;

            float oldHealth = localTank->GetHealth();
            localTank->SetHealth(localPlayerData.health);
            localTank->SetMaxHealth(localPlayerData.maxHealth);

            // Log health changes for debugging
            if (oldHealth != localPlayerData.health) {
                //Utils::printMsg("LOCAL PLAYER health synced: " +
                //    std::to_string(oldHealth) + " → " +
                //    std::to_string(localPlayerData.health), success);
            }
        }
    }
}

void MultiplayerGame::CreateTankForPlayer(uint32_t playerId, const PlayerData& playerData) {
    auto tank = std::make_unique<Tank>(playerData.color, playerData.playerName);
    UpdateTankFromPlayerData(*tank, playerData);
    otherTanks[playerId] = std::move(tank);
}

void MultiplayerGame::UpdateTankFromPlayerData(Tank& tank, const PlayerData& playerData) {
    tank.position.x = playerData.x;
    tank.position.y = playerData.y;
    tank.bodyRotation = sf::degrees(playerData.bodyRotation);

    //  Apply barrel rotation from server data
    tank.barrelRotation = sf::degrees(playerData.barrelRotation);

    tank.isMoving.forward = playerData.isMoving_forward;
    tank.isMoving.backward = playerData.isMoving_backward;
    tank.isMoving.left = playerData.isMoving_left;
    tank.isMoving.right = playerData.isMoving_right;

    if (tank.GetPlayerName() != playerData.playerName) {
        tank.SetPlayerName(playerData.playerName);
    }
    // Synchronize health from server
    tank.SetHealth(playerData.health);
    tank.SetMaxHealth(playerData.maxHealth);

    tank.UpdateSprites();
}

void MultiplayerGame::UpdateTankFromInterpolatedState(Tank& tank, const InterpolatedState& state) {
    tank.position = state.position;
    tank.bodyRotation = state.bodyRotation;

    //Apply barrel rotation from interpolated state
    tank.barrelRotation = state.barrelRotation;

    static std::unordered_map<std::string, int> logCounters;
    std::string tankKey = tank.GetPlayerName();
    if (++logCounters[tankKey] % 30 == 0) {
     /*   Utils::printMsg(" TANK UPDATED: " + tankKey +
            " Body=" + std::to_string(state.bodyRotation.asDegrees()) + "° " +
            "Barrel=" + std::to_string(state.barrelRotation.asDegrees()) + "°", debug);*/
    }

    tank.isMoving.forward = state.isMoving;
    tank.isMoving.backward = false;
    tank.isMoving.left = false;
    tank.isMoving.right = false;

    tank.UpdateSprites();
}

void MultiplayerGame::Render(sf::RenderWindow& window) {
    if (background) window.draw(*background);
    if (borderManager) borderManager->Render(window);
    for (auto& [enemyId, enemy] : enemies) {
        if (enemy) {
            enemy->Render(window);
        }
    }
    for (auto& bullet : bullets) {
        if (bullet) {
            bullet->Render(window);
        }
    }
    // Only render local tank if alive
    if (localTank && networkClient && !networkClient->GetServerAuthoritativeIsDead()) {
        localTank->Render(window);
    }
    for (auto it = otherTanks.begin(); it != otherTanks.end(); ++it) {
        // Only render other players if they're alive
        uint32_t playerId = it->first;
        const auto& otherPlayersData = networkClient->GetOtherPlayers();
        auto playerDataIt = otherPlayersData.find(playerId);

        if (playerDataIt != otherPlayersData.end() && !playerDataIt->second.isDead) {
            it->second->Render(window);
        }
    }
    if (scoreFontLoaded && scoreText) {
        scoreText->setString("Score: " + std::to_string(playerScore));
        window.draw(*scoreText);

        // Show "DEAD - Respawning..." overlay if player is dead
        if (networkClient && networkClient->GetServerAuthoritativeIsDead()) {
            sf::Text deadText(scoreFont);
            deadText.setString("DEAD - Respawning...");
            deadText.setCharacterSize(48);
            deadText.setFillColor(sf::Color::Red);
            deadText.setOutlineColor(sf::Color::Black);
            deadText.setOutlineThickness(3.0f);

            // Center on screen
            sf::FloatRect textBounds = deadText.getLocalBounds();
            deadText.setPosition(
                sf::Vector2f((window.getSize().x - textBounds.size.x) / 2.0f,
                    (window.getSize().y - textBounds.size.y) / 2.0f)
            );

            window.draw(deadText);
        }
    }
}

bool MultiplayerGame::IsConnected() const {
    return networkClient && networkClient->IsConnected();
}

size_t MultiplayerGame::GetPlayerCount() const {
    size_t count = localTank ? 1 : 0;
    count += otherTanks.size();
    return count;
}

float MultiplayerGame::GetAverageRTT() const {
    return networkClient ? networkClient->GetAverageRTT() : 35.0f;
}
/**
 * Updates all enemies from network data received from server
 * Creates new enemies, updates existing ones, removes destroyed ones
 * @param enemyData Map of enemy ID to enemy data from server
 */
void MultiplayerGame::UpdateEnemies(const std::unordered_map<uint32_t, EnemyData>& enemyData) {
    try {
        // Remove enemies that no longer exist on server OR are dead locally
        auto it = enemies.begin();
        while (it != enemies.end()) {
            uint32_t enemyId = it->first;

            // Remove if server stopped sending OR if dead locally
            if (enemyData.find(enemyId) == enemyData.end() ||
                (it->second && it->second->IsDead())) {

                Utils::printMsg("Enemy " + std::to_string(enemyId) + " removed", debug);
                it = enemies.erase(it);
            }
            else {
                ++it;
            }
        }

        // Create or update enemies from server data
        for (const auto& [enemyId, data] : enemyData) {
            auto enemyIt = enemies.find(enemyId);

            if (enemyIt == enemies.end()) {
                // New enemy - create it
                CreateEnemyFromData(enemyId, data);
            }
            else {
                // Existing enemy - update position/rotation only
                UpdateEnemyFromData(*enemyIt->second, data);
            }
        }

        // Debug log enemy count periodically
        static float logTimer = 0;
        static float lastDt = 0.016f;
        logTimer += lastDt;
        if (logTimer >= 5.0f) {
            Utils::printMsg("Client: " + std::to_string(enemies.size()) + " enemies active", debug);
            logTimer = 0;
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in UpdateEnemies: " + std::string(e.what()), error);
    }
}


/**
 * Creates a new enemy tank from network data
 * @param enemyId Unique enemy ID from server
 * @param data Enemy data received from server
 */
void MultiplayerGame::CreateEnemyFromData(uint32_t enemyId, const EnemyData& data) {
    try {
        // Convert network type to EnemyType enum
        EnemyTank::EnemyType enemyType = ConvertEnemyType(data.enemyType);

        // Create enemy at the position specified
        sf::Vector2f position(data.x, data.y);
        auto newEnemy = std::make_unique<EnemyTank>(enemyType, position);

        // Set rotation from network data
        newEnemy->SetBodyRotation(sf::degrees(data.bodyRotation));
        newEnemy->SetBarrelRotation(sf::degrees(data.barrelRotation));
        //  Set health from server data on creation
        newEnemy->SetHealth(data.health);
        newEnemy->SetMaxHealth(data.maxHealth);

        // Update sprites with initial data
        newEnemy->UpdateSprites();

        // Add to enemies map
        enemies[enemyId] = std::move(newEnemy);

        Utils::printMsg("Created " + enemies[enemyId]->GetEnemyTypeName() +
            " (ID: " + std::to_string(enemyId) + ")", success);
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in CreateEnemyFromData: " + std::string(e.what()), error);
    }
}

/**
 * Updates an existing enemy tank with new network data
 * @param enemy Reference to enemy tank to update
 * @param data New data from server
 */
void MultiplayerGame::UpdateEnemyFromData(EnemyTank& enemy, const EnemyData& data) {
    try {
        // Update position
        enemy.SetPosition(sf::Vector2f(data.x, data.y));

        // Update rotations
        enemy.SetBodyRotation(sf::degrees(data.bodyRotation));
        enemy.SetBarrelRotation(sf::degrees(data.barrelRotation));

        // Sync health from server (server IS authoritative for enemy health)
        // Server tracks bullet damage and enemy health, so we must sync it
        enemy.SetHealth(data.health);
        enemy.SetMaxHealth(data.maxHealth);

        // Update sprites
        enemy.UpdateSprites();
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in UpdateEnemyFromData: " + std::string(e.what()), error);
    }
}
/**
 * Converts network enemy type value to EnemyType enum
 * @param typeValue Numeric type from network (0-4)
 * @return EnemyType enum value
 */
EnemyTank::EnemyType MultiplayerGame::ConvertEnemyType(uint8_t typeValue) {
    switch (typeValue) {
    case 0: return EnemyTank::EnemyType::RED;
    case 1: return EnemyTank::EnemyType::BLACK;
    case 2: return EnemyTank::EnemyType::PURPLE;
    case 3: return EnemyTank::EnemyType::ORANGE;
    case 4: return EnemyTank::EnemyType::TEAL;
    default:
        Utils::printMsg("Unknown enemy type: " + std::to_string(typeValue) +
            ", defaulting to RED", warning);
        return EnemyTank::EnemyType::RED;
    }
}
/**
 * SIMPLIFIED: Just detect collisions for feedback, don't block movement
 * Server is authoritative for collision resolution
 * This prevents client-server position fighting
 */
void MultiplayerGame::CheckTankCollisions() {
    if (!localTank) return;

    const float TANK_RADIUS = 25.0f;

    // Just check for collision, don't modify position
    // Let server handle authoritative collision resolution

    // Check collision with enemies
    for (auto& [enemyId, enemy] : enemies) {
        if (!enemy) continue;

        if (CheckCircleCollision(localTank->position, TANK_RADIUS,
            enemy->GetPosition(), enemy->GetRadius())) {
            // Collision detected - could add visual/audio feedback here
            // But don't modify position - server will handle it
            return;
        }
    }

    // Check collision with other player tanks
    for (auto& [playerId, otherTank] : otherTanks) {
        if (!otherTank) continue;

        if (CheckCircleCollision(localTank->position, TANK_RADIUS,
            otherTank->position, TANK_RADIUS)) {
            // Collision detected - could add visual/audio feedback here
            // But don't modify position - server will handle it
            return;
        }
    }
}

/**
 * Checks if two circles (tanks) are colliding
 * @param pos1 Position of first circle
 * @param radius1 Radius of first circle
 * @param pos2 Position of second circle
 * @param radius2 Radius of second circle
 * @return True if circles overlap
 */
bool MultiplayerGame::CheckCircleCollision(sf::Vector2f pos1, float radius1,
    sf::Vector2f pos2, float radius2) {
    // Calculate distance between centers
    float dx = pos2.x - pos1.x;
    float dy = pos2.y - pos1.y;
    float distanceSquared = dx * dx + dy * dy;

    // Calculate minimum distance for collision
    float minDistance = radius1 + radius2;
    float minDistanceSquared = minDistance * minDistance;

    // Collision occurs if distance is less than sum of radii
    return distanceSquared < minDistanceSquared;
}


// Implement bullet collision detection methods


/**
 * Main bullet collision checking - checks all collision types
 */
void MultiplayerGame::CheckBulletCollisions() {
    CheckBulletEnemyCollisions();
    CheckBulletBorderCollisions();
}

/**
 * Checks if any bullets hit any enemies
 * Applies damage and awards points on hit
 */
void MultiplayerGame::CheckBulletEnemyCollisions() {
    // Debug counter
    static int collisionChecks = 0;
    collisionChecks++;

    // Iterate through all bullets
    for (auto& bullet : bullets) {
        if (!bullet || bullet->IsDestroyed()) {
            continue;
        }

        sf::Vector2f bulletPos = bullet->GetPosition();
        float bulletRadius = bullet->GetRadius();
        uint32_t bulletOwnerId = bullet->GetOwnerId();

        // Check if this bullet is from a player (not an enemy)
        // Enemy IDs start at 1000+, Player IDs are < 1000
        bool isPlayerBullet = (bulletOwnerId < 1000);

        //  Only check collisions if this is a PLAYER bullet
        if (!isPlayerBullet) {
            // This is an enemy bullet - skip enemy collision checks
            // (Enemy bullets can only hit players, checked elsewhere)
            continue;
        }

        // Now we know this is a player bullet - check against enemies
        for (auto& [enemyId, enemy] : enemies) {
            if (!enemy || enemy->IsDead()) {
                continue;
            }

            // Check circle collision
            if (CheckCircleCollision(bulletPos, bulletRadius,
                enemy->GetPosition(), enemy->GetRadius())) {

                // HIT!
                float damage = bullet->GetDamage();
                float oldHealth = enemy->GetHealth();

                enemy->TakeDamage(damage);

                float newHealth = enemy->GetHealth();

                //Utils::printMsg("HIT! Enemy " + std::to_string(enemyId) +
                //    " (" + enemy->GetEnemyTypeName() +
                //    ") damaged: " + std::to_string(oldHealth) +
                //    " → " + std::to_string(newHealth) + " HP", success);

                // Check if enemy died
                if (enemy->IsDead()) {
                    int points = enemy->GetScoreValue();
                    // NOTE: Don't modify playerScore here - server is authoritative
                    // Score will be synced from server in next game state update

                    Utils::printMsg(" Enemy DESTROYED! +" + std::to_string(points) +
                        " points (server will update)", success);
                }

                // Destroy bullet
                bullet->Destroy();

                // One bullet can only hit one enemy
                break;
            }
        }
    }

    // Log every 60 frames (about once per second)
    //if (collisionChecks % 60 == 0) {
    //    Utils::printMsg("Collision checks: " + std::to_string(collisionChecks) +
    //        " | Active bullets: " + std::to_string(bullets.size()) +
    //        " | Active enemies: " + std::to_string(enemies.size()), debug);
    //}
}

/**
 * Checks if any bullets hit the world borders
 * Destroys bullets that leave playable area
 */
void MultiplayerGame::CheckBulletBorderCollisions() {
    if (!borderManager) return;

    sf::FloatRect worldBounds = borderManager->GetWorldBounds();

    for (auto& bullet : bullets) {
        if (!bullet || bullet->IsDestroyed()) {
            continue;
        }

        sf::Vector2f bulletPos = bullet->GetPosition();

        // Check if bullet is outside world bounds
        if (bulletPos.x < worldBounds.position.x ||
            bulletPos.x > worldBounds.position.x + worldBounds.size.x ||
            bulletPos.y < worldBounds.position.y ||
            bulletPos.y > worldBounds.position.y + worldBounds.size.y) {

            bullet->Destroy();
            Utils::printMsg("Bullet hit border and was destroyed", debug);
        }
    }
}
void MultiplayerGame::SynchronizeBulletsFromServer() {
    if (!networkClient) return;

    try {
        const auto& serverBullets = networkClient->GetBullets();

        // STRATEGY: Replace local bullets with server state
        // Clear all bullets and recreate from server data
        // This ensures server is always authoritative

        // Track which bullets exist on server
        std::unordered_set<uint32_t> serverBulletIds;
        for (const auto& [bulletId, bulletData] : serverBullets) {
            serverBulletIds.insert(bulletId);
        }

        // Remove bullets not on server (client predicted bullets that server rejected/destroyed)
        bullets.erase(
            std::remove_if(bullets.begin(), bullets.end(),
                [&serverBulletIds](const std::unique_ptr<Bullet>& bullet) {
                    uint32_t bulletId = bullet->GetBulletId();
                    // Keep bullets with ID 0 (local predicted, waiting for server confirmation)
                    // Remove bullets with IDs not in server list
                    return bulletId != 0 && serverBulletIds.find(bulletId) == serverBulletIds.end();
                }),
            bullets.end()
        );

        // Add or update bullets from server
        for (const auto& [bulletId, bulletData] : serverBullets) {
            // Check if we already have this bullet
            auto it = std::find_if(bullets.begin(), bullets.end(),
                [bulletId](const std::unique_ptr<Bullet>& bullet) {
                    return bullet->GetBulletId() == bulletId;
                });

            if (it != bullets.end()) {
                // Update existing bullet position (server correction)
                (*it)->position = sf::Vector2f(bulletData.x, bulletData.y);
                (*it)->velocity = sf::Vector2f(bulletData.velocityX, bulletData.velocityY);
                (*it)->rotation = bulletData.rotation;
            }
            else {
                // Create new bullet from server data
                CreateBulletFromServerData(bulletData);
            }
        }

        // Debug log periodically
        static float logTimer = 0;
        static float lastDt = 0.016f;
        logTimer += lastDt;
        if (logTimer >= 2.0f) {
 //           Utils::printMsg("Client bullets: " + std::to_string(bullets.size()) +
  //              " (Server: " + std::to_string(serverBullets.size()) + ")", debug);
            logTimer = 0;
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in SynchronizeBulletsFromServer: " + std::string(e.what()), error);
    }
}

/**
 * Creates a bullet from server bullet data
 */
void MultiplayerGame::CreateBulletFromServerData(const BulletData& data) {
    try {
        // Convert bullet type
        Bullet::BulletType bulletType = ConvertBulletType(data.bulletType);

        // Calculate direction from velocity
        sf::Vector2f velocity(data.velocityX, data.velocityY);
        float velLength = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);

        sf::Vector2f direction(1.0f, 0.0f);  // Default
        if (velLength > 0.001f) {
            direction = velocity / velLength;
        }

        // Create bullet
        auto bullet = std::make_unique<Bullet>(
            bulletType,
            sf::Vector2f(data.x, data.y),
            direction,
            data.ownerId
        );

        // Set server-assigned ID
        bullet->SetBulletId(data.bulletId);

        // Set velocity and rotation from server
        bullet->velocity = velocity;
        bullet->rotation = data.rotation;

        // Add to bullets
        bullets.push_back(std::move(bullet));

     //   Utils::printMsg("Created bullet " + std::to_string(data.bulletId) +
     //       " from server (owner: " + std::to_string(data.ownerId) + ")", debug);
    }
    catch (const std::exception& e) {
        Utils::printMsg("Exception in CreateBulletFromServerData: " + std::string(e.what()), error);
    }
}

/**
 * Converts network bullet type to Bullet::BulletType enum
 */
Bullet::BulletType MultiplayerGame::ConvertBulletType(uint8_t typeValue) {
    switch (typeValue) {
    case 0: return Bullet::BulletType::PLAYER_STANDARD;
    case 1: return Bullet::BulletType::ENEMY_STANDARD;
    case 2: return Bullet::BulletType::TANK_SHELL;
    case 3: return Bullet::BulletType::TRACER;
    default:
        Utils::printMsg("Unknown bullet type: " + std::to_string(typeValue), warning);
        return Bullet::BulletType::PLAYER_STANDARD;
    }
}