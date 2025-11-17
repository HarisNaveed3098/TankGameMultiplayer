#include "HealthBarRenderer.h"
#include <algorithm>
#include <cmath>

/**
 * Constructs health bar: sets dimensions, styles shapes for visual health display.
 * @param barWidth Float width of bar for sizing.
 * @param barHeight Float height of bar for sizing.
 * @param offsetY Float vertical offset above entity.
 */
HealthBarRenderer::HealthBarRenderer(float barWidth, float barHeight, float offsetY)
    : barWidth(barWidth)
    , barHeight(barHeight)
    , offsetY(offsetY)
    , showBackground(true)
    , showBorder(true)
    , borderThickness(1.0f)
{
    // Initialize background bar (dark gray, semi-transparent)
    backgroundBar.setSize(sf::Vector2f(barWidth, barHeight));
    backgroundBar.setFillColor(sf::Color(40, 40, 40, 200));
    backgroundBar.setOrigin(sf::Vector2f(barWidth / 2.0f, barHeight / 2.0f));
    // Initialize health bar (will be colored based on health %)
    healthBar.setSize(sf::Vector2f(barWidth, barHeight));
    healthBar.setFillColor(sf::Color::Green);
    healthBar.setOrigin(sf::Vector2f(barWidth / 2.0f, barHeight / 2.0f));
    // Initialize border outline (white, semi-transparent)
    borderBar.setSize(sf::Vector2f(barWidth + borderThickness * 2, barHeight + borderThickness * 2));
    borderBar.setFillColor(sf::Color::Transparent);
    borderBar.setOutlineColor(sf::Color(255, 255, 255, 150));
    borderBar.setOutlineThickness(borderThickness);
    borderBar.setOrigin(sf::Vector2f((barWidth + borderThickness * 2) / 2.0f,
        (barHeight + borderThickness * 2) / 2.0f));
}

/**
 * Destructs health bar: no manual cleanup needed per SFML RAII.
 */
HealthBarRenderer::~HealthBarRenderer() {
    // Nothing to clean up (SFML shapes manage their own memory)
}

/**
 * Draws health bar above entity: updates size/color, renders layers for UI feedback.
 * @param window RenderWindow ref for drawing.
 * @param entityPosition Vector2f position of entity.
 * @param currentHealth Float current health value.
 * @param maxHealth Float maximum health value.
 */
void HealthBarRenderer::Render(sf::RenderWindow& window, sf::Vector2f entityPosition,
    float currentHealth, float maxHealth) {
    // Validate inputs
    if (!IsValidHealth(currentHealth, maxHealth)) {
        return; // Skip rendering for invalid health values
    }
    // Calculate health percentage (clamped to 0-1)
    float healthPercentage = std::max(0.0f, std::min(1.0f, currentHealth / maxHealth));
    // Update positions relative to entity
    UpdatePositions(entityPosition);
    // Update health bar width based on current health
    float currentBarWidth = barWidth * healthPercentage;
    healthBar.setSize(sf::Vector2f(currentBarWidth, barHeight));
    healthBar.setOrigin(sf::Vector2f(currentBarWidth / 2.0f, barHeight / 2.0f));
    // Update health bar color based on percentage
    healthBar.setFillColor(GetHealthColor(healthPercentage));
    // Render layers (back to front)
    if (showBorder) {
        window.draw(borderBar);
    }
    if (showBackground) {
        window.draw(backgroundBar);
    }
    window.draw(healthBar);
}

/**
 * Updates bar dimensions: resizes shapes for customizable UI scaling.
 * @param width Positive float new width.
 * @param height Positive float new height.
 */
void HealthBarRenderer::SetDimensions(float width, float height) {
    if (width <= 0 || height <= 0 || !std::isfinite(width) || !std::isfinite(height)) {
        return; // Ignore invalid dimensions
    }
    barWidth = width;
    barHeight = height;
    // Update shape sizes
    backgroundBar.setSize(sf::Vector2f(barWidth, barHeight));
    backgroundBar.setOrigin(sf::Vector2f(barWidth / 2.0f, barHeight / 2.0f));
    healthBar.setSize(sf::Vector2f(barWidth, barHeight));
    healthBar.setOrigin(sf::Vector2f(barWidth / 2.0f, barHeight / 2.0f));
    borderBar.setSize(sf::Vector2f(barWidth + borderThickness * 2, barHeight + borderThickness * 2));
    borderBar.setOrigin(sf::Vector2f((barWidth + borderThickness * 2) / 2.0f,
        (barHeight + borderThickness * 2) / 2.0f));
}

/**
 * Sets vertical offset: adjusts position for entity alignment.
 * @param newOffsetY Finite float new offset value.
 */
void HealthBarRenderer::SetOffset(float newOffsetY) {
    if (!std::isfinite(newOffsetY)) {
        return; // Ignore invalid offset
    }
    offsetY = newOffsetY;
}

/**
 * Computes color by percentage: interpolates green-yellow-red for intuitive feedback.
 * @param healthPercentage Clamped 0-1 float ratio.
 * @return Color based on health level.
 */
sf::Color HealthBarRenderer::GetHealthColor(float healthPercentage) const {
    // Clamp to valid range
    healthPercentage = std::max(0.0f, std::min(1.0f, healthPercentage));
    if (healthPercentage > 0.5f) {
        // High health: Green to Yellow (100% -> 50%)
        // Interpolate from Green (0,255,0) to Yellow (255,255,0)
        float t = (1.0f - healthPercentage) * 2.0f; // 0.0 at 100%, 1.0 at 50%
        uint8_t red = static_cast<uint8_t>(255 * t);
        uint8_t green = 255;
        return sf::Color(red, green, 0);
    }
    else {
        // Low health: Yellow to Red (50% -> 0%)
        // Interpolate from Yellow (255,255,0) to Red (255,0,0)
        float t = healthPercentage * 2.0f; // 1.0 at 50%, 0.0 at 0%
        uint8_t red = 255;
        uint8_t green = static_cast<uint8_t>(255 * t);
        return sf::Color(red, green, 0);
    }
}

/**
 * Syncs shape positions: centers above entity for dynamic tracking.
 * @param entityPosition Vector2f current entity coords.
 */
void HealthBarRenderer::UpdatePositions(sf::Vector2f entityPosition) {
    // Calculate health bar position (centered above entity)
    sf::Vector2f barPosition(entityPosition.x, entityPosition.y + offsetY);
    // Update all shape positions
    backgroundBar.setPosition(barPosition);
    healthBar.setPosition(barPosition);
    borderBar.setPosition(barPosition);
}

/**
 * Checks health validity: ensures finite, positive max, non-negative current.
 * @param current Float current health.
 * @param max Float max health.
 * @return Bool true if valid for rendering.
 */
bool HealthBarRenderer::IsValidHealth(float current, float max) const {
    // Check for finite values
    if (!std::isfinite(current) || !std::isfinite(max)) {
        return false;
    }
    // Max health must be positive
    if (max <= 0.0f) {
        return false;
    }
    // Current health must be non-negative
    if (current < 0.0f) {
        return false;
    }
    return true;
}