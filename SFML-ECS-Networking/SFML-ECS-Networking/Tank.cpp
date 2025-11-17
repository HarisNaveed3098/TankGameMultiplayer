#include "Tank.h"
#include "utils.h" // For Utils::printMsg
#include "HealthBarRenderer.h"  // For health bar visualization
#include "world_constants.h"    // For TANK_RADIUS
#include <iostream>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

/**
 * Constructor for Tank with colour only.
 * Delegates to the main constructor with an empty player name.
 * @param colour The colour of the tank
 */
Tank::Tank(std::string colour) : Tank(colour, "") {
}


/**
 * Constructor for Tank with colour and player name.
 * Initializes textures, sprites, and name label.
 * @param colour The colour of the tank (used for texture files)
 * @param playerName The player's name to display
 */
Tank::Tank(std::string colour, const std::string& playerName)
    : body(placeholder), barrel(placeholder),  // Initialize sprites with placeholder
    shootCooldown(0.0f),                     // Can shoot immediately
    shootCooldownTime(1.0f),                 // 1 second between shots
    barrelLength(30.0f),                     // Barrel length for spawn position
    maxHealth(100.0f),                       // NEW: Starting max health
    currentHealth(100.0f),                   // NEW: Starting at full health
    healthBarRenderer(std::make_unique<HealthBarRenderer>(50.0f, 6.0f, -40.0f)),  // NEW: Health bar (width, height, offset)
    showHealthBar(true),                     // NEW: Show health bar by default
    collisionRadius(WorldConstants::TANK_RADIUS)  // NEW: Collision radius from constants
{
    // Initialize placeholder texture (1x1 white pixel to avoid white square)
    try {
        std::vector<std::uint8_t> placeholderPixels = { 255, 255, 255, 255 }; // 1x1 white pixel (RGBA)
        if (!placeholder.loadFromMemory(placeholderPixels.data(), placeholderPixels.size())) {
            Utils::printMsg("Error: Failed to create placeholder texture", error);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception creating placeholder texture - " + std::string(e.what()), error);
    }

    // Validate colour string
    if (!IsValidString(colour)) {
        Utils::printMsg("Error: Invalid tank colour string", error);
        colorString = "default"; // Fallback to default colour
    }
    else {
        colorString = colour;
    }

    // Validate and set player name
    if (!IsValidString(playerName)) {
        Utils::printMsg("Warning: Invalid player name, using empty string", warning);
        this->playerName = "";
    }
    else {
        this->playerName = playerName;
    }

    // Load textures with error handling
    try {
        if (!bodyTexture.loadFromFile("Assets/" + colorString + "Tank.png")) {
            Utils::printMsg("Warning: Could not load tank body texture for " + colorString, warning);
            body.setTexture(placeholder); // Use placeholder
        }
        else {
            body.setTexture(bodyTexture); // Update body sprite texture
            Utils::printMsg("✓ Loaded tank body texture for " + colorString);
        }
        if (!barrelTexture.loadFromFile("Assets/" + colorString + "Barrel.png")) {
            Utils::printMsg("Warning: Could not load tank barrel texture for " + colorString, warning);
            barrel.setTexture(placeholder); // Use placeholder
        }
        else {
            barrel.setTexture(barrelTexture); // Update barrel sprite texture
            Utils::printMsg("✓ Loaded tank barrel texture for " + colorString);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception loading textures - " + std::string(e.what()), error);
        body.setTexture(placeholder);
        barrel.setTexture(placeholder);
    }

    // Set texture rectangles - SFML 3.0 syntax
    try {
        sf::Vector2u bodySize = bodyTexture.getSize();
        sf::Vector2u barrelSize = barrelTexture.getSize();
        body.setTextureRect(sf::IntRect({ 0, 0 }, { static_cast<int>(bodySize.x), static_cast<int>(bodySize.y) }));
        barrel.setTextureRect(sf::IntRect({ 0, 0 }, { static_cast<int>(barrelSize.x), static_cast<int>(barrelSize.y) }));
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception setting texture rectangles - " + std::string(e.what()), error);
    }

    // Set sprite origins - SFML 3.0 syntax
    try {
        sf::FloatRect bodyBounds = body.getLocalBounds();
        body.setOrigin({ bodyBounds.position.x + bodyBounds.size.x / 2.0f, bodyBounds.position.y + bodyBounds.size.y / 2.0f });
        barrel.setOrigin({ 6.0f, 2.0f });
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception setting sprite origins - " + std::string(e.what()), error);
    }

    // Set initial positions and rotations
    try {
        body.setPosition(position);
        barrel.setPosition(position);
        body.setRotation(bodyRotation);
        barrel.setRotation(barrelRotation);
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception setting initial positions/rotations - " + std::string(e.what()), error);
    }

    // Initialize name label
    InitializeNameLabel();
}

/**
 * Destructor for Tank.
 * Cleans up dynamically allocated resources.
 */
Tank::~Tank() {
    try {
        delete nameLabel;
        nameLabel = nullptr;
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception in Tank destructor - " + std::string(e.what()), error);
    }
}

/**
 * Initializes the name label for the tank.
 * Attempts to load fonts from multiple sources for robustness.
 */
void Tank::InitializeNameLabel() {
    // Initialize pointer
    nameLabel = nullptr;

    // Try to load a font from multiple sources
    try {
        fontLoaded = nameFont.openFromFile("C:/Windows/Fonts/arial.ttf");
        if (!fontLoaded) {
            fontLoaded = nameFont.openFromFile("C:/Windows/Fonts/calibri.ttf");
        }
        if (!fontLoaded) {
            fontLoaded = nameFont.openFromFile("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf");
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception loading font - " + std::string(e.what()), error);
        fontLoaded = false;
    }

    // Create text object if font loaded and player name is not empty
    if (fontLoaded && !playerName.empty()) {
        try {
            nameLabel = new sf::Text(nameFont); // Use sf::Text(const sf::Font&)
            nameLabel->setString(playerName);
            nameLabel->setCharacterSize(16);
            nameLabel->setFillColor(sf::Color::White);
            nameLabel->setOutlineColor(sf::Color::Black);
            nameLabel->setOutlineThickness(1.0f);
            UpdateNameLabelPosition();
            Utils::printMsg("✓ Name label created for: " + playerName);
        }
        catch (const std::exception& e) {
            Utils::printMsg("Error: Exception creating name label - " + std::string(e.what()), error);
            delete nameLabel;
            nameLabel = nullptr;
            showNameLabel = false;
        }
    }
    else {
        if (!fontLoaded) {
            Utils::printMsg("Warning: Font loading failed, name label disabled", warning);
        }
        if (playerName.empty()) {
            Utils::printMsg("Warning: Player name empty, name label disabled", warning);
        }
        showNameLabel = false;
    }
}

/**
 * Sets the player name and updates the name label if font is loaded.
 * @param name The new player name
 */
void Tank::SetPlayerName(const std::string& name) {
    if (!IsValidString(name)) {
        Utils::printMsg("Warning: Invalid player name, keeping existing name", warning);
        return;
    }

    playerName = name;
    if (fontLoaded && nameLabel) {
        try {
            nameLabel->setString(playerName);
            UpdateNameLabelPosition();
            Utils::printMsg("Updated player name to: " + playerName);
        }
        catch (const std::exception& e) {
            Utils::printMsg("Error: Exception updating name label - " + std::string(e.what()), error);
            showNameLabel = false;
        }
    }
}

/**
 * Updates the position of the name label above the tank.
 */
void Tank::UpdateNameLabelPosition() {
    if (fontLoaded && showNameLabel && nameLabel) {
        try {
            sf::FloatRect textBounds = nameLabel->getLocalBounds();
            nameLabel->setOrigin({ textBounds.position.x + textBounds.size.x / 2.0f, textBounds.position.y + textBounds.size.y / 2.0f });
            nameLabel->setPosition({ position.x, position.y - 45.0f });
        }
        catch (const std::exception& e) {
            Utils::printMsg("Error: Exception updating name label position - " + std::string(e.what()), error);
            showNameLabel = false;
        }
    }
}

/**
 * Updates the tank's position and rotation based on input and delta time.
 * Uses network-driven barrel rotation (for remote players or fallback).
 * @param dt Delta time for frame-independent movement
 */
void Tank::Update(float dt) {
    if (!IsValidDeltaTime(dt)) {
        Utils::printMsg("Warning: Invalid delta time (" + std::to_string(dt) + ")", warning);
        return;
    }

    try {
        // Update shoot cooldown with logging
        if (shootCooldown > 0.0f) {
            float oldCooldown = shootCooldown;
            shootCooldown -= dt;
            if (shootCooldown < 0.0f) {
                shootCooldown = 0.0f;
            }

            // Log every 0.5 seconds
            static float logTimer = 0;
            logTimer += dt;
            if (logTimer >= 0.5f && shootCooldown > 0.0f) {
                Utils::printMsg("Cooldown: " + std::to_string(shootCooldown) + "s remaining", debug);
                logTimer = 0;
            }
        }

        // Update rotation angle based on input
        if (isMoving.left) {
            bodyRotation -= sf::degrees(rotationSpeed * dt);
        }
        if (isMoving.right) {
            bodyRotation += sf::degrees(rotationSpeed * dt);
        }

        // Normalize rotation to 0-360 like server
        float bodyRotDegrees = bodyRotation.asDegrees();
        while (bodyRotDegrees < 0.0f) {
            bodyRotDegrees += 360.0f;
        }
        while (bodyRotDegrees >= 360.0f) {
            bodyRotDegrees -= 360.0f;
        }
        bodyRotation = sf::degrees(bodyRotDegrees);

        // Calculate direction vector - use same math as server
        float radians = bodyRotDegrees * 3.14159f / 180.0f;
        sf::Vector2f body_direction = { std::cos(radians), std::sin(radians) };

        // Update position based on input and direction
        if (isMoving.forward) {
            position += body_direction * movementSpeed * dt;
        }
        if (isMoving.backward) {
            position -= body_direction * movementSpeed * dt;
        }

        // Update barrel rotation (fallback: follow body rotation)
        barrelRotation = bodyRotation; // Used for remote players or fallback

        // Update sprites with new values
        UpdateSprites();
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception in Update - " + std::string(e.what()), error);
    }
}

/**
 * Updates the tank's position and rotation based on input, delta time, and mouse position.
 * Uses mouse-driven barrel rotation for local player.
 * @param dt Delta time for frame-independent movement
 * @param mousePos Mouse position in window coordinates
 * @param isLocalPlayer True if this is the local player's tank
 */
void Tank::Update(float dt, sf::Vector2f mousePos, bool isLocalPlayer) {
    if (!IsValidDeltaTime(dt)) {
        Utils::printMsg("Warning: Invalid delta time (" + std::to_string(dt) + ")", warning);
        return;
    }

    try {
        // Update shoot cooldown with logging
        if (shootCooldown > 0.0f) {
            shootCooldown -= dt;
            if (shootCooldown < 0.0f) {
                shootCooldown = 0.0f;
            }

            // Log every 0.5 seconds
            static float logTimer = 0;
            logTimer += dt;
            if (logTimer >= 0.5f && shootCooldown > 0.0f) {
                Utils::printMsg("Cooldown: " + std::to_string(shootCooldown) + "s remaining", debug);
                logTimer = 0;
            }
        }

        // Update body rotation and position (same as original Update)
        if (isMoving.left) {
            bodyRotation -= sf::degrees(rotationSpeed * dt);
        }
        if (isMoving.right) {
            bodyRotation += sf::degrees(rotationSpeed * dt);
        }

        // Normalize rotation to 0-360 like server
        float bodyRotDegrees = bodyRotation.asDegrees();
        while (bodyRotDegrees < 0.0f) {
            bodyRotDegrees += 360.0f;
        }
        while (bodyRotDegrees >= 360.0f) {
            bodyRotDegrees -= 360.0f;
        }
        bodyRotation = sf::degrees(bodyRotDegrees);

        // Calculate direction vector - use same math as server
        float radians = bodyRotDegrees * 3.14159f / 180.0f;
        sf::Vector2f body_direction = { std::cos(radians), std::sin(radians) };

        // Update position based on input and direction
        if (isMoving.forward) {
            position += body_direction * movementSpeed * dt;
        }
        if (isMoving.backward) {
            position -= body_direction * movementSpeed * dt;
        }

        // Update barrel rotation
        if (isLocalPlayer) {
            // Calculate angle to mouse position
            float deltaX = mousePos.x - position.x;
            float deltaY = mousePos.y - position.y;
            if (std::isfinite(deltaX) && std::isfinite(deltaY)) {
                float desiredAngle = std::atan2(deltaY, deltaX) * 180.0f / 3.14159f;
                barrelRotation = sf::degrees(desiredAngle);
            }
            else {
                Utils::printMsg("Warning: Invalid mouse position, keeping current barrel rotation", warning);
            }
        }
        else {
            // Use network-driven barrelRotation for remote players
            barrelRotation = bodyRotation; // Fallback for consistency
        }

        // Update sprites with new values
        UpdateSprites();
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception in Update (mouse) - " + std::string(e.what()), error);
    }
}
void Tank::UpdateCooldown(float dt) {
    if (shootCooldown > 0.0f) {
        shootCooldown -= dt;
        if (shootCooldown < 0.0f) {
            shootCooldown = 0.0f;
        }
    }
}
/**
 * Updates sprite positions and rotations without movement logic.
 */
void Tank::UpdateSprites() {
    try {
        // Apply rotation to tank body and barrel
        body.setRotation(bodyRotation);
        barrel.setRotation(barrelRotation);

        // Apply position to tank body and barrel
        if (IsValidPosition(position)) {
            body.setPosition(position);
            barrel.setPosition(position);
        }
        else {
            Utils::printMsg("Warning: Invalid tank position, skipping sprite position update", warning);
        }

        // Update name label position
        UpdateNameLabelPosition();
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception in UpdateSprites - " + std::string(e.what()), error);
    }
}

/**
 * Renders the tank and its name label to the provided window.
 * @param window The SFML render window
 */
void Tank::Render(sf::RenderWindow& window) {
    if (!window.isOpen()) {
        Utils::printMsg("Error: Render window is not open", error);
        return;
    }

    try {
        window.draw(body);
        window.draw(barrel);

        // Render health bar above tank
        if (showHealthBar && healthBarRenderer) {
            healthBarRenderer->Render(window, position, currentHealth, maxHealth);
        }

        // Render player name above health bar
        if (fontLoaded && showNameLabel && nameLabel && !playerName.empty()) {
            window.draw(*nameLabel);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception during rendering - " + std::string(e.what()), error);
    }
}

/**
 * Validates that delta time is positive and finite.
 * @param dt Delta time
 * @return True if valid, false otherwise
 */
bool Tank::IsValidDeltaTime(float dt) const {
    return dt >= 0 && std::isfinite(dt);
}

/**
 * Validates that a position has finite coordinates.
 * @param pos The position to validate
 * @return True if valid, false otherwise
 */
bool Tank::IsValidPosition(sf::Vector2f pos) const {
    return std::isfinite(pos.x) && std::isfinite(pos.y);
}

/**
 * Validates that a string is not excessively long and contains valid characters.
 * @param str The string to validate
 * @return True if valid, false otherwise
 */
bool Tank::IsValidString(const std::string& str) const {
    // Limit string length to prevent buffer issues (e.g., 100 characters)
    if (str.length() > 100) {
        return false;
    }

    // Check for printable ASCII characters only (32 to 126)
    for (char c : str) {
        if (c < 32 || c > 126) {
            return false;
        }
    }

    return true;
}
/**
 * Fires a bullet from this tank
 * Creates bullet at barrel end position, aimed in barrel direction
 * @param bullets Reference to bullet container (bullets will be added here)
 */
void Tank::Shoot(std::vector<std::unique_ptr<Bullet>>& bullets) {
    // Check cooldown with debug logging
    if (!CanShoot()) {
        Utils::printMsg("Cannot shoot - cooldown remaining: " + std::to_string(shootCooldown) + "s", debug);
        return;  // Still on cooldown
    }

    try {
        // Calculate barrel direction (where bullet should go)
        float barrelRad = barrelRotation.asDegrees() * 3.14159f / 180.0f;
        sf::Vector2f barrelDirection(std::cos(barrelRad), std::sin(barrelRad));

        // Get spawn position (at end of barrel)
        sf::Vector2f spawnPos = GetBarrelEndPosition();

        Utils::printMsg("Spawning bullet at (" + std::to_string(spawnPos.x) + ", " +
            std::to_string(spawnPos.y) + ") in direction (" +
            std::to_string(barrelDirection.x) + ", " + std::to_string(barrelDirection.y) + ")", debug);

        // Create bullet
        // Note: ownerId will be set by the caller (client or server)
        auto bullet = std::make_unique<Bullet>(
            Bullet::BulletType::PLAYER_STANDARD,
            spawnPos,
            barrelDirection,
            0  // ownerId placeholder (set by network code)
        );

        // Add to bullets container
        bullets.push_back(std::move(bullet));

        // Start cooldown
        shootCooldown = shootCooldownTime;

        Utils::printMsg("Tank fired! Cooldown set to: " + std::to_string(shootCooldown) +
            "s (cooldownTime: " + std::to_string(shootCooldownTime) + "s)", success);
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception in Shoot - " + std::string(e.what()), error);
    }
}


/**
 * Calculates the position at the end of the barrel
 * This is where bullets spawn from
 * @return Position at barrel tip
 */
sf::Vector2f Tank::GetBarrelEndPosition() const {
    try {
        // Calculate barrel direction
        float barrelRad = barrelRotation.asDegrees() * 3.14159f / 180.0f;

        // Calculate offset from tank center to barrel end
        float offsetX = std::cos(barrelRad) * barrelLength;
        float offsetY = std::sin(barrelRad) * barrelLength;

        // Return position at barrel end
        return sf::Vector2f(position.x + offsetX, position.y + offsetY);
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception in GetBarrelEndPosition - " + std::string(e.what()), error);
        return position;  // Fallback to tank center
    }
}

// ============================================================================
// HEALTH MANAGEMENT METHODS
// ============================================================================

/**
 * Apply damage to the tank
 * @param damage Amount of damage to apply (positive value)
 */
void Tank::TakeDamage(float damage) {
    if (damage < 0.0f || !std::isfinite(damage)) {
        Utils::printMsg("Warning: Invalid damage value, ignoring", warning);
        return;
    }

    currentHealth -= damage;

    // Clamp to valid range
    if (currentHealth < 0.0f) {
        currentHealth = 0.0f;
    }

    // Log health change
    Utils::printMsg("Tank took " + std::to_string(damage) + " damage. Health: " +
        std::to_string(currentHealth) + "/" + std::to_string(maxHealth), debug);
}

/**
 * Heal the tank
 * @param amount Amount of health to restore (positive value)
 */
void Tank::Heal(float amount) {
    if (amount < 0.0f || !std::isfinite(amount)) {
        Utils::printMsg("Warning: Invalid heal amount, ignoring", warning);
        return;
    }

    currentHealth += amount;

    // Clamp to max health
    if (currentHealth > maxHealth) {
        currentHealth = maxHealth;
    }

    Utils::printMsg("Tank healed " + std::to_string(amount) + " HP. Health: " +
        std::to_string(currentHealth) + "/" + std::to_string(maxHealth), debug);
}

/**
 * Set current health directly (for network synchronization)
 * @param health New health value
 */
void Tank::SetHealth(float health) {
    if (health < 0.0f || !std::isfinite(health)) {
        Utils::printMsg("Warning: Invalid health value, ignoring", warning);
        return;
    }

    currentHealth = std::min(health, maxHealth);
}

/**
 * Set maximum health (also updates current if needed)
 * @param max New maximum health value
 */
void Tank::SetMaxHealth(float max) {
    if (max <= 0.0f || !std::isfinite(max)) {
        Utils::printMsg("Warning: Invalid max health value, ignoring", warning);
        return;
    }

    maxHealth = max;

    // Clamp current health to new maximum
    if (currentHealth > maxHealth) {
        currentHealth = maxHealth;
    }
}