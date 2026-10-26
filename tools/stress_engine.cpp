// Real-time paced stress harness for StagePianoHost.
// Usage: stress_engine <libraryDir> [secondsPerScenario] [drone,piano,all]
// Exit code 0 = clean, 1 = glitch indicators found.
#include "StagePianoHost.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <random>
#include <thread>
#include <vector>

// ---- Allocation counter (only counts while g_countAllocs is set on this thread) ----
static thread_local bool g_countAllocs = false;
static std::atomic<uint64_t> g_renderAllocs{0};

void* operator new(std::size_t n) {
    if (g_countAllocs) g_renderAllocs.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

namespace {

constexpr float kSampleRate = 44100.0f;
constexpr uint32_t kBlock = 256;

struct Report {
    const char* name = "";
    uint64_t blocks = 0;
    uint64_t nanCount = 0;
    float peak = 0.0f;
    uint64_t limiterSamples = 0;     // |x| > 0.85 (saturator engaged)
    uint64_t discontinuities = 0;    // sample jumps outside onset windows
    uint64_t silentWindows = 0;      // drone scenario: 100ms windows with RMS ~ 0 while drone should sound
    double worstRenderPct = 0.0;     // of block budget
    double worstNoteOnMs = 0.0;      // time noteOn holds the caller (I/O under lock)
    uint64_t renderAllocs = 0;
    uint64_t underruns = 0;
};

enum class Scenario { DroneOnly, PianoOnly, AllLayers };

Report run(const std::string& libDir, Scenario sc, double seconds) {
    Report rep;
    rep.name = sc == Scenario::DroneOnly ? "drone-only" : sc == Scenario::PianoOnly ? "piano-only" : "piano+synth+drone";

    StagePianoHost host(kSampleRate, libDir);
    host.setPianoEnabled(sc != Scenario::DroneOnly);
    host.setSynthEnabled(sc == Scenario::AllLayers);
    const uint64_t underrunsBefore = host.getDfdUnderrunCount();
    const uint64_t allocsBefore = g_renderAllocs.load();

    if (sc != Scenario::PianoOnly) host.triggerDroneKey(36);

    std::mt19937 rng(1234);
    std::uniform_int_distribution<int> noteDist(36, 96);
    std::uniform_int_distribution<int> velDist(20, 127);

    std::vector<float> buf(kBlock * 2);
    const uint64_t totalBlocks = static_cast<uint64_t>(seconds * kSampleRate / kBlock);
    const double budgetUs = kBlock / kSampleRate * 1e6;
    const uint64_t onsetGuard = static_cast<uint64_t>(0.010 * kSampleRate);

    std::vector<int> held;
    uint64_t lastOnsetFrame = 0;
    float prevL = 0.0f, prevR = 0.0f;
    double windowEnergy = 0.0;
    uint64_t windowFrames = 0;
    const uint64_t windowLen = static_cast<uint64_t>(0.1 * kSampleRate);
    const uint64_t droneSettleFrames = static_cast<uint64_t>(1.5 * kSampleRate); // after 800ms attack

    auto next = std::chrono::steady_clock::now();
    const auto blockDur = std::chrono::microseconds(static_cast<int64_t>(budgetUs));

    for (uint64_t b = 0; b < totalBlocks; ++b) {
        const uint64_t frame0 = b * kBlock;

        // Events: a new chord every ~250ms, pedal toggles every ~2s
        if (sc != Scenario::DroneOnly && b % 43 == 0) {
            for (int n : held) host.noteOff(n);
            held.clear();
            host.setSustainPedal((b / 344) % 2 == 0);
            for (int i = 0; i < 4; ++i) {
                int n = noteDist(rng);
                auto t0 = std::chrono::steady_clock::now();
                host.noteOn(n, velDist(rng));
                double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
                if (ms > rep.worstNoteOnMs) rep.worstNoteOnMs = ms;
                held.push_back(n);
            }
            lastOnsetFrame = frame0;
        }

        auto t0 = std::chrono::steady_clock::now();
        g_countAllocs = true;
        host.render(buf.data(), kBlock, 2);
        g_countAllocs = false;
        double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
        double pct = us / budgetUs * 100.0;
        if (pct > rep.worstRenderPct) rep.worstRenderPct = pct;

        for (uint32_t f = 0; f < kBlock; ++f) {
            float l = buf[f * 2], r = buf[f * 2 + 1];
            uint64_t absFrame = frame0 + f;
            if (!std::isfinite(l) || !std::isfinite(r)) { rep.nanCount++; continue; }
            float a = std::max(std::fabs(l), std::fabs(r));
            if (a > rep.peak) rep.peak = a;
            if (a > 0.85f) rep.limiterSamples++;
            bool inOnset = absFrame >= lastOnsetFrame && absFrame - lastOnsetFrame < onsetGuard;
            // A slow pad has tiny sample-to-sample slopes, so a loop-seam step is far smaller than a piano attack
            const float jumpLimit = sc == Scenario::DroneOnly ? 0.02f : 0.25f;
            if (!inOnset && (std::fabs(l - prevL) > jumpLimit || std::fabs(r - prevR) > jumpLimit)) rep.discontinuities++;
            prevL = l; prevR = r;

            if (sc == Scenario::DroneOnly && absFrame > droneSettleFrames) {
                windowEnergy += static_cast<double>(l) * l + static_cast<double>(r) * r;
                if (++windowFrames == windowLen) {
                    double rms = std::sqrt(windowEnergy / (2.0 * windowLen));
                    if (rms < 1e-4) rep.silentWindows++;
                    windowEnergy = 0.0;
                    windowFrames = 0;
                }
            }
        }
        rep.blocks++;

        next += blockDur;
        std::this_thread::sleep_until(next);
    }

    rep.renderAllocs = g_renderAllocs.load() - allocsBefore;
    rep.underruns = host.getDfdUnderrunCount() - underrunsBefore;
    return rep;
}

bool print(const Report& r) {
    bool bad = r.nanCount || r.discontinuities || r.silentWindows || r.renderAllocs || r.underruns ||
               r.worstRenderPct > 50.0 || r.worstNoteOnMs > 1.0;
    std::printf("\n== %s (%llu blocks) %s\n", r.name, static_cast<unsigned long long>(r.blocks), bad ? "FAIL" : "OK");
    std::printf("  NaN/Inf samples      : %llu\n", static_cast<unsigned long long>(r.nanCount));
    std::printf("  peak                 : %.3f\n", r.peak);
    std::printf("  limiter samples      : %llu\n", static_cast<unsigned long long>(r.limiterSamples));
    std::printf("  discontinuities      : %llu\n", static_cast<unsigned long long>(r.discontinuities));
    std::printf("  silent 100ms windows : %llu\n", static_cast<unsigned long long>(r.silentWindows));
    std::printf("  worst render         : %.1f%% of budget\n", r.worstRenderPct);
    std::printf("  worst noteOn         : %.3f ms\n", r.worstNoteOnMs);
    std::printf("  allocs in render     : %llu\n", static_cast<unsigned long long>(r.renderAllocs));
    std::printf("  DFD underruns        : %llu\n", static_cast<unsigned long long>(r.underruns));
    return !bad;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <libraryDir> [secondsPerScenario]\n", argv[0]);
        return 2;
    }
    std::string libDir = argv[1];
    double seconds = argc > 2 ? std::atof(argv[2]) : 20.0;
    std::string only = argc > 3 ? argv[3] : "drone,piano,all";

    bool ok = true;
    if (only.find("drone") != std::string::npos) ok &= print(run(libDir, Scenario::DroneOnly, seconds));
    if (only.find("piano") != std::string::npos) ok &= print(run(libDir, Scenario::PianoOnly, seconds));
    if (only.find("all") != std::string::npos) ok &= print(run(libDir, Scenario::AllLayers, seconds));
    std::printf("\nRESULT: %s\n", ok ? "CLEAN" : "GLITCH INDICATORS FOUND");
    return ok ? 0 : 1;
}
