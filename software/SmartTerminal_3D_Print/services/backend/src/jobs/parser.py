from __future__ import annotations

import base64
import re
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path


@dataclass
class PrintMetadata:
    filament_g: float = 0.0
    filament_type: str = "Unbekannt"
    estimated_time_s: int = 0
    slicer_cost_cents: int | None = None
    printer_model: str = ""
    thumbnail_bytes: bytes | None = None
    thumbnail_ext: str = "png"


@dataclass
class PriceResult:
    price_cents: int
    slicer_price_cents: int | None
    price_source: str  # "slicer" | "clamped_min" | "fallback"


_TIME_RE = re.compile(
    r"(?:(?P<d>\d+)\s*d)?\s*(?:(?P<h>\d+)\s*h)?\s*(?:(?P<m>\d+)\s*m)?\s*(?:(?P<s>\d+)\s*s)?",
    re.IGNORECASE,
)
_NUM_RE = re.compile(r"[-+]?\d+(?:\.\d+)?")
_THUMB_BEGIN_RE = re.compile(r";\s*thumbnail(?:_JPG|_QOI)?\s+begin\s+(\d+)x(\d+)\s+(\d+)", re.IGNORECASE)
_THUMB_END_RE = re.compile(r";\s*thumbnail(?:_JPG|_QOI)?\s+end", re.IGNORECASE)


_SLICER_SUFFIX_RE = re.compile(
    r"_(?:\d+(?:\.\d+)?n_)?\d+(?:\.\d+)?mm_.*$",
    re.IGNORECASE,
)


def derive_display_name(filename: str) -> str:
    """Shorten PrusaSlicer filenames like 'Shape-Box_0.4n_0.2mm_PLA_MINIIS_20m.gcode' -> 'Shape-Box'."""
    stem = Path(filename).stem.strip()
    if not stem:
        return filename
    shortened = _SLICER_SUFFIX_RE.sub("", stem).strip(" _-")
    return shortened or stem


def parse_duration_seconds(raw: str) -> int:
    text = raw.strip()
    if not text:
        return 0
    if text.isdigit():
        return int(text)
    total = 0
    for match in _TIME_RE.finditer(text):
        d = int(match.group("d") or 0)
        h = int(match.group("h") or 0)
        m = int(match.group("m") or 0)
        s = int(match.group("s") or 0)
        val = d * 86400 + h * 3600 + m * 60 + s
        if val > total:
            total = val
    return total


def _sum_numbers(raw: str) -> float:
    nums = [float(x) for x in _NUM_RE.findall(raw)]
    return round(sum(nums), 2) if nums else 0.0


def _normalize_filament_type(raw: str) -> str:
    parts = [p.strip().strip('"').strip("'") for p in re.split(r"[;,]", raw) if p.strip()]
    unique: list[str] = []
    for p in parts:
        if p and p not in unique:
            unique.append(p)
    return " + ".join(unique) if unique else "Unbekannt"


def _apply_kv(meta: PrintMetadata, key: str, value: str) -> None:
    k = key.strip().lower()
    v = value.strip()
    if not v:
        return

    if k in {"filament used [g]", "total filament used [g]"}:
        grams = _sum_numbers(v)
        if grams > 0 and (k.startswith("total") or meta.filament_g == 0.0):
            meta.filament_g = grams
    elif k in {"total filament cost", "filament cost"}:
        cost_eur = _sum_numbers(v)
        if cost_eur > 0 and (k.startswith("total") or meta.slicer_cost_cents is None):
            meta.slicer_cost_cents = int(round(cost_eur * 100))
    elif k in {
        "estimated printing time (normal mode)",
        "estimated printing time (silent mode)",
        "estimated printing time",
    }:
        secs = parse_duration_seconds(v)
        if secs > 0 and ("normal" in k or meta.estimated_time_s == 0):
            meta.estimated_time_s = secs
    elif k == "filament_type":
        meta.filament_type = _normalize_filament_type(v)
    elif k == "printer_model":
        meta.printer_model = v.strip('"').strip("'")


def _parse_gcode_text(file_path: Path) -> PrintMetadata:
    meta = PrintMetadata()
    size = file_path.stat().st_size

    head_size = min(size, 512 * 1024)
    tail_size = min(size, 512 * 1024)

    with file_path.open("rb") as f:
        head_bytes = f.read(head_size)
        if size > head_size:
            f.seek(max(0, size - tail_size))
            tail_bytes = f.read(tail_size)
        else:
            tail_bytes = b""

    head_text = head_bytes.decode("utf-8", errors="ignore")
    tail_text = tail_bytes.decode("utf-8", errors="ignore")

    # Extract largest PNG/JPG thumbnail from header
    best_thumb: tuple[int, bytes, str] | None = None
    in_thumb = False
    thumb_lines: list[str] = []
    thumb_area = 0
    thumb_ext = "png"

    for line in head_text.splitlines():
        s = line.strip()
        if not s.startswith(";"):
            continue
        if in_thumb:
            if _THUMB_END_RE.match(s):
                in_thumb = False
                if thumb_ext in {"png", "jpg"}:
                    try:
                        decoded = base64.b64decode("".join(thumb_lines))
                        if decoded and (best_thumb is None or thumb_area > best_thumb[0]):
                            best_thumb = (thumb_area, decoded, thumb_ext)
                    except Exception:
                        pass
                thumb_lines = []
            else:
                thumb_lines.append(s[1:].strip())
            continue

        m = _THUMB_BEGIN_RE.match(s)
        if m:
            w, h = int(m.group(1)), int(m.group(2))
            lower = s.lower()
            if "thumbnail_jpg" in lower:
                thumb_ext = "jpg"
            elif "thumbnail_qoi" in lower:
                thumb_ext = "qoi"
            else:
                thumb_ext = "png"
            thumb_area = w * h
            thumb_lines = []
            in_thumb = True
            continue

        if "=" in s:
            key, val = s[1:].split("=", 1)
            _apply_kv(meta, key, val)

    for line in tail_text.splitlines():
        s = line.strip()
        if s.startswith(";") and "=" in s:
            key, val = s[1:].split("=", 1)
            _apply_kv(meta, key, val)

    if best_thumb is not None:
        meta.thumbnail_bytes = best_thumb[1]
        meta.thumbnail_ext = best_thumb[2]

    return meta


def _parse_bgcode(file_path: Path) -> PrintMetadata:
    """Parse Prusa Binary G-Code (.bgcode) metadata & thumbnail blocks without decompressing G-Code."""
    meta = PrintMetadata()
    best_thumb: tuple[int, bytes, str] | None = None

    with file_path.open("rb") as f:
        header = f.read(10)
        if len(header) < 10 or header[:4] != b"GCDE":
            return meta
        _version, checksum_type = struct.unpack("<IH", header[4:10])
        checksum_len = 4 if checksum_type == 1 else 0

        while True:
            bh = f.read(8)
            if len(bh) < 8:
                break
            block_type, compression, uncompressed_size = struct.unpack("<HHI", bh)
            if compression != 0:
                cs_bytes = f.read(4)
                if len(cs_bytes) < 4:
                    break
                data_size = struct.unpack("<I", cs_bytes)[0]
            else:
                data_size = uncompressed_size

            # Block parameter sizes per Prusa libbgcode spec
            if block_type in {0, 1, 2, 3, 4}:
                params = f.read(2)
                if len(params) < 2:
                    break
            elif block_type == 5:  # Thumbnail
                params = f.read(6)
                if len(params) < 6:
                    break
            else:
                # Unknown block type — stop safely
                break

            # Skip G-Code blocks (type 1) without reading into memory
            if block_type == 1:
                f.seek(data_size + checksum_len, 1)
                continue

            raw_data = f.read(data_size)
            if checksum_len:
                f.seek(checksum_len, 1)

            if len(raw_data) < data_size:
                break

            if block_type in {0, 2, 3, 4}:
                payload: bytes | None = None
                if compression == 0:
                    payload = raw_data
                elif compression == 1:
                    try:
                        payload = zlib.decompress(raw_data)
                    except zlib.error:
                        payload = None

                if payload:
                    for line in payload.decode("utf-8", errors="ignore").splitlines():
                        if "=" in line:
                            key, val = line.split("=", 1)
                            _apply_kv(meta, key, val)

            elif block_type == 5:
                fmt, width, height = struct.unpack("<HHH", params)
                # 0 = PNG, 1 = JPG, 2 = QOI
                if fmt in {0, 1} and compression == 0:
                    ext = "png" if fmt == 0 else "jpg"
                    area = width * height
                    if best_thumb is None or area > best_thumb[0]:
                        best_thumb = (area, raw_data, ext)

    if best_thumb is not None:
        meta.thumbnail_bytes = best_thumb[1]
        meta.thumbnail_ext = best_thumb[2]

    return meta


def parse_print_file(file_path: Path) -> PrintMetadata:
    with file_path.open("rb") as f:
        magic = f.read(4)
    if magic == b"GCDE":
        return _parse_bgcode(file_path)
    return _parse_gcode_text(file_path)


def compute_job_price(
    meta: PrintMetadata,
    default_price_per_gram_cents: int,
    min_price_per_gram_cents: int,
    min_job_price_cents: int,
) -> PriceResult:
    grams = max(0.0, meta.filament_g)
    fallback_cents = max(min_job_price_cents, int(round(grams * default_price_per_gram_cents)))
    floor_cents = max(min_job_price_cents, int(round(grams * min_price_per_gram_cents)))

    if meta.slicer_cost_cents is None or meta.slicer_cost_cents <= 0:
        return PriceResult(
            price_cents=fallback_cents,
            slicer_price_cents=meta.slicer_cost_cents,
            price_source="fallback",
        )

    if meta.slicer_cost_cents < floor_cents:
        return PriceResult(
            price_cents=floor_cents,
            slicer_price_cents=meta.slicer_cost_cents,
            price_source="clamped_min",
        )

    return PriceResult(
        price_cents=meta.slicer_cost_cents,
        slicer_price_cents=meta.slicer_cost_cents,
        price_source="slicer",
    )
