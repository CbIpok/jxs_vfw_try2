import os
import re
import subprocess
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

# Paths and constants
FFMPEG_PATH = r"C:\dmitrienkomy\cpp\jxs_ffmpeg\install-dir\bin\ffmpeg.exe"
FFPROBE_PATH = FFMPEG_PATH.replace('ffmpeg.exe', 'ffprobe.exe')
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_COMPRESSED_DIR = os.path.normpath(os.path.join(BASE_DIR, "..", "test_data", "compressed"))
DEFAULT_RAW_DIR = os.path.normpath(os.path.join(BASE_DIR, "..", "test_data", "raw"))

# Supported formats mapping: Display -> (ffmpeg pixel format, bit depth)
COLOR_FORMATS = {
    "YUV 420 8-bit": ("yuv420p", "8"),
    "YUV 420 10-bit": ("yuv420p10le", "10"),
    "YUV 420 12-bit": ("yuv420p12le", "12"),
    "YUV 420 14-bit": ("yuv420p14le", "14"),
    "YUV 422 8-bit": ("yuv422p", "8"),
    "YUV 422 10-bit": ("yuv422p10le", "10"),
    "YUV 422 12-bit": ("yuv422p12le", "12"),
    "YUV 422 14-bit": ("yuv422p14le", "14"),
    "YUV 444 8-bit": ("yuv444p", "8"),
    "YUV 444 10-bit": ("yuv444p10le", "10"),
    "YUV 444 12-bit": ("yuv444p12le", "12"),
    "YUV 444 14-bit": ("yuv444p14le", "14"),
    "RGB 8-bit":     ("gbrp", "8"),
    "RGB 10-bit":    ("gbrp10le", "10"),
    "RGB 12-bit":    ("gbrp12le", "12"),
    "RGB 14-bit":    ("gbrp14le", "14"),
}

def exec_cmd(cmd):
    """Execute command, capture combined stdout and stderr, and print shell-ready command."""
    shell_str = ' '.join(f'"{c}"' for c in cmd)
    print(f"[exec] Shell command: {shell_str}")
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    out, _ = process.communicate()
    rc = process.returncode
    log = out + f"\n[exec] rc={rc}"
    print(log)
    return rc, shell_str


def get_resolution(input_path):
    """Use ffprobe to get video resolution as WIDTHxHEIGHT."""
    try:
        cmd = [FFPROBE_PATH, '-v', 'error', '-select_streams', 'v:0',
               '-show_entries', 'stream=width,height', '-of', 'csv=p=0:s=x', input_path]
        output = subprocess.check_output(cmd, stderr=subprocess.STDOUT, text=True)
        return output.strip()
    except subprocess.CalledProcessError as e:
        print(f"Resolution probe error: {e.output}")
        return None

class ConverterGUI(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("MP4 <-> RAW Converter")
        self.geometry("600x350")

        tab_control = ttk.Notebook(self)
        self.decode_tab = ttk.Frame(tab_control)
        self.encode_tab = ttk.Frame(tab_control)
        tab_control.add(self.decode_tab, text='Decode (MP4 -> RAW)')
        tab_control.add(self.encode_tab, text='Encode (RAW -> MP4)')
        tab_control.pack(expand=1, fill='both')

        self._build_decode_tab()
        self._build_encode_tab()

    def _build_decode_tab(self):
        ttk.Label(self.decode_tab, text="Select MP4 file:").grid(column=0, row=0, padx=5, pady=5, sticky='w')
        self.mp4_path_var = tk.StringVar()
        ttk.Entry(self.decode_tab, textvariable=self.mp4_path_var, width=50).grid(column=1, row=0, padx=5, pady=5)
        ttk.Button(self.decode_tab, text="Browse...", command=self.browse_mp4).grid(column=2, row=0)

        ttk.Label(self.decode_tab, text="Output format:").grid(column=0, row=1, padx=5, pady=5, sticky='w')
        self.decode_format_var = tk.StringVar()
        decode_combo = ttk.Combobox(self.decode_tab, textvariable=self.decode_format_var, state='readonly', width=30)
        decode_combo['values'] = list(COLOR_FORMATS.keys())
        decode_combo.grid(column=1, row=1, padx=5, pady=5)

        ttk.Button(self.decode_tab, text="Decode", command=self.decode).grid(column=1, row=2, pady=10)

    def _build_encode_tab(self):
        ttk.Label(self.encode_tab, text="Select RAW file:").grid(column=0, row=0, padx=5, pady=5, sticky='w')
        self.raw_path_var = tk.StringVar()
        ttk.Entry(self.encode_tab, textvariable=self.raw_path_var, width=50).grid(column=1, row=0, padx=5, pady=5)
        ttk.Button(self.encode_tab, text="Browse...", command=self.browse_raw).grid(column=2, row=0)

        ttk.Button(self.encode_tab, text="Encode", command=self.encode).grid(column=1, row=1, pady=10)

    def browse_mp4(self):
        path = filedialog.askopenfilename(initialdir=DEFAULT_COMPRESSED_DIR, filetypes=[("MP4 files", "*.mp4")])
        if path:
            self.mp4_path_var.set(path)

    def browse_raw(self):
        path = filedialog.askopenfilename(initialdir=DEFAULT_RAW_DIR, filetypes=[("RAW files", "*.raw")])
        if path:
            self.raw_path_var.set(path)

    def decode(self):
        input_path = self.mp4_path_var.get()
        fmt = self.decode_format_var.get()
        if not input_path or not fmt:
            messagebox.showwarning("Missing input or format", "Please select an MP4 file and output format.")
            return

        resolution = get_resolution(input_path)
        if not resolution:
            messagebox.showerror("Resolution Error", "Failed to probe video resolution. See console for details.")
            return

        pix_fmt, depth = COLOR_FORMATS[fmt]
        base = os.path.splitext(os.path.basename(input_path))[0]
        output_name = f"{base}_{resolution}_{pix_fmt}_{depth}.raw"
        output_path = os.path.join(DEFAULT_RAW_DIR, output_name)

        cmd = [FFMPEG_PATH, '-i', input_path, '-f', 'rawvideo', '-pix_fmt', pix_fmt, output_path]
        rc, shell_str = exec_cmd(cmd)
        if rc == 0:
            messagebox.showinfo("Success", f"Decoded to {output_path}\nCommand:\n{shell_str}")
        else:
            messagebox.showerror("Decoding Error", "See console for details.")

    def encode(self):
        input_path = self.raw_path_var.get()
        if not input_path:
            messagebox.showwarning("Missing input", "Please select a RAW file to encode.")
            return

        fname = os.path.splitext(os.path.basename(input_path))[0]
        parts = fname.split('_')
        # Expect at least 4 parts: base, resolution, pix_fmt, depth
        if len(parts) < 4 or not re.match(r"^\d+x\d+$", parts[-3]) or parts[-2] not in [v[0] for v in COLOR_FORMATS.values()]:
            messagebox.showerror("Invalid filename",
                                 "Filename must be in format base_resolution_pixfmt_depth.raw,\n" +
                                 "e.g. video_1920x1080_yuv420p_8.raw")
            return

        resolution = parts[-3]
        pix_fmt = parts[-2]
        depth = parts[-1]
        base = '_'.join(parts[:-3])
        output_name = f"{base}_{resolution}_{pix_fmt}_{depth}.mp4"
        output_path = os.path.join(DEFAULT_COMPRESSED_DIR, output_name)

        cmd = [FFMPEG_PATH, '-f', 'rawvideo', '-pix_fmt', pix_fmt,
               '-s', resolution, '-i', input_path,
               '-c:v', 'mpeg4', output_path]
        rc, shell_str = exec_cmd(cmd)
        if rc == 0:
            messagebox.showinfo("Success", f"Encoded to {output_path}\nCommand:\n{shell_str}")
        else:
            messagebox.showerror("Encoding Error", "See console for details.")

if __name__ == "__main__":
    app = ConverterGUI()
    app.mainloop()
