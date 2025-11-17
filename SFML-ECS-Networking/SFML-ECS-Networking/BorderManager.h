#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>

/**
 * BorderManager handles rendering and collision detection for game world borders.
 * Uses barbed wire textures to create visible boundaries that tanks cannot cross.
 * Includes robust error handling and input validation for production stability.
 */
class BorderManager {
public:
    BorderManager();
    ~BorderManager();

    // Initialize the border system with game world dimensions
    // Returns false if initialization fails (e.g., invalid dimensions or texture loading failure)
    bool Initialize(float worldWidth, float worldHeight, float borderThickness = 32.0f);

    // Render all border elements to the provided window
    // Validates window before rendering
    void Render(sf::RenderWindow& window);

    // Check if a position is within bounds, considering entity radius
    // Returns false if position or radius is invalid
    bool IsPositionInBounds(sf::Vector2f position, float entityRadius = 20.0f) const;

    // Get corrected position if entity is out of bounds
    // Returns clamped position or input position if invalid
    sf::Vector2f ClampPositionToBounds(sf::Vector2f position, float entityRadius = 20.0f) const;

    // Get world bounds
    sf::FloatRect GetWorldBounds() const { return worldBounds; }

private:
    // Border textures
    sf::Texture horizontalWireTexture;
    sf::Texture verticalWireTexture;
    sf::Texture cornerPostTexture;

    // Border sprites
    std::vector<sf::Sprite> horizontalBorders; // Top and bottom borders
    std::vector<sf::Sprite> verticalBorders;   // Left and right borders
    std::vector<sf::Sprite> cornerPosts;       // Corner decorations

    // World dimensions
    sf::FloatRect worldBounds;
    float borderThickness;

    // Helper methods
    bool LoadTextures(); // Loads textures with error handling
    void CreateBorderSprites(); // Creates border sprites with validation
    void CreateHorizontalBorder(float y, bool isTop); // Creates horizontal border with validation
    void CreateVerticalBorder(float x, bool isLeft); // Creates vertical border with validation
    void CreateCornerPosts(); // Creates corner posts with validation

    // Validation helper methods
    bool IsValidDimension(float dimension) const; // Checks if a dimension is positive and finite
    bool IsValidRadius(float radius) const; // Checks if entity radius is positive and finite
    bool IsValidPosition(sf::Vector2f position) const; // Checks if position is finite
};