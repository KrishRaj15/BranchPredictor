# Branch Predictor Analysis Framework

A branch prediction analysis framework developed as part of the **CS204: Computer Architecture** course.

**Authors:**

* Deepanshu Pundir (2024CSB1112)
* Krish Raj (2024CSB1126)

## Overview

This project analyzes the effectiveness of different branch prediction algorithms using real execution traces generated through **Intel PIN**. The framework consists of:

1. **Trace Generator** – Collects branch execution data from a running program.
2. **Predictor Analyzer** – Simulates multiple branch predictors and compares their prediction accuracy.

---

## Features

* Dynamic branch tracing using Intel PIN
* Instruction and branch count collection
* Human-readable branch trace generation
* Accuracy comparison of multiple branch predictors
* JSON output for visualization and analysis

---

## Implemented Predictors

| Predictor        | Description                             |
| ---------------- | --------------------------------------- |
| Always Taken     | Predicts every branch as taken          |
| Always Not Taken | Predicts every branch as not taken      |
| 1-Bit Predictor  | Single-bit branch history table         |
| 2-Bit Predictor  | Saturating counter predictor            |
| Loop Predictor   | Optimized for loop branches             |
| GShare Predictor | Uses global history with XOR indexing   |
| RACBP (Our own Predictor)           | Regret-Aware Confidence-Based Predictor |

---

## Project Structure

```text
.
├── MyPinTool.cpp        # Intel PIN instrumentation tool
├── Branch_Predictor.cpp    # Branch predictor simulator
├── Example.cpp        # A sample code for branch analysis. Any .cpp file can be used for testing 
└── README.md
```

---

## Trace Format

```text
BRANCH_TRACE_FILE

Total_Instructions: 123456
Total_Branches: 23456

PC          Target     Taken

0x401234    0x401278   1
0x401278    0x0        0
```

Where:

* **PC** → Branch instruction address
* **Target** → Branch target address
* **Taken** → `1` (Taken), `0` (Not Taken)

---

## Build Instructions

### Compile the PIN Tool

```bash
make
```

### Compile the Analyzer

```bash
g++ Branch_Predictor.cpp -o analyze
```

---

## Usage

### 1. Generate Branch Trace

```bash
/path-to-pin/pin -t obj-intel64/MyPinTool.so -- ./Example
```

This generates:

```text
new_trace.txt
```

### 2. Analyze the Trace

```bash
./analyze new_trace.txt
```

---

## Sample Output

```text
Predictor Accuracies

Always Taken : 65.4%
Always NT    : 34.6%
1-Bit        : 81.2%
2-Bit        : 90.5%
RACBP        : 91.82%
Loop         : 93.1%
GShare       : 95.3%
```

---

## Key Learnings

* Static predictors provide useful baselines.
* 2-bit predictors outperform 1-bit predictors by reducing oscillations.
* Global history improves prediction accuracy through correlation.
* Loop predictors are effective for repetitive branch patterns.
* RACBP adapts dynamically using confidence and regret tracking.

---
## RACBP: Regret-Aware Confidence Branch Predictor (Our Own Predictor)

### Motivation

Traditional branch predictors rely solely on confidence counters to determine branch outcomes. However, highly confident predictors often suffer large performance penalties when branch behavior suddenly changes.

To address this issue, we designed **RACBP (Regret-Aware Confidence Branch Predictor)**, a custom branch prediction architecture that augments confidence tracking with a novel **Regret** metric. The predictor dynamically penalizes unstable branches and adapts faster to changing execution patterns.

---

## Core Idea

RACBP combines two state variables:

1. **Confidence (C)** – Represents the directional bias of a branch.
2. **Regret (R)** – Represents recent prediction volatility and misprediction severity.

Instead of making predictions solely from confidence, RACBP computes a **Decision Value** that discounts confidence based on accumulated regret.

---

## State Representation

Each predictor entry stores:

### Confidence (C)

A signed 4-bit counter indicating branch bias.

```text
Range: [-8, 7]

Positive and Zero  → Taken Bias
Negative  → Not Taken Bias
```

### Regret (R)

A 3-bit unsigned counter indicating prediction instability.

```text
Range: [0, 7]
```

Higher regret values reduce the predictor's trust in its own confidence.

---

## Prediction Model

Unlike traditional saturating-counter predictors that rely solely on confidence, RACBP introduces a **Regret-Aware Decision Function** that penalizes highly volatile branches and reduces overconfidence.

### Decision Value

For every prediction, RACBP computes a **Decision Value (D)**:

```text
D = C − (sgn(C) × R)
```

where:

```text
sgn(C) =  1    if C ≥ 0
         -1    otherwise
```

* **C** = Confidence Counter
* **R** = Regret Counter

The regret term dynamically pulls confidence toward a neutral state, making the predictor less aggressive on unstable branches.

### Final Prediction

The branch outcome prediction **P** is defined as:

```text
P = Taken (1)      if D > 0
P = Taken (1)      if D = 0 and C ≥ 0
P = Not Taken (0)  otherwise
```

This tie-breaking rule ensures that when regret exactly cancels confidence, the predictor falls back to the original confidence direction.

---

## State Update Logic

The predictor adapts dynamically based on prediction correctness and confidence strength.

A prediction is considered **high confidence** when:

```text
|C| ≥ T_high
```

where:

```text
T_high = 4
```

---

### Scenario A: Correct Prediction

When a prediction is correct, regret decays and confidence is reinforced.

#### Regret Decay

```text
R_new = max(0, R − 1)
```

#### Confidence Reinforcement

```text
If branch was Taken:
    C_new = min(7, C + 1)

If branch was Not Taken:
    C_new = max(-8, C - 1)
```

---

### Scenario B: Misprediction

The update policy depends on whether the predictor was highly confident before the prediction.

#### Case B1: High-Confidence Misprediction

Condition:

```text
|C| ≥ 4
```

A highly confident incorrect prediction is heavily penalized.

##### Regret Penalty

```text
R_new = min(7, R + 2)
```

##### Confidence Shattering

```text
If C > 0:
    C_new = 0

If C ≤ 0:
    C_new = -1
```

This immediately destroys the previous strong belief and forces rapid relearning.

---

#### Case B2: Low-Confidence Misprediction

Condition:

```text
|C| < 4
```

A low-confidence mistake results in a standard update.

##### Regret Penalty

```text
R_new = min(7, R + 1)
```

##### Confidence Shift

```text
If branch was Taken:
    C_new = min(7, C + 1)

If branch was Not Taken:
    C_new = max(-8, C - 1)
```

---

## Why RACBP?

The key innovation of RACBP is the introduction of a **Regret Counter** that explicitly models prediction instability.

By combining confidence and regret in the prediction equation, RACBP:

* Reduces overconfidence on volatile branches.
* Recovers rapidly from incorrect strong predictions.
* Maintains lightweight hardware overhead.
* Adapts faster than traditional confidence-only predictors.
* Improves robustness against rapidly changing branch behavior.

### Summary

```text
Prediction:
D = C − (sgn(C) × R)

High Confidence:
|C| ≥ 4

Correct Prediction:
R = max(0, R − 1)
C = reinforce toward actual outcome

High-Confidence Misprediction:
R = min(7, R + 2)
C = shattered to 0 or -1

Low-Confidence Misprediction:
R = min(7, R + 1)
C = shifted toward actual outcome
```


---

## Course Information

**Course:** CS204 – Computer Architecture

This project explores branch prediction techniques and evaluates their performance using real execution traces obtained through dynamic binary instrumentation.

---

