"""
WAD Browser for League of Legends `.wad.client` archives.

References:
  - https://github.com/CommunityDragon/Data
  - https://www.communitydragon.org/documentation/wad
"""

from __future__ import annotations

import ctypes
import ctypes.util
import datetime
import json
import os
import re
import struct
import threading
import urllib.error
import urllib.request
import zlib
from pathlib import Path, PurePosixPath

import tkinter as tk
from tkinter import filedialog, messagebox, ttk


WAD_HEADER_STRUCT = struct.Struct("<4s256sQI")
WAD_FILE_ENTRY_STRUCT = struct.Struct("<QIIIBBHQ")
SUBCHUNK_ENTRY_STRUCT = struct.Struct("<IIQ")

WAD_MAGIC = b"RW"
SUPPORTED_MAJOR_VERSION = 3

STORAGE_TYPE_NAMES = {
    0: "raw",
    1: "zlib",
    2: "unknown",
    3: "zstd",
    4: "zstd-multi",
}

COMMUNITY_DRAGON_HASHES_API = "https://api.github.com/repos/CommunityDragon/Data/contents/hashes/lol"
CACHE_DIR = Path("cache")
HASH_CACHE_PATH = CACHE_DIR / "hashes.game.txt"
HTTP_HEADERS = {
    "User-Agent": "LeagueModel-WadBrowser/1.0",
    "Accept": "application/vnd.github+json",
}

XXH64_PRIME1 = 11400714785074694791
XXH64_PRIME2 = 14029467366897019727
XXH64_PRIME3 = 1609587929392839161
XXH64_PRIME4 = 9650029242287828579
XXH64_PRIME5 = 2870177450012600261
UINT64_MASK = 0xFFFFFFFFFFFFFFFF

ROOT_DEFAULT = r"\\DESKTOP-GAMA3CK\GameHDD\myGames\Riot Games\League of Legends\Game\DATA\FINAL"

def _format_time_placeholder() -> str:
    return "-"


def _get_default_root_dir() -> str:
    if os.path.isdir(ROOT_DEFAULT):
        return ROOT_DEFAULT
    return os.getcwd()


def _rotl64(value: int, bits: int) -> int:
    return ((value << bits) | (value >> (64 - bits))) & UINT64_MASK


def _xxh64_round(accumulator: int, lane: int) -> int:
    accumulator = (accumulator + lane * XXH64_PRIME2) & UINT64_MASK
    accumulator = _rotl64(accumulator, 31)
    accumulator = (accumulator * XXH64_PRIME1) & UINT64_MASK
    return accumulator


def _xxh64_merge_round(accumulator: int, lane: int) -> int:
    accumulator ^= _xxh64_round(0, lane)
    accumulator = (accumulator * XXH64_PRIME1 + XXH64_PRIME4) & UINT64_MASK
    return accumulator


def xxh64(data: bytes, seed: int = 0) -> int:
    length = len(data)
    offset = 0

    if length >= 32:
        acc1 = (seed + XXH64_PRIME1 + XXH64_PRIME2) & UINT64_MASK
        acc2 = (seed + XXH64_PRIME2) & UINT64_MASK
        acc3 = seed & UINT64_MASK
        acc4 = (seed - XXH64_PRIME1) & UINT64_MASK

        limit = length - 32
        while offset <= limit:
            acc1 = _xxh64_round(acc1, int.from_bytes(data[offset:offset + 8], "little"))
            acc2 = _xxh64_round(acc2, int.from_bytes(data[offset + 8:offset + 16], "little"))
            acc3 = _xxh64_round(acc3, int.from_bytes(data[offset + 16:offset + 24], "little"))
            acc4 = _xxh64_round(acc4, int.from_bytes(data[offset + 24:offset + 32], "little"))
            offset += 32

        result = (
            _rotl64(acc1, 1)
            + _rotl64(acc2, 7)
            + _rotl64(acc3, 12)
            + _rotl64(acc4, 18)
        ) & UINT64_MASK
        result = _xxh64_merge_round(result, acc1)
        result = _xxh64_merge_round(result, acc2)
        result = _xxh64_merge_round(result, acc3)
        result = _xxh64_merge_round(result, acc4)
    else:
        result = (seed + XXH64_PRIME5) & UINT64_MASK

    result = (result + length) & UINT64_MASK

    while offset + 8 <= length:
        lane = int.from_bytes(data[offset:offset + 8], "little")
        result ^= _xxh64_round(0, lane)
        result = (_rotl64(result, 27) * XXH64_PRIME1 + XXH64_PRIME4) & UINT64_MASK
        offset += 8

    if offset + 4 <= length:
        lane = int.from_bytes(data[offset:offset + 4], "little")
        result ^= (lane * XXH64_PRIME1) & UINT64_MASK
        result = (_rotl64(result, 23) * XXH64_PRIME2 + XXH64_PRIME3) & UINT64_MASK
        offset += 4

    while offset < length:
        result ^= (data[offset] * XXH64_PRIME5) & UINT64_MASK
        result = (_rotl64(result, 11) * XXH64_PRIME1) & UINT64_MASK
        offset += 1

    result ^= result >> 33
    result = (result * XXH64_PRIME2) & UINT64_MASK
    result ^= result >> 29
    result = (result * XXH64_PRIME3) & UINT64_MASK
    result ^= result >> 32
    return result & UINT64_MASK


def hash_wad_path(path: str) -> int:
    lowered = path.replace("\\", "/").lower().encode("utf-8")
    return xxh64(lowered, 0)


def _sanitize_path_component(value: str) -> str:
    value = value.strip()
    if not value:
        return "_"
    return "".join(character if character not in '<>:"\\|?*' else "_" for character in value)


def _try_get_lol_skin_bin_path(file_path: str) -> str | None:
    normalized = file_path.replace("\\", "/").strip("/").lower()
    if not normalized.startswith("data/") or not normalized.endswith(".bin"):
        return None

    relative = normalized[5:]
    if "/" in relative:
        return None

    base_name = PurePosixPath(relative).stem
    champion_match = re.match(r"^([a-z0-9]+)", base_name)
    if champion_match is None:
        return None

    skin_indices = re.findall(r"skin(\d+)", base_name)
    if not skin_indices:
        return None

    champion = champion_match.group(1)
    final_skin_num = skin_indices[-1]
    return f"data/characters/{champion}/skins/skin{final_skin_num}.bin"


def _safe_cache_mkdir() -> None:
    CACHE_DIR.mkdir(parents=True, exist_ok=True)


def _ascii_preview(data: bytes) -> str:
    return "".join(chr(byte) if 32 <= byte <= 126 else "." for byte in data)


def format_hex_dump(data: bytes, max_bytes: int = 16384, bytes_per_row: int = 16) -> str:
    preview = data[:max_bytes]
    lines: list[str] = []
    for offset in range(0, len(preview), bytes_per_row):
        chunk = preview[offset:offset + bytes_per_row]
        hex_part = " ".join(f"{byte:02X}" for byte in chunk)
        hex_part = hex_part.ljust(bytes_per_row * 3 - 1)
        lines.append(f"{offset:08X}  {hex_part}  |{_ascii_preview(chunk)}|")

    if len(data) > max_bytes:
        lines.append("")
        lines.append(f"... truncated preview: showing {max_bytes:,} of {len(data):,} bytes ...")

    return "\n".join(lines)


class ZstdDecompressor:
    def __init__(self) -> None:
        self._dll = None
        self._load_error: str | None = None
        self._load_library()

    def _load_library(self) -> None:
        candidates: list[str] = []
        for candidate in (
            Path("bin/Debug/zstd.dll"),
            Path("bin/Release/zstd.dll"),
            Path("build/ext/leaguelib/ext/zstd/build/cmake/lib/Debug/zstd.dll"),
            Path("build/ext/leaguelib/ext/zstd/build/cmake/lib/Release/zstd.dll"),
        ):
            if candidate.exists():
                candidates.append(str(candidate.resolve()))

        located = ctypes.util.find_library("zstd")
        if located:
            candidates.append(located)
        candidates.append("zstd.dll")

        for candidate in candidates:
            try:
                dll = ctypes.CDLL(candidate)
                dll.ZSTD_decompress.argtypes = [
                    ctypes.c_void_p,
                    ctypes.c_size_t,
                    ctypes.c_void_p,
                    ctypes.c_size_t,
                ]
                dll.ZSTD_decompress.restype = ctypes.c_size_t
                dll.ZSTD_isError.argtypes = [ctypes.c_size_t]
                dll.ZSTD_isError.restype = ctypes.c_uint
                dll.ZSTD_getErrorName.argtypes = [ctypes.c_size_t]
                dll.ZSTD_getErrorName.restype = ctypes.c_char_p
                self._dll = dll
                self._load_error = None
                return
            except OSError as ex:
                self._load_error = str(ex)

    def is_available(self) -> bool:
        return self._dll is not None

    def decompress(self, payload: bytes, expected_size: int) -> bytes:
        if self._dll is None:
            raise RuntimeError(
                "Zstandard support is unavailable. Build the project or place zstd.dll next to the script."
            )

        output = ctypes.create_string_buffer(expected_size)
        source = ctypes.create_string_buffer(payload, len(payload))
        result_size = self._dll.ZSTD_decompress(output, expected_size, source, len(payload))
        if self._dll.ZSTD_isError(result_size):
            message = self._dll.ZSTD_getErrorName(result_size)
            raise RuntimeError(f"ZSTD_decompress failed: {message.decode('utf-8', errors='replace')}")

        return output.raw[:result_size]


class HashNameResolver:
    def __init__(self) -> None:
        self._resolved_names: dict[int, str] = {}
        self._missing_names: set[int] = set()
        self._cache_ready = False
        self._lock = threading.Lock()

    def resolve_many(self, hashes: set[int], status_callback=None) -> None:
        required = {value for value in hashes if value not in self._resolved_names and value not in self._missing_names}
        if not required:
            return

        with self._lock:
            required = {value for value in hashes if value not in self._resolved_names and value not in self._missing_names}
            if not required:
                return

            self._ensure_cache(status_callback)
            scanned_lines = 0
            with HASH_CACHE_PATH.open("r", encoding="utf-8", errors="replace") as handle:
                for line in handle:
                    scanned_lines += 1
                    if scanned_lines % 500000 == 0 and status_callback:
                        status_callback(f"Resolving hash names... scanned {scanned_lines:,} lines")

                    line = line.strip()
                    if not line or line.startswith("#"):
                        continue

                    hash_text, separator, name = line.partition(" ")
                    if not separator:
                        continue

                    try:
                        hash_value = int(hash_text, 16)
                    except ValueError:
                        continue

                    if hash_value not in required:
                        continue

                    self._resolved_names[hash_value] = name
                    required.remove(hash_value)
                    if not required:
                        break

            self._missing_names.update(required)

    def get(self, hash_value: int) -> str | None:
        return self._resolved_names.get(hash_value)

    def _ensure_cache(self, status_callback=None) -> None:
        if self._cache_ready and HASH_CACHE_PATH.exists():
            return

        _safe_cache_mkdir()
        if HASH_CACHE_PATH.exists():
            self._cache_ready = True
            return

        if status_callback:
            status_callback("Downloading CommunityDragon hashes.game.txt cache...")

        request = urllib.request.Request(COMMUNITY_DRAGON_HASHES_API, headers=HTTP_HEADERS)
        with urllib.request.urlopen(request) as response:
            payload = json.load(response)

        files = []
        for item in payload:
            name = item.get("name", "")
            if name == "hashes.game.txt" or name.startswith("hashes.game.txt."):
                files.append(item)

        if not files:
            raise RuntimeError("CommunityDragon hashes.game.txt files were not found.")

        def sort_key(item: dict) -> tuple[int, int]:
            name = item["name"]
            if name == "hashes.game.txt":
                return (0, 0)
            suffix = name.rsplit(".", 1)[-1]
            return (1, int(suffix))

        files.sort(key=sort_key)

        temp_path = HASH_CACHE_PATH.with_suffix(".txt.download")
        with temp_path.open("wb") as output:
            for index, item in enumerate(files, 1):
                if status_callback:
                    status_callback(f"Downloading {item['name']} ({index}/{len(files)})...")

                download_url = item.get("download_url")
                if not download_url:
                    raise RuntimeError(f"Missing download URL for {item['name']}")

                request = urllib.request.Request(download_url, headers=HTTP_HEADERS)
                with urllib.request.urlopen(request) as response:
                    while True:
                        chunk = response.read(1024 * 1024)
                        if not chunk:
                            break
                        output.write(chunk)

        temp_path.replace(HASH_CACHE_PATH)
        self._cache_ready = True


class WadRecord:
    __slots__ = (
        "wad_path",
        "path_hash",
        "offset",
        "compressed_size",
        "file_size",
        "type_data",
        "duplicate",
        "first_subchunk_index",
        "sha256",
        "name",
    )

    def __init__(
        self,
        wad_path: str,
        path_hash: int,
        offset: int,
        compressed_size: int,
        file_size: int,
        type_data: int,
        duplicate: int,
        first_subchunk_index: int,
        sha256: int,
    ) -> None:
        self.wad_path = wad_path
        self.path_hash = path_hash
        self.offset = offset
        self.compressed_size = compressed_size
        self.file_size = file_size
        self.type_data = type_data
        self.duplicate = duplicate
        self.first_subchunk_index = first_subchunk_index
        self.sha256 = sha256
        self.name: str | None = None

    @property
    def storage_type(self) -> int:
        return self.type_data & 0x0F

    @property
    def storage_name(self) -> str:
        return STORAGE_TYPE_NAMES.get(self.storage_type, f"unknown-{self.storage_type}")

    @property
    def subchunk_count(self) -> int:
        return self.type_data >> 4

    def ensure_name_loaded(self, resolver: HashNameResolver) -> None:
        if self.name is None:
            self.name = resolver.get(self.path_hash)

    def matches_text(self, text: str) -> bool:
        haystack = f"{self.path_hash:016x}"
        if self.name:
            haystack += " " + self.name.lower()
            derived_skin_path = _try_get_lol_skin_bin_path(self.name)
            if derived_skin_path:
                haystack += " " + derived_skin_path
        return text in haystack


class WadFile:
    def __init__(self, path: str, resolver: HashNameResolver, zstd: ZstdDecompressor):
        self.path = path
        self.resolver = resolver
        self.zstd = zstd
        self.records: list[WadRecord] = []
        self.record_map: dict[int, WadRecord] = {}
        self.version_major = 0
        self.version_minor = 0
        self.subchunk_stream: bytes | None = None

    def load_index(self) -> None:
        with open(self.path, "rb") as handle:
            header_raw = handle.read(WAD_HEADER_STRUCT.size)
            if len(header_raw) != WAD_HEADER_STRUCT.size:
                raise ValueError("File too small for .wad.client header.")

            base, _ecdsa, _checksum, file_count = WAD_HEADER_STRUCT.unpack(header_raw)
            if base[:2] != WAD_MAGIC:
                raise ValueError(f"Unexpected WAD magic: {base[:2]!r}")

            self.version_major = base[2]
            self.version_minor = base[3]
            if self.version_major != SUPPORTED_MAJOR_VERSION:
                raise ValueError(f"Unsupported WAD version: {self.version_major}.{self.version_minor}")

            for _ in range(file_count):
                raw_entry = handle.read(WAD_FILE_ENTRY_STRUCT.size)
                if len(raw_entry) != WAD_FILE_ENTRY_STRUCT.size:
                    raise ValueError("Unexpected EOF while reading WAD file entries.")

                path_hash, offset, compressed_size, file_size, type_data, duplicate, first_subchunk_index, sha256 = (
                    WAD_FILE_ENTRY_STRUCT.unpack(raw_entry)
                )
                record = WadRecord(
                    wad_path=self.path,
                    path_hash=path_hash,
                    offset=offset,
                    compressed_size=compressed_size,
                    file_size=file_size,
                    type_data=type_data,
                    duplicate=duplicate,
                    first_subchunk_index=first_subchunk_index,
                    sha256=sha256,
                )
                self.records.append(record)
                self.record_map[path_hash] = record

    def resolve_names(self) -> None:
        hashes = {record.path_hash for record in self.records}
        self.resolver.resolve_many(hashes)
        for record in self.records:
            record.ensure_name_loaded(self.resolver)

    def _derive_subchunk_toc_path(self) -> str | None:
        lowered = self.path.replace("\\", "/").lower()
        marker = "data/final"
        marker_index = lowered.find(marker)
        if marker_index < 0:
            return None

        relative_path = PurePosixPath(lowered[marker_index:])
        return relative_path.with_suffix(".subchunktoc").as_posix()

    def _ensure_subchunk_stream(self) -> None:
        if self.subchunk_stream is not None:
            return

        toc_path = self._derive_subchunk_toc_path()
        if not toc_path:
            self.subchunk_stream = b""
            return

        toc_hash = hash_wad_path(toc_path)
        toc_record = self.record_map.get(toc_hash)
        if toc_record is None:
            self.subchunk_stream = b""
            return

        self.subchunk_stream = self.extract_record(toc_record, allow_multichunk=False)

    def extract_record(self, record: WadRecord, allow_multichunk: bool = True) -> bytes:
        with open(self.path, "rb") as handle:
            handle.seek(record.offset, os.SEEK_SET)
            payload = handle.read(record.compressed_size)

        storage_type = record.storage_type
        if storage_type == 0:
            return payload[:record.file_size]

        if storage_type == 1:
            try:
                return zlib.decompress(payload)
            except zlib.error as ex:
                raise RuntimeError(f"zlib decompression failed for {record.path_hash:016x}: {ex}") from ex

        if storage_type == 3:
            return self.zstd.decompress(payload, record.file_size)

        if storage_type == 4:
            if not allow_multichunk:
                return self.zstd.decompress(payload, record.file_size)

            self._ensure_subchunk_stream()
            if not self.subchunk_stream:
                return self.zstd.decompress(payload, record.file_size)

            output = bytearray()
            payload_offset = 0
            frame_count = record.subchunk_count
            for index in range(record.first_subchunk_index, record.first_subchunk_index + frame_count):
                chunk_offset = index * SUBCHUNK_ENTRY_STRUCT.size
                if chunk_offset + SUBCHUNK_ENTRY_STRUCT.size > len(self.subchunk_stream):
                    raise RuntimeError(f"Subchunk index {index} is outside the TOC.")

                compressed_size, uncompressed_size, _subchunk_hash = SUBCHUNK_ENTRY_STRUCT.unpack_from(
                    self.subchunk_stream, chunk_offset
                )
                chunk_payload = payload[payload_offset:payload_offset + compressed_size]
                if len(chunk_payload) != compressed_size:
                    raise RuntimeError(f"Unexpected EOF while reading subchunk {index}.")

                if compressed_size == uncompressed_size:
                    output.extend(chunk_payload)
                elif compressed_size < uncompressed_size:
                    output.extend(self.zstd.decompress(chunk_payload, uncompressed_size))
                else:
                    raise RuntimeError(f"Invalid subchunk sizes at index {index}.")

                payload_offset += compressed_size

            return bytes(output)

        raise RuntimeError(f"Unsupported WAD storage type: {storage_type}")


class WadBrowserApp(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("WAD Browser - League .wad.client")
        self.geometry("1320x780")

        self.hash_resolver = HashNameResolver()
        self.zstd = ZstdDecompressor()

        self.wad_files: list[WadFile] = []
        self.wad_files_by_path: dict[str, WadFile] = {}
        self.asset_tree_scopes: dict[str, tuple[str, object]] = {}
        self.asset_tree_counts: dict[str, int] = {}
        self.current_selected_records: list[WadRecord] = []
        self.current_selected_label = ""
        self._hex_request_serial = 0

        self._build_ui()
        self._set_status("Ready.")

    def _build_ui(self) -> None:
        top = ttk.Frame(self)
        top.pack(side=tk.TOP, fill=tk.X, padx=8, pady=6)

        ttk.Button(top, text="Open WAD Folder...", command=self.open_folder).pack(side=tk.LEFT)
        ttk.Button(top, text="Open WAD File...", command=self.open_wad_file).pack(side=tk.LEFT, padx=(6, 0))

        ttk.Label(top, text="  Filter: ").pack(side=tk.LEFT)

        self.var_filter = tk.StringVar()
        filter_entry = ttk.Entry(top, textvariable=self.var_filter, width=60)
        filter_entry.pack(side=tk.LEFT, fill=tk.X, expand=True)
        filter_entry.bind("<Return>", lambda _event: self.apply_filter())
        filter_entry.bind("<KP_Enter>", lambda _event: self.apply_filter())

        ttk.Button(top, text="Clear", command=self._clear_filter).pack(side=tk.LEFT, padx=(6, 0))
        ttk.Button(top, text="Extract Selected...", command=self.extract_selected).pack(side=tk.LEFT, padx=(12, 0))

        paned = ttk.Panedwindow(self, orient=tk.HORIZONTAL)
        paned.pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=8, pady=6)

        left = ttk.Frame(paned)
        left.grid_rowconfigure(1, weight=1)
        left.grid_columnconfigure(0, weight=1)
        paned.add(left, weight=1)

        ttk.Label(left, text="Assets").grid(row=0, column=0, sticky="w")

        self.tree_assets = ttk.Treeview(left, columns=("count",), show="tree headings", height=20)
        self.tree_assets.heading("#0", text="Asset")
        self.tree_assets.heading("count", text="#Records")
        self.tree_assets.column("#0", width=420, anchor="w")
        self.tree_assets.column("count", width=80, anchor="e")
        self.tree_assets.grid(row=1, column=0, sticky="nsew")

        yscroll_assets = ttk.Scrollbar(left, orient=tk.VERTICAL, command=self.tree_assets.yview)
        self.tree_assets.configure(yscrollcommand=yscroll_assets.set)
        yscroll_assets.grid(row=1, column=1, sticky="ns")
        self.tree_assets.bind("<<TreeviewSelect>>", self.on_select_asset_scope)

        right = ttk.Frame(paned)
        right.grid_rowconfigure(1, weight=1)
        right.grid_columnconfigure(0, weight=1)
        paned.add(right, weight=1)

        ttk.Label(right, text="Hex View").grid(row=0, column=0, sticky="w")

        self.hex_text = tk.Text(right, wrap="none", font=("Consolas", 10))
        self.hex_text.grid(row=1, column=0, sticky="nsew")
        self.hex_text.configure(state="disabled")

        yscroll_hex = ttk.Scrollbar(right, orient=tk.VERTICAL, command=self.hex_text.yview)
        self.hex_text.configure(yscrollcommand=yscroll_hex.set)
        yscroll_hex.grid(row=1, column=1, sticky="ns")

        xscroll_hex = ttk.Scrollbar(right, orient=tk.HORIZONTAL, command=self.hex_text.xview)
        self.hex_text.configure(xscrollcommand=xscroll_hex.set)
        xscroll_hex.grid(row=2, column=0, sticky="ew")

        status_bar = ttk.Frame(self)
        status_bar.pack(side=tk.BOTTOM, fill=tk.X)

        self.var_status = tk.StringVar(value="")
        status = ttk.Label(status_bar, textvariable=self.var_status, relief=tk.SUNKEN, anchor="w")
        status.pack(side=tk.LEFT, fill=tk.X, expand=True)

        self.var_progress = tk.DoubleVar(value=0.0)
        self.progress_bar = ttk.Progressbar(
            status_bar,
            orient=tk.HORIZONTAL,
            mode="determinate",
            variable=self.var_progress,
            maximum=1.0,
            length=220,
        )
        self.progress_bar.pack(side=tk.RIGHT, padx=(8, 4), pady=2)

    def _set_status(self, message: str) -> None:
        self.var_status.set(message)
        self.update_idletasks()

    def _set_progress(self, value: float, maximum: float = 1.0) -> None:
        safe_maximum = maximum if maximum > 0 else 1.0
        self.progress_bar.configure(maximum=safe_maximum)
        self.var_progress.set(max(0.0, min(value, safe_maximum)))
        self.update_idletasks()

    def _reset_progress(self) -> None:
        self.progress_bar.configure(maximum=1.0)
        self.var_progress.set(0.0)
        self.update_idletasks()

    def _clear_filter(self) -> None:
        self.var_filter.set("")
        self.rebuild_asset_tree()

    def open_wad_file(self) -> None:
        wad_paths = filedialog.askopenfilenames(
            title="Select one or more .wad.client files",
            initialdir=_get_default_root_dir(),
            filetypes=[
                ("League WAD archives", "*.wad.client *.wad.mobile *.wad"),
                ("All files", "*.*"),
            ],
        )
        if not wad_paths:
            return

        paths = list(wad_paths)
        self._set_status(f"Loading {len(paths)} archive(s)...")
        self._set_progress(0.0, float(len(paths)))
        self._clear_all()
        self._load_wad_paths(paths)

    def open_folder(self) -> None:
        folder = filedialog.askdirectory(
            title="Select folder containing .wad.client files",
            initialdir=_get_default_root_dir(),
        )
        if not folder:
            return

        self._set_status(f"Scanning folder: {folder}")
        self._clear_all()
        try:
            wad_paths = sorted(
                os.path.join(folder, filename)
                for filename in os.listdir(folder)
                if filename.lower().endswith((".wad.client", ".wad.mobile", ".wad"))
                and os.path.isfile(os.path.join(folder, filename))
            )
            if not wad_paths:
                messagebox.showinfo("WAD Browser", "No WAD archives found in that folder.")
                self._set_status("No WAD archives found.")
                self._reset_progress()
                return
            self._set_progress(0.0, float(len(wad_paths)))
            self._load_wad_paths(wad_paths)
        except Exception as ex:
            messagebox.showerror("Error", f"Failed to scan WAD folder:\n{ex}")
            self._set_status("Scan failed.")
            self._reset_progress()

    def _set_hex_text(self, text: str) -> None:
        self.hex_text.configure(state="normal")
        self.hex_text.delete("1.0", tk.END)
        self.hex_text.insert("1.0", text)
        self.hex_text.configure(state="disabled")

    def _load_wad_paths(self, wad_paths: list[str]) -> None:
        def update_status(message: str) -> None:
            self.after(0, lambda: self._set_status(message))

        def update_progress(current: float, maximum: float) -> None:
            self.after(0, lambda: self._set_progress(current, maximum))

        def show_error(message: str) -> None:
            self.after(0, lambda: messagebox.showerror("WAD Browser", message))

        def worker() -> None:
            try:
                wad_files: list[WadFile] = []
                all_hashes: set[int] = set()

                for index, path in enumerate(wad_paths, 1):
                    update_status(f"Parsing archive {index}/{len(wad_paths)}: {os.path.basename(path)}")
                    wad_file = WadFile(path, self.hash_resolver, self.zstd)
                    wad_file.load_index()
                    wad_files.append(wad_file)
                    all_hashes.update(record.path_hash for record in wad_file.records)
                    update_progress(float(index), float(len(wad_paths)))

                self.hash_resolver.resolve_many(all_hashes, status_callback=update_status)
                for wad_file in wad_files:
                    for record in wad_file.records:
                        record.ensure_name_loaded(self.hash_resolver)

                self.after(0, lambda: self._apply_loaded_wads(wad_files))
            except Exception as ex:
                show_error(f"Failed to load WAD archives:\n{ex}")
                update_status("Load failed.")
                update_progress(0.0, 1.0)

        threading.Thread(target=worker, daemon=True).start()

    def _apply_loaded_wads(self, wad_files: list[WadFile]) -> None:
        self.wad_files = wad_files
        self.wad_files_by_path = {wad_file.path: wad_file for wad_file in wad_files}
        self.rebuild_asset_tree()
        self._set_status(f"Loaded {len(wad_files)} WAD archive(s). Select an asset to preview.")
        self._reset_progress()

    def _clear_all(self) -> None:
        self.wad_files = []
        self.wad_files_by_path = {}
        self.asset_tree_scopes = {}
        self.asset_tree_counts = {}
        self.current_selected_records = []
        self.current_selected_label = ""
        for tree in (self.tree_assets,):
            for item in tree.get_children():
                tree.delete(item)
        self._set_hex_text("")

    def _increment_asset_tree_count(self, item_id: str) -> None:
        self.asset_tree_counts[item_id] = self.asset_tree_counts.get(item_id, 0) + 1

    def _set_asset_scope(self, item_id: str, kind: str, value: object) -> None:
        self.asset_tree_scopes[item_id] = (kind, value)

    def rebuild_asset_tree(self) -> None:
        for item in self.tree_assets.get_children():
            self.tree_assets.delete(item)

        self.asset_tree_scopes.clear()
        self.asset_tree_counts.clear()

        terms = self._parse_filter(self.var_filter.get())
        all_records = [record for wad_file in self.wad_files for record in wad_file.records]
        records = self._filter_records(all_records, terms)

        prefix_nodes: dict[str, str] = {}
        file_nodes: dict[str, str] = {}
        file_records: dict[str, list[WadRecord]] = {}
        unresolved_root_id: str | None = None
        parsed_bin_root_id: str | None = None
        parsed_bin_prefix_nodes: dict[str, str] = {}
        parsed_bin_file_nodes: dict[str, str] = {}
        parsed_bin_file_records: dict[str, list[WadRecord]] = {}

        records.sort(key=lambda record: (record.name is None, (record.name or f"{record.path_hash:016x}").lower()))

        def add_record_path(
            asset_path: str,
            record: WadRecord,
            prefix_node_map: dict[str, str],
            file_node_map: dict[str, str],
            file_record_map: dict[str, list[WadRecord]],
            root_id: str = "",
        ) -> None:
            path_parts = [part for part in asset_path.split("/") if part]
            if not path_parts:
                return

            parent_id = root_id
            prefix_parts: list[str] = []
            folder_path_ids: list[str] = [root_id] if root_id else []

            for part in path_parts[:-1]:
                prefix_parts.append(part)
                prefix = "/".join(prefix_parts) + "/"
                node_id = prefix_node_map.get(prefix)
                if node_id is None:
                    node_id = self.tree_assets.insert(parent_id, "end", text=part, values=(0,))
                    prefix_node_map[prefix] = node_id
                    self._set_asset_scope(node_id, "folder", None)
                parent_id = node_id
                folder_path_ids.append(node_id)

            full_path = "/".join(path_parts)
            leaf_id = file_node_map.get(full_path)
            if leaf_id is None:
                leaf_id = self.tree_assets.insert(parent_id, "end", text=path_parts[-1], values=(0,))
                file_node_map[full_path] = leaf_id
                file_record_map[full_path] = []
                self._set_asset_scope(leaf_id, "record", file_record_map[full_path])

            file_record_map[full_path].append(record)
            for folder_id in folder_path_ids:
                self._increment_asset_tree_count(folder_id)
            self._increment_asset_tree_count(leaf_id)

        for record in records:
            if not record.name:
                if unresolved_root_id is None:
                    unresolved_root_id = self.tree_assets.insert("", "end", text="[unresolved]", values=(0,))
                    self._set_asset_scope(unresolved_root_id, "folder", None)

                leaf_key = f"unresolved/{record.path_hash:016x}"
                leaf_id = file_nodes.get(leaf_key)
                if leaf_id is None:
                    leaf_id = self.tree_assets.insert(
                        unresolved_root_id,
                        "end",
                        text=f"0x{record.path_hash:016X}",
                        values=(0,),
                    )
                    file_nodes[leaf_key] = leaf_id
                    file_records[leaf_key] = []
                    self._set_asset_scope(leaf_id, "record", file_records[leaf_key])

                file_records[leaf_key].append(record)
                self._increment_asset_tree_count(unresolved_root_id)
                self._increment_asset_tree_count(leaf_id)
                continue

            add_record_path(record.name, record, prefix_nodes, file_nodes, file_records)

            derived_skin_path = _try_get_lol_skin_bin_path(record.name)
            if derived_skin_path and derived_skin_path != record.name.lower():
                if parsed_bin_root_id is None:
                    parsed_bin_root_id = self.tree_assets.insert("", "end", text="[parsed .bin]", values=(0,))
                    self._set_asset_scope(parsed_bin_root_id, "folder", None)

                add_record_path(
                    derived_skin_path,
                    record,
                    parsed_bin_prefix_nodes,
                    parsed_bin_file_nodes,
                    parsed_bin_file_records,
                    parsed_bin_root_id,
                )

        for item_id, count in self.asset_tree_counts.items():
            self.tree_assets.item(item_id, values=(count,))

        root_items = self.tree_assets.get_children()
        if root_items:
            self.tree_assets.selection_set(root_items[0])
            self.on_select_asset_scope()
        else:
            self.current_selected_records = []
            self.current_selected_label = ""
            self._set_hex_text("")
            self._set_status("No matching assets.")

    def on_select_asset_scope(self, _event=None) -> None:
        selection = self.tree_assets.selection()
        if not selection:
            return

        item_id = selection[0]
        if item_id not in self.asset_tree_scopes:
            return

        kind, value = self.asset_tree_scopes[item_id]
        label = self.tree_assets.item(item_id, "text")
        if kind != "record":
            count = self.asset_tree_counts.get(item_id, 0)
            self.current_selected_records = []
            self.current_selected_label = label
            self._set_hex_text("")
            self._set_status(f"Folder: {label} | records: {count}")
            return

        records = list(value)
        self.current_selected_records = records
        self.current_selected_label = label
        self._update_record_status(records, label)
        self._load_hex_preview(records, label)

    def _parse_filter(self, text: str):
        text = (text or "").strip()
        if not text:
            return None

        terms = []
        for part in text.split():
            if ":" in part:
                key, value = part.split(":", 1)
                terms.append((key.lower(), value))
            else:
                terms.append(("text", part.lower()))
        return terms

    def _filter_records(self, records: list[WadRecord], terms):
        if not terms:
            return records

        filtered: list[WadRecord] = []
        for record in records:
            match = True
            for key, value in terms:
                if key == "hash":
                    target = int(value, 16) if value.lower().startswith("0x") else int(value, 16)
                    if record.path_hash != target:
                        match = False
                        break
                elif key == "storage":
                    if value.lower() not in record.storage_name.lower():
                        match = False
                        break
                elif key == "text":
                    if not record.matches_text(value):
                        match = False
                        break
                else:
                    if not record.matches_text(value.lower()):
                        match = False
                        break

            if match:
                filtered.append(record)

        return filtered

    def apply_filter(self) -> None:
        self.rebuild_asset_tree()

    def _update_record_status(self, records: list[WadRecord], label: str) -> None:
        if not records:
            self._set_status(f"Record: {label} | no record data")
            return

        primary = records[0]
        duplicate_note = ""
        if len(records) > 1:
            duplicate_note = f" | duplicates: {len(records)}"

        self._set_status(
            f"Record: {label}"
            f" | hash=0x{primary.path_hash:016X}"
            f" | storage={primary.storage_name}"
            f" | compressed={primary.compressed_size:,}"
            f" | size={primary.file_size:,}"
            f" | wad={Path(primary.wad_path).name}"
            f"{duplicate_note}"
        )

    def _get_selected_record(self) -> WadRecord | None:
        if not self.current_selected_records:
            return None
        return self.current_selected_records[0]

    def _load_hex_preview(self, records: list[WadRecord], label: str) -> None:
        self._hex_request_serial += 1
        request_serial = self._hex_request_serial
        self._set_hex_text("Loading preview...")

        def worker() -> None:
            try:
                if not records:
                    preview_text = ""
                else:
                    primary = records[0]
                    wad_file = self.wad_files_by_path.get(primary.wad_path)
                    if wad_file is None:
                        raise RuntimeError(f"WAD archive is not loaded: {primary.wad_path}")

                    data = wad_file.extract_record(primary)
                    header_lines = [
                        f"Name: {primary.name or label}",
                        f"WAD: {Path(primary.wad_path).name}",
                        f"Hash: 0x{primary.path_hash:016X}",
                        f"Storage: {primary.storage_name}",
                        f"Compressed Size: {primary.compressed_size:,}",
                        f"Decoded Size: {len(data):,}",
                    ]
                    if len(records) > 1:
                        header_lines.append(f"Duplicates: {len(records)} record(s), previewing the first one")
                    header_lines.append("")
                    preview_text = "\n".join(header_lines) + format_hex_dump(data)
            except Exception as ex:
                preview_text = f"Failed to load preview:\n{ex}"

            def apply_preview() -> None:
                if request_serial != self._hex_request_serial:
                    return
                self._set_hex_text(preview_text)

            self.after(0, apply_preview)

        threading.Thread(target=worker, daemon=True).start()

    def extract_selected(self) -> None:
        record = self._get_selected_record()
        if not record:
            messagebox.showinfo("Extract", "Select a record first.")
            return

        output_dir = filedialog.askdirectory(title="Select output folder")
        if not output_dir:
            return

        try:
            wad_file = self.wad_files_by_path.get(record.wad_path)
            if wad_file is None:
                raise RuntimeError(f"WAD archive is not loaded: {record.wad_path}")

            data = wad_file.extract_record(record)
            relative_name = record.name or f"{record.path_hash:016x}.bin"
            parts = [_sanitize_path_component(part) for part in relative_name.replace("\\", "/").split("/") if part]
            if not parts:
                parts = [f"{record.path_hash:016x}.bin"]

            output_path = Path(output_dir).joinpath(*parts)
            output_path.parent.mkdir(parents=True, exist_ok=True)
            with output_path.open("wb") as handle:
                handle.write(data)

            self._set_status(f"Extracted: {output_path}")
            messagebox.showinfo("Extract", f"Done:\n{output_path}")
        except Exception as ex:
            messagebox.showerror("Extract", f"Failed:\n{ex}")


if __name__ == "__main__":
    app = WadBrowserApp()
    app.mainloop()
