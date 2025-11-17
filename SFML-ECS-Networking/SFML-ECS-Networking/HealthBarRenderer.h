#pragma once
#include <SFML/Graphics.hpp>
class HealthBarRenderer {
public:
    /**
     * Constructor with default styling
     * @param barWidth Width of the health bar in pixels (default: 50)
     * @param barHeight Height of the health bar in pixels (default: 6)
     * @param offsetY Vertical offset above entity position (default: -35)
     */
    HealthBarRenderer(float barWidth = 50.0f, float barHeight = 6.0f, float offsetY = -35.0f);
    ~HealthBarRenderer();

    /**
     * Render a health bar above an entity
     * @param window SFML render window
     * @param entityPosition World position of the entity (center point)
     * @param currentHealth Current health value
     * @param maxHealth Maximum health value
     */
    void Render(sf::RenderWindow& window, sf::Vector2f entityPosition,
        float currentHealth, float maxHealth);

    /**
     * Set custom dimensions for the health bar
     */
    void SetDimensions(float width, float height);

    /**
     * Set vertical offset above entity
     */
    void SetOffset(float offsetY);

    /**
     * Enable/disable background bar (for contrast)
     */
    void SetShowBackground(bool show) { showBackground = show; }

    /**
     * Enable/disable border outline
     */
    void SetShowBorder(bool show) { showBorder = show; }

private:
    // Health bar dimensions
    float barWidth;
    float barHeight;
    float offsetY;

    // Display options
    bool showBackground;
    bool showBorder;

    // Rendering components
    sf::RectangleShape backgroundBar;  // Dark background
    sf::RectangleShape healthBar;      // Colored health indicator
    sf::RectangleShape borderBar;      // Border outline

    // Border styling
    float borderThickness;

    /**
     * Calculate health bar color based on health percentage
     * Green (100%) -> Yellow (50%) -> Red (0%)
     */
    sf::Color GetHealthColor(float healthPercentage) const;

    /**
     * Update shape positions relative to entity
     */
    void UpdatePositions(sf::Vector2f entityPosition);

    /**
     * Validate health values to prevent rendering errors
     */
    bool IsValidHealth(float current, float max) const;
};