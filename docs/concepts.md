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
