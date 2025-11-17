#include "client_prediction.h"
#include <algorithm>
#include "network_messages.h"

/**
 * Constructs ClientPrediction: initializes sequence counter for input tracking.
 */
ClientPrediction::ClientPrediction()
    : nextSequenceNumber(1) { // Start at 1 (0 is reserved for "no sequence")
}

/**
 * Destructs ClientPrediction: clears all data structures via Clear().
 */
ClientPrediction::~ClientPrediction() {
    Clear();
}

/**
 * Stores input: assigns sequence/timestamp, adds to history/buffer, limits size.
 * @param input Const InputState ref to store.
 * @return Uint32_t assigned sequence number.
 */
uint32_t ClientPrediction::StoreInput(const InputState& input) {
    // Assign sequence number to this input
    InputState numberedInput = input;
    numberedInput.sequenceNumber = nextSequenceNumber++;
    numberedInput.sentTime = GetCurrentTimestamp();
    numberedInput.acknowledged = false; // Not yet acknowledged
    // Add to history
    inputHistory.push_back(numberedInput);
    // Also add to buffer for tracking unacknowledged inputs
    BufferInput(numberedInput);
    // Limit history size (circular buffer behavior)
    while (inputHistory.size() > MAX_PREDICTION_HISTORY) {
        inputHistory.pop_front();
    }
    return numberedInput.sequenceNumber;
}

/**
 * Stores predicted state: adds to history, limits size for prediction tracking.
 * @param state Const PredictedState ref to store.
 */
void ClientPrediction::StorePredictedState(const PredictedState& state) {
    // Add to prediction history
    predictionHistory.push_back(state);
    // Limit history size
    while (predictionHistory.size() > MAX_PREDICTION_HISTORY) {
        predictionHistory.pop_front();
    }
}

/**
 * Retrieves input by sequence: searches history for match.
 * @param sequenceNumber Uint32_t sequence to find.
 * @param outInput InputState ref to populate if found.
 * @return Bool true if found.
 */
bool ClientPrediction::GetInput(uint32_t sequenceNumber, InputState& outInput) const {
    // Search for input with matching sequence number
    for (const auto& input : inputHistory) {
        if (input.sequenceNumber == sequenceNumber) {
            outInput = input;
            return true;
        }
    }
    return false;
}

/**
 * Retrieves predicted state by sequence: searches history for match.
 * @param sequenceNumber Uint32_t sequence to find.
 * @param outState PredictedState ref to populate if found.
 * @return Bool true if found.
 */
bool ClientPrediction::GetPredictedState(uint32_t sequenceNumber, PredictedState& outState) const {
    // Search for predicted state with matching sequence number
    for (const auto& state : predictionHistory) {
        if (state.sequenceNumber == sequenceNumber) {
            outState = state;
            return true;
        }
    }
    return false;
}

/**
 * Collects inputs after sequence: gathers and sorts for replay.
 * @param sequenceNumber Uint32_t starting sequence (exclusive).
 * @param outInputs Vector<InputState> ref to fill.
 */
void ClientPrediction::GetInputsAfter(uint32_t sequenceNumber, std::vector<InputState>& outInputs) const {
    outInputs.clear();
    // Collect all inputs with sequence number > sequenceNumber
    for (const auto& input : inputHistory) {
        if (input.sequenceNumber > sequenceNumber) {
            outInputs.push_back(input);
        }
    }
    // Sort by sequence number (should already be sorted, but be safe)
    std::sort(outInputs.begin(), outInputs.end(),
        [](const InputState& a, const InputState& b) {
            return a.sequenceNumber < b.sequenceNumber;
        });
}

/**
 * Removes old data: cleans history/buffer below cutoff for memory efficiency.
 * @param lastAckedSequence Uint32_t last acknowledged sequence.
 */
void ClientPrediction::CleanupOldHistory(uint32_t lastAckedSequence) {
    // Remove inputs and predictions older than the last acknowledged sequence
    // Keep a small buffer (10 frames) for safety
    const uint32_t safetyBuffer = 10;
    uint32_t cutoff = (lastAckedSequence > safetyBuffer) ? (lastAckedSequence - safetyBuffer) : 0;
    // Remove old inputs
    while (!inputHistory.empty() && inputHistory.front().sequenceNumber < cutoff) {
        inputHistory.pop_front();
    }
    // Remove old predictions
    while (!predictionHistory.empty() && predictionHistory.front().sequenceNumber < cutoff) {
        predictionHistory.pop_front();
    }
    // Clean up acknowledged inputs from buffer
    auto it = inputBuffer.begin();
    while (it != inputBuffer.end()) {
        if (it->second.input.sequenceNumber < cutoff || it->second.input.acknowledged) {
            it = inputBuffer.erase(it);
        }
        else {
            ++it;
        }
    }
}

/**
 * Returns latest assigned sequence number for current state tracking.
 * @return Uint32_t last sequence number.
 */
uint32_t ClientPrediction::GetLatestSequenceNumber() const {
    return nextSequenceNumber - 1; // Last assigned sequence number
}

/**
 * Clears all histories, buffer, resets sequence for reset operations.
 */
void ClientPrediction::Clear() {
    inputHistory.clear();
    predictionHistory.clear();
    inputBuffer.clear(); // NEW: Clear buffer too
    nextSequenceNumber = 1;
}

/**
 * Adds input to buffer: tracks unacknowledged, enforces max size by age.
 * @param input Const InputState ref to buffer.
 */
void ClientPrediction::BufferInput(const InputState& input) {
    // Add input to buffer
    BufferedInput buffered(input);
    inputBuffer[input.sequenceNumber] = buffered;
    // Enforce maximum buffer size
    if (inputBuffer.size() > MAX_INPUT_BUFFER_SIZE) {
        // Find and remove oldest input
        uint32_t oldestSeq = UINT32_MAX;
        int64_t oldestTime = INT64_MAX;
        for (const auto& pair : inputBuffer) {
            if (pair.second.input.timestamp < oldestTime) {
                oldestTime = pair.second.input.timestamp;
                oldestSeq = pair.first;
            }
        }
        if (oldestSeq != UINT32_MAX) {
            inputBuffer.erase(oldestSeq);
        }
    }
}

/**
 * Marks input acknowledged: updates buffer/history, removes from buffer.
 * @param sequenceNumber Uint32_t sequence to acknowledge.
 */
void ClientPrediction::AcknowledgeInput(uint32_t sequenceNumber) {
    // Find input in buffer and mark as acknowledged
    auto it = inputBuffer.find(sequenceNumber);
    if (it != inputBuffer.end()) {
        it->second.input.acknowledged = true;
        // Remove acknowledged input from buffer
        inputBuffer.erase(it);
    }
    // Also update in input history
    for (auto& input : inputHistory) {
        if (input.sequenceNumber == sequenceNumber) {
            input.acknowledged = true;
            break;
        }
    }
}

/**
 * Collects unacknowledged inputs: gathers and sorts for resending.
 * @param outInputs Vector<InputState> ref to fill.
 */
void ClientPrediction::GetUnacknowledgedInputs(std::vector<InputState>& outInputs) const {
    outInputs.clear();
    // Collect all unacknowledged inputs
    for (const auto& pair : inputBuffer) {
        if (!pair.second.input.acknowledged) {
            outInputs.push_back(pair.second.input);
        }
    }
    // Sort by sequence number
    std::sort(outInputs.begin(), outInputs.end(),
        [](const InputState& a, const InputState& b) {
            return a.sequenceNumber < b.sequenceNumber;
        });
}

/**
 * Flags inputs >= sequence for replay after misprediction.
 * @param fromSequence Uint32_t starting sequence for marking.
 */
void ClientPrediction::MarkInputsForReplay(uint32_t fromSequence) {
    // Mark all inputs from this sequence onwards for replay
    for (auto& pair : inputBuffer) {
        if (pair.second.input.sequenceNumber >= fromSequence) {
            pair.second.needsReplay = true;
        }
    }
}

/**
 * Collects flagged replay inputs: gathers and sorts for correction.
 * @param outInputs Vector<InputState> ref to fill.
 */
void ClientPrediction::GetInputsToReplay(std::vector<InputState>& outInputs) const {
    outInputs.clear();
    // Collect all inputs marked for replay
    for (const auto& pair : inputBuffer) {
        if (pair.second.needsReplay) {
            outInputs.push_back(pair.second.input);
        }
    }
    // Sort by sequence number to replay in correct order
    std::sort(outInputs.begin(), outInputs.end(),
        [](const InputState& a, const InputState& b) {
            return a.sequenceNumber < b.sequenceNumber;
        });
}

/**
 * Resets all replay flags in buffer after correction.
 */
void ClientPrediction::ClearReplayFlags() {
    // Clear all replay flags
    for (auto& pair : inputBuffer) {
        pair.second.needsReplay = false;
    }
}

/**
 * Increments buffer timers: tracks age for timeout cleanup.
 * @param deltaTime Float time delta in seconds.
 */
void ClientPrediction::UpdateBufferTimers(float deltaTime) {
    // Update buffer time for all inputs
    int64_t deltaTimeMs = static_cast<int64_t>(deltaTime * 1000.0f);
    for (auto& pair : inputBuffer) {
        pair.second.bufferTime += deltaTimeMs;
    }
}

/**
 * Removes timed-out inputs from buffer to prevent stale data.
 */
void ClientPrediction::CleanupTimedOutInputs() {
    // Remove inputs that have been in buffer too long
    auto it = inputBuffer.begin();
    while (it != inputBuffer.end()) {
        if (it->second.bufferTime > INPUT_TIMEOUT_MS) {
            it = inputBuffer.erase(it);
        }
        else {
            ++it;
        }
    }
}

/**
 * Returns oldest unacknowledged input timestamp or 0 if none.
 * @return Int64_t oldest timestamp.
 */
int64_t ClientPrediction::GetOldestUnacknowledgedTimestamp() const {
    if (inputBuffer.empty()) {
        return 0;
    }
    int64_t oldestTimestamp = INT64_MAX;
    for (const auto& pair : inputBuffer) {
        if (!pair.second.input.acknowledged &&
            pair.second.input.timestamp < oldestTimestamp) {
            oldestTimestamp = pair.second.input.timestamp;
        }
    }
    return (oldestTimestamp == INT64_MAX) ? 0 : oldestTimestamp;
}

/**
 * Computes buffer statistics: total, replays needed, oldest time, average age.
 * @return BufferStats struct with computed values.
 */
ClientPrediction::BufferStats ClientPrediction::GetBufferStats() const {
    BufferStats stats;
    stats.totalBuffered = inputBuffer.size();
    stats.needingReplay = 0;
    stats.oldestTimestamp = 0;
    stats.averageBufferTime = 0.0f;
    if (inputBuffer.empty()) {
        return stats;
    }
    int64_t oldestTime = INT64_MAX;
    int64_t totalBufferTime = 0;
    for (const auto& pair : inputBuffer) {
        if (pair.second.needsReplay) {
            stats.needingReplay++;
        }
        if (pair.second.input.timestamp < oldestTime) {
            oldestTime = pair.second.input.timestamp;
        }
        totalBufferTime += pair.second.bufferTime;
    }
    stats.oldestTimestamp = (oldestTime == INT64_MAX) ? 0 : oldestTime;
    stats.averageBufferTime = static_cast<float>(totalBufferTime) / static_cast<float>(inputBuffer.size());
    return stats;
}