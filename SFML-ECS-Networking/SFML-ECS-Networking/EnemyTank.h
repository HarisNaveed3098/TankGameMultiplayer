#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include "utils.h"
#include <memory>
#include "world_constants.h"
// Forward declaration
class HealthBarRenderer;

/**
 * EnemyTank class represents an AI-controlled enemy tank in the game.
 * Enemies have health, can move, rotate, and will later be able to shoot.
 */
class EnemyTank {
public:
    /**
     * Enemy tank types/variants with different colors
     */
    enum class EnemyType {
        RED,        // Crimson - basic enemy
        BLACK,      // Charcoal - armored variant
        PURPLE,     // Dark purple - fast variant
        ORANGE,     // Burnt orange - heavy variant
        TEAL        // Dark teal - scout variant
    };

    /**
     * AI behavior states for state machine
     */
    enum class AIState {
        IDLE,       // Standing still, scanning for threats
        PATROL,     // Moving to random waypoints
        CHASE,      // Pursuing a target player
        ATTACK,     // In combat range, shooting at target
        RETREAT     // Low health, moving away from threats
    };

    // Constructors
    EnemyTank(EnemyType type, sf::Vector2f startPosition);
    ~EnemyTank();

    // Update enemy state (movement, AI logic)
    void Update(float dt);

    // Update sprite positions and rotations without movement logic
    void UpdateSprites();

    // Render enemy tank to the provided window
    void Render(sf::RenderWindow& window);

    // Health management
    void TakeDamage(float damage);
    void Heal(float amount);
    bool IsDead() const { return currentHealth <= 0.0f; }
    float GetHealth() const { return currentHealth; }
    float GetMaxHealth() const { return maxHealth; }
    float GetHealthPercentage() const { return currentHealth / maxHealth; }
    void SetHealth(float health);
    void SetMaxHealth(float maxHp);

    // Position and rotation accessors
    sf::Vector2f GetPosition() const { return position; }
    void SetPosition(sf::Vector2f pos);
    sf::Angle GetBodyRotation() const { return bodyRotation; }
    sf::Angle GetBarrelRotation() const { return barrelRotation; }
    void SetBodyRotation(sf::Angle rotation) { bodyRotation = rotation; }
    void SetBarrelRotation(sf::Angle rotation) { barrelRotation = rotation; }

    // Enemy type info
    EnemyType GetEnemyType() const { return enemyType; }
    std::string GetEnemyTypeName() const;

    // Collision detection helper
    float GetRadius() const { return collisionRadius; }

    // For future AI: target tracking
    void SetTargetPosition(sf::Vector2f target) { targetPosition = target; }
    sf::Vector2f GetTargetPosition() const { return targetPosition; }

    // Score value when destroyed
    int GetScoreValue() const { return scoreValue; }

    // Shooting mechanics
    bool CanShoot() const { return shootCooldown <= 0.0f; }
    float GetShootCooldown() const { return shootCooldown; }
    void UpdateCooldown(float dt);
    sf::Vector2f GetBarrelEndPosition() const;

    // Fire control (returns bullet data for server to spawn)
    bool TryShoot();
    sf::Vector2f GetAimDirection() const;

    // Accuracy system
    float GetAccuracy() const { return baseAccuracy; }
    sf::Vector2f ApplyAccuracySpread(sf::Vector2f direction) const;

    // AI state accessors (for debugging/network sync)
    AIState GetAIState() const { return currentAIState; }
    void SetAIState(AIState newState);
    std::string GetAIStateName() const;

    // AI target accessors
    uint32_t GetTargetPlayerId() const { return targetPlayerId; }
    void SetTargetPlayerId(uint32_t playerId) { targetPlayerId = playerId; }
    bool HasTarget() const { return targetPlayerId != 0; }

    // AI parameters accessors (for server)
    float GetDetectionRange() const { return detectionRange; }
    float GetAttackRange() const { return attackRange; }
    float GetAggressionLevel() const { return aggressionLevel; }

    // FIXED: Make these public so GameServer can call them
    void SelectNewTarget(uint32_t playerId, sf::Vector2f playerPos);
    void ClearTarget();

    bool ShouldShootAtTarget() const;

    // Public members for easy network sync (like Tank class)
    sf::Vector2f position;
    sf::Angle bodyRotation;
    sf::Angle barrelRotation;

private:
    // Enemy type and stats
    EnemyType enemyType;
    float maxHealth;
    float currentHealth;
    int scoreValue;

    // Movement properties
    float movementSpeed;
    float rotationSpeed;
    float collisionRadius;

    // AI state machine
    AIState currentAIState;
    AIState previousAIState;
    float stateTimer;

    // Shooting system
    float shootCooldown;
    float shootCooldownTime;
    float barrelLength;

    // Accuracy system
    float baseAccuracy;
    float accuracySpreadAngle;

    // Combat behavior
    float lastShotTime;
    int shotsInBurst;
    int maxBurstSize;

    // AI targeting
    uint32_t targetPlayerId;
    sf::Vector2f lastKnownTargetPos;
    float targetLostTimer;

    // AI decision-making timers
    float targetScanTimer;
    float targetScanInterval;
    float stateChangeTimer;

    // AI personality parameters
    float detectionRange;
    float attackRange;
    float retreatHealthThreshold;
    float aggressionLevel;

    // Patrol behavior
    sf::Vector2f patrolWaypoint;
    float waypointReachedDistance;
    float patrolWaitTimer;
    float patrolWaitDuration;

    // AI targeting (for future implementation)
    sf::Vector2f targetPosition;

    // Textures and sprites
    sf::Texture placeholder;
    sf::Texture bodyTexture;
    sf::Texture barrelTexture;
    sf::Sprite body;
    sf::Sprite barrel;

    // Color string for texture loading
    std::string colorString;

    // Health bar visualization
    std::unique_ptr<HealthBarRenderer> healthBarRenderer;
    bool showHealthBar;

    // Helper methods
    void InitializeTextures();
    void InitializeStats();
    std::string GetColorStringFromType(EnemyType type) const;
    bool IsValidDeltaTime(float dt) const;
    bool IsValidPosition(sf::Vector2f pos) const;

    // AI utility functions
    float CalculateDistanceTo(sf::Vector2f targetPos) const;
    float CalculateAngleTo(sf::Vector2f targetPos) const;
    sf::Vector2f GetDirectionTo(sf::Vector2f targetPos) const;

    // AI decision-making
    void UpdateAIBehavior(float dt);
    bool IsTargetInRange(float range) const;
    bool ShouldRetreat() const;
    bool ShouldChaseTarget() const;

    // State-specific updates
    void UpdateIdleState(float dt);
    void UpdatePatrolState(float dt);
    void UpdateChaseState(float dt);
    void UpdateAttackState(float dt);
    void UpdateRetreatState(float dt);

    // Patrol helpers
    void GenerateNewPatrolWaypoint();
    bool HasReachedWaypoint() const;

    // Movement and rotation
    void RotateTowards(sf::Vector2f targetPos, float dt);
    void MoveTowards(sf::Vector2f targetPos, float dt);
    void MoveAwayFrom(sf::Vector2f threatPos, float dt);

    // Obstacle avoidance
    void MoveTowardsWithAvoidance(sf::Vector2f targetPos, float dt);
    bool IsPositionSafe(sf::Vector2f pos) const;
    bool WillCollideWithBoundary(float lookAheadTime) const;
    sf::Vector2f GetSafeDirection() const;

    // AI initialization
    void InitializeAIParameters();

    // Shooting helpers
    sf::Vector2f CalculateLeadTarget(sf::Vector2f targetPos, sf::Vector2f targetVel, float bulletSpeed) const;
    void InitializeShootingParameters();

    sf::Vector2f CalculateSafeRetreatPosition(sf::Vector2f threatPos) const;
    sf::Vector2f GenerateSafeInteriorPosition() const;
    sf::Vector2f SelectSafeCorner() const;
};