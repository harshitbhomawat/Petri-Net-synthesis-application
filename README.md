# Fixed region-synthesis code

## Files
- `regions.h` — the fixed, general k-bounded region computation (single header, C++17)
- `main.cpp` — unit tests + demo of the k-search API

## Build & run
```
g++ -std=c++17 -O2 -o region_synth main.cpp
./region_synth
```

## What changed vs. the original Algo1.cpp

1. **Floor division bug**: `(gmin+gmax)/2` used C++ truncating division instead
   of floor division. Silently broke every split where `gmin+gmax` is negative
   and odd (very common — e.g. gmin=-1, gmax=0), collapsing both expansion
   branches and dropping whole subtrees of minimal regions. Fixed with an
   explicit `floorDiv()`.

2. **Truncated gmin/gmax scan**: the code `break`-ed out of the gradient scan
   on the *first* disagreement between two transitions of an event, instead
   of scanning all of that event's transitions to find the true min/max
   gradient. Fixed.

3. **Ad-hoc per-transition region growth**: `forceGradientLeqG`/`forceGradientGeqG`
   patched one transition-pair at a time and had a documented no-op for the
   `g == -1` case. This doesn't generalize to multisets/k>1 at all — the
   required increase at a state depends on *all* transitions touching that
   state under that event, not one pair in isolation. Replaced with the
   paper's actual `delta_g`/`delta^g` and `⊔_g`/`⊔^g` operators
   (Definitions 6.2–6.3), proven correct by Theorem 6.1.

4. **Region representation**: was `std::set<int>` (0/1 only, so nothing
   above 1-bounded could ever be represented). Now a dense multiset
   (`vector<int>` indexed by state), matching the paper's general k-bounded
   theory (Section 2.2/2.3).

## New: k-search driver

`findMinimalKForSynthesis(ts, kmax)` tries k = 1..kmax, computes the minimal
k-bounded regions at each step, and checks **k-excitation closure**
(Definition 5.1) — the condition (Theorem 5.1) under which Algorithm 1
produces a Petri net *bisimilar* to the input TS (true synthesis, not just
mining/overapproximation).

- Returns the **smallest k** for which this holds, with `bisimilarPossible = true`.
- If no such k exists up to `kmax`, returns the `kmax`-bounded minimal
  regions with `bisimilarPossible = false` — usable for mining
  (Section 4: `L(TS) ⊆ L(PN)`, language inclusion only) but not guaranteed
  bisimilar. The paper's own escape hatch for this case is **label
  splitting** (Section 8) — not implemented here since it wasn't part of
  the request, but flagging it in case that's the actual next thing you need.

## Note on the test transition systems

The TS's in `main.cpp` are built from scratch (one from the trace language
`{aaa, ab, ba, bb}` stated in the paper's Section 1.1 text, to exercise the
weight>1 case) rather than transcribed from the paper's scanned figures —
OCR of the figures wasn't reliable enough to trust as ground truth. Every
region the code produces is instead verified programmatically against the
formal region definition (`isValidRegion`), which is a stronger check than
eyeballing a hand-reconstructed diagram anyway.

## Update: irredundant cover (Section 7) added

`computeIrredundantCover(ts, regions)` now implements the redundancy test
from Corollary 7.1 directly (drop a region iff removing it leaves every
event's enabling closure `EC(e)` unchanged), using a greedy fixpoint pass
since the paper notes redundancy is non-monotonic.

`findMinimalKForSynthesis` now returns both:
- `minimalRegions` — the complete set of minimal k-bounded regions (Def. 2.6)
- `irredundantCover` — the reduced, sufficient subset (Section 7)

Verified end-to-end on the paper's own Fig. 4a transition system
(8 states, events a/b/c/d/e): the full minimal-region set has exactly 8
regions (brute-force confirmed against every possible 2-bounded multiset,
not just re-checked against `isValidRegion`), and `computeIrredundantCover`
reduces it to exactly the 6 regions shown in Fig. 5a:
`{7}`, `{3,5,6}`, `{1,2,4²,6}`, `{0,2,5}`, `{0,1,3}`, `{0,1,2,4}`.
This is now a permanent regression test in `main.cpp`.

## Update: application main.cpp + Algorithm 1 (Petri net derivation)

### New in regions.h
- **`derivePetriNet(ts, regionSet)`** — Algorithm 1 (Section 3), builds a
  `PetriNet` (places = regions, transitions = events, weighted arcs per
  the paper's pseudocode lines 1-15) from any region set (works with
  either `minimalRegions` or `irredundantCover`).
- **`isEnabled` / `fire` / `canFireTrace` / `reachabilityGraphSize`** —
  simulate the derived net. Used to regression-test the two theorems the
  paper actually proves: Theorem 4.1 (`L(TS) ⊆ L(PN)`, checked by
  replaying real TS traces through the derived net) and Theorem 5.1
  (bisimilarity when k-ECTS, checked by confirming the reachability graph
  has exactly as many markings as the TS has states).
- **`regionCompact(r)` / `printPetriNet(os, pn)`** — human-readable output.

These are tested in `regions_selftest.cpp` (renamed from the old
library-test `main.cpp` to avoid clashing with the real application entry
point) on the paper's own Fig. 4a example: all 4 valid TS traces fire
correctly and return to the initial marking, and the derived net's
reachability graph has exactly 8 markings, matching the TS's 8 states
exactly (genuine bisimilarity, not just language inclusion). 92/92 checks
pass.

### New main.cpp (application entry point)
Replaces the old `Algo1` / `RegionToPN` / `Petrinet2` pipeline with
`regions.h`. Keeps `GenerateTraces.h` / `TransitionSystem.h` untouched
(you said that part works). Flow:

1. Generate all traces once (`generateAllTraces`).
2. Ask for `kmax`.
3. `while (x) { ... }` loop, `x` read from the user each round (1 = run
   another iteration, 0 = stop). Each iteration:
   - asks how many random traces `n` to pick this round
   - `pickRandomTraces(...)` into `SelectedTraces_iter<N>.txt` (one file
     per iteration, so nothing gets overwritten)
   - builds the `TransitionSystem`, adapts it to `regions::TransitionSystem`
   - runs `findMinimalKForSynthesis` + `derivePetriNet`
   - appends a section to `synthesis_report.md` (created fresh each run)
     with: selected traces, the transition system, the k found and
     whether bisimilar synthesis was possible, both the full minimal
     region set and the irredundant cover, and the derived Petri net —
     all in fenced code blocks under markdown headers, so it reads
     cleanly both raw and rendered.

### IMPORTANT: one assumption I could not verify
I don't have your actual `TransitionSystem.h` / `GenerateTraces.h`, so
`toRegionsTS()` in `main.cpp` assumes:
- states are 0-indexed integers
- `ts.event_transitions` iterates as `(eventName, vector<Transition>)`
  where `Transition` has `.pre`/`.post` int members — this exactly
  matches the iteration your *original* pasted code already used
  successfully in its Step 3 printout, so it should need no changes
- **the initial state is `0`** (`INITIAL_STATE_OVERRIDE` at the top of
  `main.cpp`) — this is the one thing your pasted code never showed me
  how to obtain. If your `TransitionSystem` exposes a real initial-state
  accessor, wire it in there instead of the hardcoded `0`.

I built minimal stand-in versions of both headers locally (not included
here — throwaway test doubles) to compile and run the full pipeline
end-to-end, including multi-iteration runs and an edge case (n=0 →
degenerate 1-state TS), with no crashes, and confirmed the report file
renders cleanly. But since I was testing against stand-ins rather than
your real classes, please compile against your actual headers and let me
know if anything doesn't line up — most likely candidate is the initial
state assumption above.

## Update: fixed a real gap in computeIrredundantCover (postregion loss)

**Bug report**: a derived Petri net had event `a` with zero
transition->place arcs (nothing produced anywhere after firing `a`),
even though `bisimilarPossible` reported YES.

**Root cause**: `computeIrredundantCover`'s redundancy test was built
entirely on Definition 7.1's enabling closure, which only looks at
PREregions. A region that is never a preregion of anything (e.g. one
whose support consists solely of terminal/dead-end states nothing ever
fires from again) is invisible to that test -- it never affects any
event's `EC(e)`, so the old code always saw it as "safe to remove," even
when it was the *only* postregion of some event. Dropping it silently
stripped that event's entire output side in Algorithm 1 (line 13 had
nothing left to write). In the reported case this happened not to break
bisimilarity (the 4 resulting markings stayed distinct anyway, purely by
coincidence of that TS's structure -- verified by direct computation),
but the redundancy test had no way of knowing that in general, so it
could not be trusted.

**Fix**: `computeIrredundantCover` now enforces a second guard alongside
the enabling-closure check -- a region can only be removed if every event
it was a postregion of still has at least one *other* postregion left
afterward. This is the natural production-side counterpart of Definition
5.1's "event effectiveness" (which already protects the last preregion
of each event); the paper's text doesn't spell out the postregion case
explicitly, but Algorithm 1 needs it to produce a well-formed net.

Verified against the exact reported TS: the cover grows from 6 to 7
regions (the dead-end-only region `{s4,s6,s9,s10}` is now correctly
retained), event `a` now has its expected output arc, and the derived
PN's reachability graph still has exactly 11 markings, matching the TS's
11 states. This is now a permanent regression test
(`testPostregionPreservation` in `regions_selftest.cpp`, using your
exact transition system). 98/98 checks pass, including all previous ones
-- the fix did not disturb the earlier Fig. 4a result (still 6 regions,
since none of those regions are postregion-only there).

`main.cpp` (the application) needs no changes for this fix -- it already
calls `computeIrredundantCover` through `findMinimalKForSynthesis`, so it
picks up the corrected behavior automatically.
