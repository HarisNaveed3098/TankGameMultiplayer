#include "Bullet.h"
#include "utils.h"
#include <iostream>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

/**
 * Constructor for Bullet
 * @param type Type of bullet (player, enemy, shell, tracer)
 * @param startPosition Spawn position in world
 * @param direction Direction vector (should be normalized)
 * @param ownerId ID of tank that fired this bullet
 */
Bullet::Bullet(BulletType type, sf::Vector2f startPosition, sf::Vector2f direction, uint32_t ownerId)
    : bulletType(type), position(startPosition), ownerId(ownerId),
    bulletId(0), isDestroyed(false), sprite(placeholder)
{
    // Initialize placeholder texture (4x4 white square) - FIXED VERSION
    try {
        // Create a simple 4x4 white image using sf::Image
        sf::Image placeholderImage;
        placeholderImage.resize(sf::Vector2u(4, 4), sf::Color::White);

        if (!placeholder.loadFromImage(placeholderImage)) {
            Utils::printMsg("Warning: Failed to create bullet placeholder texture", warning);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception creating bullet placeholder - " + std::string(e.what()), error);
    }

    // Normalize direction vector (ensure it's unit length)
    float dirLength = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (dirLength > 0.001f) {
        direction.x /= dirLength;
        direction.y /= dirLength;
    }
    else {
        // Fallback: shoot right if direction is invalid
        direction = sf::Vector2f(1.0f, 0.0f);
        Utils::printMsg("Warning: Invalid bullet direction, defaulting to right", warning);
    }

    // Initialize stats based on bullet type
    InitializeStats();

    // Calculate velocity from direction and speed
    velocity = direction * speed;

    // Calculate rotation angle from direction (for sprite orientation)
    rotation = std::atan2(direction.y, direction.x) * 180.0f / 3.14159f;

    // Load texture
    InitializeTexture();

    // Set texture rectangle - SFML 3.0 syntax
    try {
        sf::Vector2u textureSize = texture.getSize();
        sprite.setTextureRect(sf::IntRect({ 0, 0 },
            { static_cast<int>(textureSize.x), static_cast<int>(textureSize.y) }));
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception setting bullet texture rectangle - " + std::string(e.what()), error);
    }

    // Set sprite origin to center - SFML 3.0 syntax
    try {
        sf::FloatRect bounds = sprite.getLocalBounds();
        sprite.setOrigin({ bounds.position.x + bounds.size.x / 2.0f,
                          bounds.position.y + bounds.size.y / 2.0f });
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception setting bullet sprite origin - " + std::string(e.what()), error);
    }

    // Set initial position and rotation
    try {
        sprite.setPosition(position);
        sprite.setRotation(sf::degrees(rotation));
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception setting bullet initial transform - " + std::string(e.what()), error);
    }

    //Uncomment to see logs
    //Utils::printMsg("Created bullet (Owner: " + std::to_string(ownerId) +
    //    ", Type: " + std::to_string(static_cast<int>(bulletType)) +
    //    ", Damage: " + std::to_string(damage) + ")", debug);
}

/**
 * Destructor for Bullet
 */
Bullet::~Bullet() {
    // SFML resources clean up automatically
}

/**
 * Initializes bullet stats based on type
 * Different bullet types have different speed, damage, lifetime, size
 */
void Bullet::InitializeStats() {
    switch (bulletType) {
    case BulletType::PLAYER_STANDARD:
        // Standard player bullet - balanced
        speed = 500.0f;             // Fast
        damage = 25.0f;             // Medium damage
        maxLifetime = 3.0f;         // 3 seconds
        collisionRadius = 4.0f;     // Small hitbox
        break;

    case BulletType::ENEMY_STANDARD:
        // Enemy bullet - slightly different from player
        speed = 450.0f;             // Slightly slower
        damage = 20.0f;             // Slightly less damage
        maxLifetime = 3.0f;         // 3 seconds
        collisionRadius = 4.0f;     // Small hitbox
        break;

    case BulletType::TANK_SHELL:
        // Heavy tank shell - high damage, slow
        speed = 300.0f;             // Slower
        damage = 50.0f;             // High damage
        maxLifetime = 5.0f;         // Longer lifetime
        collisionRadius = 6.0f;     // Larger hitbox
        break;

    case BulletType::TRACER:
        // Tracer round - visual emphasis, balanced stats
        speed = 600.0f;             // Very fast
        damage = 20.0f;             // Lower damage
        maxLifetime = 2.5f;         // Shorter lifetime
        collisionRadius = 4.0f;     // Small hitbox
        break;

    default:
        // Fallback to player standard
        speed = 500.0f;
        damage = 25.0f;
        maxLifetime = 3.0f;
        collisionRadius = 4.0f;
        break;
    }

    // Start at full lifetime
    lifetime = maxLifetime;
    /*Uncomment to see logs
    Utils::printMsg("Bullet stats: Speed=" + std::to_string(speed) +
        ", Damage=" + std::to_string(damage) +
        ", Lifetime=" + std::to_string(maxLifetime) + "s", debug);
    */
}

/**
 * Initializes texture for this bullet type
 * Loads appropriate texture from Assets folder
 */
void Bullet::InitializeTexture() {
    try {
        std::string filename = GetTextureFilename();

        if (!texture.loadFromFile(filename)) {
            Utils::printMsg("Warning: Could not load bullet texture: " + filename, warning);
            sprite.setTexture(placeholder);
        }
        else {
            sprite.setTexture(texture);
            Utils::printMsg("Loaded bullet texture: " + filename, debug);
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception loading bullet texture - " + std::string(e.what()), error);
        sprite.setTexture(placeholder);
    }
}

/**
 * Gets texture filename based on bullet type
 * @return Filename string for texture
 */
std::string Bullet::GetTextureFilename() const {
    switch (bulletType) {
    case BulletType::PLAYER_STANDARD:
        return "Assets/playerBullet.png";
    case BulletType::ENEMY_STANDARD:
        return "Assets/enemyBullet.png";
    case BulletType::TANK_SHELL:
        return "Assets/tankShell.png";
    case BulletType::TRACER:
        return "Assets/tracerBullet.png";
    default:
        return "Assets/playerBullet.png";
    }
}

/**
 * Updates bullet state - movement and lifetime
 * @param dt Delta time for frame-independent movement
 */
void Bullet::Update(float dt) {
    if (!IsValidDeltaTime(dt)) {
        Utils::printMsg("Warning: Invalid bullet delta time (" + std::to_string(dt) + ")", warning);
        return;
    }

    if (isDestroyed) {
        return;  // Don't update if already destroyed
    }

    try {
        // Update position based on velocity
        position += velocity * dt;

        // Update sprite position
        if (IsValidPosition(position)) {
            sprite.setPosition(position);
        }
        else {
            Utils::printMsg("Warning: Invalid bullet position, destroying bullet", warning);
            isDestroyed = true;
            return;
        }

        // Update lifetime (countdown to expiration)
        lifetime -= dt;

        if (lifetime <= 0.0f) {
            Utils::printMsg("Bullet expired (lifetime ended)", debug);
            isDestroyed = true;
        }
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception in bullet Update - " + std::string(e.what()), error);
        isDestroyed = true;
    }
}

/**
 * Renders the bullet to the provided window
 * @param window The SFML render window
 */
void Bullet::Render(sf::RenderWindow& window) {
    if (!window.isOpen()) {
        Utils::printMsg("Error: Render window is not open for bullet", error);
        return;
    }

    if (isDestroyed) {
        return;  // Don't render destroyed bullets
    }

    try {
        window.draw(sprite);
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception during bullet rendering - " + std::string(e.what()), error);
    }
}

/**
 * Gets bounding box for collision detection
 * @return FloatRect representing bullet bounds
 */
sf::FloatRect Bullet::GetBounds() const {
    // Create bounding box centered on position
    return sf::FloatRect(
        sf::Vector2f(position.x - collisionRadius, position.y - collisionRadius),
        sf::Vector2f(collisionRadius * 2.0f, collisionRadius * 2.0f)
    );
}

/**
 * Validates that delta time is positive and finite
 * @param dt Delta time
 * @return True if valid, false otherwise
 */
bool Bullet::IsValidDeltaTime(float dt) const {
    return dt >= 0 && std::isfinite(dt);
}

/**
 * Validates that a position has finite coordinates
 * @param pos The position to validate
 * @return True if valid, false otherwise
 */
bool Bullet::IsValidPosition(sf::Vector2f pos) const {
    return std::isfinite(pos.x) && std::isfinite(pos.y);
}