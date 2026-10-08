#ifndef GENERATE_TRACES_H
#define GENERATE_TRACES_H

#include <string>

// Reads event names from `events_file` (space/newline separated, e.g. "a b c d")
// Writes every permutation of every non-empty subset to AllPossibleTraces.txt
// Returns total number of traces written.
int generateAllTraces(const std::string& events_file,
                      const std::string& output_file = "AllPossibleTraces.txt");

// Picks `n` random traces from `traces_file` (without replacement if possible)
// and writes them to `output_file`.
void pickRandomTraces(const std::string& traces_file,
                      int n,
                      const std::string& output_file = "SelectedTraces.txt");

#endif
