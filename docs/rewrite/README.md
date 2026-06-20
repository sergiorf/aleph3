# Rewrite Docs

This directory is the working index for the embedded-engine rewrite. The older
top-level documents remain the detailed source material; these subdocs make the
rewrite easier to navigate as the SDK-first implementation grows.

## What Is Stable Now

- Public SDK headers under `include/sdk/`
- Minimal trusted-subset IR in `include/ir/Node.hpp`
- Rewrite build target split in `CMakeLists.txt`
- Rewrite tooling CLI target `aleph3_rewrite_cli`
- Rewrite REPL and built-in help/examples in `aleph3_rewrite_cli`
- Contract direction defined by the top-level rewrite docs

## What Is Not Stable Yet

- Frontend parser and lexer
- Validation semantics
- Runtime evaluator behavior
- Packaging and final target names

## Document Map

- [Stable Interfaces](stable_interfaces.md)
- [Build And Targets](build_and_targets.md)
- [Testing Strategy](testing_strategy.md)
- [Migration Notes](migration_notes.md)

## Rewrite Layer Diagram

```mermaid
flowchart LR
    HostApp["Host Application"] --> SDK["sdk\nEngine / Schema / Policy / Types"]
    SDK --> Frontend["frontend\nlexer / parser / diagnostics"]
    Frontend --> IR["ir\ntrusted subset nodes"]
    IR --> Semantics["semantics\nvalidation / analysis"]
    Semantics --> Runtime["runtime\nevaluator / host dispatch / budgets"]
    Runtime --> Value["public Value / RuntimeError"]
    Tooling["tooling\nCLI / examples / tests"] --> SDK
```

## Rewrite Build Diagram

```mermaid
flowchart TD
    Legacy["aleph3_legacy"] --> LegacyCli["aleph3_cli_legacy"]
    RewriteRuntime["aleph3_runtime"] --> RewriteSdk["aleph3_sdk"]
    RewriteSdk --> RewriteTests["aleph3_sdk_tests"]
    Legacy --> LegacyTests["aleph3_legacy_tests"]
```
