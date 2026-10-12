#include "GenerateTraces.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static std::vector<std::string> readEvents(const std::string& filename) {
    std::ifstream f(filename);
    if (!f.is_open())
        throw std::runtime_error("Cannot open events file: " + filename);

    std::vector<std::string> events;
    std::string token;
    // Accept events separated by any whitespace (spaces, tabs, newlines)
    while (f >> token)
        events.push_back(token);

    if (events.empty())
        throw std::runtime_error("No events found in: " + filename);

    return events;
}

static std::string joinArrow(const std::vector<std::string>& events,
                              int start, int len) {
    std::string out;
    for (int i = start; i < start + len; ++i) {
        if (i != start) out += "->";
        out += events[i];
    }
    return out;
}

// ---------------------------------------------------------------------------
// generateAllTraces
//
// Strategy:
//   For each non-empty subset (via bitmask over 2^n - 1 masks):
//     Collect the subset's events into a vector.
//     Sort it to get the lexicographically-first permutation.
//     Generate every permutation with next_permutation and write.
//
// Total traces = sum_{k=1}^{n} C(n,k) * k!  =  sum_{k=1}^{n} n!/(n-k)!
// E.g. n=4 → 1*4 + 2*6 + 6*4 + 24*1 = 4+12+24+24 = 64 traces
// ---------------------------------------------------------------------------

int generateAllTraces(const std::string& events_file,
                      const std::string& output_file) {
    std::vector<std::string> events = readEvents(events_file);
    int n = static_cast<int>(events.size());

    std::ofstream out(output_file);
    if (!out.is_open())
        throw std::runtime_error("Cannot open output file: " + output_file);

    int total = 0;
    int num_subsets = 1 << n;  // 2^n

    for (int mask = 1; mask < num_subsets; ++mask) {
        // Collect events in this subset (preserve original order for first perm)
        std::vector<std::string> subset;
        for (int i = 0; i < n; ++i)
            if (mask & (1 << i))
                subset.push_back(events[i]);

        // Sort to ensure next_permutation covers all orderings
        std::sort(subset.begin(), subset.end());

        do {
            // Write as arrow-separated trace
            for (size_t i = 0; i < subset.size(); ++i) {
                if (i) out << "->";
                out << subset[i];
            }
            out << "\n";
            ++total;
        } while (std::next_permutation(subset.begin(), subset.end()));
    }

    out.close();
    std::cout << "[generateAllTraces] Wrote " << total
              << " traces to " << output_file << "\n";
    return total;
}

// ---------------------------------------------------------------------------
// pickRandomTraces
//
// Reads all lines from traces_file, then does a Fisher-Yates shuffle on
// indices and picks the first n (without replacement).
// If n >= total lines, all traces are written (shuffled).
// ---------------------------------------------------------------------------

void pickRandomTraces(const std::string& traces_file,
                      int n,
                      const std::string& output_file) {
    std::ifstream in(traces_file);
    if (!in.is_open())
        throw std::runtime_error("Cannot open traces file: " + traces_file);

    std::vector<std::string> all_traces;
    std::string line;
    while (std::getline(in, line)) {
        // skip blanks / comments
        std::string trimmed = line;
        while (!trimmed.empty() && isspace((unsigned char)trimmed.front()))
            trimmed.erase(trimmed.begin());
        if (!trimmed.empty() && trimmed[0] != '#')
            all_traces.push_back(trimmed);
    }
    in.close();

    if (all_traces.empty())
        throw std::runtime_error("No traces found in: " + traces_file);

    int pick = std::min(n, static_cast<int>(all_traces.size()));
    if (n > static_cast<int>(all_traces.size())) {
        std::cerr << "[pickRandomTraces] Warning: requested " << n
                  << " but only " << all_traces.size() << " available. "
                  << "Picking all.\n";
    }

    // Fisher-Yates partial shuffle — only shuffle the first `pick` positions
    std::mt19937 rng(std::random_device{}());
    for (int i = 0; i < pick; ++i) {
        std::uniform_int_distribution<int> dist(i, static_cast<int>(all_traces.size()) - 1);
        std::swap(all_traces[i], all_traces[dist(rng)]);
    }

    std::ofstream out(output_file);
    if (!out.is_open())
        throw std::runtime_error("Cannot open output file: " + output_file);

    for (int i = 0; i < pick; ++i)
        out << all_traces[i] << "\n";

    out.close();
    std::cout << "[pickRandomTraces] Picked " << pick
              << " traces → " << output_file << "\n";
}
