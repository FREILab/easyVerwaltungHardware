from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path


def _env_int(name: str, default: int) -> int:
    raw = os.getenv(name, "").strip()
    if not raw:
        return default
    try:
        return int(raw)
    except ValueError:
        return default


def _env_bool(name: str, default: bool) -> bool:
    raw = os.getenv(name, "").strip().lower()
    if not raw:
        return default
    return raw in {"1", "true", "yes", "on"}


@dataclass(frozen=True)
class Settings:
    terminal_id: str
    data_dir: Path
    hardware_mode: str
    payment_mode: str
    printer_mode: str
    power_mode: str
    hotend_cooldown_temp_c: float
    device_agent_url: str
    easyverwaltung_url: str
    easyverwaltung_service_id: str
    easyverwaltung_service_token: str
    printers_config_file: Path
    printers_config_url: str
    printers_poll_interval_seconds: int
    slicer_api_key: str
    default_price_per_gram_cents: int
    min_price_per_gram_cents: int
    min_job_price_cents: int
    job_ttl_seconds: int
    max_upload_bytes: int
    feature_topup: bool
    feature_approval: bool
    allow_negative_balance: bool
    max_negative_balance_cents: int
    admin_bv_rfids: tuple[str, ...]
    dev_ui_tools: bool

    @property
    def is_dev_mock(self) -> bool:
        return (
            self.dev_ui_tools
            or self.hardware_mode == "mock"
            or self.payment_mode == "mock"
            or self.printer_mode == "mock"
        )


def load_settings() -> Settings:
    max_upload_mb = max(1, _env_int("MAX_UPLOAD_MB", 250))
    return Settings(
        terminal_id=os.getenv("TERMINAL_ID", "3dprint-01").strip() or "3dprint-01",
        data_dir=Path(os.getenv("DATA_DIR", "/data").strip() or "/data"),
        hardware_mode=os.getenv("HARDWARE_MODE", "mock").strip().lower() or "mock",
        payment_mode=os.getenv("PAYMENT_MODE", "mock").strip().lower() or "mock",
        printer_mode=os.getenv("PRINTER_MODE", "mock").strip().lower() or "mock",
        power_mode=os.getenv("POWER_MODE", "mock").strip().lower() or "mock",
        hotend_cooldown_temp_c=float(os.getenv("HOTEND_COOLDOWN_TEMP_C", "50").strip() or "50"),
        device_agent_url=os.getenv("DEVICE_AGENT_URL", "http://device-agent:8081").rstrip("/"),
        easyverwaltung_url=os.getenv("EASYVERWALTUNG_URL", "").rstrip("/"),
        easyverwaltung_service_id=os.getenv("EASYVERWALTUNG_SERVICE_ID", "").strip(),
        easyverwaltung_service_token=os.getenv("EASYVERWALTUNG_SERVICE_TOKEN", "").strip(),
        printers_config_file=Path(
            os.getenv("PRINTERS_CONFIG_FILE", "/config/printers.dev.json").strip()
            or "/config/printers.dev.json"
        ),
        printers_config_url=os.getenv("PRINTERS_CONFIG_URL", "").strip(),
        printers_poll_interval_seconds=max(3, _env_int("PRINTERS_POLL_INTERVAL_SECONDS", 10)),
        slicer_api_key=os.getenv("SLICER_API_KEY", "").strip(),
        default_price_per_gram_cents=max(1, _env_int("DEFAULT_PRICE_PER_GRAM_CENTS", 4)),
        min_price_per_gram_cents=max(0, _env_int("MIN_PRICE_PER_GRAM_CENTS", 2)),
        min_job_price_cents=max(1, _env_int("MIN_JOB_PRICE_CENTS", 10)),
        job_ttl_seconds=max(30, _env_int("JOB_TTL_SECONDS", 300)),
        max_upload_bytes=max_upload_mb * 1024 * 1024,
        feature_topup=_env_bool("FEATURE_TOPUP", False),
        feature_approval=_env_bool("FEATURE_APPROVAL", False),
        allow_negative_balance=_env_bool("ALLOW_NEGATIVE_BALANCE", True),
        max_negative_balance_cents=max(0, _env_int("MAX_NEGATIVE_BALANCE_CENTS", 2000)),
        admin_bv_rfids=tuple(
            u.strip().upper()
            for u in os.getenv("ADMIN_BV_RFIDS", "04AABBCCDD99").split(",")
            if u.strip()
        ),
        dev_ui_tools=_env_bool("DEV_UI_TOOLS", False),
    )
