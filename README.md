# GradeAnt

[!WARNING]
**This project is a Work in Progress (WIP).** Core features are actively being developed, and breaking changes may occur. Contributions are highly welcome!

GradeAnt is a small cross-platform desktop prototype that converts UI layout images into editable HTML using a CPU-only OpenCV library and a Python `customtkinter` interface. The native implementation uses C++20 and generates nested HTML/CSS from a typed, versioned intermediate representation (IR).

## Build the native compiler

Install a C++20 compiler, CMake 3.16+, and OpenCV development files (including `core`, `imgproc`, and `imgcodecs`). Then build from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The library is written to `build/` (or `build/Release/` with Visual Studio generators).

## Architecture and tests

See [the architecture overview](docs/architecture.md) and [the v1 IR JSON Schema](docs/schemas/gradeant-ir-v1.schema.json). The fidelity, semantic, and hybrid profiles operate on the compiled IR; hybrid is the default. The profile selector in the desktop app regenerates output without repeating detection.

The IR and HTML generation tests can be built without OpenCV:

```sh
cmake -S . -B build-tests -DGRADEANT_BUILD_NATIVE=OFF
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

## Run the desktop app

```sh
python -m venv .venv
# Activate the virtual environment, then:
python -m pip install -r requirements.txt
python src/app/main.py
```

The UI loads the platform library from `build/` and exports a self-contained HTML file. On Windows, build and run with matching architectures (for example, both x64).

## Notes

The current detector uses contour geometry and conservative alignment heuristics; it does not yet reconstruct text, source colours, assets, or exact design intent. Its output is a structural starting point, not production-ready code. The versioned C ABI provides an owned compilation-result handle, IR access, profile-based HTML generation, error codes, optional progress callbacks, and explicit release. The older `compile_image_layout` entry point remains available for compatibility.
