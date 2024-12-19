#include "parser_utils.hpp"
#include "common.hpp"
#include <charconv>
#include <string_view>
#include <expected>
#include <cstdint>

namespace as {

auto parse_num(std::string_view &source) -> std::expected<word_type, sim::Error> {
    if (source.empty()) {
        return std::unexpected("Expected a number, found ''");
    }

    // Check for negative sign
    const bool is_negative = source[0] == '-';
    if (is_negative) {
        source.remove_prefix(1);
        if (source.empty()) {
            return std::unexpected("Expected a number, found '-'");
        }
    }

    // Lambda to parse an integer with a specific base
    auto parse_integral = [&](int base) -> std::expected<word_type, sim::Error> {
        const char* begin = source.data();

        auto i = 0u;
        while ( i < source.size() && is_numeric(source[i], 16) ) {
            if (!is_numeric(source[i], base)) {
                auto failing_part = std::string_view(begin, begin + i + 1);
                auto digit = source[i];
                source.remove_prefix(i);
                return std::unexpected(std::format("Failed to parse number: '{}': Invalid digit '{}' for base {}", failing_part, digit, base));
            }
            i++;
        }
        const char* end = source.data() + i;
        int32_t result = 0;
        auto [ptr, ec] = std::from_chars(begin, end, result, base);
        if (ec != std::errc{}) {
            auto failing_part = std::string_view(begin, end - begin);
            return std::unexpected(std::format("Failed to parse number '{}': {}", failing_part, std::make_error_code(ec).message()));
        }
        source.remove_prefix(ptr - begin);
        if (is_negative) {
            result = -result;
        }
        return result;
    };

    // Check for number format
    if (source[0] == '0' && source.size() > 1) {
        switch (source[1]) {
            case 'x': case 'X': // Hexadecimal
                source.remove_prefix(2);
                return parse_integral(16);
            case 'b': case 'B': // Binary
                source.remove_prefix(2);
                return parse_integral(2);
            default: // Octal
                return parse_integral(8);
        }
    }

    // Default to decimal
    return parse_integral(10);
}


auto str_check_predicate(const std::string_view str, const std::function<bool(char)>& predicate) -> bool {
    return std::all_of(str.begin(), str.end(), predicate);
}

auto str_to_reg(std::string_view str) -> std::expected<RegisterData, sim::Error> {
    if (str.size() < 2) {
        return std::unexpected(std::format("Invalid register name: '{}'", str));
    }

    auto reg = RegisterData{};
    if (str[0] == 'x') {
        reg.type = RegisterType::VECTOR;
    } else if (str[0] == 's') {
        reg.type = RegisterType::SCALAR;
    } else if (str == "pc") {
        reg.type = RegisterType::PC;
        return reg;
    } else {
        return std::unexpected(std::format("Invalid register name: '{}'", str));
    }

    auto reg_num_str = str.substr(1);
    auto reg_num = std::int32_t{};
    auto [ptr, ec] = std::from_chars(reg_num_str.data(), reg_num_str.data() + reg_num_str.size(), reg_num);
    if (ec != std::errc{}) {
        return std::unexpected(std::format("Failed to parse register number '{}': {}", reg_num_str, std::make_error_code(ec).message()));
    }
    reg.register_number = reg_num;

    return reg;
}

} // namespace as
