#include "VerilogParser.h"

#include <cctype>
#include <fstream>
#include <sstream>

const std::unordered_map<std::string, GateType> VerilogParser::kGateTypeKeywords = {
    {"and", GateType::AND},   
    {"or", GateType::OR},
    {"nand", GateType::NAND}, 
    {"nor", GateType::NOR},
    {"xor", GateType::XOR},   
    {"xnor", GateType::XNOR},
    {"not", GateType::NOT},   
    {"buf", GateType::BUF},
};

// ---------------------------------------------------------------------------
// Lexer
// ---------------------------------------------------------------------------

void VerilogParser::reset(std::string_view source) {
    source_ = std::string(source);
    pos_ = 0;
    line_ = 1;
}

void VerilogParser::skip_whitespace_and_comments() {
    while (pos_ < source_.size()) {
        const char c = source_[pos_];

        if (c == '\n') {
            ++line_;
            ++pos_;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++pos_;
            continue;
        }
        if (c == '/' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '/') {
            // Line comment: skip to end of line (the newline itself is
            // handled on the next loop iteration so line_ stays accurate).
            // TODO(stretch goal): support /* block comments */ too.
            while (pos_ < source_.size() && source_[pos_] != '\n') {
                ++pos_;
            }
            continue;
        }
        break;
    }
}

VerilogParser::Token VerilogParser::lex_next_token() {
    skip_whitespace_and_comments();

    if (pos_ >= source_.size()) {
        return Token{ TokenType::EndOfFile, "", line_ };
    }

    const char c = source_[pos_];
    const int start_line = line_;

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        std::size_t start = pos_;
        while (pos_ < source_.size() &&
            (std::isalnum(static_cast<unsigned char>(source_[pos_])) || source_[pos_] == '_')) {
            ++pos_;
        }
        return Token{ TokenType::Identifier, source_.substr(start, pos_ - start), start_line };
    }

    switch (c) {
    case '(': ++pos_; return Token{ TokenType::LParen, "(", start_line };
    case ')': ++pos_; return Token{ TokenType::RParen, ")", start_line };
    case ',': ++pos_; return Token{ TokenType::Comma, ",", start_line };
    case ';': ++pos_; return Token{ TokenType::Semicolon, ";", start_line };
    default:
        throw ParseError(std::string("Unexpected character '") + c + "'", start_line);
    }
}

void VerilogParser::advance() {
    current_ = lex_next_token();
}

// ---------------------------------------------------------------------------
// Parser helpers
// ---------------------------------------------------------------------------

void VerilogParser::expect_keyword(const std::string& keyword) {
    if (current_.type != TokenType::Identifier || current_.text != keyword) {
        throw ParseError("Expected '" + keyword + "', got '" + current_.text + "'", current_.line);
    }
    advance();
}

std::string VerilogParser::expect_identifier() {
    if (current_.type != TokenType::Identifier) {
        throw ParseError("Expected an identifier", current_.line);
    }
    std::string text = current_.text;
    advance();
    return text;
}

void VerilogParser::expect_punct(TokenType type, const char* what) {
    if (current_.type != type) {
        throw ParseError(std::string("Expected '") + what + "'", current_.line);
    }
    advance();
}

NetId VerilogParser::get_or_create_net(Circuit& circuit, const std::string& name) {
    if (const Net* existing = circuit.find_net(name)) {
        return existing->id;
    }
    return circuit.add_net(name);
}

// ---------------------------------------------------------------------------
// Grammar
// ---------------------------------------------------------------------------

void VerilogParser::parse_port_list() {
    // Just consumes and discards the header's port names; declarations
    // below (input/output/wire) are what actually matter for building the
    // circuit.
    // TODO(Day 4/stretch): cross-check these names against the input/output
    // declarations and report a mismatch as a parse error -- real Verilog
    // requires every header port to be declared exactly once.
    expect_identifier();
    while (current_.type == TokenType::Comma) {
        advance();
        expect_identifier();
    }
}

void VerilogParser::parse_declaration(Circuit& circuit) {
    const std::string kind = current_.text;  // "input" | "output" | "wire"
    advance();

    std::vector<std::string> names;
    names.push_back(expect_identifier());
    while (current_.type == TokenType::Comma) {
        advance();
        names.push_back(expect_identifier());
    }
    expect_punct(TokenType::Semicolon, ";");

    for (const auto& name : names) {
        const NetId net_id = get_or_create_net(circuit, name);

        if (kind == "input") {
            const GateId pi = circuit.add_gate(name, GateType::INPUT);
            circuit.connect_output(pi, net_id);
        }
        else if (kind == "output") {
            const GateId po = circuit.add_gate(name, GateType::OUTPUT);
            circuit.connect_input(po, net_id);
        }
        // "wire": the net itself is enough, no pseudo-gate needed -- its
        // driver/loads get wired up when gate instances reference it.
    }
}

void VerilogParser::parse_gate_instance(Circuit& circuit) {
    const std::string type_text = current_.text;
    const int decl_line = current_.line;
    const GateType gate_type = kGateTypeKeywords.at(type_text);  // caller already checked membership
    advance();

    const std::string instance_name = expect_identifier();

    expect_punct(TokenType::LParen, "(");
    std::vector<std::string> port_names;
    port_names.push_back(expect_identifier());
    while (current_.type == TokenType::Comma) {
        advance();
        port_names.push_back(expect_identifier());
    }
    expect_punct(TokenType::RParen, ")");
    expect_punct(TokenType::Semicolon, ";");

    // Arity check: not/buf are 1-input (2 ports total: output + input).
    // Everything else in this subset is a fixed 2-input gate (3 ports
    // total: output + 2 inputs).
    // TODO(stretch goal): support N-input gates instead of this fixed check.
    const bool is_unary = (gate_type == GateType::NOT || gate_type == GateType::BUF);
    const std::size_t expected_ports = is_unary ? 2 : 3;
    if (port_names.size() != expected_ports) {
        throw ParseError(
            "Gate '" + instance_name + "' (" + type_text + ") expects " +
            std::to_string(expected_ports) + " port(s), got " +
            std::to_string(port_names.size()),
            decl_line);
    }

    const GateId gate_id = circuit.add_gate(instance_name, gate_type);

    // Verilog gate-primitive port order: output first, then input(s).
    const NetId output_net = get_or_create_net(circuit, port_names[0]);
    circuit.connect_output(gate_id, output_net);

    for (std::size_t i = 1; i < port_names.size(); ++i) {
        const NetId input_net = get_or_create_net(circuit, port_names[i]);
        circuit.connect_input(gate_id, input_net);
    }
}

void VerilogParser::parse_module(Circuit& circuit) {
    expect_keyword("module");
    expect_identifier();  // module name -- unused in this subset (single module per file)

    expect_punct(TokenType::LParen, "(");
    parse_port_list();
    expect_punct(TokenType::RParen, ")");
    expect_punct(TokenType::Semicolon, ";");

    while (!(current_.type == TokenType::Identifier && current_.text == "endmodule")) {
        if (current_.type != TokenType::Identifier) {
            throw ParseError("Expected a declaration, gate instance, or 'endmodule'", current_.line);
        }

        if (current_.text == "input" || current_.text == "output" || current_.text == "wire") {
            parse_declaration(circuit);
        }
        else if (kGateTypeKeywords.count(current_.text)) {
            parse_gate_instance(circuit);
        }
        else {
            throw ParseError("Unknown keyword or gate type '" + current_.text + "'", current_.line);
        }
    }

    expect_keyword("endmodule");

    if (current_.type != TokenType::EndOfFile) {
        throw ParseError("Unexpected tokens after 'endmodule'", current_.line);
        // TODO(stretch goal): once multiple modules per file are supported,
        // this is where you'd loop back into parse_module() again instead
        // of treating trailing content as an error.
    }
}

// ---------------------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------------------

Circuit VerilogParser::parse(std::string_view source) {
    reset(source);
    advance();  // prime current_ with the first token

    Circuit circuit;
    parse_module(circuit);
    return circuit;
}

Circuit VerilogParser::parse_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw ParseError("Could not open file: " + path, 0);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse(buffer.str());
}