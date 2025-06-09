#pragma once
#include <string>
#include <unordered_map>

namespace aleph3 {

    struct HelpEntry {
        std::string name;
        std::string description;
        std::string category;
    };

    inline const std::vector<HelpEntry>& get_help_entries() {
        static const std::vector<HelpEntry> entries = {
            // Trigonometric
            {"Sin", "Sin[x]: Sine of x (x in radians)", "Trigonometric"},
            {"Cos", "Cos[x]: Cosine of x (x in radians)", "Trigonometric"},
            {"Tan", "Tan[x]: Tangent of x (x in radians)", "Trigonometric"},
            {"ArcSin", "ArcSin[x]: Inverse sine of x", "Trigonometric"},
            {"ArcCos", "ArcCos[x]: Inverse cosine of x", "Trigonometric"},
            {"ArcTan", "ArcTan[x]: Inverse tangent of x", "Trigonometric"},
            {"Csc", "Csc[x]: Cosecant of x (1/sin(x))", "Trigonometric"},
            {"Sec", "Sec[x]: Secant of x (1/cos(x))", "Trigonometric"},
            {"Cot", "Cot[x]: Cotangent of x (1/tan(x))", "Trigonometric"},

            // Arithmetic (binary)
            {"Plus", "Plus[a, b]: a + b (addition)", "Arithmetic"},
            {"Minus", "Minus[a, b]: a - b (subtraction)", "Arithmetic"},
            {"Times", "Times[a, b]: a * b (multiplication)", "Arithmetic"},
            {"Divide", "Divide[a, b]: a / b (division)", "Arithmetic"},
            {"Power", "Power[a, b]: a^b (exponentiation)", "Arithmetic"},
            {"Log", "Log[b, x]: Logarithm of x with base b", "Exponential/Logarithmic"},
            {"ArcTan", "ArcTan[x, y]: Two-argument arctangent (atan2)", "Trigonometric"},

            // Hyperbolic
            {"Sinh", "Sinh[x]: Hyperbolic sine of x", "Hyperbolic"},
            {"Cosh", "Cosh[x]: Hyperbolic cosine of x", "Hyperbolic"},
            {"Tanh", "Tanh[x]: Hyperbolic tangent of x", "Hyperbolic"},
            {"Coth", "Coth[x]: Hyperbolic cotangent of x", "Hyperbolic"},
            {"Sech", "Sech[x]: Hyperbolic secant of x", "Hyperbolic"},
            {"Csch", "Csch[x]: Hyperbolic cosecant of x", "Hyperbolic"},

            // Exponential/Logarithmic
            {"Exp", "Exp[x]: Exponential function e^x", "Exponential/Logarithmic"},
            {"Log", "Log[x]: Natural logarithm of x", "Exponential/Logarithmic"},

            // Other math
            {"Abs", "Abs[x]: Absolute value of x", "Other"},
            {"Floor", "Floor[x]: Greatest integer <= x", "Other"},
            {"Ceiling", "Ceiling[x]: Smallest integer >= x", "Other"},
            {"Sqrt", "Sqrt[x]: Square root of x", "Other"},
            {"Round", "Round[x]: Round x to the nearest integer", "Other"},
            {"Gamma", "Gamma[x]: Gamma function of x", "Other"},
            {"Rational", "Rational[n, d]: Rational number n/d (exact)", "Other"},

            // Polynomial manipulation
            {"Expand", "Expand[expr]: Expand out products and powers in a polynomial expression", "Polynomial"},
            {"Factor", "Factor[expr]: Factor a polynomial expression over the integers", "Polynomial"},
            {"Collect", "Collect[expr, x]: Collect terms in expr by powers of x", "Polynomial"},
            {"GCD", "GCD[a, b]: Greatest common divisor of two polynomials or integers", "Polynomial"},
            {"PolynomialQuotient", "PolynomialQuotient[a, b, x]: Quotient of a divided by b with respect to variable x", "Polynomial"},
            
            // Logical
            {"And", "And[a, b, ...]: Logical AND (True if all arguments are True)", "Logical"},
            {"Or", "Or[a, b, ...]: Logical OR (True if any argument is True)", "Logical"},

            // String functions
            {"StringJoin", "StringJoin[str1, str2, ...]: Concatenate strings", "String"},
            {"StringLength", "StringLength[str]: Length of a string", "String"},
            {"StringReplace", "StringReplace[str, rule]: Replace substrings using a rule", "String"},
            {"StringTake", "StringTake[str, n or {start, end}]: Take substring by count or range", "String"},

            // List functions
            {"Length", "Length[list]: Number of elements in a list", "List"},

            // Numeric
            {"N", "N[expr]: Evaluate numerically", "Numeric"},

            // Output/Display
            {"FullForm", "FullForm[expr]: Show the internal structure of expr", "Other"},

            // Constants (not functions, but useful for help)
            {"Pi", "Pi: The mathematical constant π ≈ 3.14159", "Constants"},
            {"E", "E: The mathematical constant e ≈ 2.71828", "Constants"},
            {"Degree", "Degree: 1 degree = Pi/180 radians", "Constants"},
        };
        return entries;
    }

}