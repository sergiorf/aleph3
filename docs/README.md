# Aleph3 Documentation

This page is the entry point and ownership map for the documentation. Each
topic has one canonical home; focused specifications may link to it instead of
repeating its background.

## Start Here

For the complete user journey, start with the
[Aleph3 Manual](manual/README.md). It covers expressions, built-ins, rewriting,
the SDK, packs, and the notebook/workbench direction. The same sources can be
[built as a PDF book](manual/README.md#build-the-pdf-book).

1. [Project README](../README.md) — build, run, and product orientation.
2. [Architecture](architecture.md) — system shape and ownership boundaries.
3. [SDK Guide](sdk/README.md) — embedding surface and SDK-specific references.
4. [Notebook MVP Design](notebook_mvp_design.md) — product scope, planned cells,
   evaluation, persistence, display, and acceptance contract.
5. [IP and Repository Strategy](ip_and_repo_strategy.md) — practical
   public/private transition guidance.
6. [Unified Plan](aleph3_unified_plan.md) — the sole active implementation roadmap.

## Normative References

These documents define current behavior rather than retelling the whole
architecture.

| Area | Canonical documents |
| --- | --- |
| SDK language | [Trusted subset](trusted_subset_v1.md), [stable interfaces](sdk/stable_interfaces.md) |
| Kernel structure | [Kernel design](kernel_design_spec.md), [execution bridge](kernel_execution_bridge_spec.md) |
| Symbols | [Symbol model](kernel_symbol_model_spec.md), [definition precedence](kernel_symbol_definition_precedence.md), [attributes](kernel_attribute_spec.md) |
| Rewriting | [Rewrite specification](kernel_rewrite_spec.md) |
| Exact mathematics | [Exact algebra](kernel_exact_algebra_spec.md), [supported algebra subset](algebra_supported_subset.md), [dense matrices](algebra_dense_matrix_spec.md) |
| Assumptions | [Assumptions specification](kernel_assumptions_spec.md) |
| Registration | [Registration lifecycle](kernel_registration_lifecycle_spec.md) |
| Quality | [Feature development workflow](feature_development_workflow.md), [contract test matrix](contract_test_matrix.md), [header documentation guideline](header_documentation_guideline.md) |
| Build and tests | [Build and targets](sdk/build_and_targets.md), [testing strategy](sdk/testing_strategy.md) |

## Document Types

- **Guide** documents teach readers how the system fits together.
- **Specification** documents state testable current contracts and limits.
- **Plan** documents track unfinished work and sequencing.

A specification should link to architecture terminology rather than reproduce
it. A plan may point at a specification, but does not override it. When code,
a specification, and a plan disagree, treat that as a defect to resolve—not a
reason to add a fourth explanation.

## Maintenance Rule

Before adding a document, decide whether the material belongs in an existing
guide, specification, or plan. Prefer a section and a stable anchor over a new
top-level file. Keep historical discussion in Git history; keep the current
tree focused on facts readers still need.
