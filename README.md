# MiniSTA - Minimal Static Timing Analyzer

**MiniSTA** is a C++17 Static Timing Analysis (STA) tool designed to parse Verilog gate-level netlists, validate circuit topology, perform topological sorting, and compute key timing metrics such as Arrival Time ($AT$), Required Arrival Time ($RAT$), and Slack.

---

## Features

* **Verilog Netlist Parser:** Custom lexer and parser for Verilog gate-level primitives (`and`, `or`, `nand`, `nor`, `xor`, `xnor`, `not`, `buf`, `input`, `output`).
* **Topology Validation:** Built-in checks for combinational cycles, gate arity, and driver/load connections using Kahn's algorithm (topological sort).
* **Static Timing Pass:**
  * **Forward Pass ($AT$):** Computes worst-case signal propagation delays from primary inputs to outputs.
  * **Backward Pass ($RAT$):** Propagates clock constraints backward from primary outputs to compute required arrival times.
  * **Slack Computation:** Calculates $Slack = RAT - AT$ to identify timing violations.
  * **Critical Path Extraction:** Traces and reports the critical timing path through the circuit.

---

## Circuit Architecture

The project decouples circuit topology management from timing analysis execution:
* **`Circuit`**: Holds gate/net connectivity and manages graph structures.
* **`VerilogParser`**: Converts Verilog netlists into `Circuit` representations.
* **`TimingReport`**: Independent data structure holding per-gate $AT$, $RAT$, $Slack$, and critical path vectors.
---

## Cell Delay Model

Gate delays are currently mapped via intrinsic lookup values (`GateType`):

| Gate Type | Delay (ps) | Gate Type | Delay (ps) |
| :--- | :--- | :--- | :--- |
| `NOT` / `BUF` | 30.0 | `NAND` / `NOR` | 50.0 |
| `AND` / `OR` | 80.0 | `XOR` / `XNOR` | 120.0 |

---

## Quick Start
### Prerequisites
* **Compiler:** C++17 compliant compiler (MSVC 2019+, GCC 9+, or Clang 10+)
* **Build System:** Visual Studio 2019/2022 or CMake

### Example Usage

```cpp
#include "VerilogParser.h"
#include <iostream>
int main() {
    try {
        // 1. Parse Verilog Netlist
        Circuit circuit = VerilogParser::parse_file("c17.v");

        // 2. Validate Topology (Ensures DAG structure)
        if (!circuit.validate_topology()) {
            std::cerr << "Circuit topology validation failed (cycle or arity error).\n";
            return 1;
        }

        // 3. Perform STA Pass (e.g., Target Clock Period = 500.0 ps)
        constexpr double kClockPeriod = 500.0;
        TimingReport report = circuit.analyze_timing(kClockPeriod);

        // 4. Output Results
std::cout << "Critical Path Delay: " << report.critical_path_delay << " ps\n";
        
        std::cout << "Critical Path: ";
        for (GateId id : report.critical_path) {
            std::cout << circuit.get_gate(id).name << " -> ";
        }
        std::cout << "END\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    return 0;
}

Plaintext
MiniSTA/
├── Circuit.h / .cpp        # Circuit graph representation & topology logic
├── VerilogParser.h / .cpp # Lexer & parser for Verilog netlists
├── MiniSTA.h               # Core data structures (Gate, Net, TimingReport)
└── main.cpp                # CLI entry point


MiniSTA/
├── include/                 # Header files
│   ├── MiniSTA.h
│   ├── Circuit.h
│   └── VerilogParser.h
├── src/                     # Core implementation files
│   ├── Circuit.cpp
│   ├── VerilogParser.cpp
│   └── MiniSTA_cli.cpp      # CLI Entry Point
├── tests/                   # Test suite
│   ├── test_main.cpp        # Test Runner Entry Point
│   └── netlists/            # Sample .v files for testing
│       ├── simple_and.v
│       ├── reconvergent_diamond.v
│       ├── multi_path_recon.v
│       └── fanout_tree.v
├── examples
└── README.md
