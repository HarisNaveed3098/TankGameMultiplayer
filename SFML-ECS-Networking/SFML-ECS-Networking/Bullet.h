#pragma once
#include <SFML/Graphics.hpp>
#include <cstdint>
#include "utils.h"

/**
 * Bullet class represents a projectile fired by tanks
 * Handles movement, collision detection, and rendering
 *
 * Design Features:
 * Flies in straight line from spawn point
 * Different types for players vs enemies
 * Lifetime system (destroys after timeout)
 * Damage value varies by type
 * Network-friendly (server-authoritative)
 */
class Bullet {
public:
    /**
     * Bullet types with different properties
     */
    enum class BulletType {
        PLAYER_STANDARD,    // Blue energy bullet (normal damage, normal speed)
        ENEMY_STANDARD,     // Red enemy bullet (normal damage, normal speed)
        TANK_SHELL,         // Heavy shell (high damage, slow speed)
        TRACER              // Tracer round (visual effect, normal stats)
    };

    /**
     * Constructor
     * @param type Type of bullet
     * @param startPosition Spawn position in world
     * @param direction Direction vector (normalized)
     * @param ownerId ID of tank that fired this bullet (for hit detection)
     */
    Bullet(BulletType type, sf::Vector2f startPosition, sf::Vector2f direction, uint32_t ownerId);
    ~Bullet();

    // Update bullet position and lifetime
    void Update(float dt);

    // Render bullet to window
    void Render(sf::RenderWindow& window);

    // Check if bullet should be removed
    bool IsExpired() const { return lifetime <= 0.0f || isDestroyed; }
    bool IsDestroyed() const { return isDestroyed; }

    // Mark bullet as destroyed (hit something)
    void Destroy() { isDestroyed = true; }

    // Collision detection
    sf::Vector2f GetPosition() const { return position; }
    float GetRadius() const { return collisionRadius; }
    sf::FloatRect GetBounds() const;

    // Bullet properties
    float GetDamage() const { return damage; }
    uint32_t GetOwnerId() const { return ownerId; }
    BulletType GetBulletType() const { return bulletType; }

    // For network synchronization
    uint32_t GetBulletId() const { return bulletId; }
    void SetBulletId(uint32_t id) { bulletId = id; }
    sf::Vector2f GetVelocity() const { return velocity; }

    // Public for easy network sync (like Tank/EnemyTank)
    sf::Vector2f position;
    sf::Vector2f velocity;
    float rotation;  // Visual rotation (in degrees)

private:
    // Bullet identification
    uint32_t bulletId;          // Unique ID for network sync
    uint32_t ownerId;           // Who fired this bullet
    BulletType bulletType;

    // Physics properties
    float speed;                // Movement speed (pixels/second)
    float collisionRadius;      // Radius for collision detection

    // Damage and lifetime
    float damage;               // Damage dealt on hit
    float lifetime;             // Time before auto-destroy (seconds)
    float maxLifetime;          // Maximum lifetime
    bool isDestroyed;           // Hit something

    // Rendering
    sf::Texture texture;
    sf::Sprite sprite;
    sf::Texture placeholder;    // Fallback texture

    // Helper methods
    void InitializeStats();
    void InitializeTexture();
    std::string GetTextureFilename() const;
    bool IsValidDeltaTime(float dt) const;
    bool IsValidPosition(sf::Vector2f pos) const;
};