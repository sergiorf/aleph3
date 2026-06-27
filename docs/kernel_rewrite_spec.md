# Kernel Rewrite Spec

## Status

Initial rewrite infrastructure is now implemented as a structural rule engine
with the first minimal pattern language.

Primary implementation:

- [include/kernel/Rewrite.hpp](/home/sergio/dev/aleph3/include/kernel/Rewrite.hpp)
- [src/kernel/Rewrite.cpp](/home/sergio/dev/aleph3/src/kernel/Rewrite.cpp)

## Purpose

This document defines the first kernel-owned rewrite contract.

It is intentionally smaller than the long-term target. The current goal is to
establish rewrite ownership, traversal semantics, bounded repeated
application, and the first reusable pattern contract.

## What "Rewrite" Means

In Aleph3, a rewrite is a rule-based symbolic transformation.

Example rule:

```text
f[x] -> g[x]
```

Example input:

```text
f[f[x]]
```

Example output after repeated rewriting:

```text
g[g[x]]
```

That is different from ordinary numeric evaluation.

Numeric evaluation asks:

- what is `2 + 3`?

Rewrite asks:

- how should one expression shape be transformed into another expression
  shape?

This matters because symbolic algebra depends heavily on transformations such
as:

- replacing one form with an equivalent form
- distributing or factoring structure
- canonicalizing expressions
- applying user or pack rules without hardcoding every case into evaluator
  branches

## Current Contract

### Rule Scope

The current rewrite engine accepts structural `Rule` expressions and a first
pattern language:

- exact structural rules still work
- symbols ending in `_` act as named pattern binders
- repeated use of the same named binder must match the same expression
- the right-hand side substitutes named binders by bare symbol name

Examples:

- `f[x] -> g[x]`
- `f[a_] -> g[a]`
- `f[a_, a_] -> same[a]`

This is still intentionally small.
There are no typed patterns, predicates, conditions, sequence patterns, or
attribute-aware matcher rules yet.

### Equality Model

`kernel::structurally_equal(...)` compares expressions structurally across:

- atoms
- function calls
- rules
- assignments
- function definitions
- lists

This is the equality contract used by structural matching and by repeated
named-pattern consistency checks.

### Traversal Semantics

`rewrite_once(...)` performs:

- top-level structural or pattern match first
- recursive traversal into child expressions otherwise
- whole-tree reconstruction when any child rewrite happens

`rewrite_repeated(...)` performs repeated application up to a caller-provided
rewrite bound.

`rewrite_repeated(..., EvaluationContext&, max_rewrites)` uses the same
traversal semantics, but each successful rewrite also consumes one kernel
evaluation step through the supplied context.

### Bounded Behavior

Rewrite is bounded in two layers:

- caller provides `max_rewrites` as the local rewrite-loop cap
- traversal stops when no rewrite occurs
- traversal also stops once the bound is reached
- when an `EvaluationContext` is supplied, each successful rewrite consumes one
  step from `policy().budget().max_evaluation_steps`

This intentionally avoids creating a second independent policy budget just for
rewrite.
The current contract is:

- `max_rewrites` bounds a specific rewrite loop
- evaluation-step budget bounds total kernel work when rewrite participates in
  runtime execution

## Scheduling Relative To Evaluation And Normalization

Current scheduling rules are intentionally conservative:

- generic evaluation remains evaluator-owned
- builtin simplification remains evaluator-owned
- rewrite is a caller-directed transformation API, not an implicit global pass
- normalization remains a separate kernel concern

That means rewrite does not automatically:

- normalize before matching
- normalize after substitution
- run after every evaluator reduction

Callers that need canonical ordering should normalize explicitly before or
after rewrite, depending on the intended contract.

Today the general evaluator still does:

1. normalize once at evaluation entry
2. run evaluator-owned dispatch and builtin simplification
3. preserve symbolic fallback when no owner reduces the form

The first rewrite-owned simplification slice is now intentionally narrow:

- binary `Plus` neutral-element identities such as `0 + a -> a`
- binary `Times` neutral and annihilator identities such as `1 * a -> a` and
  `0 * a -> 0`
- basic `Power` identities such as `a^0 -> 1`, `a^1 -> a`, and `1^a -> 1`

For this slice, the scheduling contract is:

- normalize before matching
- apply the rewrite rule
- normalize the rewritten result again

This normalization policy is currently limited to these fixed-arity arithmetic
identity rewrites because it improves canonical matching without changing the
ownership of broader n-ary simplification.

The following still remain evaluator-owned for now:

- list broadcasting and other container-aware simplifications
- numeric-domain and special-value handling

## Decision On Broader Arithmetic Simplification

Broader arithmetic simplification should not wait for general sequence
patterns.

The next rewrite expansion should introduce a dedicated n-ary arithmetic
rewrite contract for canonical `Plus` and `Times` forms.

This decision is intentional for three reasons:

- sequence patterns would widen the general matcher surface significantly and
  pull in a larger semantic program than the current arithmetic migration
  needs
- `Plus` and `Times` already have kernel-known `Flat` and `Orderless`
  semantics, so they can use a narrower contract than a full sequence-pattern
  language
- waiting for general sequence patterns would keep too much arithmetic cleanup
  trapped in evaluator-local code even though the next safe reductions are
  already well understood

### N-ary Arithmetic Rewrite Contract

The current rewrite-owned arithmetic slice now operates on already-normalized
`Plus` and `Times` expressions and treat them as variadic arithmetic forms,
not as arbitrary pattern-matched trees.

The intended contract is:

- input is a normalized `Plus[...]` or `Times[...]`
- the rewrite step may inspect the whole argument list for that head
- the rewrite step may rebuild a new argument list for the same head
- the rebuilt result is normalized again before it leaves the rewrite-owned
  arithmetic path
- each successful reduction remains budgeted through the same rewrite/eval
  budgeting rules already defined above

The currently implemented reductions are:

- variadic neutral-element elimination for `Plus` and `Times`
- variadic annihilator handling for scalar `Times`
- scalar numeric bucket folding for `Plus` and `Times`
- exact rational bucket folding where the current exact-rational behavior is
  already stable

This is not the same thing as sequence patterns.
It is a head-aware variadic reduction contract for a small number of arithmetic
heads whose algebraic flattening and ordering rules are already known.

### Safe Arithmetic Migrations After The Fixed-arity Identity Slice

The first evaluator-owned simplifications chosen for migration were:

1. variadic neutral-element elimination for `Plus` and `Times`
   Examples:
   - `x + 0 + y -> x + y`
   - `1 * x * y -> x * y`
2. variadic annihilator handling for `Times`
   Examples:
   - `x * 0 * y -> 0`
   - this remains gated to scalar arithmetic forms, not list-aware broadcast
     paths
3. numeric bucket folding for `Plus` and `Times`
   Examples:
   - `2 + x + 3 -> x + 5`
   - `2 * x * 3 -> 6 * x`
4. exact numeric bucket folding where the existing exact-rational contract is
   already stable
   Examples:
   - `1/2 + x + 1/3 -> x + 5/6`
   - `2 * 1/3 * x -> 2/3 * x`

### Simplifications That Should Stay Evaluator-owned For Now

The following should not be migrated in the next rewrite slice:

- like-term combination such as `2*x + 3*x -> 5*x`
- division simplifications such as `x/x -> 1` or `0/x -> 0`
- power-domain-sensitive behavior beyond the current fixed identities
- list broadcasting and elementwise arithmetic
- special-function reductions such as `Gamma` shortcuts

These are deferred because they either:

- depend on a stronger exact-algebra foundation
- depend on richer matcher power than the current minimal pattern language
- depend on container semantics that are not just scalar rewrites
- or carry domain/error behavior that should remain explicit before migration

## Decision On Like-term Collection

Like-term collection should not wait for the full exact-algebra program, but it
also should not be folded into the current arithmetic rewrite contract.

The long-term direction should be a separate symbolic coefficient contract.

That is the better split because like-term collection is not only "more `Plus`
cleanup". It requires explicit reasoning about:

- symbolic monomials
- coefficient extraction
- coefficient recombination
- canonical comparison between term bases

Those concepts are richer than the current n-ary arithmetic rewrite contract,
but narrower than the full algebra-pack and exact-polynomial program.

### Planned Symbolic Coefficient Contract

The next contract above arithmetic rewrite should make the following concepts
explicit:

- scalar coefficient
- symbolic basis term
- canonical monomial key for supported forms
- additive accumulation over matching basis terms

For the near term, that contract only needs to cover the currently supported
surface already implied by evaluator behavior, such as:

- `x`
- `c * x` where `c` is numeric or exact rational
- `x^n`
- `c * x^n`

It should not initially require:

- multivariate polynomial normalization
- symbolic coefficient domains
- distributive expansion across arbitrary products
- pack-level algebra metadata

The currently implemented surface now covers:

- `x`
- `x^n` for numeric exponents already preserved by current supported semantics
- `c * x`
- `c * x^n`

Where `c` may be:

- `Number`
- `Rational`

And where collection is limited to normalized `Plus` forms.

### Why This Should Be Separate From Arithmetic Rewrite

Arithmetic rewrite for `Plus` and `Times` currently owns:

- neutral elimination
- annihilator handling
- numeric and exact-rational bucket folding

Like-term collection requires a different abstraction boundary:

- identify "same basis term"
- extract coefficient from each compatible term
- combine coefficients
- rebuild canonical term form

That is a symbolic coefficient problem, not just a variadic arithmetic-head
problem.

### Current Small Coefficient Layer

This contract is now implemented in initial form as a separate kernel-owned
reduction step above arithmetic rewrite.

It currently supports:

- combining `x + 2*x + 1/3*x` into `10/3 * x`
- combining `x^2 + 2*x^2` into `3 * x^2`
- cancelling `x + (-1 * x)` into `0`

It intentionally does not yet support:

- `x*y + 2*x*y`
- `x*y^2 + 3*x*y^2`
- symbolic coefficient domains
- basis extraction beyond single-symbol or single-power terms

## Decision On Exponent Merging

Exponent merging should not move into the arithmetic rewrite layer.
It belongs in the separate algebra-aware layer.

Examples of the deferred behavior include:

- `x * x -> x^2`
- `x * x^2 -> x^3`
- `(x^2)^3 -> x^6`

### Why Exponent Merging Should Be Deferred

Exponent merging depends on more than arithmetic bucket folding:

- identifying compatible power bases
- reasoning about exponent domains
- preserving canonical form across nested power/product interactions
- avoiding accidental extension into unsupported algebraic identities

The current arithmetic rewrite contract is intentionally head-local and
coefficient-light. Exponent merging crosses into algebra structure.

### Planned Ownership Boundary

The intended long-term split should be:

- arithmetic rewrite layer:
  numeric and exact-rational accumulation for normalized `Plus`/`Times`
- symbolic coefficient layer:
  like-term collection for supported coefficient/basis shapes
- algebra-aware layer:
  exponent merging, power flattening beyond fixed identities, and other
  structure-sensitive multiplicative rewrites

That gives Aleph3 three cleaner stages instead of one overloaded simplifier.

### Current Small Algebra-aware Layer

That algebra-aware layer now exists in initial form.

Its currently implemented surface is intentionally small:

- `x * x -> x^2`
- `x * x^2 -> x^3`
- `(x^2)^3 -> x^6`

Current constraints:

- only normalized `Times` and `Power` forms participate
- basis matching is limited to the same single symbol basis already supported
  by current evaluator semantics
- nested power collapse only runs when both exponents are numeric
- division cancellation is explicitly out of scope
- power-domain-sensitive transformations are explicitly out of scope
- list-aware arithmetic is explicitly out of scope

General rewrite integration beyond explicit callers is still future work and
should only happen where the scheduling contract is precise enough to avoid
hidden semantic drift.

## What This Enables Now

The rewrite subsystem can already support:

- exact local transformation tests
- first pattern-based substitution tests
- rule-driven structural cleanup experiments
- migration of selected hardcoded transforms toward kernel-owned rewrite APIs
- the first evaluator-facing identity-simplification slice for fixed-arity
  arithmetic forms

Example of what works now:

- rule: `f[x] -> g[x]`
- input: `f[f[x]]`
- repeated rewrite output: `g[g[x]]`

## What Is Not Implemented Yet

- conditional rules
- typed or predicate-based patterns
- sequence patterns
- rewrite strategy selection
- integration with assumptions
- broad evaluator/rewrite scheduling integration

## Next Steps

- decide whether the symbolic coefficient contract should remain limited to
  single-symbol and single-power bases until stronger exact algebra exists
- decide whether the algebra-aware layer should remain limited to same-symbol
  exponent accumulation until stronger exact algebra exists
- decide which non-arithmetic simplifications are good candidates for future
  rewrite-owned migration
