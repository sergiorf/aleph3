# Built-in Functions

This chapter introduces the current kernel-facing function families. Algebra
pack functions are covered in [The Algebra Pack](packs-algebra.md). Availability
does not imply that every possible symbolic identity is implemented.

## Arithmetic And Elementary Functions

```text
2 + 3                    -> 5
8 - 3                    -> 5
4 * 5                    -> 20
8 / 4                    -> 2
2^5                      -> 32
Sqrt[9]                  -> 3
Abs[-4]                  -> 4
```

`Floor`, `Ceiling`/`Ceil`, and `Round` provide rounding. `Exp`, `Ln`, and `Log`
provide exponential and logarithmic forms. Domain errors remain explicit.

## Trigonometric And Hyperbolic Functions

Angles are in radians:

```text
Sin[0]                   -> 0
Cos[0]                   -> 1
Tan[x]
ArcTan[y, x]
Sinh[x]
Cosh[x]
```

The catalog also includes reciprocal and inverse families such as `Sec`,
`Csc`, `Cot`, `ArcSin`, `ArcCos`, and `ArcTan`.

## Logic And Conditional Evaluation

```text
And[True, True]          -> True
Or[False, True]          -> True
If[3 < 4, "yes", "no"]  -> "yes"
```

`If`, `And`, and `Or` control argument evaluation. An unselected `If` branch is
not evaluated; this is kernel behavior, not a text macro.

## Session State Cleanup

Interactive sessions preserve assignments and user function definitions until
they are removed or the session ends. `Clear` and `Unset` take an unevaluated
symbol name and return that symbol on success:

```text
a = 10                                  -> a
a                                       -> 10
Clear[a]                                -> a
a                                       -> a

f[x_] := x + 1                          -> f[x_] := x + 1
f[2]                                    -> 3
Clear[f]                                -> f
f[2]                                    -> f[2]
```

`Unset` removes only an own value. It does not remove user function
definitions:

```text
f[x_] := x + 1                          -> f[x_] := x + 1
Unset[f]                                -> f
f[2]                                    -> 3
```

Unknown user symbols are deterministic no-ops. Builtin, pack, special-form, and
host-owned names cannot be cleared unless the command is removing a
session-local value or definition layered on that name. Syntax such as `x =.`
is not implemented.

To discard all session-local definitions at once in the CLI REPL, use the
session lifecycle command `:reset`; it is not a symbolic builtin.

## Strings And Lists

```text
StringJoin["aleph", "3"]               -> "aleph3"
StringLength["aleph3"]                  -> 6
StringReplace["abcabc", "abc" -> "x"] -> "xx"
StringTake["Hello", {2, 4}]             -> "ell"
Length[{a, b, c}]                        -> 3
```

String ranges use the documented one-based convention. Lists are expression
containers; accepting a list does not make every function automatically
listable.

## Structural Inspection And Finite List Transforms

`Head` returns a symbol naming the public head of an evaluated expression. The
right-hand side in each example is the value returned by Aleph3:

```text
Head[f[x, y + 1]]                       -> f
Head[{a, b}]                            -> List
Head[3]                                 -> Integer
Head[1.5]                               -> Real
Head[1/2]                               -> Rational
```

The returned value is an ordinary symbol, so it can itself be inspected:

```text
Head[Head[3]]                           -> Symbol
```

`Part` extracts one-based parts from supported compound expressions:

```text
Part[f[x, y], 1]                        -> x
Part[{a, b, c}, 2]                      -> b
Part[x -> y, 2]                         -> y
```

The first list-transform slice works only over explicit finite lists:

```text
Map[f, {a, b, c}]                       -> {f[a], f[b], f[c]}
Apply[f, {a, b}]                        -> f[a, b]
Select[{1, x, 2}, IntegerQ]             -> {1, 2}
Cases[{x, 1, y}, _Symbol]               -> {x, y}
```

This is intentionally not general Mathematica compatibility. Nested `Part`,
levels, tree-wide traversal, heads traversal, sequence patterns, rule lists,
and implicit `Listable` behavior are outside this slice. Invalid part indexes,
atomic part extraction, non-list list-transform inputs, unsupported patterns,
and predicates that return concrete non-boolean values are diagnosed.

## Numeric And Structural Output

`N[expr]` requests numeric evaluation where supported. `FullForm[expr]`
evaluates its argument, then returns an exact string containing the internal
expression structure:

```text
FullForm[x + 1]                         -> "Plus[x, 1]"
FullForm[1/2 + 1/3]                     -> "Rational[5, 6]"
```

Use exact input when exact preservation matters; a decimal literal is already
approximate before `N` runs. `FullForm` output is diagnostic text, not a stable
serialization format. CLI and session inspection commands such as
`symbolic-fullform` and `:inspect` inspect parsed input structure before normal
evaluation.

## Symbolic Predicates

```text
Positive[3]              -> True
Negative[-2]             -> True
ZeroQ[0]                 -> True
IntegerQ[3]              -> True
RationalQ[1/2]           -> True
```

`NonNegative`, `NonPositive`, `NonZeroQ`, and `RealQ` belong to the same family.
An unknown fact is not silently treated as proven.

```text
Positive[x]              -> Positive[x]
Refine[Positive[x], x > 0] -> True
```

Predicates answer only from supported exact values and assumption facts. They
do not perform general inequality or theorem solving.

## Discovering Functions

```text
:help
:help Factor
:help :reset
:complete Str
:packs
```

Bare `:help` shows a grouped discovery menu for REPL commands, builtins,
special forms, discovered packs, and current user-defined names. Focused help,
such as `:help Factor` or `:help core-algebra`, prints accepted forms, short
manual-backed examples, exactness notes, unsupported boundaries, and owning
pack or component metadata where relevant.

Completion includes builtins, special forms, pack functions, and names defined
in the current session. If a user definition has the same name as a
provider-owned symbol, discovery still reports the provider-owned entry because
provider behavior keeps precedence.
