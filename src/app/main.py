"""GradeAnt desktop shell for converting UI layout images to HTML."""

from __future__ import annotations

import ctypes
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


class CompilerLibrary:
    """Small ctypes boundary with explicit UTF-8 and thread-local pointer copying."""

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
        self._compile = self._library.compile_image_layout
        self._compile.argtypes = [ctypes.c_char_p]
        self._compile.restype = ctypes.c_void_p
        self._get_error = self._library.get_last_error
        self._get_error.argtypes = []
        self._get_error.restype = ctypes.c_char_p

    def compile(self, image_path: Path) -> str:
        # Keep bytes alive for the duration of the native call. Copy the returned
        # thread-local C string immediately, before making any other native call.
        encoded_path = os.fsencode(image_path)
        result = self._compile(encoded_path)
        if not result:
            raw_error = self._get_error()
            detail = raw_error.decode("utf-8", errors="replace") if raw_error else "Unknown native error."
            raise RuntimeError(detail)
        return ctypes.string_at(result).decode("utf-8", errors="replace")


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
        self.preview_photo: PhotoImage | None = None
        self._has_compiled_result = False
        self._build_ui()

    def _build_ui(self) -> None:
        self.grid_columnconfigure(1, weight=1)
        self.grid_rowconfigure(0, weight=1)
        deck = ctk.CTkFrame(self, width=280, corner_radius=0, fg_color=BG)
        deck.grid(row=0, column=0, sticky="nsew", padx=(20, 14), pady=20)
        deck.grid_propagate(False)
        ctk.CTkLabel(deck, text="GradeAnt", font=ctk.CTkFont(size=26, weight="bold")).pack(anchor="w", pady=(8, 2))
        ctk.CTkLabel(deck, text="Turn layouts into code", text_color="#A3A3A3").pack(anchor="w", pady=(0, 24))
        ctk.CTkButton(deck, text="Upload UI image", fg_color=BLUE, command=self._choose_image).pack(fill="x", pady=(0, 10))
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
        ctk.CTkButton(footer, text="Export Production HTML", fg_color=BLUE, command=self._export).pack(side="right")

    def _choose_image(self) -> None:
        selected = filedialog.askopenfilename(title="Choose a layout image", filetypes=[("Images", "*.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff"), ("All files", "*.*")])
        if not selected:
            return
        path = Path(selected).expanduser().resolve()
        self._has_compiled_result = False
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
            result = self.compiler.compile(path)
            self.code.delete("1.0", "end")
            self.code.insert("1.0", result)
            self._update_tree(result)
            self._has_compiled_result = True
            self.status.configure(text=f"Compiled {path.name}")
        except Exception as exc:
            self.status.configure(text="Compilation failed")
            messagebox.showerror("Could not compile layout", str(exc))

    def _update_tree(self, html: str) -> None:
        lines = [line.strip() for line in html.splitlines() if 'class="element element--' in line]
        counts = {kind: sum(f"element--{kind}" in line for line in lines) for kind in ("container", "button", "input")}
        summary = [f"{kind.title()} · {count}" for kind, count in counts.items() if count]
        self.tree.configure(state="normal")
        self.tree.delete("1.0", "end")
        self.tree.insert("1.0", "Detected components\n\n" + ("\n".join(summary) if summary else "No clear rectangular components found."))
        self.tree.configure(state="disabled")

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
