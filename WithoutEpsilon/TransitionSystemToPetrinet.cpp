// main.cpp -- application entry point
//
// Pipeline:
//   Step 1 (unchanged, done once): generate all traces from events.txt.
//   Then: read x from the user; while (x) { run one iteration; ask again }.
//   Each iteration:
//     - asks how many random traces (n) to select this round
//     - picks n random traces -> SelectedTraces_iter<N>.txt
//     - builds a TransitionSystem from them (your existing class)
//     - runs the region-based k-search (regions.h) to find the smallest
//       k for which the TS is k-excitation-closed (bisimilar synthesis
//       possible), falling back to kmax-bounded minimal regions
//       (mining-only) if no such k exists
//     - derives a Petri net (Algorithm 1) from the resulting region cover
//     - appends a formatted section (selected traces, TS, k-result,
//       regions, Petri net) to synthesis_report.md
//
// NOTE ON INTEGRATION: this replaces the old Algo1 / RegionToPN /
// Petrinet2 classes with the fixed, verified regions.h library (see the
// conversation history: the original Algo1 had a floor-division bug,
// a truncated gradient scan, and an ad-hoc region-growth step that didn't
// generalize past k=1; regions.h fixes all three and is regression-tested
// against a brute-force ground truth, and now also implements Algorithm 1
// PN derivation with its own regression tests -- see regions_selftest.cpp).
// GenerateTraces.h and TransitionSystem.h are left untouched, since you
// said that part already works correctly.
//
// ADAPTER ASSUMPTIONS (toRegionsTS, below): states are 0-indexed
// integers, ts.event_transitions iterates as (eventName, vector of
// {.pre,.post}) -- exactly the pattern your original Step 3 printout
// already used, so this should need no changes. The one thing I could
// NOT infer from the pasted code is how your TransitionSystem exposes
// its initial state; INITIAL_STATE_OVERRIDE below defaults to 0
// (state "0" is the usual convention for a trace-built TS). If your
// TransitionSystem has an explicit initial-state accessor, wire it in
// at the marked line instead.

#include "GenerateTraces.h"
#include "TransitionSystem.h"
#include "regions.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <stdexcept>
#include <algorithm>

// ============================================================
// CONFIGURE HERE
// ============================================================
static const char* EVENTS_FILE      = "events.txt";           // input: "a b c d"
static const char* ALL_TRACES_FILE  = "output/AllPossibleTraces.txt";
static const char* REPORT_FILE      = "output/synthesis_report.md";
static const int   DEFAULT_KMAX     = 3;
static const int   DEFAULT_N_TRACES = 4;
static const int   INITIAL_STATE_OVERRIDE = 0; // <-- adjust if your TS exposes a real initial state
// ============================================================

// ---------------------------------------------------------------------
// Adapter: your TransitionSystem -> regions::TransitionSystem
// ---------------------------------------------------------------------
static regions::TransitionSystem toRegionsTS(const TransitionSystem& ts) {
    regions::TransitionSystem rts;
    rts.initialState = INITIAL_STATE_OVERRIDE;

    int maxState = 0;
    for (const auto& [ev, transList] : ts.event_transitions) {
        for (const auto& t : transList) {
            rts.event_transitions[ev].push_back({t.pre, t.post});
            maxState = std::max({maxState, t.pre, t.post});
        }
    }
    rts.numStates = maxState + 1;
    return rts;
}

// ---------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------
static std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return std::string(buf);
}

// Reads an int from stdin; keeps `fallback` if the line is empty or
// not a valid integer.
static int readIntOrDefault(const std::string& prompt, int fallback) {
    std::cout << prompt << " [default " << fallback << "]: ";
    std::string line;
    std::getline(std::cin, line);
    if (line.empty()) return fallback;
    try { return std::stoi(line); } catch (...) { return fallback; }
}

static void writeSelectedTraces(std::ofstream& out, const std::string& selectedFile) {
    std::ifstream in(selectedFile);
    out << "### Selected traces\n\n```\n";
    std::string line;
    int count = 0;
    while (std::getline(in, line)) { out << line << "\n"; ++count; }
    out << "```\n\n(" << count << " trace(s) selected, from `" << selectedFile << "`)\n\n";
}

static void writeTransitionSystem(std::ofstream& out, const TransitionSystem& ts) {
    out << "### Transition system\n\n";
    out << "- States: " << ts.states.size() << "\n";
    out << "- Event types: " << ts.event_transitions.size() << "\n\n";
    out << "```\n";
    for (const auto& [ev, transitions] : ts.event_transitions) {
        out << "Event '" << ev << "': " << transitions.size() << " transition(s)\n";
        for (const auto& t : transitions)
            out << "  " << t.pre << " -> " << t.post << "\n";
    }
    out << "```\n\n";
}

static void writeRegionsSection(std::ofstream& out, const std::string& title,
                                 const std::vector<regions::Region>& regs) {
    out << "**" << title << "** (" << regs.size() << " region(s)):\n\n```\n";
    for (size_t i = 0; i < regs.size(); ++i)
        out << "  r" << i << " = " << regions::regionCompact(regs[i]) << "\n";
    out << "```\n\n";
}

// ---------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------
int main() {
    try {
        // ------------------------------------------------------------
        // Step 1: Generate all traces (unchanged, done once)
        // ------------------------------------------------------------
        // std::cout << "=== Step 1: Generating all traces ===\n";
        // int total = generateAllTraces(EVENTS_FILE, ALL_TRACES_FILE);
        // std::cout << "Total traces generated: " << total << "\n\n";

        int kmax = readIntOrDefault("Max region-weight bound to search (kmax)", DEFAULT_KMAX);

        // Fresh report for this run.
        {
            std::ofstream reset(REPORT_FILE, std::ios::trunc);
            if (!reset) throw std::runtime_error(std::string("could not open ") + REPORT_FILE);
            reset << "# Region-based synthesis report\n\n"
                  << "Run started: " << timestamp() << "\n\n"
                  << "kmax for this run: " << kmax << "\n\n"
                  << "---\n\n";
        }

        // int iteration = 0;
        // int x = 5;
        // while (x) {
        //     ++iteration;
            //std::cout << "\n--- Iteration " << iteration << " ---\n";

            // int n = readIntOrDefault("How many random traces to select (n)?", DEFAULT_N_TRACES);

            // std::string selectedFile = "output/SelectedTraces_iter" + std::to_string(iteration) + ".txt";
            // pickRandomTraces(ALL_TRACES_FILE, n, selectedFile);

            // ---- build TS (your existing, working class) ----
            string selectedFile = "transition_system.txt";
            TransitionSystem ts;
            ts.loadFromFile(selectedFile);
            std::cout << "States: " << ts.states.size()
                      << ", Event types: " << ts.event_transitions.size() << "\n";

            // ---- region synthesis (fixed regions.h library) ----
            regions::TransitionSystem rts = toRegionsTS(ts);
            regions::SynthesisSearchResult result = regions::findMinimalKForSynthesis(rts, kmax);
            regions::PetriNet pn = regions::derivePetriNet(rts, result.irredundantCover);

            std::cout << "  k=" << result.k
                      << (result.bisimilarPossible
                              ? " (bisimilar synthesis possible)"
                              : " (bisimilar NOT reached by kmax -- mining-only PN)")
                      << ", cover size=" << result.irredundantCover.size() << "\n";

            // ---- append the report section ----
            std::ofstream out(REPORT_FILE, std::ios::app);
            if (!out) throw std::runtime_error(std::string("could not reopen ") + REPORT_FILE);

            //out << "## Iteration " << iteration << " -- " << timestamp() << "\n\n";
            //out << "- Requested random traces (n): " << n << "\n";
            out << "- kmax searched: " << kmax << "\n\n";

            writeSelectedTraces(out, selectedFile);
            writeTransitionSystem(out, ts);

            out << "### Region synthesis result\n\n";
            out << "- Smallest k reached: **" << result.k << "**\n";
            out << "- Excitation closed (bisimilar synthesis possible): **"
                << (result.bisimilarPossible ? "YES" : "NO (mining-only overapproximation)") << "**\n\n";

            writeRegionsSection(out, "All minimal k-bounded regions", result.minimalRegions);
            writeRegionsSection(out, "Irredundant cover (used to derive the Petri net below)",
                                 result.irredundantCover);

            out << "### Derived Petri net\n\n```\n";
            std::ostringstream pnStream;
            regions::printPetriNet(pnStream, pn);
            out << pnStream.str();
            out << "```\n\n---\n\n";
            out.close();

            std::cout << "  Report section written to " << REPORT_FILE << "\n";

            // x = readIntOrDefault("\nRun another iteration? (1 = yes, 0 = stop)", 0);
        //}

        std::cout << "\nDone. Full report in " << REPORT_FILE << "\n";

    } catch (const std::exception& ex) {
        std::cerr << "ERROR: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
