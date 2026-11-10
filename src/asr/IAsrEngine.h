#pragma once
#include <span>
#include <memory>
#include <string>
#include <vector>

// Streaming ASR on the worker thread.
class IAsrEngine {
public:
    virtual ~IAsrEngine() = default;

    // Feed 16 kHz mono samples and decode the available audio.
    virtual void acceptWaveform(std::span<const float> samples) = 0;
    // Current hypothesis for the in-progress utterance (UTF-8).
    virtual std::string partial() = 0;
    // Discard stream state, retaining the loaded model.
    virtual void reset() = 0;
    // Drain completed utterances. One feed can produce several finalized utterances.
    virtual std::vector<std::string> takeFinals() = 0;
    virtual void finish() {}
};

std::unique_ptr<IAsrEngine> createAsrEngine(const std::string &modelDir, int numThreads,
                                           std::string *error);
