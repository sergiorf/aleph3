# Kernel Representation Decision

## Status

Accepted for the current refactor program.

Related documents:

- [Aleph3 Unified Plan](aleph3_unified_plan.md)
- [Representation Lowering Strategy](representation_lowering_strategy.md)

## Decision

Aleph3 should use the symbolic expression model centered on `Expr` as the
long-term kernel semantic representation.

`ir::Node` should remain an SDK/trusted-subset representation only.

That means:

- `Expr` is the kernel semantic form
- `ir::Node` is not a peer kernel representation
- any future use of `ir::Node` must be as a constrained frontend, validation,
  or compile-time artifact

## Why This Decision Exists

The repository historically developed two semantic centers:

- symbolic evaluation built on `Expr`
- SDK runtime evaluation built on `ir::Node`

That is the main architectural problem in the project.
This decision exists to stop the repo from preserving both as permanent
semantic peers.

## Evaluation Criteria

The kernel representation must:

- represent symbolic structure without loss
- support symbolic fallback
- support exact rational and future exact scalar growth
- support user definitions and symbol metadata
- support future pattern matching and rewrite rules
- support assumptions/domain-aware transformations later
- allow SDK trusted-subset execution as a constrained case

## Comparison

### `Expr`

Current strengths:

- already represents symbolic constructs directly
- already supports symbols, function calls, rules, assignments, and function
  definitions
- already serves as the basis for symbolic fallback behavior
- already aligns with planned symbol-centric and rewrite-driven kernel growth

Current weaknesses:

- not yet cleanly separated into a formal kernel module
- some surrounding evaluator architecture is still ad hoc
- host-facing diagnostics and operational controls are weaker than the SDK path

### `ir::Node`

Current strengths:

- narrow and disciplined for the trusted SDK subset
- clear source spans and validation/runtime flow
- well-suited for bounded embedded evaluation

Current weaknesses:

- does not represent the full symbolic expression universe
- does not model rules, assignments, symbolic definitions, or broader symbolic
  constructs required by the kernel direction
- would require substantial expansion to become a full symbolic kernel model
- expanding it to full kernel scope would likely recreate `Expr` under another
  name

## Conclusion

`Expr` is the better kernel foundation because the long-term kernel needs full
symbolic semantics, not only trusted-subset execution.

`ir::Node` is still valuable, but only as a constrained SDK-side form.

## Consequences

### What stays kernel-owned

- symbolic expression/value structure
- symbolic evaluation and fallback
- symbol definitions and metadata
- future rewrite and assumptions hooks

### What stays SDK-owned

- schema validation over the trusted subset
- policy enforcement and operational limits
- host-facing `Engine`, `Schema`, `Policy`, and `CompiledFormula`
- trusted-subset compilation artifacts

### What happens to `ir::Node`

`ir::Node` may remain in the repo for one or more of these roles:

- parser output for the SDK trusted subset
- validator input for the SDK trusted subset
- migration adapter input lowered into kernel execution

It should not become:

- the second kernel AST
- a rival evaluator center
- the place where new symbolic language features are added first

## Immediate Implementation Implications

1. Introduce `aleph3_kernel` as the target that owns `Expr`-based semantics.
2. Treat the former `runtime::Evaluator` path as transitional and removable.
3. Route SDK evaluation toward the kernel rather than preserving a permanent
   `ir::Node` evaluator.
4. Keep trusted-subset validation and policy in the SDK layer.
5. Add explicit adapters only where needed for migration.

## Explicit Non-Decision

This document does not claim that the current `Expr` type is already the final
kernel API or final data model.

It only decides the semantic direction:

- evolve `Expr` into the kernel representation
- do not evolve `ir::Node` into a second full symbolic kernel
