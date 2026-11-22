from __future__ import annotations

import base64
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from jobs.parser import compute_job_price, derive_display_name, parse_duration_seconds, parse_print_file
from jobs.store import JobStore
from printer.client import map_prusalink_state
from printer.emulation import _parse_print_after_upload


def _make_bgcode(print_meta_ini: str, png_bytes: bytes, compress_meta: bool = False) -> bytes:
    """Build a valid minimal Prusa .bgcode v1 file with metadata, skipped gcode, and PNG thumbnail."""
    out = bytearray()
    out += b"GCDE"
    out += struct.pack("<IH", 1, 1)  # version=1, checksum=CRC32

    def add_block(btype: int, compression: int, params: bytes, raw_payload: bytes) -> None:
        if compression == 1:
            payload = zlib.compress(raw_payload)
            header = struct.pack("<HHII", btype, compression, len(raw_payload), len(payload))
        else:
            payload = raw_payload
            header = struct.pack("<HHI", btype, 0, len(raw_payload))
        block = header + params + payload
        crc = zlib.crc32(block) & 0xFFFFFFFF
        out.extend(block)
        out.extend(struct.pack("<I", crc))

    # G-Code block (type 1) — should be skipped cleanly without decompression
    add_block(1, 0, struct.pack("<H", 0), b"G28\nG1 X10 Y10\n" * 50)
    # Thumbnail block (type 5): format=0 (PNG), 64x64
    add_block(5, 0, struct.pack("<HHH", 0, 64, 64), png_bytes)
    # Print Metadata block (type 4): encoding=0 (INI)
    add_block(4, 1 if compress_meta else 0, struct.pack("<H", 0), print_meta_ini.encode("utf-8"))
    return bytes(out)


class Phase1ParserAndStoreTests(unittest.TestCase):
    def test_duration_parser(self) -> None:
        self.assertEqual(parse_duration_seconds("47m 12s"), 47 * 60 + 12)
        self.assertEqual(parse_duration_seconds("1d 2h 3m 4s"), 86400 + 7200 + 180 + 4)
        self.assertEqual(parse_duration_seconds("120"), 120)

    def test_display_name_shortening(self) -> None:
        self.assertEqual(derive_display_name("Shape-Box_0.4n_0.2mm_PLA_MINIIS_20m.gcode"), "Shape-Box")
        self.assertEqual(derive_display_name("3DBenchy_0.2mm_PETG_MK4S_47m.bgcode"), "3DBenchy")
        self.assertEqual(derive_display_name("Gehaeuse_v2_0.6n_0.30mm_PETG_XL_2h10m.bgcode"), "Gehaeuse_v2")
        self.assertEqual(derive_display_name("mein_teil.gcode"), "mein_teil")

    def test_gcode_text_parser_and_pricing(self) -> None:
        png = base64.b64decode(
            "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=="
        )
        b64 = base64.b64encode(png).decode("ascii")
        gcode = (
            f"; thumbnail begin 16x16 {len(b64)}\n; {b64}\n; thumbnail end\n"
            "G28\n"
            "; filament used [g] = 12.5, 7.5\n"
            "; total filament cost = 0.80\n"
            "; estimated printing time (normal mode) = 1h 15m 30s\n"
            "; filament_type = PETG;PLA\n"
        )
        with tempfile.TemporaryDirectory() as td:
            p = Path(td) / "xl_multi.gcode"
            p.write_text(gcode, encoding="utf-8")
            meta = parse_print_file(p)

        self.assertAlmostEqual(meta.filament_g, 20.0)
        self.assertEqual(meta.slicer_cost_cents, 80)
        self.assertEqual(meta.estimated_time_s, 4530)
        self.assertEqual(meta.filament_type, "PETG + PLA")
        self.assertEqual(meta.thumbnail_bytes, png)

        # Slicer price accepted when above floor
        pr = compute_job_price(meta, 4, 2, 10)
        self.assertEqual(pr.price_cents, 80)
        self.assertEqual(pr.price_source, "slicer")

        # Tampered low slicer price gets clamped to min floor (20g * 2ct = 40ct)
        meta.slicer_cost_cents = 5
        pr_clamped = compute_job_price(meta, 4, 2, 10)
        self.assertEqual(pr_clamped.price_cents, 40)
        self.assertEqual(pr_clamped.price_source, "clamped_min")

        # Missing slicer price falls back to default (20g * 4ct = 80ct)
        meta.slicer_cost_cents = None
        pr_fb = compute_job_price(meta, 4, 2, 10)
        self.assertEqual(pr_fb.price_cents, 80)
        self.assertEqual(pr_fb.price_source, "fallback")

    def test_bgcode_parser_with_deflate_metadata_and_thumbnail(self) -> None:
        png = b"\x89PNG\r\n\x1a\nFAKEPNG"
        ini = (
            "filament used [g]=35.4\n"
            "total filament cost=1.42\n"
            "estimated printing time (normal mode)=2h 10m 5s\n"
            "filament_type=PETG\n"
        )
        raw = _make_bgcode(ini, png, compress_meta=True)
        with tempfile.TemporaryDirectory() as td:
            p = Path(td) / "part.bgcode"
            p.write_bytes(raw)
            meta = parse_print_file(p)

        self.assertAlmostEqual(meta.filament_g, 35.4)
        self.assertEqual(meta.slicer_cost_cents, 142)
        self.assertEqual(meta.estimated_time_s, 2 * 3600 + 10 * 60 + 5)
        self.assertEqual(meta.filament_type, "PETG")
        self.assertEqual(meta.thumbnail_bytes, png)

    def test_job_store_wal_and_expiry_cleanup(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            store = JobStore(Path(td))
            self.assertTrue(store.health_ok())
            f = store.uploads_dir / "job_test1.gcode"
            f.write_text("G28\n", encoding="utf-8")
            store.add_job(
                job_id="job_test1",
                printer_slot=1,
                printer_id="prusa-01",
                printer_name="Drucker 1",
                filename="benchy.gcode",
                file_path=f,
                file_size=4,
                thumbnail_path=None,
                thumbnail_ext=None,
                filament_g=10.0,
                filament_type="PLA",
                estimated_time_s=600,
                price_cents=40,
                slicer_price_cents=40,
                price_source="slicer",
                ttl_seconds=-1,  # already expired
            )
            expired = store.cleanup_expired_and_orphans()
            self.assertEqual(expired, 1)
            self.assertFalse(f.exists())
            self.assertEqual(store.list_queued_jobs(), [])

    def test_prusalink_state_mapping_bed_safety(self) -> None:
        self.assertEqual(map_prusalink_state("IDLE"), ("READY", None))
        self.assertEqual(map_prusalink_state("PRINTING"), ("PRINTING", None))
        self.assertEqual(map_prusalink_state("PAUSED"), ("PAUSED", None))
        # FINISHED must NOT be READY so no print starts onto an uncleared print bed
        state, note = map_prusalink_state("FINISHED")
        self.assertEqual(state, "ATTENTION")
        self.assertIsNotNone(note)
        self.assertEqual(map_prusalink_state("ATTENTION"), ("ATTENTION", "Dialog am Drucker"))

    def test_print_after_upload_parsing_and_store_stats(self) -> None:
        self.assertTrue(_parse_print_after_upload("?1"))
        self.assertTrue(_parse_print_after_upload("true"))
        self.assertTrue(_parse_print_after_upload("1"))
        self.assertFalse(_parse_print_after_upload("?0"))
        self.assertFalse(_parse_print_after_upload("false"))
        self.assertFalse(_parse_print_after_upload("0"))
        self.assertFalse(_parse_print_after_upload(None))
        self.assertFalse(_parse_print_after_upload(""))

        with tempfile.TemporaryDirectory() as td:
            store = JobStore(Path(td))
            f = store.uploads_dir / "xl_part.gcode"
            f.write_text("G28\n", encoding="utf-8")
            job = store.add_job(
                job_id="job_xl_1",
                printer_slot=1,
                printer_id="prusa-xl",
                printer_name="Prusa XL",
                filename="xl_part.gcode",
                file_path=f,
                file_size=4,
                thumbnail_path=None,
                thumbnail_ext=None,
                filament_g=100.0,
                filament_type="PLA",
                estimated_time_s=3600,
                price_cents=400,
                slicer_price_cents=400,
                price_source="slicer",
                ttl_seconds=300,
                print_after_upload=False,
            )
            self.assertFalse(job["printAfterUpload"])
            store.mark_job_started(
                "job_xl_1",
                payment_mode="personal",
                payment_ref="ref_1",
                user_display_name="Max",
            )
            stats = store.printer_lifetime_stats()
            self.assertIn("prusa-xl", stats)
            self.assertAlmostEqual(stats["prusa-xl"]["hours"], 1.0, places=2)
            self.assertAlmostEqual(stats["prusa-xl"]["grams"], 100.0, places=2)

            latest = store.latest_started_jobs_by_printer()
            self.assertEqual(latest["prusa-xl"]["userDisplayName"], "Max")

            p0 = store.get_power_state("prusa-xl")
            self.assertEqual(p0["powerState"], "ON")
            self.assertFalse(p0["pendingShutdown"])
            store.set_power_state("prusa-xl", pending_shutdown=True)
            p1 = store.get_power_state("prusa-xl")
            self.assertTrue(p1["pendingShutdown"])
            store.set_power_state("prusa-xl", power_state="OFF", pending_shutdown=False)
            p2 = store.get_power_state("prusa-xl")
            self.assertEqual(p2["powerState"], "OFF")
            self.assertFalse(p2["pendingShutdown"])


if __name__ == "__main__":
    unittest.main()
