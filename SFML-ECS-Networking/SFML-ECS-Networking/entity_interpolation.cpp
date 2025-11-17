#include "entity_interpolation.h"
#include "utils.h"
#include <algorithm>
#include <cmath>

// EntityInterpolationBuffer Implementation

EntityInterpolationBuffer::EntityInterpolationBuffer()
    : wasLastStateExtrapolated(false), lastExtrapolationTime(0), cachedSearchIdx(0) {
}

EntityInterpolationBuffer::~EntityInterpolationBuffer() {
    Clear();
}

void EntityInterpolationBuffer::AddSnapshot(const EntitySnapshot& snapshot) {
    // Calculate velocity before adding snapshot
    EntitySnapshot snapshotWithVelocity = snapshot;

    if (!snapshots.empty()) {
        // Find where this snapshot belongs chronologically
        auto it = std::lower_bound(snapshots.begin(), snapshots.end(), snapshotWithVelocity,
            [](const EntitySnapshot& a, const EntitySnapshot& b) {
                return a.timestamp < b.timestamp;
            });

        // Calculate velocity from chronologically PREVIOUS snapshot (not just .back())
        if (it != snapshots.begin()) {
            const EntitySnapshot& prevSnapshot = *(it - 1);
            snapshotWithVelocity.velocity = CalculateVelocity(prevSnapshot, snapshot);
            snapshotWithVelocity.angularVelocity = CalculateAngularVelocity(prevSnapshot, snapshot);
        }
        else {
            // This is the earliest snapshot chronologically
            snapshotWithVelocity.velocity = sf::Vector2f(0.0f, 0.0f);
            snapshotWithVelocity.angularVelocity = 0.0f;
        }
    }
    else {
        // First snapshot - no velocity data yet
        snapshotWithVelocity.velocity = sf::Vector2f(0.0f, 0.0f);
        snapshotWithVelocity.angularVelocity = 0.0f;
    }

    // Add snapshot to buffer in chronological order
    if (snapshots.empty() || snapshotWithVelocity.timestamp >= snapshots.back().timestamp) {
        snapshots.push_back(snapshotWithVelocity);
    }
    else {
        // Find correct position to insert (maintain chronological order)
        auto it = std::lower_bound(snapshots.begin(), snapshots.end(), snapshotWithVelocity,
            [](const EntitySnapshot& a, const EntitySnapshot& b) {
                return a.timestamp < b.timestamp;
            });

        // Check for duplicate timestamp
        if (it != snapshots.end() && it->timestamp == snapshotWithVelocity.timestamp) {
            // Update existing snapshot instead of adding duplicate
            *it = snapshotWithVelocity;
        }
        else {
            snapshots.insert(it, snapshotWithVelocity);
        }
    }

    // Enforce maximum buffer size (circular buffer behavior)
    while (snapshots.size() > MAX_BUFFER_SIZE) {
        snapshots.pop_front();
        // Adjust cached index if we removed from front
        if (cachedSearchIdx > 0) {
            cachedSearchIdx--;
        }
    }
}


bool EntityInterpolationBuffer::GetInterpolatedState(int64_t renderTime, InterpolatedState& outState) const {
    if (snapshots.empty()) {
        return false;  // No data at all
    }

    // Single snapshot - use it directly (no interpolation/extrapolation possible)
    if (snapshots.size() < 2) {
        const EntitySnapshot& latest = snapshots.back();
        outState.position = latest.position;
        outState.bodyRotation = latest.bodyRotation;
        outState.barrelRotation = latest.barrelRotation;
        outState.isMoving = latest.isMoving_forward || latest.isMoving_backward ||
            latest.isMoving_left || latest.isMoving_right;
        outState.wasExtrapolated = false;
        return true;
    }

    // Get latest snapshot timestamp
    int64_t latestTimestamp = snapshots.back().timestamp;

    // CASE 1: Render time is AHEAD of latest snapshot (need EXTRAPOLATION)
    if (renderTime > latestTimestamp) {
        // We need to extrapolate forward
        InterpolatedState extrapolatedState = Extrapolate(snapshots.back(), renderTime);

        // If we were previously interpolating and now need to extrapolate,
        // blend smoothly to avoid sudden jumps
        if (!wasLastStateExtrapolated) {
            // First frame of extrapolation - just use it directly
            outState = extrapolatedState;
            wasLastStateExtrapolated = true;
            lastExtrapolationTime = renderTime;
        }
        else {
            // Continue extrapolating
            outState = extrapolatedState;
        }

        return true;
    }

    // CASE 2: Render time is WITHIN buffer range (normal INTERPOLATION)
    EntitySnapshot before, after;
    if (!FindSnapshotsForInterpolation(renderTime, before, after)) {
        // Fallback to latest if search fails
        const EntitySnapshot& latest = snapshots.back();
        outState.position = latest.position;
        outState.bodyRotation = latest.bodyRotation;
        outState.barrelRotation = latest.barrelRotation;
        outState.isMoving = latest.isMoving_forward || latest.isMoving_backward ||
            latest.isMoving_left || latest.isMoving_right;
        outState.wasExtrapolated = false;
        wasLastStateExtrapolated = false;
        return true;
    }

    // Calculate interpolation factor (t)
    float t = 0.0f;
    int64_t timeDiff = after.timestamp - before.timestamp;
    if (timeDiff > 0) {
        t = static_cast<float>(renderTime - before.timestamp) / static_cast<float>(timeDiff);
    }

    // Interpolate between the two snapshots
    InterpolatedState interpolatedState = Interpolate(before, after, t);
    interpolatedState.wasExtrapolated = false;

    // CASE 3: Transition from EXTRAPOLATION back to INTERPOLATION (smooth blend)
    if (wasLastStateExtrapolated) {
        // Calculate blend factor based on time since extrapolation ended
        int64_t timeSinceExtrapolation = renderTime - lastExtrapolationTime;
        float blendFactor = static_cast<float>(timeSinceExtrapolation) / EXTRAPOLATION_BLEND_TIME_MS;
        blendFactor = std::max(0.0f, std::min(1.0f, blendFactor));

        if (blendFactor < 1.0f) {
            // Blend from SAVED extrapolated state to interpolated state
            outState = BlendExtrapolationToInterpolation(
                lastExtrapolatedState, interpolatedState, blendFactor);
        }
        else {
            // Blend complete - use pure interpolation
            outState = interpolatedState;
            wasLastStateExtrapolated = false;
        }
    }
    else {
        // Normal interpolation (no blending needed)
        outState = interpolatedState;
    }

    return true;
}

bool EntityInterpolationBuffer::GetLatestSnapshot(EntitySnapshot& outSnapshot) const {
    if (snapshots.empty()) {
        return false;
    }

    outSnapshot = snapshots.back();
    return true;
}

void EntityInterpolationBuffer::Clear() {
    snapshots.clear();
}

int64_t EntityInterpolationBuffer::GetOldestTimestamp() const {
    if (snapshots.empty()) {
        return 0;
    }
    return snapshots.front().timestamp;
}

int64_t EntityInterpolationBuffer::GetNewestTimestamp() const {
    if (snapshots.empty()) {
        return 0;
    }
    return snapshots.back().timestamp;
}

void EntityInterpolationBuffer::CleanupOldSnapshots(int64_t currentRenderTime) {
    // Keep some safety margin (2x interpolation delay)
    const int64_t SAFETY_MARGIN = INTERPOLATION_DELAY_MS * 2;
    int64_t cutoffTime = currentRenderTime - SAFETY_MARGIN;

    // FIX #7: Remove old snapshots, but always keep at least 2 for interpolation
    while (snapshots.size() > 2 && snapshots.front().timestamp < cutoffTime) {
        snapshots.pop_front();
        // Adjust cached index when removing from front
        if (cachedSearchIdx > 0) {
            cachedSearchIdx--;
        }
    }
}

bool EntityInterpolationBuffer::FindSnapshotsForInterpolation(int64_t renderTime,
    EntitySnapshot& outBefore, EntitySnapshot& outAfter) const {

    // Find the two snapshots that bracket the render time
    // We want: before.timestamp <= renderTime < after.timestamp

    if (snapshots.size() < 2) {
        return false;
    }

    // PERFORMANCE OPTIMIZATION: Try cached position first (frame coherency)
    // 95% of the time, renderTime advances monotonically, so we're near cachedSearchIdx
    if (cachedSearchIdx < snapshots.size() - 1) {
        if (snapshots[cachedSearchIdx].timestamp <= renderTime &&
            renderTime < snapshots[cachedSearchIdx + 1].timestamp) {
            // Cache hit! (most common case)
            outBefore = snapshots[cachedSearchIdx];
            outAfter = snapshots[cachedSearchIdx + 1];
            return true;
        }
    }

    // Cache miss - search forward (renderTime advanced since last frame)
    for (size_t i = cachedSearchIdx + 1; i < snapshots.size() - 1; ++i) {
        if (snapshots[i].timestamp <= renderTime &&
            renderTime < snapshots[i + 1].timestamp) {
            cachedSearchIdx = i;
            outBefore = snapshots[i];
            outAfter = snapshots[i + 1];
            return true;
        }
    }

    // Search backward (rare: renderTime went backward or jitter)
    for (int i = static_cast<int>(cachedSearchIdx) - 1; i >= 0 && i < static_cast<int>(snapshots.size()) - 1; --i) {
        if (snapshots[i].timestamp <= renderTime &&
            renderTime < snapshots[i + 1].timestamp) {
            cachedSearchIdx = static_cast<size_t>(i);
            outBefore = snapshots[i];
            outAfter = snapshots[i + 1];
            return true;
        }
    }

    // If render time is before all snapshots, DON'T extrapolate backward
    if (renderTime < snapshots.front().timestamp) {
        cachedSearchIdx = 0;
        const EntitySnapshot& first = snapshots.front();
        outBefore = first;
        outAfter = first;  // Same snapshot = t will be 0 = no movement (clamped)
        return true;
    }

    // If render time is after all snapshots, use last two (extrapolation will handle it)
    if (renderTime >= snapshots.back().timestamp) {
        cachedSearchIdx = snapshots.size() - 2;
        outBefore = snapshots[snapshots.size() - 2];
        outAfter = snapshots[snapshots.size() - 1];
        return true;
    }

    // Shouldn't reach here, but fallback to last two snapshots
    cachedSearchIdx = snapshots.size() - 2;
    outBefore = snapshots[snapshots.size() - 2];
    outAfter = snapshots[snapshots.size() - 1];
    return true;
}

InterpolatedState EntityInterpolationBuffer::Interpolate(const EntitySnapshot& before,
    const EntitySnapshot& after, float t) const {

    // Clamp t to [0, 1] range to prevent extrapolation
    t = std::max(0.0f, std::min(1.0f, t));

    InterpolatedState result;

    // Linear interpolation for position (LERP)
    result.position.x = before.position.x + (after.position.x - before.position.x) * t;
    result.position.y = before.position.y + (after.position.y - before.position.y) * t;

    // Interpolate body rotation (handle 360° wrapping correctly)
    result.bodyRotation = InterpolateAngle(before.bodyRotation, after.bodyRotation, t);

    // CRITICAL: Interpolate barrel rotation independently from body rotation
    result.barrelRotation = InterpolateAngle(before.barrelRotation, after.barrelRotation, t);

    // Debug log (optional - comment out after testing)
    Utils::printMsg("Interpolate: before barrel=" + std::to_string(before.barrelRotation.asDegrees()) + 
        "° after=" + std::to_string(after.barrelRotation.asDegrees()) + 
        "° result=" + std::to_string(result.barrelRotation.asDegrees()) + "°", debug);

    // Movement state (use "after" state for current movement indicators)
    result.isMoving = after.isMoving_forward || after.isMoving_backward ||
        after.isMoving_left || after.isMoving_right;

    return result;
}
sf::Angle EntityInterpolationBuffer::InterpolateAngle(sf::Angle a, sf::Angle b, float t) const {
    // Convert to degrees for easier math
    float angleDegrees1 = a.asDegrees();
    float angleDegrees2 = b.asDegrees();

    // Calculate shortest path between angles
    float diff = angleDegrees2 - angleDegrees1;

    // Normalize difference to [-180, 180] range (shortest rotation path)
    while (diff > 180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;

    // Apply smoothing - use ease-in-out cubic for smoother interpolation
    float smoothT = t * t * (3.0f - 2.0f * t);  // Smoothstep function

    // Interpolate along shortest path
    float result = angleDegrees1 + diff * smoothT;  // Use smoothT instead of t

    // Normalize result to [0, 360] range
    while (result < 0.0f) result += 360.0f;
    while (result >= 360.0f) result -= 360.0f;

    return sf::degrees(result);
}

// InterpolationManager Implementation

InterpolationManager::InterpolationManager()
    : renderTime(0),
    interpolationDelay(EntityInterpolationBuffer::INTERPOLATION_DELAY_MS),
    interpolationEnabled(true),
    jitterAccumulator(0.0f),
    lastSnapshotTime(0) {
}

InterpolationManager::~InterpolationManager() {
    Clear();
}

void InterpolationManager::Initialize(int64_t serverTime) {
    // Initialize render time to be (interpolationDelay) milliseconds behind server
    renderTime = serverTime - interpolationDelay;
    lastSnapshotTime = serverTime;

    Utils::printMsg("Interpolation manager initialized with server time: " +
        std::to_string(serverTime) + " (render time: " +
        std::to_string(renderTime) + ", delay: " +
        std::to_string(interpolationDelay) + "ms)", debug);
}

void InterpolationManager::Update(float deltaTime) {
    if (!interpolationEnabled) {
        return;
    }

    // Advance render time (this is our interpolation clock)
    // Convert deltaTime from seconds to milliseconds
    int64_t deltaTimeMs = static_cast<int64_t>(deltaTime * 1000.0f);
    renderTime += deltaTimeMs;

    // Cleanup old snapshots from all entity buffers
    for (auto& [entityId, buffer] : entityBuffers) {
        buffer.CleanupOldSnapshots(renderTime);
    }

    // Optional: Update adaptive interpolation delay based on network conditions
    UpdateInterpolationDelay(deltaTime);

    // Log statistics periodically (every 5 seconds)
    static float statsTimer = 0.0f;
    statsTimer += deltaTime;
    if (statsTimer >= 5.0f) {
        size_t totalSnapshots = GetTotalSnapshotsBuffered();
        Utils::printMsg("Interpolation:  ACTIVE " 
           " render time: " +
            std::to_string(renderTime) + ", delay: " +
            std::to_string(interpolationDelay) + "ms", debug);
        statsTimer = 0.0f;
    }
}

void InterpolationManager::AddEntitySnapshot(uint32_t entityId, const EntitySnapshot& snapshot) {
    // Create buffer for entity if it doesn't exist
    if (entityBuffers.find(entityId) == entityBuffers.end()) {
        entityBuffers[entityId] = EntityInterpolationBuffer();
        Utils::printMsg("Created interpolation buffer for entity " + std::to_string(entityId), debug);
    }

    // Add snapshot to entity's buffer
    entityBuffers[entityId].AddSnapshot(snapshot);

    // Update last snapshot time for adaptive delay adjustment
    lastSnapshotTime = snapshot.timestamp;
}

bool InterpolationManager::GetEntityState(uint32_t entityId, InterpolatedState& outState) const {
    auto it = entityBuffers.find(entityId);
    if (it == entityBuffers.end()) {
        return false;
    }

    // If interpolation is disabled, return latest snapshot
    if (!interpolationEnabled) {
        EntitySnapshot latest;
        if (it->second.GetLatestSnapshot(latest)) {
            outState.position = latest.position;
            outState.bodyRotation = latest.bodyRotation;
            outState.barrelRotation = latest.barrelRotation;
            outState.isMoving = latest.isMoving_forward || latest.isMoving_backward ||
                latest.isMoving_left || latest.isMoving_right;
            return true;
        }
        return false;
    }

    // Get interpolated state at current render time
    return it->second.GetInterpolatedState(renderTime, outState);
}

bool InterpolationManager::GetEntityLatestSnapshot(uint32_t entityId, EntitySnapshot& outSnapshot) const {
    auto it = entityBuffers.find(entityId);
    if (it == entityBuffers.end()) {
        return false;
    }

    return it->second.GetLatestSnapshot(outSnapshot);
}

void InterpolationManager::RemoveEntity(uint32_t entityId) {
    auto it = entityBuffers.find(entityId);
    if (it != entityBuffers.end()) {
        entityBuffers.erase(it);
        Utils::printMsg("Removed interpolation buffer for entity " + std::to_string(entityId), debug);
    }
}

void InterpolationManager::Clear() {
    entityBuffers.clear();
    renderTime = 0;
    lastSnapshotTime = 0;
    jitterAccumulator = 0.0f;

    Utils::printMsg("Interpolation manager cleared", debug);
}

void InterpolationManager::SetInterpolationDelay(int64_t delayMs) {
    // Clamp to valid range
    int64_t oldDelay = interpolationDelay;
    interpolationDelay = std::max(EntityInterpolationBuffer::MIN_DELAY_MS,
        std::min(EntityInterpolationBuffer::MAX_DELAY_MS, delayMs));

    // Adjust render time to maintain continuity
    int64_t delayDiff = interpolationDelay - oldDelay;
    renderTime -= delayDiff;  // If delay increases, render time should go back

    Utils::printMsg("Interpolation delay changed from " + std::to_string(oldDelay) +
        "ms to " + std::to_string(interpolationDelay) + "ms", debug);
}

size_t InterpolationManager::GetTotalSnapshotsBuffered() const {
    size_t total = 0;
    for (const auto& [entityId, buffer] : entityBuffers) {
        total += buffer.GetBufferSize();
    }
    return total;
}

void InterpolationManager::GetBufferInfo(std::vector<EntityBufferInfo>& outInfo) const {
    outInfo.clear();
    outInfo.reserve(entityBuffers.size());

    for (const auto& [entityId, buffer] : entityBuffers) {
        EntityBufferInfo info;
        info.entityId = entityId;
        info.snapshotCount = buffer.GetBufferSize();
        info.oldestTimestamp = buffer.GetOldestTimestamp();
        info.newestTimestamp = buffer.GetNewestTimestamp();
        outInfo.push_back(info);
    }
}
sf::Vector2f EntityInterpolationBuffer::CalculateVelocity(const EntitySnapshot& older, const EntitySnapshot& newer) {
    if (newer.timestamp <= older.timestamp) {
        return sf::Vector2f(0.0f, 0.0f);
    }

    float timeDiffSeconds = static_cast<float>(newer.timestamp - older.timestamp) / 1000.0f;

    const float MAX_TIME_GAP = 0.3f;  // Was 0.2f - more tolerant of gaps
    if (timeDiffSeconds > MAX_TIME_GAP) {
        return sf::Vector2f(0.0f, 0.0f);
    }

    if (timeDiffSeconds < 0.001f) {
        return sf::Vector2f(0.0f, 0.0f);
    }

    sf::Vector2f posDiff = newer.position - older.position;
    sf::Vector2f velocity = posDiff / timeDiffSeconds;

    const float MAX_VELOCITY = 500.0f;  // Was 300.0f
    float velocityMagnitude = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);

    if (velocityMagnitude > MAX_VELOCITY) {
        velocity = velocity * (MAX_VELOCITY / velocityMagnitude);
    }

    return velocity;
}
float EntityInterpolationBuffer::CalculateAngularVelocity(const EntitySnapshot& older, const EntitySnapshot& newer) {
    if (newer.timestamp <= older.timestamp) {
        return 0.0f;
    }

    float timeDiffSeconds = static_cast<float>(newer.timestamp - older.timestamp) / 1000.0f;

    const float MAX_TIME_GAP = 0.3f;  // Was 0.2f
    if (timeDiffSeconds > MAX_TIME_GAP || timeDiffSeconds < 0.001f) {
        return 0.0f;
    }

    // Get rotation difference (handle 360° wrapping)
    float angleDiff = newer.bodyRotation.asDegrees() - older.bodyRotation.asDegrees();

    // Normalize to [-180, 180] (shortest path)
    while (angleDiff > 180.0f) angleDiff -= 360.0f;
    while (angleDiff < -180.0f) angleDiff += 360.0f;

    // Angular velocity = angle difference / time
    float angularVelocity = angleDiff / timeDiffSeconds;

    const float MAX_ANGULAR_VELOCITY = 1080.0f;  // Was 720.0f (3 rotations/sec instead of 2)
    angularVelocity = std::max(-MAX_ANGULAR_VELOCITY,
        std::min(MAX_ANGULAR_VELOCITY, angularVelocity));

    return angularVelocity;
}
InterpolatedState EntityInterpolationBuffer::Extrapolate(const EntitySnapshot& latest, int64_t renderTime) const {
    InterpolatedState result;
    result.wasExtrapolated = true;

    // Calculate how far ahead we need to extrapolate
    int64_t extrapolationTime = renderTime - latest.timestamp;  // in milliseconds

    // Clamp extrapolation time to max limit (prevent infinite prediction)
    if (extrapolationTime > MAX_EXTRAPOLATION_TIME_MS) {
        extrapolationTime = MAX_EXTRAPOLATION_TIME_MS;
    }

    // If extrapolation time is negative or zero, just return latest state
    if (extrapolationTime <= 0) {
        result.position = latest.position;
        result.bodyRotation = latest.bodyRotation;
        result.barrelRotation = latest.barrelRotation;  // CRITICAL: Use snapshot's barrel
        result.isMoving = latest.isMoving_forward || latest.isMoving_backward ||
            latest.isMoving_left || latest.isMoving_right;
        return result;
    }

    // Convert extrapolation time to seconds
    float extrapolationSeconds = static_cast<float>(extrapolationTime) / 1000.0f;

    // Extrapolate position using velocity: position = position + velocity * time
    result.position = latest.position + latest.velocity * extrapolationSeconds;

    // Extrapolate BODY rotation using angular velocity
    float newBodyRotation = latest.bodyRotation.asDegrees() +
        latest.angularVelocity * extrapolationSeconds;

    // Normalize rotation to [0, 360]
    while (newBodyRotation < 0.0f) newBodyRotation += 360.0f;
    while (newBodyRotation >= 360.0f) newBodyRotation -= 360.0f;

    result.bodyRotation = sf::degrees(newBodyRotation);

    // CRITICAL: Barrel rotation does NOT extrapolate with body
    // Barrel is mouse-driven (client-authoritative), so use latest snapshot value
    result.barrelRotation = latest.barrelRotation;

    // Movement state from latest snapshot
    result.isMoving = latest.isMoving_forward || latest.isMoving_backward ||
        latest.isMoving_left || latest.isMoving_right;

    // Save this extrapolated state for smooth blending
    lastExtrapolatedState = result;

    return result;
}
InterpolatedState EntityInterpolationBuffer::BlendExtrapolationToInterpolation(
    const InterpolatedState& extrapolated,
    const InterpolatedState& interpolated,
    float blendFactor) const {

    // Clamp blend factor to [0, 1]
    blendFactor = std::max(0.0f, std::min(1.0f, blendFactor));

    InterpolatedState result;
    result.wasExtrapolated = false;  // We're blending back to interpolation

    // LERP position
    result.position.x = extrapolated.position.x +
        (interpolated.position.x - extrapolated.position.x) * blendFactor;
    result.position.y = extrapolated.position.y +
        (interpolated.position.y - extrapolated.position.y) * blendFactor;

    // SLERP body rotation (shortest path)
    result.bodyRotation = InterpolateAngle(extrapolated.bodyRotation,
        interpolated.bodyRotation, blendFactor);

    // CRITICAL FIX: Also blend barrel rotation
    result.barrelRotation = InterpolateAngle(extrapolated.barrelRotation,
        interpolated.barrelRotation, blendFactor);

    // Use interpolated movement state
    result.isMoving = interpolated.isMoving;

    return result;
}

void InterpolationManager::UpdateInterpolationDelay(float deltaTime) {

    /*
    // Measure jitter from snapshots
    if (lastSnapshotTime > 0) {
        int64_t expectedTime = lastSnapshotTime + 33; // 30Hz = 33ms
        int64_t actualTime = GetCurrentTimestamp();
        int64_t jitter = std::abs(actualTime - expectedTime);

        // Accumulate jitter over time
        jitterAccumulator = jitterAccumulator * 0.95f + static_cast<float>(jitter) * 0.05f;

        // Adjust delay based on accumulated jitter
        if (jitterAccumulator < 20.0f) {
            // Low jitter - can use shorter delay for more responsiveness
            SetInterpolationDelay(50);
        } else if (jitterAccumulator < 50.0f) {
            // Medium jitter - use default delay
            SetInterpolationDelay(100);
        } else {
            // High jitter - use longer delay for stability
            SetInterpolationDelay(150);
        }
    }
    */
}
size_t InterpolationManager::GetExtrapolatedEntityCount() const {
    size_t count = 0;
    for (const auto& [entityId, buffer] : entityBuffers) {
        InterpolatedState state;
        if (buffer.GetInterpolatedState(renderTime, state) && state.wasExtrapolated) {
            count++;
        }
    }
    return count;
}