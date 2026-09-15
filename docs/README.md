# Aleph3 Documentation

This page is the entry point and ownership map for the documentation. Each
topic has one canonical home; focused specifications may link to it instead of
repeating its background.

## Start Here

For the complete user journey, start with the
[Aleph3 Manual](manual/README.md). It covers expressions, built-ins, rewriting,
the SDK, packs, sessions, the CLI, and the current notebook foundation. The
same sources can be
[built as a PDF book](manual/README.md#build-the-pdf-book).

1. [Project README](../README.md) - build, run, and product orientation.
2. [Architecture](architecture.md) - system shape and ownership boundaries,
   including the current kernel, pack, session, notebook-core, SDK, and
   dormant web/API experiments.
3. [SDK Guide](sdk/README.md) - embedding surface and SDK-specific references.
4. [Notebook MVP Design](notebook_mvp_design.md) - product scope, shipped
   headless notebook-core slices, planned GUI behavior, evaluation,
   persistence, display, and acceptance contract.
5. [Unified Plan](aleph3_unified_plan.md) - active implementation roadmap,
   including the Windows-first local notebook MVP.
6. [Rational-Expression Domain Conditions Plan](rational_expression_domain_conditions_plan.md) -
   active plan for consuming preserved denominator-exclusion metadata through a
   bounded condition-aware algebra workflow.
7. [Exact Vector Specification](algebra_vector_spec.md) - current exact
   `Dot`, `Cross`, `Norm`, and exact square-root boundary.
   [Exact Vector Algebra Plan](archive/exact_vector_algebra_plan.md) records
   the completed implementation plan.
8. [Simplify Trigonometric Identity Plan](simplify_trig_identity_plan.md) -
   proposed plan for adding the first explicit `Simplify` trigonometric
   identity while preserving conservative ordinary evaluation.
9. [Signal Systems V0 Plan](signal_systems_v0_plan.md) - future plan for
   exact continuous-time SISO transfer functions and stability analysis. It is
   not the active roadmap.
10. [Web Operations](web_mvp_operations.md) - developer runbook for dormant
   web/API experiments. It is not a product roadmap.
11. [IP and Repository Strategy](ip_and_repo_strategy.md) - practical
   public/private transition guidance.
12. [Public App / Private Kernel Split Plan](public_private_cli_split_plan.md) -
   direction for keeping the kernel and CLI private while public notebook and
   app clients use a stable `aleph-runtime` protocol.
13. [Aleph Runtime Protocol M3 Plan](archive/aleph_runtime_protocol_m3_plan.md) -
   completed implementation record for the process-launching runtime client
   slice.
14. [Aleph Runtime Protocol M4 Plan](aleph_runtime_protocol_m4_plan.md) -
   completed implementation record for private CLI migration through the
   runtime client.
15. [Aleph Runtime Protocol M5 Plan](aleph_runtime_protocol_m5_plan.md) -
   active plan for private notebook-lite rehearsal over the runtime boundary.

## Normative References

These documents define current behavior rather than retelling the whole
architecture.

| Area | Canonical documents |
| --- | --- |
| SDK language | [Trusted subset](trusted_subset_v1.md), [stable interfaces](sdk/stable_interfaces.md) |
| Kernel and engine structure | [Architecture](architecture.md), [Kernel design](kernel_design_spec.md), [execution bridge](kernel_execution_bridge_spec.md) |
| Symbols | [Symbol model](kernel_symbol_model_spec.md), [definition precedence](kernel_symbol_definition_precedence.md), [attributes](kernel_attribute_spec.md), [variable analysis](kernel_variable_analysis_spec.md), [list and structural operations](kernel_list_structural_spec.md) |
| Rewriting | [Rewrite specification](kernel_rewrite_spec.md) |
| Exact mathematics | [Exact algebra](kernel_exact_algebra_spec.md), [supported algebra subset](algebra_supported_subset.md), [algebra equivalence](algebra_equivalence_spec.md), [dense matrices](algebra_dense_matrix_spec.md), [vectors](algebra_vector_spec.md), [focused differentiation](calculus_differentiation_spec.md) |
| Assumptions | [Assumptions specification](kernel_assumptions_spec.md) |
| Registration | [Registration lifecycle](kernel_registration_lifecycle_spec.md) |
| Quality | [Feature development workflow](feature_development_workflow.md), [contract test matrix](contract_test_matrix.md), [header documentation guideline](header_documentation_guideline.md) |
| Build and tests | [Build and targets](sdk/build_and_targets.md), [testing strategy](sdk/testing_strategy.md), [Windows Codex build environment](agents/windows-codex-build.md) |

## Document Types

- **Guide** documents teach readers how the system fits together.
- **Specification** documents state testable current contracts and limits.
- **Plan** documents track unfinished work and sequencing. Keep only plans
  that affect current or near-term decisions.
- **Archive** documents under [docs/archive](archive/README.md) preserve completed
  implementation plans that are no longer canonical current-behavior
  references.

A specification should link to architecture terminology rather than reproduce
it. A plan may point at a specification, but does not override it. When code,
a specification, and a plan disagree, treat that as a defect to resolve - not a
reason to add a fourth explanation.

## Maintenance Rule

Before adding a document, decide whether the material belongs in an existing
guide, specification, or plan. Prefer a section and a stable anchor over a new
top-level file. Keep historical discussion in Git history; keep the current
tree focused on facts readers still need.

Remove a document when it is obsolete, unreferenced, and no longer useful as a
canonical specification, current guide, active plan, operational runbook, or
archived implementation record. When a superseded document still explains a
completed decision or migration, move it under `docs/archive/` instead of
deleting it.
