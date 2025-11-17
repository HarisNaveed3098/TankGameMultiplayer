// BorderManager.cpp
#include "BorderManager.h"
#include "utils.h"
#include "world_constants.h"
#include <iostream>
#include <cmath>
#include <limits>

/**
 * Constructor for BorderManager.
 * Initializes border thickness and world bounds using constants.
 */
BorderManager::BorderManager() : borderThickness(WorldConstants::BORDER_THICKNESS) {
    worldBounds = sf::FloatRect(
        sf::Vector2f(WorldConstants::PLAYABLE_MIN_X, WorldConstants::PLAYABLE_MIN_Y),
        sf::Vector2f(WorldConstants::PLAYABLE_WIDTH, WorldConstants::PLAYABLE_HEIGHT)
    );
}

/**
 * Destructor for BorderManager.
 * Vectors and SFML resources are cleaned up automatically.
 */
BorderManager::~BorderManager() {
    // No manual cleanup needed; SFML textures and vectors handle their own destruction
}

/**
 * Initializes the border system with specified world dimensions and border thickness.
 * Validates inputs and loads textures.
 * @param worldWidth Width of the game world
 * @param worldHeight Height of the game world
 * @param borderThickness Thickness of the border
 * @return True if initialization succeeds, false otherwise
 */
bool BorderManager::Initialize(float worldWidth, float worldHeight, float borderThickness) {
    // Validate input dimensions
    if (!IsValidDimension(worldWidth) || !IsValidDimension(worldHeight)) {
        Utils::printMsg("Error: Invalid world dimensions (" +
            std::to_string(worldWidth) + "x" + std::to_string(worldHeight) + ")", error);
        return false;
    }
    if (!IsValidDimension(borderThickness)) {
        Utils::printMsg("Error: Invalid border thickness (" +
            std::to_string(borderThickness) + ")", error);
        return false;
    }
    if (borderThickness * 2 >= worldWidth || borderThickness * 2 >= worldHeight) {
        Utils::printMsg("Error: Border thickness too large for world dimensions", error);
        return false;
    }

    this->borderThickness = borderThickness;

    // Set world bounds (playable area excludes borders)
    worldBounds = sf::FloatRect(
        sf::Vector2f(borderThickness, borderThickness),
        sf::Vector2f(worldWidth - 2 * borderThickness, worldHeight - 2 * borderThickness)
    );

    Utils::printMsg("Initializing border system...");
    Utils::printMsg("World dimensions: " + std::to_string(worldWidth) + "x" + std::to_string(worldHeight));
    Utils::printMsg("Playable area: " + std::to_string(worldBounds.size.x) + "x" + std::to_string(worldBounds.size.y));

    // Load all border textures
    if (!LoadTextures()) {
        Utils::printMsg("Warning: Failed to load one or more border textures - continuing with partial borders", warning);
        // Continue even if textures fail - original behavior
    }

    // Create all border sprites
    CreateBorderSprites();

    Utils::printMsg("Border system initialized successfully", success);
    return true;
}

/**
 * Loads border textures from files.
 * Continues even if some textures fail to load (graceful degradation).
 * @return True if all textures loaded, false if any failed
 */
bool BorderManager::LoadTextures() {
    bool allLoaded = true;

    // Load horizontal barbed wire texture
    if (!horizontalWireTexture.loadFromFile("Assets/barbed_wire_horizontal.png")) {
        Utils::printMsg("Warning: Could not load barbed_wire_horizontal.png", warning);
        allLoaded = false;
    }
    else {
        horizontalWireTexture.setRepeated(true);
        Utils::printMsg("✓ Loaded horizontal barbed wire texture");
    }

    // Load vertical barbed wire texture
    if (!verticalWireTexture.loadFromFile("Assets/barbed_wire_vertical.png")) {
        Utils::printMsg("Warning: Could not load barbed_wire_vertical.png", warning);
        allLoaded = false;
    }
    else {
        verticalWireTexture.setRepeated(true);
        Utils::printMsg("✓ Loaded vertical barbed wire texture");
    }

    // Load corner post texture
    if (!cornerPostTexture.loadFromFile("Assets/border_corner_post.png")) {
        Utils::printMsg("Warning: Could not load border_corner_post.png", warning);
        allLoaded = false;
    }
    else {
        Utils::printMsg("✓ Loaded corner post texture");
    }

    return allLoaded;
}

/**
 * Creates all border sprites for rendering.
 * Handles sprite creation with validation
 */
void BorderManager::CreateBorderSprites() {
    // Clear existing sprites
    horizontalBorders.clear();
    verticalBorders.clear();
    cornerPosts.clear();

    // Create border sprites with proper symmetric positioning 
    CreateHorizontalBorder(0, true); // Top border
    CreateHorizontalBorder(worldBounds.size.y + borderThickness, false); // Bottom border

    // Calculate positioning for left and right borders 
    float leftBorderPadding = 16.0f; // Left border padding from window edge
    float rightBorderPadding = 8.0f; // Right border - less padding (pushed more right)
    CreateVerticalBorder(leftBorderPadding, true); // Left border
    CreateVerticalBorder(worldBounds.size.x + borderThickness - rightBorderPadding - 32.0f, false); // Right border - pushed right

    // Create corner decorations
    CreateCornerPosts();

    Utils::printMsg("Created " + std::to_string(horizontalBorders.size()) + " horizontal borders");
    Utils::printMsg("Created " + std::to_string(verticalBorders.size()) + " vertical borders");
    Utils::printMsg("Created " + std::to_string(cornerPosts.size()) + " corner posts");
}

/**
 * Creates a horizontal border at the specified y position.
 * @param y Y-coordinate of the border
 * @param isTop True if top border, false if bottom
 */
void BorderManager::CreateHorizontalBorder(float y, bool isTop) {
    if (horizontalWireTexture.getSize().x == 0) return; // Texture not loaded - skip silently like original

    float totalWidth = worldBounds.size.x + 2 * borderThickness;
    float textureWidth = static_cast<float>(horizontalWireTexture.getSize().x);
    int numTiles = static_cast<int>(std::ceil(totalWidth / textureWidth));

    for (int i = 0; i < numTiles; ++i) {
        sf::Sprite borderSprite(horizontalWireTexture);
        float x = i * textureWidth;
        borderSprite.setPosition(sf::Vector2f(x, y));

        // Set texture rectangle to repeat the pattern - SFML 3.0 syntax
        sf::IntRect textureRect(sf::Vector2i(0, 0),
            sf::Vector2i(static_cast<int>(textureWidth),
                static_cast<int>(horizontalWireTexture.getSize().y)));
        if (x + textureWidth > totalWidth) {
            // Adjust last tile to fit exactly
            textureRect.size.x = static_cast<int>(totalWidth - x);
        }
        borderSprite.setTextureRect(textureRect);
        horizontalBorders.push_back(borderSprite);
    }
}

/**
 * Creates a vertical border at the specified x position.
 * @param x X-coordinate of the border
 * @param isLeft True if left border, false if right
 */
void BorderManager::CreateVerticalBorder(float x, bool isLeft) {
    if (verticalWireTexture.getSize().y == 0) return; // Texture not loaded - skip silently like original

    float totalHeight = worldBounds.size.y + 2 * borderThickness;
    float textureHeight = static_cast<float>(verticalWireTexture.getSize().y);
    int numTiles = static_cast<int>(std::ceil(totalHeight / textureHeight));

    for (int i = 0; i < numTiles; ++i) {
        sf::Sprite borderSprite(verticalWireTexture);
        float y = i * textureHeight;
        borderSprite.setPosition(sf::Vector2f(x, y));

        // Set texture rectangle to repeat the single wire pattern - SFML 3.0 syntax
        sf::IntRect textureRect(sf::Vector2i(0, 0),
            sf::Vector2i(static_cast<int>(verticalWireTexture.getSize().x),
                static_cast<int>(textureHeight)));
        if (y + textureHeight > totalHeight) {
            // Adjust last tile to fit exactly
            textureRect.size.y = static_cast<int>(totalHeight - y);
        }
        borderSprite.setTextureRect(textureRect);
        verticalBorders.push_back(borderSprite);
    }
}

/**
 * Creates corner posts at the four corners of the world bounds.
 */
void BorderManager::CreateCornerPosts() {
    if (cornerPostTexture.getSize().x == 0) return; // Texture not loaded - skip silently like original

    float postOffset = borderThickness / 2;

    // Create corner posts at each corner - EXACTLY like original
    std::vector<sf::Vector2f> cornerPositions = {
        {-postOffset, -postOffset}, // Top-left
        {worldBounds.size.x + borderThickness - postOffset, -postOffset}, // Top-right
        {-postOffset, worldBounds.size.y + borderThickness - postOffset}, // Bottom-left
        {worldBounds.size.x + borderThickness - postOffset,
         worldBounds.size.y + borderThickness - postOffset} // Bottom-right
    };

    for (const auto& pos : cornerPositions) {
        sf::Sprite cornerSprite(cornerPostTexture);
        cornerSprite.setPosition(pos);
        cornerPosts.push_back(cornerSprite);
    }
}

/**
 * Renders all border elements to the provided window.
 * Validates window before rendering.
 * @param window The SFML render window
 */
void BorderManager::Render(sf::RenderWindow& window) {
    if (!window.isOpen()) {
        Utils::printMsg("Error: Render window is not open", error);
        return;
    }

    // Draw all border elements in order (back to front) - EXACTLY like original
    // Draw horizontal borders
    for (const auto& border : horizontalBorders) {
        window.draw(border);
    }
    // Draw vertical borders
    for (const auto& border : verticalBorders) {
        window.draw(border);
    }
    // Draw corner posts (on top of wire)
    for (const auto& post : cornerPosts) {
        window.draw(post);
    }
}

/**
 * Checks if a position is within the world bounds, considering entity radius.
 * @param position The position to check
 * @param entityRadius The radius of the entity
 * @return True if position is within bounds, false otherwise
 */
bool BorderManager::IsPositionInBounds(sf::Vector2f position, float entityRadius) const {
    if (!IsValidPosition(position) || !IsValidRadius(entityRadius)) {
        return false;
    }

    sf::FloatRect entityBounds(
        sf::Vector2f(position.x - entityRadius, position.y - entityRadius),
        sf::Vector2f(2 * entityRadius, 2 * entityRadius)
    );
    return worldBounds.contains(entityBounds.position) &&
        worldBounds.contains(entityBounds.position + entityBounds.size);
}

/**
 * Clamps a position to stay within world bounds, considering entity radius.
 * @param position The position to clamp
 * @param entityRadius The radius of the entity
 * @return Clamped position
 */
sf::Vector2f BorderManager::ClampPositionToBounds(sf::Vector2f position, float entityRadius) const {
    if (!IsValidPosition(position) || !IsValidRadius(entityRadius)) {
        return position;
    }

    sf::Vector2f clampedPosition = position;
    // Clamp to world bounds considering entity radius
    clampedPosition.x = std::max(worldBounds.position.x + entityRadius, clampedPosition.x);
    clampedPosition.x = std::min(worldBounds.position.x + worldBounds.size.x - entityRadius, clampedPosition.x);
    clampedPosition.y = std::max(worldBounds.position.y + entityRadius, clampedPosition.y);
    clampedPosition.y = std::min(worldBounds.position.y + worldBounds.size.y - entityRadius, clampedPosition.y);
    return clampedPosition;
}

/**
 * Validates that a dimension is positive and finite.
 * @param dimension The dimension to validate
 * @return True if valid, false otherwise
 */
bool BorderManager::IsValidDimension(float dimension) const {
    return dimension > 0 && std::isfinite(dimension);
}

/**
 * Validates that a radius is positive and finite.
 * @param radius The radius to validate
 * @return True if valid, false otherwise
 */
bool BorderManager::IsValidRadius(float radius) const {
    return radius >= 0 && std::isfinite(radius);
}

/**
 * Validates that a position has finite coordinates.
 * @param position The position to validate
 * @return True if valid, false otherwise
 */
bool BorderManager::IsValidPosition(sf::Vector2f position) const {
    return std::isfinite(position.x) && std::isfinite(position.y);
}