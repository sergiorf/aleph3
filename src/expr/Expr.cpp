#include "expr/Expr.hpp"

namespace mathix {

// Helper: overloaded visitor
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

std::string to_string(const Expr& expr) {
    return std::visit(overloaded{
        [](const Number& num) -> std::string {
            return std::to_string(num.value);
        },
        [](const Symbol& sym) -> std::string {
            return sym.name;
        },
        [](const FunctionCall& f) -> std::string {
            const auto& args = f.args;

            // Handle known infix operators
            if (f.head == "Plus" && args.size() == 2) {
                return to_string(args[0]) + " + " + to_string(args[1]);
            }
            else if (f.head == "Minus" && args.size() == 2) {
                return to_string(args[0]) + " - " + to_string(args[1]);
            }
            else if (f.head == "Times" && args.size() == 2) {
                return to_string(args[0]) + " * " + to_string(args[1]);
            }
            else if (f.head == "Divide" && args.size() == 2) {
                return to_string(args[0]) + " / " + to_string(args[1]);
            }
            else if (f.head == "Pow" && args.size() == 2) {
                return to_string(args[0]) + "^" + to_string(args[1]);
            }
            else if (f.head == "Negate" && args.size() == 1) {
                return "-" + to_string(args[0]);
            }

            // Default: print as head[arg1, arg2, ...]
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
