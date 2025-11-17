#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/System/Angle.hpp>
#include <deque>
#include <cstdint>
#include <unordered_map>
#include <vector>



// Represents a snapshot of an entity's state at a specific timestamp.
// Used for network interpolation/extrapolation to smooth out entity movement.
struct EntitySnapshot {
    int64_t timestamp;             // Time (in ms) when this snapshot was recorded.
    sf::Vector2f position;         // Entity's position in world space.
    sf::Angle bodyRotation;        // Rotation of the entity's body.
    sf::Angle barrelRotation;      // Rotation of the entity's turret/barrel (if applicable).

    // Movement flags for input/state prediction.
    bool isMoving_forward;
    bool isMoving_backward;
    bool isMoving_left;
    bool isMoving_right;

    sf::Vector2f velocity;         // Calculated linear velocity.
    float angularVelocity;         // Calculated rotational speed.

    // Default constructor initializes everything to safe defaults.
    EntitySnapshot()
        : timestamp(0), position(0.0f, 0.0f),
        bodyRotation(sf::degrees(0)), barrelRotation(sf::degrees(0)),
        isMoving_forward(false), isMoving_backward(false),
        isMoving_left(false), isMoving_right(false),
        velocity(0.0f, 0.0f), angularVelocity(0.0f) {
    }

    // Overloaded constructor for quick snapshot creation.
    EntitySnapshot(int64_t ts, sf::Vector2f pos, sf::Angle body, sf::Angle barrel)
        : timestamp(ts), position(pos), bodyRotation(body), barrelRotation(barrel),
        isMoving_forward(false), isMoving_backward(false),
        isMoving_left(false), isMoving_right(false),
        velocity(0.0f, 0.0f), angularVelocity(0.0f) {
    }
};



// Represents the smoothed (interpolated or extrapolated) state of an entity
// at a given render time, used for rendering and client-side prediction.
struct InterpolatedState {
    sf::Vector2f position;
    sf::Angle bodyRotation;
    sf::Angle barrelRotation;
    bool isMoving;             // Indicates if entity was moving at that moment.
    bool wasExtrapolated;      // True if this state was extrapolated (not directly interpolated).

    InterpolatedState()
        : position(0.0f, 0.0f), bodyRotation(sf::degrees(0)), barrelRotation(sf::degrees(0)),
        isMoving(false), wasExtrapolated(false) {
    }
};


// This class maintains a buffer of snapshots for a single entity and
// provides methods to interpolate or extrapolate entity states between snapshots.


class EntityInterpolationBuffer {
public:
    EntityInterpolationBuffer();
    ~EntityInterpolationBuffer();

    // Constants for buffer configuration and timing thresholds.
    static constexpr size_t MAX_BUFFER_SIZE = 64;           // Max number of snapshots to store per entity.
    static constexpr int64_t INTERPOLATION_DELAY_MS = 100;  // Default interpolation delay to smooth network jitter.
    static constexpr int64_t MIN_DELAY_MS = 50;             // Minimum allowed delay for smoothing.
    static constexpr int64_t MAX_DELAY_MS = 200;            // Maximum allowed delay before snapping.
    static constexpr int64_t MAX_EXTRAPOLATION_TIME_MS = 100;   // Max time we allow extrapolation before freezing entity.
    static constexpr int64_t EXTRAPOLATION_BLEND_TIME_MS = 200; // Blend window between extrapolated and interpolated state.

    // Adds a new snapshot to the buffer (old ones are removed if limit reached).
    void AddSnapshot(const EntitySnapshot& snapshot);

    // Retrieves interpolated (or extrapolated) state for the given render time.
    bool GetInterpolatedState(int64_t renderTime, InterpolatedState& outState) const;

    // Gets the most recent snapshot stored in this buffer.
    bool GetLatestSnapshot(EntitySnapshot& outSnapshot) const;

    // Clears all stored snapshots.
    void Clear();

    // Utility accessors
    size_t GetBufferSize() const { return snapshots.size(); }
    bool IsEmpty() const { return snapshots.empty(); }
    int64_t GetOldestTimestamp() const;
    int64_t GetNewestTimestamp() const;

    // Removes outdated snapshots that are no longer relevant to interpolation.
    void CleanupOldSnapshots(int64_t currentRenderTime);

private:
    std::deque<EntitySnapshot> snapshots;   // Rolling buffer of recent snapshots.
    mutable bool wasLastStateExtrapolated;  // Caches if last returned state was extrapolated.
    mutable int64_t lastExtrapolationTime;  // Timestamp of last extrapolation.
    mutable InterpolatedState lastExtrapolatedState; // Cached last extrapolated state.
    mutable size_t cachedSearchIdx;         // Optimization: remembers last interpolation index.

    // Internal helper: Finds two snapshots that bound the given render time.
    bool FindSnapshotsForInterpolation(int64_t renderTime,
        EntitySnapshot& outBefore, EntitySnapshot& outAfter) const;

    // Internal helper: Interpolates between two snapshots.
    InterpolatedState Interpolate(const EntitySnapshot& before,
        const EntitySnapshot& after, float t) const;

    // Smoothly interpolates two angles (handles wrapping around 360°).
    sf::Angle InterpolateAngle(sf::Angle a, sf::Angle b, float t) const;

    // Calculates linear velocity between two snapshots.
    sf::Vector2f CalculateVelocity(const EntitySnapshot& older, const EntitySnapshot& newer);

    // Calculates angular velocity between two snapshots.
    float CalculateAngularVelocity(const EntitySnapshot& older, const EntitySnapshot& newer);

    // Predicts (extrapolates) next state if renderTime > latest snapshot time.
    InterpolatedState Extrapolate(const EntitySnapshot& latest, int64_t renderTime) const;

    // Blends smoothly between extrapolated and interpolated state to avoid snapping.
    InterpolatedState BlendExtrapolationToInterpolation(
        const InterpolatedState& extrapolated,
        const InterpolatedState& interpolated,
        float blendFactor) const;
};

//
// High-level manager that handles interpolation for *all* networked entities.
// It keeps a buffer per entity and manages timing, interpolation delay, etc.
//

class InterpolationManager {
public:
    InterpolationManager();
    ~InterpolationManager();

    // Initialize system with current server time reference.
    void Initialize(int64_t serverTime);

    // Updates the internal render time and jitter adjustments.
    void Update(float deltaTime);

    // Adds a new snapshot for the specified entity.
    void AddEntitySnapshot(uint32_t entityId, const EntitySnapshot& snapshot);

    // Retrieves the interpolated (or extrapolated) state for rendering.
    bool GetEntityState(uint32_t entityId, InterpolatedState& outState) const;

    // Retrieves the latest snapshot for diagnostic or debug purposes.
    bool GetEntityLatestSnapshot(uint32_t entityId, EntitySnapshot& outSnapshot) const;

    // Removes an entity from the system (e.g., if destroyed or disconnected).
    void RemoveEntity(uint32_t entityId);

    // Clears all entity buffers.
    void Clear();

    // Delay configuration getters/setters.
    int64_t GetInterpolationDelay() const { return interpolationDelay; }
    void SetInterpolationDelay(int64_t delayMs);

    // Returns current render time (used to determine what snapshots to interpolate).
    int64_t GetRenderTime() const { return renderTime; }

    // Enable or disable interpolation (useful for debugging).
    void SetInterpolationEnabled(bool enabled) { interpolationEnabled = enabled; }
    bool IsInterpolationEnabled() const { return interpolationEnabled; }

    // Diagnostics and metrics
    size_t GetEntityCount() const { return entityBuffers.size(); }
    size_t GetTotalSnapshotsBuffered() const;
    size_t GetExtrapolatedEntityCount() const;

    // Struct for debug info about each entity's buffer.
    struct EntityBufferInfo {
        uint32_t entityId;
        size_t snapshotCount;
        int64_t oldestTimestamp;
        int64_t newestTimestamp;
    };

    // Collects buffer diagnostic information for UI or debugging.
    void GetBufferInfo(std::vector<EntityBufferInfo>& outInfo) const;

private:
    std::unordered_map<uint32_t, EntityInterpolationBuffer> entityBuffers; // All entity buffers keyed by entity ID.

    int64_t renderTime;             // Current time used for interpolation rendering.
    int64_t interpolationDelay;     // Current interpolation delay (may adapt dynamically).
    bool interpolationEnabled;      // Flag controlling interpolation behavior.
    float jitterAccumulator;        // Accumulates timing adjustments to smooth jitter.
    int64_t lastSnapshotTime;       // Tracks last snapshot received time.

    // Adjusts interpolation delay dynamically based on network jitter.
    void UpdateInterpolationDelay(float deltaTime);
};
