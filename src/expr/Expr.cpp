#include "expr/Expr.hpp"

namespace mathix {

// Helper: overloaded visitor
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

std::string to_string(const Expr& expr) {
    return std::visit(overloaded {
        [](const Symbol& sym) -> std::string {
            return sym.name;
        },
        [](const Number& num) -> std::string {
            return std::to_string(num.value);
        },
        [](const FunctionCall& call) -> std::string {
            std::string result = call.head + "[";
            for (size_t i = 0; i < call.args.size(); ++i) {
                result += to_string(call.args[i]);
                if (i + 1 < call.args.size()) {
                    result += ", ";
                }
            }
            result += "]";
            return result;
        }
    }, expr);
}

} // namespace mathix
