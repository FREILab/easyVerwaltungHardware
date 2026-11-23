from __future__ import annotations

import asyncio
import logging
from collections.abc import AsyncIterator
from contextlib import asynccontextmanager

from fastapi import FastAPI

from api.router import router as api_router
from config import load_settings
from events import EventBus
from jobs.store import JobStore
from payment.service import CashboxPaymentProvider, MockPaymentProvider
from printer.client import MockPrinterClient, PrusaLinkClient
from printer.emulation import router as prusalink_router
from printer.power import EasyVerwaltungPowerControlService, MockPowerControlService
from printer.registry import PrinterConfig, PrinterRegistry, PrinterStatus
from rfid.client import DeviceAgentClient

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s [%(name)s] %(message)s",
)
logger = logging.getLogger("st3d.main")


async def _cleanup_loop(app: FastAPI) -> None:
    while True:
        try:
            removed = app.state.store.cleanup_expired_and_orphans()
            if removed > 0:
                app.state.events.publish("queue_updated", {"action": "expired", "count": removed})
            # Purge expired in-memory sessions
            expired_sessions = [sid for sid, s in list(app.state.sessions.items()) if s.is_expired]
            for sid in expired_sessions:
                app.state.sessions.pop(sid, None)
        except Exception as exc:
            logger.error("Cleanup loop error: %s", exc)
        await asyncio.sleep(10)


async def _poll_and_reconcile_printer(app: FastAPI, printer: PrinterConfig) -> PrinterStatus:
    st = app.state
    thresh = st.settings.hotend_cooldown_temp_c
    pstate = st.store.get_power_state(printer.id)

    if st.settings.power_mode == "mock" and pstate["powerState"] == "OFF":
        new = PrinterStatus(
            state="OFFLINE",
            power_state="OFF",
            pending_shutdown=False,
            shutdown_temp_threshold_c=thresh,
            error="Ausgeschaltet",
        )
        st.registry.set_status(printer.id, new)
        return new

    new = await st.printer_client.poll_status(printer)
    if printer.id in st.dev_nozzle_overrides:
        new.nozzle_temp = float(st.dev_nozzle_overrides[printer.id])

    new.shutdown_temp_threshold_c = thresh
    new.pending_shutdown = bool(pstate["pendingShutdown"])
    if new.state != "OFFLINE":
        new.power_state = "ON"
        if pstate["powerState"] != "ON":
            st.store.set_power_state(printer.id, power_state="ON")
    else:
        new.power_state = pstate["powerState"] if pstate["powerState"] in {"OFF", "BOOTING"} else "OFF"
        if new.power_state == "OFF":
            new.error = "Ausgeschaltet"
        elif new.power_state == "BOOTING":
            new.error = "Startet…"

    # Thermal safety auto-shutdown: cut mains once hotend cooled down to <= threshold
    if new.pending_shutdown and new.state not in {"PRINTING", "UPLOADING"} and printer.id not in st.active_uploads:
        if new.nozzle_temp is None or new.nozzle_temp <= thresh:
            res = await st.power_control.power_off(printer, reason="COOLDOWN_COMPLETE")
            if res.ok:
                st.store.set_power_state(printer.id, power_state="OFF", pending_shutdown=False)
                st.store.log_event(
                    "printer_powered_off",
                    printer_id=printer.id,
                    details={"reason": "COOLDOWN_COMPLETE", "nozzleTemp": new.nozzle_temp},
                )
                new.pending_shutdown = False
                new.power_state = "OFF"
                if st.settings.power_mode == "mock":
                    new.state = "OFFLINE"
                    new.error = "Ausgeschaltet"
                st.events.publish(
                    "printer_power_changed",
                    {
                        "printerId": printer.id,
                        "printerSlot": printer.slot,
                        "action": "powered_off",
                        "reason": "COOLDOWN_COMPLETE",
                    },
                )

    st.registry.set_status(printer.id, new)
    return new


async def _printer_poll_loop(app: FastAPI) -> None:
    interval = app.state.settings.printers_poll_interval_seconds
    while True:
        try:
            for printer in app.state.registry.list_printers():
                old = app.state.registry.get_status(printer.id)
                new = await _poll_and_reconcile_printer(app, printer)
                temp_changed_during_cooldown = (
                    new.pending_shutdown
                    and round(old.nozzle_temp or 0) != round(new.nozzle_temp or 0)
                )
                if (
                    old.state != new.state
                    or old.power_state != new.power_state
                    or old.pending_shutdown != new.pending_shutdown
                    or temp_changed_during_cooldown
                ):
                    app.state.events.publish("printers_updated", {"printerId": printer.id, "state": new.state})
        except Exception as exc:
            logger.error("Printer poll loop error: %s", exc)
        await asyncio.sleep(interval)


@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    settings = load_settings()
    store = JobStore(settings.data_dir)
    registry = PrinterRegistry(
        settings.printers_config_file,
        settings.printers_config_url,
        override_file=settings.data_dir / "printers.json",
    )
    events = EventBus()
    printer_client = PrusaLinkClient() if settings.printer_mode == "live" else MockPrinterClient()
    payment = (
        CashboxPaymentProvider(settings.admin_bv_rfids)
        if settings.payment_mode == "cashbox"
        else MockPaymentProvider()
    )
    power_control = (
        EasyVerwaltungPowerControlService(
            settings.easyverwaltung_url,
            settings.easyverwaltung_service_id,
            settings.easyverwaltung_service_token,
        )
        if settings.power_mode == "live"
        else MockPowerControlService()
    )

    def _handle_rfid_scan(uid: str) -> None:
        logger.info("RFID scan received from device-agent")
        events.publish("rfid_scan", {"uid": uid})

    device_client = DeviceAgentClient(settings.device_agent_url, on_rfid_scan=_handle_rfid_scan)

    app.state.settings = settings
    app.state.store = store
    app.state.registry = registry
    app.state.events = events
    app.state.printer_client = printer_client
    app.state.payment = payment
    app.state.power_control = power_control
    app.state.device_client = device_client
    app.state.sessions = {}
    app.state.admin_sessions = {}
    app.state.active_uploads = {}
    app.state.upload_tasks = set()
    app.state.upload_tasks_by_job = {}
    app.state.dev_nozzle_overrides = {}
    app.state.cached_metadata = await payment.get_metadata()

    # Initial status poll
    for printer in registry.list_printers():
        await _poll_and_reconcile_printer(app, printer)

    tasks = [
        asyncio.create_task(_cleanup_loop(app), name="cleanup-loop"),
        asyncio.create_task(_printer_poll_loop(app), name="printer-poll-loop"),
        asyncio.create_task(device_client.run_ws_listener(), name="device-agent-ws"),
    ]
    logger.info(
        "SmartTerminal Backend started (terminal=%s, modes=hw:%s/pay:%s/printer:%s, printers=%d)",
        settings.terminal_id,
        settings.hardware_mode,
        settings.payment_mode,
        settings.printer_mode,
        len(registry.list_printers()),
    )
    try:
        yield
    finally:
        events.shutdown()
        device_client.stop()
        for t in list(app.state.upload_tasks):
            t.cancel()
        for t in tasks:
            t.cancel()
        await asyncio.gather(*tasks, *list(app.state.upload_tasks), return_exceptions=True)


app = FastAPI(title="SmartTerminal 3D-Print Backend", version="0.2.0", lifespan=lifespan)

# Interne UI-API (/api/health, /api/state, /api/events, /api/jobs, /api/session, /api/dev/*)
app.include_router(api_router, prefix="/api")

# PrusaLink-Emulation (/api/version, /api/v1/*, /api/files/local)
app.include_router(prusalink_router)
