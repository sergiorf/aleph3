# Aleph3 Product Plan

## Product Vision

Aleph3 is a lightweight local symbolic notebook and computation environment
written in modern C++, designed for fast mathematical exploration, clean
notation, and developer-friendly formula/code workflows.

It serves developers, engineers, students, and technical users who want a
fast native workspace without adopting a Python/Jupyter stack or a commercial
CAS. It is a focused product, not a claim of Mathematica, Maple, SageMath,
SymPy, or Jupyter parity.

## Product Layers

- `aleph3_kernel` is the only semantic core and the strategic asset.
- `aleph3_cli` remains useful for scripting, diagnostics, and kernel testing,
  but is not the intended main product.
- the planned `aleph3_notebook` is the main desktop application. It owns cells,
  local documents, display, examples, and later export.
- `aleph3_packs_*` add domain capability through kernel registration. Packs do
  not create private evaluators.

The existing SDK remains a supported embedding boundary and a useful way to
prove that kernel contracts are consumable outside the repository.

## Notebook MVP

The first useful release should let a user:

1. launch a local desktop application;
2. create input and text cells;
3. evaluate supported symbolic expressions through a persistent session;
4. see canonical output and structured diagnostics;
5. save, close, and reload a versioned notebook;
6. open a small gallery of tested example notebooks;
7. use the CLI as a fallback while the kernel remains independently testable.

The MVP is intentionally narrow. Rich export, plotting beyond an explicitly
bounded first slice, collaboration, cloud execution, AI features, broad CAS
coverage, and polished IDE behavior are later work.

## Technology Choice

Choose a GUI toolkit only after a short prototype validates the notebook loop.

| Option | Strength | Cost or risk |
| --- | --- | --- |
| Qt | mature desktop widgets, text, files, accessibility, and packaging | larger dependency and licensing/deployment diligence |
| Dear ImGui | fast prototype and excellent developer tooling | weaker document editing, accessibility, and native desktop conventions |
| wxWidgets | native controls and permissive distribution | smaller ecosystem and less flexible rich notebook presentation |
| Webview/native wrapper | flexible HTML/CSS math and document UI | adds a web runtime and a C++/web bridge |
| custom UI | maximum control | highest schedule, accessibility, text, and platform risk |

The recommended discovery path is a small Qt and webview spike against the
same session fixture. Select on startup size, packaging, text/math rendering,
accessibility, and implementation effort. Do not let the spike add semantics
or commit the repository to a framework before the evaluation is recorded.

## Adoption and Monetization

First make the free or low-friction notebook useful, easy to install, and easy
to understand through screenshots, demos, and examples. Paid packs come only
after the free notebook has a credible create/evaluate/save/reopen loop.

Potential later offerings include:

- Codegen Pack: C/C++ generation, optimization, and embedded-friendly output.
- Engineering Pack: units and electrical, mechanical, control, and numerical workflows.
- Education Pack: guided derivations, exercises, and printable worksheets.
- Finance/Quant Pack: pricing formulas, Greeks, interpolation, and curves.
- AI Assistant Pack: natural-language input, explanations, and formula help,
  gated because it requires provider/API costs and explicit privacy controls.

Free versus paid boundaries must be explicit. The kernel remains the semantic
authority in either tier, and unavailable features produce clear diagnostics.

## Delivery Sequence

The canonical schedule is the [Unified Plan](aleph3_unified_plan.md):

1. specify notebook document, session, display, diagnostics, and persistence
   contracts;
2. build a toolkit spike and select the smallest credible desktop path;
3. deliver cells, evaluation, output, save/reload, and tested examples;
4. harden packaging, recovery, compatibility, and onboarding;
5. add export, plotting, and packs only through separately approved slices.

The behavioral proposal for the first release is the
[Notebook MVP Design](notebook_mvp_design.md).
