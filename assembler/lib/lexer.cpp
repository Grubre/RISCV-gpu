#include "lexer.hpp"
#include "common.hpp"
#include "instructions.hpp"
#include "parser_utils.hpp"

namespace as {

auto Lexer::chop() -> char {
    auto result = source.front();
    source.remove_prefix(1);
    column_number++;
    return result;
}

auto Lexer::chop_while(const std::function<bool(char)> &predicate) -> std::string_view {
    auto i = 0u;
    while (i < source.size() && predicate(source[i])) {
        i++;
        column_number++;
    }

    auto result = source.substr(0, i);
    source.remove_prefix(i);
    return result;
}

auto Lexer::peek() -> std::optional<char> {
    if (source.empty()) {
        return std::nullopt;
    }
    return source.front();
}

void Lexer::trim_whitespace() {
    while (auto c = peek()) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            chop();
        } else {
            break;
        }
    }
}

auto Lexer::make_error(const std::string_view message, std::optional<uint32_t> col) -> Error {
    auto column_number_loc = col.value_or(this->column_number);
    return std::format("Error:{}: {}.", column_number_loc, message);
}

auto Lexer::parse_directive() -> std::expected<Token, Error> {
    const auto keyword = chop_while(is_alphabetic);

    if (keyword == "threads") {
        return Token{ThreadsDirective{}, column_number};
    }

    if (keyword == "warps") {
        return Token{WarpsDirective{}, column_number};
    }

    return std::unexpected(make_error(std::format("Unexpected keyword '{}'", keyword)));
}

auto Lexer::parse_number() -> std::expected<Token, Error> {
    const auto starting_col = column_number;
    const auto source_size_before = source.size();
    auto number = parse_num(source);
    if (!number.has_value()) {
        auto error = number.error();
        return std::unexpected(make_error(error));
    }
    column_number += source_size_before - source.size();
    return Token{Immediate{*number}, starting_col};
}

auto Lexer::parse_keyword() -> std::expected<Token, Error> {
    const auto starting_col = column_number;

    auto word = chop_while([](char c) { return is_alphanumeric(c) || is_label_char(c) || c == '.' || c == ':'; });
    const auto opcode = sim::opcode::str_to_opcode(word);

    std::println("word: '{}', rest: '{}'", word, source);

    // 1. Check if it's an opcode
    if (opcode.has_value()) {
        return Token{Mnemonic{.mnemonic = *opcode}, starting_col};
    }

    // 2. Check if it's a label
    if (word.back() == ':') {
        word.remove_suffix(1);
        if (str_check_predicate(word, is_label_char)) {
            return Token{Label{.name = word}, starting_col};
        }
    }

    // 3. Check if it's a register
    auto reg_error = std::optional<Error>{};
    if (word[0] == 'x' || word[0] == 's' || word == "pc") {
        auto reg = str_to_reg(word);
        if (reg.has_value()) {
            return Token{Register{.register_data = *reg}, starting_col};
        } else {
            reg_error = reg.error();
        }
    }

    // 4. Check if it's a label reference
    if (str_check_predicate(word, is_label_char)) {
        return Token{LabelRef{.label_name = word}, starting_col};
    }

    // 5. If none of the above, return an error
    if (reg_error) {
        return std::unexpected(make_error(reg_error.value(), starting_col));
    }

    return std::unexpected(make_error(std::format("Unexpected keyword '{}'", word), starting_col));
}

auto Lexer::next_token() -> std::optional<std::expected<Token, Error>> {
    trim_whitespace();

    const auto first_char_column = column_number;

    if (source.empty()) {
        return std::nullopt;
    }

    const char c = *peek();

    if (is_numeric(c)) {
        return parse_number();
    }

    if (c == '.') {
        chop();
        return parse_directive();
    }

    if (is_alphabetic(c)) {
        return parse_keyword();
    }

    return std::unexpected(make_error(std::format("Unexpected character '{}'", c), first_char_column));
}

auto collect_tokens(const std::string_view source) -> std::pair<std::vector<Token>, std::vector<sim::Error>> {
    auto tokens = std::vector<Token>{};
    auto errors = std::vector<sim::Error>{};

    for (const auto &next_token : Lexer(source)) {
        if (next_token.has_value()) {
            tokens.push_back(next_token.value());
        } else {
            errors.push_back(next_token.error());
        }
    }

    return {tokens, errors};
}

}
