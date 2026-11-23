# GradeAnt architecture

## Current pipeline

The native library is split into focused stages:

1. `src/api/gradeant_api.cpp` loads and validates the input image and owns the public C ABI.
2. `src/core/detection.cpp` converts the image to grayscale, finds candidate regions, assigns containment-based parents, and conservatively infers aligned horizontal or vertical groups.
3. `src/core/ir.cpp` owns the typed, versioned interface representation, validation, and JSON serialisation.
4. `src/core/generation.cpp` generates nested HTML and CSS from that representation. The reconstruction profile is selected during generation, so switching profiles does not repeat image detection.
5. `src/app/main.py` calls the versioned API through `ctypes`, presents the returned component hierarchy, and retains/releases the native result while profiles are changed.

The previous `compile_image_layout` and `get_last_error` entry points remain available. The former now adapts the new API and returns hybrid-profile HTML.

## Intermediate representation

The C++ IR is defined in `src/core/ir.hpp`; its JSON Schema is `docs/schemas/gradeant-ir-v1.schema.json`. Schema version 1 records document dimensions and metadata, layout, component identifiers and types, absolute bounds, recognised text, style, layout and constraints, asset references, confidence, properties, and recursive children.

The native validator checks schema version, positive document dimensions, unique non-empty identifiers, component types, in-document bounds, finite confidence scores, and non-negative layout values. The serializer emits UTF-8 JSON. The schema file supports validation by external tools; the library currently validates typed IR before generation rather than parsing arbitrary IR JSON.

## Reconstruction profiles

- **Fidelity** positions detected components from their measured bounds.
- **Semantic** emits inferred flow layouts and uses a vertical document-flow fallback for otherwise unclassified sibling groups.
- **Hybrid** is the default and uses the same conservative flow-layout inference, retaining absolute positioning where the detector has insufficient evidence.

This is geometry-based inference, not semantic understanding. The detector currently classifies only generic containers, buttons, and inputs using contour shape heuristics. It does not recognise text, recover source colours or assets, or infer grids.

## Native API and ownership

`src/api/gradeant_api.h` is a C-compatible, versioned interface. It exposes explicit status codes, optional progress callbacks, a result handle, IR retrieval, profile-based generation, error retrieval, and an explicit release function. The API does not expose C++ types or exceptions.

The returned IR string remains valid until its result handle is released. Generated HTML remains valid until the next generation call on that same handle or until it is released. Error text is thread-local and remains valid until the next API call that updates the thread-local error state or the thread exits. Callers must copy strings they need to retain. A handle must be released exactly once.

The options structs carry their size and API version. Only version 1 is accepted; unrecognised generation flags are not supported.

## Build and tests

The native application requires C++20, CMake 3.16+, and OpenCV development libraries (`core`, `imgproc`, and `imgcodecs`). The focused IR and generation test target does not need OpenCV:

```sh
cmake -S . -B build-tests -DGRADEANT_BUILD_NATIVE=OFF
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

Build the application library with the default native option:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Deferred work

The current repository does not yet include OCR, robust image and asset recognition, richer hierarchy or grid inference, independently configurable HTML/CSS output, project persistence, asynchronous desktop compilation, browser preview or screenshot comparison, a reference screenshot corpus, or broad Python/C ABI integration tests. These remain follow-on work; the current contour-based output is a structural starting point and is not production-ready.
