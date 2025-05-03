#include "expr/Expr.hpp"

#include <sstream>
#include <iomanip>
#include <cmath>

namespace mathix {

    // Format numbers: show integers cleanly, floats with fixed precision
    inline std::string format_number(double value) {
        if (std::floor(value) == value) {
            // Format as integer without decimals
            return std::to_string(static_cast<int>(value));
        }
        std::ostringstream out;
        out << std::fixed << std::setprecision(6) << value;

        // Remove trailing zeros
        std::string result = out.str();
        result.erase(result.find_last_not_of('0') + 1, std::string::npos);  // Remove trailing zeros
        if (result.back() == '.') {  // If last char is '.', remove it
            result.pop_back();
        }
        return result;
    }

    // Precedence levels: higher = tighter binding
    inline int get_precedence(const std::string& op) {
        if (op == "Negate") return 4;
        if (op == "Pow")    return 3;
        if (op == "Times" || op == "Divide") return 2;
        if (op == "Plus" || op == "Minus")   return 1;
        return 0; // Lowest
    }

    // Helper: overloaded visitor
    template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
    template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

    // Forward declaration
    std::string to_string(const Expr&);

    // Wrap with parentheses if needed based on precedence
    std::string to_string_with_parens(const ExprPtr& e, int parent_precedence, bool is_right = false) {
        if (auto* f = std::get_if<FunctionCall>(e.get())) {
            int prec = get_precedence(f->head);
            if (prec < parent_precedence || (prec == parent_precedence && is_right)) {
                return "(" + to_string(*e) + ")";
            }
        }
        return to_string(*e);
    }

    std::string to_string(const Expr& expr) {
        return std::visit(overloaded{

            [](const Number& num) -> std::string {
                return format_number(num.value);
            },

            [](const Symbol& sym) -> std::string {
                return sym.name;
            },

            [](const FunctionCall& f) -> std::string {
                const auto& args = f.args;

                if (f.head == "Plus") {
                    std::string result;
                    for (size_t i = 0; i < args.size(); ++i) {
                        if (i > 0) result += " + ";
                        result += to_string_with_parens(args[i], get_precedence("Plus"));
                    }
                    return result;
                }

                if (f.head == "Times") {
                    std::string result;
                    for (size_t i = 0; i < args.size(); ++i) {
                        if (i > 0) result += " * ";
                        result += to_string_with_parens(args[i], get_precedence("Times"));
                    }
                    return result;
                }

                if (f.head == "Minus" && args.size() == 2) {
                    return to_string_with_parens(args[0], get_precedence("Minus")) +
                           " - " +
                           to_string_with_parens(args[1], get_precedence("Minus"), true);
                }

                if (f.head == "Divide" && args.size() == 2) {
                    return to_string_with_parens(args[0], get_precedence("Divide")) +
                           " / " +
                           to_string_with_parens(args[1], get_precedence("Divide"), true);
                }

                if (f.head == "Pow" && args.size() == 2) {
                    return to_string_with_parens(args[0], get_precedence("Pow")) +
                           "^" +
                           to_string_with_parens(args[1], get_precedence("Pow"), true);
                }

                if (f.head == "Negate" && args.size() == 1) {
                    return "-" + to_string_with_parens(args[0], get_precedence("Negate"));
                }

                // Default: head[arg1, arg2, ...]
                std::string result = f.head + "[";
                for (size_t i = 0; i < args.size(); ++i) {
                    result += to_string(args[i]);
                    if (i + 1 < args.size()) result += ", ";
                }
                result += "]";
                return result;
            },

            [](const FunctionDefinition& def) -> std::string {
                std::string result = def.name + "[";
                for (size_t i = 0; i < def.params.size(); ++i) {
                    result += def.params[i] + "_";
                    if (i + 1 < def.params.size()) result += ", ";
                }
                result += "] := " + to_string(def.body);
                return result;
            }

            }, expr);
    }

} // namespace mathix
