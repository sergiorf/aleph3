# Kernel Rewrite Spec

## Status

Initial rewrite infrastructure is now implemented as a structural rule engine
with the first minimal pattern language.

Primary implementation:

- [`include/kernel/Rewrite.hpp`](../include/kernel/Rewrite.hpp)
- [`src/kernel/Rewrite.cpp`](../src/kernel/Rewrite.cpp)

## Purpose

This document defines the first kernel-owned rewrite contract.

It is intentionally smaller than the long-term target. The current goal is to
establish rewrite ownership, traversal semantics, bounded repeated
application, and the first reusable pattern contract.

Plain-language summary:

- rewrite means "change this expression shape into that expression shape"
- it is used for symbolic manipulation, not just numeric calculation
- Aleph3 uses rewrite for things like structural cleanup and a small supported
  simplification layer

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

Practical examples:

- `0 + x -> x`
- `1 * x -> x`
- `f[a_] -> g[a]`
- later, a future calculus pack could express rules such as
  `D[x^2, x] -> 2*x`

Practical user-facing workflows now exposed on top of this kernel surface:

- `Replace[f[x], f[a_] -> g[a]] -> g[x]`
- `ReplaceAll[f[x], f[a_] -> g[a]] -> g[x]`
- `f[x] /. x -> y -> f[y]`
- `ReplaceRepeated[f[f[x]], f[a_] -> g[a]] -> g[g[x]]`
- `MatchQ[f[x, x], f[a_, a_]] -> True`

## Current Contract

### Rule Scope

The current rewrite engine accepts structural `Rule` expressions and a first
pattern language:

- exact structural rules still work
- symbols ending in `_` act as named pattern binders
- `_Integer`, `_Rational`, `_Real`, `_Symbol`, `_String`, `_Boolean`, and
  `_Function` constrain one anonymous match by expression kind
- forms such as `n_Integer` apply the same constraint and bind the match to `n`
- repeated use of the same named binder must match the same expression
- the right-hand side substitutes named binders by bare symbol name

Examples:

- `f[x] -> g[x]`
- `f[a_] -> g[a]`
- `f[a_, a_] -> same[a]`

This is still intentionally small. The implemented conditional surface is
limited to current `Condition[...]` predicates. There are no predicate
patterns, sequence patterns, nested conditional patterns, or attribute-aware
matcher rules yet. Unknown type constraints and sequence forms are rejected
explicitly.

In plain terms:

- `a_` means "match any one expression and remember it as `a`"
- `f[a_, a_]` means both inputs must match the same expression
- `n_Integer` means "match one integer and remember it as `n`"
- there is not yet a way to say "match any number of arguments"

Current product-facing transformation surface:

- `Replace[expr, rule]` applies one rewrite traversal
- `Replace[expr, rule, n]` targets exactly nonnegative depth `n`
- `Replace[expr, rule, {min, max}]` targets an inclusive nonnegative depth range
- `ReplaceAll[expr, rule]` applies the same whole-expression traversal as
  `Replace[expr, rule]`
- `expr /. rule` lowers to `ReplaceAll[expr, rule]` in the symbolic frontend
- `ReplaceRepeated[expr, rule]` re-applies a rule with an explicit safety cap
- `ReplaceRepeated` accepts the same optional depth controls
- `MatchQ[expr, pattern]` tests the same supported matcher surface without
  rewriting

Still intentionally unsupported:

- rule lists
- nested conditional patterns
- sequence patterns
- predicate-based patterns
- hold-sensitive pattern evaluation

### Equality Model

Rewrite uses the expression-owned structural identity APIs. The compatibility
entrypoint `kernel::structurally_equal(...)` delegates to `aleph3::structural_equal(...)`;
there must not be a second recursive equality implementation in rewrite.

Structural equality compares expressions structurally across:

- atoms
- function calls
- rules
- assignments
- function definitions
- lists

This is the equality contract used by structural matching and by repeated
named-pattern consistency checks. It is not mathematical equivalence, and it
does not rely on rendered expression text. Rewrite buckets and deterministic
ordering should use `ExprHash`, `ExprEqual`, or `ExprStructuralLess` when they
need structural identity, hashing, or ordering.

### Traversal Semantics

`rewrite_once(...)` performs:

- top-level structural or pattern match first
- recursive traversal into child expressions otherwise
- whole-tree reconstruction when any child rewrite happens

The optional `RewriteTraversal` contract defines root depth as zero and counts
function arguments, list elements, and other expression children from there.
Function head names are not traversed. A successful replacement is not
traversed again during the same pass. Existing overloads use the unbounded
range and therefore preserve the original two-argument product behavior.

Negative levels, fractional levels, reversed ranges, head traversal, and
broader `LevelSpec` forms are rejected rather than approximated.

`rewrite_repeated(...)` performs repeated application up to a caller-provided
rewrite bound.

`rewrite_repeated(..., EvaluationContext&, max_rewrites)` uses the same
traversal semantics, but each successful rewrite also consumes one kernel
evaluation step through the supplied context.

### Bounded Behavior

Rewrite is bounded in two layers:

- caller provides `max_rewrites` as the local rewrite-loop cap
- normalized-head simplification rewrites are now registered through
  `kernel::FunctionRegistry` and run in deterministic priority order per head

### Registered Normalized-Head Rewrites

The first rewrite-extension surface above plain `Rule` rewriting is now a
registered normalized-head contract.

That means:

- `Plus`, `Times`, and `Power` simplification rewrites are registered
  explicitly instead of being dispatched inline from simplification code
- each registered rewrite declares a head, owner metadata, and priority
- simplification consults the registry and stops at the first matching rewrite
- kernel-owned rewrites register as builtin-owned specs
- future pack-owned rewrites can use the same registration surface
- consulting a registered rewrite now syncs rewrite ownership into shared
  `symbol_metadata` and `definition_records`
- those shared records use `rewrite_rule` ownership facts for the rewritten
  head rather than pretending rewrites are ordinary callable function owners

Practical examples:

- consulting a builtin normalized `Plus` rewrite records builtin rewrite
  ownership for `Plus`
- consulting a pack-registered normalized `Power` rewrite can record pack
  ownership such as provider `core-algebra` for `Power`

This slice does not widen rewrite semantics.
It only changes how existing normalized-head rewrites are owned and reached.
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

Practical reading:

- rewrite is currently a tool the evaluator or another caller uses on purpose
- it is not yet a global "always-on symbolic simplifier"
- if you need canonical ordering before matching, normalize first

Registered normalized-head rewrites have one narrow scheduling role:

- they are consulted only from the simplification-stage normalized-head flow
- they are not part of ordinary function-call dispatch precedence
- they do not make `Plus`, `Times`, or `Power` callable through a separate
  rewrite dispatcher
- they do not replace evaluator ownership of list-aware arithmetic,
  domain-sensitive numeric behavior, or special-function shortcuts

Today the general evaluator still does:

1. normalize once at evaluation entry
2. run evaluator-owned dispatch and builtin simplification
3. preserve symbolic fallback when no owner reduces the form

The first rewrite-owned simplification slice is now intentionally narrow:

- binary `Plus` neutral-element identities such as `0 + a -> a`
- binary `Times` neutral and annihilator identities such as `1 * a -> a` and
  `0 * a -> 0`
- basic `Power` identities through a dedicated normalized `Power` rewrite
  entrypoint: `a^1 -> a` and `1^a -> 1` are unconditional, while `a^0 -> 1`
  requires an exact nonzero base or a shared `NonZeroQ[a]` assumption
- unknown `a^0` and `0^0` remain symbolic; the rewrite does not invent a
  domain convention
- exact-integer exponent aggregation in normalized `Times` cancels
  structurally identical generic symbolic bases when the accumulated exponent
  reaches zero, such as `x*x^-1 -> 1`, while preserving explicit invalid
  numeric zero-power forms

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
- special-function symbolic shortcuts and recurrence transforms

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
- supported monomial products such as `x*y`
- `c * x*y` and `c * x^m*y^n`

It should not initially require:

- symbolic coefficient domains
- distributive expansion across arbitrary products
- pack-level algebra metadata

The currently implemented surface now covers:

- `x`
- `x^n` for numeric exponents already preserved by current supported semantics
- `c * x`
- `c * x^n`
- products of supported symbolic basis factors, such as `x*y` and `x^m*y^n`
- scalar multiples of those products

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

It should be treated as a product contract, not as a best-effort heuristic.

It currently supports:

- combining `x + 2*x + 1/3*x` into `10/3 * x`
- combining `x^2 + 2*x^2` into `3 * x^2`
- combining `x*y + 2*x*y` into `3 * x * y`
- combining structurally identical function terms such as
  `Sin[x] + Sin[x]` into `2 * Sin[x]`
- combining structurally identical compound terms such as
  `(x + 1)^2 + (x + 1)^2` into `2 * (x + 1)^2`
- cancelling `x + (-1 * x)` into `0`

### Supported Basis Class For The Coefficient Layer

The supported basis class is structural rather than polynomial-specific.

Collection may run only for normalized `Plus` terms whose addends reduce to one
of these coefficient/body shapes:

- `body`
- `c * body`

Where:

- `body` is a normalized symbol or non-list function expression
- `c` is `Number` or `Rational`
- product bodies use the existing canonical `Times` normalization before they
  are used as structural keys

This means the current coefficient layer promises stable behavior for
structurally identical symbolic bodies with numeric or exact-rational scalar
coefficients. It is not a general polynomial factorer or symbolic coefficient
domain.

The following are outside the supported basis class and must remain preserved
rather than partially collected:

- symbolic coefficient domains
- list-aware arithmetic
- non-identical terms that merely share a factor, such as `x*y + x*z`

If a normalized `Plus` contains both supported and unsupported basis shapes,
the supported slice may still collect, but unsupported terms must pass through
without reinterpretation.

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

### Supported Exponent Class For The Algebra-aware Layer

The algebra-aware layer also has a product contract.

Its supported exponent class is:

- same-base exponent accumulation inside normalized `Times`, where the base is
  a symbol or another structurally identical non-list expression
- nested `Power[Power[x, a], b]` collapse when both `a` and `b` are numeric

In contract terms, the current supported surface is:

- one structurally identical non-list basis reused across the multiplicative
  chain
- exact-integer exponent accumulation on that basis
- numeric nested-power exponent multiplication

Current constraints:

- only normalized `Times` and `Power` forms participate
- basis matching uses normalized structural identity for non-list bases
- nested power collapse only runs when both exponents are numeric
- division cancellation is explicitly out of scope
- power-domain-sensitive transformations are explicitly out of scope
- list-aware arithmetic is explicitly out of scope

The following are outside the supported exponent class and must remain
preserved rather than partially normalized:

- division-driven identities such as `x^a / x^b`
- branch- or assumption-sensitive power laws
- list-aware or container-aware multiplicative behavior

Those forms are intentionally evaluator-owned where behavior exists today. The
current rewrite contract is preservation, not partial imitation of evaluator
semantics.

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

- `RuleDelayed`, pending a precise right-hand-side evaluation timing contract
- rule lists
- predicate-based patterns
- sequence patterns
- rewrite strategy selection
- broad evaluator/rewrite scheduling integration

Conditional rules are implemented only for the current `Condition[...]`
predicate surface. Nested conditional patterns and broader assumption-driven
conditional rewriting remain outside the current contract.

## Replacement Usability Slice

The symbolic MVP added the familiar replacement spelling without changing the
rewrite engine's semantics:

- `ReplaceAll[expr, rule]` applies the existing whole-expression replacement
  traversal.
- `expr /. rule` lowers to `ReplaceAll[expr, rule]` in the symbolic frontend.
- the result and diagnostics must match the equivalent `Replace[expr, rule]`
  call for the supported whole-tree traversal.
- rule lists remain unsupported until list ordering, first-match behavior, and
  diagnostics are specified.
- `RuleDelayed` is gated on a contract that says when the right-hand side is
  evaluated, what bindings are visible, how held arguments behave, and how
  repeated replacements consume budgets.

## Next Steps

- keep the symbolic coefficient contract limited to structural monomial bases
  until stronger exact algebra exists
- keep the algebra-aware layer limited to exact-integer exponent accumulation
  on structurally identical bases and numeric nested-power collapse until
  stronger exact algebra exists
- keep division cancellation, power-domain-sensitive behavior, list-aware
  arithmetic, and special-function shortcuts explicitly evaluator-owned until
  stronger kernel contracts exist
- decide which non-arithmetic simplifications are good candidates for future
  rewrite-owned migration
- add broader replacement strategy controls only after a focused traversal
  design is approved
