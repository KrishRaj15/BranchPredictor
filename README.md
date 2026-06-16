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
0x401278    0x401290   0
```

Where:

* **PC** → Branch instruction address
* **Target** → Branch target address
* **Taken** → `1` (Taken), `0` (Not Taken)

---

## Build Instructions

### Compile the PIN Tool

```bash
make obj-intel64/MyPinTool.so
```

### Compile the Analyzer

```bash
g++ Branch_Predictor.cpp -o analyze
```

---

## Usage

### 1. Generate Branch Trace

```bash
pin -t MyPinTool.so -- ./Example
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
2-Bit        : 87.5%
Loop         : 89.1%
GShare       : 92.3%
RACBP        : 94.0%
```

---

## Key Learnings

* Static predictors provide useful baselines.
* 2-bit predictors outperform 1-bit predictors by reducing oscillations.
* Global history improves prediction accuracy through correlation.
* Loop predictors are effective for repetitive branch patterns.
* RACBP adapts dynamically using confidence and regret tracking.

---

## Course Information

**Course:** CS204 – Computer Architecture

This project explores branch prediction techniques and evaluates their performance using real execution traces obtained through dynamic binary instrumentation.

---

