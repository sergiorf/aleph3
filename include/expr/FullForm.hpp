#pragma once
#include "expr/Expr.hpp"
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>

namespace aleph3 {

// Forward declaration
std::string to_fullform(const ExprPtr& expr);

// Helper for printing a Parameter in FullForm style
inline std::string to_fullform(const Parameter& param) {
    if (param.default_value) {
        return "Parameter[" + param.name + ", " + to_fullform(param.default_value) + "]";
    }
    else {
        return "Parameter[" + param.name + "]";
    }
}

// Implementation
inline std::string to_fullform(const ExprPtr& expr) {
    struct Visitor {
        std::ostringstream out;
        void operator()(const Number& n) {
            out << std::setprecision(16) << n.value;
        }
        void operator()(const Complex& c) {
            out << "Complex[" << c.real << ", " << c.imag << "]";
        }
        void operator()(const Symbol& s) {
            out << s.name;
        }
        void operator()(const String& s) {
            out << '"' << s.value << '"';
        }
        void operator()(const Boolean& b) {
            out << (b.value ? "True" : "False");
        }
        void operator()(const Rational& r) {
            out << "Rational[" << r.numerator << ", " << r.denominator << "]";
        }
        void operator()(const FunctionCall& f) {
            out << f.head << "[";
            for (size_t i = 0; i < f.args.size(); ++i) {
                if (i > 0) out << ", ";
                out << to_fullform(f.args[i]);
            }
            out << "]";
        }
        void operator()(const FunctionDefinition& f) {
            out << "FunctionDefinition[" << f.name << ", List[";
            for (size_t i = 0; i < f.params.size(); ++i) {
                if (i > 0) out << ", ";
                out << to_fullform(f.params[i]);
            }
            out << "], " << to_fullform(f.body) << ", " << (f.delayed ? "True" : "False") << "]";
        }
        void operator()(const Assignment& a) {
            out << "Set[" << a.name << ", " << to_fullform(a.value) << "]";
        }
        void operator()(const Rule& r) {
            out << "Rule[" << to_fullform(r.lhs) << ", " << to_fullform(r.rhs) << "]";
        }
        void operator()(const List& l) {
            out << "List[";
            for (size_t i = 0; i < l.elements.size(); ++i) {
                if (i > 0) out << ", ";
                out << to_fullform(l.elements[i]);
            }
            out << "]";
        }
        void operator()(const Infinity&) {
            out << "Infinity";
        }
        void operator()(const Indeterminate&) {
            out << "Indeterminate";
        }
    };

    Visitor v;
    std::visit([&](auto&& arg) { v(arg); }, *expr);
    return v.out.str();
}

} // namespace aleph3