#pragma once
#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <unordered_map>
#include <memory>
#include "utils.h"

/**
 * AssetManager - Centralized asset loading with error handling and fallback support
 *
 * Features:
 * - Try-catch protection for all file I/O operations
 * - Automatic fallback texture generation when assets fail to load
 * - Asset caching to prevent duplicate loading
 * - Graceful degradation for non-critical assets
 */
class AssetManager {
public:
    static AssetManager& Instance() {
        static AssetManager instance;
        return instance;
    }

    // Delete copy constructor and assignment operator
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    /**
     * Load texture with error handling and fallback
     * @param filename - Path to texture file
     * @param critical - If true, logs error but continues. If false, just warning
     * @return Reference to loaded or fallback texture (never nullptr)
     */
    const sf::Texture& LoadTexture(const std::string& filename, bool critical = true);

    /**
     * Load font with error handling and fallback
     * @param filename - Path to font file
     * @return Pointer to loaded font (nullptr if failed and font is optional)
     */
    const sf::Font* LoadFont(const std::string& filename);

    /**
     * Try multiple font paths and return first successful load
     * @param fontPaths - Vector of font file paths to try
     * @return Pointer to loaded font (nullptr if all failed)
     */
    const sf::Font* LoadFontWithFallbacks(const std::vector<std::string>& fontPaths);

    /**
     * Check if a texture was successfully loaded (not a fallback)
     * @param filename - Original filename requested
     * @return true if actual texture loaded, false if using fallback
     */
    bool IsTextureLoaded(const std::string& filename) const;

    /**
     * Check if any font was successfully loaded
     * @return true if at least one font is available
     */
    bool IsFontAvailable() const { return defaultFont != nullptr; }

    /**
     * Get statistics about asset loading
     */
    struct LoadStats {
        size_t texturesLoaded;
        size_t texturesFailed;
        size_t fontsLoaded;
        size_t fontsFailed;
    };
    LoadStats GetLoadStats() const { return loadStats; }

    /**
     * Clear all cached assets (for cleanup)
     */
    void Clear();

private:
    AssetManager();
    ~AssetManager() = default;

    // Asset caches
    std::unordered_map<std::string, std::unique_ptr<sf::Texture>> textureCache;
    std::unordered_map<std::string, bool> textureLoadSuccess; // Track if actual file loaded
    std::unique_ptr<sf::Font> defaultFont;

    // Fallback assets
    std::unique_ptr<sf::Texture> fallbackTexture;
    std::unique_ptr<sf::Texture> fallbackTransparentTexture;

    // Statistics
    LoadStats loadStats;

    // Helper methods
    void CreateFallbackTextures();
    sf::Texture* CreateFallbackTexture(sf::Color color, sf::Vector2u size);
    sf::Texture* CreatePatternTexture(sf::Vector2u size, sf::Color color1, sf::Color color2);
    bool TryLoadTexture(const std::string& filename, sf::Texture& texture);
    bool TryLoadFont(const std::string& filename, sf::Font& font);
};