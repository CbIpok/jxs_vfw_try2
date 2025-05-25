import os
import re
import subprocess
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog, ttk

# ----------------------------------------
# Константы
# ----------------------------------------
SCRIPT_DIR     = os.path.dirname(os.path.abspath(__file__))
FFMPEG_EXE     = r"C:\dmitrienkomy\cpp\jxs_ffmpeg\install-dir\bin\ffmpeg.exe"
RAW_DIR        = os.path.join(SCRIPT_DIR, "../test_data/raw")
COMPRESSED_DIR = os.path.join(SCRIPT_DIR, "../test_data/compressed")

os.makedirs(RAW_DIR, exist_ok=True)
os.makedirs(COMPRESSED_DIR, exist_ok=True)

# ----------------------------------------
# Утилиты
# ----------------------------------------
def exec_cmd(cmd_list):
    """Выполняет cmd_list через shell, печатает команду, stdout+stderr, возвращает готовую строку."""
    shell = " ".join(f'"{p}"' for p in cmd_list)
    print(f"[exec] Shell command: {shell}")
    proc = subprocess.Popen(shell, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)
    out, _ = proc.communicate()
    print(out.decode(errors='ignore'))
    print(f"[exec] rc={proc.returncode}")
    return shell

def parse_keyed_filename(path):
    """
    Пытается разбить имя (raw или mkv) вида:
      base_w=1920x1080_fmt=yuv420p_depth=8[_bpp=1.25].*
    Возвращает info dict с ключами base, width, height, resolution, pix_fmt, depth, bpp.
    Если какого-то ключа нет в имени, значение будет None.
    """
    name = os.path.splitext(os.path.basename(path))[0]
    parts = name.split("_")
    info = {"base": [], "width": None, "height": None,
            "resolution": None, "pix_fmt": None,
            "depth": None, "bpp": None}
    for part in parts:
        if part.startswith("w="):
            wh = part[2:].split("x")
            if len(wh) == 2 and all(p.isdigit() for p in wh):
                info["width"], info["height"] = wh
                info["resolution"] = f"{wh[0]}x{wh[1]}"
        elif part.startswith("fmt="):
            info["pix_fmt"] = part[4:]
        elif part.startswith("depth="):
            info["depth"] = part[6:]
        elif part.startswith("bpp="):
            info["bpp"] = part[4:]
        else:
            info["base"].append(part)
    info["base"] = "_".join(info["base"]) or None
    return info

def ask_if_missing(info, key, prompt, initial=""):
    """Если info[key] None, спрашивает у пользователя через простой диалог."""
    if not info.get(key):
        val = simpledialog.askstring("Введите параметр", prompt, initialvalue=initial)
        if not val:
            raise RuntimeError(f"Нужен параметр '{key}', операция прервана.")
        info[key] = val
    return info[key]

def probe_resolution(input_path):
    """Пытается получить разрешение через ffprobe."""
    probe = [
        FFMPEG_EXE.replace("ffmpeg.exe", "ffprobe.exe"),
        "-v", "error", "-select_streams", "v:0",
        "-show_entries", "stream=width,height",
        "-of", "csv=p=0", input_path
    ]
    try:
        out = subprocess.check_output(probe, stderr=subprocess.STDOUT, text=True).strip()
        w, h = out.split(",")
        return f"{w}x{h}"
    except Exception:
        return None

# ----------------------------------------
# Конвертации
# ----------------------------------------
def decode_mp4_to_raw(mp4_path):
    pix_fmt = simpledialog.askstring("PIX_FMT", "Enter pixel format (e.g., yuv420p, gbrp12le):")
    depth   = simpledialog.askstring("DEPTH",   "Enter bit depth (e.g., 8,10,12,14):")
    if not pix_fmt or not depth:
        messagebox.showerror("Прервано", "Не заданы pix_fmt или depth")
        return

    resolution = probe_resolution(mp4_path)
    if not resolution:
        resolution = simpledialog.askstring("Resolution", "Enter resolution (e.g., 1920x1080):")
        if not resolution:
            messagebox.showerror("Прервано", "Не задано разрешение")
            return

    base = os.path.splitext(os.path.basename(mp4_path))[0]
    out_name = f"{base}_w={resolution}_fmt={pix_fmt}_depth={depth}.raw"
    raw_path = os.path.join(RAW_DIR, out_name)

    cmd = [
        FFMPEG_EXE,
        "-y",
        "-i", mp4_path,
        "-f", "rawvideo",
        "-pix_fmt", pix_fmt,
        raw_path
    ]
    shell = exec_cmd(cmd)
    messagebox.showinfo("Готово", f"Команда:\n{shell}")

def encode_raw_to_mp4(raw_path):
    info = parse_keyed_filename(raw_path)
    try:
        ask_if_missing(info, "resolution", "Enter resolution (e.g., 1920x1080):")
        ask_if_missing(info, "pix_fmt",     "Enter source pixel format (e.g., yuv420p, gbrp12le):")
        ask_if_missing(info, "depth",       "Enter source bit depth (e.g., 8,10,12,14):")
    except RuntimeError as e:
        messagebox.showerror("Прервано", str(e))
        return

    base      = info["base"] or os.path.splitext(os.path.basename(raw_path))[0]
    resolution= info["resolution"]
    pix_fmt   = info["pix_fmt"]
    depth     = info["depth"]

    out_name = f"{base}_w={resolution}_fmt=yuv420p_depth=8.mp4"
    mp4_path = os.path.join(COMPRESSED_DIR, out_name)

    cmd = [
        FFMPEG_EXE,
        "-y",
        "-f", "rawvideo",
        "-pix_fmt", pix_fmt,
        "-s:v", resolution,
        "-i", raw_path,
        "-c:v", "mpeg4",
        mp4_path
    ]
    shell = exec_cmd(cmd)
    messagebox.showinfo("Готово", f"Команда:\n{shell}")

def encode_raw_to_jpegxs(raw_path):
    info = parse_keyed_filename(raw_path)
    try:
        ask_if_missing(info, "resolution", "Enter resolution (e.g., 1920x1080):")
        ask_if_missing(info, "pix_fmt",     "Enter source pixel format (e.g., yuv420p, gbrp12le):")
        ask_if_missing(info, "depth",       "Enter source bit depth (e.g., 8,10,12,14):")
        ask_if_missing(info, "bpp",         "Enter bits per pixel (e.g., 1.25):")
    except RuntimeError as e:
        messagebox.showerror("Прервано", str(e))
        return

    base      = info["base"] or os.path.splitext(os.path.basename(raw_path))[0]
    resolution= info["resolution"]
    pix_fmt   = info["pix_fmt"]
    depth     = info["depth"]
    bpp       = info["bpp"]

    out_name = f"{base}_w={resolution}_fmt={pix_fmt}_depth={depth}_bpp={bpp}.mkv"
    mkv_path = os.path.join(COMPRESSED_DIR, out_name)

    cmd = [
        FFMPEG_EXE,
        "-y",
        "-f", "rawvideo",
        "-pix_fmt", pix_fmt,
        "-s:v", resolution,
        "-i", raw_path,
        "-c:v", "libsvtjpegxs",
        "-bpp", bpp,
        mkv_path
    ]
    shell = exec_cmd(cmd)
    messagebox.showinfo("Готово", f"Команда:\n{shell}")

def decode_jpegxs_to_raw(mkv_path):
    info = parse_keyed_filename(mkv_path)
    try:
        ask_if_missing(info, "pix_fmt",     "Enter pixel format used inside MKV (e.g., yuv420p, gbrp12le):")
        ask_if_missing(info, "depth",       "Enter bit depth (e.g., 8,10,12,14):")
        ask_if_missing(info, "resolution",  "Enter resolution (e.g., 1920x1080):")
        # bpp не нужен для декодирования
    except RuntimeError as e:
        messagebox.showerror("Прервано", str(e))
        return

    base      = info["base"] or os.path.splitext(os.path.basename(mkv_path))[0]
    resolution= info["resolution"]
    pix_fmt   = info["pix_fmt"]
    depth     = info["depth"]
    bpp       = info.get("bpp")

    out_name = f"{base}_w={resolution}_fmt={pix_fmt}_depth={depth}"
    if bpp:
        out_name += f"_bpp={bpp}"
    out_name += ".raw"
    raw_path = os.path.join(RAW_DIR, out_name)

    cmd = [
        FFMPEG_EXE,
        "-y",
        "-i", mkv_path,
        "-f", "rawvideo",
        "-pix_fmt", pix_fmt,
        raw_path
    ]
    shell = exec_cmd(cmd)
    messagebox.showinfo("Готово", f"Команда:\n{shell}")

# ----------------------------------------
# GUI
# ----------------------------------------
root = tk.Tk()
root.title("Video Converter")

# Выбор действия
action_var = tk.StringVar(value="MP4 → RAW")
action_combo = ttk.Combobox(
    root, textvariable=action_var, state="readonly",
    values=[
        "MP4 → RAW",
        "RAW → MP4",
        "RAW → JPEG-XS (.mkv)",
        "JPEG-XS (.mkv) → RAW"
    ]
)
action_combo.grid(row=0, column=0, padx=10, pady=10)

# Выбор входного файла
input_var = tk.StringVar()
input_entry = tk.Entry(root, textvariable=input_var, width=50)
input_entry.grid(row=1, column=0, padx=10, pady=5)
def browse():
    ft = [("All files","*.*")]
    a = action_var.get()
    if a == "MP4 → RAW":
        ft = [("MP4","*.mp4")]
    elif a.startswith("RAW"):
        ft = [("RAW","*.raw")]
    else:
        ft = [("MKV","*.mkv")]
    p = filedialog.askopenfilename(filetypes=ft)
    input_var.set(p)
ttk.Button(root, text="Browse", command=browse).grid(row=1, column=1, padx=5)

# Запуск операции
def on_run():
    path = input_var.get()
    if not path:
        messagebox.showerror("Error", "Не выбран входной файл")
        return
    a = action_var.get()
    try:
        if a == "MP4 → RAW":
            decode_mp4_to_raw(path)
        elif a == "RAW → MP4":
            encode_raw_to_mp4(path)
        elif a == "RAW → JPEG-XS (.mkv)":
            encode_raw_to_jpegxs(path)
        elif a == "JPEG-XS (.mkv) → RAW":
            decode_jpegxs_to_raw(path)
    except Exception as e:
        messagebox.showerror("Ошибка", str(e))

ttk.Button(root, text="Run", command=on_run).grid(row=3, column=0, pady=10)

root.mainloop()

