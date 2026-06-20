# Migration Notes

The repository is currently in a split state:

- the legacy prototype remains buildable and testable
- the rewrite path now has explicit SDK and IR boundaries
- the rewrite implementation is still ahead of the parser/runtime layers

## Recommended Developer Workflow

1. Add new implementation under rewrite-specific targets first.
2. Reuse legacy code only after isolating the dependency and documenting why it is safe.
3. Add rewrite tests for any promoted behavior before wiring it into the SDK surface.
4. Keep product-facing documentation centered on the embedded-engine wedge, not the old broad CAS behavior.

## Promotion Checklist

- Does the feature belong in Trusted Subset V1?
- Can it be expressed without exposing legacy `Expr`?
- Does it have explicit diagnostic behavior?
- Can it be bounded by schema or policy?
- Does it have rewrite-path tests?
