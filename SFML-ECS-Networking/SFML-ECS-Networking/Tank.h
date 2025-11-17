#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <memory>
#include "utils.h" // For Utils::printMsg
#include "Bullet.h"  // For shooting mechanics

// Forward declaration to avoid circular dependency
class HealthBarRenderer;



/**
 * Tank class represents a player-controlled tank in the game.
 * Manages tank movement, rendering, and player name display.
 * Includes robust error handling and input validation for production stability.
 */
class Tank {
public:
    // Constructors
    Tank(std::string colour);
    Tank(std::string colour, const std::string& playerName);
    ~Tank();

    // Update tank state based on input and delta time
    void Update(float dt);
    void Update(float dt, sf::Vector2f mousePos, bool isLocalPlayer); // Added for mouse-driven barrel rotation

    // Update sprite positions and rotations without movement logic
    void UpdateSprites();

    // Render tank and name label to the provided window
    void Render(sf::RenderWindow& window);

    // Player name management
    void SetPlayerName(const std::string& name);
    const std::string& GetPlayerName() const { return playerName; }

    // Health management
    void TakeDamage(float damage);
    void Heal(float amount);
    bool IsDead() const { return currentHealth <= 0.0f; }
    float GetHealth() const { return currentHealth; }
    float GetMaxHealth() const { return maxHealth; }
    float GetHealthPercentage() const { return currentHealth / maxHealth; }
    void SetHealth(float health);
    void SetMaxHealth(float max);

    // Collision detection helper
    float GetRadius() const { return collisionRadius; }

    // Public members
    sf::Vector2f position = { 0.f, 0.f }; // Tank position
    sf::Angle barrelRotation = sf::degrees(0); // Barrel rotation
    sf::Angle bodyRotation = sf::degrees(0); // Body rotation
    struct {
        bool forward = false;
        bool backward = false;
        bool left = false;
        bool right = false;
    } isMoving; // Movement flags
    void Shoot(std::vector<std::unique_ptr<Bullet>>& bullets);
    bool CanShoot() const { return shootCooldown <= 0.0f; }
    float GetShootCooldown() const { return shootCooldown; }
    sf::Vector2f GetBarrelEndPosition() const;  // Where bullets spawn
    void UpdateCooldown(float dt);
private:
    // Name display
    void InitializeNameLabel(); // Initializes the name label with font
    void UpdateNameLabelPosition(); // Updates the position of the name label
    std::string playerName = ""; // Player's name
    sf::Font nameFont; // Font for name label
    sf::Text* nameLabel = nullptr; // Dynamic text for name display
    bool fontLoaded = false; // Tracks if font was loaded successfully
    bool showNameLabel = true; // Controls name label visibility
    float shootCooldown;           // Time until can shoot again (seconds)
    float shootCooldownTime;       // Time between shots (seconds)
    float barrelLength;            // Length of barrel (for bullet spawn position)

    // Health system
    float maxHealth;               // Maximum health points
    float currentHealth;           // Current health points
    std::unique_ptr<HealthBarRenderer> healthBarRenderer;  // Health bar visualization
    bool showHealthBar;            // Controls health bar visibility
    float collisionRadius;         // Radius for collision detection (same as world constants)

    // Tank textures and sprites
    sf::Texture placeholder; // Fallback texture (1x1 white pixel)
    sf::Texture bodyTexture; // Tank body texture
    sf::Texture barrelTexture; // Tank barrel texture
    sf::Sprite body; // Tank body sprite (initialized in constructor)
    sf::Sprite barrel; // Tank barrel sprite (initialized in constructor)

    // Movement properties
    float movementSpeed = 150.f; // Tank movement speed
    float rotationSpeed = 200.f; // Tank rotation speed
    float barrelRotationSpeed = 200.f; // Barrel rotation speed

    // Tank colour
    std::string colorString = ""; // Stores tank colour for texture loading

    // Validation helper methods
    bool IsValidDeltaTime(float dt) const; // Validates delta time
    bool IsValidPosition(sf::Vector2f pos) const; // Validates position
    bool IsValidString(const std::string& str) const; // Validates string input
};