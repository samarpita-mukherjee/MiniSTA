#include "MiniSTA.h"
#include <cassert>
#include <iostream>

// Builds the example from the project plan:
//   and  g1 (n1, a, b);
//   or   g2 (n2, b, c);
//   nand g3 (out1, n1, n2);
//   not  g4 (out2, n1);
static void test_basic_topology() {
    Circuit c;

    GateId gpi_a = c.add_gate("a", GateType::INPUT);
    GateId gpi_b = c.add_gate("b", GateType::INPUT);
    GateId gpi_c = c.add_gate("c", GateType::INPUT);

    NetId na = c.add_net("a");
    NetId nb = c.add_net("b");
    NetId nc = c.add_net("c");
    c.connect_output(gpi_a, na);
    c.connect_output(gpi_b, nb);
    c.connect_output(gpi_c, nc);

    GateId g1 = c.add_gate("g1", GateType::AND);
    NetId n1 = c.add_net("n1");
    c.connect_input(g1, na);
    c.connect_input(g1, nb);
    c.connect_output(g1, n1);

    GateId g2 = c.add_gate("g2", GateType::OR);
    NetId n2 = c.add_net("n2");
    c.connect_input(g2, nb);
    c.connect_input(g2, nc);
    c.connect_output(g2, n2);

    GateId g3 = c.add_gate("g3", GateType::NAND);
    NetId nout1 = c.add_net("out1");
    c.connect_input(g3, n1);
    c.connect_input(g3, n2);
    c.connect_output(g3, nout1);

    GateId g4 = c.add_gate("g4", GateType::NOT);
    NetId nout2 = c.add_net("out2");
    c.connect_input(g4, n1);
    c.connect_output(g4, nout2);

    assert(c.validate_topology());

    auto order = c.topological_sort();
    assert(order.size() == c.num_gates());

    // Check ordering constraints: every gate must appear after its inputs' drivers.
    std::vector<std::size_t> position(order.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        position[order[i]] = i;
    }
    assert(position[g1] > position[gpi_a]);
    assert(position[g1] > position[gpi_b]);
    assert(position[g3] > position[g1]);
    assert(position[g3] > position[g2]);
    assert(position[g4] > position[g1]);

    // find_gate - find_net sanity
    assert(c.find_gate("g1") != nullptr);
    assert(c.find_gate("g1")->type == GateType::AND);
    assert(c.find_gate("does_not_exist") == nullptr);
    assert(c.find_net("n1") != nullptr);

    std::cout << "test_basic_topology: PASS\n";
}

// Builds a deliberate combinational loop: g1 -> n1 -> g2 -> n2 -> g1
static void test_cycle_detection() {
    Circuit c;

    GateId g1 = c.add_gate("g1", GateType::AND);
    GateId g2 = c.add_gate("g2", GateType::AND);
    NetId n1 = c.add_net("n1");
    NetId n2 = c.add_net("n2");

    c.connect_output(g1, n1);
    c.connect_input(g2, n1);

    c.connect_output(g2, n2);
    c.connect_input(g1, n2);

    assert(!c.validate_topology());

    auto order = c.topological_sort();
    assert(order.size() < c.num_gates()); // partial order, cycle present

    std::cout << "test_cycle_detection: PASS\n";
}

// Double-drive on the same net should throw.
static void test_multi_driver_rejected() {
    Circuit c;
    GateId g1 = c.add_gate("g1", GateType::AND);
    GateId g2 = c.add_gate("g2", GateType::OR);
    NetId n1 = c.add_net("n1");

    c.connect_output(g1, n1);
    bool threw = false;
    try {
        c.connect_output(g2, n1);
    }
    catch (const std::logic_error&) {
        threw = true;
    }
    assert(threw);
    std::cout << "test_multi_driver_rejected: PASS\n";
}

int main() {
    test_basic_topology();
    test_cycle_detection();
    test_multi_driver_rejected();
    std::cout << "All tests passed.\n";
    return 0;
}
