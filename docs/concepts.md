# Concepts and Terminology

This guide explains Aleph3 vocabulary in plain language. The examples use the
project's Wolfram-like surface syntax, but the concepts are independent of a
particular parser.

## Formula, Expression, and Value

A **formula** is source text submitted through the SDK, such as:

```text
If[temperature > limit, "alarm", "ok"]
```

An **expression** is the kernel's structured representation of meaning. The
kernel represents the same formula as nested heads and arguments, roughly:

```text
If[Greater[temperature, limit], "alarm", "ok"]
```

A **value** is a result the SDK can return to a host application: a number,
boolean, string, or another supported public type. A symbolic expression is
not automatically an SDK value; `x + 1` may remain symbolic if `x` has no
value.

## Head and Argument

A function-shaped expression has a **head** and zero or more **arguments**.
In `Clamp[x, 0, 10]`, `Clamp` is the head and `x`, `0`, and `10` are arguments.
Arithmetic is represented the same way: `x + 2` has the normalized shape
`Plus[x, 2]`.

This uniform shape matters because matching, evaluation, and registration can
reason about all calls through the same model.

## Kernel

The **kernel** is the semantic center: it decides what expressions mean. It
owns evaluation, symbols, definitions, rewriting, normalization, exact
arithmetic, and assumptions.

Example: the fact that exact `1/2 + 1/3` produces `5/6` is a kernel concern.
Whether a host application permits division at all is an SDK policy concern.

## Notebook, Document, Cell, and Display

The planned **notebook** is the local desktop product built above the session.
A **notebook document** is an ordered collection of cells plus format metadata.
It is product data, not a second expression representation.

An **input cell** stores Aleph3 source text. A **text cell** stores explanatory
content. An **output cell** records presentation associated with an evaluation,
but its cached rendering is not semantic truth: re-evaluation always goes
through the session and kernel.

A **display node** is a presentation-oriented result such as plain text,
structured mathematics, a diagnostic, or later a plot. Display nodes describe
how a result may be shown; they do not evaluate expressions.

## Session

A **session** owns interactive execution state across requests, including user
definitions and the kernel evaluation context. The CLI already uses this
boundary. The planned notebook will create or restore a session and submit cell
requests through it rather than embedding evaluator state in widgets.

Closing a document and saving a document are product operations. Preserving
kernel state across a reopen requires an explicit replay or serialization
contract; it must never happen accidentally through hidden process state.

## SDK and Trusted Subset

The **SDK** is the stable API used to embed Aleph3. Its **trusted subset** is a
deliberately limited formula language that can be checked before execution.

For example, a host may declare only `price` and `tax` in its schema. The SDK
accepts `price * (1 + tax)` and rejects `price * secretRate` before runtime.
The kernel supplies arithmetic semantics; the SDK supplies the boundary of
what this application trusts.

## Schema and Policy

A **schema** describes names and types available to a formula. A **policy**
describes permitted operations and resource limits.

```text
schema:  x is Number, label is String
policy:  allow If and Clamp; maximum expression depth 32
formula: If[x > 10, label, "small"]
```

The schema answers “does this name exist, and what kind of value is it?” The
policy answers “is this operation allowed, and within what budget?”

## IR, AST, and Lowering

An **AST** (abstract syntax tree) records parsed structure. Aleph3's SDK-side
**IR** (intermediate representation), `ir::Node`, is an AST enriched for
validation and source diagnostics. It is intentionally temporary.

**Lowering** translates a validated SDK node into the kernel's `Expr` form:

```text
SDK subtraction node:  x - 2
kernel expression:      Plus[x, Times[-1, 2]]
```

“Lower” does not mean less capable. It means moving from syntax-oriented
structure to the canonical structure consumed by the execution layer.

## Evaluation

**Evaluation** resolves an expression according to values, definitions,
attributes, and registered functions.

```text
input:  Plus[2, 3]
output: 5
```

If no applicable meaning is known, symbolic evaluation can preserve the
expression:

```text
input:  Mystery[x]
output: Mystery[x]
```

This symbolic fallback is why the kernel is not merely a numeric calculator.

## Normalization

**Normalization** gives equivalent expression structures a predictable
canonical shape. For example, nested addition may be flattened and terms may
be placed in a deterministic order.

Normalization is not “make this as simple as a human would.” Its goal is
stable structure. That stability makes equality checks, caching, and rewrite
matching reliable.

## Rewrite, Rule, Pattern, and Match

A **rule** describes a structural replacement. A **pattern** describes which
structures it accepts. A **match** binds pattern names to actual subexpressions.

```text
rule:        f[a_] -> g[a]
expression:  f[x]
binding:     a = x
result:      g[x]
```

A **rewrite** is the act of applying such a rule. Rewriting differs from
evaluation: `2 + 3 -> 5` computes a built-in meaning, while `f[a_] -> g[a]`
performs a caller-directed structural transformation.

Rewrites are bounded because a rule such as `a_ -> f[a]` could otherwise run
forever.

### Replacement Depth

The root expression is depth `0`; its arguments are depth `1`; their arguments
are depth `2`, and so on. Function head names are not counted as children.

```text
expression: f[f[x], x]

Replace[f[f[x], x], x -> y, 1]       -> f[f[x], y]
Replace[f[f[x], x], x -> y, 2]       -> f[f[y], x]
Replace[f[f[x], x], x -> y, {1, 2}]  -> f[f[y], y]
```

The last argument is either one exact depth or an inclusive `{min, max}`
range. Omitting it preserves the existing whole-expression traversal. Aleph3
currently supports nonnegative depths only; negative levels and traversal of
head names are intentionally outside this contract.

A **conditional pattern** adds a predicate after structural matching. Bindings
are substituted into the predicate, which is evaluated with the same session
budget as the rewrite:

```text
Condition[n_Integer, Positive[n]]
Replace[3, Condition[n_Integer, Positive[n]] -> g[n]]  -> g[3]
```

Only an exact `True` accepts the match. `False` and unresolved predicates do
not match. Sequence and nested conditional patterns remain unsupported.

## Definition and Attribute

A **definition** attaches behavior or a value to a symbol. An **attribute**
changes how evaluation treats a head and its arguments.

For example, an `If`-like head must avoid evaluating both branches before it
knows the condition; an evaluation-control attribute can express that rule.
Attributes affect scheduling, while definitions provide results.

## Exact Arithmetic

**Exact arithmetic** preserves mathematical values without rounding when the
supported representation allows it. `1/3` remains a rational value rather
than becoming an approximate binary floating-point number.

```text
exact:       1/3 + 1/6  -> 1/2
approximate: 0.333... + 0.166... (subject to rounding)
```

Exactness is a contract, not a claim that every mathematical object is already
supported. The algebra specifications state the current boundary.

## Polynomial Vocabulary

A **polynomial** is a sum of terms made from coefficients and variables raised
to nonnegative integer powers:

```text
3*x^2*y + 1/2*y - 4
```

In this expression:

- `3`, `1/2`, and `-4` are **coefficients**
- `x^2*y` and `y` are **monomials**, the variable-and-exponent parts
- `3*x^2*y`, `1/2*y`, and `-4` are **terms**
- the polynomial is **multivariate** because it contains more than one
  variable (`x` and `y`)

A univariate polynomial uses one variable, such as `x^3 - 2*x + 1`.
Multivariate does not mean “multiple equations”; it means one polynomial whose
terms may involve multiple variables.

### Canonical Monomial Order

A multivariate polynomial needs a deterministic rule for deciding which term
comes first. Aleph3's current canonical order compares:

1. total degree, the sum of a monomial's exponents
2. variable exponents in the declared precedence order

For example, both `x^2*y` and `x*y^2` have total degree three. With precedence
`{x, y}`, `x^2*y` comes first because it has the larger exponent of `x`. This
policy is called **graded lexicographic order**.

The order is not cosmetic. Polynomial division repeatedly works with the
leading term, so changing variable precedence can change the quotient and
remainder while preserving the same reconstruction identity.

### Multivariate Polynomial Division

`PolynomialQuotient[dividend, divisor, {x, y}]` performs exact division by one
divisor. `{x, y}` declares that `x` has precedence over `y`.

```text
PolynomialQuotient[x^2*y + x*y^2 + y, x*y, {x, y}]
    -> {x + y, y}
```

The two outputs are `{quotient, remainder}` and satisfy:

```text
dividend = divisor * quotient + remainder

x^2*y + x*y^2 + y = x*y * (x + y) + y
```

The remainder is reduced: none of its terms can be divided by the divisor's
leading monomial under the selected order. This is the multivariate analogue
of integer division with a remainder, although the ordering of monomials now
matters.

Aleph3 currently supports this operation only for exact integer or rational
coefficients and an explicit variable list. Floating-point multivariate
division, multiple divisors, multivariate GCD, and broad multivariate
factorization remain outside the supported subset. See the
[algebra contract](algebra_supported_subset.md) for the precise boundary.

## Assumption

An **assumption** is contextual knowledge used to justify a transformation.
Without assumptions, `Sqrt[x^2]` cannot generally become `x`; for negative
real `x`, the result is `-x`. With `x >= 0`, that simplification is valid.

```text
Refine[Sqrt[x^2], x >= 0]  -> x
```

Assumptions never license a transformation that is merely convenient. The
kernel or a pack must be able to justify it from supported facts.

## Pack

A **pack** is a domain library built on kernel contracts. It contributes
registered functions, rules, or algorithms without changing the evaluator's
architecture.

Examples include the current algebra pack and possible future calculus,
solver, or special-function packs. The kernel provides pattern matching; a
calculus pack provides differentiation knowledge.

The interactive session can inspect an expression without evaluating or
storing it, discover packs, and complete registered or session-defined symbols.
The CLI exposes these shared operations as `:inspect <expression>`, `:packs`,
and `:complete <prefix>`.

## Built-in, Host Function, and Registered Function

A **built-in** is implemented by Aleph3. A **host function** is implemented by
the embedding application. Both use registration contracts so the engine can
check names, arity, types, and runtime behavior.

```text
host registers: PriceForSku[String] -> Number
formula calls:  PriceForSku["ABC-123"] * quantity
```

Registration is engine-scoped: one application's functions do not silently
appear in another engine instance.

## Diagnostic and Runtime Error

A **diagnostic** explains a problem found while parsing or validating and can
point to source text. A **runtime error** reports a failure that occurs with
runtime inputs, such as division by zero or a host callback returning the
wrong type.

Keeping them structured lets applications display errors without scraping
human-readable strings.

## Budget

A **budget** bounds work: evaluation steps, rewrite passes, expression size,
or related resources. It turns accidental or adversarial nontermination into
a controlled error.

Budgets are part of safe embedding, not a performance afterthought. A formula
can be mathematically valid and still exceed the work a host permits.

## Symbolic Fallback

**Symbolic fallback** preserves a well-formed expression when no concrete
result is available. It allows later definitions, assumptions, or packs to
make progress without treating every unknown as an error.

The trusted SDK may reject some unknown names earlier because its contract is
deliberately narrower. That is a useful example of the difference between
kernel semantics and SDK policy.
