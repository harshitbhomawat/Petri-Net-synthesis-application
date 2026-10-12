#include "TransitionSystem.h"
#include <fstream>
#include <iostream>
#include <sstream>

// void TransitionSystem::loadFromFile(const string& filename) {
//     std::ifstream infile(filename);
//     if (!infile.is_open()) {
//         std::cout << "Error opening file: " << filename << std::endl;
//         return;
//     }
//     std::string line;
//     while (std::getline(infile, line)) {
//         if (line.empty() || line[0] == '#') continue; // skip comments and empty lines
//         std::stringstream ss(line);
//         std::string type;
//         ss >> type;
//         if (type == "states") {
//             std::string word;
//             while(ss>>word)
//             {
//                 states[word] = states.size();
//             }
//         } 
//         else if (type == "events") {
//             std::string word;
//             while(ss>>word)
//             {
//                 events[word] = events.size();
//             }
//         }
//         else if (type == "transition") {
//             std::string evnt, pre_state, post_state;
//             ss>>pre_state>>evnt>>post_state;
//             event_transitions[evnt].push_back({states[pre_state], states[post_state]});
//         }
//     }
//     infile.close();
// }


void TransitionSystem::loadFromFile(const string& filename) {
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cout << "Error opening file: " << filename << std::endl;
        return;
    }
    std::string line;
    while (std::getline(infile, line)) {
        if (line.empty() || line[0] == '#') continue; // skip comments and empty lines
        std::stringstream ss(line);
        std::string type;
        ss >> type;
        if (type == "states") {
            int num_states;
            ss >> num_states >> TransitionSystem::initialState;
            for (int i = 0; i < num_states; ++i) {
                states.insert(i);
            }
        }
        else if (type == "transition") {
            std::string evnt;
            int pre_state_int, post_state_int;
            ss >> pre_state_int >> evnt >> post_state_int;
            addTransition(pre_state_int, evnt, post_state_int);
            //event_transitions[evnt].push_back({pre_state_int, post_state_int});
        }
    }
    infile.close();
}



// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Split a single trace string like "a->b->c" into ["a","b","c"]
static vector<string> parseTrace(const string& line) {
    vector<string> events;
    string token;
    size_t pos = 0;
    while (pos < line.size()) {
        size_t arrow = line.find("->", pos);
        if (arrow == string::npos) {
            // last token
            token = line.substr(pos);
            // trim whitespace
            while (!token.empty() && isspace((unsigned char)token.front())) token.erase(token.begin());
            while (!token.empty() && isspace((unsigned char)token.back()))  token.pop_back();
            if (!token.empty()) events.push_back(token);
            break;
        }
        token = line.substr(pos, arrow - pos);
        while (!token.empty() && isspace((unsigned char)token.front())) token.erase(token.begin());
        while (!token.empty() && isspace((unsigned char)token.back()))  token.pop_back();
        if (!token.empty()) events.push_back(token);
        pos = arrow + 2; // skip "->"
    }
    return events;
}

// ---------------------------------------------------------------------------
// Core algorithm: prefix-tree (trie) construction
//
// Each node in the trie maps  event → child node id.
// We walk every trace; when a (state, event) pair already exists we reuse
// the existing child state (prefix sharing).  When it does not exist we
// allocate a new state.  This gives the canonical prefix-merged TS.
// ---------------------------------------------------------------------------

void TransitionSystem::loadFromTraceFile(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()){
        cout << "Error opening file: " << filename << std::endl;
        return;
    }

    // trie_children[state][event] = next_state
    map<int, map<string, int>> trie_children;

    int next_state_id = 0;
    int root = next_state_id++;   // state 0 is the initial state
    states.insert(root);
    initialState = root;

    string line;
    while (getline(file, line)) {
        // skip empty lines and comments (lines starting with '#')
        while (!line.empty() && isspace((unsigned char)line.front())) line.erase(line.begin());
        if (line.empty() || line[0] == '#') continue;

        vector<string> trace = parseTrace(line);
        if (trace.empty()) continue;

        int current = root;

        for (const string& event : trace) {
            auto& children = trie_children[current];
            auto it = children.find(event);

            if (it != children.end()) {
                // prefix already exists — reuse the child state
                current = it->second;
            } else {
                // allocate a new state
                int next = next_state_id++;
                states.insert(next);
                children[event] = next;

                // record the transition
                //event_transitions[event].emplace_back(current, next);
                addTransition(current, event, next);

                current = next;
            }
        }
    }
}
