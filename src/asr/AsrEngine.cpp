#include "asr/IAsrEngine.h"
#include "asr/SherpaStreamingEngine.h"
#include "asr/MoonshineStreamingEngine.h"
#ifdef SUBTLY_NEMOTRON
#include "asr/NemotronStreamingEngine.h"
#endif

#include <filesystem>
#include <stdexcept>

std::unique_ptr<IAsrEngine> createAsrEngine(const std::string &modelDir, int numThreads,
                                           std::string *error) {
    namespace fs = std::filesystem;
    try {
        const fs::path dir(modelDir);
        if (fs::exists(dir / "moonshine-language.txt"))
            return MoonshineStreamingEngine::create(modelDir, error);
        if (fs::exists(dir / "tokens.txt"))
            return SherpaStreamingEngine::create(modelDir, numThreads, error);
#ifdef SUBTLY_NEMOTRON
        fs::path model;
        for (const auto &entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".gguf") {
                if (!model.empty())
                    throw std::runtime_error("Keep exactly one GGUF file in " + modelDir);
                model = entry.path();
            }
        }
        if (!model.empty())
            return NemotronStreamingEngine::create(model.string(), error);
#endif
        throw std::runtime_error("No supported ASR model in " + modelDir);
    } catch (const std::exception &e) {
        if (error)
            *error = e.what();
        return nullptr;
    }
}
