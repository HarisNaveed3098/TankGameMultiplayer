#pragma once
#include <deque>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <SFML/System/Vector2.hpp>
#include <SFML/System/Angle.hpp>

// Input state for a single frame - stores what the player did
struct InputState {
    uint32_t sequenceNumber;    // Unique ID for this input
    int64_t timestamp;          // When input was generated
    bool moveForward;
    bool moveBackward;
    bool turnLeft;
    bool turnRight;
    float deltaTime;            // Time step for this input
    bool acknowledged;          // NEW: Has server acknowledged this input?
    int64_t sentTime;           // NEW: When was this input sent to server?

    InputState()
        : sequenceNumber(0), timestamp(0),
        moveForward(false), moveBackward(false),
        turnLeft(false), turnRight(false),
        deltaTime(0.0f), acknowledged(false), sentTime(0) {
    }
};

// Predicted state after applying an input - stores where we think we are
struct PredictedState {
    uint32_t sequenceNumber;    // Which input created this state
    int64_t timestamp;          // When this prediction was made
    sf::Vector2f position;      // Predicted position
    sf::Angle bodyRotation;     // Predicted body rotation
    sf::Angle barrelRotation;   // Predicted barrel rotation

    PredictedState()
        : sequenceNumber(0), timestamp(0),
        position(0.0f, 0.0f),
        bodyRotation(sf::degrees(0)),
        barrelRotation(sf::degrees(0)) {
    }

    PredictedState(uint32_t seq, int64_t ts, sf::Vector2f pos, sf::Angle body, sf::Angle barrel)
        : sequenceNumber(seq), timestamp(ts),
        position(pos), bodyRotation(body), barrelRotation(barrel) {
    }
};

// Input buffer entry for tracking unacknowledged inputs
struct BufferedInput {
    InputState input;           // The input itself
    bool needsReplay;           // Should this be replayed after correction?
    int64_t bufferTime;         // How long has this been buffered (milliseconds)

    BufferedInput() : needsReplay(false), bufferTime(0) {}

    BufferedInput(const InputState& inp)
        : input(inp), needsReplay(false), bufferTime(0) {
    }
};

// Client-side prediction manager - handles prediction history and input buffering
class ClientPrediction {
public:
    ClientPrediction();
    ~ClientPrediction();

    // Configuration
    static constexpr size_t MAX_PREDICTION_HISTORY = 60;  // Store last 60 inputs (1 second at 60Hz)
    static constexpr size_t MAX_INPUT_BUFFER_SIZE = 100;  // Maximum buffered inputs
    static constexpr int64_t INPUT_TIMEOUT_MS = 5000;     // 5 seconds before dropping old inputs

    // Store a new input and get its sequence number
    uint32_t StoreInput(const InputState& input);

    // Store predicted state after applying an input
    void StorePredictedState(const PredictedState& state);

    // Get input by sequence number (for replay during reconciliation)
    bool GetInput(uint32_t sequenceNumber, InputState& outInput) const;

    // Get predicted state by sequence number
    bool GetPredictedState(uint32_t sequenceNumber, PredictedState& outState) const;

    // Server reconciliation: Get all inputs after a certain sequence number
    void GetInputsAfter(uint32_t sequenceNumber, std::vector<InputState>& outInputs) const;

    // Clear old history (cleanup)
    void CleanupOldHistory(uint32_t lastAckedSequence);

    // Get latest sequence number
    uint32_t GetLatestSequenceNumber() const;

    // Get current prediction history size (for debugging)
    size_t GetHistorySize() const { return inputHistory.size(); }
    size_t GetPredictionHistorySize() const { return predictionHistory.size(); }

    // Clear all history
    void Clear();

    // INPUT BUFFERING SYSTEM 
    // Add input to buffer (for unacknowledged inputs)
    void BufferInput(const InputState& input);

    // Mark input as acknowledged by server
    void AcknowledgeInput(uint32_t sequenceNumber);

    // Get all unacknowledged inputs (for display/debugging)
    void GetUnacknowledgedInputs(std::vector<InputState>& outInputs) const;

    // Get count of unacknowledged inputs
    size_t GetUnacknowledgedCount() const { return inputBuffer.size(); }

    // Mark inputs for replay after server correction
    void MarkInputsForReplay(uint32_t fromSequence);

    // Get inputs that need to be replayed
    void GetInputsToReplay(std::vector<InputState>& outInputs) const;

    // Clear replay flags
    void ClearReplayFlags();

    // Update buffer timers (call each frame)
    void UpdateBufferTimers(float deltaTime);

    // Remove timed-out inputs from buffer
    void CleanupTimedOutInputs();

    // Get oldest unacknowledged input timestamp (for latency monitoring)
    int64_t GetOldestUnacknowledgedTimestamp() const;

    // Get buffer statistics (for debugging)
    struct BufferStats {
        size_t totalBuffered;
        size_t needingReplay;
        int64_t oldestTimestamp;
        float averageBufferTime;
    };
    BufferStats GetBufferStats() const;

private:
    std::deque<InputState> inputHistory;           // Circular buffer of inputs
    std::deque<PredictedState> predictionHistory;  // Circular buffer of predicted states
    uint32_t nextSequenceNumber;                   // Counter for sequence numbers

    // Input buffer
    std::unordered_map<uint32_t, BufferedInput> inputBuffer;  // Unacknowledged inputs keyed by sequence
};