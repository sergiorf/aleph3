# Aleph3 Agent Guidance

Before planning substantial work, read
[`docs/aleph3_unified_plan.md`](docs/aleph3_unified_plan.md), the relevant
canonical specifications, and
[`docs/feature_development_workflow.md`](docs/feature_development_workflow.md).

Use the complete feature workflow automatically when work changes public or
architectural behavior, subsystem ownership, or the supported symbolic subset.
Keep small fixes lightweight.

The kernel is the only semantic core. Do not create private SDK, evaluator,
pack, CLI, session, or IDE semantics to bypass a missing shared contract. Keep
exactness, unsupported behavior, diagnostics, compatibility, and resource
budgets explicit.

Run focused and affected broader tests, review the final diff, and report
verification evidence before claiming completion. Worktrees, subagents, and
commits are optional execution tools, not repository requirements.
