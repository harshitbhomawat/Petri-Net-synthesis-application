# Master's Thesis Project: Final Report

**Title:** Bounded Petri Net Synthesis from Execution Traces: Theory, Algorithmic Enhancements, and Interactive Tooling  
**Author:** Harshit Bhomawat  
**Affiliation:** Master of Technology Project (MTP)  

---

## Complete Table of Contents

- **Chapter 1: Introduction, Problem Statement & Motivation** *(Included in this draft)*
  - 1.1 Background and Context: Process Mining & Petri Net Synthesis
  - 1.2 The Classical Region Theory Foundation (Carmona et al., 2010)
  - 1.3 Key Theoretical Limitations & Practical Failure Modes
    - 1.3.1 Critical Bugs in the Canonical BFS Implementation
    - 1.3.2 The Event Effectiveness Breakdown in Non-Excitation-Closed Systems
    - 1.3.3 The Expressiveness Barrier of Visible-Only Synthesis
  - 1.4 Problem Statement and Objectives
  - 1.5 Research Questions
  - 1.6 Summary of Contributions
  - 1.7 Thesis Outline
- **Chapter 2: Mathematical Foundations of Region-Based Synthesis** *(Upcoming)*
  - 2.1 Transition Systems and Language Semantics
  - 2.2 $k$-Bounded Regions as Multisets
  - 2.3 Gradient Invariance and Support Mappings
  - 2.4 Pre-regions, Excitation Regions, and Excitation Closure
  - 2.5 Petri Net Derivation via Algorithm 1
  - 2.6 Irredundant Covers and Minimality
- **Chapter 3: Rectification of Algorithmic Flaws in Canonical Bounded Synthesis** *(Upcoming)*
  - 3.1 Truncated Gradient Disagreement Bug
  - 3.2 The Integer Floor Division Anomaly in Branch Bounding
  - 3.3 Theoretical Region Expansion Operators ($\delta_g, \delta^g, \sqcup_g, \sqcup^g$)
  - 3.4 Regression Benchmarking and Formal Ground Truth Verification
- **Chapter 4: Event Effectiveness and Composite Region Preservation** *(Upcoming)*
  - 4.1 The Event Disconnection Anomaly in Concurrent Trace Loops
  - 4.2 Proof of Failure: Unconditional Non-Minimal Region Pruning
  - 4.3 Proposed Event-Effectiveness Constrained Relaxation Algorithm
  - 4.4 Comparative Analysis with Label Splitting and Integer Linear Programming (ILP)
- **Chapter 5: Epsilon-Augmented Synthesis for Non-ECTS Systems** *(Upcoming)*
  - 5.1 Visible vs. Silent Transitions: Preserving Activity Identity
  - 5.2 Defect State Characterization: $EC(e) \setminus ER(e)$
  - 5.3 Candidate Silent Arc Generation: Decoupling Branch Choices
  - 5.4 The Identity Buffer Inadmissibility Theorem and Pruning Filter
  - 5.5 Weak Bisimulation and Language Equivalence Guarantees
- **Chapter 6: Pipeline Architecture, Loop Inference & Tool Implementation** *(Upcoming)*
  - 6.1 End-to-End Synthesis Pipeline Architecture
  - 6.2 Trace Mining and Repetition-to-Regex Loop Inference
  - 6.3 Dual-Engine C++ Architecture (`WithoutEpsilon` vs. `WithEpsilon`)
  - 6.4 Non-Interactive CLI Interface and Diagnostic Export
- **Chapter 7: Interactive Graphical Workbench (GUI)** *(Upcoming)*
  - 7.1 Architecture and Design Rationale
  - 7.2 Trace Exploration, Parameter Tuning, and Mode Switching
  - 7.3 Real-Time Side-by-Side Graphviz Visualizations
- **Chapter 8: Experimental Evaluation and Case Studies** *(Upcoming)*
  - 8.1 Benchmark Transition Systems
  - 8.2 The Canonical $(a\,b^*\,c) + (a\,c\,b^*)$ Challenge
  - 8.3 Synthesized Net Complexity, Minimality, and Verification
- **Chapter 9: Conclusion and Future Directions** *(Upcoming)*
  - 9.1 Summary of Work Done
  - 9.2 Limitations
  - 9.3 Future Perspectives
- **Appendix: Algorithmic Details, Formal Proofs & System Specifications** *(Included in this draft)*
  - Appendix A: Formal Region Derivation Operators
  - Appendix B: Complete Command-Line Interface (CLI) Specification
  - Appendix C: Software Architecture and Artifact Manifest
  - Appendix D: Reproducibility and Compilation Guide

---

# Chapter 1: Introduction, Problem Statement & Motivation

## 1.1 Background and Context: Process Mining & Petri Net Synthesis

Petri nets represent one of the most expressive, mathematically rigorous, and universally adopted formalisms for modeling concurrent, asynchronous, distributed, and resource-sharing systems. Originating from Carl Adam Petri's foundational work in 1962, Petri nets provide dual power: a simple graphical visual language consisting of places (conditions), transitions (events), and directed weighted arcs, alongside an exact algebraic foundation based on state markings, incidence matrices, and linear invariants.

In modern computer science and software engineering, **Process Mining** and **Automated Workflow Synthesis** have emerged as central paradigms. Organizations capture execution histories—sequences of events recorded during system execution—in the form of event logs or execution traces. A fundamental scientific challenge is: *How can we automatically synthesize a concise, faithful, and formally verified Petri net directly from observed event traces?*

Mathematically, the synthesis pipeline progresses through two sequential stages:
1. **Trace to Transition System (TS):** The finite set of execution traces is compiled into an operational state graph (a labeled Transition System). Identical execution prefixes are merged to form an automaton reflecting observable system dynamics.
2. **Transition System to Petri Net (PN):** The Transition System is analyzed to discover the underlying concurrency, mutual exclusion, and synchronization constraints, synthesizing a Petri net whose reachability graph is formally related (ideally bisimilar or isomorphic) to the input Transition System.

The gold standard for the second stage is **Region Theory**, pioneered by Ehrenfeucht and Rozenberg (1990) for elementary net systems and extended to general Petri nets by Cortadella, Kishinevsky, Lavagno, and Yakovlev (1998).

---

## 1.2 The Classical Region Theory Foundation (Carmona et al., 2010)

While classical region theory was restricted to 1-bounded (safe) Petri nets where places can hold at most one token, Carmona, Cortadella, and Kishinevsky (*"New Region-Based Algorithms for Deriving Bounded Petri Nets", IEEE Transactions on Computers, Vol. 59, No. 3, 2010*) established a general algorithmic framework for synthesizing **$k$-bounded Petri nets** using multiset theory.

### 1.2.1 The Core Intuition of a Region
A region $r$ is a mathematical abstraction of a Petri net place. Instead of viewing a place as an external entity, Region Theory defines a place entirely by its token distribution across the states of the Transition System.
- A **$k$-bounded region** is a mapping:
  $$r: S \to \{0, 1, \dots, k\}$$
  which assigns an integer token count to each state $s \in S$.
- For $r$ to represent a valid Petri net place, the firing of an event $e \in E$ must consume and produce a fixed number of tokens regardless of the state in which $e$ occurs. This is captured by the **Constant Gradient Condition**:
  $$\forall (u \xrightarrow{e} v) \in T: \quad \text{gradient}_r(e) = r(v) - r(u) = \text{const}(e)$$
  where $\text{const}(e) = \Delta_r(e) \in \mathbb{Z}$ represents the net token production or consumption of event $e$ on place $r$.

### 1.2.2 The Carmona et al. Synthesis Framework
The 2010 IEEE TC framework consists of two core algorithms:
- **Algorithm 2 (Minimal Region Generation):** A Breadth-First Search (BFS) over the lattice of state multisets. Whenever a candidate multiset violates the constant gradient condition for some event $e$, the algorithm expands the candidate in two directions: increasing token counts at states where the gradient is too low, or increasing token counts where the gradient is too high. Non-minimal regions are pruned to ensure compactness.
- **Algorithm 1 (Petri Net Derivation):** Constructs the synthesized Petri net from a discovered set of regions:
  - Each valid region $r$ becomes a place $p_r$.
  - The initial marking is $M_0(p_r) = r(s_0)$.
  - Place $p_r$ connects to transition $e$ with an arc of weight $W(p_r \to e) = -\Delta_r(e)$ if $r$ is a pre-region that restricts the firing of $e$.
  - Transition $e$ connects to place $p_r$ with an arc of weight $W(e \to p_r) = \Delta_r(e)$ if $r$ is a post-region.

When the input Transition System satisfies **$k$-Excitation Closure ($k$-ECTS)**, Theorem 5.1 of Carmona et al. guarantees that the derived Petri net is **bisimilar** to the input Transition System: the Petri net allows all valid system behaviors and strictly prohibits any illegal behaviors.

---

## 1.3 Key Theoretical Limitations & Practical Failure Modes

Despite the elegance of the published 2010 theory, any attempt to implement and deploy this framework on realistic execution traces encounters three severe roadblocks:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       THREE MAJOR ROADBLOCKS IN PRACTICE                     │
├─────────────────────────────────────────────────────────────────────────────┤
│ 1. Implementation Bugs in Canonical Code Base                                │
│    • Truncating division bug breaks negative gradient splitting             │
│    • Truncated scan terminates gradient evaluation prematurely              │
│    • Ad-hoc per-transition growth violates multiset correctness             │
├─────────────────────────────────────────────────────────────────────────────┤
│ 2. The Event Effectiveness Breakdown                                        │
│    • Unconditional removal of non-minimal regions deletes composite places  │
│    • Events executing across multi-phase workflows lose all input arcs       │
│    • Transitions become completely disconnected, rendering synthesis unsound│
├─────────────────────────────────────────────────────────────────────────────┤
│ 3. The Expressiveness Ceiling of Visible-Only Synthesis                     │
│    • Non-elementary branching and concurrent loops lack visible separation   │
│    • Classical remedy (Label Splitting) duplicates event labels             │
│    • Discarding silent actions causes language over-approximation           │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 1.3.1 Critical Bugs in the Canonical BFS Implementation
An initial inspection of the existing baseline implementation of Algorithm 2 revealed multiple algorithmic errors that silently caused the algorithm to discard valid regions:
1. **The Floor Division Bug:** The midpoint calculation $\lfloor (g_{\min} + g_{\max}) / 2 \rfloor$ was implemented using standard C++ integer division `/`. In C++, integer division truncates towards zero. When $g_{\min} + g_{\max}$ was negative and odd (e.g., $-1 + 0 = -1$), C++ evaluated $-1 / 2$ to $0$ rather than the mathematically required floor $-1$. As a consequence, both expansion branches collapsed onto the same boundary, silently aborting entire subtrees of valid minimal regions.
2. **Truncated Gradient Disagreement Scan:** The gradient scan `break`-ed on the *first* pair of transitions exhibiting a disagreement, instead of examining all transitions labeled by event $e$. This produced incorrect values for $g_{\min}(r, e)$ and $g_{\max}(r, e)$, leading to erroneous branch conditions.
3. **Ad-hoc Per-Transition Region Growth:** Instead of applying the formal multiset operators defined in the paper, the code attempted to adjust state weights one transition at a time, failing to handle cases where multiple transitions shared states.

### 1.3.2 The Event Effectiveness Breakdown in Non-Excitation-Closed Systems
Even after correcting the bugs above, a deeper theoretical flaw emerged during testing on realistic trace workloads containing loops and milestone events (such as the workflow $(a\,b^*\,c) + (a\,c\,b^*)$):
- Carmona's Algorithm 2 ends with an unconditional filter:
  $$\mathcal{R}_{\text{minimal}} = \text{removeNonMinimal}(\mathcal{C})$$
  which discards any candidate region $r$ that is a superset or multiset-sum of smaller valid regions ($r_1 \subset r$).
- However, when an event $b$ can execute in multiple distinct phases of a system (both before and after a milestone event $c$), the excitation region $ER(b)$ spans across both phases.
- The only valid region that covers all pre-states of $b$ is a **composite region** $r_{\text{composite}} = r_{\text{pre}} + r_{\text{post}}$.
- Because $r_{\text{pre}} \subset r_{\text{composite}}$ and $r_{\text{post}} \subset r_{\text{composite}}$, Carmona's algorithm **unconditionally discards $r_{\text{composite}}$**.
- Consequently, event $b$ has **zero pre-regions** in the final cover. When Algorithm 1 derives the Petri net, event $b$ receives **zero incoming and zero outgoing arcs**. The transition is left floating completely disconnected, violating the foundational axiom of **Event Effectiveness**!

### 1.3.3 The Expressiveness Barrier of Visible-Only Synthesis
In classical synthesis, the transition set of the Petri net is strictly restricted to the visible alphabet of the specification ($T = E$). 
When a Transition System is not excitation-closed at bound $k_{\max}$, there exist states where an event is falsely enabled:
$$s_{\text{defect}} \in EC(e) \setminus ER(e)$$
In business processes and workflow nets, requiring strict visible bijection forces a dilemma:
- **Option A (Over-approximation):** Accept an over-approximated Petri net where invalid traces are allowed.
- **Option B (Label Splitting):** Artificially split visible transitions into multiple identical copies (e.g., $b \to b_1, b_2$). This obscures the real-world identity of actions and causes state space explosion.

The literature lacked a direct, automatic technique to synthesize **$\varepsilon$ (silent / unobservable) transitions** within region-based BFS to decouple state conflicts without altering visible trace semantics.

---

## 1.4 Problem Statement and Objectives

The objective of this Master's Thesis Project is to develop a complete, mathematically verified, and practically usable framework for synthesizing bounded Petri nets from execution traces.

The core objectives are:
1. **Correct and Verify Canonical Synthesis:** Identify, mathematically analyze, and eliminate all implementation anomalies in the Carmona et al. (2010) bounded region algorithm, proving adherence to theoretical ground truth via formal regression suites.
2. **Resolve the Event Effectiveness Crisis:** Formulate an algorithmic relaxation to the unconditional `removeNonMinimal()` pruning rule that guarantees Event Effectiveness for multi-phase and looping events without resorting to label splitting or external ILP solvers.
3. **Formulate and Implement Epsilon-Augmented Synthesis:** Establish a theoretical framework and algorithmic procedure to detect state-separation defects ($EC(e) \setminus ER(e)$) and synthesize minimal, non-redundant $\varepsilon$-transitions, proving weak bisimulation on previously unsynthesizable systems.
4. **Build an Automated End-to-End Pipeline:** Integrate trace extraction, repetition-to-regex loop inference (e.g., $b \to b \to b \implies b^*$), Transition System generation, region search, and Petri net derivation into dual production-grade C++ engines.
5. **Develop an Interactive Graphical Studio:** Design and deploy a real-time, browser-accessible Graphical User Interface (GUI) allowing users to enter custom execution traces, configure synthesis bounds ($k_{\max}$, repetition threshold, $\varepsilon$-assistance), and visualize Transition Systems and synthesized Petri nets side-by-side.

---

## 1.5 Research Questions

This thesis directly investigates and resolves four central research questions:

- **RQ1 (Algorithmic Fidelity):** *Under what formal conditions does the canonical BFS region expansion fail, and how can the exact multiset operators ($\delta_g, \delta^g, \sqcup_g, \sqcup^g$) be efficiently implemented to guarantee complete minimal region discovery?*
- **RQ2 (Event Effectiveness):** *How can composite regions be preserved selectively during multiset pruning such that Event Effectiveness is guaranteed for every active event without sacrificing the irredundancy of the resulting Petri net?*
- **RQ3 (Silent Decoupling & Weak Bisimulation):** *Can silent transitions ($\varepsilon$) be automatically discovered and placed to resolve state-separation conflicts in non-$k$-ECTS systems while rigorously eliminating trivial identity buffers?*
- **RQ4 (Practical Usability & Workflow Mining):** *How can loop inference from execution traces be integrated with interactive region synthesis to provide a rapid, visual, and verified model discovery environment for practitioners?*

---

## 1.6 Summary of Contributions

The contributions of this Master's Thesis Project are summarized below:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                             KEY CONTRIBUTIONS                               │
├─────────────────────────────────────────────────────────────────────────────┤
│ 1. Complete Bug Elimination & Formal Verification                           │
│    • Discovered and rectified the integer floor division and truncated-scan │
│      bugs in Algorithm 2.                                                   │
│    • Implemented verified multiset operators with 100% test pass on ground  │
│      truth benchmarks (Carmona Fig. 4a).                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│ 2. Event-Effectiveness Constrained Relaxation (EECR)                        │
│    • Proved that classical minimal-region pruning causes Event Effectiveness│
│      failure in multi-phase and looping systems.                            │
│    • Implemented an algorithmic salvage pass that preserves minimal         │
│      covering composite regions, preventing disconnected transitions.        │
├─────────────────────────────────────────────────────────────────────────────┤
│ 3. Novel Epsilon-Augmented Synthesis Framework                              │
│    • Formulated automated defect detection for states in EC(e) \ ER(e).      │
│    • Implemented silent transition routing with formal Identity Buffer      │
│      pruning filters, achieving weak bisimulation on hard benchmarks.       │
├─────────────────────────────────────────────────────────────────────────────┤
│ 4. Trace-to-Petri-Net Pipeline with Configurable Loop Inference             │
│    • Developed an automated parser translating repeating trace patterns     │
│      (e.g., a->b->b->b->d) into regular expression loops (a->(b)*->d).      │
│    • Created non-interactive CLI interfaces for batch execution and tooling.│
├─────────────────────────────────────────────────────────────────────────────┤
│ 5. Interactive Web-Based Synthesis Workbench (GUI)                          │
│    • Built a high-performance Streamlit application in Python.              │
│    • Provides real-time parameter tuning, dual-engine execution, and side-  │
│      by-side interactive Graphviz visualization of Transition Systems and   │
│      synthesized Petri nets.                                                │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 1.7 Thesis Outline

The remainder of this report is organized as follows:
- **Chapter 2 (Mathematical Foundations of Region-Based Synthesis):** Formally defines Transition Systems, multiset regions, gradient invariance, excitation closure, and Algorithm 1 derivation.
- **Chapter 3 (Rectification of Algorithmic Flaws in Canonical Bounded Synthesis):** Analyzes the root causes of the floor-division and truncated-scan bugs, details the replacement with formal multiset operators, and presents regression test results.
- **Chapter 4 (Event Effectiveness and Composite Region Preservation):** Analyzes the disconnection anomaly in multi-phase workflows, proves why minimal pruning fails, and details the Event-Effectiveness Constrained Relaxation algorithm.
- **Chapter 5 (Epsilon-Augmented Synthesis for Non-ECTS Systems):** Details the theory and implementation of $\varepsilon$-transition discovery, defect state identification, and identity buffer filtration.
- **Chapter 6 (Pipeline Architecture, Loop Inference & Tool Implementation):** Documents the end-to-end architecture, the trace repetition inference algorithm, and the C++ engine design.
- **Chapter 7 (Interactive Graphical Workbench):** Presents the architecture, interface components, and real-time visualization features of the Python Streamlit GUI.
- **Chapter 8 (Experimental Evaluation and Case Studies):** Evaluates the entire framework on benchmark systems, including the canonical $(a\,b^*\,c) + (a\,c\,b^*)$ challenge.
- **Chapter 9 (Conclusion and Future Directions):** Summarizes the project, discusses current limitations, and outlines directions for future research.
- **Appendix:** Contains formal operator definitions, CLI reference manuals, software manifests, and compilation instructions.

---

# Appendix: Algorithmic Details, Formal Proofs & System Specifications

## Appendix A: Formal Region Derivation Operators

In accordance with Definitions 6.2 and 6.3 of Carmona et al. (2010), when a candidate multiset region $r$ exhibits a gradient violation on event $e$ such that $g_{\min}(r, e) < g_{\max}(r, e)$, the candidate must be expanded using mathematically exact multiset operators rather than ad-hoc increments.

### A.1 Gradient Bounds
For a candidate multiset $r: S \to \mathbb{N}$ and event $e \in E$:
$$g_{\min}(r, e) = \min_{(u \xrightarrow{e} v) \in T} (r(v) - r(u))$$
$$g_{\max}(r, e) = \max_{(u \xrightarrow{e} v) \in T} (r(v) - r(u))$$

### A.2 The Correct Midpoint Pivot
To guarantee that both branches strictly subdivide the violation space without collapse, the midpoint $g$ is computed via integer floor division:
$$g = \text{floorDiv}(g_{\min}(r, e) + g_{\max}(r, e), 2) = \left\lfloor \frac{g_{\min}(r, e) + g_{\max}(r, e)}{2} \right\rfloor$$
```cpp
inline int floorDiv(int a, int b) {
    int res = a / b;
    int rem = a % b;
    if (rem != 0 && ((a < 0) ^ (b < 0))) {
        res--;
    }
    return res;
}
```

### A.3 The Expansion Operators
1. **Lower-Bound Expansion ($\delta_g$ and $\sqcup_g$):**  
   Forces all transitions of event $e$ to have gradient at least $g + 1$:
   $$\delta_g(r, u \xrightarrow{e} v) = \max(0, (g + 1) - (r(v) - r(u)))$$
   The minimal region satisfying this condition is:
   $$(r \sqcup_g e)(s) = r(s) + \max_{(u \xrightarrow{e} s) \in T} \delta_g(r, u \xrightarrow{e} s)$$

2. **Upper-Bound Expansion ($\delta^g$ and $\sqcup^g$):**  
   Forces all transitions of event $e$ to have gradient at most $g$:
   $$\delta^g(r, u \xrightarrow{e} v) = \max(0, (r(v) - r(u)) - g)$$
   The minimal region satisfying this condition is:
   $$(r \sqcup^g e)(s) = r(s) + \max_{(s \xrightarrow{e} v) \in T} \delta^g(r, s \xrightarrow{e} v)$$

---

## Appendix B: Complete Command-Line Interface (CLI) Specification

Both synthesis engines (`WithoutEpsilon/run.exe` and `WithEpsilon/run.exe`) provide a unified non-interactive CLI interface designed for batch scripts, unit tests, and external GUI integration.

### B.1 CLI Flags and Arguments

| Flag | Parameter Type | Default Value | Description |
| :--- | :--- | :--- | :--- |
| `--trace-file` | `string` (path) | *Required for CLI* | Path to input trace file (one trace per line, e.g. `a->b->b->b->d`). |
| `--kmax` | `integer` ($\ge 1$) | `3` | Maximum region weight bound $k_{\max}$ to search during BFS. |
| `--repeat-threshold` | `integer` ($\ge 2$) | `3` | Number of consecutive repeating patterns to trigger loop inference ($a \to b \to b \to b \implies a \to b^*$). |
| `--epsilon` | `0` or `1` | `1` *(WithEps)* | `1` enables $\varepsilon$-assisted state separation; `0` forces visible-only synthesis. |
| `--out-pn-dot` | `string` (path) | `output/petri_net.dot` | Output destination for the synthesized Petri net Graphviz DOT file. |
| `--out-ts-dot` | `string` (path) | `output/transition_system.dot`| Output destination for the Transition System Graphviz DOT file. |
| `--out-report` | `string` (path) | `output/synthesis_report.md` | Output destination for the comprehensive Markdown synthesis report. |

### B.2 Sample Invocation Commands
```bash
# Baseline Synthesis (Without Epsilon)
./WithoutEpsilon/run.exe \
  --trace-file sample_traces.txt \
  --kmax 3 \
  --repeat-threshold 3 \
  --out-pn-dot pn.dot \
  --out-ts-dot ts.dot

# Advanced Synthesis with Epsilon Assistance Enabled
./WithEpsilon/run.exe \
  --trace-file sample_traces.txt \
  --kmax 3 \
  --repeat-threshold 3 \
  --epsilon 1 \
  --out-pn-dot pn_eps.dot \
  --out-ts-dot ts_eps.dot \
  --out-report report.md
```

---

## Appendix C: Software Architecture and Artifact Manifest

The project repository is structured into distinct, self-contained modules ensuring clear separation of concerns:

```
Petri-Net-synthesis-application/
├── README.md                             # Top-level documentation & test report
├── Final Report.md                       # Comprehensive MTP Thesis Report
├── gui/                                  # Graphical User Interface Module
│   ├── app.py                            # Streamlit interactive application
│   ├── requirements.txt                  # Python runtime dependencies
│   └── venv/                             # Dedicated Python 3.12 virtual environment
├── WithoutEpsilon/                       # Baseline Synthesis Engine
│   ├── GenerateRandomTracesToPetrinets.cpp # Application entry point & CLI parser
│   ├── GenerateTraces.h / .cpp           # Trace generator library
│   ├── TransitionSystem.h / .cpp         # Automaton model & regex compiler
│   ├── regions.h                         # Fixed Carmona 2010 region synthesis library
│   ├── compileAndRun                     # Automated compilation & execution script
│   ├── run.exe                           # Compiled native Linux x86_64 binary
│   └── EVENT_EFFECTIVENESS_DISCOVERY.md  # Formal documentation of EE preservation
└── WithEpsilon/                          # Epsilon-Augmented Synthesis Engine
    ├── GenerateRandomTracesToPetrinets.cpp # Extended entry point with epsilon router
    ├── GenerateTraces.h / .cpp           # Trace generator library
    ├── TransitionSystem.h / .cpp         # Automaton model with loop inference
    ├── regions.h                         # Epsilon-augmented region synthesis library
    ├── compileAndRun                     # Automated compilation script
    ├── run.exe                           # Compiled native Linux binary
    ├── EPSILON_SYNTHESIS_THEORY.md       # Formal specification of epsilon synthesis
    └── EpsilonTheoryExplanation.txt      # In-depth theory & canonical proofs
```

---

## Appendix D: Reproducibility and Compilation Guide

### D.1 System Requirements
- **Operating System:** Linux (Ubuntu 22.04 LTS / 24.04 LTS or compatible x86_64 distribution).
- **C++ Compiler:** GCC (`g++`) version 11 or higher, supporting ISO C++17 (`-std=c++17`).
- **Graph Visualization:** Graphviz (`dot` command installed and available in `$PATH`).
- **Python Environment:** Python 3.10+ with `streamlit` and `graphviz` packages.

### D.2 Compilation Instructions
Both C++ engines can be built independently without external dependencies:

```bash
# 1. Compile the Baseline Engine (Without Epsilon)
cd /home/harshit/HarshitMTP/Petri-Net-synthesis-application/WithoutEpsilon
g++ -std=c++17 -c GenerateTraces.cpp TransitionSystem.cpp
g++ -std=c++17 GenerateRandomTracesToPetrinets.cpp GenerateTraces.o TransitionSystem.o -o run.exe
rm -f *.o

# 2. Compile the Enhanced Engine (With Epsilon)
cd /home/harshit/HarshitMTP/Petri-Net-synthesis-application/WithEpsilon
g++ -std=c++17 -c GenerateTraces.cpp TransitionSystem.cpp
g++ -std=c++17 GenerateRandomTracesToPetrinets.cpp GenerateTraces.o TransitionSystem.o -o run.exe
rm -f *.o
```

### D.3 Launching the Interactive GUI
```bash
cd /home/harshit/HarshitMTP/Petri-Net-synthesis-application/gui
source venv/bin/activate
streamlit run app.py --server.port 8501
```
The application will be live at `http://localhost:8501`.

