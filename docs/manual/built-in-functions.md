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

## Numeric And Structural Output

`N[expr]` requests numeric evaluation where supported. `FullForm[expr]` shows
the internal expression structure. Use exact input when exact preservation
matters; a decimal literal is already approximate before `N` runs.

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
:complete Str
:packs
```

Completion includes built-ins, special forms, pack functions, and names defined
in the current session. The CLI help catalog provides terse reference text;
this manual supplies behavioral context.
