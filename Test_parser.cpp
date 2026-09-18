#include "VerilogParser.h"
#include <cassert>
#include <iostream>

// CMake defines this to an absolute path (see CMakeLists.txt) so the test
// works regardless of the directory ctest/the binary is run from. This
// fallback only matters if you compile test_parser.cpp by hand without
// going through CMake.
#ifndef EXAMPLE_NETLIST_PATH
#define EXAMPLE_NETLIST_PATH "examples/example_netlist.v"
#endif

static const char* kExampleNetlist = R"(
module example (a, b, c, out1, out2);
  input a, b, c;
  output out1, out2;
  wire n1, n2;

  and  g1 (n1, a, b);
  or   g2 (n2, b, c);
  nand g3 (out1, n1, n2);
  not  g4 (out2, n1);
endmodule
)";

static void test_parse_example() {
    VerilogParser parser;
    Circuit circuit = parser.parse(kExampleNetlist);

    // 3 primary inputs + 2 primary outputs + 4 real gates = 9 gate objects
    assert(circuit.num_gates() == 9);
    // a, b, c, n1, n2, out1, out2 = 7 nets
    assert(circuit.num_nets() == 7);

    assert(circuit.validate_topology());

    const Gate* g1 = circuit.find_gate("g1");
    assert(g1 != nullptr);
    assert(g1->type == GateType::AND);
    assert(g1->input_nets.size() == 2);

    const Gate* g4 = circuit.find_gate("g4");
    assert(g4 != nullptr);
    assert(g4->type == GateType::NOT);
    assert(g4->input_nets.size() == 1);

    const Gate* pi_a = circuit.find_gate("a");
    assert(pi_a != nullptr);
    assert(pi_a->type == GateType::INPUT);

    const Gate* po_out1 = circuit.find_gate("out1");
    assert(po_out1 != nullptr);
    assert(po_out1->type == GateType::OUTPUT);

    auto order = circuit.topological_sort();
    assert(order.size() == circuit.num_gates());

    std::cout << "test_parse_example: PASS\n";
}

static void test_arity_mismatch_throws() {
    const char* bad = R"(
module bad (a, b, out1);
  input a, b;
  output out1;
  and g1 (out1, a);
endmodule
)";
    VerilogParser parser;
    bool threw = false;
    try {
        parser.parse(bad);
    }
    catch (const ParseError& e) {
        threw = true;
        std::string msg = e.what();
        assert(msg.find("expects 3 port") != std::string::npos);
    }
    assert(threw);
    std::cout << "test_arity_mismatch_throws: PASS\n";
}

static void test_missing_semicolon_throws() {
    const char* bad = R"(
module bad (a, b, out1)
  input a, b;
  output out1;
  and g1 (out1, a, b);
endmodule
)";
    VerilogParser parser;
    bool threw = false;
    try {
        parser.parse(bad);
    }
    catch (const ParseError&) {
        threw = true;
    }
    assert(threw);
    std::cout << "test_missing_semicolon_throws: PASS\n";
}

static void test_declarations_and_gates_any_order() {
    // wire declared after it's used by a gate -- should still work since
    // get_or_create_net doesn't care about declaration order.
    const char* mixed = R"(
module mixed (a, b, out1);
  input a, b;
  and g1 (n1, a, b);
  wire n1;
  output out1;
  buf g2 (out1, n1);
endmodule
)";
    VerilogParser parser;
    Circuit circuit = parser.parse(mixed);
    assert(circuit.validate_topology());
    std::cout << "test_declarations_and_gates_any_order: PASS\n";
}

static void test_parse_file() {
    // Reads the real example netlist checked into the repo (examples/
    // example_netlist.v), rather than writing a throwaway copy -- so this
    // test also catches the file going out of sync with kExampleNetlist,
    // and doubles as a regression check on the sample file itself.
    VerilogParser parser;
    Circuit circuit = parser.parse_file(EXAMPLE_NETLIST_PATH);
    assert(circuit.validate_topology());
    assert(circuit.num_gates() == 9);
    assert(circuit.num_nets() == 7);
    std::cout << "test_parse_file: PASS\n";
}

static void test_missing_file_throws() {
    VerilogParser parser;
    bool threw = false;
    try {
        parser.parse_file("/tmp/this_file_does_not_exist.v");
    }
    catch (const ParseError&) {
        threw = true;
    }
    assert(threw);
    std::cout << "test_missing_file_throws: PASS\n";
}

int main() {
    test_parse_example();
    test_arity_mismatch_throws();
    test_missing_semicolon_throws();
    test_declarations_and_gates_any_order();
    test_parse_file();
    test_missing_file_throws();
    std::cout << "All Verilog parser tests passed.\n";
    return 0;
}