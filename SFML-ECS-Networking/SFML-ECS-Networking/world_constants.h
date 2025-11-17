#pragma once

namespace WorldConstants {
   /* Total window/world width in pixels */
    constexpr float WORLD_WIDTH = 1280.0f;

    /* Total window/world height in pixels */
    constexpr float WORLD_HEIGHT = 960.0f;

    /* Thickness of border decorations around playable area */
    constexpr float BORDER_THICKNESS = 48.0f;

    /* Minimum X coordinate of playable area (inside border) */
    constexpr float PLAYABLE_MIN_X = BORDER_THICKNESS;

    /* Maximum X coordinate of playable area (inside border) */
    constexpr float PLAYABLE_MAX_X = WORLD_WIDTH - BORDER_THICKNESS;

    /* Minimum Y coordinate of playable area (inside border) */
    constexpr float PLAYABLE_MIN_Y = BORDER_THICKNESS;

    /** Maximum Y coordinate of playable area (inside border) */
    constexpr float PLAYABLE_MAX_Y = WORLD_HEIGHT - BORDER_THICKNESS;

 
    /* Approximate radius of tank for collision detection */
    constexpr float TANK_RADIUS = 25.0f;

    /* Minimum X for tank center (playable area - tank radius) */
    constexpr float MOVEMENT_MIN_X = PLAYABLE_MIN_X + TANK_RADIUS;

    /* Maximum X for tank center (playable area - tank radius) */
    constexpr float MOVEMENT_MAX_X = PLAYABLE_MAX_X - TANK_RADIUS;

    /* Minimum Y for tank center (playable area - tank radius) */
    constexpr float MOVEMENT_MIN_Y = PLAYABLE_MIN_Y + TANK_RADIUS;

    /** Maximum Y for tank center (playable area - tank radius) */
    constexpr float MOVEMENT_MAX_Y = PLAYABLE_MAX_Y - TANK_RADIUS;

 
    /* Extra margin for safe spawning (beyond border + tank radius) */
    constexpr float SPAWN_SAFETY_MARGIN = 10.0f;

    /* Total spawn margin from edge */
    constexpr float SPAWN_MARGIN = BORDER_THICKNESS + TANK_RADIUS + SPAWN_SAFETY_MARGIN;

    /* Minimum X for spawning tanks */
    constexpr float SPAWN_MIN_X = SPAWN_MARGIN;

    /* Maximum X for spawning tanks */
    constexpr float SPAWN_MAX_X = WORLD_WIDTH - SPAWN_MARGIN;

    /* Minimum Y for spawning tanks */
    constexpr float SPAWN_MIN_Y = SPAWN_MARGIN;

    /* Maximum Y for spawning tanks */
    constexpr float SPAWN_MAX_Y = WORLD_HEIGHT - SPAWN_MARGIN;

    /* Playable area width (inside borders) */
    constexpr float PLAYABLE_WIDTH = PLAYABLE_MAX_X - PLAYABLE_MIN_X;

    /*  Playable area height (inside borders) */
    constexpr float PLAYABLE_HEIGHT = PLAYABLE_MAX_Y - PLAYABLE_MIN_Y;

    /* Safe movement area width (accounting for tank size) */
    constexpr float MOVEMENT_WIDTH = MOVEMENT_MAX_X - MOVEMENT_MIN_X;

    /* Safe movement area height (accounting for tank size) */
    constexpr float MOVEMENT_HEIGHT = MOVEMENT_MAX_Y - MOVEMENT_MIN_Y;

    /* World center X coordinate */
    constexpr float CENTER_X = WORLD_WIDTH / 2.0f;

    /* World center Y coordinate */
    constexpr float CENTER_Y = WORLD_HEIGHT / 2.0f;
    constexpr float ENEMY_TANK_RADIUS = 25.0f;     // Enemy tank collision radius
    constexpr float BULLET_RADIUS = 4.0f;          // Bullet collision radius
}
