#ifndef TRANSITION_SYSTEM_H
#define TRANSITION_SYSTEM_H
#include <vector>
#include <string>
#include <map>
#include <set>
#include <iostream>
#include <ostream>
using namespace std;

// struct TransitionSystem {
//     // event name → list of (pre_state, post_state)
//     map<string, vector<pair<int,int>>> event_transitions;
//     int initial_state;
//     set<int> states; //all states in the TS, and final states (if any)
//     void loadFromFile(const string& filename);
//     void loadFromTraceFile(const string& filename);
// };

using State  = int;
using Event  = std::string;

// A region (or partial candidate) is a multiset over the state set,
// represented densely: region[s] = multiplicity at state s.
using Region = std::vector<int>;

struct Transition {
    public:
    State pre, post;
};

struct TransitionSystem {
    int numStates = 0;
    State initialState = 0;
    set<State> states; // all states in the TS
    // event -> list of (pre,post) transitions labeled with that event
    std::map<Event, std::vector<Transition>> event_transitions;

    void addTransition(State pre, const Event& e, State post) {
        numStates = std::max(numStates, std::max(pre, post) + 1);
        states.insert(pre);
        states.insert(post);
        event_transitions[e].push_back({pre, post});
    }
    void writeDot(std::ostream& os) const {
        os << "digraph TransitionSystem {\n";
        os << "  rankdir=LR;\n";
        os << "  node [shape=circle, fontsize=10];\n";
        os << "  __start [shape=point, width=0.1];\n";
        os << "  __start -> " << initialState << ";\n";
        for (State s : states) {
            os << "  " << s << " [label=\"" << s << "\"];\n";
        }
        for (const auto& [ev, transList] : event_transitions) {
            for (const auto& t : transList) {
                os << "  " << t.pre << " -> " << t.post << " [label=\"" << ev << "\"];\n";
            }
        }
        os << "}\n";
    }

    void loadFromFile(const string& filename);
    void loadFromTraceFile(const string& filename);
    void loadFromTraceFileWithRepetitions(const string& filename, int repeatThreshold = 3);
};

#endif