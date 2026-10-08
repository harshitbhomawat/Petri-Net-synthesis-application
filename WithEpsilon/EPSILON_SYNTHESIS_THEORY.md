# Epsilon-Augmented Petri Net Synthesis Theory

**Folder:** `WithEpsilon/`  
**Base Architecture:** Carmona, Cortadella, Kishinevsky (IEEE Trans. Computers 2010)  
**Enhancement:** Epsilon-Transition Guided State Separation and Weak Bisimulation Synthesis

---

## 1. Motivation and Problem Formulation

In classical Region Synthesis, the transition set of the synthesized Petri net is strictly bijective to the visible event alphabet of the input Transition System ($T = E$). 

When a Transition System is **not $k$-Excitation Closed ($k$-ECTS)** at a given search bound $k$, there exist one or more events $e \in E$ with **Separation Defects**:
$$s_{\text{defect}} \in EC(e) \setminus ER(e)$$
where $EC(e) = \bigcap_{r \in \text{Preregions}(e)} \text{supp}(TOP(r, e))$.

At state $s_{\text{defect}}$, event $e$ is **falsely enabled** in the synthesized Petri net because no $k$-bounded region can assign 0 tokens to $s_{\text{defect}}$ while maintaining a constant non-negative firing gradient for all visible transitions.

### Classical Alternatives:
1. **Incrementing $k$:** Fails if the underlying defect is structural (e.g. non-free-choice synchronization or non-elementary loops) rather than a token counting deficit.
2. **Label Splitting:** Duplicates event $e$ into $e_1, e_2$. This destroys the identity of activities in business process models and causes state explosion.

### The $\varepsilon$-Transition Solution:
We formulate **Epsilon-Augmented Synthesis**: when visible $k$-bounded synthesis cannot achieve excitation closure, the engine detects the precise state defects $EC(e) \setminus ER(e)$ and introduces minimal silent transitions ($\varepsilon \in T_\varepsilon$) to decouple the conflicting states, achieving **weak bisimulation** without altering the visible trace language.

---

## 2. Theoretical Architecture

### 2.1 Separation Defect Detection
For every event $e \in E$, the engine computes the set of defect states:
$$\text{Defects}(e) = EC(e) \setminus ER(e)$$
- If $EC(e)$ is undefined (Event Effectiveness failure), the entire excitation region $ER(e)$ is flagged for silent gating.
- If $\text{Defects}(e) = \emptyset$ for all events, the system is strictly $k$-ECTS and pure visible synthesis is returned.

### 2.2 Transition System Augmentation ($\text{TS} \to \text{TS}_\varepsilon$)
For each violating state $s_{\text{defect}}$, a silent transition $\varepsilon_i$ is introduced:
$$s_{\text{defect}} \xrightarrow{\quad \varepsilon_i \quad} s_{\text{new}}$$
This introduces a new silent degree of freedom:
$$\text{gradient}_r(\varepsilon_i) = r(s_{\text{new}}) - r(s_{\text{defect}})$$
This allows a control place to hold 1 token at $s_{\text{defect}}$, which is consumed silently by $\varepsilon_i$, leaving 0 tokens at $s_{\text{new}}$ and thereby disabling event $e$ at $s_{\text{new}}$.

### 2.3 Weak Bisimulation Guarantee
Because $\varepsilon_i$ is unobservable, the visible trace projection satisfies:
$$\text{Traces}(PN_\varepsilon) \downarrow E \;=\; \text{Traces}(TS)$$
The reachability graph of $PN_\varepsilon$ is weakly bisimilar to $TS$.

---

## 3. Implementation in `WithEpsilon/`

1. **[`WithEpsilon/regions.h`](file:///home/harshit/HarshitMTP/Petri-Net-synthesis-application/WithEpsilon/regions.h):**
   - `detectSeparationDefects(ts, regions)`: Identifies $s_{\text{defect}} \in EC(e) \setminus ER(e)$.
   - `augmentWithEpsilon(origTS, defects, epsilonInfo)`: Automatically synthesizes the silent edges.
   - `findMinimalKWithEpsilonSynthesis(ts, kmax)`: Two-phase synthesis driver (tries pure visible first; if non-bisimilar at $k_{\max}$, activates epsilon assistance).
   - `writeDot(os, pn)`: Renders $\varepsilon$-transitions as narrow solid black rectangular bars (standard formal Petri net notation for silent actions).

2. **[`WithEpsilon/GenerateRandomTracesToPetrinets.cpp`](file:///home/harshit/HarshitMTP/Petri-Net-synthesis-application/WithEpsilon/GenerateRandomTracesToPetrinets.cpp):**
   - Integrates the $\varepsilon$-synthesis driver into the interactive trace execution pipeline.
   - Outputs detailed diagnostic logs showing each synthesized $\varepsilon$-transition and its exact separation purpose.
   - Saves DOT files to `WithEpsilon/output/iteration_i.dot`.

3. **[`WithEpsilon/compileAndRun`](file:///home/harshit/HarshitMTP/Petri-Net-synthesis-application/WithEpsilon/compileAndRun):**
   - Independent build and execution script operating exclusively inside `WithEpsilon/`.

---

## 4. Execution and Verification

To run synthesis with $\varepsilon$-assistance:
```bash
cd WithEpsilon
./compileAndRun GenerateRandomTracesToPetrinets
```

In Iteration 2:
- Defect states were detected where event $b$ was enabled outside its true excitation set due to concurrent loop interleavings.
- The engine synthesized 4 silent transitions (`eps_1`, `eps_2`, `eps_3`, `eps_4`) to isolate the post-milestone states.
- The resulting Petri net in `WithEpsilon/output/iteration_2.dot` cleanly represents both the visible process logic (`a`, `b`, `c`) and the silent routing guards.
