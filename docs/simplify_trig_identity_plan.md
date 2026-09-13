# Simplify Trigonometric Identity Plan

## Status And Scope

This plan records the implementation direction for the first
Mathematica-inspired simplification slice around standard trigonometric
identities. It is not current behavior. When implemented, durable behavior
belongs in the kernel rewrite and simplification specifications, the algebra
supported subset where relevant, the manual, help text, and tests.

The work is a substantial feature under the
[Feature Development Workflow](feature_development_workflow.md) because it
changes public symbolic behavior and adds a simplification strategy boundary.
Complete repository research and design approval are required before
implementation changes.

The core invariant is:

```text
Cos[x]^2 + Sin[x]^2
```

continues to preserve its symbolic form during ordinary evaluation, while:

```text
Simplify[Cos[x]^2 + Sin[x]^2]
```

returns:

```text
1
```

## Goal

Establish a kernel-owned `Simplify` pass that can apply a small, deterministic
set of mathematically valid simplifications when the user explicitly requests
them, while preserving conservative ordinary evaluation.

Observable success:

- ordinary evaluation and canonicalization remain cheap, deterministic, and
  conservative;
- `Simplify[expr]` recursively simplifies children before considering
  whole-expression identities;
- `Simplify` applies the globally valid Pythagorean identity
  `Sin[arg]^2 + Cos[arg]^2 -> 1` when both arguments are structurally equal
  after ordinary canonicalization;
- term order does not affect the identity;
- different arguments such as `Sin[x]^2 + Cos[y]^2` do not simplify to `1`;
- repeated simplification reaches a stable expression;
- the design leaves room for broader `FullSimplify` search and explicit
  trigonometric transformations without implementing them in this slice.

## Roadmap Alignment

Unified Plan workstreams:

- Exact Algebra Depth: continue growing symbolic simplification only through
  explicit kernel and algebra contracts.
- CAS Engine Roadmap Gap Closure: broaden CAS behavior incrementally while
  keeping domain assumptions, branch-sensitive identities, diagnostics, and
  unsupported behavior explicit.
- Rewrite and Assumptions: keep rewrite scheduling, normalized-head
  simplification, and future assumption-aware rules distinct.

Owner:

- The kernel owns expression meaning, ordinary evaluation, canonicalization,
  simplification strategy, structural equality, budgets, and diagnostics.
- Packs may later register domain simplifiers through shared contracts, but
  this initial trig identity should not create private CLI, session, SDK,
  notebook, or pack-local semantics.
- CLI, session, notebook, and SDK consumers reuse the shared kernel behavior.

This work is not part of the local notebook MVP critical path. It may proceed
as allowed parallel CAS-engine work only after the design confirms it does not
displace higher-priority session, notebook, or bounded algebra tasks.

## Repository Research Checklist

Before editing implementation files, inspect and record the current evidence
for:

- parser and AST lowering for infix `+`, `*`, and `^`;
- `Expr` node types and function-call representation;
- builtin and pack function dispatch;
- current evaluation and canonicalization pipeline;
- existing `Simplify` behavior, if any;
- arithmetic normalization for `Plus`, `Times`, and `Power`;
- function expressions such as `Sin[x]`, `Cos[x]`, and unknown `f[x]`;
- structural equality, hashing, and ordering APIs;
- rewrite and normalized-head simplification infrastructure;
- assumptions infrastructure and the current absence or presence of
  assumption-aware simplification hooks;
- regression, unit, session, CLI, and help/completion tests.

Likely starting documents:

- [Kernel Rewrite Spec](kernel_rewrite_spec.md)
- [Kernel Design Spec](kernel_design_spec.md)
- [Algebra Supported Subset](algebra_supported_subset.md)
- [Assumptions Spec](kernel_assumptions_spec.md)
- [Expressions And Evaluation](manual/expressions-and-evaluation.md)
- [Built-In Functions](manual/built-in-functions.md)

Do not introduce a parallel symbolic representation, evaluator, or matcher
when existing Aleph3 contracts can be extended.

## Design

Aleph3 should keep three concepts separate.

### Evaluation And Canonicalization

Ordinary evaluation owns cheap, terminating, deterministic reductions and
canonical forms:

```text
2 + 3      -> 5
x + 0      -> x
x * 1      -> x
```

It may continue to perform existing arithmetic normalization and like-term
collection where already supported. It must not automatically apply
mathematically nontrivial trigonometric identities such as:

```text
Sin[x]^2 + Cos[x]^2 -> 1
```

### Simplify

`Simplify[expr]` is an explicit request to search for a mathematically simpler
equivalent expression. The first implementation should remain controlled:

1. recursively simplify child expressions;
2. canonicalize using existing kernel normalization;
3. consider applicable safe simplification rules;
4. accept a candidate only when expression complexity strictly decreases;
5. repeat to a stable result under a fixed iteration or budget bound.

This is not an unrestricted recursive rewrite engine.

### FullSimplify And Trig Transformations

`FullSimplify[expr]` is reserved for a future broader search strategy.
Implement only minimal shared infrastructure that `Simplify` truly needs.

Explicit trigonometric transformations such as:

```text
TrigExpand[expr]
TrigFactor[expr]
TrigReduce[expr]
```

remain conceptually separate. They may intentionally produce larger or
different representations in a later tranche.

## Expression Complexity

Introduce or reuse an isolated complexity metric, conceptually:

```cpp
ExpressionComplexity complexity(const Expr&);
```

The first metric can be intentionally small. It should primarily count AST
nodes and secondarily account for depth or expensive constructs. Atoms have
low cost; function calls, `Power`, `Times`, and `Plus` add cost through their
children.

The metric is a simplification policy tool, not a mathematical proof. It must
be isolated so later work can refine it without changing every rule.

Acceptance rule:

- only replace an expression with a candidate when the candidate has strictly
  lower complexity after canonicalization;
- preserve the current expression on equal complexity.

## First Trigonometric Rule

Add a structural, non-string rule for the globally valid identity:

```text
Sin[arg]^2 + Cos[arg]^2 -> 1
```

The rule should identify:

```text
Power(Sin(arg), 2)
Power(Cos(arg), 2)
```

inside a normalized `Plus` form using Aleph3's actual expression APIs.
Arguments must match by structural equality after ordinary canonicalization.
Term order must not matter.

Required examples:

```text
Simplify[Sin[x]^2 + Cos[x]^2]             -> 1
Simplify[Cos[x]^2 + Sin[x]^2]             -> 1
Simplify[Sin[2*x]^2 + Cos[2*x]^2]         -> 1
Simplify[Sin[x + y]^2 + Cos[x + y]^2]     -> 1
Simplify[Sin[f[x]]^2 + Cos[f[x]]^2]       -> 1
Simplify[Sin[x]^2 + Cos[y]^2]             -> preserved, not 1
```

If the current function-call model or parser does not support an example such
as `f[x]`, record that as an existing syntax limitation rather than forcing a
new function-call feature into this slice.

## Coefficients

Support common factors only if existing term decomposition makes it robust:

```text
Simplify[a*Sin[x]^2 + a*Cos[x]^2]         -> a
Simplify[3*Sin[x]^2 + 3*Cos[x]^2]         -> 3
Simplify[a + b*Sin[x]^2 + b*Cos[x]^2]     -> a + b
```

Do not add brittle AST-position logic solely for these examples. If symbolic
coefficient extraction is not strong enough, implement the basic identity and
name coefficient-aware trig matching as a follow-up.

## Deferred Identities

Do not implement these unless the approved design shows they fit cleanly and
strictly reduce complexity:

```text
1 - Sin[x]^2 -> Cos[x]^2
1 - Cos[x]^2 -> Sin[x]^2
```

Do not add identities whose validity depends on assumptions or branch
conditions, such as:

```text
Sqrt[x^2] -> x
Log[x*y] -> Log[x] + Log[y]
```

Assumption-aware simplification remains a future contract.

## Implementation Slices

### 1. Research And Design Approval

Confirm current repository structure and write the design decision before
repo-tracked implementation changes.

Completion criteria:

- exact implementation entry points are identified;
- the plan is reconciled with current `Simplify` behavior, if any;
- public examples, non-goals, and unsupported cases are approved;
- ownership remains in the kernel and shared registry paths.

### 2. Complexity Metric

Add or reuse a small expression complexity helper in the kernel.

Completion criteria:

- unit tests cover atoms, calls, `Plus`, `Times`, `Power`, and nested
  expressions;
- the helper has no dependency on rendering or source text;
- equal-complexity candidates are not preferred by default.

### 3. Simplify Driver

Implement or extend `Simplify[expr]` so it recursively simplifies children,
canonicalizes, and applies accepted simplification candidates to a stable
result under a fixed bound.

Completion criteria:

- existing arithmetic examples still simplify as currently documented;
- repeated `Simplify[Simplify[expr]]` is stable;
- termination is protected by strict complexity decrease plus a fixed-point or
  iteration bound.

### 4. Trigonometric Simplifier

Add the Pythagorean trig rule in a focused kernel simplification component,
for example a trigonometric simplifier called from the `Simplify` pipeline.

Completion criteria:

- matching is structural and order-independent;
- arguments are compared with existing structural equality APIs;
- ordinary evaluation of `Cos[x]^2 + Sin[x]^2` remains unchanged;
- the rule is not embedded as a one-off string or printer comparison.

### 5. Documentation And Discovery

Update current-behavior documentation in the same implementation change.

Likely locations:

- `docs/manual/expressions-and-evaluation.md`
- `docs/manual/built-in-functions.md`
- `docs/manual/concepts-and-terminology.md` if new simplification vocabulary
  is introduced
- `docs/kernel_rewrite_spec.md` or a focused kernel simplification spec if the
  durable simplification contract needs a new home
- help/completion metadata for `Simplify` if user-facing metadata changes

Completion criteria:

- docs explain evaluation versus `Simplify` versus future `FullSimplify`;
- docs state that `TrigExpand`, `TrigFactor`, and `TrigReduce` are separate
  future explicit transformations;
- examples are covered by tests or verified against the executable;
- docs do not claim broad Mathematica compatibility.

### 6. Verification And Diff Review

Run the feature workflow gates.

Completion criteria:

- baseline failures, if any, were recorded before implementation;
- focused simplification tests pass;
- affected evaluator, kernel, session, CLI, and help tests pass where relevant;
- full existing test suite passes when feasible;
- `git diff --check` passes;
- final diff has no duplicated symbolic semantics in CLI, session, SDK,
  notebook, or pack layers.

## Tests

Add regression tests in the project's current style for:

- ordinary evaluation preserving `Cos[x]^2 + Sin[x]^2`;
- `Simplify[Cos[x]^2 + Sin[x]^2] -> 1`;
- `Simplify[Sin[x]^2 + Cos[x]^2] -> 1`;
- common arguments such as `2*x`, `x + y`, and supported call expressions;
- different arguments not simplifying to `1`;
- recursive simplification:

```text
Simplify[(Sin[x]^2 + Cos[x]^2)^2] -> 1
```

- existing arithmetic simplification examples such as:

```text
Simplify[x + 0]
Simplify[x * 1]
Simplify[2 + 3]
```

- no rewrite loop:

```text
Simplify[Simplify[Sin[x]^2 + Cos[x]^2]] -> 1
```

If coefficient-aware matching is implemented, add tests for numeric and
symbolic common factors. If it is deferred, add no misleading expected-output
tests for it.

## Manual REPL Regression Set

After implementation, these are the manual smoke checks:

```text
Cos[x]^2 + Sin[x]^2
Simplify[Cos[x]^2 + Sin[x]^2]
Simplify[Sin[x]^2 + Cos[x]^2]
Simplify[Sin[2*x]^2 + Cos[2*x]^2]
Simplify[Sin[x+1]^2 + Cos[x+1]^2]
Simplify[Sin[x]^2 + Cos[y]^2]
Simplify[(Sin[x]^2 + Cos[x]^2)^2]
Simplify[Simplify[Sin[x]^2 + Cos[x]^2]]
```

Expected conceptual results:

```text
(Cos[x])^2 + (Sin[x])^2
1
1
1
1
Sin[x]^2 + Cos[y]^2
1
1
```

Printer ordering and parentheses may follow the existing renderer.

If coefficient-aware matching is implemented:

```text
Simplify[3*Sin[x]^2 + 3*Cos[x]^2] -> 3
```

## Completion Report Requirements

The final implementation report should include:

1. modified files;
2. components reused and components added;
3. resulting simplification architecture;
4. exact location of trig identities;
5. termination guarantee;
6. expression complexity metric;
7. automated tests added or changed;
8. focused and full-suite verification results;
9. copy-paste REPL regression set;
10. three sensible next extensions.

Suggested future extensions:

- coefficient-aware trig identity matching;
- directional complement identities such as `1 - Sin[x]^2 -> Cos[x]^2`;
- `Tan[x]` identities when they reduce total complexity;
- explicit `TrigExpand`, `TrigFactor`, and `TrigReduce`;
- assumption-aware simplification;
- broader `FullSimplify` search strategies.
