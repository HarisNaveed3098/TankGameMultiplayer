#pragma once
#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include "world_constants.h"

// Validation constants and utilities for network data
namespace NetworkValidation {
    // World bounds for position validation (use movement bounds for strict validation)
    constexpr float MIN_X = WorldConstants::MOVEMENT_MIN_X;
    constexpr float MAX_X = WorldConstants::MOVEMENT_MAX_X;
    constexpr float MIN_Y = WorldConstants::MOVEMENT_MIN_Y;
    constexpr float MAX_Y = WorldConstants::MOVEMENT_MAX_Y;

    // Rotation bounds (degrees)
    constexpr float MIN_ROTATION = -360.0f;
    constexpr float MAX_ROTATION = 720.0f;

    // Sanity checks
    constexpr uint32_t MAX_PLAYER_COUNT = 100;
    constexpr uint32_t MAX_PLAYER_NAME_LENGTH = 50;
    constexpr uint32_t MAX_COLOR_NAME_LENGTH = 20;
    constexpr int64_t MAX_TIMESTAMP_DELTA = 60000; // 60 seconds in milliseconds

    // Packet loss detection
    constexpr uint32_t SEQUENCE_WINDOW_SIZE = 100;
    constexpr float PACKET_LOSS_THRESHOLD = 10.0f; // 10% loss triggers warning

    // Helper validation functions
    inline bool IsValidPosition(float x, float y) {
        return x >= MIN_X && x <= MAX_X && y >= MIN_Y && y <= MAX_Y &&
            !std::isnan(x) && !std::isnan(y) &&
            !std::isinf(x) && !std::isinf(y);
    }

    inline bool IsValidRotation(float rotation) {
        return rotation >= MIN_ROTATION && rotation <= MAX_ROTATION &&
            !std::isnan(rotation) && !std::isinf(rotation);
    }

    inline bool IsValidPlayerName(const std::string& name) {
        return !name.empty() && name.length() <= MAX_PLAYER_NAME_LENGTH;
    }

    inline bool IsValidColor(const std::string& color) {
        return !color.empty() && color.length() <= MAX_COLOR_NAME_LENGTH;
    }

    inline bool IsValidTimestamp(int64_t timestamp, int64_t currentTime) {
        if (timestamp <= 0 || currentTime <= 0) return false;
        int64_t delta = std::abs(currentTime - timestamp);
        return delta <= MAX_TIMESTAMP_DELTA;
    }

    inline bool IsValidPlayerCount(uint32_t count) {
        return count > 0 && count <= MAX_PLAYER_COUNT;
    }

    inline bool IsValidPlayerId(uint32_t playerId) {
        return playerId > 0 && playerId < 1000000; // Reasonable upper limit
    }

    inline float ClampPosition(float value, float min, float max) {
        if (std::isnan(value) || std::isinf(value)) return min;
        return std::max(min, std::min(max, value));
    }

    inline float ClampPositionX(float x) {
        return ClampPosition(x, MIN_X, MAX_X);
    }

    inline float ClampPositionY(float y) {
        return ClampPosition(y, MIN_Y, MAX_Y);
    }

    inline float NormalizeRotation(float rotation) {
        if (std::isnan(rotation) || std::isinf(rotation)) return 0.0f;
        while (rotation < 0.0f) rotation += 360.0f;
        while (rotation >= 360.0f) rotation -= 360.0f;
        return rotation;
    }

    // Validate complete player data
    inline bool ValidatePlayerData(float x, float y, float bodyRotation, float barrelRotation) {
        return IsValidPosition(x, y) &&
            IsValidRotation(bodyRotation) &&
            IsValidRotation(barrelRotation);
    }
}