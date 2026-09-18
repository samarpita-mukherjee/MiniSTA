// MiniSTA.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <unordered_map>
#include <cstdint>
#include <span>
#include <limits>
#include <memory>
#include <array>

using GateId = std::size_t;
using NetId = std::size_t;

inline constexpr GateId kInvalidGateId = std::numeric_limits<GateId>::max();
inline constexpr NetId kInvalidNetId = std::numeric_limits<NetId>::max();

enum class GateType : uint8_t {
    AND,
    OR,
    NAND,
    NOR,
    XOR,
    XNOR,
    NOT,
    BUF,
    INPUT,
    OUTPUT,
    NUM_TYPES // Helper for array sizing
};

// Fixed lookup table mapping GateType -> intrinsic delay
constexpr std::array<double, static_cast<std::size_t>(GateType::NUM_TYPES)> GATE_DELAY_TABLE = [] {
    std::array<double, static_cast<std::size_t>(GateType::NUM_TYPES)> table{};
    table[static_cast<std::size_t>(GateType::AND)] = 80.0;
    table[static_cast<std::size_t>(GateType::OR)] = 80.0;
    table[static_cast<std::size_t>(GateType::NAND)] = 50.0;
    table[static_cast<std::size_t>(GateType::NOR)] = 50.0;
    table[static_cast<std::size_t>(GateType::XOR)] = 120.0;
    table[static_cast<std::size_t>(GateType::XNOR)] = 120.0;
    table[static_cast<std::size_t>(GateType::NOT)] = 30.0;
    table[static_cast<std::size_t>(GateType::BUF)] = 30.0;
    table[static_cast<std::size_t>(GateType::INPUT)] = 0.0;
    table[static_cast<std::size_t>(GateType::OUTPUT)] = 0.0;
    return table;
    }();

// Helper function to query gate delays
[[nodiscard]] constexpr double get_gate_delay(GateType type) noexcept {
    return GATE_DELAY_TABLE[static_cast<std::size_t>(type)];
}

struct Net {
    NetId id { 0 };
    std::string name;

    GateId driver_gate{ kInvalidGateId };
    std::vector<GateId> load_gates;
};

struct Gate {
    GateId id { 0 };
    std::string name;
    GateType type;

    std::vector<NetId> input_nets;
    NetId output_net{ kInvalidNetId };

    explicit Gate(GateId g_id, std::string_view g_name, GateType g_type)
        : id(g_id), name(g_name), type(g_type) {
    }
};

struct TimingReport {
    std::vector<double> arrival_time;   // indexed by GateId
    std::vector<double> required_time;  // indexed by GateId
    double critical_path_delay{ 0.0 };
    std::vector<GateId> critical_path;  // ordered gate sequence

    [[nodiscard]] double slack(GateId id) const {
        return required_time.at(id) - arrival_time.at(id);
    }
};

class Circuit {
public:
    Circuit() = default;

    // Polymorphic/Heap ownership support
    Circuit(const Circuit&) = delete;
    Circuit& operator=(const Circuit&) = delete;
    Circuit(Circuit&&) noexcept = default;
    Circuit& operator=(Circuit&&) noexcept = default;

    // Factory methods
    GateId add_gate(std::string_view name, GateType type);
    NetId  add_net(std::string_view name);

    // Topology wiring
    void connect_input(GateId gate_id, NetId net_id);
    void connect_output(GateId gate_id, NetId net_id);

    // Fast Accessors
    [[nodiscard]] const Gate& get_gate(GateId id) const { return *gates_.at(id); }
    [[nodiscard]] Gate& get_gate(GateId id) { return *gates_.at(id); }

    [[nodiscard]] const Net& get_net(NetId id)   const { return *nets_.at(id); }
    [[nodiscard]] Net& get_net(NetId id) { return *nets_.at(id); }

    [[nodiscard]] const Gate* find_gate(std::string_view name) const;
    [[nodiscard]] const Net* find_net(std::string_view name) const;

    // View-based iteration over unique_ptr containers
    [[nodiscard]] std::span<const std::unique_ptr<Gate>> gates() const noexcept { return gates_; }
    [[nodiscard]] std::span<const std::unique_ptr<Net>>  nets()  const noexcept { return nets_; }

    [[nodiscard]] std::size_t num_gates() const noexcept { return gates_.size(); }
    [[nodiscard]] std::size_t num_nets()  const noexcept { return nets_.size(); }

    void clear() noexcept;
    bool validate_topology() const;
    std::vector<GateId> topological_sort() const;

private:
    std::vector<std::unique_ptr<Gate>> gates_;
    std::vector<std::unique_ptr<Net>>  nets_;

    std::unordered_map<std::string, GateId> gate_lookup_;
    std::unordered_map<std::string, NetId>  net_lookup_;
};


// TODO: Reference additional headers your program requires here.
