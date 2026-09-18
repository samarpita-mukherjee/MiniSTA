// MiniSTA.cpp : Defines the entry point for the application.

#include "MiniSTA.h"

#include <queue>
#include <stdexcept>

//TODO comment flagging that undriven 
//nets and input - count validation(e.g.AND should have exactly 2 inputs) aren't checked yet.
// 
// 
// ---------------------------------------------------------------------------
// Factory methods
// ---------------------------------------------------------------------------

GateId Circuit::add_gate(std::string_view name, GateType type) {
    const GateId id = gates_.size();
    gates_.push_back(std::make_unique<Gate>(id, name, type));
    gate_lookup_.emplace(std::string(name), id);
    return id;
}

// Minimal program entry so the project links as an executable. Replace with
// real CLI parsing / tests as you implement the rest of the application.
//int main() {
//    return 0;
//}

NetId Circuit::add_net(std::string_view name) {
    const NetId id = nets_.size();
    auto net = std::make_unique<Net>();
    net->id = id;
    net->name = std::string(name);
    nets_.push_back(std::move(net));
    net_lookup_.emplace(std::string(name), id);
    return id;
}

// ---------------------------------------------------------------------------
// Topology wiring
// ---------------------------------------------------------------------------

void Circuit::connect_input(GateId gate_id, NetId net_id) {
    Gate& gate = get_gate(gate_id);
    Net& net = get_net(net_id);

    gate.input_nets.push_back(net_id);
    net.load_gates.push_back(gate_id);

    // NOTE: this allows the same net to be listed as an input to a gate more
    // than once (e.g. an AND gate tied to itself, a==a). That's legal in
    // real circuits (it just means both inputs switch together), so we don't
    // reject it here. If you add N-input gates later, this still holds.
}

void Circuit::connect_output(GateId gate_id, NetId net_id) {
    Gate& gate = get_gate(gate_id);
    Net& net = get_net(net_id);

    // Every gate we currently support (see GateType) has exactly one output,
    // so calling this twice on the same gate is almost certainly a bug in
    // the caller (e.g. the parser) rather than something we should silently
    // overwrite.
    if (gate.output_net != kInvalidNetId) {
        throw std::logic_error(
            "Gate '" + gate.name + "' already has an output net assigned");
    }

    // A net driven by two different gates is a real design error (two
    // outputs shorted together), so we catch it here rather than letting it
    // silently corrupt the timing graph later.
    if (net.driver_gate != kInvalidGateId) {
        throw std::logic_error(
            "Net '" + net.name + "' is driven by more than one gate");
    }

    gate.output_net = net_id;
    net.driver_gate = gate_id;
}

// ---------------------------------------------------------------------------
// Lookups
// ---------------------------------------------------------------------------

const Gate* Circuit::find_gate(std::string_view name) const {
    auto it = gate_lookup_.find(std::string(name));
    if (it == gate_lookup_.end()) {
        return nullptr;
    }
    return gates_.at(it->second).get();
}

const Net* Circuit::find_net(std::string_view name) const {
    auto it = net_lookup_.find(std::string(name));
    if (it == net_lookup_.end()) {
        return nullptr;
    }
    return nets_.at(it->second).get();
}

// ---------------------------------------------------------------------------
// Misc
// ---------------------------------------------------------------------------

void Circuit::clear() noexcept {
    gates_.clear();
    nets_.clear();
    gate_lookup_.clear();
    net_lookup_.clear();
}

// ---------------------------------------------------------------------------
// Topological sort (Kahn's algorithm)
// ---------------------------------------------------------------------------
//
// Returns gates in an order where every gate appears after all gates that
// feed it. This is required before Day 3's arrival-time computation, which
// walks gates in this order and assumes every input's arrival time is
// already known by the time it processes a gate.
//
// If the circuit contains a combinational loop (a cycle), a full ordering
// is impossible. In that case this returns a vector smaller than
// num_gates() -- callers (see validate_topology below) should check the
// size rather than assume every gate was placed.
std::vector<GateId> Circuit::topological_sort() const {
    const std::size_t n = gates_.size();

    std::vector<int> in_degree(n, 0);
    std::vector<std::vector<GateId>> successors(n);

    // Build the graph purely from net driver/load relationships: an edge
    // exists from a net's driver gate to every gate that loads that net.
    for (const auto& net_ptr : nets_) {
        const Net& net = *net_ptr;
        if (net.driver_gate == kInvalidGateId) {
            // Net has no driver (e.g. parser hasn't wired it yet, or it's
            // a malformed netlist). Nothing to propagate from here.
            // TODO(Day 4/parser): decide whether an undriven net used as a
            // load should be a hard parse error instead of being silently
            // skipped here.
            continue;
        }
        for (GateId load : net.load_gates) {
            successors[net.driver_gate].push_back(load);
            in_degree[load]++;
        }
    }

    // Seed the queue with every gate that has no predecessors (primary
    // inputs, plus any gate whose inputs are all undriven). Iterating in id
    // order keeps the result deterministic, which makes tests reproducible.
    std::queue<GateId> ready;
    for (GateId id = 0; id < n; ++id) {
        if (in_degree[id] == 0) {
            ready.push(id);
        }
    }

    std::vector<GateId> order;
    order.reserve(n);

    while (!ready.empty()) {
        const GateId current = ready.front();
        ready.pop();
        order.push_back(current);

        for (GateId next : successors[current]) {
            if (--in_degree[next] == 0) {
                ready.push(next);
            }
        }
    }

    // If order.size() < n, some gates never reached in-degree 0, which
    // means they're part of a cycle (or depend on one). We deliberately
    // return the partial order rather than throwing here -- validate_topology
    // is the place that turns this into a pass/fail result, so callers who
    // just want "best effort order" can still get one.
    return order;
}

bool Circuit::validate_topology() const {
    // A combinational circuit must be a DAG. If topological_sort() couldn't
    // place every gate, there's a cycle somewhere.
    //
    // TODO(Day 4): this only checks for cycles. Consider also checking:
    //   - every net has exactly one driver (partially enforced already in
    //     connect_output, but a net could still have zero drivers)
    //   - every gate's declared input count matches what its GateType
    //     expects (e.g. AND should have exactly 2 input_nets)
    return topological_sort().size() == gates_.size();
}
