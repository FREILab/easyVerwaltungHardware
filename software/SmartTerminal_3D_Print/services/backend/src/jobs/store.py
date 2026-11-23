from __future__ import annotations

import json
import sqlite3
import threading
import time
import uuid
from pathlib import Path
from typing import Any

from jobs.parser import derive_display_name


class JobStore:
    """SQLite (WAL-Mode) persistent staging queue + audit log + upload file management."""

    def __init__(self, data_dir: Path) -> None:
        self.data_dir = data_dir
        self.uploads_dir = data_dir / "uploads"
        self.thumbs_dir = data_dir / "thumbnails"
        self.db_path = data_dir / "terminal.db"
        self._lock = threading.RLock()

        self.uploads_dir.mkdir(parents=True, exist_ok=True)
        self.thumbs_dir.mkdir(parents=True, exist_ok=True)
        self._init_db()

    def _connect(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.db_path, timeout=5.0)
        conn.row_factory = sqlite3.Row
        conn.execute("PRAGMA journal_mode=WAL;")
        conn.execute("PRAGMA synchronous=NORMAL;")
        conn.execute("PRAGMA busy_timeout=5000;")
        return conn

    def _init_db(self) -> None:
        with self._lock, self._connect() as conn:
            conn.executescript(
                """
                CREATE TABLE IF NOT EXISTS jobs (
                    id TEXT PRIMARY KEY,
                    printer_slot INTEGER NOT NULL,
                    printer_id TEXT NOT NULL,
                    printer_name TEXT NOT NULL,
                    filename TEXT NOT NULL,
                    file_path TEXT NOT NULL,
                    file_size INTEGER NOT NULL,
                    thumbnail_path TEXT,
                    thumbnail_ext TEXT,
                    filament_g REAL NOT NULL DEFAULT 0,
                    filament_type TEXT NOT NULL DEFAULT 'Unbekannt',
                    estimated_time_s INTEGER NOT NULL DEFAULT 0,
                    price_cents INTEGER NOT NULL,
                    slicer_price_cents INTEGER,
                    price_source TEXT NOT NULL,
                    print_after_upload INTEGER NOT NULL DEFAULT 1,
                    status TEXT NOT NULL,
                    created_at REAL NOT NULL,
                    expires_at REAL NOT NULL,
                    started_at REAL,
                    payment_mode TEXT,
                    payment_ref TEXT,
                    user_display_name TEXT,
                    section_id TEXT,
                    section_name TEXT
                );

                CREATE INDEX IF NOT EXISTS idx_jobs_status_created
                    ON jobs (status, created_at DESC);

                CREATE TABLE IF NOT EXISTS audit_log (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    ts REAL NOT NULL,
                    event_type TEXT NOT NULL,
                    job_id TEXT,
                    printer_id TEXT,
                    user_id TEXT,
                    user_name TEXT,
                    amount_cents INTEGER,
                    details_json TEXT
                );

                CREATE INDEX IF NOT EXISTS idx_audit_ts ON audit_log (ts DESC);

                CREATE TABLE IF NOT EXISTS printer_power_state (
                    printer_id TEXT PRIMARY KEY,
                    power_state TEXT NOT NULL DEFAULT 'ON',
                    pending_shutdown INTEGER NOT NULL DEFAULT 0,
                    updated_at REAL NOT NULL
                );
                """
            )
            # Safe migration for existing SQLite DBs
            cols = {r["name"] for r in conn.execute("PRAGMA table_info(jobs)").fetchall()}
            if "print_after_upload" not in cols:
                conn.execute("ALTER TABLE jobs ADD COLUMN print_after_upload INTEGER NOT NULL DEFAULT 1")
            # Recover any jobs stuck in 'sending' after a sudden power loss
            conn.execute(
                "UPDATE jobs SET status = 'queued' WHERE status = 'sending' AND expires_at > ?",
                (time.time(),),
            )
        self.cleanup_expired_and_orphans()

    def new_job_id(self) -> str:
        return f"job_{uuid.uuid4().hex[:12]}"

    def add_job(
        self,
        *,
        job_id: str,
        printer_slot: int,
        printer_id: str,
        printer_name: str,
        filename: str,
        file_path: Path,
        file_size: int,
        thumbnail_path: Path | None,
        thumbnail_ext: str | None,
        filament_g: float,
        filament_type: str,
        estimated_time_s: int,
        price_cents: int,
        slicer_price_cents: int | None,
        price_source: str,
        ttl_seconds: int,
        print_after_upload: bool = True,
    ) -> dict[str, Any]:
        now = time.time()
        expires_at = now + ttl_seconds
        with self._lock, self._connect() as conn:
            conn.execute(
                """
                INSERT INTO jobs (
                    id, printer_slot, printer_id, printer_name, filename,
                    file_path, file_size, thumbnail_path, thumbnail_ext,
                    filament_g, filament_type, estimated_time_s,
                    price_cents, slicer_price_cents, price_source,
                    print_after_upload, status, created_at, expires_at
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'queued', ?, ?)
                """,
                (
                    job_id,
                    printer_slot,
                    printer_id,
                    printer_name,
                    filename,
                    str(file_path),
                    file_size,
                    str(thumbnail_path) if thumbnail_path else None,
                    thumbnail_ext,
                    filament_g,
                    filament_type,
                    estimated_time_s,
                    price_cents,
                    slicer_price_cents,
                    price_source,
                    1 if print_after_upload else 0,
                    now,
                    expires_at,
                ),
            )
        self.log_event(
            "job_uploaded",
            job_id=job_id,
            printer_id=printer_id,
            amount_cents=price_cents,
            details={
                "filename": filename,
                "filament_g": filament_g,
                "estimated_time_s": estimated_time_s,
                "price_source": price_source,
            },
        )
        return self.get_job(job_id)  # type: ignore[return-value]

    def get_job(self, job_id: str) -> dict[str, Any] | None:
        with self._lock, self._connect() as conn:
            row = conn.execute("SELECT * FROM jobs WHERE id = ?", (job_id,)).fetchone()
        return self._row_to_dict(row) if row else None

    def list_queued_jobs(self) -> list[dict[str, Any]]:
        now = time.time()
        with self._lock, self._connect() as conn:
            rows = conn.execute(
                """
                SELECT * FROM jobs
                WHERE status = 'sending' OR (status = 'queued' AND expires_at > ?)
                ORDER BY created_at DESC
                """,
                (now,),
            ).fetchall()
        return [self._row_to_dict(r) for r in rows]

    def set_job_status(self, job_id: str, status: str) -> bool:
        with self._lock, self._connect() as conn:
            cur = conn.execute("UPDATE jobs SET status = ? WHERE id = ?", (status, job_id))
            return cur.rowcount > 0

    def mark_job_started(
        self,
        job_id: str,
        *,
        payment_mode: str,
        payment_ref: str | None,
        user_display_name: str,
        section_id: str | None = None,
        section_name: str | None = None,
    ) -> dict[str, Any] | None:
        job = self.get_job(job_id)
        if not job:
            return None
        now = time.time()
        with self._lock, self._connect() as conn:
            conn.execute(
                """
                UPDATE jobs
                SET status = 'started', started_at = ?, payment_mode = ?,
                    payment_ref = ?, user_display_name = ?,
                    section_id = ?, section_name = ?
                WHERE id = ?
                """,
                (now, payment_mode, payment_ref, user_display_name, section_id, section_name, job_id),
            )
        self._delete_job_files(job)
        return self.get_job(job_id)

    def mark_job_failed(self, job_id: str) -> bool:
        job = self.get_job(job_id)
        if not job:
            return False
        self.set_job_status(job_id, "failed")
        self._delete_job_files(job)
        return True

    def cancel_job(self, job_id: str) -> bool:
        job = self.get_job(job_id)
        if not job or job["status"] not in {"queued", "sending"}:
            return False
        self.set_job_status(job_id, "cancelled")
        self._delete_job_files(job)
        self.log_event("job_cancelled", job_id=job_id, printer_id=job["printerId"])
        return True

    def cleanup_expired_and_orphans(self) -> int:
        now = time.time()
        expired_rows: list[dict[str, Any]] = []
        active_files: set[Path] = set()

        with self._lock, self._connect() as conn:
            rows = conn.execute(
                "SELECT * FROM jobs WHERE status = 'queued' AND expires_at <= ?",
                (now,),
            ).fetchall()
            for r in rows:
                expired_rows.append(self._row_to_dict(r))
            if rows:
                conn.execute(
                    "UPDATE jobs SET status = 'expired' WHERE status = 'queued' AND expires_at <= ?",
                    (now,),
                )
            active = conn.execute(
                "SELECT file_path, thumbnail_path FROM jobs WHERE status IN ('queued', 'sending')"
            ).fetchall()
            for a in active:
                if a["file_path"]:
                    active_files.add(Path(a["file_path"]).resolve())
                if a["thumbnail_path"]:
                    active_files.add(Path(a["thumbnail_path"]).resolve())

        for job in expired_rows:
            self._delete_job_files(job)
            self.log_event("job_expired", job_id=job["id"], printer_id=job["printerId"])

        # Sweep orphaned files older than 30 s (avoid racing an active upload)
        cutoff = now - 30
        for folder in (self.uploads_dir, self.thumbs_dir):
            if not folder.exists():
                continue
            for p in folder.iterdir():
                if not p.is_file():
                    continue
                try:
                    if p.resolve() not in active_files and p.stat().st_mtime < cutoff:
                        p.unlink(missing_ok=True)
                except OSError:
                    pass

        return len(expired_rows)

    def log_event(
        self,
        event_type: str,
        *,
        job_id: str | None = None,
        printer_id: str | None = None,
        user_id: str | None = None,
        user_name: str | None = None,
        amount_cents: int | None = None,
        details: dict[str, Any] | None = None,
    ) -> None:
        with self._lock, self._connect() as conn:
            conn.execute(
                """
                INSERT INTO audit_log (ts, event_type, job_id, printer_id, user_id, user_name, amount_cents, details_json)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    time.time(),
                    event_type,
                    job_id,
                    printer_id,
                    user_id,
                    user_name,
                    amount_cents,
                    json.dumps(details or {}, ensure_ascii=False),
                ),
            )

    def recent_audit(self, limit: int = 20) -> list[dict[str, Any]]:
        with self._lock, self._connect() as conn:
            rows = conn.execute(
                "SELECT * FROM audit_log ORDER BY ts DESC LIMIT ?",
                (limit,),
            ).fetchall()
        return [
            {
                "id": r["id"],
                "ts": r["ts"],
                "eventType": r["event_type"],
                "jobId": r["job_id"],
                "printerId": r["printer_id"],
                "userName": r["user_name"],
                "amountCents": r["amount_cents"],
                "details": json.loads(r["details_json"] or "{}"),
            }
            for r in rows
        ]

    def printer_lifetime_stats(self) -> dict[str, dict[str, float]]:
        with self._lock, self._connect() as conn:
            rows = conn.execute(
                """
                SELECT printer_id,
                       COALESCE(SUM(estimated_time_s), 0) AS total_s,
                       COALESCE(SUM(filament_g), 0) AS total_g
                FROM jobs
                WHERE status = 'started'
                GROUP BY printer_id
                """
            ).fetchall()
        return {
            r["printer_id"]: {
                "hours": float(r["total_s"]) / 3600.0,
                "grams": float(r["total_g"]),
            }
            for r in rows
        }

    def list_print_history(self, limit: int = 50) -> list[dict[str, Any]]:
        """Return recent started/failed prints for the BV Admin Print History panel."""
        with self._lock, self._connect() as conn:
            rows = conn.execute(
                """
                SELECT id, printer_slot, printer_id, printer_name, filename,
                       filament_g, filament_type, estimated_time_s, price_cents,
                       status, created_at, started_at, payment_mode,
                       user_display_name, section_name, print_after_upload
                FROM jobs
                WHERE status IN ('started', 'failed')
                ORDER BY COALESCE(started_at, created_at) DESC
                LIMIT ?
                """,
                (max(1, min(200, int(limit))),),
            ).fetchall()
        return [
            {
                "id": r["id"],
                "printerSlot": r["printer_slot"],
                "printerId": r["printer_id"],
                "printerName": r["printer_name"],
                "filename": r["filename"],
                "displayName": derive_display_name(r["filename"]),
                "filamentGrams": float(r["filament_g"] or 0.0),
                "filamentType": r["filament_type"] or "PLA",
                "estimatedTimeSeconds": int(r["estimated_time_s"] or 0),
                "priceCents": int(r["price_cents"] or 0),
                "status": r["status"],
                "startedAt": float(r["started_at"] or r["created_at"] or 0.0),
                "paymentMode": r["payment_mode"] or "personal",
                "userDisplayName": r["user_display_name"] or "Unbekannt",
                "sectionName": r["section_name"],
                "printAfterUpload": bool(r["print_after_upload"]),
            }
            for r in rows
        ]

    def latest_started_jobs_by_printer(self) -> dict[str, dict[str, Any]]:
        """Return the most recently started job per printer (for displaying owner & fallback ETA)."""
        with self._lock, self._connect() as conn:
            rows = conn.execute(
                """
                SELECT j.printer_id, j.filename, j.user_display_name, j.section_name,
                       j.payment_mode, j.started_at, j.estimated_time_s
                FROM jobs j
                INNER JOIN (
                    SELECT printer_id, MAX(started_at) AS max_started
                    FROM jobs
                    WHERE status = 'started'
                    GROUP BY printer_id
                ) m ON j.printer_id = m.printer_id AND j.started_at = m.max_started
                """
            ).fetchall()
        return {
            r["printer_id"]: {
                "filename": r["filename"],
                "displayName": derive_display_name(r["filename"]),
                "userDisplayName": r["user_display_name"] or "",
                "sectionName": r["section_name"],
                "paymentMode": r["payment_mode"],
                "startedAt": float(r["started_at"] or 0.0),
                "estimatedTimeSeconds": int(r["estimated_time_s"] or 0),
            }
            for r in rows
        }

    def get_power_state(self, printer_id: str) -> dict[str, Any]:
        with self._lock, self._connect() as conn:
            row = conn.execute(
                "SELECT power_state, pending_shutdown, updated_at FROM printer_power_state WHERE printer_id = ?",
                (printer_id,),
            ).fetchone()
        if not row:
            return {"powerState": "ON", "pendingShutdown": False, "updatedAt": 0.0}
        return {
            "powerState": str(row["power_state"] or "ON"),
            "pendingShutdown": bool(row["pending_shutdown"]),
            "updatedAt": float(row["updated_at"] or 0.0),
        }

    def set_power_state(
        self,
        printer_id: str,
        *,
        power_state: str | None = None,
        pending_shutdown: bool | None = None,
    ) -> dict[str, Any]:
        cur = self.get_power_state(printer_id)
        new_ps = power_state if power_state is not None else cur["powerState"]
        new_pend = pending_shutdown if pending_shutdown is not None else cur["pendingShutdown"]
        now = time.time()
        with self._lock, self._connect() as conn:
            conn.execute(
                """
                INSERT INTO printer_power_state (printer_id, power_state, pending_shutdown, updated_at)
                VALUES (?, ?, ?, ?)
                ON CONFLICT(printer_id) DO UPDATE SET
                    power_state = excluded.power_state,
                    pending_shutdown = excluded.pending_shutdown,
                    updated_at = excluded.updated_at
                """,
                (printer_id, new_ps, 1 if new_pend else 0, now),
            )
        return {"powerState": new_ps, "pendingShutdown": new_pend, "updatedAt": now}

    def health_ok(self) -> bool:
        try:
            with self._lock, self._connect() as conn:
                conn.execute("SELECT 1").fetchone()
            return True
        except Exception:
            return False

    def _delete_job_files(self, job: dict[str, Any]) -> None:
        for key in ("filePath", "thumbnailPath"):
            val = job.get(key)
            if val:
                try:
                    Path(val).unlink(missing_ok=True)
                except OSError:
                    pass

    @staticmethod
    def _row_to_dict(row: sqlite3.Row) -> dict[str, Any]:
        now = time.time()
        remaining = max(0, int(round(row["expires_at"] - now)))
        return {
            "id": row["id"],
            "printerSlot": row["printer_slot"],
            "printerId": row["printer_id"],
            "printerName": row["printer_name"],
            "filename": row["filename"],
            "displayName": derive_display_name(row["filename"]),
            "filePath": row["file_path"],
            "fileSize": row["file_size"],
            "thumbnailPath": row["thumbnail_path"],
            "thumbnailExt": row["thumbnail_ext"],
            "hasThumbnail": bool(row["thumbnail_path"]),
            "thumbnailUrl": f"/api/jobs/{row['id']}/thumbnail" if row["thumbnail_path"] else None,
            "filamentGrams": row["filament_g"],
            "filamentType": row["filament_type"],
            "estimatedTimeSeconds": row["estimated_time_s"],
            "priceCents": row["price_cents"],
            "slicerPriceCents": row["slicer_price_cents"],
            "priceSource": row["price_source"],
            "printAfterUpload": bool(row["print_after_upload"]),
            "status": row["status"],
            "createdAt": row["created_at"],
            "expiresAt": row["expires_at"],
            "remainingSeconds": remaining,
        }
