# GradeAnt

GradeAnt is a small cross-platform desktop prototype that converts UI layout images into editable HTML using a CPU-only OpenCV library and a Python `customtkinter` interface. The native implementation uses C++20.

## Build the native compiler

Install a C++20 compiler, CMake 3.16+, and OpenCV development files (including `core`, `imgproc`, and `imgcodecs`). Then build from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The library is written to `build/` (or `build/Release/` with Visual Studio generators).

## Run the desktop app

```sh
python -m venv .venv
# Activate the virtual environment, then:
python -m pip install -r requirements.txt
python src/app/main.py
```

The UI loads the platform library from `build/` and exports a self-contained HTML file. On Windows, build and run with matching architectures (for example, both x64).

## Notes

The current detector uses contour geometry, so it produces a useful structural starting point rather than reconstructing text, colors, or exact design intent. The C ABI keeps its output and error strings in thread-local buffers; callers should copy the returned HTML before the next compile call on that thread. The HTML emitter is deliberately independent of the detector so additional output targets can be introduced without changing image segmentation.
