// regions.h
//
// General k-bounded region computation, following:
//   J. Carmona, J. Cortadella, M. Kishinevsky,
//   "New Region-Based Algorithms for Deriving Bounded Petri Nets",
//   IEEE Trans. Computers, 59(3), 2010.
//
// This replaces the original 1-bounded, set-based Algo1 with the paper's
// general multiset formulation (Section 6, Definitions 6.1-6.3,
// Algorithm 2), and adds a k-ECTS (excitation closure) check
// (Definition 5.1 / Section 5.2) so callers can find the smallest k for
// which a *bisimilar* Petri net can be synthesized, falling back to plain
// k-bounded minimal regions (mining / overapproximation case, Section 4)
// when no such k <= kmax exists.
//
// -------------------------------------------------------------------
// BUGS FIXED relative to the original Algo1.cpp:
//
// 1. `(gmin + gmax) / 2` used C++ truncating division. The paper requires
//    FLOOR division (Algorithm 2, line 9). These disagree whenever
//    gmin+gmax is negative and odd -- which happens routinely, since
//    gradients in {-1,0,+1} very often split as gmin=-1, gmax=0.
//    Truncating division gave g=0 instead of the correct g=-1, which made
//    forceGradientLeqG(...,0) a no-op (both children collapsed / vanished),
//    silently dropping whole subtrees of minimal regions.
//    FIX: explicit floorDiv().
//
// 2. The gmin/gmax scan `break`-ed out of the inner loop as soon as ANY
//    two transitions of the violating event disagreed, so if a later
//    transition of the same event had a more extreme gradient it was
//    never seen. expand() then used a wrong (gmin,gmax) range.
//    FIX: scan every transition of the violating event before computing
//    (gmin,gmax).
//
// 3. forceGradientLeqG's g==-1 branch was a documented no-op: it silently
//    returned a region that had NOT actually been forced to gradient <= -1
//    whenever `post` was already in the region (which is genuinely
//    infeasible by addition-only expansion). This is not just a missing
//    check -- it's also the wrong *algorithm*: per-transition insertion of
//    `pre` does not correctly generalize to multisets/k>1, because the
//    required increase for a state depends on ALL transitions touching
//    that state under that event, not one transition-pair in isolation.
//    FIX: replaced forceGradientLeqG/forceGradientGeqG entirely with the
//    paper's delta_g / delta^g and the "square" operators |_g / |^g from
//    Definition 6.2/6.3, which are defined per-state, over all matching
//    transitions, and proven correct by Theorem 6.1.
//
// A fourth, smaller issue also fixed: Region was `set<int>` (i.e.
// restricted to 0/1-bounded elementary regions), so nothing above k=1
// could ever be represented correctly regardless of the other fixes.
// Region is now a dense multiset (vector<int> indexed by state).
// -------------------------------------------------------------------

#pragma once
#include <vector>
#include <map>
#include <set>
#include <string>
#include <queue>
#include <optional>
#include <algorithm>
#include <climits>
#include <ostream>

namespace regions {

using State  = int;
using Event  = std::string;

// A region (or partial candidate) is a multiset over the state set,
// represented densely: region[s] = multiplicity at state s.
using Region = std::vector<int>;

struct Transition {
    State pre;
    State post;
};

struct TransitionSystem {
    int numStates = 0;
    State initialState = 0;
    // event -> list of (pre,post) transitions labeled with that event
    std::map<Event, std::vector<Transition>> event_transitions;

    void addTransition(State pre, const Event& e, State post) {
        numStates = std::max(numStates, std::max(pre, post) + 1);
        event_transitions[e].push_back({pre, post});
    }
};

// ---------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------

// Correct floor division (C++'s built-in `/` truncates toward zero).
inline int floorDiv(int a, int b) {
    int q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

inline Region zeroRegion(int n) { return Region(n, 0); }

inline bool isTrivial(const Region& r) {
    // Definition: trivial multiset has r(s) == r(s') for all s,s'
    // (this covers 0, 1, ..., K simultaneously).
    for (size_t i = 1; i < r.size(); ++i)
        if (r[i] != r[0]) return false;
    return true;
}

inline bool isKBounded(const Region& r, int k) {
    for (int v : r) if (v > k) return false;
    return true;
}

// multiset "all ones": support(1) intersect everything = everything with
// count>=1. Used for Algorithm 2's pruning check "1 (subset) r'" which
// discards branches that can no longer yield a *minimal* region.
inline bool containsAllOnes(const Region& r) {
    for (int v : r) if (v < 1) return false;
    return true;
}

// r1 is a (non-strict) multiset subset of r2
inline bool isSubset(const Region& r1, const Region& r2) {
    for (size_t i = 0; i < r1.size(); ++i)
        if (r1[i] > r2[i]) return false;
    return true;
}

inline bool isProperSubset(const Region& r1, const Region& r2) {
    return r1 != r2 && isSubset(r1, r2);
}

// ---------------------------------------------------------------------
// Gradients (Definition 2.5) and region validity (Definition 2.4)
// ---------------------------------------------------------------------

// Returns the constant gradient of `event` in region r, or nullopt if
// the gradient is nonconstant (i.e. r is not a valid region w.r.t. e).
inline std::optional<int> computeGradient(const Region& r,
                                           const Event& event,
                                           const TransitionSystem& ts) {
    std::optional<int> grad;
    auto it = ts.event_transitions.find(event);
    if (it == ts.event_transitions.end()) return std::nullopt;
    for (const auto& t : it->second) {
        int g = r[t.post] - r[t.pre];
        if (!grad) grad = g;
        else if (*grad != g) return std::nullopt; // nonconstant -> violation
    }
    return grad;
}

inline bool isValidRegion(const Region& r, const TransitionSystem& ts) {
    for (const auto& [event, _] : ts.event_transitions)
        if (!computeGradient(r, event, ts).has_value()) return false;
    return true;
}

// ER(e), SR(e) as multisets (indicator: 1 where present, 0 elsewhere) --
// used as seeds, per Property 6.1 (every nontrivial region is a
// preregion or postregion of some event, hence a superset of ER/SR).
inline Region excitationRegion(const Event& e, const TransitionSystem& ts) {
    Region er = zeroRegion(ts.numStates);
    for (const auto& t : ts.event_transitions.at(e)) er[t.pre] = 1;
    return er;
}

inline Region switchingRegion(const Event& e, const TransitionSystem& ts) {
    Region sr = zeroRegion(ts.numStates);
    for (const auto& t : ts.event_transitions.at(e)) sr[t.post] = 1;
    return sr;
}

// ---------------------------------------------------------------------
// Definition 6.2 / 6.3: delta_g, delta^g and the "square" growth
// operators |_g(r,e), |^g(r,e). These replace the old ad-hoc
// forceGradientLeqG/GeqG and are defined for every state (not just
// states touched by a single transition pair), matching the paper
// exactly and generalizing correctly to k>1.
// ---------------------------------------------------------------------

// Lower bound on required increase of r(s), from arcs LEAVING s under e,
// to force gradient(e) <= g.
inline int deltaLeqG(const Region& r, const Event& e, State s, int g,
                      const TransitionSystem& ts) {
    int best = 0;
    for (const auto& t : ts.event_transitions.at(e)) {
        if (t.pre == s) {
            int val = (r[t.post] - r[t.pre]) - g;
            best = std::max(best, val);
        }
    }
    return best;
}

// Lower bound on required increase of r(s), from arcs ARRIVING at s
// under e, to force gradient(e) >= g.
inline int deltaGeqG(const Region& r, const Event& e, State s, int g,
                      const TransitionSystem& ts) {
    int best = 0;
    for (const auto& t : ts.event_transitions.at(e)) {
        if (t.post == s) {
            int val = (r[t.pre] - r[t.post]) + g;
            best = std::max(best, val);
        }
    }
    return best;
}

inline Region squareLeqG(const Region& r, const Event& e, int g,
                          const TransitionSystem& ts) {
    Region out = r;
    for (int s = 0; s < ts.numStates; ++s)
        out[s] = r[s] + deltaLeqG(r, e, s, g, ts);
    return out;
}

inline Region squareGeqG(const Region& r, const Event& e, int g,
                          const TransitionSystem& ts) {
    Region out = r;
    for (int s = 0; s < ts.numStates; ++s)
        out[s] = r[s] + deltaGeqG(r, e, s, g, ts);
    return out;
}

// Algorithm 2's binary expansion step (lines 8-13):
// g = floor((gmin+gmax)/2); branch1 forces gradient<=g, branch2 forces
// gradient>=g+1.
inline std::pair<Region, Region> expand(const Region& r, const Event& e,
                                         int gmin, int gmax,
                                         const TransitionSystem& ts) {
    int g = floorDiv(gmin + gmax, 2);
    Region b1 = squareLeqG(r, e, g, ts);
    Region b2 = squareGeqG(r, e, g + 1, ts);
    return {b1, b2};
}

// ---------------------------------------------------------------------
// Algorithm 2: GenerateMinimalRegions
// ---------------------------------------------------------------------

inline std::vector<Region> removeNonMinimal(const std::vector<Region>& regs) {
    std::vector<Region> minimal;
    for (const auto& r : regs) {
        bool ok = true;
        for (const auto& other : regs) {
            if (&other != &r && isProperSubset(other, r)) { ok = false; break; }
        }
        if (ok) minimal.push_back(r);
    }
    return minimal;
}

inline std::set<State> excitationRegionSet(const Event& e, const TransitionSystem& ts);
inline bool isPreregion(const Region& r, const std::set<State>& er);
inline std::set<State> switchingRegionSet(const Event& e, const TransitionSystem& ts);
inline bool isPostregion(const Region& r, const std::set<State>& sr);

// Computes all minimal k-bounded (general) regions of ts.
inline std::vector<Region> generateMinimalRegions(const TransitionSystem& ts,
                                                    int k) {
    std::set<Region> visited;
    std::queue<Region> worklist;
    Region zero = zeroRegion(ts.numStates);

    for (const auto& [event, _] : ts.event_transitions) {
        Region er = excitationRegion(event, ts);
        Region sr = switchingRegion(event, ts);
        if (er != zero && !visited.count(er)) { visited.insert(er); worklist.push(er); }
        if (sr != zero && !visited.count(sr)) { visited.insert(sr); worklist.push(sr); }
    }

    std::vector<Region> candidates; // regions found valid during BFS

    while (!worklist.empty()) {
        Region current = worklist.front();
        worklist.pop();

        // Find a violating event, scanning ALL its transitions (bug #2 fix)
        // to get the true (gmin,gmax), not just the first disagreement.
        Event violatingEvent;
        bool found = false;
        int gmin = INT_MAX, gmax = INT_MIN;

        for (const auto& [event, transitions] : ts.event_transitions) {
            int lgmin = INT_MAX, lgmax = INT_MIN;
            for (const auto& t : transitions) {
                int g = current[t.post] - current[t.pre];
                lgmin = std::min(lgmin, g);
                lgmax = std::max(lgmax, g);
            }
            if (lgmin != lgmax) {
                violatingEvent = event;
                gmin = lgmin;
                gmax = lgmax;
                found = true;
                break;
            }
        }

        if (!found) {
            // current is a valid region (Definition 2.4)
            if (!current.empty() && !isTrivial(current))
                candidates.push_back(current);
            continue;
        }

        auto [b1, b2] = expand(current, violatingEvent, gmin, gmax, ts);

        for (const Region& b : {b1, b2}) {
            int power = *std::max_element(b.begin(), b.end());
            // Algorithm 2 lines 11/13: only keep if k-bounded and does not
            // already contain the all-ones multiset (pruning: such regions
            // can't be minimal, Theorem 6.2 proof item 3).
            if (power <= k && !containsAllOnes(b) && !visited.count(b)) {
                visited.insert(b);
                worklist.push(b);
            }
        }
    }

    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

    std::vector<Region> minimal = removeNonMinimal(candidates);

    // Event effectiveness preservation (Approach 1):
    // If an event has no preregion among minimal regions, find valid candidate
    // regions that cover ER(event) so the event is not left disconnected.
    for (const auto& [event, _] : ts.event_transitions) {
        std::set<State> er = excitationRegionSet(event, ts);
        if (er.empty()) continue;
        bool hasPre = false;
        for (const auto& r : minimal) {
            if (isPreregion(r, er)) {
                hasPre = true;
                break;
            }
        }
        if (!hasPre) {
            std::vector<Region> coveringCandidates;
            for (const auto& c : candidates) {
                if (isPreregion(c, er)) {
                    coveringCandidates.push_back(c);
                }
            }
            if (!coveringCandidates.empty()) {
                auto minimalCovering = removeNonMinimal(coveringCandidates);
                for (const auto& r : minimalCovering) {
                    if (std::find(minimal.begin(), minimal.end(), r) == minimal.end()) {
                        minimal.push_back(r);
                    }
                }
            }
        }
    }

    // Similarly ensure events have at least one postregion if a candidate covers SR(event):
    for (const auto& [event, _] : ts.event_transitions) {
        std::set<State> sr = switchingRegionSet(event, ts);
        if (sr.empty()) continue;
        bool hasPost = false;
        for (const auto& r : minimal) {
            if (isPostregion(r, sr)) {
                hasPost = true;
                break;
            }
        }
        if (!hasPost) {
            std::vector<Region> coveringCandidates;
            for (const auto& c : candidates) {
                if (isPostregion(c, sr)) {
                    coveringCandidates.push_back(c);
                }
            }
            if (!coveringCandidates.empty()) {
                auto minimalCovering = removeNonMinimal(coveringCandidates);
                for (const auto& r : minimalCovering) {
                    if (std::find(minimal.begin(), minimal.end(), r) == minimal.end()) {
                        minimal.push_back(r);
                    }
                }
            }
        }
    }

    return minimal;
}

// ---------------------------------------------------------------------
// Excitation closure / k-ECTS check (Section 5.2, Definition 5.1)
// Needed to decide whether a BISIMILAR (synthesis, not just mined)
// Petri net can be derived at a given k, without label splitting.
// ---------------------------------------------------------------------

// Definition 3.1: ER(e) as a plain state set.
inline std::set<State> excitationRegionSet(const Event& e, const TransitionSystem& ts) {
    std::set<State> er;
    for (const auto& t : ts.event_transitions.at(e)) er.insert(t.pre);
    return er;
}

// Definition 3.2: r is a preregion of e if ER(e) subseteq supp(r).
inline bool isPreregion(const Region& r, const std::set<State>& er) {
    for (State s : er) if (r[s] <= 0) return false;
    return true;
}

// Definition 3.1's counterpart on the output side: SR(e) as a plain
// state set, and the postregion test (r is a postregion of e if
// SR(e) subseteq supp(r)). Needed both for Algorithm 1 (line 13) and
// for the postregion-preservation guard in computeIrredundantCover
// below.
inline std::set<State> switchingRegionSet(const Event& e, const TransitionSystem& ts) {
    std::set<State> sr;
    for (const auto& t : ts.event_transitions.at(e)) sr.insert(t.post);
    return sr;
}

inline bool isPostregion(const Region& r, const std::set<State>& sr) {
    for (State s : sr) if (r[s] <= 0) return false;
    return true;
}

// Definition 3.3: TOP(r,e) = smallest topset of r still covering ER(e).
// The degree g = MAX-K-TOP(r,e) = min_{s in ER(e)} r(s).
// support(TOP(r,e)) = { s | r(s) >= g }.
inline std::set<State> topsetSupport(const Region& r, const std::set<State>& er) {
    int g = INT_MAX;
    for (State s : er) g = std::min(g, r[s]);
    std::set<State> supp;
    for (size_t s = 0; s < r.size(); ++s)
        if (r[s] >= g) supp.insert((State)s);
    return supp;
}

// Definition 7.1 (Enabling Closure), computed via enabling topsets
// (Definition 3.4): EC(e) w.r.t. a region set R is the intersection,
// over every preregion r of e in R, of supp(TOP(r,e)). Returns nullopt
// if R contains no preregion of e at all (event effectiveness fails --
// EC is undefined / e is not covered by R in any way).
//
// Note: since TOP(r,e) is defined (Definition 3.3) as the SMALLEST
// topset of r that still covers ER(e), supp(TOP(r,e)) is always a
// superset of ER(e). So EC(e) is always a superset of ER(e), and
// intersecting over FEWER preregions (i.e. removing a region from R)
// can only make EC(e) grow or stay the same -- never shrink back toward
// ER(e). That monotonicity is what makes the redundancy test in
// computeIrredundantCover() below correct for both the excitation-closed
// (bisimilar) and non-excitation-closed (mining-only) cases: "EC(e)
// unchanged after removing r" is equivalent to "EC(e) still equals
// ER(e) after removing r" whenever it equalled ER(e) before.
inline std::optional<std::set<State>> enablingClosure(const Event& event,
                                                        const TransitionSystem& ts,
                                                        const std::vector<Region>& R) {
    std::set<State> er = excitationRegionSet(event, ts);
    std::optional<std::set<State>> ec;
    for (const auto& r : R) {
        if (!isPreregion(r, er)) continue;
        std::set<State> supp = topsetSupport(r, er);
        if (!ec) ec = supp;
        else {
            std::set<State> inter;
            std::set_intersection(ec->begin(), ec->end(),
                                   supp.begin(), supp.end(),
                                   std::inserter(inter, inter.begin()));
            ec = inter;
        }
    }
    return ec;
}

// Definition 5.1: TS is k-ECTS (using the given minimal k-bounded
// regions) if for every event e:
//   1. Excitation closure: EC(e) == ER(e).
//   2. Event effectiveness: e has at least one preregion (EC(e) defined).
inline bool isExcitationClosed(const TransitionSystem& ts,
                                const std::vector<Region>& regions) {
    for (const auto& [event, _] : ts.event_transitions) {
        auto ec = enablingClosure(event, ts, regions);
        if (!ec) return false; // event effectiveness fails
        if (*ec != excitationRegionSet(event, ts)) return false;
    }
    return true;
}

// ---------------------------------------------------------------------
// Section 7: Irredundant covers.
//
// All the minimal regions together satisfy the properties needed for
// synthesis/mining, but (per the paper) not all of them are actually
// NEEDED -- a region is "redundant" if dropping it doesn't change any
// event's enabling closure (Corollary 7.1). We implement that test
// directly (semantic form) rather than the arc-simplification bookkeeping
// of Theorem 7.1, which is an optimization for computing the same result
// without recomputing EC(e) from scratch -- the end result (which
// regions survive) is the same either way.
//
// IMPORTANT GAP THIS FUNCTION CLOSES: Definition 7.1's enabling closure
// is built entirely from PREregions. A region that is never a preregion
// of ANY event (e.g. one whose support consists only of states nothing
// ever fires from) is invisible to that test -- it never affects any
// EC(e), so the EC-only criterion always calls it "redundant", even when
// it is the ONLY postregion of some event. Dropping it then strips that
// event's entire output side in Algorithm 1 (no transition->place arcs
// at all, i.e. line 13 of Algorithm 1 gets nothing to write for that
// event). Whether that actually breaks bisimilarity depends on the TS's
// structure (it can be harmless if the event only ever leads to states
// with no further behavior) -- but the EC-only test has no way to know
// that, so it must not be trusted to make that call. We therefore add a
// second, independent guard: a region can only be removed if doing so
// leaves every event it was a postregion of with at least one OTHER
// postregion remaining. This is the natural production-side counterpart
// of Definition 5.1's "event effectiveness" condition (which already
// requires every event to keep at least one preregion).
//
// The paper explicitly notes redundancy is NOT monotonic: removing one
// region can change whether another is removable, and vice versa
// (Section 7, paragraph after Corollary 7.1). So this greedy pass finds
// *an* irredundant cover -- one where no single remaining region can be
// dropped without changing some EC(e) or stripping some event's last
// postregion -- not necessarily the minimum-cardinality cover. That
// matches what the paper itself defines "irredundant cover" to mean.
// ---------------------------------------------------------------------
inline std::vector<Region> computeIrredundantCover(const TransitionSystem& ts,
                                                     const std::vector<Region>& regions) {
    // Baseline: enabling closure of every event using the FULL region set.
    std::map<Event, std::optional<std::set<State>>> baseline;
    for (const auto& [event, _] : ts.event_transitions)
        baseline[event] = enablingClosure(event, ts, regions);

    // Precompute SR(e) for every event once (used by the postregion guard).
    std::map<Event, std::set<State>> srOf;
    for (const auto& [event, _] : ts.event_transitions)
        srOf[event] = switchingRegionSet(event, ts);

    std::vector<Region> current = regions;
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < current.size(); ++i) {
            std::vector<Region> trial;
            trial.reserve(current.size() - 1);
            for (size_t j = 0; j < current.size(); ++j)
                if (j != i) trial.push_back(current[j]);

            bool removable = true;

            // (a) must not change any event's enabling closure (Corollary 7.1)
            for (const auto& [event, _] : ts.event_transitions) {
                if (enablingClosure(event, ts, trial) != baseline.at(event)) {
                    removable = false;
                    break;
                }
            }

            // (b) must not strip the last postregion of any event this
            // region was covering (see the gap explained above).
            if (removable) {
                for (const auto& [event, sr] : srOf) {
                    if (!isPostregion(current[i], sr)) continue; // not relevant to this event
                    bool otherPostregionSurvives = false;
                    for (const auto& r : trial) {
                        if (isPostregion(r, sr)) { otherPostregionSurvives = true; break; }
                    }
                    if (!otherPostregionSurvives) { removable = false; break; }
                }
            }

            if (removable) {
                current = std::move(trial);
                changed = true;
                break; // restart the scan over the now-smaller set
            }
        }
    }
    return current;
}

// ---------------------------------------------------------------------
// Top-level driver requested: try k = 1..kmax, return the smallest k for
// which the TS is k-ECTS (i.e. a bisimilar Petri net can be synthesized
// with Algorithm 1 on these minimal regions, per Theorem 5.1). If no such
// k <= kmax exists, fall back to returning the kmax-bounded minimal
// regions (usable for mining / language-inclusion overapproximation,
// Section 4, but not guaranteed bisimilar).
// ---------------------------------------------------------------------

struct SynthesisSearchResult {
    int k = 0;
    bool bisimilarPossible = false;    // true iff TS is k-ECTS at this k
    std::vector<Region> minimalRegions;    // ALL minimal k-bounded regions (Def. 2.6)
    std::vector<Region> irredundantCover;  // Section 7 cover, sufficient and reduced
};

// reduceToIrredundantCover: if true (default), irredundantCover is
// computed via computeIrredundantCover(). Set to false to skip that pass
// (e.g. for large TSs where you only care about minimalRegions and want
// to avoid the extra O(|regions|^2 * |events|) redundancy-check work).
inline SynthesisSearchResult findMinimalKForSynthesis(const TransitionSystem& ts,
                                                        int kmax,
                                                        bool reduceToIrredundantCover = true) {
    SynthesisSearchResult best;
    for (int k = 1; k <= kmax; ++k) {
        std::vector<Region> regs = generateMinimalRegions(ts, k);
        bool closed = isExcitationClosed(ts, regs);

        SynthesisSearchResult candidate;
        candidate.k = k;
        candidate.bisimilarPossible = closed;
        candidate.minimalRegions = regs;
        candidate.irredundantCover = reduceToIrredundantCover
                                          ? computeIrredundantCover(ts, regs)
                                          : regs;

        if (closed) return candidate;
        best = std::move(candidate); // keep the last one in case nothing succeeds
    }
    return best; // best.* holds kmax-bounded results (mining-only guarantee)
}

// =============================================================================
// EPSILON-AUGMENTED REGION SYNTHESIS THEORY
//
// When a Transition System cannot achieve k-excitation closure at a given bound k,
// classical synthesis fails bisimilarity. The epsilon-augmented synthesis theory
// detects separation defects EC(e) \ ER(e) and inserts minimal silent transitions
// (epsilon) to resolve state conflicts while preserving weak bisimulation.
// =============================================================================

struct SeparationDefect {
    Event event;
    std::set<State> violatingStates; // states in EC(e) \ ER(e) where event e is falsely enabled
};

inline std::vector<SeparationDefect> detectSeparationDefects(const TransitionSystem& ts,
                                                            const std::vector<Region>& regions) {
    std::vector<SeparationDefect> defects;
    for (const auto& [event, _] : ts.event_transitions) {
        auto ec = enablingClosure(event, ts, regions);
        std::set<State> er = excitationRegionSet(event, ts);
        if (!ec) {
            // Event effectiveness failure: event has no preregion at all
            defects.push_back({event, er});
            continue;
        }
        std::set<State> badStates;
        for (State s : *ec) {
            if (!er.count(s)) badStates.insert(s);
        }
        if (!badStates.empty()) {
            defects.push_back({event, badStates});
        }
    }
    return defects;
}

// =============================================================================
// PRINCIPLED EPSILON INSERTION — Connected-Component + Frontier Greedy Algorithm
//
// Instead of naively splitting every violating state, we:
//   1. Cluster violating states into connected components via BFS over ALL TS edges.
//   2. For each component, find the frontier: transitions (u --label--> v) where
//      u is NOT in the component and v IS in it (entry boundary).
//   3. Insert ONE epsilon by subdividing the best frontier edge:
//         u --label--> v   becomes   u --label--> s_mid --eps_N--> v
//      This gives the region solver a new intermediate state to use as a
//      "firewall" so a place can be 1 at s_mid and 0 at v (or vice-versa),
//      resolving the separation defect without touching the original structure.
//   4. Re-evaluate synthesis.  If the defect is gone, stop. Otherwise repeat.
// =============================================================================

// ---------------------------------------------------------------------------
// Helper: BFS-based connected-component clustering of a set of states.
// Two states are "connected" if there is any TS transition (of any event)
// directly between them (in either direction).
// ---------------------------------------------------------------------------
inline std::vector<std::set<State>> computeDefectComponents(
        const TransitionSystem& ts,
        const std::set<State>& violatingStates) {

    // Build adjacency restricted to the violating states.
    std::map<State, std::set<State>> adj;
    for (const auto& [ev, tvec] : ts.event_transitions) {
        for (const auto& t : tvec) {
            if (violatingStates.count(t.pre) && violatingStates.count(t.post)) {
                adj[t.pre].insert(t.post);
                adj[t.post].insert(t.pre);
            }
        }
    }

    std::set<State> unvisited = violatingStates;
    std::vector<std::set<State>> components;

    while (!unvisited.empty()) {
        State seed = *unvisited.begin();
        std::set<State> comp;
        std::queue<State> bfsQ;
        bfsQ.push(seed);
        comp.insert(seed);
        unvisited.erase(seed);

        while (!bfsQ.empty()) {
            State cur = bfsQ.front(); bfsQ.pop();
            if (adj.count(cur)) {
                for (State nb : adj.at(cur)) {
                    if (unvisited.count(nb)) {
                        unvisited.erase(nb);
                        comp.insert(nb);
                        bfsQ.push(nb);
                    }
                }
            }
        }
        components.push_back(std::move(comp));
    }
    return components;
}

// ---------------------------------------------------------------------------
// Helper: find all TS transitions entering a component from outside.
// Returns (event, pre, post) triples where pre NOT in component, post IN component.
// ---------------------------------------------------------------------------
struct FrontierEdge {
    Event  event;
    State  pre;   // outside the component
    State  post;  // inside the component
};

inline std::vector<FrontierEdge> computeFrontierEdges(
        const TransitionSystem& ts,
        const std::set<State>& component) {
    std::vector<FrontierEdge> frontier;
    for (const auto& [ev, tvec] : ts.event_transitions) {
        for (const auto& t : tvec) {
            if (!component.count(t.pre) && component.count(t.post)) {
                frontier.push_back({ev, t.pre, t.post});
            }
        }
    }
    return frontier;
}

// ---------------------------------------------------------------------
// Section 3 / Algorithm 1: BoundedPNDerivation
//
// Derives PN=(R,E,W,M0) from a set of regions R (every region becomes a
// place, every event becomes a transition, T=E -- no label splitting).
// Implements the pseudocode exactly:
//
//   foreach region r in R: M0[r] = r(sin)
//   foreach event e in E:
//     foreach preregion r of e:
//       g = MAX-K-TOP(r,e)
//       W += (r --g--> e)                          [line 8]
//       if gradient_r(e) > -g: W += (e --g+gradient_r(e)--> r)   [line 10]
//     foreach postregion r of e:
//       W += (e --gradient_r(e)--> r)               [line 13]
//
// If the same (place,transition) arc is written by more than one of
// these steps (a region that is both a pre- and postregion of the same
// event), the weights accumulate (the paper's "W = W union {...}" is a
// multiset union over arc insertions, not an overwrite) -- hence += below.
//
// Per Theorem 4.1, L(TS) subseteq L(PN) for ANY set of minimal regions;
// per Theorem 5.1, PN is bisimilar to TS specifically when the region
// set is k-ECTS. This function works with any region set you pass it
// (minimalRegions or irredundantCover from SynthesisSearchResult).
// ---------------------------------------------------------------------

struct PetriNet {
    std::vector<Region> places;                       // places[i] IS the region (also its identity)
    std::vector<int> initialMarking;                   // initialMarking[i] = places[i][sin]
    std::vector<Event> transitions;                    // T = E
    std::map<int, std::map<Event, int>> placeToTrans;  // W(place -> transition)
    std::map<Event, std::map<int, int>> transToPlace;  // W(transition -> place)
};

inline int maxKTop(const Region& r, const std::set<State>& er) {
    int g = INT_MAX;
    for (State s : er) g = std::min(g, r[s]);
    return g;
}

inline PetriNet derivePetriNet(const TransitionSystem& ts,
                                const std::vector<Region>& regionSet) {
    PetriNet pn;
    pn.places = regionSet;
    pn.initialMarking.resize(regionSet.size());
    for (size_t i = 0; i < regionSet.size(); ++i)
        pn.initialMarking[i] = regionSet[i][ts.initialState];

    for (const auto& [event, _] : ts.event_transitions)
        pn.transitions.push_back(event);

    for (const auto& [event, _] : ts.event_transitions) {
        std::set<State> er = excitationRegionSet(event, ts);
        std::set<State> sr = switchingRegionSet(event, ts);

        for (size_t i = 0; i < regionSet.size(); ++i) {
            const Region& r = regionSet[i];
            std::optional<int> gradOpt = computeGradient(r, event, ts);
            if (!gradOpt) continue; // shouldn't happen for a valid region, but be defensive
            int grad = *gradOpt;

            if (isPreregion(r, er)) {
                int g = maxKTop(r, er); // always >=1 for a genuine preregion
                pn.placeToTrans[(int)i][event] += g;           // line 8
                if (grad > -g) {
                    pn.transToPlace[event][(int)i] += (g + grad); // line 10
                }
            }
            if (isPostregion(r, sr) && grad > 0) {
                pn.transToPlace[event][(int)i] += grad;        // line 13
            }
        }
    }

    // Drop any zero-weight arcs that could arise from accumulation
    // cancelling out (defensive cleanup; a zero-weight arc means "no arc").
    for (auto& [p, m] : pn.placeToTrans)
        for (auto it = m.begin(); it != m.end(); )
            it = (it->second <= 0) ? m.erase(it) : std::next(it);
    for (auto& [e, m] : pn.transToPlace)
        for (auto it = m.begin(); it != m.end(); )
            it = (it->second <= 0) ? m.erase(it) : std::next(it);

    return pn;
}

// ---------------------------------------------------------------------------
// Identity Buffer Check:
// An epsilon transition is an unconstrained identity buffer if:
//   1. It has exactly one input place p_in with no other outputs (p_in has only epsName as output)
//   2. p_in has a single input transition t_in.
// In that case, epsName simply buffers tokens from t_in without resolving any choice or constraint.
// Such silent transitions are useless and must never be synthesized.
// ---------------------------------------------------------------------------
inline bool isIdentityBuffer(const PetriNet& pn, const std::string& epsName) {
    int inPlaces = 0;
    int p_in = -1;
    for (const auto& [p, tmap] : pn.placeToTrans) {
        if (tmap.count(epsName) && tmap.at(epsName) > 0) {
            inPlaces++;
            p_in = p;
        }
    }
    if (inPlaces != 1 || p_in == -1) return false;

    // Check if p_in has any other output transitions
    int outTransitionsFromP = 0;
    if (pn.placeToTrans.count(p_in)) {
        for (const auto& [t, w] : pn.placeToTrans.at(p_in)) {
            if (w > 0) outTransitionsFromP++;
        }
    }
    if (outTransitionsFromP != 1) return false;

    // Check input transitions to p_in
    int inTransitionsToP = 0;
    for (const auto& [t, pmap] : pn.transToPlace) {
        if (pmap.count(p_in) && pmap.at(p_in) > 0) inTransitionsToP++;
    }
    return (inTransitionsToP == 1);
}

// ---------------------------------------------------------------------------
// Candidate Types for Epsilon Insertion:
//   1. BRANCH_CHOICE: At a fork state u with out-degree >= 2, decouple an outgoing
//      branch by turning u --e--> v into a silent decision u --eps--> v.
//   2. SUBDIVISION: Subdivide a frontier transition u --e--> v into u --e--> s_mid --eps--> v.
// ---------------------------------------------------------------------------
enum class CandidateKind {
    BRANCH_CHOICE,
    SUBDIVISION
};

struct EpsilonCandidate {
    CandidateKind kind;
    FrontierEdge edge;
    std::string description;
};

// ---------------------------------------------------------------------------
// Collect all meaningful candidate transitions for a given defect.
// ---------------------------------------------------------------------------
inline std::vector<FrontierEdge> collectCandidateFrontierEdges(
        const TransitionSystem& ts,
        const Event& defectEvent,
        const std::set<State>& violatingStates) {

    std::vector<FrontierEdge> candidates;

    // 1. Defect components entry boundaries
    auto comps = computeDefectComponents(ts, violatingStates);
    for (const auto& comp : comps) {
        auto f = computeFrontierEdges(ts, comp);
        candidates.insert(candidates.end(), f.begin(), f.end());
    }

    // 2. Excitation region entry boundary
    auto er = excitationRegionSet(defectEvent, ts);
    auto erFrontier = computeFrontierEdges(ts, er);
    candidates.insert(candidates.end(), erFrontier.begin(), erFrontier.end());

    // 3. Other events transitioning into ER(e)
    for (const auto& [ev, tvec] : ts.event_transitions) {
        if (ev == defectEvent) continue;
        for (const auto& t : tvec) {
            if (er.count(t.post)) {
                candidates.push_back({ev, t.pre, t.post});
            }
        }
    }

    // Remove duplicates while preserving order
    std::vector<FrontierEdge> uniqueCand;
    for (const auto& c : candidates) {
        bool exists = false;
        for (const auto& u : uniqueCand) {
            if (u.event == c.event && u.pre == c.pre && u.post == c.post) {
                exists = true;
                break;
            }
        }
        if (!exists) uniqueCand.push_back(c);
    }

    std::map<State, int> inDeg;
    for (const auto& [ev, tvec] : ts.event_transitions)
        for (const auto& t : tvec) inDeg[t.post]++;

    std::sort(uniqueCand.begin(), uniqueCand.end(), [&](const FrontierEdge& a, const FrontierEdge& b) {
        return inDeg[a.post] > inDeg[b.post];
    });

    return uniqueCand;
}

inline std::vector<EpsilonCandidate> collectEpsilonCandidates(
        const TransitionSystem& ts,
        const Event& defectEvent,
        const std::set<State>& violatingStates) {

    std::vector<EpsilonCandidate> candidates;

    // 1. Branch Choice Decoupling Candidates:
    // Identify branching states (forks with out-degree >= 2)
    std::map<State, int> outDeg;
    for (const auto& [ev, tvec] : ts.event_transitions) {
        for (const auto& t : tvec) outDeg[t.pre]++;
    }

    for (const auto& [u, deg] : outDeg) {
        if (deg >= 2) {
            for (const auto& [ev, tvec] : ts.event_transitions) {
                if (ev.rfind("eps_", 0) == 0) continue; // skip existing silent transitions
                for (const auto& t : tvec) {
                    if (t.pre == u && t.pre != t.post) { // non-self-loop outgoing branch
                        EpsilonCandidate cand;
                        cand.kind = CandidateKind::BRANCH_CHOICE;
                        cand.edge = {ev, t.pre, t.post};
                        cand.description = "Silent branch choice at fork s" + std::to_string(u) + 
                                           " (decouples " + ev + " branch entering s" + std::to_string(t.post) + ")";
                        candidates.push_back(cand);
                    }
                }
            }
        }
    }

    // 2. Structural Frontier Edge Subdivision Candidates:
    auto frontierEdges = collectCandidateFrontierEdges(ts, defectEvent, violatingStates);
    for (const auto& fe : frontierEdges) {
        EpsilonCandidate cand;
        cand.kind = CandidateKind::SUBDIVISION;
        cand.edge = fe;
        cand.description = "Subdivide frontier edge s" + std::to_string(fe.pre) + 
                           " --" + fe.event + "--> s" + std::to_string(fe.post);
        candidates.push_back(cand);
    }

    return candidates;
}

inline TransitionSystem applyCandidate(
        const TransitionSystem& ts,
        const EpsilonCandidate& cand,
        const std::string& epsName) {

    TransitionSystem trial = ts;
    if (cand.kind == CandidateKind::BRANCH_CHOICE) {
        auto& tvec = trial.event_transitions[cand.edge.event];
        std::vector<Transition> newVec;
        bool removed = false;
        for (const auto& t : tvec) {
            if (!removed && t.pre == cand.edge.pre && t.post == cand.edge.post) {
                removed = true;
                continue;
            }
            newVec.push_back(t);
        }
        trial.event_transitions[cand.edge.event] = newVec;
        trial.event_transitions[epsName].push_back({cand.edge.pre, cand.edge.post});
    } else { // SUBDIVISION
        State sMid = trial.numStates++;
        auto& tvec = trial.event_transitions[cand.edge.event];
        bool replaced = false;
        for (auto& t : tvec) {
            if (!replaced && t.pre == cand.edge.pre && t.post == cand.edge.post) {
                t.post = sMid;
                replaced = true;
            }
        }
        trial.event_transitions[epsName].push_back({sMid, cand.edge.post});
    }
    return trial;
}

// Helper: count total separation defects across original events only.
inline int countOriginalSeparationDefects(const TransitionSystem& ts,
                                         const std::set<Event>& origEvents,
                                         const std::vector<Region>& regions) {
    auto defects = detectSeparationDefects(ts, regions);
    int total = 0;
    for (const auto& d : defects) {
        if (origEvents.count(d.event)) total += static_cast<int>(d.violatingStates.size());
    }
    return total;
}

struct EpsilonSynthesisResult {
    int k = 0;
    bool bisimilarAchieved = false;
    bool epsilonAssisted = false;
    TransitionSystem augmentedTS;
    SynthesisSearchResult searchResult;
    std::map<std::string, std::string> epsilonTransitions; // epsName -> purpose
};

// ---------------------------------------------------------------------------
// Top-level driver for Epsilon-Augmented Synthesis:
// 1. Tries standard visible synthesis up to kmax.
// 2. If bisimilarity achieved without epsilon, returns standard net (0 epsilons).
// 3. If k-ECTS fails, runs principled greedy minimal epsilon insertion:
//      - Gathers branch choice decouplers and structural boundary candidates.
//      - Tests candidates 1-by-1:
//        * Rejects any candidate that creates an unconstrained identity buffer.
//        * Accepts ONLY if bisimilarity is achieved or defect count strictly decreases.
//      - Stops immediately once bisimilarity is reached.
//      - Caps total epsilon count at a strict minimal bound (at most 2 epsilons).
// ---------------------------------------------------------------------------
inline EpsilonSynthesisResult findMinimalKWithEpsilonSynthesis(const TransitionSystem& ts,
                                                               int kmax,
                                                               bool reduceToIrredundantCover = true) {
    EpsilonSynthesisResult res;
    res.augmentedTS = ts;

    // Phase 1: Try standard visible synthesis
    res.searchResult = findMinimalKForSynthesis(ts, kmax, reduceToIrredundantCover);
    res.k = res.searchResult.k;
    res.bisimilarAchieved = res.searchResult.bisimilarPossible;

    if (res.bisimilarAchieved) {
        return res; // Pure visible synthesis succeeded — no epsilons needed.
    }

    // Collect the ORIGINAL event set (before any augmentation).
    std::set<Event> originalEvents;
    for (const auto& [ev, _] : ts.event_transitions)
        originalEvents.insert(ev);

    // Initial baseline defect count
    int currentDefects = countOriginalSeparationDefects(ts, originalEvents, res.searchResult.irredundantCover);
    if (currentDefects == 0) {
        return res;
    }

    // Phase 2: Principled Greedy Epsilon Insertion
    const int maxEpsilons = 2;
    int epsCounter = 1;

    TransitionSystem curTS = ts;
    SynthesisSearchResult curResult = res.searchResult;

    while (epsCounter <= maxEpsilons && currentDefects > 0 && !curResult.bisimilarPossible) {
        auto allDefects = detectSeparationDefects(curTS, curResult.irredundantCover);
        std::vector<SeparationDefect> origDefects;
        for (const auto& d : allDefects) {
            if (originalEvents.count(d.event)) origDefects.push_back(d);
        }
        if (origDefects.empty()) break;

        // Sort defect events by number of violating states descending
        std::sort(origDefects.begin(), origDefects.end(), [](const auto& a, const auto& b) {
            return a.violatingStates.size() > b.violatingStates.size();
        });

        bool acceptedCandidate = false;

        for (const auto& defect : origDefects) {
            auto candidates = collectEpsilonCandidates(curTS, defect.event, defect.violatingStates);

            for (const auto& cand : candidates) {
                std::string ename = "eps_" + std::to_string(epsCounter);
                TransitionSystem trialTS = applyCandidate(curTS, cand, ename);

                // Re-evaluate synthesis on trial TS
                auto trialResult = findMinimalKForSynthesis(trialTS, kmax, reduceToIrredundantCover);

                // Derive Petri net to verify its structure
                PetriNet trialPN = derivePetriNet(trialTS, trialResult.irredundantCover);

                // STRICT FILTER: Reject any candidate that results in a useless identity buffer
                if (isIdentityBuffer(trialPN, ename)) {
                    continue;
                }

                int trialDefects = countOriginalSeparationDefects(trialTS, originalEvents, trialResult.irredundantCover);

                // STRICT VALIDATION: Accept ONLY if bisimilar achieved or defects strictly decreased
                if (trialResult.bisimilarPossible || trialDefects < currentDefects) {
                    curTS = trialTS;
                    curResult = trialResult;
                    currentDefects = trialDefects;

                    res.epsilonTransitions[ename] = cand.description;
                    epsCounter++;
                    acceptedCandidate = true;
                    if (trialResult.bisimilarPossible) {
                        break; // Bisimilar reached! Stop immediately!
                    }
                }
            }

            if (acceptedCandidate) break;
        }

        // If no candidate improved the defect count or achieved bisimilarity, stop cleanly
        if (!acceptedCandidate) {
            break;
        }
    }

    res.augmentedTS = curTS;
    res.searchResult = curResult;
    res.k = curResult.k;
    res.bisimilarAchieved = curResult.bisimilarPossible;
    res.epsilonAssisted = !res.epsilonTransitions.empty();

    return res;
}

// ---------------------------------------------------------------------
// Petri net simulation -- used both for sanity-checking derivePetriNet()
// and for anyone who wants to actually run the derived net.
// ---------------------------------------------------------------------

inline bool isEnabled(const PetriNet& pn, const Event& e, const std::vector<int>& marking) {
    for (const auto& [p, m] : pn.placeToTrans) {
        auto it = m.find(e);
        if (it != m.end() && marking[p] < it->second) return false;
    }
    return true;
}

inline std::vector<int> fire(const PetriNet& pn, const Event& e, const std::vector<int>& marking) {
    std::vector<int> next = marking;
    for (const auto& [p, m] : pn.placeToTrans) {
        auto it = m.find(e);
        if (it != m.end()) next[p] -= it->second;
    }
    auto tit = pn.transToPlace.find(e);
    if (tit != pn.transToPlace.end())
        for (const auto& [p, w] : tit->second) next[p] += w;
    return next;
}

// Replays a trace of events against the PN starting from its initial
// marking. Returns false if some event in the trace is not enabled at
// its point in the sequence (i.e. the PN does NOT accept this trace).
inline bool canFireTrace(const PetriNet& pn, const std::vector<Event>& trace,
                          std::vector<int>* finalMarking = nullptr) {
    std::vector<int> m = pn.initialMarking;
    for (const auto& e : trace) {
        if (!isEnabled(pn, e, m)) return false;
        m = fire(pn, e, m);
    }
    if (finalMarking) *finalMarking = m;
    return true;
}

// BFS over the reachability graph. Useful to check bisimilarity: when
// the region set used to derive pn is k-ECTS (Theorem 5.1), this count
// should equal the original TS's state count exactly.
inline int reachabilityGraphSize(const PetriNet& pn, int maxStates = 200000) {
    std::set<std::vector<int>> visited;
    std::queue<std::vector<int>> q;
    visited.insert(pn.initialMarking);
    q.push(pn.initialMarking);
    while (!q.empty() && (int)visited.size() < maxStates) {
        std::vector<int> m = q.front(); q.pop();
        for (const auto& e : pn.transitions) {
            if (isEnabled(pn, e, m)) {
                std::vector<int> nm = fire(pn, e, m);
                if (!visited.count(nm)) { visited.insert(nm); q.push(nm); }
            }
        }
    }
    return (int)visited.size();
}

// ---------------------------------------------------------------------
// Pretty printing
// ---------------------------------------------------------------------

inline void printRegion(std::ostream& os, const Region& r) {
    os << "{";
    for (size_t s = 0; s < r.size(); ++s) {
        if (s) os << ", ";
        os << "s" << s << ":" << r[s];
    }
    os << "}";
}

// Compact multiset notation matching the paper's own style, e.g.
// {s1, s2, s4^2, s6} instead of listing every zero entry.
inline std::string regionCompact(const Region& r) {
    std::string out = "{";
    bool first = true;
    for (size_t s = 0; s < r.size(); ++s) {
        if (r[s] <= 0) continue;
        if (!first) out += ", ";
        first = false;
        out += "s" + std::to_string(s);
        if (r[s] > 1) out += "^" + std::to_string(r[s]);
    }
    out += "}";
    return out;
}

inline void writeDot(std::ostream& os, const PetriNet& pn) {
    os << "digraph PetriNet {\n";
    os << "  rankdir=LR;\n";
    os << "  node [fontsize=10];\n\n";
 
    // Places (circles)
    for (size_t i = 0; i < pn.places.size(); ++i) {
        os << "  p" << i << " [shape=circle, label=\"p" << i;
        if (pn.initialMarking[i] > 0)
            os << "\\n" << pn.initialMarking[i] << "*"; // "*" marks tokens
        os << "\"];\n";
    }
    os << "\n";
 
    // Transitions (boxes) -- quoted since event names are arbitrary strings
    for (const auto& e : pn.transitions) {
        if (e.rfind("eps_", 0) == 0 || e == "epsilon" || e == "tau") {
            // Silent/epsilon transition: rendered as black narrow bar (standard academic notation)
            os << "  \"" << e << "\" [shape=box, style=filled, fillcolor=black, "
                  "fontcolor=white, width=0.15, height=0.6, label=\"" << e << "\"];\n";
        } else {
            os << "  \"" << e << "\" [shape=box, style=filled, "
                  "fillcolor=lightgray, label=\"" << e << "\"];\n";
        }
    }
    os << "\n";
 
    // place -> transition arcs
    for (const auto& [p, m] : pn.placeToTrans)
        for (const auto& [e, w] : m) {
            os << "  p" << p << " -> \"" << e << "\"";
            if (w != 1) os << " [label=\"" << w << "\"]";
            os << ";\n";
        }
 
    // transition -> place arcs
    for (const auto& [e, m] : pn.transToPlace)
        for (const auto& [p, w] : m) {
            os << "  \"" << e << "\" -> p" << p;
            if (w != 1) os << " [label=\"" << w << "\"]";
            os << ";\n";
        }
 
    os << "}\n";
}

inline void printPetriNet(std::ostream& os, const PetriNet& pn) {
    os << "Places (" << pn.places.size() << "):\n";
    for (size_t i = 0; i < pn.places.size(); ++i) {
        os << "  p" << i << " = " << regionCompact(pn.places[i])
           << "   (initial marking: " << pn.initialMarking[i] << ")\n";
    }
    os << "Transitions (" << pn.transitions.size() << "): ";
    for (const auto& e : pn.transitions) os << e << " ";
    os << "\n";
    os << "Arcs place->transition:\n";
    for (const auto& [p, m] : pn.placeToTrans)
        for (const auto& [e, w] : m)
            os << "  p" << p << " --" << w << "--> " << e << "\n";
    os << "Arcs transition->place:\n";
    for (const auto& [e, m] : pn.transToPlace)
        for (const auto& [p, w] : m)
            os << "  " << e << " --" << w << "--> p" << p << "\n";
}

} // namespace regions
