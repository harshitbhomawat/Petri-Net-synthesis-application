#include "TransitionSystem.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cctype>
#include <memory>
#include <tuple>

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



// =============================================================================
// loadFromTraceFile — extended to support a small trace-expression grammar:
//     concatenation  : "a->b->c"  or  "abc"
//     alternation    : "b+c"        (means "b OR c")
//     Kleene star    : "(b+c)*"     (means zero-or-more repetitions)
//     grouping       : "( ... )"
//
// Example: "a(b+c)*d" -> a, then any interleaving of b's and c's, then d.
//
// This is a pure superset of the old behaviour: any old-style flat trace
// like "a->b->c" is just a CONCAT of plain events and parses/compiles
// identically to before, including prefix sharing across multiple lines.
//
// Everything below is file-local (static / anonymous namespace) — the
// header (TransitionSystem.h) is NOT modified by this change.
// =============================================================================

namespace {

// ---- AST -------------------------------------------------------------------
enum class NodeType { EVENT, CONCAT, UNION, STAR };

struct Node {
    NodeType type;
    string name;                     // used only when type == EVENT
    vector<std::unique_ptr<Node>> children;
};
using NodePtr = std::unique_ptr<Node>;

// ---- Tokenizer ---------------------------------------------------------------
enum class TokType { EVENT, LPAREN, RPAREN, PLUS, STAR, END };
struct Token { TokType type; string text; };

vector<Token> tokenize(const string& line) {
    vector<Token> tokens;
    size_t i = 0, n = line.size();
    while (i < n) {
        char c = line[i];
        if (isspace((unsigned char)c)) { i++; continue; }
        if (c == '-' && i + 1 < n && line[i+1] == '>') { i += 2; continue; } // "->" = separator
        if (c == '(') { tokens.push_back({TokType::LPAREN, "("}); i++; continue; }
        if (c == ')') { tokens.push_back({TokType::RPAREN, ")"}); i++; continue; }
        if (c == '+') { tokens.push_back({TokType::PLUS,   "+"}); i++; continue; }
        if (c == '*') { tokens.push_back({TokType::STAR,   "*"}); i++; continue; }
        if (isalnum((unsigned char)c) || c == '_') {
            size_t start = i;
            while (i < n && (isalnum((unsigned char)line[i]) || line[i] == '_')) i++;
            tokens.push_back({TokType::EVENT, line.substr(start, i - start)});
            continue;
        }
        i++; // skip unrecognized character (e.g. stray '\r')
    }
    tokens.push_back({TokType::END, ""});
    return tokens;
}

// ---- Parser (recursive descent) ---------------------------------------------
// Grammar:  expr := seq ('+' seq)*
//           seq  := term+
//           term := atom '*'?
//           atom := EVENT | '(' expr ')'
struct Parser {
    const vector<Token>& toks;
    size_t pos = 0;
    explicit Parser(const vector<Token>& t) : toks(t) {}

    const Token& peek() const { return toks[pos]; }
    Token advance() { return toks[pos++]; }

    NodePtr parseAtom() {
        if (peek().type == TokType::EVENT) {
            auto node = std::make_unique<Node>();
            node->type = NodeType::EVENT;
            node->name = advance().text;
            return node;
        }
        if (peek().type == TokType::LPAREN) {
            advance();
            NodePtr inner = parseExpr();
            if (peek().type == TokType::RPAREN) advance();
            return inner;
        }
        auto node = std::make_unique<Node>(); // malformed input fallback
        node->type = NodeType::EVENT;
        node->name = "";
        return node;
    }

    NodePtr parseTerm() {
        NodePtr atom = parseAtom();
        if (peek().type == TokType::STAR) {
            advance();
            auto star = std::make_unique<Node>();
            star->type = NodeType::STAR;
            star->children.push_back(std::move(atom));
            return star;
        }
        return atom;
    }

    NodePtr parseSeq() {
        auto node = std::make_unique<Node>();
        node->type = NodeType::CONCAT;
        while (peek().type == TokType::EVENT || peek().type == TokType::LPAREN) {
            node->children.push_back(parseTerm());
        }
        if (node->children.size() == 1) return std::move(node->children[0]);
        return node;
    }

    NodePtr parseExpr() {
        NodePtr first = parseSeq();
        if (peek().type != TokType::PLUS) return first;
        auto node = std::make_unique<Node>();
        node->type = NodeType::UNION;
        node->children.push_back(std::move(first));
        while (peek().type == TokType::PLUS) {
            advance();
            node->children.push_back(parseSeq());
        }
        return node;
    }
};

// ---- Compiler ----------------------------------------------------------------
// Threads a `forcedTarget` through the recursion so UNION branches and STAR
// self-loops converge onto real states directly -- no epsilon transitions
// needed, so this fits TransitionSystem's existing (pre,event,post) model
// with zero header changes.
//
//   forcedTarget == -1  -> normal step (reuse trieChildren, same as before)
//   forcedTarget != -1  -> this node's exit MUST be exactly that state
//
// `addedEdges` is a local dedup guard: TransitionSystem::addTransition() does
// not itself dedupe, so we avoid ever calling it twice for the same
// (pre,event,post) triple, which could otherwise happen if two different
// trace lines share an identical prefix leading into an identical
// union/star sub-pattern.
int compileNode(TransitionSystem& ts, Node* node, int current, int forcedTarget,
                 std::map<int, std::map<string,int>>& trieChildren,
                 std::set<std::tuple<int,string,int>>& addedEdges,
                 int& nextStateId) {

    auto allocState = [&]() {
        int id = nextStateId++;
        ts.states.insert(id);
        return id;
    };
    auto addEdge = [&](int from, const string& ev, int to) {
        auto key = std::make_tuple(from, ev, to);
        if (addedEdges.count(key)) return; // already added, skip duplicate
        addedEdges.insert(key);
        ts.addTransition(from, ev, to);
    };

    switch (node->type) {

        case NodeType::EVENT: {
            const string& ev = node->name;
            if (ev.empty()) return (forcedTarget != -1) ? forcedTarget : current;

            if (forcedTarget == -1) {
                // Normal step: reuse shared prefix if it already exists.
                auto& children = trieChildren[current];
                auto it = children.find(ev);
                if (it != children.end()) return it->second;
                int next = allocState();
                children[ev] = next;
                addEdge(current, ev, next);
                return next;
            } else {
                // Forced convergence: land exactly on forcedTarget.
                addEdge(current, ev, forcedTarget);
                return forcedTarget;
            }
        }

        case NodeType::CONCAT: {
            int cur = current;
            size_t k = node->children.size();
            for (size_t i = 0; i < k; i++) {
                int ft = (i == k - 1) ? forcedTarget : -1;
                cur = compileNode(ts, node->children[i].get(), cur, ft, trieChildren, addedEdges, nextStateId);
            }
            return cur;
        }

        case NodeType::UNION: {
            int join = (forcedTarget != -1) ? forcedTarget : allocState();
            for (auto& alt : node->children) {
                compileNode(ts, alt.get(), current, join, trieChildren, addedEdges, nextStateId);
            }
            return join;
        }

        case NodeType::STAR: {
            int loopPoint = current;
            compileNode(ts, node->children[0].get(), loopPoint, loopPoint, trieChildren, addedEdges, nextStateId);
            return loopPoint; // star never advances `current` by itself
        }
    }
    return current;
}

} // anonymous namespace


void TransitionSystem::loadFromTraceFile(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()){
        cout << "Error opening file: " << filename << std::endl;
        return;
    }

    // trieChildren[state][event] = next_state  (only used on the normal,
    // non-forced path, so plain concatenation traces still prefix-merge
    // exactly as before).
    map<int, map<string, int>> trieChildren;
    std::set<std::tuple<int,string,int>> addedEdges;

    int next_state_id = 0;
    int root = next_state_id++;   // state 0 is the initial state
    states.insert(root);
    initialState = root;

    string line;
    while (getline(file, line)) {
        // skip empty lines and comments (lines starting with '#')
        while (!line.empty() && isspace((unsigned char)line.front())) line.erase(line.begin());
        if (line.empty() || line[0] == '#') continue;

        vector<Token> tokens = tokenize(line);
        Parser parser(tokens);
        NodePtr ast = parser.parseExpr();
        if (!ast) continue;

        compileNode(*this, ast.get(), root, -1, trieChildren, addedEdges, next_state_id);
    }
}
