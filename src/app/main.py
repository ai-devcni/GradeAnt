"""GradeAnt desktop shell for converting UI layout images to HTML."""

from __future__ import annotations

import ctypes
import json
import os
import platform
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox

try:
    import customtkinter as ctk
except ImportError as exc:
    raise SystemExit("GradeAnt needs customtkinter. Install the packages in requirements.txt.") from exc

try:
    from PIL import Image
    from PIL.ImageTk import PhotoImage
except ImportError as exc:
    raise SystemExit("GradeAnt needs Pillow. Install the packages in requirements.txt.") from exc


ROOT = Path(__file__).resolve().parents[2]
LIBRARY_NAMES = {
    "Windows": ("gradeant_core.dll",),
    "Linux": ("libgradeant_core.so",),
    "Darwin": ("libgradeant_core.dylib",),
}
BG = "#1A1A1A"
CARD = "#2D2D2D"
BLUE = "#007AFF"


# The native library owns this opaque result; Python only passes its pointer back to the API.
class _GradeAntResult(ctypes.Structure):
    pass


# Match the native GradeAntResult pointer used by the C ABI.
_GradeAntResultPointer = ctypes.POINTER(_GradeAntResult)


# Keep these field widths and order aligned with the versioned structs in gradeant_api.h.
class _CompileOptions(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("api_version", ctypes.c_uint32),
        ("progress_callback", ctypes.c_void_p),
        ("user_data", ctypes.c_void_p),
    ]


class _GenerateOptions(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("api_version", ctypes.c_uint32),
        ("profile", ctypes.c_int32),
        ("flags", ctypes.c_uint32),
    ]


class CompilerLibrary:
    """Typed ctypes boundary for the versioned native compiler API."""

    def __init__(self) -> None:
        names = LIBRARY_NAMES.get(platform.system())
        if not names:
            raise RuntimeError(f"Unsupported platform: {platform.system()}")
        candidates = [ROOT / "build" / name for name in names]
        candidates += [ROOT / "build" / "Release" / name for name in names]
        path = next((item for item in candidates if item.is_file()), None)
        if path is None:
            searched = "\n".join(str(item) for item in candidates)
            raise FileNotFoundError(f"Native compiler library was not found. Build it with CMake. Searched:\n{searched}")
        self._library = ctypes.CDLL(os.fspath(path))
        self._compile = self._library.gradeant_compile_image_v1
        self._compile.argtypes = [
            ctypes.c_char_p,
            ctypes.POINTER(_CompileOptions),
            ctypes.POINTER(_GradeAntResultPointer),
        ]
        self._compile.restype = ctypes.c_int32
        self._get_ir = self._library.gradeant_result_get_ir_json
        self._get_ir.argtypes = [_GradeAntResultPointer]
        self._get_ir.restype = ctypes.c_char_p
        self._generate = self._library.gradeant_result_generate_html_v1
        self._generate.argtypes = [_GradeAntResultPointer, ctypes.POINTER(_GenerateOptions)]
        self._generate.restype = ctypes.c_char_p
        self._release = self._library.gradeant_result_release
        self._release.argtypes = [_GradeAntResultPointer]
        self._release.restype = None
        self._get_error = self._library.gradeant_get_last_error_v1
        self._get_error.argtypes = []
        self._get_error.restype = ctypes.c_char_p

    def compile(self, image_path: Path) -> tuple[_GradeAntResultPointer, dict]:
        encoded_path = os.fsencode(image_path)
        result = _GradeAntResultPointer()
        status = self._compile(encoded_path, None, ctypes.byref(result))
        if status != 0:
            raw_error = self._get_error()
            detail = raw_error.decode("utf-8", errors="replace") if raw_error else "Unknown native error."
            raise RuntimeError(detail)
        if not result:
            raise RuntimeError("The native compiler returned success without a compilation result.")
        try:
            # c_char_p copies the borrowed native JSON string into Python bytes here.
            raw_ir = self._get_ir(result)
            if raw_ir is None:
                raw_error = self._get_error()
                detail = raw_error.decode("utf-8", errors="replace") if raw_error else "Could not retrieve IR."
                raise RuntimeError(detail)
            return result, json.loads(raw_ir.decode("utf-8"))
        except Exception:
            self.release(result)
            raise

    def generate_html(self, result: _GradeAntResultPointer, profile: str) -> str:
        profiles = {"fidelity": 0, "semantic": 1, "hybrid": 2}
        if profile not in profiles:
            raise ValueError(f"Unsupported reconstruction profile: {profile}")
        options = _GenerateOptions(ctypes.sizeof(_GenerateOptions), 1, profiles[profile], 0)
        raw_html = self._generate(result, ctypes.byref(options))
        if raw_html is None:
            raw_error = self._get_error()
            detail = raw_error.decode("utf-8", errors="replace") if raw_error else "Unknown generation error."
            raise RuntimeError(detail)
        return raw_html.decode("utf-8", errors="replace")

    def release(self, result: _GradeAntResultPointer) -> None:
        if result:
            self._release(result)


class GradeAntApp(ctk.CTk):
    def __init__(self) -> None:
        super().__init__()
        ctk.set_appearance_mode("Dark")
        ctk.set_default_color_theme("blue")
        self.title("GradeAnt · UI to Code")
        self.geometry("1280x820")
        self.minsize(900, 600)
        self.configure(fg_color=BG)
        self.compiler: CompilerLibrary | None = None
        self.image_path: Path | None = None
        # Retain the native result so profile changes regenerate output without re-detection.
        self.compiled_result: _GradeAntResultPointer | None = None
        self.ir_data: dict | None = None
        self.preview_photo: PhotoImage | None = None
        self._has_compiled_result = False
        self._build_ui()
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build_ui(self) -> None:
        self.grid_columnconfigure(1, weight=1)
        self.grid_rowconfigure(0, weight=1)
        deck = ctk.CTkFrame(self, width=280, corner_radius=0, fg_color=BG)
        deck.grid(row=0, column=0, sticky="nsew", padx=(20, 14), pady=20)
        deck.grid_propagate(False)
        ctk.CTkLabel(deck, text="GradeAnt", font=ctk.CTkFont(size=26, weight="bold")).pack(anchor="w", pady=(8, 2))
        ctk.CTkLabel(deck, text="Turn layouts into code", text_color="#A3A3A3").pack(anchor="w", pady=(0, 24))
        ctk.CTkButton(deck, text="Upload UI image", fg_color=BLUE, command=self._choose_image).pack(fill="x", pady=(0, 10))
        ctk.CTkLabel(deck, text="RECONSTRUCTION PROFILE", text_color="#A3A3A3", font=ctk.CTkFont(size=11, weight="bold")).pack(anchor="w", pady=(12, 6))
        self.profile = ctk.CTkOptionMenu(deck, values=["Hybrid", "Fidelity", "Semantic"], command=self._profile_changed)
        self.profile.set("Hybrid")
        self.profile.pack(fill="x")
        ctk.CTkLabel(deck, text="COMPONENTS", text_color="#A3A3A3", font=ctk.CTkFont(size=11, weight="bold")).pack(anchor="w", pady=(18, 8))
        self.tree = ctk.CTkTextbox(deck, height=280, fg_color=CARD, border_width=0, font=ctk.CTkFont(family="Consolas", size=13))
        self.tree.pack(fill="both", expand=True)
        self.tree.insert("1.0", "Upload an image to detect\nlayout components.")
        self.tree.configure(state="disabled")

        workspace = ctk.CTkFrame(self, fg_color=BG)
        workspace.grid(row=0, column=1, sticky="nsew", padx=(0, 20), pady=20)
        workspace.grid_columnconfigure((0, 1), weight=1, uniform="pane")
        workspace.grid_rowconfigure(1, weight=1)
        ctk.CTkLabel(workspace, text="SOURCE IMAGE", text_color="#A3A3A3", font=ctk.CTkFont(size=11, weight="bold")).grid(row=0, column=0, sticky="w", padx=(0, 8), pady=(0, 8))
        ctk.CTkLabel(workspace, text="COMPILED HTML", text_color="#A3A3A3", font=ctk.CTkFont(size=11, weight="bold")).grid(row=0, column=1, sticky="w", padx=(8, 0), pady=(0, 8))
        self.image_panel = ctk.CTkLabel(workspace, text="Choose a PNG, JPEG, BMP, WebP, or TIFF image", fg_color=CARD, corner_radius=10)
        self.image_panel.grid(row=1, column=0, sticky="nsew", padx=(0, 8))
        self.code = ctk.CTkTextbox(workspace, fg_color=CARD, corner_radius=10, font=ctk.CTkFont(family="Consolas", size=12), wrap="none")
        self.code.grid(row=1, column=1, sticky="nsew", padx=(8, 0))
        self.code.insert("1.0", "<!-- Generated HTML will appear here -->")
        footer = ctk.CTkFrame(workspace, fg_color="transparent")
        footer.grid(row=2, column=0, columnspan=2, sticky="ew", pady=(12, 0))
        self.status = ctk.CTkLabel(footer, text="Ready", text_color="#A3A3A3")
        self.status.pack(side="left")
        ctk.CTkButton(footer, text="Export HTML", fg_color=BLUE, command=self._export).pack(side="right")

    def _choose_image(self) -> None:
        selected = filedialog.askopenfilename(title="Choose a layout image", filetypes=[("Images", "*.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff"), ("All files", "*.*")])
        if not selected:
            return
        path = Path(selected).expanduser().resolve()
        self._release_compiled_result()
        self._has_compiled_result = False
        self.ir_data = None
        self.code.delete("1.0", "end")
        self.code.insert("1.0", "<!-- Generated HTML will appear here -->")
        self.tree.configure(state="normal")
        self.tree.delete("1.0", "end")
        self.tree.insert("1.0", "No components detected yet.")
        self.tree.configure(state="disabled")
        if not path.is_file():
            messagebox.showerror("Image unavailable", "The selected file could not be found.")
            return
        try:
            with Image.open(path) as source:
                image = source.convert("RGB")
                image.thumbnail((760, 700))
                self.preview_photo = PhotoImage(image)
            self.image_panel.configure(image=self.preview_photo, text="")
            self.image_path = path
            self.status.configure(text=f"Compiling {path.name}…")
            self.update_idletasks()
            if self.compiler is None:
                self.compiler = CompilerLibrary()
            result, ir_data = self.compiler.compile(path)
            try:
                html = self.compiler.generate_html(result, self.profile.get().lower())
            except Exception:
                self.compiler.release(result)
                raise
            self.compiled_result = result
            self.ir_data = ir_data
            self.code.delete("1.0", "end")
            self.code.insert("1.0", html)
            self._update_tree(ir_data)
            self._has_compiled_result = True
            self.status.configure(text=f"Compiled {path.name}")
        except Exception as exc:
            self.status.configure(text="Compilation failed")
            messagebox.showerror("Could not compile layout", str(exc))

    def _update_tree(self, ir_data: dict) -> None:
        lines: list[str] = []

        def append_component(component: dict, depth: int) -> None:
            lines.append(f"{'  ' * depth}{component.get('type', 'component')} · {component.get('id', '')}")
            for child in component.get("children", []):
                append_component(child, depth + 1)

        for component in ir_data.get("components", []):
            append_component(component, 0)
        self.tree.configure(state="normal")
        self.tree.delete("1.0", "end")
        self.tree.insert("1.0", "Detected component hierarchy\n\n" + ("\n".join(lines) if lines else "No clear rectangular components found."))
        self.tree.configure(state="disabled")

    def _profile_changed(self, profile: str) -> None:
        if self.compiler is None or self.compiled_result is None:
            return
        try:
            html = self.compiler.generate_html(self.compiled_result, profile.lower())
            self.code.delete("1.0", "end")
            self.code.insert("1.0", html)
            self._has_compiled_result = True
            self.status.configure(text=f"Generated {profile.lower()} profile")
        except Exception as exc:
            self.status.configure(text="Generation failed")
            messagebox.showerror("Could not generate layout", str(exc))

    def _release_compiled_result(self) -> None:
        if self.compiler is not None and self.compiled_result is not None:
            self.compiler.release(self.compiled_result)
        self.compiled_result = None

    def _on_close(self) -> None:
        self._release_compiled_result()
        self.destroy()

    def _export(self) -> None:
        html = self.code.get("1.0", "end-1c")
        if not self._has_compiled_result or not html.strip() or html.startswith("<!-- Generated"):
            messagebox.showinfo("Nothing to export", "Compile an image before exporting HTML.")
            return
        destination = filedialog.asksaveasfilename(title="Export HTML", defaultextension=".html", filetypes=[("HTML document", "*.html")], initialfile="generated-layout.html")
        if not destination:
            return
        try:
            Path(destination).write_text(html, encoding="utf-8", newline="\n")
            self.status.configure(text=f"Exported {Path(destination).name}")
        except OSError as exc:
            messagebox.showerror("Export failed", str(exc))


if __name__ == "__main__":
    app = GradeAntApp()
    app.mainloop()
