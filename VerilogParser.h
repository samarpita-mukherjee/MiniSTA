#pragma once

#include "MiniSTA.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

// Thrown for any malformed input in the simplified Verilog subset. The
// message includes the 1-based source line number where the problem was
// detected, e.g. "Line 7: Gate 'g1' (and) expects 3 ports...".
class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& message, int line)
        : std::runtime_error("Line " + std::to_string(line) + ": " + message) {}
};

// Parses a small structural-Verilog subset into a Circuit:
//
//   module <name> ( <port>, <port>, ... );
//     input  <net>, <net>, ...;
//     output <net>, <net>, ...;
//     wire   <net>, <net>, ...;
//
//     <gate_type> <instance_name> ( <output_net>, <input_net>, ... );
//     ...
//   endmodule
//
// Supported gate_type: and, or, nand, nor, xor, xnor (2-input),
// not, buf (1-input). Declarations and gate instances may appear in any
// order after the module header.
//
// NOT supported (intentionally, to keep this subset small):
//   - behavioral constructs (always, assign, if/case, etc.)
//   - multiple modules per file / module instantiation
//   - bit ranges / vectors (e.g. wire [7:0] bus) -- every identifier is a
//     single 1-bit net
//   - block comments (/* ... */) -- only "// line comments" are stripped
//
// Primary inputs/outputs are represented in the resulting Circuit as
// pseudo-gates of type GateType::INPUT / GateType::OUTPUT (zero delay, see
// Circuit.hpp), so the timing graph has well-defined sources and sinks.
class VerilogParser {
public:
    VerilogParser() = default;

    // Parses Verilog source text directly (useful for tests / in-memory strings).
    Circuit parse(std::string_view source);

    // Reads the file at `path` and parses its contents. Throws ParseError
    // (line 0) if the file can't be opened.
    Circuit parse_file(const std::string& path);

private:
    // ---- token stream -------------------------------------------------
    enum class TokenType {
        Identifier,
        LParen,
        RParen,
        Comma,
        Semicolon,
        EndOfFile
    };

    struct Token {
        TokenType type{ TokenType::EndOfFile };
        std::string text;
        int line{ 0 };
    };

    std::string source_;
    std::size_t pos_{ 0 };
    int line_{ 1 };
    Token current_{};

    void reset(std::string_view source);
    Token lex_next_token();
    void skip_whitespace_and_comments();
    void advance();

    // ---- parsing helpers -----------------------------------------------
    void expect_keyword(const std::string& keyword);
    std::string expect_identifier();
    void expect_punct(TokenType type, const char* what);

    NetId get_or_create_net(Circuit& circuit, const std::string& name);

    void parse_module(Circuit& circuit);
    void parse_port_list();  // module header port list; parsed & discarded
    void parse_declaration(Circuit& circuit);
    void parse_gate_instance(Circuit& circuit);

static const std::unordered_map<std::string, GateType> kGateTypeKeywords;
};
