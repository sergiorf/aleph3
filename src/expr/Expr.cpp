#include "expr/Expr.hpp"

#include <sstream>
#include <iomanip>
#include <cmath>

namespace aleph3 {

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
        if (op == "Power")    return 3;
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
            
            [](const Boolean& boolean) -> std::string {
            return boolean.value ? "True" : "False";
            },

            [](const String& str) -> std::string { 
                return "\"" + str.value + "\""; 
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
                    // Special case: Times[-1, x] => -x
                    if (args.size() == 2) {
                        if (auto num = std::get_if<Number>(args[0].get()); num && num->value == -1) {
                            return "-" + to_string_with_parens(args[1], get_precedence("Negate"));
                        }
                        if (auto num = std::get_if<Number>(args[1].get()); num && num->value == -1) {
                            return "-" + to_string_with_parens(args[0], get_precedence("Negate"));
                        }
                    }
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
                if (f.head == "Power" && args.size() == 2) {
                    return to_string_with_parens(args[0], get_precedence("Power")) +
                           "^" +
                           to_string_with_parens(args[1], get_precedence("Power"), true);
                }
                if (f.head == "Negate" && args.size() == 1) {
                    return "-" + to_string_with_parens(args[0], get_precedence("Negate"));
                }
                // Comparison operators
                if (f.head == "Equal" && args.size() == 2) {
                    return to_string_with_parens(args[0], 0) + " == " + to_string_with_parens(args[1], 0);
                }
                if (f.head == "NotEqual" && args.size() == 2) {
                    return to_string_with_parens(args[0], 0) + " != " + to_string_with_parens(args[1], 0);
                }
                if (f.head == "Less" && args.size() == 2) {
                    return to_string_with_parens(args[0], 0) + " < " + to_string_with_parens(args[1], 0);
                }
                if (f.head == "Greater" && args.size() == 2) {
                    return to_string_with_parens(args[0], 0) + " > " + to_string_with_parens(args[1], 0);
                }
                if (f.head == "LessEqual" && args.size() == 2) {
                    return to_string_with_parens(args[0], 0) + " <= " + to_string_with_parens(args[1], 0);
                }
                if (f.head == "GreaterEqual" && args.size() == 2) {
                    return to_string_with_parens(args[0], 0) + " >= " + to_string_with_parens(args[1], 0);
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
                    result += def.params[i].name + "_";
                    if (def.params[i].default_value) {
                        result += ":" + to_string(def.params[i].default_value);
                    }
                    if (i + 1 < def.params.size()) result += ", ";
                }
                // Use `:=` for delayed assignment and `=` for immediate assignment
                result += (def.delayed ? "] := " : "] = ") + to_string(def.body);
                return result;
            },

            [](const Assignment& assign) -> std::string {
                return assign.name + " = " + to_string(assign.value);
            },

            [](const Rule& rule) -> std::string {
                return to_string(rule.lhs) + " -> " + to_string(rule.rhs);
            },

            [](const Infinity&) -> std::string {
                return "Infinity";
            },

            [](const Indeterminate&) -> std::string {
                return "Indeterminate";
            },

            [](const List& list) -> std::string {
                std::string result = "{";
                for (size_t i = 0; i < list.elements.size(); ++i) {
                    result += to_string(list.elements[i]);
                    if (i + 1 < list.elements.size()) result += ", ";
                }
                result += "}";
                return result;
            },

            }, expr);
    }

    // Raw version: never adds parentheses
    std::string to_string_raw(const Expr& expr) {
        return std::visit(overloaded{
            [](const Number& num) -> std::string {
                return format_number(num.value);
            },
            [](const Symbol& sym) -> std::string {
                return sym.name;
            },
            [](const Boolean& boolean) -> std::string {
                return boolean.value ? "True" : "False";
            },
            [](const String& str) -> std::string {
                return "\"" + str.value + "\"";
            },
            [](const FunctionCall& f) -> std::string {
                // Comparison operators
                if (f.head == "Equal" && f.args.size() == 2)
                    return to_string_raw(*f.args[0]) + "==" + to_string_raw(*f.args[1]);
                if (f.head == "NotEqual" && f.args.size() == 2)
                    return to_string_raw(*f.args[0]) + "!=" + to_string_raw(*f.args[1]);
                if (f.head == "Less" && f.args.size() == 2)
                    return to_string_raw(*f.args[0]) + "<" + to_string_raw(*f.args[1]);
                if (f.head == "Greater" && f.args.size() == 2)
                    return to_string_raw(*f.args[0]) + ">" + to_string_raw(*f.args[1]);
                if (f.head == "LessEqual" && f.args.size() == 2)
                    return to_string_raw(*f.args[0]) + "<=" + to_string_raw(*f.args[1]);
                if (f.head == "GreaterEqual" && f.args.size() == 2)
                    return to_string_raw(*f.args[0]) + ">=" + to_string_raw(*f.args[1]);
                // For Plus, Times, etc., just join args with operator, no parens
                if (f.head == "Plus" || f.head == "Times" || f.head == "Divide" || f.head == "Power" || f.head == "Minus") {
                    std::string op;
                    if (f.head == "Plus") op = "+";
                    else if (f.head == "Times") op = "*";
                    else if (f.head == "Divide") op = "/";
                    else if (f.head == "Power") op = "^";
                    else if (f.head == "Minus") op = "-";
                    std::string result;
                    for (size_t i = 0; i < f.args.size(); ++i) {
                        if (i > 0) result += op;
                        result += to_string_raw(*f.args[i]);
                    }
                    return result;
                }
                // Negate: always -arg
                if (f.head == "Negate" && f.args.size() == 1) {
                    return "-" + to_string_raw(*f.args[0]);
                }
                // Default: head[arg1,arg2,...]
                std::string result = f.head + "[";
                for (size_t i = 0; i < f.args.size(); ++i) {
                    result += to_string_raw(*f.args[i]);
                    if (i + 1 < f.args.size()) result += ",";
                }
                result += "]";
                return result;
            },
            [](const FunctionDefinition& def) -> std::string {
                return def.name; // Not needed for keys
            },
            [](const Assignment& assign) -> std::string {
                return assign.name;
            },
            [](const Rule& rule) -> std::string {
                return to_string_raw(*rule.lhs) + "->" + to_string_raw(*rule.rhs);
            },
            [](const Infinity&) -> std::string {
                return "Infinity";
            },
            [](const Indeterminate&) -> std::string {
                return "Indeterminate";
            },
            [](const List& list) -> std::string {
                std::string result = "{";
                for (size_t i = 0; i < list.elements.size(); ++i) {
                    result += to_string_raw(*list.elements[i]);
                    if (i + 1 < list.elements.size()) result += ",";
                }
                result += "}";
                return result;
            }
            }, expr);
    }

}
