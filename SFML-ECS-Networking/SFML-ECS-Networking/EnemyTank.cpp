#include "EnemyTank.h"
#include "utils.h"
#include "HealthBarRenderer.h"  // For health bar visualization
#include <iostream>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>
#include <random>
#include <algorithm>

/**
 * Constructor for EnemyTank.
 * Initializes textures, sprites, stats based on enemy type.
 * @param type The type/variant of enemy tank
 * @param startPosition Initial spawn position in world
 */
EnemyTank::EnemyTank(EnemyType type, sf::Vector2f startPosition)
    : body(placeholder), barrel(placeholder), enemyType(type), position(startPosition),
    bodyRotation(sf::degrees(0)), barrelRotation(sf::degrees(0)),
    targetPosition(startPosition), collisionRadius(25.0f)
{
    // Initialize placeholder texture (1x1 white pixel)
    try {
        // Create a simple 4x4 white image using sf::Image
        sf::Image placeholderImage;
        placeholderImage.resize(sf::Vector2u(4, 4), sf::Color::White);

        if (!placeholder.loadFromImage(placeholderImage)) {
            Utils::printMsg("Warning: Failed to create enemy placeholder texture", warning);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception creating enemy placeholder - " + std::string(e.what()), error);
    }

    // Get color string for this enemy type
    colorString = GetColorStringFromType(type);

    // Initialize stats based on enemy type
    InitializeStats();

    // Load textures
    InitializeTextures();

    // Set texture rectangles - SFML 3.0 syntax
    try {
        sf::Vector2u bodySize = bodyTexture.getSize();
        sf::Vector2u barrelSize = barrelTexture.getSize();
        body.setTextureRect(sf::IntRect({ 0, 0 },
            { static_cast<int>(bodySize.x), static_cast<int>(bodySize.y) }));
        barrel.setTextureRect(sf::IntRect({ 0, 0 },
            { static_cast<int>(barrelSize.x), static_cast<int>(barrelSize.y) }));
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception setting enemy texture rectangles - " + std::string(e.what()), error);
    }

    // Set sprite origins - SFML 3.0 syntax
    try {
        sf::FloatRect bodyBounds = body.getLocalBounds();
        body.setOrigin({ bodyBounds.position.x + bodyBounds.size.x / 2.0f,
                        bodyBounds.position.y + bodyBounds.size.y / 2.0f });
        barrel.setOrigin({ 6.0f, 5.0f }); // Barrel mount point
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception setting enemy sprite origins - " + std::string(e.what()), error);
    }

    // Set initial positions and rotations
    try {
        body.setPosition(position);
        barrel.setPosition(position);
        body.setRotation(bodyRotation);
        barrel.setRotation(barrelRotation);
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception setting enemy initial transform - " + std::string(e.what()), error);
    }

    Utils::printMsg("Created " + GetEnemyTypeName() + " enemy tank at (" +
        std::to_string(position.x) + ", " + std::to_string(position.y) + ")");

    // Initialize health bar renderer
    // Initialize health bar renderer
    healthBarRenderer = std::make_unique<HealthBarRenderer>(50.0f, 6.0f, -40.0f);
    showHealthBar = true;

    // Initialize AI System - NEW
    InitializeAIParameters();
}

/**
 * Destructor for EnemyTank.
 */
EnemyTank::~EnemyTank() {
    // SFML resources clean up automatically
}

/**
 * Initializes enemy stats based on type.
 * Different enemy types have different health, speed, and score values.
 */
void EnemyTank::InitializeStats() {
    switch (enemyType) {
    case EnemyType::RED:
        // Basic enemy - balanced stats
        maxHealth = 100.0f;
        movementSpeed = 80.0f;
        rotationSpeed = 120.0f;
        scoreValue = 10;
        break;

    case EnemyType::BLACK:
        // Armored variant - high health, slow movement
        maxHealth = 200.0f;
        movementSpeed = 50.0f;
        rotationSpeed = 80.0f;
        scoreValue = 25;
        break;

    case EnemyType::PURPLE:
        // Fast variant - low health, high speed
        maxHealth = 60.0f;
        movementSpeed = 150.0f;
        rotationSpeed = 200.0f;
        scoreValue = 15;
        break;

    case EnemyType::ORANGE:
        // Heavy variant - very high health, very slow
        maxHealth = 300.0f;
        movementSpeed = 40.0f;
        rotationSpeed = 60.0f;
        scoreValue = 50;
        break;

    case EnemyType::TEAL:
        // Scout variant - medium stats
        maxHealth = 80.0f;
        movementSpeed = 120.0f;
        rotationSpeed = 150.0f;
        scoreValue = 12;
        break;

    default:
        // Fallback to basic stats
        maxHealth = 100.0f;
        movementSpeed = 80.0f;
        rotationSpeed = 120.0f;
        scoreValue = 10;
        break;
    }

    // Start at full health
    currentHealth = maxHealth;
    
    /*Utils::printMsg("Enemy stats: HP=" + std::to_string(maxHealth) +
        ", Speed=" + std::to_string(movementSpeed) +
        ", Score=" + std::to_string(scoreValue), debug);*/
}

/**
 * Initializes textures for this enemy type.
 * Loads body and barrel textures from Assets folder.
 */
void EnemyTank::InitializeTextures() {
    try {
        // Load body texture
        if (!bodyTexture.loadFromFile("Assets/" + colorString + "Tank.png")) {
            Utils::printMsg("Warning: Could not load enemy body texture: " + colorString, warning);
            body.setTexture(placeholder);
        }
        else {
            body.setTexture(bodyTexture);
            Utils::printMsg("Loaded enemy body texture: " + colorString);
        }

        // Load barrel texture
        if (!barrelTexture.loadFromFile("Assets/" + colorString + "Barrel.png")) {
            Utils::printMsg("Warning: Could not load enemy barrel texture: " + colorString, warning);
            barrel.setTexture(placeholder);
        }
        else {
            barrel.setTexture(barrelTexture);
            Utils::printMsg("Loaded enemy barrel texture: " + colorString);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception loading enemy textures - " + std::string(e.what()), error);
        body.setTexture(placeholder);
        barrel.setTexture(placeholder);
    }
}

/**
 * Converts enemy type enum to color string for texture loading.
 * @param type Enemy type enum
 * @return Color string matching texture filenames
 */
std::string EnemyTank::GetColorStringFromType(EnemyType type) const {
    switch (type) {
    case EnemyType::RED:    return "enemyRed";
    case EnemyType::BLACK:  return "enemyBlack";
    case EnemyType::PURPLE: return "enemyPurple";
    case EnemyType::ORANGE: return "enemyOrange";
    case EnemyType::TEAL:   return "enemyTeal";
    default:                return "enemyRed";
    }
}

/**
 * Gets human-readable name for enemy type.
 * @return Enemy type name as string
 */
std::string EnemyTank::GetEnemyTypeName() const {
    switch (enemyType) {
    case EnemyType::RED:    return "Red Enemy";
    case EnemyType::BLACK:  return "Black Armored";
    case EnemyType::PURPLE: return "Purple Fast";
    case EnemyType::ORANGE: return "Orange Heavy";
    case EnemyType::TEAL:   return "Teal Scout";
    default:                return "Unknown Enemy";
    }
}

/**
/**
 * Updates enemy tank state with full AI behavior.
 * Calls UpdateAIBehavior which handles state machine logic.
 * @param dt Delta time for frame-independent movement
 */
void EnemyTank::Update(float dt) {
    if (!IsValidDeltaTime(dt)) {
        Utils::printMsg("Warning: Invalid enemy delta time (" + std::to_string(dt) + ")", warning);
        return;
    }

    try {
        // Update shooting cooldown
        UpdateCooldown(dt);

        // Update AI behavior (state machine logic)
        UpdateAIBehavior(dt);

        // Update sprites with current position/rotation
        UpdateSprites();
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception in enemy Update - " + std::string(e.what()), error);
    }
}

/**
 * Updates sprite positions and rotations without movement logic.
 * Called after position/rotation changes from network or AI.
 */
void EnemyTank::UpdateSprites() {
    try {
        // Apply rotation to body and barrel
        body.setRotation(bodyRotation);
        barrel.setRotation(barrelRotation);

        // Apply position to body and barrel
        if (IsValidPosition(position)) {
            body.setPosition(position);
            barrel.setPosition(position);
        }
        else {
            Utils::printMsg("Warning: Invalid enemy position, skipping sprite update", warning);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception in enemy UpdateSprites - " + std::string(e.what()), error);
    }
}

/**
 * Renders the enemy tank to the provided window.
 * @param window The SFML render window
 */
void EnemyTank::Render(sf::RenderWindow& window) {
    if (!window.isOpen()) {
        Utils::printMsg("Error: Render window is not open for enemy", error);
        return;
    }

    try {
        window.draw(body);
        window.draw(barrel);

        // Render health bar above enemy tank
        if (showHealthBar && healthBarRenderer) {
            healthBarRenderer->Render(window, position, currentHealth, maxHealth);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception during enemy rendering - " + std::string(e.what()), error);
    }
}

/**
 * Applies damage to this enemy tank.
 * @param damage Amount of damage to apply (reduces health)
 */
void EnemyTank::TakeDamage(float damage) {
    if (damage < 0) {
        Utils::printMsg("Warning: Negative damage value (" + std::to_string(damage) + "), ignoring", warning);
        return;
    }

    currentHealth -= damage;

    // Clamp to 0 (can't go negative)
    if (currentHealth < 0) {
        currentHealth = 0;
    }

    //Utils::printMsg(GetEnemyTypeName() + " took " + std::to_string(damage) +
    //    " damage. Health: " + std::to_string(currentHealth) + "/" +
    //    std::to_string(maxHealth), debug);

    if (IsDead()) {
        Utils::printMsg(GetEnemyTypeName() + " destroyed! +" +
            std::to_string(scoreValue) + " points", success);
    }
}

/**
 * Heals this enemy tank.
 * @param amount Amount of health to restore
 */
void EnemyTank::Heal(float amount) {
    if (amount < 0) {
        Utils::printMsg("Warning: Negative heal value (" + std::to_string(amount) + "), ignoring", warning);
        return;
    }

    currentHealth += amount;

    // Clamp to max health
    if (currentHealth > maxHealth) {
        currentHealth = maxHealth;
    }

    //Utils::printMsg(GetEnemyTypeName() + " healed " + std::to_string(amount) +
    //    ". Health: " + std::to_string(currentHealth) + "/" +
    //    std::to_string(maxHealth), debug);
}

/**
 * Sets enemy position and updates sprites.
 * @param pos New position
 */
void EnemyTank::SetPosition(sf::Vector2f pos) {
    if (!IsValidPosition(pos)) {
        Utils::printMsg("Warning: Invalid enemy position set, ignoring", warning);
        return;
    }

    position = pos;
    UpdateSprites();
}

/**
 * Validates that delta time is positive and finite.
 * @param dt Delta time
 * @return True if valid, false otherwise
 */
bool EnemyTank::IsValidDeltaTime(float dt) const {
    return dt >= 0 && std::isfinite(dt);
}

/**
 * Validates that a position has finite coordinates.
 * @param pos The position to validate
 * @return True if valid, false otherwise
 */
bool EnemyTank::IsValidPosition(sf::Vector2f pos) const {
    return std::isfinite(pos.x) && std::isfinite(pos.y);
}
/**
 * Sets enemy health directly (for network synchronization).
 * Server is authoritative for enemy health, so clients sync from server state.
 * @param health New health value (clamped to [0, maxHealth])
 */
void EnemyTank::SetHealth(float health) {
    // Clamp health to valid range
    if (health < 0.0f) {
        currentHealth = 0.0f;
    }
    else if (health > maxHealth) {
        currentHealth = maxHealth;
    }
    else {
        currentHealth = health;
    }
}

/**
 * Sets enemy maximum health (for network synchronization).
 * @param maxHp New maximum health value
 */
void EnemyTank::SetMaxHealth(float maxHp) {
    if (maxHp <= 0.0f) {
        Utils::printMsg("Warning: Invalid maxHealth value (" + std::to_string(maxHp) + "), ignoring", warning);
        return;
    }
    maxHealth = maxHp;

    // Clamp current health if it exceeds new max
    if (currentHealth > maxHealth) {
        currentHealth = maxHealth;
    }
}
// AI SYSTEM IMPLEMENTATIONS 


/**
 * Sets the AI state and handles state transition logic.
 * Resets timers and performs any necessary state entry actions.
 * @param newState The new AI state to transition to
 */
void EnemyTank::SetAIState(AIState newState) {
    if (currentAIState == newState) {
        return; // Already in this state
    }

    // Log state transition
    Utils::printMsg(GetEnemyTypeName() + " AI state: " + GetAIStateName() +
        " -> " + GetAIStateName(), debug);

    previousAIState = currentAIState;
    currentAIState = newState;
    stateTimer = 0.0f;
    stateChangeTimer = 0.0f;

    // State entry actions
    switch (newState) {
    case AIState::IDLE:
        // Reset target when entering idle
        targetPlayerId = 0;
        break;

    case AIState::PATROL:
        // Generate first patrol waypoint
        GenerateNewPatrolWaypoint();
        patrolWaitTimer = 0.0f;
        break;

    case AIState::CHASE:
        // Start chasing - clear patrol wait
        patrolWaitTimer = 0.0f;
        break;

    case AIState::ATTACK:
        // Entering attack mode
        break;

    case AIState::RETREAT:
        // Start retreating
        Utils::printMsg(GetEnemyTypeName() + " is retreating! (Health: " +
            std::to_string(GetHealthPercentage() * 100.0f) + "%)", warning);
        break;
    }
}

/**
 * Gets human-readable name for current AI state.
 * @return AI state name as string
 */
std::string EnemyTank::GetAIStateName() const {
    switch (currentAIState) {
    case AIState::IDLE:    return "IDLE";
    case AIState::PATROL:  return "PATROL";
    case AIState::CHASE:   return "CHASE";
    case AIState::ATTACK:  return "ATTACK";
    case AIState::RETREAT: return "RETREAT";
    default:               return "UNKNOWN";
    }
}

// AI UTILITY FUNCTIONS
// Distance and Positioning

/**
 * Calculates the distance to a target position.
 * @param targetPos The target position
 * @return Distance in world units
 */
float EnemyTank::CalculateDistanceTo(sf::Vector2f targetPos) const {
    float dx = targetPos.x - position.x;
    float dy = targetPos.y - position.y;
    return std::sqrt(dx * dx + dy * dy);
}

/**
 * Calculates the angle to a target position in degrees.
 * Returns angle in range [0, 360) degrees.
 * @param targetPos The target position
 * @return Angle in degrees
 */
float EnemyTank::CalculateAngleTo(sf::Vector2f targetPos) const {
    float dx = targetPos.x - position.x;
    float dy = targetPos.y - position.y;
    float angleRad = std::atan2(dy, dx);
    float angleDeg = angleRad * 180.0f / 3.14159265f;

    // Normalize to [0, 360)
    while (angleDeg < 0.0f) angleDeg += 360.0f;
    while (angleDeg >= 360.0f) angleDeg -= 360.0f;

    return angleDeg;
}

/**
 * Gets the normalized direction vector to a target position.
 * @param targetPos The target position
 * @return Normalized direction vector
 */
sf::Vector2f EnemyTank::GetDirectionTo(sf::Vector2f targetPos) const {
    sf::Vector2f direction = targetPos - position;
    float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);

    if (length < 0.001f) {
        return sf::Vector2f(0.0f, 0.0f); // Avoid division by zero
    }

    return direction / length;
}

// AI DECISION-MAKING FUNCTIONS

/**
 * Checks if the current target is within a specified range.
 * @param range The range to check
 * @return True if target is within range, false otherwise
 */
bool EnemyTank::IsTargetInRange(float range) const {
    if (!HasTarget()) {
        return false;
    }

    float distance = CalculateDistanceTo(lastKnownTargetPos);
    return distance <= range;
}

/**
 * Determines if the enemy should retreat based on health.
 * @return True if should retreat, false otherwise
 */
bool EnemyTank::ShouldRetreat() const {
    return GetHealthPercentage() <= retreatHealthThreshold;
}

/**
 * Determines if the enemy should chase the current target.
 * @return True if should chase, false otherwise
 */
bool EnemyTank::ShouldChaseTarget() const {
    if (!HasTarget()) {
        return false;
    }

    // Chase if target is within detection range
    float distance = CalculateDistanceTo(lastKnownTargetPos);
    return distance <= detectionRange;
}

/**
 * Sets a specific player as the target.
 * This is called by the game server when it determines which player should be targeted.
 * @param playerId The ID of the player to target
 * @param playerPos The current position of that player
 */
void EnemyTank::SelectNewTarget(uint32_t playerId, sf::Vector2f playerPos) {
    if (playerId == 0) {
        ClearTarget();
        return;
    }

    targetPlayerId = playerId;
    lastKnownTargetPos = playerPos;
    targetLostTimer = 0.0f;

    //Utils::printMsg(GetEnemyTypeName() + " targeting player " +
    //    std::to_string(playerId) + " at (" +
    //    std::to_string(playerPos.x) + ", " +
    //    std::to_string(playerPos.y) + ")", debug);
}

/**
 * Clears the current target.
 */
void EnemyTank::ClearTarget() {
    //if (targetPlayerId != 0) {
    //    Utils::printMsg(GetEnemyTypeName() + " clearing target " +
    //        std::to_string(targetPlayerId), debug);
    //}
    targetPlayerId = 0;
    lastKnownTargetPos = position;
    targetLostTimer = 0.0f;
}

// PATROL HELPERS

/**
 * Generates a new random patrol waypoint within world bounds.
 */
void EnemyTank::GenerateNewPatrolWaypoint() {
    // Use static random for consistency
    static std::random_device rd;
    static std::mt19937 gen(rd());

    // Generate waypoint within safe patrol area (avoiding borders)
    // World is 1280x960, border is 48, tank radius is 25
    // Safe spawn: 83 to 1197 (X), 83 to 877 (Y) with extra 10px margin
    std::uniform_real_distribution<float> distX(100.0f, 1180.0f);  // Safe X range
    std::uniform_real_distribution<float> distY(100.0f, 860.0f);   // Safe Y range

    patrolWaypoint.x = distX(gen);
    patrolWaypoint.y = distY(gen);

    //Utils::printMsg(GetEnemyTypeName() + " new patrol waypoint: (" +
    //    std::to_string(patrolWaypoint.x) + ", " +
    //    std::to_string(patrolWaypoint.y) + ")", debug);
}

/**
 * Checks if the enemy has reached the current patrol waypoint.
 * @return True if waypoint reached, false otherwise
 */
bool EnemyTank::HasReachedWaypoint() const {
    float distance = CalculateDistanceTo(patrolWaypoint);
    return distance <= waypointReachedDistance;
}

// MOVEMENT AND ROTATION FUNCTIONS

/**
 * Smoothly rotates the tank body towards a target position.
 * @param targetPos The position to rotate towards
 * @param dt Delta time
 */
void EnemyTank::RotateTowards(sf::Vector2f targetPos, float dt) {
    float targetAngle = CalculateAngleTo(targetPos);
    float currentAngle = bodyRotation.asDegrees();

    // Normalize angles to [0, 360)
    while (currentAngle < 0.0f) currentAngle += 360.0f;
    while (currentAngle >= 360.0f) currentAngle -= 360.0f;

    // Calculate shortest rotation direction
    float angleDiff = targetAngle - currentAngle;
    if (angleDiff > 180.0f) angleDiff -= 360.0f;
    if (angleDiff < -180.0f) angleDiff += 360.0f;

    // Smoothly rotate towards target
    float rotationStep = rotationSpeed * dt;
    if (std::abs(angleDiff) < rotationStep) {
        // Close enough, snap to target angle
        bodyRotation = sf::degrees(targetAngle);
    }
    else {
        // Rotate by step amount in the correct direction
        if (angleDiff > 0) {
            bodyRotation = sf::degrees(currentAngle + rotationStep);
        }
        else {
            bodyRotation = sf::degrees(currentAngle - rotationStep);
        }
    }
}

/**
 * Moves the tank towards a target position.
 * @param targetPos The position to move towards
 * @param dt Delta time
 */
void EnemyTank::MoveTowards(sf::Vector2f targetPos, float dt) {
    // First rotate towards target
    RotateTowards(targetPos, dt);

    // Calculate distance to target
    float distance = CalculateDistanceTo(targetPos);

    // Only move if not already at target
    if (distance > waypointReachedDistance) {
        // Move forward in the direction we're facing
        float radians = bodyRotation.asDegrees() * 3.14159265f / 180.0f;
        float dirX = std::cos(radians);
        float dirY = std::sin(radians);

        // Apply movement
        position.x += dirX * movementSpeed * dt;
        position.y += dirY * movementSpeed * dt;

        // Clamp to world bounds: 73 to 1207 (X), 73 to 887 (Y)
        // BORDER_THICKNESS (48) + TANK_RADIUS (25) = 73
        const float MIN_X = 73.0f;
        const float MAX_X = 1207.0f; // 1280 - 48 - 25
        const float MIN_Y = 73.0f;
        const float MAX_Y = 887.0f;  // 960 - 48 - 25

        position.x = std::max(MIN_X, std::min(MAX_X, position.x));
        position.y = std::max(MIN_Y, std::min(MAX_Y, position.y));
    }
}

/**
 * Moves the tank away from a threat position.
 * @param threatPos The position to move away from
 * @param dt Delta time
 */
void EnemyTank::MoveAwayFrom(sf::Vector2f threatPos, float dt) {
    // Calculate opposite direction
    sf::Vector2f awayVector = position - threatPos;

    // Normalize
    float length = std::sqrt(awayVector.x * awayVector.x + awayVector.y * awayVector.y);
    if (length < 0.001f) {
        // Too close, pick a random direction
        awayVector = sf::Vector2f(1.0f, 0.0f);
    }
    else {
        awayVector /= length;
    }

    // Calculate target position far away
    sf::Vector2f retreatTarget = position + awayVector * 300.0f;

    // Move towards that position
    MoveTowards(retreatTarget, dt);
}

// AI INITIALIZATION

/**
 * Initializes AI parameters based on enemy type.
 * Different enemy types have different AI personalities.
 */
void EnemyTank::InitializeAIParameters() {
    // Common defaults
    targetPlayerId = 0;
    lastKnownTargetPos = position;
    targetLostTimer = 0.0f;
    targetScanTimer = 0.0f;
    targetScanInterval = 1.0f;  // Scan for targets every 1 second
    stateChangeTimer = 0.0f;
    stateTimer = 0.0f;
    waypointReachedDistance = 50.0f;
    patrolWaitTimer = 0.0f;
    patrolWaitDuration = 2.0f;  // Wait 2 seconds at each waypoint

    // Set initial state
    currentAIState = AIState::PATROL;
    previousAIState = AIState::IDLE;

    // Type-specific AI parameters
    switch (enemyType) {
    case EnemyType::RED:
        // Balanced enemy - standard parameters
        detectionRange = 400.0f;
        attackRange = 250.0f;
        retreatHealthThreshold = 0.3f;  // 30% health
        aggressionLevel = 0.5f;
        break;

    case EnemyType::BLACK:
        // Armored tank - defensive, holds ground
        detectionRange = 350.0f;
        attackRange = 300.0f;
        retreatHealthThreshold = 0.2f;  // 20% health (tanky)
        aggressionLevel = 0.3f;
        break;

    case EnemyType::PURPLE:
        // Fast scout - hit and run tactics
        detectionRange = 500.0f;
        attackRange = 200.0f;
        retreatHealthThreshold = 0.5f;  // 50% health (fragile)
        aggressionLevel = 0.7f;
        break;

    case EnemyType::ORANGE:
        // Heavy assault - aggressive and slow
        detectionRange = 300.0f;
        attackRange = 350.0f;
        retreatHealthThreshold = 0.15f; // 15% health (tough)
        aggressionLevel = 0.8f;
        break;

    case EnemyType::TEAL:
        // Scout - balanced mobility and range
        detectionRange = 450.0f;
        attackRange = 220.0f;
        retreatHealthThreshold = 0.4f;  // 40% health
        aggressionLevel = 0.6f;
        break;
    }

    // Generate initial patrol waypoint
    GenerateNewPatrolWaypoint();

    // Initialize shooting parameters (Phase 3)
    InitializeShootingParameters();

    Utils::printMsg(GetEnemyTypeName() + " AI initialized - Detection: " +
        std::to_string(detectionRange) + ", Attack: " +
        std::to_string(attackRange) + ", Aggression: " +
        std::to_string(aggressionLevel), success);
}
//  MOVEMENT & NAVIGATION
// State Update Implementations

/**
 * Main AI behavior update function.
 * Handles state machine logic and calls state-specific updates.
 * @param dt Delta time
 */
void EnemyTank::UpdateAIBehavior(float dt) {
    // Update timers
    stateTimer += dt;
    targetScanTimer += dt;
    stateChangeTimer += dt;
    targetLostTimer += dt;

    // Check if we should retreat (low health)
    if (ShouldRetreat() && currentAIState != AIState::RETREAT) {
        SetAIState(AIState::RETREAT);
    }

    // Update based on current state
    switch (currentAIState) {
    case AIState::IDLE:
        UpdateIdleState(dt);
        break;
    case AIState::PATROL:
        UpdatePatrolState(dt);
        break;
    case AIState::CHASE:
        UpdateChaseState(dt);
        break;
    case AIState::ATTACK:
        UpdateAttackState(dt);
        break;
    case AIState::RETREAT:
        UpdateRetreatState(dt);
        break;
    }

    // Aim barrel at target if we have one
    if (HasTarget()) {
        float targetAngle = CalculateAngleTo(lastKnownTargetPos);
        barrelRotation = sf::degrees(targetAngle);
    }
}

/**
 * Updates IDLE state behavior.
 * Enemy stands still and scans for threats.
 * @param dt Delta time
 */
void EnemyTank::UpdateIdleState(float dt) {
    // Rotate barrel slowly while scanning
    float currentBarrel = barrelRotation.asDegrees();
    barrelRotation = sf::degrees(currentBarrel + 20.0f * dt);

    // After being idle for a while, start patrolling
    if (stateTimer > 3.0f) {
        SetAIState(AIState::PATROL);
    }
}

/**
 * Updates PATROL state behavior.
 * Enemy moves to random waypoints around the map.
 * @param dt Delta time
 */
void EnemyTank::UpdatePatrolState(float dt) {
     
    // CHECK FOR TARGETS WHILE PATROLLING
    // If we have a target and they're in detection range, start chasing
    if (HasTarget()) {
        float distanceToTarget = CalculateDistanceTo(lastKnownTargetPos);

        // Target is within detection range - start chasing!
        if (distanceToTarget <= detectionRange) {
            //Utils::printMsg(GetEnemyTypeName() + " detected target at " +
            //    std::to_string(distanceToTarget) + " units - CHASING!", success);
            SetAIState(AIState::CHASE);
            return;  // Exit patrol, start chasing
        }
    }


    // Check if we've reached the current waypoint
    if (HasReachedWaypoint()) {
        // Wait at waypoint
        patrolWaitTimer += dt;

        if (patrolWaitTimer >= patrolWaitDuration) {
            // Generate new waypoint and continue patrol
            GenerateNewPatrolWaypoint();
            patrolWaitTimer = 0.0f;
        }

        // Rotate barrel slowly while waiting
        float currentBarrel = barrelRotation.asDegrees();
        barrelRotation = sf::degrees(currentBarrel + 30.0f * dt);
    }
    else {
        // Move towards waypoint with obstacle avoidance
        MoveTowardsWithAvoidance(patrolWaypoint, dt);

        // Aim barrel in movement direction
        float moveAngle = CalculateAngleTo(patrolWaypoint);
        barrelRotation = sf::degrees(moveAngle);
    }
}


/**
 * Updates CHASE state behavior.
 * Enemy pursues the target player.
 * @param dt Delta time
 */
void EnemyTank::UpdateChaseState(float dt) {
    if (!HasTarget()) {
        //Utils::printMsg(GetEnemyTypeName() + " lost target - returning to patrol", debug);
        //SetAIState(AIState::PATROL);
        return;
    }

    float distanceToTarget = CalculateDistanceTo(lastKnownTargetPos);

    //  Use ENTER threshold (0.7x) to switch to ATTACK
    // This matches the EXIT threshold (1.5x) in UpdateAttackState
    const float ATTACK_ENTER_RANGE = attackRange * 0.7f;

    // Enter ATTACK mode earlier (at 70% of attack range)
    if (distanceToTarget <= ATTACK_ENTER_RANGE) {
        //Utils::printMsg(GetEnemyTypeName() + " in attack range (" +
        //    std::to_string(distanceToTarget) + " <= " +
        //    std::to_string(ATTACK_ENTER_RANGE) + ") - ATTACKING!", success);
        SetAIState(AIState::ATTACK);
        return;
    }

    // Check if target escaped detection range
    if (distanceToTarget > detectionRange * 1.5f) {
        //Utils::printMsg(GetEnemyTypeName() + " target out of range (" +
        //    std::to_string(distanceToTarget) + " > " +
        //    std::to_string(detectionRange * 1.5f) + ") - lost target", warning);
        ClearTarget();
        SetAIState(AIState::PATROL);
        return;
    }

    // Continue chasing
    MoveTowardsWithAvoidance(lastKnownTargetPos, dt);

    // Aim barrel at target while chasing
    float targetAngle = CalculateAngleTo(lastKnownTargetPos);
    barrelRotation = sf::degrees(targetAngle);
}

/**
 * Updates ATTACK state behavior.
 * Enemy attacks the target while maintaining optimal distance.
 * @param dt Delta time
 */
void EnemyTank::UpdateAttackState(float dt) {
    if (!HasTarget()) {
        //Utils::printMsg(GetEnemyTypeName() + " lost target in ATTACK - returning to patrol", warning);
        SetAIState(AIState::PATROL);
        return;
    }

    float distanceToTarget = CalculateDistanceTo(lastKnownTargetPos);


    // Use DIFFERENT thresholds for entering vs exiting ATTACK state
    const float ATTACK_EXIT_RANGE = attackRange * 1.5f;  // Stay in ATTACK until 1.5x range
    const float ATTACK_ENTER_RANGE = attackRange * 0.7f; // Enter ATTACK at 0.7x range

    // Only exit ATTACK if target is MUCH farther than attack range
    if (distanceToTarget > ATTACK_EXIT_RANGE) {
        //Utils::printMsg(GetEnemyTypeName() + " target too far (" +
        //    std::to_string(distanceToTarget) + " > " +
        //    std::to_string(ATTACK_EXIT_RANGE) + ") - resuming chase", debug);
        SetAIState(AIState::CHASE);
        return;
    }

    // Range Management 
    // Maintain Optimal Distance
    const float OPTIMAL_MIN = attackRange * 0.6f;  // Too close
    const float OPTIMAL_MAX = attackRange * 1.1f;  // Too far

    if (distanceToTarget < OPTIMAL_MIN) {
        // Too close - back up slowly
        MoveAwayFrom(lastKnownTargetPos, dt);
    }
    else if (distanceToTarget > OPTIMAL_MAX) {
        // Too far - move closer
        MoveTowardsWithAvoidance(lastKnownTargetPos, dt);
    }
    else {
        // Optimal range - just rotate to face target (don't move)
        RotateTowards(lastKnownTargetPos, dt);
    }

    // Always aim barrel at target in attack mode
    float targetAngle = CalculateAngleTo(lastKnownTargetPos);
    barrelRotation = sf::degrees(targetAngle);



    // Calculate aim accuracy
    float barrelAngle = barrelRotation.asDegrees();
    while (barrelAngle < 0.0f) barrelAngle += 360.0f;
    while (barrelAngle >= 360.0f) barrelAngle -= 360.0f;

    float targetAngleDeg = targetAngle;
    while (targetAngleDeg < 0.0f) targetAngleDeg += 360.0f;
    while (targetAngleDeg >= 360.0f) targetAngleDeg -= 360.0f;

    float angleDiff = std::abs(targetAngleDeg - barrelAngle);
    if (angleDiff > 180.0f) {
        angleDiff = 360.0f - angleDiff;
    }

    // DYNAMIC AIM THRESHOLD based on distance
    // Closer targets = stricter aim (they're easier to hit)
    // Farther targets = looser aim (compensate for movement)
    float aimThreshold = 45.0f;  // Base threshold
    if (distanceToTarget > attackRange * 0.8f) {
        aimThreshold = 75.0f;  // Very loose for far targets
    }
    else if (distanceToTarget > attackRange * 0.5f) {
        aimThreshold = 60.0f;  // Medium for mid-range
    }

    // Log aiming info every 0.5 seconds
    static std::unordered_map<uint32_t, float> aimLogTimers;
    static uint32_t enemyIdCounter = 0;
    uint32_t logId = enemyIdCounter++;
    aimLogTimers[logId] += dt;

    if (aimLogTimers[logId] >= 0.5f) {
        //Utils::printMsg("ATTACK: " + GetEnemyTypeName() +
        //    " | Dist=" + std::to_string(distanceToTarget) +
        //    " | AimDiff=" + std::to_string(angleDiff) + "°" +
        //    " | Threshold=" + std::to_string(aimThreshold) + "°" +
        //    " | Cooldown=" + std::to_string(GetShootCooldown()) + "s", info);
        aimLogTimers[logId] = 0.0f;
    }


    bool aimIsGoodEnough = (angleDiff <= aimThreshold);
    bool cooldownReady = CanShoot();

    if (aimIsGoodEnough && cooldownReady) {
        // TRY to shoot - TryShoot() handles burst logic internally
        if (TryShoot()) {
            //Utils::printMsg(GetEnemyTypeName() + " FIRED at distance " +
            //    std::to_string(distanceToTarget) + "! (Aim: " +
            //    std::to_string(angleDiff) + "°)", success);
        }
        else {
            // TryShoot failed (shouldn't happen if CanShoot() was true)
            Utils::printMsg(GetEnemyTypeName() + " TryShoot() returned FALSE (logic error)", error);
        }
    }
    else if (!aimIsGoodEnough) {
        // Aim not ready - this is normal, just keep aiming
        static std::unordered_map<uint32_t, float> noAimLogTimers;
        noAimLogTimers[logId] += dt;
        if (noAimLogTimers[logId] >= 2.0f) {
            //Utils::printMsg(GetEnemyTypeName() + " aiming... (off by " +
            //    std::to_string(angleDiff) + "°, need <" +
            //    std::to_string(aimThreshold) + "°)", debug);
            noAimLogTimers[logId] = 0.0f;
        }
    }
    else if (!cooldownReady) {
        // On cooldown - this is normal after shooting
        static std::unordered_map<uint32_t, float> cooldownLogTimers;
        cooldownLogTimers[logId] += dt;
        if (cooldownLogTimers[logId] >= 1.0f) {
            //Utils::printMsg(GetEnemyTypeName() + " on cooldown: " +
            //    std::to_string(GetShootCooldown()) + "s remaining", debug);
            cooldownLogTimers[logId] = 0.0f;
        }
    }
}

/**
 * Updates RETREAT state behavior.
 * Enemy moves away from threats when health is low.
 * @param dt Delta time
 */
void EnemyTank::UpdateRetreatState(float dt) {
    // If health recovered above threshold, return to patrol
    if (!ShouldRetreat()) {
        //Utils::printMsg(GetEnemyTypeName() + " health recovered, resuming patrol", success);
        SetAIState(AIState::PATROL);
        return;
    }

    //  Check if we're stuck at a boundary
    const float BOUNDARY_STUCK_THRESHOLD = 50.0f;  // Within 50px of edge = stuck
    bool stuckAtBoundary = false;

    // Check distance to each boundary
    float distToLeft = position.x - WorldConstants::MOVEMENT_MIN_X;
    float distToRight = WorldConstants::MOVEMENT_MAX_X - position.x;
    float distToTop = position.y - WorldConstants::MOVEMENT_MIN_Y;
    float distToBottom = WorldConstants::MOVEMENT_MAX_Y - position.y;

    // Find minimum distance to any boundary
    float minBoundaryDist = std::min({ distToLeft, distToRight, distToTop, distToBottom });

    if (minBoundaryDist < BOUNDARY_STUCK_THRESHOLD) {
        stuckAtBoundary = true;
    }

    if (HasTarget()) {
        // We have a threat - run away from it

        //  If stuck at boundary, pick a new safe position instead
        if (stuckAtBoundary) {
            // We're at the edge and can't retreat further
            // Pick a new retreat position that's:
            // 1. Away from current boundary
            // 2. Away from the threat
            // 3. Within safe movement area

            sf::Vector2f retreatTarget = CalculateSafeRetreatPosition(lastKnownTargetPos);
            MoveTowardsWithAvoidance(retreatTarget, dt);

            //Utils::printMsg(GetEnemyTypeName() + " repositioning from boundary to (" +
            //    std::to_string(retreatTarget.x) + ", " + std::to_string(retreatTarget.y) + ")",
            //    debug);
        }
        else {
            // Normal retreat - move away from threat
            MoveAwayFrom(lastKnownTargetPos, dt);
        }

        // Keep barrel aimed at threat (defensive posture)
        float threatAngle = CalculateAngleTo(lastKnownTargetPos);
        barrelRotation = sf::degrees(threatAngle);
    }
    else {
        // No specific threat known

        //  If stuck at boundary, pick new position
        if (stuckAtBoundary) {
            // Generate a safe position away from edges
            sf::Vector2f safeTarget = GenerateSafeInteriorPosition();
            MoveTowardsWithAvoidance(safeTarget, dt);

            //Utils::printMsg(GetEnemyTypeName() + " moving to safe interior position: (" +
            //    std::to_string(safeTarget.x) + ", " + std::to_string(safeTarget.y) + ")",
            //    debug);
        }
        else {
            // Move to a safe corner (improved logic with boundary check)
            sf::Vector2f safeCorner = SelectSafeCorner();
            MoveTowardsWithAvoidance(safeCorner, dt);
        }
    }
}

//  OBSTACLE AVOIDANCE 
// Enhanced Movement Functions

/**
 * Checks if a position is safe (within world bounds with margin).
 * @param pos Position to check
 * @return True if position is safe, false if near boundaries
 */
bool EnemyTank::IsPositionSafe(sf::Vector2f pos) const {
    const float BOUNDARY_MARGIN = 80.0f;  // Stay this far from edges

    // Use actual world dimensions: 1280x960
    return (pos.x > BOUNDARY_MARGIN && pos.x < 1280.0f - BOUNDARY_MARGIN &&
        pos.y > BOUNDARY_MARGIN && pos.y < 960.0f - BOUNDARY_MARGIN);
}

/**
 * Enhanced MoveTowards with obstacle avoidance.
 * Replaces the basic version from Phase 1.
 * @param targetPos The position to move towards
 * @param dt Delta time
 */
void EnemyTank::MoveTowardsWithAvoidance(sf::Vector2f targetPos, float dt) {
    // First rotate towards target
    RotateTowards(targetPos, dt);

    // Calculate distance to target
    float distance = CalculateDistanceTo(targetPos);

    // Only move if not already at target
    if (distance > waypointReachedDistance) {
        // Calculate intended next position
        float radians = bodyRotation.asDegrees() * 3.14159265f / 180.0f;
        float dirX = std::cos(radians);
        float dirY = std::sin(radians);

        sf::Vector2f intendedPos;
        intendedPos.x = position.x + dirX * movementSpeed * dt;
        intendedPos.y = position.y + dirY * movementSpeed * dt;

        // Check if intended position is safe
        if (IsPositionSafe(intendedPos)) {
            // Safe to move
            position = intendedPos;
        }
        else {
            // Near boundary - try to steer away
            // World center: 1280x960 → center at (640, 480)
            sf::Vector2f centerOfMap(640.0f, 480.0f);

            // If we're near a boundary, add a steering force towards center
            sf::Vector2f toCenter = centerOfMap - position;
            float toCenterLength = std::sqrt(toCenter.x * toCenter.x + toCenter.y * toCenter.y);

            if (toCenterLength > 0.001f) {
                toCenter /= toCenterLength;  // Normalize

                // Blend target direction with center direction (more towards center near edges)
                // Calculate distance to nearest edge
                float edgeDistance = 1280.0f / 2.0f - std::abs(position.x - 640.0f);
                edgeDistance = std::min(edgeDistance, 960.0f / 2.0f - std::abs(position.y - 480.0f));
                float centerWeight = std::max(0.0f, 1.0f - (edgeDistance / 200.0f));

                sf::Vector2f blendedDir;
                blendedDir.x = dirX * (1.0f - centerWeight) + toCenter.x * centerWeight;
                blendedDir.y = dirY * (1.0f - centerWeight) + toCenter.y * centerWeight;

                // Normalize blended direction
                float blendLength = std::sqrt(blendedDir.x * blendedDir.x + blendedDir.y * blendedDir.y);
                if (blendLength > 0.001f) {
                    blendedDir /= blendLength;
                }

                // Apply blended movement
                position.x += blendedDir.x * movementSpeed * dt * 0.7f;  // Slower when avoiding
                position.y += blendedDir.y * movementSpeed * dt * 0.7f;
            }
        }

        // CRITICAL: Final safety clamp using WorldConstants values
        // Movement bounds: PLAYABLE - TANK_RADIUS = 48 + 25 = 73 to (1280-48-25) = 1207
        const float MIN_X = 73.0f;  // BORDER_THICKNESS + TANK_RADIUS
        const float MAX_X = 1207.0f; // WORLD_WIDTH - BORDER_THICKNESS - TANK_RADIUS
        const float MIN_Y = 73.0f;  // BORDER_THICKNESS + TANK_RADIUS
        const float MAX_Y = 887.0f; // WORLD_HEIGHT - BORDER_THICKNESS - TANK_RADIUS

        position.x = std::max(MIN_X, std::min(MAX_X, position.x));
        position.y = std::max(MIN_Y, std::min(MAX_Y, position.y));
    }
}

/**
 * Predicts if the enemy will collide with world boundaries soon.
 * Used for proactive obstacle avoidance.
 * @param lookAheadTime How far ahead to check (seconds)
 * @return True if collision predicted, false otherwise
 */
bool EnemyTank::WillCollideWithBoundary(float lookAheadTime) const {
    // Calculate future position
    float radians = bodyRotation.asDegrees() * 3.14159265f / 180.0f;
    float dirX = std::cos(radians);
    float dirY = std::sin(radians);

    sf::Vector2f futurePos;
    futurePos.x = position.x + dirX * movementSpeed * lookAheadTime;
    futurePos.y = position.y + dirY * movementSpeed * lookAheadTime;

    // Check if future position is unsafe
    return !IsPositionSafe(futurePos);
}

/**
 * Gets a safe direction to avoid obstacles.
 * Returns a direction vector that steers away from boundaries.
 * @return Safe direction vector (normalized)
 */
sf::Vector2f EnemyTank::GetSafeDirection() const {
    sf::Vector2f safeDir(0.0f, 0.0f);

    const float BOUNDARY_MARGIN = 150.0f;
    float repulsionStrength = 200.0f;

    // Check distance to each boundary and add repulsion forces
    // World dimensions: 1280x960

    // Left boundary
    if (position.x < BOUNDARY_MARGIN) {
        float repulsion = (BOUNDARY_MARGIN - position.x) / BOUNDARY_MARGIN;
        safeDir.x += repulsion * repulsionStrength;
    }

    // Right boundary (1280)
    if (position.x > 1280.0f - BOUNDARY_MARGIN) {
        float repulsion = (position.x - (1280.0f - BOUNDARY_MARGIN)) / BOUNDARY_MARGIN;
        safeDir.x -= repulsion * repulsionStrength;
    }

    // Top boundary
    if (position.y < BOUNDARY_MARGIN) {
        float repulsion = (BOUNDARY_MARGIN - position.y) / BOUNDARY_MARGIN;
        safeDir.y += repulsion * repulsionStrength;
    }

    // Bottom boundary (960)
    if (position.y > 960.0f - BOUNDARY_MARGIN) {
        float repulsion = (position.y - (960.0f - BOUNDARY_MARGIN)) / BOUNDARY_MARGIN;
        safeDir.y -= repulsion * repulsionStrength;
    }

    // Normalize
    float length = std::sqrt(safeDir.x * safeDir.x + safeDir.y * safeDir.y);
    if (length > 0.001f) {
        safeDir /= length;
    }
    else {
        // No boundary pressure, return forward direction
        float radians = bodyRotation.asDegrees() * 3.14159265f / 180.0f;
        safeDir.x = std::cos(radians);
        safeDir.y = std::sin(radians);
    }

    return safeDir;
}
//  COMBAT INTELLIGENCE 
// Shooting System Implementation

/**
 * Updates the shooting cooldown timer.
 * @param dt Delta time
 */
void EnemyTank::UpdateCooldown(float dt) {
    if (shootCooldown > 0.0f) {
        shootCooldown -= dt;
        if (shootCooldown < 0.0f) {
            shootCooldown = 0.0f;
        }
    }

    lastShotTime += dt;
}

/**
 * Calculates the position at the end of the barrel.
 * This is where bullets spawn from.
 * @return Position at barrel tip
 */
sf::Vector2f EnemyTank::GetBarrelEndPosition() const {
    try {
        // Calculate barrel direction
        float barrelRad = barrelRotation.asDegrees() * 3.14159265f / 180.0f;

        // Calculate offset from tank center to barrel end
        float offsetX = std::cos(barrelRad) * barrelLength;
        float offsetY = std::sin(barrelRad) * barrelLength;

        // Return position at barrel end
        return sf::Vector2f(position.x + offsetX, position.y + offsetY);
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception in Enemy GetBarrelEndPosition - " + std::string(e.what()), error);
        return position;  // Fallback to tank center
    }
}

/**
 * Gets the current aim direction as a normalized vector.
 * @return Direction vector where barrel is pointing
 */
sf::Vector2f EnemyTank::GetAimDirection() const {
    float barrelRad = barrelRotation.asDegrees() * 3.14159265f / 180.0f;
    return sf::Vector2f(std::cos(barrelRad), std::sin(barrelRad));
}

/**
 * Applies accuracy spread to a direction vector.
 * Adds random deviation based on enemy's accuracy stat.
 * @param direction Base direction vector (normalized)
 * @return Modified direction with accuracy spread applied
 */
sf::Vector2f EnemyTank::ApplyAccuracySpread(sf::Vector2f direction) const {
    // Perfect accuracy (1.0) means no spread
    // Poor accuracy (0.0) means maximum spread
    float spreadAmount = (1.0f - baseAccuracy) * accuracySpreadAngle;

    if (spreadAmount < 0.01f) {
        // Nearly perfect accuracy, no spread needed
        return direction;
    }

    // Generate random angle offset within spread range
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> spreadDist(-spreadAmount, spreadAmount);

    float angleOffset = spreadDist(gen);
    float angleRad = angleOffset * 3.14159265f / 180.0f;

    // Apply rotation to direction vector
    float cosAngle = std::cos(angleRad);
    float sinAngle = std::sin(angleRad);

    sf::Vector2f newDirection;
    newDirection.x = direction.x * cosAngle - direction.y * sinAngle;
    newDirection.y = direction.x * sinAngle + direction.y * cosAngle;

    // Normalize (should already be normalized, but ensure it)
    float length = std::sqrt(newDirection.x * newDirection.x + newDirection.y * newDirection.y);
    if (length > 0.001f) {
        newDirection /= length;
    }

    return newDirection;
}

/**
 * Attempts to fire a shot.
 * Checks cooldown and manages burst fire.
 * @return True if shot was fired, false if on cooldown
 */
bool EnemyTank::TryShoot() {
    // Check if we can shoot
    if (!CanShoot()) {
        return false;
    }

    // Check if we're in attack state (only shoot when attacking)
    if (currentAIState != AIState::ATTACK) {
        return false;
    }

    // Start cooldown
    shootCooldown = shootCooldownTime;
    lastShotTime = 0.0f;

    // Track burst fire
    shotsInBurst++;
    if (shotsInBurst >= maxBurstSize) {
        // End of burst, add extra cooldown
        shootCooldown *= 1.5f;
        shotsInBurst = 0;
    }

    //Utils::printMsg(GetEnemyTypeName() + " fired! (Burst: " +
    //    std::to_string(shotsInBurst) + "/" +
    //    std::to_string(maxBurstSize) + ")", debug);

    return true;
}

/**
 * Calculates a lead target position for moving targets.
 * Predicts where the target will be by the time bullet arrives.
 * @param targetPos Current target position
 * @param targetVel Target velocity vector
 * @param bulletSpeed Speed of the bullet
 * @return Predicted lead position
 */
sf::Vector2f EnemyTank::CalculateLeadTarget(sf::Vector2f targetPos, sf::Vector2f targetVel, float bulletSpeed) const {
    // Calculate time for bullet to reach current target position
    sf::Vector2f toTarget = targetPos - position;
    float distance = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
    float timeToImpact = distance / bulletSpeed;

    // Predict where target will be
    sf::Vector2f predictedPos;
    predictedPos.x = targetPos.x + targetVel.x * timeToImpact;
    predictedPos.y = targetPos.y + targetVel.y * timeToImpact;

    // Clamp prediction to world bounds
    predictedPos.x = std::max(100.0f, std::min(1180.0f, predictedPos.x));
    predictedPos.y = std::max(100.0f, std::min(860.0f, predictedPos.y));

    return predictedPos;
}

/**
 * Determines if enemy should shoot at current target.
 * Checks line of sight, distance, and aim accuracy.
 * @return True if should shoot, false otherwise
 */
bool EnemyTank::ShouldShootAtTarget() const {
    // Must have a target
    if (!HasTarget()) {
        return false;
    }

    // Must be in attack state
    if (currentAIState != EnemyTank::AIState::ATTACK) {
        return false;
    }

    // Must be off cooldown
    if (!CanShoot()) {
        return false;
    }

    // All basic checks passed - caller (UpdateAttackState) will check aim
    return true;
}
/**
 * Initializes shooting parameters based on enemy type.
 * Different enemy types have different fire rates and accuracy.
 */
void EnemyTank::InitializeShootingParameters() {
    // Common defaults
    barrelLength = 20.0f;  // Barrel length for bullet spawn
    shotsInBurst = 0;
    lastShotTime = 0.0f;
    shootCooldown = 0.0f;

    // Type-specific shooting parameters
    switch (enemyType) {
    case EnemyType::RED:
        // Balanced enemy - moderate fire rate and accuracy
        shootCooldownTime = 1.5f;     // Fire every 1.5 seconds
        baseAccuracy = 0.6f;          // 60% accuracy
        accuracySpreadAngle = 15.0f;  // ±15° spread
        maxBurstSize = 3;             // 3-shot bursts
        break;

    case EnemyType::BLACK:
        // Armored tank - slow but accurate
        shootCooldownTime = 2.5f;     // Fire every 2.5 seconds
        baseAccuracy = 0.8f;          // 80% accuracy
        accuracySpreadAngle = 8.0f;   // ±8° spread
        maxBurstSize = 1;             // Single shots
        break;

    case EnemyType::PURPLE:
        // Fast scout - rapid but inaccurate
        shootCooldownTime = 0.8f;     // Fire every 0.8 seconds
        baseAccuracy = 0.4f;          // 40% accuracy
        accuracySpreadAngle = 25.0f;  // ±25° spread
        maxBurstSize = 5;             // 5-shot bursts
        break;

    case EnemyType::ORANGE:
        // Heavy assault - very slow, very accurate
        shootCooldownTime = 3.0f;     // Fire every 3 seconds
        baseAccuracy = 0.9f;          // 90% accuracy
        accuracySpreadAngle = 5.0f;   // ±5° spread
        maxBurstSize = 1;             // Single heavy shots
        break;

    case EnemyType::TEAL:
        // Scout - balanced rapid fire
        shootCooldownTime = 1.2f;     // Fire every 1.2 seconds
        baseAccuracy = 0.7f;          // 70% accuracy
        accuracySpreadAngle = 12.0f;  // ±12° spread
        maxBurstSize = 2;             // 2-shot bursts
        break;
    }

    //Utils::printMsg(GetEnemyTypeName() + " shooting initialized - Cooldown: " +
    //    std::to_string(shootCooldownTime) + "s, Accuracy: " +
    //    std::to_string(baseAccuracy * 100.0f) + "%, Burst: " +
    //    std::to_string(maxBurstSize), success);
}
sf::Vector2f EnemyTank::CalculateSafeRetreatPosition(sf::Vector2f threatPos) const {
    // Calculate vector away from threat
    sf::Vector2f awayFromThreat = position - threatPos;
    float length = std::sqrt(awayFromThreat.x * awayFromThreat.x +
        awayFromThreat.y * awayFromThreat.y);

    if (length > 0.001f) {
        awayFromThreat.x /= length;
        awayFromThreat.y /= length;
    }
    else {
        awayFromThreat = sf::Vector2f(1.0f, 0.0f);
    }

    // Calculate center of safe area
    sf::Vector2f safeCenter(
        (WorldConstants::MOVEMENT_MIN_X + WorldConstants::MOVEMENT_MAX_X) / 2.0f,
        (WorldConstants::MOVEMENT_MIN_Y + WorldConstants::MOVEMENT_MAX_Y) / 2.0f
    );

    // Calculate vector toward center
    sf::Vector2f towardCenter = safeCenter - position;
    float centerDist = std::sqrt(towardCenter.x * towardCenter.x +
        towardCenter.y * towardCenter.y);

    if (centerDist > 0.001f) {
        towardCenter.x /= centerDist;
        towardCenter.y /= centerDist;
    }

    // Blend: 60% away from threat, 40% toward center
    sf::Vector2f blendedDirection;
    blendedDirection.x = awayFromThreat.x * 0.6f + towardCenter.x * 0.4f;
    blendedDirection.y = awayFromThreat.y * 0.6f + towardCenter.y * 0.4f;

    // Normalize blended direction
    float blendLength = std::sqrt(blendedDirection.x * blendedDirection.x +
        blendedDirection.y * blendedDirection.y);
    if (blendLength > 0.001f) {
        blendedDirection.x /= blendLength;
        blendedDirection.y /= blendLength;
    }

    // Calculate target position (200 units in blended direction)
    sf::Vector2f targetPos;
    targetPos.x = position.x + blendedDirection.x * 200.0f;
    targetPos.y = position.y + blendedDirection.y * 200.0f;

    // Clamp to safe movement area with margin
    const float SAFETY_MARGIN = 100.0f;
    targetPos.x = std::max(WorldConstants::MOVEMENT_MIN_X + SAFETY_MARGIN,
        std::min(WorldConstants::MOVEMENT_MAX_X - SAFETY_MARGIN, targetPos.x));
    targetPos.y = std::max(WorldConstants::MOVEMENT_MIN_Y + SAFETY_MARGIN,
        std::min(WorldConstants::MOVEMENT_MAX_Y - SAFETY_MARGIN, targetPos.y));

    return targetPos;
}

sf::Vector2f EnemyTank::GenerateSafeInteriorPosition() const {
    // Use randomization with boundary avoidance
    static std::random_device rd;
    static std::mt19937 gen(rd());

    const float INTERIOR_MARGIN = 150.0f;

    std::uniform_real_distribution<float> distX(
        WorldConstants::MOVEMENT_MIN_X + INTERIOR_MARGIN,
        WorldConstants::MOVEMENT_MAX_X - INTERIOR_MARGIN
    );

    std::uniform_real_distribution<float> distY(
        WorldConstants::MOVEMENT_MIN_Y + INTERIOR_MARGIN,
        WorldConstants::MOVEMENT_MAX_Y - INTERIOR_MARGIN
    );

    return sf::Vector2f(distX(gen), distY(gen));
}

sf::Vector2f EnemyTank::SelectSafeCorner() const {
    const float CORNER_MARGIN = 130.0f;

    // Define four corner positions (with safety margin)
    sf::Vector2f topLeft(
        WorldConstants::MOVEMENT_MIN_X + CORNER_MARGIN,
        WorldConstants::MOVEMENT_MIN_Y + CORNER_MARGIN
    );
    sf::Vector2f topRight(
        WorldConstants::MOVEMENT_MAX_X - CORNER_MARGIN,
        WorldConstants::MOVEMENT_MIN_Y + CORNER_MARGIN
    );
    sf::Vector2f bottomLeft(
        WorldConstants::MOVEMENT_MIN_X + CORNER_MARGIN,
        WorldConstants::MOVEMENT_MAX_Y - CORNER_MARGIN
    );
    sf::Vector2f bottomRight(
        WorldConstants::MOVEMENT_MAX_X - CORNER_MARGIN,
        WorldConstants::MOVEMENT_MAX_Y - CORNER_MARGIN
    );

    // Calculate distances to each corner
    auto calcDist = [this](const sf::Vector2f& corner) -> float {
        float dx = corner.x - position.x;
        float dy = corner.y - position.y;
        return std::sqrt(dx * dx + dy * dy);
        };

    float distTL = calcDist(topLeft);
    float distTR = calcDist(topRight);
    float distBL = calcDist(bottomLeft);
    float distBR = calcDist(bottomRight);

    // Find farthest corner
    sf::Vector2f bestCorner = topLeft;
    float maxDistance = distTL;

    if (distTR > maxDistance) {
        maxDistance = distTR;
        bestCorner = topRight;
    }
    if (distBL > maxDistance) {
        maxDistance = distBL;
        bestCorner = bottomLeft;
    }
    if (distBR > maxDistance) {
        maxDistance = distBR;
        bestCorner = bottomRight;
    }

    return bestCorner;
}