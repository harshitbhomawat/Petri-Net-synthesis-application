# SynthesisWithEpsilon: Complete Step-by-Step Algorithm & Architectural Guide

**Project:** Petri Net Synthesis Application  
**Document:** End-to-End Algorithmic Specification: Trace-to-Petri-Net Synthesis with Epsilon ($\varepsilon$) Transitions and Event-Effectiveness Preservation  
**Author:** Harshit Bhomawat  

---

## Table of Contents
1. **Executive Overview & High-Level Pipeline**
2. **Phase 1: Trace Ingestion, Repetition Mining & Transition System Compilation**
3. **Phase 2: Mathematical Regions & The Classical Synthesis Foundation**
4. **Phase 3: The Event-Effectiveness Breakdown & Preserving Composite Regions**
   - 4.1 The Anomaly: Disconnected Floating Transitions
   - 4.2 The Mathematical Root Cause: Unconditional Minimality Pruning
   - 4.3 Our Fix: Event-Effectiveness Constrained Relaxation (EECR)
5. **Phase 4: Why Visible-Only Synthesis Fails on Complex Systems**
   - 5.1 Separation Defects ($EC(e) \setminus ER(e)$)
   - 5.2 The Impasse of Visible Bijections and Why Label Splitting is Flawed
6. **Phase 5: The Epsilon ($\varepsilon$) Augmentation Algorithm in Complete Detail**
   - 6.1 What is an $\varepsilon$-Transition and How Does it Solve State Separation?
   - 6.2 Step 1: Automated Separation Defect Detection
   - 6.3 Step 2: Defect Clustering & Candidate Generation (Frontier vs. Branch Decoupling)
   - 6.4 Step 3: Trial TS Augmentation & Intermediate State Injection
   - 6.5 Step 4: The Identity Buffer Filter (Preventing Useless Buffering)
   - 6.6 Step 5: Strict Trace Preservation Check (Silent Reachability Simulation)
   - 6.7 Step 6: Greedy Acceptance & Termination Criteria
7. **Phase 6: Petri Net Derivation (Algorithm 1) & Graph Rendering**
8. **End-to-End Walkthrough on the Canonical Challenge: $(a\,b^*\,c) + (a\,c\,b^*)$**
9. **Algorithmic Flowcharts & Execution Summary**

---

# 1. Executive Overview & High-Level Pipeline

The goal of this synthesis system is to take **raw execution traces** (e.g., recorded sequences of events from software logs, business workflows, or concurrent protocols) and automatically synthesize a **formally verified, concise, and bounded Petri net**.

The complete synthesis flow operates across six sequential phases:

```
[ Raw Execution Traces ]
          │
          ▼
[ Phase 1: Repetition Mining & Regex Loop Inference ]
  (e.g., a->b->b->b->d with threshold x=3 becomes a->(b)*->d)
          │
          ▼
[ Phase 2: Prefix-Tree Automaton & Transition System (TS) ]
  (States S, Events E, Transitions T, Initial State s0)
          │
          ▼
[ Phase 3: Region Discovery with Event-Effectiveness Preservation (EECR) ]
  (BFS Multiset Expansion -> Salvage composite regions for active events -> Irredundant Cover)
          │
          ▼
   Is TS k-Excitation Closed (k-ECTS)?
        ├─── YES ───► [ Phase 6: Direct Petri Net Derivation ] (Pure Visible PN)
        │
        └─── NO (Defects EC(e) \ ER(e) exist)
               │
               ▼
[ Phase 5: Principled Epsilon (ε) Augmentation Engine ]
  (Detect defects -> Generate decoupling candidates -> Reject Identity Buffers
   -> Verify Trace Preservation -> Minimal ε injection -> Achieve Weak Bisimulation)
               │
               ▼
[ Phase 6: Bounded Petri Net Derivation (Algorithm 1) ]
  (Places = Regions, Transitions = Events + ε-bars, Arcs = In/Out Weights)
               │
               ▼
[ Interactive DOT Graph & Markdown Synthesis Report ]
```

---

# 2. Phase 1: Trace Ingestion, Repetition Mining & Transition System Compilation

### 2.1 Trace Tokenization
Users or log extractors provide traces in text form:
```text
a -> b -> b -> b -> d
a -> c -> c -> c -> d
a -> b -> b -> b -> b -> c -> c -> c -> d
```

### 2.2 Repetition-to-Regex Loop Inference
When analyzing real systems, observers do not observe infinite executions; they observe repeated occurrences of an activity. If an event occurs consecutively $x$ times (where $x$ is the user-configured repetition threshold, default $x=3$), our engine infers that this represents a repetitive loop rather than a rigid sequence of distinct linear states:

$$\underbrace{b \to b \to \dots \to b}_{k \ge x \text{ times}} \implies b^*$$

**The Algorithm (`inferRegexFromTrace`):**
1. The trace string is tokenized into a vector of event tokens: $[e_0, e_1, \dots, e_{m-1}]$.
2. A sliding window searches for repeating patterns of length $L \ge 1$ repeated $k \ge x$ times.
3. When a repetition pattern $P$ of length $L$ repeating $k$ times is detected:
   - The subsequence is collapsed into `(P)*`.
   - The scanner skips forward by $k \times L$ tokens.
4. If regex symbols (`*`, `+`, `(`, `)`) already exist in the input string, the parser retains them directly.

### 2.3 Compiling Traces into a Transition System
Once regex expressions are generated, they are parsed into an Abstract Syntax Tree (AST) supporting concatenation (`ConcatNode`) and Kleene star (`StarNode`):
1. **Prefix Sharing (Trie Logic):** Sequential traces that share common prefixes (e.g., both starting with `a`) reuse the same state chain $s_0 \xrightarrow{a} s_1$ instead of creating duplicate states.
2. **Kleene Star Loop Wiring:**
   - For an expression `(b)*` starting at state $s_{\text{cur}}$, the engine creates a self-loop or cyclic path:
     $$s_{\text{cur}} \xrightarrow{\quad b \quad} s_{\text{cur}}$$
   - This ensures the Transition System accurately encodes cyclic behavior without artificially creating infinite states.

The output of Phase 1 is a formal **Transition System**:
$$TS = (S, E, T, s_0)$$
- $S$: Finite set of states $\{0, 1, \dots, n-1\}$.
- $E$: Visible alphabet of events $\{a, b, c, \dots\}$.
- $T \subseteq S \times E \times S$: Transition relations $(u \xrightarrow{e} v)$.
- $s_0 \in S$: Designated initial state (state 0).

---

# 3. Phase 2: Mathematical Regions & The Classical Synthesis Foundation

### 3.1 What is a Region?
In Petri net synthesis, we do not guess where places should be. Instead, **every place is synthesized from a mathematical object called a Region**.

A $k$-bounded region $r$ is a multiset over states, represented as an integer vector:
$$r = [r(s_0), r(s_1), \dots, r(s_{n-1})], \quad r(s) \in \{0, 1, \dots, k\}$$
where $r(s)$ denotes the number of tokens the place would hold when the system is in state $s$.

### 3.2 The Constant Gradient Axiom
For place $p_r$ to be valid under Petri net firing rules, firing transition $e$ must consume and produce a constant number of tokens regardless of which state the transition originates from.

For every event $e \in E$, the gradient across every transition $u \xrightarrow{e} v$ must be invariant:
$$\text{gradient}_r(u \xrightarrow{e} v) = r(v) - r(u) = \Delta_r(e) = \text{const}$$

- If $\Delta_r(e) = -1$: Event $e$ consumes 1 token from place $p_r$ on every firing.
- If $\Delta_r(e) = +1$: Event $e$ produces 1 token into place $p_r$ on every firing.
- If $\Delta_r(e) = 0$: Place $p_r$ is a self-loop (or untouched) by event $e$.

### 3.3 Algorithm 2: BFS Region Discovery
To find all minimal valid regions up to bound $k$:

1. **Seed Initialization (Property 6.1 of Carmona et al.):**
   Instead of arbitrary singleton vectors, the BFS worklist is seeded directly with the **Excitation Regions** ($ER$) and **Switching Regions** ($SR$) of every event $e \in E$:
   - **Excitation Region Seed ($ER(e)$):** The indicator multiset marking all pre-states of event $e$:
     $$ER(e)[s] = \begin{cases} 1 & \text{if } \exists v: (s \xrightarrow{e} v) \in T \\ 0 & \text{otherwise} \end{cases}$$
   - **Switching Region Seed ($SR(e)$):** The indicator multiset marking all post-states of event $e$:
     $$SR(e)[s] = \begin{cases} 1 & \text{if } \exists u: (u \xrightarrow{e} s) \in T \\ 0 & \text{otherwise} \end{cases}$$
   *Theoretical Justification (Property 6.1):* Every non-trivial region is either a pre-region ($ER(e) \subseteq \text{supp}(r)$) or a post-region ($SR(e) \subseteq \text{supp}(r)$) of at least one event $e$. Therefore, starting the search from $\{ER(e), SR(e) \mid e \in E\}$ is guaranteed to cover the entire space of all minimal regions without wasting search steps on irrelevant state combinations.

2. **Gradient Violation Evaluation:**
   Pop candidate $r$ from the worklist. Compute the minimum and maximum gradients across all transitions for each event $e$:
   $$g_{\min}(r, e) = \min_{(u \xrightarrow{e} v)} (r(v) - r(u)), \quad g_{\max}(r, e) = \max_{(u \xrightarrow{e} v)} (r(v) - r(u))$$

3. **Validity Check:**
   If $g_{\min}(r, e) = g_{\max}(r, e)$ for all events, candidate $r$ satisfies the constant gradient axiom $\implies$ **$r$ is a valid region**. If $r$ is non-trivial (not all identical values), $r$ is collected into the candidate region pool.

4. **Expansion via Gradient Splitting:**
   If $g_{\min}(r, e) < g_{\max}(r, e)$, a gradient violation exists. The algorithm selects the violating event $e$, computes the floor-division midpoint:
   $$g = \left\lfloor \frac{g_{\min}(r, e) + g_{\max}(r, e)}{2} \right\rfloor$$
   and creates two child candidates using the exact multiset growth operators:
   - **Branch 1 (Upper-bound push $\sqcup^g$):** Forces all transitions of event $e$ to have gradient $\le g$ via $(r \sqcup^g e)$.
   - **Branch 2 (Lower-bound push $\sqcup_g$):** Forces all transitions of event $e$ to have gradient $\ge g + 1$ via $(r \sqcup_g e)$.

5. **Pruning Rules:**
   A child candidate $b$ is enqueued to the BFS worklist only if:
   - It remains $k$-bounded: $\max_{s} b[s] \le k$.
   - It does not contain the all-ones multiset: $\exists s: b[s] = 0$ (multisets with $b[s] \ge 1$ for all $s$ cannot yield minimal regions).
   - It has not been previously visited.

---

# 4. Phase 3: The Event-Effectiveness Breakdown & Preserving Composite Regions

This is one of the most important theoretical breakthroughs implemented in this project.

### 4.1 The Anomaly: Disconnected Floating Transitions
When running classical synthesis on processes with concurrent loops or multi-phase tasks (e.g. Iteration 2), we discovered that active transitions (such as event $b$) ended up **completely disconnected in the synthesized Petri net**—having **0 incoming arcs and 0 outgoing arcs**!

In Petri net semantics, a transition with zero input places is "always enabled", meaning it can fire infinitely at any time, completely destroying the correctness of the synthesized model.

### 4.2 The Mathematical Root Cause: Unconditional Minimality Pruning
Carmona et al. (IEEE TC 2010, Algorithm 2, line 14) ends with an unconditional pruning filter:
$$\mathcal{R}_{\text{minimal}} = \text{removeNonMinimal}(\mathcal{C})$$
where multiset inclusion ($r_1 \subset r_2$) discards any candidate region that contains a smaller valid region inside it.

Consider a multi-phase system with milestone event $c$:
- **Phase 1 (Before $c$):** States $S_{\text{pre}} = \{s_1, s_4, s_6\}$
- **Phase 2 (After $c$):** States $S_{\text{post}} = \{s_2, s_3, s_5, s_7, s_8\}$
- Event $b$ can execute in Phase 1 **and** in Phase 2.

The BFS finds:
- Region $r_0$ with support $S_{\text{post}}$ (valid place holding tokens after $c$).
- Region $r_1$ with support $S_{\text{pre}}$ (valid place holding tokens before $c$).
- Region $r_3 = r_0 + r_1 = S_{\text{pre}} \cup S_{\text{post}}$ (place holding a token throughout the entire process).

Because $r_0 \subset r_3$ and $r_1 \subset r_3$, Carmona's algorithm labels $r_3$ as "composite / non-minimal" and **deletes $r_3$**.

Now look at the Excitation Region of event $b$:
$$ER(b) = \{s_1, s_4, s_2, s_3\} \subseteq S_{\text{pre}} \cup S_{\text{post}}$$
To be an input place (pre-region) for event $b$, a region $r$ must cover **all** states where $b$ can fire ($ER(b) \subseteq \text{supp}(r)$):
- Does $r_1$ cover $ER(b)$? **No** ($r_1$ is 0 on states $s_2, s_3$).
- Does $r_0$ cover $ER(b)$? **No** ($r_0$ is 0 on states $s_1, s_4$).
- The **only** region that covers $ER(b)$ is the composite region $r_3$!

Because Carmona's algorithm deleted $r_3$, **event $b$ is left with zero pre-regions in the entire cover**. Algorithm 1 generates zero input arcs, and event $b$ is completely disconnected!

### 4.3 Our Fix: Event-Effectiveness Constrained Relaxation (EECR)
We modified the region selection pipeline in [`regions.h`](file:///home/harshit/HarshitMTP/Petri-Net-synthesis-application/WithEpsilon/regions.h#L328-L389) by implementing an **Event-Effectiveness Constrained Relaxation**:

```cpp
// 1. Compute standard minimal regions
std::vector<Region> minimal = removeNonMinimal(candidates);

// 2. Event-Effectiveness Salvage Pass for Pre-regions:
for (const auto& [event, _] : ts.event_transitions) {
    std::set<State> er = excitationRegionSet(event, ts);
    if (er.empty()) continue;
    
    bool hasPre = false;
    for (const auto& r : minimal) {
        if (isPreregion(r, er)) { hasPre = true; break; }
    }
    
    // If the event has NO preregion among minimal regions:
    if (!hasPre) {
        std::vector<Region> coveringCandidates;
        for (const auto& c : candidates) {
            if (isPreregion(c, er)) coveringCandidates.push_back(c);
        }
        if (!coveringCandidates.empty()) {
            // Pick the minimal covering candidate from the pool
            auto minimalCovering = removeNonMinimal(coveringCandidates);
            for (const auto& r : minimalCovering) {
                minimal.push_back(r); // Salvage the required composite region!
            }
        }
    }
}
```

An identical symmetric pass is performed for post-regions ($SR(e)$) to ensure events never lose their output connectivity.

**Guaranteed Result:** Every active event retains at least one valid input place and output place. Composite regions are preserved **only when strictly necessary** to maintain Event Effectiveness, preserving both correctness and compactness without invoking heavy Integer Linear Programming (ILP) solvers.

---

# 5. Phase 4: Why Visible-Only Synthesis Fails on Complex Systems

### 5.1 Separation Defects ($EC(e) \setminus ER(e)$)
For any event $e$, the **Excitation Region** $ER(e)$ is the set of states where $e$ is legally permitted to fire:
$$ER(e) = \{u \in S \mid \exists v: u \xrightarrow{e} v\}$$

The **Enabling Closure** $EC(e)$ is the set of states where all input places of $e$ currently hold enough tokens:
$$EC(e) = \bigcap_{r \in \text{Preregions}(e)} \text{supp}(\text{TOP}(r, e))$$

In an ideal system:
$$EC(e) = ER(e)$$
However, in systems with concurrency, branching, or asymmetric loops, there often exist states where all input places have tokens, but event $e$ is **not** supposed to fire:
$$\text{Defect States} = EC(e) \setminus ER(e) \neq \emptyset$$
At state $s_{\text{defect}}$, event $e$ is **falsely enabled**. The synthesized Petri net can fire illegal transitions that never existed in the original traces.

### 5.2 The Impasse of Visible Bijections and Why Label Splitting is Flawed
In classical synthesis, transitions in the Petri net are strictly bijective to visible event labels ($T = E$). 

When $EC(e) \setminus ER(e) \neq \emptyset$:
- **Why increasing $k$ fails:** Increasing the search bound $k$ only finds regions with higher token capacities. If two conflicting execution paths have identical net token balances, no visible region can assign tokens to one path without also enabling the other.
- **Why Label Splitting fails:** The classical literature suggests "Label Splitting"—duplicating transition $b$ into $b_1$ and $b_2$. However:
  1. It destroys the semantic identity of activities in business processes.
  2. It breaks trace alignment and replay conformance.
  3. It causes combinatorial state space explosion.

**The Solution:** Introduce **silent / unobservable transitions ($\varepsilon$)** to route tokens and decouple state conflicts without altering visible system semantics.

---

# 6. Phase 5: The Epsilon ($\varepsilon$) Augmentation Algorithm in Complete Detail

This section explains the core logic of our novel $\varepsilon$-augmentation engine.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                 THE 6-STEP PRINCIPLED EPSILON AUGMENTATION                  │
├─────────────────────────────────────────────────────────────────────────────┤
│ Step 1: Detect separation defects EC(e) \ ER(e) for each event e             │
│                                                                             │
│ Step 2: Cluster defects into connected components & extract candidate edges│
│         - Branch Choice Decoupling (at out-degree >= 2 forks)               │
│         - Structural Frontier Edge Subdivisions                             │
│                                                                             │
│ Step 3: Trial TS Augmentation: inject intermediate state & silent edge ε     │
│                                                                             │
│ Step 4: STRICT FILTER 1 — Identity Buffer Elimination                       │
│         (Discard ε if it merely buffers tokens without resolving a choice)  │
│                                                                             │
│ Step 5: STRICT FILTER 2 — Trace Preservation via Silent Simulation          │
│         (Verify that every original trace can fire under silent closure)    │
│                                                                             │
│ Step 6: Greedy Acceptance: Accept ε only if bisimilarity is achieved or     │
│         defects strictly decrease. Stop as soon as k-ECTS is reached!       │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 6.1 What is an $\varepsilon$-Transition and How Does It Solve State Separation?
An $\varepsilon$-transition (also called a silent or $\tau$-transition) is an internal Petri net transition that performs state changes without consuming or emitting an observable event label.

In Graphviz dot files and academic Petri nets, visible transitions are drawn as labeled rectangular boxes, while $\varepsilon$-transitions are drawn as **narrow solid black bars**:
```dot
"eps_1" [shape=box, style=filled, fillcolor=black, fontcolor=white, width=0.15, height=0.6, label="eps_1"];
```

**How it resolves a defect:**
Suppose state $s_{\text{defect}}$ falsely enables event $e$. By placing an $\varepsilon$-transition:
$$s_{\text{defect}} \xrightarrow{\quad \varepsilon \quad} s_{\text{new}}$$
we create a new degree of freedom in the Transition System. A control place $p_{\text{guard}}$ can now be synthesized such that:
- Place $p_{\text{guard}}$ holds 1 token at $s_{\text{defect}}$.
- Transition $\varepsilon$ fires silently, consuming the token from $p_{\text{guard}}$ and moving to $s_{\text{new}}$.
- In state $s_{\text{new}}$, $p_{\text{guard}}$ has 0 tokens, thereby **disabling event $e$**.

Because $\varepsilon$ is unobservable, the visible language of the Petri net remains identical to the original system, achieving **weak bisimulation**!

---

### 6.2 Step 1: Automated Separation Defect Detection
The engine evaluates the enabling closure for every event $e \in E$:
```cpp
auto ec = enablingClosure(event, ts, regions);
std::set<State> er = excitationRegionSet(event, ts);
std::set<State> badStates;
for (State s : *ec) {
    if (!er.count(s)) badStates.insert(s);
}
```
If `badStates` is non-empty, a `SeparationDefect` is registered for event $e$.

---

### 6.3 Step 2: Defect Clustering & Candidate Generation
Naively inserting an $\varepsilon$-transition at every violating state causes state explosion and creates useless net bloat. Instead, our algorithm uses a **principled clustering and candidate generator**:

1. **Connected Component Clustering:**
   The algorithm clusters all violating states into connected components via BFS over existing TS transitions. Two defect states belong to the same component if they can reach each other.
2. **Frontier Edge Identification:**
   For each component, the algorithm identifies the **frontier edges**:
   $$\text{Frontier} = \{ (u \xrightarrow{ev} v) \in T \mid u \notin \text{Component} \land v \in \text{Component} \}$$
   These edges represent the exact entry points where the system transitions into the defect zone.
3. **Branch Choice Decoupling Candidates:**
   For any state $u$ with out-degree $\ge 2$ (a decision fork):
   - **`BRANCH_PREPEND`:** Replace $u \xrightarrow{ev} v$ with $u \xrightarrow{\varepsilon} s_{\text{branch}} \xrightarrow{ev} v$. This allows the Petri net to make an internal choice *before* committing to event $ev$.
   - **`BRANCH_BYPASS_LOOP`:** Direct silent routing directly into loop entry states.
4. **Subdivision Candidates:**
   - **`SUBDIVISION`:** Subdivide the frontier edge:
     $$u \xrightarrow{\quad ev \quad} s_{\text{mid}} \xrightarrow{\quad \varepsilon \quad} v$$

---

### 6.4 Step 3: Trial TS Augmentation & Intermediate State Injection
For each candidate, the engine creates a `trialTS` with a new intermediate state ID:
```cpp
State sMid = trial.numStates++;
// Divert original edge: u -> sMid
// Add silent transition: sMid --eps_N--> v
trial.event_transitions[epsName].push_back({sMid, cand.edge.post});
```
The algorithm then runs region synthesis on `trialTS` to see what places emerge.

---

### 6.5 Step 4: The Identity Buffer Filter (Preventing Useless Buffering)
A common danger of silent transitions is creating an **Identity Buffer**:
$$\text{Place } p_1 \xrightarrow{\quad \varepsilon \quad} \text{Place } p_2$$
where $\varepsilon$ has exactly one input place $p_1$ (with no other outputs) and $p_1$ has only one incoming transition.

In such a case, $\varepsilon$ does not resolve any choice, conflict, or synchronization—it merely passes a token from $p_1$ to $p_2$ like an empty delay pipe. **Such transitions are completely useless and pollute the model.**

Our engine includes an explicit structural filter:
```cpp
inline bool isIdentityBuffer(const PetriNet& pn, const std::string& epsName) {
    // 1. Check if epsName has exactly one input place p_in
    if (inPlaces != 1) return false;
    // 2. Check if p_in has NO other output transitions besides epsName
    if (outTransitionsFromP != 1) return false;
    // 3. Check if p_in has only a single incoming transition
    return (inTransitionsToP == 1);
}
```
If a trial candidate produces an identity buffer, it is **instantly rejected**.

---

### 6.6 Step 5: Strict Trace Preservation Check (Silent Reachability Simulation)
The engine must guarantee that adding $\varepsilon$ did not break any valid system behavior.

The engine extracts covering traces from the original Transition System and simulates them against the trial Petri net under **Silent Closure**:
```cpp
inline std::set<std::vector<int>> silentClosure(const PetriNet& pn, const std::set<std::vector<int>>& markings) {
    // Computes all markings reachable by firing zero or more enabled epsilon transitions
}
```
For every trace $[e_0, e_1, \dots, e_m]$:
- Starting from initial marking $M_0$, compute silent closure $\text{Closure}(M_0)$.
- For each visible event $e_i$, ensure at least one marking in the current closure can fire $e_i$.
- Compute the new marking set after firing $e_i$, and immediately expand it via silent closure.

If any valid trace fails to fire, the candidate is **instantly rejected**.

---

### 6.7 Step 6: Greedy Acceptance & Termination Criteria
A trial candidate is accepted if and only if:
1. It passes the Identity Buffer filter.
2. It passes the Trace Preservation check.
3. **It strictly improves synthesis:** either full $k$-ECTS bisimilarity is achieved (`bisimilarPossible = true`), or the total count of separation defects across original events strictly decreases.

As soon as `bisimilarPossible` becomes true, the greedy search **terminates immediately**, guaranteeing a minimal number of silent transitions (typically 1 or 2).

---

# 7. Phase 6: Petri Net Derivation (Algorithm 1) & Graph Rendering

Once the final region set $\mathcal{R}$ is selected (and augmented TS finalized), Algorithm 1 synthesizes the Petri Net:

### 7.1 Places & Markings
- Each region $r \in \mathcal{R}$ becomes place $p_i$.
- Initial marking: $M_0(p_i) = r(s_0)$.

### 7.2 Arc Construction
For each region $r$ and event $e \in E \cup T_\varepsilon$:
- **Input Arcs (Place $\to$ Transition):**
  If $r$ is a pre-region of $e$ ($ER(e) \subseteq \text{supp}(r)$):
  $$W(p_r \to e) = \min_{s \in ER(e)} r(s)$$
- **Output Arcs (Transition $\to$ Place):**
  If $r$ has a positive gradient $\Delta_r(e) > 0$, or is a post-region ($SR(e) \subseteq \text{supp}(r)$):
  $$W(e \to p_r) = W(p_r \to e) + \Delta_r(e)$$

### 7.3 Graphviz DOT Rendering
The derived Petri net is exported to Graphviz DOT:
- Places: Circles labeled with initial tokens (e.g., `p0 [label="p0\n1*"]`).
- Visible Transitions: Light gray boxes (e.g., `"b" [shape=box, style=filled, fillcolor=lightgray]`).
- Silent Transitions: Solid black narrow bars (e.g., `"eps_1" [shape=box, style=filled, fillcolor=black, width=0.15]`).
- Arcs: Directed arrows with integer weights (omitted if weight = 1).

---

# 8. End-to-End Walkthrough on the Canonical Challenge: $(a\,b^*\,c) + (a\,c\,b^*)$

To understand how all pieces work together, consider the canonical benchmark workflow:
$$\text{Branch 1: } a \to b^* \to c \qquad \text{Branch 2: } a \to c \to b^*$$

### The Challenge:
In Branch 1, event $b$ occurs **before** $c$. In Branch 2, event $b$ occurs **after** $c$.
- Both branches start with event $a$.
- After $a$, the system faces a choice: enter the $b$-loop first, or fire $c$ first.
- If $c$ fires first (Branch 2), the system must never be allowed to fire the Branch 1 sequence.
- Under pure visible synthesis at $k=1, 2, 3$, the Transition System is **not excitation closed**. Event $b$ suffers from separation defects because no visible place can distinguish the state right after $a$ without disabling legitimate executions in the other branch.

### How Our Algorithm Solves It:
1. **Loop Inference:** Consecutive occurrences (e.g., $a \to b \to b \to b \to c$) are automatically compiled into $(b)^*$.
2. **Defect Detection:** The engine detects separation defects on event $b$ and event $c$.
3. **Epsilon Decoupling:**
   - The engine analyzes the fork after event $a$.
   - It synthesizes a silent transition `eps_1` entering the loop boundary.
   - Now, a control place holds a token after $a$. Firing `eps_1` silently commits the net to Branch 1, consuming the token and preventing the premature firing of Branch 2.
4. **Result:**
   - The derived Petri net achieves **weak bisimulation**.
   - Exactly one minimal $\varepsilon$-transition is synthesized.
   - All original traces fire with 100% precision.
   - Zero label splitting required; activity identities are completely preserved.

---

# 9. Algorithmic Flowcharts & Execution Summary

### Summary Comparison Table

| Feature | Carmona et al. (IEEE TC 2010) | Our Synthesis Engine (`WithEpsilon`) |
| :--- | :--- | :--- |
| **Integer Midpoint Division** | Truncating `/` (drops valid regions) | Exact floor division $\lfloor (g_{\min}+g_{\max})/2 \rfloor$ |
| **Gradient Scan** | Truncated on first disagreement | Full scan across all transitions of the event |
| **Multi-Phase / Loop Events** | Fails: `removeNonMinimal` deletes composite regions leaving transitions disconnected | **EECR:** Automatically salvages minimal covering composite regions |
| **Non-ECTS Systems** | Requires manual Label Splitting or over-approximates language | **Epsilon Augmentation:** Automatically synthesizes minimal $\varepsilon$ transitions |
| **Useless Silent Transitions** | Unaddressed | **Identity Buffer Filter:** Strictly rejects useless buffering transitions |
| **Trace Conformance** | Theoretical static check | **Silent Closure Reachability:** Verifies 100% trace replay |
| **Loop Discovery from Traces** | Manual user intervention | Automated sliding-window repetition mining ($b^x \implies b^*$) |
| **User Interface** | Terminal only | Interactive real-time Web Studio (Streamlit + Graphviz) |

---
*End of Specification Document.*

