// Replay a 16 kHz mono s16 WAV through the selected streaming engine.
// Usage: subtly-bench <model-dir> <file.wav> [threads] [paced-seconds]
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <thread>
#include <vector>
#include <sys/resource.h>

#include "asr/IAsrEngine.h"

static bool readWav(const char *path, std::vector<float> &out) {
    std::ifstream f(path, std::ios::binary);
    char riff[12];
    if (!f.read(riff, 12) || std::memcmp(riff, "RIFF", 4) || std::memcmp(riff + 8, "WAVE", 4))
        return false;
    bool valid = false;
    for (;;) {
        char id[4];
        uint32_t size;
        if (!f.read(id, 4) || !f.read(reinterpret_cast<char *>(&size), 4))
            return false;
        if (std::memcmp(id, "fmt ", 4) == 0) {
            uint16_t format, channels, align, bits;
            uint32_t rate, bytes;
            if (size < 16 || !f.read(reinterpret_cast<char *>(&format), 2) ||
                !f.read(reinterpret_cast<char *>(&channels), 2) ||
                !f.read(reinterpret_cast<char *>(&rate), 4) ||
                !f.read(reinterpret_cast<char *>(&bytes), 4) ||
                !f.read(reinterpret_cast<char *>(&align), 2) ||
                !f.read(reinterpret_cast<char *>(&bits), 2))
                return false;
            valid = format == 1 && channels == 1 && rate == 16000 && bits == 16;
            f.seekg(size - 16 + (size & 1), std::ios::cur);
        } else if (std::memcmp(id, "data", 4) == 0) {
            if (!valid || size == 0 || size % 2)
                return false;
            std::vector<int16_t> pcm(size / 2);
            if (!f.read(reinterpret_cast<char *>(pcm.data()), size))
                return false;
            for (int16_t s : pcm)
                out.push_back(s / 32768.0f);
            return true;
        } else {
            f.seekg(size + (size & 1), std::ios::cur);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <model-dir> <file.wav> [threads] [paced-seconds]\n", argv[0]);
        return 2;
    }
    // Lower benchmark priority to keep the desktop responsive.
    if (setpriority(PRIO_PROCESS, 0, 10) != 0)
        std::perror("could not lower benchmark priority");
    std::vector<float> audio;
    if (!readWav(argv[2], audio)) {
        std::fprintf(stderr, "cannot read %s (expected 16 kHz mono s16 WAV)\n", argv[2]);
        return 1;
    }
    audio.insert(audio.end(), 3 * 16000, 0.0f); // trigger silence endpointing
    using Clock = std::chrono::steady_clock;
    const auto loadStart = Clock::now();
    std::string err;
    auto engine = createAsrEngine(argv[1], argc > 3 ? std::atoi(argv[3]) : 1, &err);
    if (!engine) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    const double load = std::chrono::duration<double>(Clock::now() - loadStart).count();
    const int pacedSeconds = argc > 4 ? std::atoi(argv[4]) : 0;
    std::fprintf(stderr, "%s replay; [threads] applies only to Zipformer. "
                         "Moonshine uses one CPU thread; Nemotron uses up to eight.\n",
                 pacedSeconds > 0 ? "Capture-paced" : "Unpaced (maximum throughput)");
    const size_t total = pacedSeconds > 0 ? size_t(pacedSeconds) * 16000 : audio.size();
    const auto start = Clock::now();
    double decode = 0, maxLag = 0;
    std::vector<double> latencies;
    auto printFinals = [&] {
        for (const auto &text : engine->takeFinals())
            std::printf("FINAL: %s\n", text.c_str());
    };
    try {
        for (size_t i = 0; i < total;) {
            const size_t offset = i % audio.size();
            const size_t n = std::min({size_t(1600), audio.size() - offset, total - i});
            const auto deadline = start + std::chrono::duration<double>((i + n) / 16000.0);
            if (pacedSeconds > 0)
                std::this_thread::sleep_until(deadline);
            const auto t0 = Clock::now();
            engine->acceptWaveform({audio.data() + offset, n});
            printFinals();
            const double elapsed = std::chrono::duration<double>(Clock::now() - t0).count();
            decode += elapsed;
            latencies.push_back(elapsed * 1000);
            if (pacedSeconds > 0)
                maxLag = std::max(maxLag, std::chrono::duration<double>(Clock::now() - deadline).count());
            i += n;
        }
        const auto t0 = Clock::now();
        engine->finish();
        printFinals();
        decode += std::chrono::duration<double>(Clock::now() - t0).count();
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
    const double wall = std::chrono::duration<double>(Clock::now() - start).count();
    const auto rest = engine->partial();
    if (!rest.empty())
        std::printf("PARTIAL: %s\n", rest.c_str());
    std::sort(latencies.begin(), latencies.end());
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    const double cpu = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6 +
                       usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6;
    std::printf("audio %.2fs, load %.3fs, decode %.3fs, RTF %.3f, wall %.3fs\n"
                "feed p95 %.1fms, max %.1fms, max lag %.3fs, CPU %.1fs, peak RSS %.1f MiB\n",
                total / 16000.0, load, decode, decode / (total / 16000.0), wall,
                latencies[size_t((latencies.size() - 1) * .95)], latencies.back(), maxLag,
                cpu, usage.ru_maxrss / 1024.0);
}
