#include "parser.hpp"
#include "instructions.hpp"
#include "lexer.hpp"
#include "token.hpp"

#define EXPECT_OR_RETURN(opt, type) \
    auto opt = expect<type>();     \
    if (!opt.has_value()) {        \
        return std::nullopt;       \
    }

namespace as {

namespace parser {
auto to_string(const ImmediateOrLabelref &imm) -> std::string {
    return std::visit(overloaded{
                          [](const token::Immediate &imm) -> std::string { return std::to_string(imm.value); },
                          [](const token::LabelRef &label) -> std::string { return std::string{label.label_name}; },
                      },
                      imm);
}

auto line_to_str(const parser::Line &line) -> std::string {
    return std::visit(overloaded{
                          [](const parser::JustLabel &label) { return std::format("{}:", label.label.name); },
                          [](const parser::BlocksDirective &blocks) { return std::format(".blocks {}", blocks.number); },
                          [](const parser::WarpsDirective &warps) { return std::format(".warps {}", warps.number); },
                          [](const parser::Instruction &instruction) {
                              return instruction.to_str();
                          },
                      },
                      line);
}

}

#define CHECK_REG(reg, should_be_scalar) \
    if (!check_register_correct_type(reg, should_be_scalar)) { \
        return std::nullopt; \
    }

auto Parser::check_register_correct_type(const Token &reg_token, bool should_be_scalar) -> bool {
    const auto &reg = reg_token.as<token::Register>();
    if (reg.register_data.is_scalar() != should_be_scalar) {
        push_err(std::format("Register '{}' should be {}", reg.register_data.to_str(), should_be_scalar ? "scalar" : "vector"), reg_token.col);
        return false;
    }

    return true;
}

auto Parser::chop() -> std::optional<Token> {
    if (tokens.empty()) {
        return std::nullopt;
    }

    auto token = tokens.front();
    tokens = tokens.subspan(1);

    return token;
}

[[nodiscard]] auto Parser::peek() const -> const Token * {
    if (tokens.empty()) {
        return nullptr;
    }

    return &tokens.front();
}

void Parser::push_err(Error &&err) { errors.push_back(std::move(err)); }

void Parser::push_err(std::string &&message, unsigned column) {
    errors.push_back(Error{.message = std::move(message), .column = column});
}

void Parser::throw_unexpected_token(std::string &&expected, const Token &unexpected) {
    push_err(std::format("Unexpected token: Expected {}, instead found {}", expected, unexpected.to_str()), unexpected.col);
}

void Parser::throw_unexpected_eos(std::string &&expected) {
    push_err(std::format("Unexpected end of stream: Expected {}", expected), 0);
}

auto Parser::parse_instruction() -> std::optional<Result> {
    auto mnemonic_token = *chop();
    auto mnemonic = mnemonic_token.as<token::Mnemonic>().mnemonic;

    // HALT
    if (mnemonic.get_name() == sim::MnemonicName::HALT) {
        return parser::Instruction{.mnemonic = mnemonic};
    }

    // ADDI, SLTI, XORI, ORI, ANDI, SLLI, SRLI, SRAI, SX_SLTI
    if (parser::is_itype_arithmetic(mnemonic.get_name())) {
        return parse_itype_arithemtic_instruction(mnemonic);
    }

    // ADD, SUB, SLL, SLT, XOR, SRL, SRA, OR, AND, SX_SLT
    if (parser::is_rtype(mnemonic.get_name())) {
        return parse_rtype_instruction(mnemonic);
    }

    // LB, LH, LW
    if (parser::is_load_type(mnemonic.get_name())) {
        return parse_load_instruction(mnemonic);
    }

    // SB, SH, SW
    if (parser::is_store_type(mnemonic.get_name())) {
        return parse_store_instruction(mnemonic);
    }

    push_err(std::format("Unknown mnemonic: '{}'", mnemonic.to_str()), mnemonic_token.col);
    return std::nullopt;
}

// <opcode> <rd>, <rs1>, <imm12>
auto Parser::parse_itype_arithemtic_instruction(const sim::Mnemonic &mnemonic) -> std::optional<parser::Instruction> {
    EXPECT_OR_RETURN(rd, token::Register);
    EXPECT_OR_RETURN(comma1, token::Comma);
    EXPECT_OR_RETURN(rs1, token::Register);
    EXPECT_OR_RETURN(comma2, token::Comma);
    EXPECT_OR_RETURN(imm12, token::Immediate);

    // if it's a vector scalar instuction, the destination register should be scalar but the source registers should be vector
    if (mnemonic.get_name() == sim::MnemonicName::SX_SLTI) {
        CHECK_REG(*rd, true);
        CHECK_REG(*rs1, false);
    } else {
        CHECK_REG(*rd, mnemonic.is_scalar());
        CHECK_REG(*rs1, mnemonic.is_scalar());
    }

    auto instruction = parser::Instruction{
        .label = {},
        .mnemonic = mnemonic,
        .operands = parser::ItypeOperands{
            .rd = rd->as<token::Register>().register_data,
            .rs1 = rs1->as<token::Register>().register_data,
            .imm12 = imm12->as<token::Immediate>(),
        },
    };

    return instruction;
}

auto Parser::parse_rtype_instruction(const sim::Mnemonic& mnemonic) -> std::optional<parser::Instruction> {
    EXPECT_OR_RETURN(rd, token::Register);
    EXPECT_OR_RETURN(comma1, token::Comma);
    EXPECT_OR_RETURN(rs1, token::Register);
    EXPECT_OR_RETURN(comma2, token::Comma);
    EXPECT_OR_RETURN(rs2, token::Register);

    // if it's a vector scalar instuction, the destination register should be scalar but the source registers should be vector
    if (mnemonic.get_name() == sim::MnemonicName::SX_SLT) {
        CHECK_REG(*rd, true);
        CHECK_REG(*rs1, false);
        CHECK_REG(*rs2, false);
    } else {
        CHECK_REG(*rd, mnemonic.is_scalar());
        CHECK_REG(*rs1, mnemonic.is_scalar());
        CHECK_REG(*rs2, mnemonic.is_scalar());
    }

    auto instruction = parser::Instruction{
        .label = {},
        .mnemonic = mnemonic,
        .operands = parser::RtypeOperands{
            .rd = rd->as<token::Register>().register_data,
            .rs1 = rs1->as<token::Register>().register_data,
            .rs2 = rs2->as<token::Register>().register_data,
        },
    };

    return instruction;
}

auto Parser::parse_load_instruction(const sim::Mnemonic& mnemonic) -> std::optional<parser::Instruction> {
    EXPECT_OR_RETURN(rd, token::Register);
    EXPECT_OR_RETURN(comma1, token::Comma);
    EXPECT_OR_RETURN(offset, token::Immediate);
    EXPECT_OR_RETURN(lparen, token::Lparen);
    EXPECT_OR_RETURN(rs1, token::Register);
    EXPECT_OR_RETURN(rparen, token::Rparen);

    CHECK_REG(*rd, mnemonic.is_scalar());
    CHECK_REG(*rs1, mnemonic.is_scalar());

    auto instruction = parser::Instruction{
        .label = {},
        .mnemonic = mnemonic,
        .operands = parser::ItypeOperands{
            .rd = rd->as<token::Register>().register_data,
            .rs1 = rs1->as<token::Register>().register_data,
            .imm12 = offset->as<token::Immediate>(),
        },
    };

    return instruction;
}


auto Parser::parse_store_instruction(const sim::Mnemonic& mnemonic) -> std::optional<parser::Instruction> {
    EXPECT_OR_RETURN(rs2, token::Register);
    EXPECT_OR_RETURN(comma1, token::Comma);
    EXPECT_OR_RETURN(offset, token::Immediate);
    EXPECT_OR_RETURN(lparen, token::Lparen);
    EXPECT_OR_RETURN(rs1, token::Register);
    EXPECT_OR_RETURN(rparen, token::Rparen);

    CHECK_REG(*rs1, mnemonic.is_scalar());
    CHECK_REG(*rs2, mnemonic.is_scalar());

    auto instruction = parser::Instruction{
        .label = {},
        .mnemonic = mnemonic,
        .operands = parser::StypeOperands{
            .rs1 = rs1->as<token::Register>().register_data,
            .rs2 = rs2->as<token::Register>().register_data,
            .imm12 = offset->as<token::Immediate>(),
        },
    };

    return instruction;
}


auto Parser::parse_directive() -> std::optional<Result> {
    auto token = chop();
    if (!token) {
        return std::nullopt;
    }

    auto number = expect<token::Immediate>();
    if(!number.has_value()) {
        return std::nullopt;
    }

    const auto value = number->as<token::Immediate>().value;
    if (value < 1) {
        push_err(std::format("Invalid number of {}: '{}'", token->to_str(), value), number->col);
        return std::nullopt;
    }

    // The line should end here, otherwise it's an error
    if (peek() != nullptr) {
        throw_unexpected_token("End of line", *peek());
        return std::nullopt;
    }

    if (token->is_of_type<token::BlocksDirective>()) {
        return parser::BlocksDirective{.number = static_cast<std::uint32_t>(value)};
    } else {
        return parser::WarpsDirective{.number = static_cast<std::uint32_t>(value)};
    }

    return std::nullopt;
}

auto Parser::parse_line() -> std::optional<Result> {
    if (tokens.empty()) {
        return std::nullopt;
    }

    auto token = *peek();

    if (token.is_of_type<token::BlocksDirective>() || token.is_of_type<token::WarpsDirective>()) {
        return parse_directive();
    }

    std::optional<as::token::Label> label = std::nullopt;

    if (token.is_of_type<token::Label>()) {
        label = token.as<token::Label>();
        chop();

        if (tokens.empty()) {
            return parser::JustLabel{.label = token.as<token::Label>()};
        }

        token = *peek();
    }

    if (token.is_of_type<token::Mnemonic>()) {
        auto instruction = parse_instruction();

        if(!instruction.has_value()) {
            return std::nullopt;
        }

        std::get<parser::Instruction>(instruction.value()).label = label;

        if (tokens.empty()) {
            return instruction;
        }

        push_err(std::format("Unexpected token: Expected end of line, instead found '{}'", peek()->to_str()), token.col);
        return std::nullopt;
    }

    push_err(std::format("Unexpected token: Expected mnemonic or directive, instead found '{}'", token.to_str()), token.col);
    return std::nullopt;
}

auto parse_line(std::span<Token> tokens) -> std::expected<Parser::Result, std::vector<Parser::Error>> { 
    Parser parser{tokens};
    auto result = parser.parse_line();
    if (!result.has_value()) {
        return std::unexpected(parser.consume_errors());
    }

    return result.value();
}

}