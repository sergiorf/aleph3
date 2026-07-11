/*
 * CLI Help Text Catalog
 * ---------------------
 * Defines the static help entries shown by the Aleph3 CLI. This header owns
 * the plain-language descriptions for user-facing functions and commands.
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace aleph3 {

    struct HelpEntry {
        std::string name;
        std::string description;
        std::string category;
        std::vector<std::string> forms;
        std::vector<std::string> examples;
        std::string exactness;
        std::string unsupported;
        std::string owning_component;
        std::string manual_anchor;
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
            {"ArcSec", "ArcSec[x]: Inverse secant of x", "Trigonometric"},
            {"ArcCsc", "ArcCsc[x]: Inverse cosecant of x", "Trigonometric"},
            {"ArcCot", "ArcCot[x]: Inverse cotangent of x", "Trigonometric"},
            {"Csc", "Csc[x]: Cosecant of x (1/sin(x))", "Trigonometric"},
            {"Sec", "Sec[x]: Secant of x (1/cos(x))", "Trigonometric"},
            {"Cot", "Cot[x]: Cotangent of x (1/tan(x))", "Trigonometric"},
            {"Sinc", "Sinc[x]: Normalized sinc function sin(x)/x with Sinc[0] = 1", "Trigonometric"},

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
            {"Ln", "Ln[x]: Natural logarithm of x", "Exponential/Logarithmic"},
            {"Log", "Log[x]: Natural logarithm of x", "Exponential/Logarithmic"},

            // Other math
            {"Abs", "Abs[x]: Absolute value of x", "Other"},
            {"Ceil", "Ceil[x]: Alias for Ceiling[x]", "Other"},
            {"Floor", "Floor[x]: Greatest integer <= x", "Other"},
            {"Ceiling", "Ceiling[x]: Smallest integer >= x", "Other"},
            {"Sqrt", "Sqrt[x]: Square root of x", "Other"},
            {"Round", "Round[x]: Round x to the nearest integer", "Other"},
            {"Gamma", "Gamma[x]: Gamma function of x", "Other"},
            {"Rational", "Rational[n, d]: Rational number n/d (exact)", "Other"},
            {"Replace", "Apply one supported rewrite rule through the existing whole-expression traversal.", "Symbolic",
                {"Replace[expr, rule]", "Replace[expr, rule, level]"},
                {"Replace[f[x], f[a_] -> g[a]] -> g[x]"},
                "Preserves exact expressions unless the replacement rule introduces approximate values.",
                "Rule lists and RuleDelayed are unsupported in the current replacement surface.",
                "builtin", "manual/rewriting-and-assumptions.md"},
            {"ReplaceAll", "Alias for Replace[expr, rule] using the same whole-expression traversal.", "Symbolic",
                {"ReplaceAll[expr, rule]", "expr /. rule"},
                {"f[x] /. x -> y -> f[y]"},
                "Preserves exact expressions unless the replacement rule introduces approximate values.",
                "Rule lists and RuleDelayed are unsupported.",
                "builtin", "manual/rewriting-and-assumptions.md"},
            {"ReplaceRepeated", "ReplaceRepeated[expr, rule, level]: Reapply a rule at an optional bounded depth", "Symbolic"},
            {"MatchQ", "MatchQ[expr, pattern]: Test whether expr matches a supported symbolic pattern", "Symbolic"},
            {"Rule", "Build a rewrite rule from a left-hand side to a right-hand side.", "Symbolic",
                {"lhs -> rhs"}, {"f[x] /. x -> y -> f[y]"}, "", "", "syntax", "manual/rewriting-and-assumptions.md"},
            {"RuleDelayed", "Reserved replacement form; not supported by the current evaluator.", "Symbolic",
                {"lhs :> rhs"}, {}, "", "RuleDelayed is not accepted by Replace or ReplaceAll.", "syntax",
                "manual/rewriting-and-assumptions.md"},
            {"Set", "Assign a session-local own value.", "Symbolic",
                {"name = expr"}, {"a = 2", "a + 3 -> 5"},
                "Stores the evaluated right-hand side in the active session.",
                "Provider-owned names still keep provider precedence.", "session", "manual/sessions-cli-and-notebook.md"},
            {"SetDelayed", "Define a session-local user function evaluated when it is called.", "Symbolic",
                {"f[x_] := expr"}, {"f[x_] := x + 1", "f[4] -> 5"}, "",
                "Only the bounded user-definition subset is supported.", "session", "manual/sessions-cli-and-notebook.md"},
            {"Clear", "Remove a session-local own value and user function definition.", "Symbolic",
                {"Clear[symbol]"}, {"a = 10", "Clear[a]", "a -> a"}, "",
                "Clear cannot remove builtin, special-form, pack, or host-owned behavior.", "builtin",
                "manual/sessions-cli-and-notebook.md"},
            {"Unset", "Remove a session-local own value only.", "Symbolic",
                {"Unset[symbol]"}, {"a = 10", "Unset[a]", "a -> a"}, "",
                "Unset does not remove user function definitions in this MVP slice.", "builtin",
                "manual/sessions-cli-and-notebook.md"},
            {"FreeVariables", "FreeVariables[expr]: List symbols that occur free in expr", "Symbolic"},
            {"BoundVariables", "BoundVariables[expr]: List pattern or function-definition variables bound in expr", "Symbolic"},
            {"DependsOn", "DependsOn[expr, x]: Test whether expr has x as a free variable", "Symbolic"},
            {"Assuming", "Assuming[assumptions, expr]: Evaluate expr using temporary boolean, sign, or domain facts", "Symbolic"},
            {"Refine", "Refine[expr, assumptions]: Simplify expr using temporary boolean, sign, or domain facts", "Symbolic"},
            {"Positive", "Positive[x]: Test whether x is known to be greater than zero", "Symbolic"},
            {"Negative", "Negative[x]: Test whether x is known to be less than zero", "Symbolic"},
            {"NonNegative", "NonNegative[x]: Test whether x is known to be greater than or equal to zero", "Symbolic"},
            {"NonPositive", "NonPositive[x]: Test whether x is known to be less than or equal to zero", "Symbolic"},
            {"ZeroQ", "ZeroQ[x]: Test whether x is known to be exactly zero", "Symbolic"},
            {"NonZeroQ", "NonZeroQ[x]: Test whether x is known to be nonzero", "Symbolic"},
            {"IntegerQ", "IntegerQ[x]: Test whether x is known to be an integer", "Symbolic"},
            {"RationalQ", "RationalQ[x]: Test whether x is known to be an exact rational", "Symbolic"},
            {"RealQ", "RealQ[x]: Test whether x is known to be a real quantity", "Symbolic"},

            // Polynomial manipulation
            {"Expand", "Expand[expr]: Expand out products and powers in a polynomial expression", "Polynomial"},
            {"Factor", "Factor a supported exact polynomial expression.", "Polynomial",
                {"Factor[expr]"},
                {"Factor[x^2 - 1] -> (x - 1) * (x + 1)"},
                "Preserves exact integer and rational polynomial coefficients.",
                "Broad transcendental and general multivariate factorization remain unsupported.",
                "core-algebra", "manual/built-in-functions.md"},
            {"Collect", "Collect[expr, x]: Collect terms in expr by powers of x", "Polynomial"},
            {"GCD", "GCD[a, b, vars]: Exact polynomial GCD, including bounded multivariate monomial content with explicit vars", "Polynomial"},
            {"PolynomialQuotient", "PolynomialQuotient[a, b, vars]: Exact quotient and remainder using explicit variable precedence", "Polynomial"},

            // Calculus
            {"D", "Differentiate an expression with respect to a symbol in the focused calculus subset.", "Calculus",
                {"D[expr, x]"},
                {"D[x^2 + 3*x, x] -> 2 * x + 3"},
                "Exact polynomial-style results stay exact.",
                "Higher-order, broad partial-derivative, and general calculus workflows are outside this slice.",
                "core-calculus", "manual/built-in-functions.md"},
            {"Differentiate", "Differentiate[expr, x]: Alias for D[expr, x]", "Calculus"},

            // Exact dense matrices
            {"MatrixAdd", "MatrixAdd[a, b]: Add exact dense matrices of equal shape", "Matrix"},
            {"MatrixMultiply", "MatrixMultiply[a, b]: Multiply compatible exact dense matrices", "Matrix"},
            {"IdentityMatrix", "IdentityMatrix[n]: Construct an exact n by n identity matrix", "Matrix"},
            {"Transpose", "Transpose[a]: Transpose an exact dense matrix", "Matrix"},
            {"Det", "Det[a]: Compute an exact determinant", "Matrix"},
            {"RowReduce", "RowReduce[a]: Compute exact reduced row-echelon form", "Matrix"},
            {"LinearSolve", "LinearSolve[a, b]: Solve a square exact system with a unique solution", "Matrix"},
            
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
            {"Head", "Head[expr]: Return the public symbolic head of an expression", "List"},
            {"Part", "Part[expr, index]: Extract a one-based part from a list, call, or rule", "List"},
            {"Map", "Map[f, list]: Apply f to each element of an explicit finite list", "List"},
            {"Apply", "Apply[f, list]: Replace the list head with f", "List"},
            {"Select", "Select[list, predicate]: Keep elements accepted by predicate", "List"},
            {"Cases", "Cases[list, pattern]: Return elements matching a supported pattern", "List"},

            // Numeric
            {"N", "N[expr]: Evaluate numerically", "Numeric",
                {"N[expr]"}, {},
                "Exact integers and rationals remain exact until N or machine-real inputs request approximation.",
                "Arbitrary precision and broad numerical analysis are deferred.",
                "builtin", "manual/built-in-functions.md"},

            // Output/Display
            {"FullForm", "FullForm[expr]: Show the internal structure of expr", "Other"},

            // Constants (not functions, but useful for help)
            {"Pi", "Pi: The mathematical constant π ≈ 3.14159", "Constants"},
            {"E", "E: The mathematical constant e ≈ 2.71828", "Constants"},
            {"Degree", "Degree: 1 degree = Pi/180 radians", "Constants"},
        };
        return entries;
    }

    inline const HelpEntry* find_help_entry(const std::string& name) {
        for (const auto& entry : get_help_entries()) {
            if (entry.name == name) {
                return &entry;
            }
        }
        return nullptr;
    }

}
