#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "cabinos/intent_router.hpp"
#include "cabinos/service_broker.hpp"

namespace {

struct Args {
    std::string label = "unnamed";
    std::string command = "find coffee on my route";
    int iterations = 100;
    bool cloud_online = false;
};

double Percentile(std::vector<double> samples, const double p) {
    if (samples.empty()) {
        return 0.0;
    }
    std::sort(samples.begin(), samples.end());
    const double pos = (p / 100.0) * static_cast<double>(samples.size() - 1);
    const size_t lo = static_cast<size_t>(pos);
    const size_t hi = std::min(lo + 1, samples.size() - 1);
    const double frac = pos - static_cast<double>(lo);
    return samples[lo] + frac * (samples[hi] - samples[lo]);
}

Args ParseArgs(const int argc, char** argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        const std::string token = argv[i];
        if (token == "--label" && i + 1 < argc) {
            args.label = argv[++i];
        } else if (token == "--iterations" && i + 1 < argc) {
            args.iterations = std::max(1, std::atoi(argv[++i]));
        } else if (token == "--cloud-online" && i + 1 < argc) {
            args.cloud_online = (std::string(argv[++i]) == "1");
        } else if (token == "--command" && i + 1 < argc) {
            args.command = argv[++i];
            while (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) {
                args.command += " ";
                args.command += argv[++i];
            }
        }
    }
    return args;
}

}  // namespace

int main(int argc, char** argv) {
    const Args args = ParseArgs(argc, argv);

    cabinos::IntentRouter router;
    cabinos::ServiceBroker broker(router);

    // Warmup.
    for (int i = 0; i < 5; ++i) {
        (void)broker.HandleTextCommand(args.command, args.cloud_online);
    }

    std::vector<double> samples_ms;
    samples_ms.reserve(static_cast<size_t>(args.iterations));
    for (int i = 0; i < args.iterations; ++i) {
        const auto start = std::chrono::steady_clock::now();
        (void)broker.HandleTextCommand(args.command, args.cloud_online);
        const auto end = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(end - start).count();
        samples_ms.push_back(ms);
    }

    const double avg =
        std::accumulate(samples_ms.begin(), samples_ms.end(), 0.0) / static_cast<double>(samples_ms.size());
    const double p50 = Percentile(samples_ms, 50.0);
    const double p95 = Percentile(samples_ms, 95.0);
    const double p99 = Percentile(samples_ms, 99.0);

    std::cout << "label=" << args.label << "\n";
    std::cout << "iterations=" << args.iterations << "\n";
    std::cout << "avg_ms=" << avg << "\n";
    std::cout << "p50_ms=" << p50 << "\n";
    std::cout << "p95_ms=" << p95 << "\n";
    std::cout << "p99_ms=" << p99 << "\n";
    return 0;
}
