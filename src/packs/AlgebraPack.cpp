#include "packs/AlgebraPack.hpp"

#include "algebra/DenseMatrix.hpp"
#include "algebra/ExactPolynomial.hpp"
#include "algebra/PolyUtils.hpp"
#include "evaluator/Evaluator.hpp"
#include "evaluator/EvaluatorErrors.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <set>
#include <vector>

namespace aleph3::packs {

namespace {

constexpr std::string_view kPackageName = "core-algebra";
constexpr std::size_t kMaxMatrixElements = 4096;

using ExactMatrix = algebra::DenseMatrix<ExactCoefficient>;

[[noreturn]] void throw_matrix_domain(std::string message) {
    kernel::throw_runtime_error(kernel::ErrorCode::domain_violation, std::move(message));
}

ExactCoefficient exact_matrix_scalar(const ExprPtr& expr) {
    if (const auto* rational = std::get_if<Rational>(expr.get())) {
        return ExactCoefficient(rational->numerator, rational->denominator);
    }
    if (const auto* number = std::get_if<Number>(expr.get())) {
        if (std::isfinite(number->value) && std::trunc(number->value) == number->value &&
            number->value >= static_cast<double>(std::numeric_limits<std::int64_t>::min()) &&
            number->value <= static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
            return ExactCoefficient(static_cast<std::int64_t>(number->value), 1);
        }
    }
    kernel::throw_runtime_error(kernel::ErrorCode::unsupported_construct,
        "Matrices currently support exact integer and rational entries only");
}

ExactMatrix exact_matrix_from_expr(const ExprPtr& expr) {
    const auto* outer = std::get_if<List>(expr.get());
    if (!outer || outer->elements.empty()) {
        kernel::throw_runtime_error(kernel::ErrorCode::invalid_form, "Matrix must be a non-empty nested list");
    }
    const auto* first = std::get_if<List>(outer->elements.front().get());
    if (!first || first->elements.empty()) {
        kernel::throw_runtime_error(kernel::ErrorCode::invalid_form, "Matrix rows must be non-empty lists");
    }
    const std::size_t columns = first->elements.size();
    if (outer->elements.size() > kMaxMatrixElements / columns) {
        kernel::throw_runtime_error(kernel::ErrorCode::domain_violation, "Matrix exceeds the 4096-element limit");
    }
    std::vector<ExactCoefficient> values;
    values.reserve(outer->elements.size() * columns);
    for (const auto& row_expr : outer->elements) {
        const auto* row = std::get_if<List>(row_expr.get());
        if (!row || row->elements.size() != columns) {
            kernel::throw_runtime_error(kernel::ErrorCode::invalid_form,
                "Matrix rows must have equal non-zero length");
        }
        for (const auto& value : row->elements) values.push_back(exact_matrix_scalar(value));
    }
    return ExactMatrix(outer->elements.size(), columns, std::move(values));
}

ExprPtr exact_scalar_to_expr(const ExactCoefficient& value) {
    if (value.denominator == 1) return make_expr<Number>(static_cast<double>(value.numerator));
    return make_expr<Rational>(value.numerator, value.denominator);
}

ExprPtr exact_matrix_to_expr(const ExactMatrix& matrix) {
    std::vector<ExprPtr> rows;
    rows.reserve(matrix.rows());
    for (std::size_t row = 0; row < matrix.rows(); ++row) {
        std::vector<ExprPtr> values;
        values.reserve(matrix.columns());
        for (std::size_t column = 0; column < matrix.columns(); ++column) {
            values.push_back(exact_scalar_to_expr(matrix(row, column)));
        }
        rows.push_back(make_expr<List>(List{std::move(values)}));
    }
    return make_expr<List>(List{std::move(rows)});
}

template <typename Operation>
ExprPtr run_matrix_operation(Operation&& operation) {
    try {
        return operation();
    } catch (const std::overflow_error& error) {
        kernel::throw_runtime_error(kernel::ErrorCode::exact_overflow, error.what());
    } catch (const std::domain_error& error) {
        throw_matrix_domain(error.what());
    }
}

std::vector<std::string> dedupe_variables(const std::vector<std::string>& variables) {
    std::vector<std::string> deduped;
    for (const auto& variable : variables) {
        if (std::find(deduped.begin(), deduped.end(), variable) == deduped.end()) {
            deduped.push_back(variable);
        }
    }
    return deduped;
}

std::vector<std::string> extract_variables(const ExprPtr& expr) {
    if (auto sym = std::get_if<Symbol>(&(*expr))) {
        return {sym->name};
    }
    if (auto func = std::get_if<FunctionCall>(&(*expr))) {
        if (func->head == "List") {
            std::vector<std::string> vars;
            for (const auto& item : func->args) {
                if (auto s = std::get_if<Symbol>(&(*item))) {
                    vars.push_back(s->name);
                } else {
                    throw_invalid_form("Variable list must contain only symbols");
                }
            }
            vars = dedupe_variables(vars);
            if (vars.empty()) {
                throw_invalid_form("Variable list must not be empty");
            }
            return vars;
        }
    }
    if (auto list = std::get_if<List>(&(*expr))) {
        std::vector<std::string> vars;
        for (const auto& item : list->elements) {
            if (auto s = std::get_if<Symbol>(&(*item))) {
                vars.push_back(s->name);
            } else {
                throw_invalid_form("Variable list must contain only symbols");
            }
        }
        vars = dedupe_variables(vars);
        if (vars.empty()) {
            throw_invalid_form("Variable list must not be empty");
        }
        return vars;
    }
    throw_invalid_form("Variable argument must be a symbol or list of symbols");
}

std::string extract_single_variable(const ExprPtr& expr) {
    if (auto sym = std::get_if<Symbol>(&(*expr))) {
        return sym->name;
    }
    throw_invalid_form("Variable argument must be a symbol");
}

int extract_non_negative_integer_exponent(const ExprPtr& expr) {
    const auto* number = std::get_if<Number>(expr.get());
    if (!number || !std::isfinite(number->value) ||
        std::trunc(number->value) != number->value ||
        number->value < 0.0 ||
        number->value > static_cast<double>(std::numeric_limits<int>::max())) {
        throw_invalid_form("Coefficient exponent must be a non-negative integer");
    }
    return static_cast<int>(number->value);
}

std::vector<std::string> infer_variables(const ExprPtr& expr) {
    std::set<std::string> vars;
    std::function<void(const ExprPtr&)> visit = [&](const ExprPtr& current) {
        if (!current) {
            return;
        }
        if (auto sym = std::get_if<Symbol>(&(*current))) {
            vars.insert(sym->name);
        } else if (auto call = std::get_if<FunctionCall>(&(*current))) {
            for (const auto& arg : call->args) {
                visit(arg);
            }
        }
    };
    visit(expr);
    return {vars.begin(), vars.end()};
}

std::vector<std::string> infer_variables(const ExprPtr& left, const ExprPtr& right) {
    auto variables = infer_variables(left);
    for (const auto& variable : infer_variables(right)) {
        if (std::find(variables.begin(), variables.end(), variable) == variables.end()) {
            variables.push_back(variable);
        }
    }
    return variables;
}

ExprPtr evaluate_expand(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.args.size() != 1) {
        throw_invalid_arity_exact("Expand", 1);
    }
    return expand_polynomial(func.args[0], ctx);
}

ExprPtr evaluate_factor(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.args.size() != 1) {
        throw_invalid_arity_exact("Factor", 1);
    }
    return factor_polynomial(func.args[0], ctx);
}

ExprPtr evaluate_collect(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.args.size() != 2) {
        throw_invalid_arity_exact("Collect", 2);
    }
    return collect_polynomial(func.args[0], extract_variables(func.args[1]), ctx);
}

ExprPtr evaluate_gcd(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.args.size() != 2 && func.args.size() != 3) {
        throw_invalid_arity_between("GCD", 2, 3);
    }
    const auto variables = func.args.size() == 3
        ? extract_variables(func.args[2])
        : infer_variables(func.args[0], func.args[1]);
    if (func.args.size() == 2 && variables.size() > 1) {
        throw_unsupported_construct(
            "gcd: multivariate GCD requires an explicit variable selector");
    }
    try {
        return gcd_polynomial(func.args[0], func.args[1], variables, ctx);
    } catch (const std::overflow_error& error) {
        kernel::throw_runtime_error(kernel::ErrorCode::exact_overflow, error.what());
    }
}

ExprPtr evaluate_polynomial_quotient(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.args.size() != 2 && func.args.size() != 3) {
        throw_invalid_arity_between("PolynomialQuotient", 2, 3);
    }
    const auto variables = func.args.size() == 3
        ? extract_variables(func.args[2])
        : infer_variables(func.args[0], func.args[1]);
    if (func.args.size() == 2 && variables.size() != 1) {
        throw_unsupported_construct(
            "divide: multivariate division requires an explicit variable selector");
    }
    try {
        const auto result = divide_polynomial(func.args[0], func.args[1], variables, ctx);
        return make_expr<List>(std::vector<ExprPtr>{result.first, result.second});
    } catch (const std::overflow_error& error) {
        kernel::throw_runtime_error(kernel::ErrorCode::exact_overflow, error.what());
    } catch (const std::domain_error& error) {
        kernel::throw_runtime_error(kernel::ErrorCode::division_by_zero, error.what());
    }
}

ExprPtr evaluate_coefficient(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.args.size() != 2 && func.args.size() != 3) {
        throw_invalid_arity_between("Coefficient", 2, 3);
    }
    const auto variable = extract_single_variable(func.args[1]);
    const int exponent = func.args.size() == 3
        ? extract_non_negative_integer_exponent(func.args[2])
        : 1;
    try {
        return coefficient_polynomial(func.args[0], variable, exponent, ctx);
    } catch (const std::overflow_error& error) {
        kernel::throw_runtime_error(kernel::ErrorCode::exact_overflow, error.what());
    }
}

ExprPtr evaluate_coefficient_list(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.args.size() != 2) {
        throw_invalid_arity_exact("CoefficientList", 2);
    }
    const auto variable = extract_single_variable(func.args[1]);
    try {
        return coefficient_list_polynomial(func.args[0], variable, ctx);
    } catch (const std::overflow_error& error) {
        kernel::throw_runtime_error(kernel::ErrorCode::exact_overflow, error.what());
    }
}

ExprPtr evaluate_matrix_add(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.args.size() != 2) throw_invalid_arity_exact("MatrixAdd", 2);
    return run_matrix_operation([&] {
        const auto left = exact_matrix_from_expr(evaluate(func.args[0], ctx));
        const auto right = exact_matrix_from_expr(evaluate(func.args[1], ctx));
        return exact_matrix_to_expr(
            algebra::matrix_add(left, right, [&] { ctx.consume_evaluation_step(); }));
    });
}

ExprPtr evaluate_matrix_multiply(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.args.size() != 2) throw_invalid_arity_exact("MatrixMultiply", 2);
    return run_matrix_operation([&] {
        const auto left = exact_matrix_from_expr(evaluate(func.args[0], ctx));
        const auto right = exact_matrix_from_expr(evaluate(func.args[1], ctx));
        if (right.columns() > kMaxMatrixElements / left.rows()) {
            throw_matrix_domain("Matrix result exceeds the 4096-element limit");
        }
        return exact_matrix_to_expr(algebra::matrix_multiply(
            left, right, [&] { ctx.consume_evaluation_step(); }));
    });
}

ExprPtr evaluate_identity_matrix(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.args.size() != 1) throw_invalid_arity_exact("IdentityMatrix", 1);
    const auto evaluated_size = evaluate(func.args[0], ctx);
    const auto* number = std::get_if<Number>(evaluated_size.get());
    if (!number || !std::isfinite(number->value) || std::trunc(number->value) != number->value ||
        number->value <= 0 || number->value > 64) {
        throw_matrix_domain("IdentityMatrix size must be an integer from 1 through 64");
    }
    return exact_matrix_to_expr(
        algebra::identity_matrix<ExactCoefficient>(static_cast<std::size_t>(number->value)));
}

ExprPtr evaluate_transpose(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.args.size() != 1) throw_invalid_arity_exact("Transpose", 1);
    return exact_matrix_to_expr(
        algebra::matrix_transpose(exact_matrix_from_expr(evaluate(func.args[0], ctx))));
}

ExprPtr evaluate_determinant(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.args.size() != 1) throw_invalid_arity_exact("Det", 1);
    return run_matrix_operation([&] {
        return exact_scalar_to_expr(algebra::determinant(
            exact_matrix_from_expr(evaluate(func.args[0], ctx)),
            [&] { ctx.consume_evaluation_step(); }));
    });
}

ExprPtr evaluate_row_reduce(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.args.size() != 1) throw_invalid_arity_exact("RowReduce", 1);
    return run_matrix_operation([&] {
        return exact_matrix_to_expr(algebra::row_reduce(
            exact_matrix_from_expr(evaluate(func.args[0], ctx)),
            [&] { ctx.consume_evaluation_step(); }));
    });
}

ExprPtr evaluate_linear_solve(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.args.size() != 2) throw_invalid_arity_exact("LinearSolve", 2);
    return run_matrix_operation([&]() -> ExprPtr {
        const auto coefficients = exact_matrix_from_expr(evaluate(func.args[0], ctx));
        if (coefficients.rows() != coefficients.columns()) {
            throw_matrix_domain("LinearSolve requires a square coefficient matrix");
        }
        const auto evaluated_vector = evaluate(func.args[1], ctx);
        const auto* vector = std::get_if<List>(evaluated_vector.get());
        if (!vector || vector->elements.size() != coefficients.rows()) {
            throw_matrix_domain("LinearSolve vector length must match the matrix");
        }
        std::vector<ExactCoefficient> augmented_values;
        augmented_values.reserve(coefficients.rows() * (coefficients.columns() + 1));
        for (std::size_t row = 0; row < coefficients.rows(); ++row) {
            for (std::size_t column = 0; column < coefficients.columns(); ++column) {
                augmented_values.push_back(coefficients(row, column));
            }
            augmented_values.push_back(exact_matrix_scalar(vector->elements[row]));
        }
        auto reduced = algebra::row_reduce(
            ExactMatrix(coefficients.rows(), coefficients.columns() + 1, std::move(augmented_values)),
            [&] { ctx.consume_evaluation_step(); });
        for (std::size_t i = 0; i < coefficients.rows(); ++i) {
            if (!reduced(i, i).is_one()) throw_matrix_domain("LinearSolve requires a unique solution");
        }
        std::vector<ExprPtr> solution;
        solution.reserve(coefficients.rows());
        for (std::size_t row = 0; row < coefficients.rows(); ++row) {
            solution.push_back(exact_scalar_to_expr(reduced(row, coefficients.columns())));
        }
        return make_expr<List>(List{std::move(solution)});
    });
}

}  // namespace

void register_algebra_pack(kernel::FunctionRegistry& registry) {
    registry.register_pack_function(std::string(kPackageName), "MatrixAdd", evaluate_matrix_add,
        "Add two exact dense matrices of equal shape.", true);
    registry.register_pack_function(std::string(kPackageName), "MatrixMultiply", evaluate_matrix_multiply,
        "Multiply two compatible exact dense matrices.", true);
    registry.register_pack_function(std::string(kPackageName), "IdentityMatrix", evaluate_identity_matrix,
        "Construct a bounded exact identity matrix.", true);
    registry.register_pack_function(std::string(kPackageName), "Transpose", evaluate_transpose,
        "Transpose an exact dense matrix.", true);
    registry.register_pack_function(std::string(kPackageName), "Det", evaluate_determinant,
        "Compute an exact dense determinant.", true);
    registry.register_pack_function(std::string(kPackageName), "RowReduce", evaluate_row_reduce,
        "Compute exact reduced row-echelon form.", true);
    registry.register_pack_function(std::string(kPackageName), "LinearSolve", evaluate_linear_solve,
        "Solve a square exact system with one unique solution.", true);
    registry.register_pack_function(
        std::string(kPackageName),
        "Expand",
        evaluate_expand,
        "Expand products and powers in the current polynomial subset.",
        true);
    registry.register_pack_function(
        std::string(kPackageName),
        "Factor",
        evaluate_factor,
        "Factor supported polynomial expressions over the current exact subset.",
        true);
    registry.register_pack_function(
        std::string(kPackageName),
        "Collect",
        evaluate_collect,
        "Collect polynomial terms by one explicit variable selector.",
        true);
    registry.register_pack_function(
        std::string(kPackageName),
        "GCD",
        evaluate_gcd,
        "Compute polynomial GCD for the current supported selector forms.",
        true);
    registry.register_pack_function(
        std::string(kPackageName),
        "PolynomialQuotient",
        evaluate_polynomial_quotient,
        "Return polynomial quotient and remainder for the current supported subset.",
        true);
    registry.register_pack_function(
        std::string(kPackageName),
        "Coefficient",
        evaluate_coefficient,
        "Extract one exact polynomial coefficient in a single selected variable.",
        true);
    registry.register_pack_function(
        std::string(kPackageName),
        "CoefficientList",
        evaluate_coefficient_list,
        "Return exact polynomial coefficients from degree zero upward.",
        true);
}

}  // namespace aleph3::packs
