#!/usr/bin/env python3
"""Bundle the live SmartTerminal 3D-Print UI into a single offline-shareable HTML demo.

Usage:
    python3 scripts/export-demo-html.py
    # Output: docs/smartterminal-demo.html
"""
from __future__ import annotations

import base64
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UI_DIR = ROOT / "services" / "ui" / "src"
OUT_FILE = ROOT / "docs" / "smartterminal-demo.html"


def _svg_data_uri(path: Path) -> str:
    b64 = base64.b64encode(path.read_bytes()).decode("ascii")
    return f"data:image/svg+xml;base64,{b64}"


DEMO_PANEL_CSS = """
/* === Standalone Offline Demo Shell === */
@media (min-width: 920px) and (min-aspect-ratio: 1/1) {
  body {
    display: flex !important;
    align-items: center;
    justify-content: center;
    gap: 28px;
    padding: 16px;
    background: radial-gradient(circle at 20% 20%, #1e293b 0%, #0b1019 100%) !important;
  }
  .demo-panel {
    display: flex !important;
  }
}
.demo-panel {
  display: none;
  flex-direction: column;
  gap: 14px;
  width: 310px;
  max-height: min(94vh, 854px);
  overflow-y: auto;
  background: rgba(15, 23, 42, 0.92);
  color: #f1f5f9;
  border: 1px solid #334155;
  border-radius: 18px;
  padding: 18px;
  box-shadow: 0 20px 50px rgba(0, 0, 0, 0.45);
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
}
.demo-panel h2 {
  margin: 0;
  font-size: 16px;
  font-weight: 800;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
}
.demo-badge {
  font-size: 10px;
  font-weight: 800;
  text-transform: uppercase;
  letter-spacing: 0.05em;
  background: rgba(245, 158, 11, 0.18);
  color: #fbbf24;
  border: 1px solid rgba(245, 158, 11, 0.45);
  padding: 3px 8px;
  border-radius: 999px;
}
.demo-sub {
  margin: 0;
  font-size: 12.5px;
  color: #94a3b8;
  line-height: 1.45;
}
.demo-sec {
  display: flex;
  flex-direction: column;
  gap: 8px;
  padding-top: 10px;
  border-top: 1px solid #1e293b;
}
.demo-sec-title {
  font-size: 11px;
  font-weight: 800;
  text-transform: uppercase;
  letter-spacing: 0.06em;
  color: #94a3b8;
}
.demo-mode-row {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 6px;
}
.demo-btn {
  border: 1px solid #334155;
  background: #1e293b;
  color: #f8fafc;
  border-radius: 10px;
  padding: 9px 10px;
  font-size: 12.5px;
  font-weight: 700;
  cursor: pointer;
  text-align: left;
  transition: background 0.12s, border-color 0.12s, transform 0.08s;
}
.demo-btn:hover { background: #293548; border-color: #475569; }
.demo-btn:active { transform: scale(0.98); }
.demo-btn.primary {
  background: #2563eb;
  border-color: #3b82f6;
  text-align: center;
}
.demo-btn.primary:hover { background: #1d4ed8; }
.demo-btn.active {
  background: rgba(37, 99, 235, 0.22);
  border-color: #60a5fa;
  color: #bfdbfe;
  text-align: center;
}
.demo-btn.center { text-align: center; }
.demo-steps {
  margin: 0;
  padding-left: 16px;
  font-size: 12px;
  color: #cbd5e1;
  line-height: 1.5;
  display: flex;
  flex-direction: column;
  gap: 4px;
}
.demo-note {
  font-size: 11.5px;
  color: #94a3b8;
  background: #0f172a;
  border: 1px solid #1e293b;
  border-radius: 10px;
  padding: 9px 10px;
  line-height: 1.45;
}
"""

DEMO_PANEL_HTML = """
  <aside class="demo-panel" id="demo-panel" aria-label="Interaktive Demo-Steuerung">
    <h2>FREILab Demo <span class="demo-badge">Offline HTML</span></h2>
    <p class="demo-sub">Interaktive Vorschau des 5&quot;-Touch-Terminals (echtes Frontend, lokaler Simulator im Browser — kein Server nötig).</p>

    <div class="demo-sec">
      <div class="demo-sec-title">1. Betriebsmodus</div>
      <div class="demo-mode-row">
        <button type="button" class="demo-btn active center" data-demo-mode="cashbox">🪙 Pilot · Kasse</button>
        <button type="button" class="demo-btn center" data-demo-mode="mock">💳 Konto (später)</button>
      </div>
    </div>

    <div class="demo-sec">
      <div class="demo-sec-title">2. Slicer-Upload simulieren</div>
      <button type="button" class="demo-btn primary" id="demo-send-job">+ Druckjob senden (Auto-RFID)</button>
    </div>

    <div class="demo-sec">
      <div class="demo-sec-title">3. RFID-Karte vorhalten</div>
      <button type="button" class="demo-btn" data-demo-rfid="04AABBCCDD01">💳 Mitgliedskarte (Max · 25,00 €)</button>
      <button type="button" class="demo-btn" data-demo-rfid="04AABBCCDD02" id="demo-card-low">💳 Karte wenig Guthaben (Erika · 0,15 €)</button>
      <button type="button" class="demo-btn" data-demo-rfid="04AABBCCDD03" id="demo-card-unapp">🚫 Karte ohne 3D-Freigabe (Alex)</button>
      <button type="button" class="demo-btn" data-demo-rfid="04AABBCCDD99">🔐 BV-Karte (Admin / Einweiser)</button>
    </div>

    <div class="demo-sec">
      <div class="demo-sec-title">Kurzanleitung</div>
      <ol class="demo-steps">
        <li>Im Terminal auf <strong>DRUCKEN</strong> tippen (oder links neuen Job senden).</li>
        <li>Wenn nach der Karte gefragt wird, hier eine <strong>RFID-Karte</strong> anklicken.</li>
        <li>Im <strong>Pilot-Modus</strong> Kassen-Einwurf oder <strong>Vereinsdruck</strong> (9 Bereiche) testen.</li>
        <li>Über <strong>⚙ → Admin (BV)</strong> + <strong>BV-Karte</strong> Verlauf, QR-Setup &amp; Geräte-Settings ansehen.</li>
      </ol>
      <div class="demo-note">PIN im Konto-Modus (Vereinsdruck / Bereits bezahlt): <strong>1234</strong> · Auf dem Smartphone klappt der <strong>DEV</strong>-Button links unten die gleichen Testkarten aus.</div>
    </div>
  </aside>
"""

MOCK_SHIM_JS = r"""
(() => {
  // Guard localStorage on strict file:// browsers (Safari / private windows)
  try {
    window.localStorage.getItem("__st3d_probe");
  } catch (_) {
    const mem = Object.create(null);
    Object.defineProperty(window, "localStorage", {
      value: {
        getItem: (k) => (k in mem ? mem[k] : null),
        setItem: (k, v) => { mem[k] = String(v); },
        removeItem: (k) => { delete mem[k]; },
      },
    });
  }

  // === Offline In-Browser Backend & SSE Mock for SmartTerminal 3D-Print Demo ===
  const svgUri = (svg) => "data:image/svg+xml;charset=utf-8," + encodeURIComponent(svg.trim());

  const THUMB_BENCHY = svgUri(`
    <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96">
      <rect width="96" height="96" rx="14" fill="#fff7ed"/>
      <path d="M18 62 L78 62 L70 76 L26 76 Z" fill="#f97316" stroke="#c2410c" stroke-width="2.5" stroke-linejoin="round"/>
      <rect x="30" y="38" width="28" height="24" rx="3" fill="#fb923c" stroke="#c2410c" stroke-width="2.5"/>
      <rect x="36" y="44" width="12" height="10" rx="2" fill="#fff7ed"/>
      <rect x="46" y="22" width="10" height="16" rx="2" fill="#ea580c" stroke="#c2410c" stroke-width="2"/>
      <path d="M24 38 L62 38 L58 32 L28 32 Z" fill="#fdba74" stroke="#c2410c" stroke-width="2"/>
    </svg>
  `);

  const THUMB_CASE = svgUri(`
    <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96">
      <rect width="96" height="96" rx="14" fill="#eff6ff"/>
      <polygon points="48,18 78,34 78,66 48,80 18,66 18,34" fill="#3b82f6" stroke="#1d4ed8" stroke-width="2.5" stroke-linejoin="round"/>
      <polygon points="48,18 78,34 48,48 18,34" fill="#93c5fd" stroke="#1d4ed8" stroke-width="2"/>
      <line x1="48" y1="48" x2="48" y2="80" stroke="#1d4ed8" stroke-width="2.5"/>
      <circle cx="33" cy="54" r="5" fill="#eff6ff"/>
      <circle cx="63" cy="54" r="5" fill="#eff6ff"/>
    </svg>
  `);

  const THUMB_BRACKET = svgUri(`
    <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96">
      <rect width="96" height="96" rx="14" fill="#ecfdf5"/>
      <path d="M22 24 H44 V56 H76 V74 H22 Z" fill="#10b981" stroke="#047857" stroke-width="2.5" stroke-linejoin="round"/>
      <circle cx="33" cy="36" r="4.5" fill="#ecfdf5"/>
      <circle cx="62" cy="65" r="4.5" fill="#ecfdf5"/>
    </svg>
  `);

  const FAKE_QR_SVG = svgUri(`
    <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 29 29" shape-rendering="crispEdges">
      <rect width="29" height="29" fill="#ffffff"/>
      <g fill="#0f172a">
        <path d="M2 2h7v7H2zm1 1v5h5V3zm1 1h3v3H4zM20 2h7v7h-7zm1 1v5h5V3zm1 1h3v3h-3zM2 20h7v7H2zm1 1v5h5v-5zm1 1h3v3H4z"/>
        <rect x="11" y="2" width="2" height="2"/><rect x="15" y="2" width="2" height="2"/><rect x="13" y="4" width="2" height="2"/>
        <rect x="11" y="6" width="3" height="2"/><rect x="16" y="6" width="2" height="3"/><rect x="11" y="10" width="2" height="3"/>
        <rect x="2" y="11" width="3" height="2"/><rect x="7" y="11" width="2" height="2"/><rect x="4" y="14" width="3" height="2"/>
        <rect x="2" y="17" width="4" height="2"/><rect x="9" y="14" width="3" height="3"/><rect x="14" y="11" width="3" height="2"/>
        <rect x="18" y="11" width="4" height="2"/><rect x="24" y="11" width="3" height="2"/><rect x="14" y="15" width="2" height="3"/>
        <rect x="18" y="15" width="3" height="2"/><rect x="23" y="14" width="4" height="2"/><rect x="11" y="19" width="2" height="3"/>
        <rect x="15" y="19" width="3" height="2"/><rect x="20" y="18" width="2" height="3"/><rect x="24" y="18" width="3" height="3"/>
        <rect x="11" y="24" width="4" height="3"/><rect x="17" y="22" width="3" height="2"/><rect x="17" y="25" width="2" height="2"/>
        <rect x="21" y="22" width="3" height="3"/><rect x="25" y="23" width="2" height="4"/>
      </g>
    </svg>
  `);

  const SECTIONS = [
    { sectionId: "sec_3d", name: "3D-Druck", description: "3D-Druck", pictogram: "printer", status: "active" },
    { sectionId: "sec_elec", name: "Elektronik", description: "Elektronik", pictogram: "cpu", status: "active" },
    { sectionId: "sec_wood", name: "Holzwerkstatt", description: "Holzwerkstatt", pictogram: "wood", status: "active" },
    { sectionId: "sec_metal", name: "Metall", description: "Metall", pictogram: "wrench", status: "active" },
    { sectionId: "sec_bike", name: "Fahrrad Bereich", description: "Fahrrad Bereich", pictogram: "bike", status: "active" },
    { sectionId: "sec_laser", name: "Lasercutter", description: "Lasercutter", pictogram: "laser", status: "active" },
    { sectionId: "sec_cnc", name: "CNC", description: "CNC", pictogram: "cnc", status: "active" },
    { sectionId: "sec_weld", name: "Schweißen", description: "Schweißen", pictogram: "flame", status: "active" },
    { sectionId: "sec_textile", name: "Textil", description: "Textil", pictogram: "scissors", status: "active" },
  ];

  const USERS = {
    "04AABBCCDD01": {
      member_id: "mem_001", user_id: "usr_marius", display_name: "Marius Test",
      pin: "1234", balance_cents: 2500, approved: true, is_instructor: false,
      transactions: [{ ts: Date.now() / 1000 - 86400, title: "Aufladung Guthaben", amountCents: 2500 }],
    },
    "04AABBCCDD02": {
      member_id: "mem_002", user_id: "usr_erika", display_name: "Erika Wenig",
      pin: "1234", balance_cents: 15, approved: true, is_instructor: false,
      transactions: [{ ts: Date.now() / 1000 - 7200, title: "3D-Druck Benchy", amountCents: -185 }],
    },
    "04AABBCCDD03": {
      member_id: "mem_003", user_id: "usr_neuling", display_name: "Alex Neuling",
      pin: "1234", balance_cents: 1000, approved: false, is_instructor: false,
      transactions: [],
    },
    "04AABBCCDD99": {
      member_id: "mem_099", user_id: "usr_bv", display_name: "Sam Einweiser (BV)",
      pin: "9999", balance_cents: 5000, approved: true, is_instructor: true,
      transactions: [],
    },
  };

  const nowSec = () => Date.now() / 1000;
  let seq = 10;

  const db = {
    paymentMode: "cashbox", // "cashbox" | "mock"
    displaySettings: { brightness: 90, dimDelaySeconds: 120, sleepDelaySeconds: 1800, buzzerMode: "all" },
    printers: [
      {
        slot: 1, id: "prusa-01", name: "Drucker 1 (MK3.5)", model: "MK3.5", ip: "10.30.0.139",
        buildVolume: "250×210×210", state: "READY", canAcceptJob: true, progress: null, activeJob: null,
        activeJobOwner: null, activeJobSection: null, nozzleTemp: 24.5, targetNozzle: 0, bedTemp: 23.0, targetBed: 0,
        nozzleDiameter: 0.4, timeRemainingSeconds: null, totalPrintHours: 142.5, totalFilamentKm: 1.82,
        powerState: "ON", pendingShutdown: false, shutdownTempThresholdC: 50, error: null,
      },
      {
        slot: 2, id: "prusa-02", name: "Drucker 2 (MK4)", model: "MK4", ip: "10.30.0.140",
        buildVolume: "250×210×220", state: "READY", canAcceptJob: true, progress: null, activeJob: null,
        activeJobOwner: null, activeJobSection: null, nozzleTemp: 25.0, targetNozzle: 0, bedTemp: 22.8, targetBed: 0,
        nozzleDiameter: 0.4, timeRemainingSeconds: null, totalPrintHours: 218.0, totalFilamentKm: 2.94,
        powerState: "ON", pendingShutdown: false, shutdownTempThresholdC: 50, error: null,
      },
      {
        slot: 3, id: "prusa-03", name: "Drucker 3 (MK3S)", model: "MK3S", ip: "10.30.0.141",
        buildVolume: "250×210×210", state: "PRINTING", canAcceptJob: false, progress: 62, activeJob: "Kabelhalter_v3",
        activeJobOwner: "Karte 04AA…DD01", activeJobSection: null, nozzleTemp: 215.2, targetNozzle: 215, bedTemp: 60.0, targetBed: 60,
        nozzleDiameter: 0.4, timeRemainingSeconds: 1920, totalPrintHours: 512.3, totalFilamentKm: 6.41,
        powerState: "ON", pendingShutdown: false, shutdownTempThresholdC: 50, error: null,
      },
      {
        slot: 4, id: "prusa-04", name: "Drucker 4 (XL)", model: "XL", ip: "10.30.0.142",
        buildVolume: "360×360×360", state: "READY", canAcceptJob: true, progress: null, activeJob: null,
        activeJobOwner: null, activeJobSection: null, nozzleTemp: 23.4, targetNozzle: 0, bedTemp: 22.1, targetBed: 0,
        nozzleDiameter: 0.6, timeRemainingSeconds: null, totalPrintHours: 89.4, totalFilamentKm: 1.54,
        powerState: "ON", pendingShutdown: false, shutdownTempThresholdC: 50, error: null,
      },
    ],
    jobs: [
      {
        id: "job_demo_1", printerSlot: 1, printerId: "prusa-01", printerName: "Drucker 1 (MK3.5)",
        filename: "3DBenchy_0.2mm_PETG_MK35_47m.bgcode", displayName: "3DBenchy",
        status: "queued", filamentGrams: 14.2, filamentType: "PETG", estimatedTimeSeconds: 2820,
        priceCents: 57, priceSource: "slicer", printAfterUpload: true,
        thumbnailUrl: THUMB_BENCHY, expiresInSeconds: 295, createdAt: nowSec() - 5,
      },
      {
        id: "job_demo_2", printerSlot: 2, printerId: "prusa-02", printerName: "Drucker 2 (MK4)",
        filename: "Gehaeuse_v2_0.2mm_PLA_MK4_1h42m.bgcode", displayName: "Gehaeuse_v2",
        status: "queued", filamentGrams: 48.0, filamentType: "PLA", estimatedTimeSeconds: 6120,
        priceCents: 192, priceSource: "slicer", printAfterUpload: true,
        thumbnailUrl: THUMB_CASE, expiresInSeconds: 280, createdAt: nowSec() - 20,
      },
    ],
    sessions: new Map(),
    history: [
      {
        jobId: "job_hist_1", displayName: "Kabelhalter_v3", printerSlot: 3, printerName: "Drucker 3 (MK3S)",
        userDisplayName: "Karte 04AA…DD01", paymentMode: "cashbox", sectionName: null,
        filamentGrams: 22.5, filamentType: "PLA", priceCents: 90, startedAt: nowSec() - 2400,
      },
      {
        jobId: "job_hist_2", displayName: "Absaugadapter_CNC", printerSlot: 4, printerName: "Drucker 4 (XL)",
        userDisplayName: "BV (04AA…DD99)", paymentMode: "club", sectionName: "CNC",
        filamentGrams: 118.0, filamentType: "PETG", priceCents: 472, startedAt: nowSec() - 10800,
      },
    ],
  };

  // --- Mock EventSource ---
  const esClients = new Set();
  function emitSSE(type, payload) {
    const data = JSON.stringify(payload || {});
    for (const c of esClients) {
      c._dispatch(type, data);
    }
  }

  class MockEventSource {
    constructor(url) {
      this.url = url;
      this.readyState = 1;
      this._listeners = new Map();
      esClients.add(this);
      setTimeout(() => {
        if (typeof this.onopen === "function") this.onopen(new Event("open"));
      }, 30);
    }
    addEventListener(type, fn) {
      if (!this._listeners.has(type)) this._listeners.set(type, new Set());
      this._listeners.get(type).add(fn);
    }
    removeEventListener(type, fn) {
      this._listeners.get(type)?.delete(fn);
    }
    _dispatch(type, data) {
      const ev = { type, data };
      const fns = this._listeners.get(type);
      if (fns) fns.forEach((fn) => fn(ev));
    }
    close() {
      this.readyState = 2;
      esClients.delete(this);
    }
  }
  window.EventSource = MockEventSource;

  const maskUid = (uid) => {
    const c = String(uid || "").trim().toUpperCase();
    return c.length <= 8 ? c : `${c.slice(0, 4)}…${c.slice(-4)}`;
  };

  function makeStatePayload() {
    const isCash = db.paymentMode === "cashbox";
    return {
      terminalId: "3dprint-01",
      terminalIp: "192.168.1.78",
      version: "0.4.0-demo",
      serviceName: isCash ? "3D-Print Terminal (Pilot · Kasse)" : "3D-Print Terminal (easyVerwaltung)",
      printers: db.printers.map((p) => ({ ...p, updatedAt: nowSec() })),
      jobs: db.jobs.map((j) => ({ ...j })),
      sections: SECTIONS,
      displaySettings: { ...db.displaySettings },
      modes: {
        hardware: "mock",
        payment: db.paymentMode,
        cashbox: isCash,
        printer: "mock",
        devMock: true,
      },
      features: {
        topup: !isCash,
        approval: !isCash,
        allowNegativeBalance: true,
        maxNegativeBalanceCents: 2000,
      },
      serverTime: nowSec(),
    };
  }

  function jsonRes(obj, status = 200) {
    return new Response(JSON.stringify(obj), {
      status,
      headers: { "Content-Type": "application/json" },
    });
  }

  const origFetch = window.fetch ? window.fetch.bind(window) : null;
  window.fetch = async (input, init = {}) => {
    const urlStr = typeof input === "string" ? input : input.url;
    if (!urlStr.startsWith("/api/")) {
      return origFetch ? origFetch(input, init) : jsonRes({ error: "offline" }, 404);
    }
    const u = new URL(urlStr, "http://demo.local");
    const path = u.pathname;
    const method = (init.method || "GET").toUpperCase();
    let body = {};
    if (init.body && typeof init.body === "string") {
      try { body = JSON.parse(init.body); } catch (_) {}
    }

    if (path === "/api/state" && method === "GET") return jsonRes(makeStatePayload());
    if (path === "/api/health") return jsonRes({ status: "ok", dbOk: true });
    if (path === "/api/feedback/touch" || path === "/api/feedback/led" || path === "/api/display/power") {
      return jsonRes({ ok: true });
    }
    if (path === "/api/display/settings" && method === "POST") {
      db.displaySettings = { ...db.displaySettings, ...body };
      const root = document.getElementById("hmi-root");
      if (root && typeof db.displaySettings.brightness === "number") {
        const b = 0.45 + (db.displaySettings.brightness / 100) * 0.55;
        root.style.filter = `brightness(${b.toFixed(2)})`;
      }
      return jsonRes({ ok: true, settings: db.displaySettings });
    }

    if (path === "/api/session/authorize" && method === "POST") {
      const uid = String(body.rfid || "").trim().toUpperCase();
      const sid = "sess_" + (++seq);
      if (db.paymentMode === "cashbox") {
        const short = maskUid(uid);
        const isBv = uid === "04AABBCCDD99";
        const sess = {
          sessionId: sid,
          rfid: uid,
          user: { member_id: `rfid_${short}`, user_id: `rfid_${short}`, display_name: isBv ? `BV (${short})` : `Karte ${short}` },
          balanceCents: 0,
          currency: "EUR",
          approved: true,
          isInstructor: isBv,
          expiresInSeconds: 300,
          transactions: [],
        };
        db.sessions.set(sid, sess);
        return jsonRes({ authorized: true, session: sess });
      }
      const rec = USERS[uid];
      if (!rec) return jsonRes({ authorized: false, reason: "RFID_NOT_FOUND" });
      const sess = {
        sessionId: sid,
        rfid: uid,
        user: { member_id: rec.member_id, user_id: rec.user_id, display_name: rec.display_name },
        balanceCents: rec.balance_cents,
        currency: "EUR",
        approved: rec.approved,
        isInstructor: rec.is_instructor,
        expiresInSeconds: 300,
        transactions: rec.transactions.slice(0, 10),
      };
      db.sessions.set(sid, sess);
      return jsonRes({ authorized: true, session: sess });
    }

    if (path === "/api/session/cancel" && method === "POST") {
      db.sessions.delete(u.searchParams.get("sessionId") || "");
      return jsonRes({ ok: true });
    }

    if (path === "/api/session/topup" && method === "POST") {
      const sess = db.sessions.get(body.sessionId);
      if (!sess) return jsonRes({ detail: "Session abgelaufen" }, 404);
      const rec = USERS[sess.rfid];
      if (rec) {
        rec.balance_cents += Number(body.amountCents || 0);
        rec.transactions.unshift({ ts: nowSec(), title: "Aufladung am Terminal", amountCents: Number(body.amountCents || 0) });
        sess.balanceCents = rec.balance_cents;
        sess.transactions = rec.transactions.slice(0, 10);
      }
      return jsonRes({ ok: true, session: sess });
    }

    if (path === "/api/session/approve" && method === "POST") {
      const sess = db.sessions.get(body.sessionId);
      const inst = USERS[String(body.instructorRfid || "").trim().toUpperCase()];
      if (!sess || !inst || !inst.is_instructor) return jsonRes({ approved: false, reason: "NOT_AUTHORIZED_INSTRUCTOR" });
      const rec = USERS[sess.rfid];
      if (rec) rec.approved = true;
      sess.approved = true;
      return jsonRes({ approved: true, instructorName: inst.display_name, session: sess });
    }

    if (path === "/api/admin/authorize" && method === "POST") {
      const uid = String(body.rfid || "").trim().toUpperCase();
      const isBv = uid === "04AABBCCDD99";
      if (!isBv) return jsonRes({ authorized: false, reason: "BV_REQUIRED" });
      const user = db.paymentMode === "cashbox"
        ? { display_name: `BV (${maskUid(uid)})` }
        : { display_name: USERS[uid].display_name };
      return jsonRes({
        authorized: true,
        adminCode: "DEMO1234",
        expiresInSeconds: 600,
        setupUrl: "http://192.168.1.78/admin.html?code=DEMO1234",
        qrUrl: FAKE_QR_SVG,
        user,
      });
    }

    if (path === "/api/admin/config" && method === "GET") {
      return jsonRes({
        terminalId: "3dprint-01",
        paymentMode: db.paymentMode,
        easyverwaltungUrl: "https://verwaltung.freilab.de",
        easyverwaltungServiceId: "svc_3dprint_01",
        hasToken: db.paymentMode !== "cashbox",
        setupUrl: "http://192.168.1.78/admin.html?code=DEMO1234",
        qrUrl: FAKE_QR_SVG,
      });
    }

    if (path === "/api/admin/history" && method === "GET") {
      return jsonRes({ items: db.history.slice(0, 50) });
    }

    if (path === "/api/admin/test-connection" && method === "POST") {
      return jsonRes({
        ok: true,
        message: db.paymentMode === "cashbox"
          ? "Pilot-Modus (Barkasse) aktiv · 4 Drucker konfiguriert"
          : "Verbindung zu easyVerwaltung (Demo) OK",
      });
    }

    const startMatch = path.match(/^\/api\/jobs\/([^/]+)\/start$/);
    if (startMatch && method === "POST") {
      const jobId = decodeURIComponent(startMatch[1]);
      const jobIdx = db.jobs.findIndex((j) => j.id === jobId);
      const job = db.jobs[jobIdx];
      const sess = db.sessions.get(body.sessionId);
      if (!job) return jsonRes({ detail: "Job nicht gefunden" }, 404);
      if (!sess) return jsonRes({ detail: "Sitzung abgelaufen" }, 401);
      const printer = db.printers.find((p) => p.id === job.printerId);
      if (!printer || !printer.canAcceptJob) {
        return jsonRes({ started: false, reason: printer?.powerState === "OFF" ? "PRINTER_OFFLINE" : "PRINTER_BUSY" });
      }
      const mode = body.mode || (db.paymentMode === "cashbox" ? "cashbox" : "personal");
      let charged = 0;
      if (db.paymentMode !== "cashbox") {
        const rec = USERS[sess.rfid];
        if (mode === "club" || mode === "already_paid") {
          if (!rec || body.pin !== rec.pin) return jsonRes({ started: false, reason: "PIN_INVALID" });
        } else if (mode === "personal" && rec) {
          const after = rec.balance_cents - job.priceCents;
          if (after < 0 && !body.allowOverdraft) {
            return jsonRes({ started: false, reason: "INSUFFICIENT_BALANCE", balanceCents: rec.balance_cents });
          }
          rec.balance_cents = after;
          charged = job.priceCents;
          rec.transactions.unshift({ ts: nowSec(), title: `3D-Druck ${job.displayName}`, amountCents: -job.priceCents });
          sess.balanceCents = after;
        }
      } else if (mode === "cashbox") {
        charged = job.priceCents;
      }

      const secObj = SECTIONS.find((s) => s.sectionId === body.sectionId);
      job.status = "sending";
      job.uploadProgress = { percent: 0, sentBytes: 0, totalBytes: 2400000, bytesPerSec: 1800000 };
      printer.state = "UPLOADING";
      printer.canAcceptJob = false;
      printer.progress = 0;
      printer.activeJob = job.displayName;
      printer.activeJobOwner = sess.user.display_name;
      printer.activeJobSection = secObj ? secObj.name : null;
      emitSSE("queue_updated", { action: "sending", jobId: job.id });
      emitSSE("printers_updated", { printerId: printer.id });

      const steps = [22, 54, 83, 100];
      steps.forEach((pct, i) => {
        setTimeout(() => {
          const prog = {
            jobId: job.id, printerId: printer.id, displayName: job.displayName,
            stage: "uploading", percent: pct,
            sentBytes: Math.round(2400000 * pct / 100), totalBytes: 2400000, bytesPerSec: 1850000,
          };
          job.uploadProgress = prog;
          printer.progress = pct;
          emitSSE("job_progress", prog);
          if (pct === 100) {
            setTimeout(() => {
              const idx = db.jobs.findIndex((x) => x.id === job.id);
              if (idx >= 0) db.jobs.splice(idx, 1);
              printer.state = job.printAfterUpload === false ? "READY" : "PRINTING";
              printer.canAcceptJob = job.printAfterUpload === false;
              printer.progress = job.printAfterUpload === false ? null : 1;
              printer.nozzleTemp = job.printAfterUpload === false ? 25 : 215;
              printer.bedTemp = job.printAfterUpload === false ? 23 : 60;
              printer.timeRemainingSeconds = job.printAfterUpload === false ? null : job.estimatedTimeSeconds;
              db.history.unshift({
                jobId: job.id, displayName: job.displayName, printerSlot: printer.slot, printerName: printer.name,
                userDisplayName: sess.user.display_name, paymentMode: mode, sectionName: secObj ? secObj.name : null,
                filamentGrams: job.filamentGrams, filamentType: job.filamentType, priceCents: job.priceCents,
                startedAt: nowSec(),
              });
              emitSSE("job_done", {
                jobId: job.id, printerName: printer.name, printerSlot: printer.slot,
                displayName: job.displayName, mode, chargedCents: charged,
                sectionName: secObj ? secObj.name : null, printAfterUpload: job.printAfterUpload !== false,
              });
              emitSSE("queue_updated", { action: "transfer_finished", jobId: job.id });
              emitSSE("printers_updated", { printerId: printer.id });
            }, 220);
          }
        }, (i + 1) * 320);
      });

      return jsonRes({ started: true, ok: true, status: "sending", jobId: job.id, chargedCents: charged, mode });
    }

    const delMatch = path.match(/^\/api\/jobs\/([^/]+)$/);
    if (delMatch && method === "DELETE") {
      const jobId = decodeURIComponent(delMatch[1]);
      db.jobs = db.jobs.filter((j) => j.id !== jobId);
      emitSSE("queue_updated", { action: "deleted", jobId });
      return jsonRes({ ok: true });
    }

    const pwrMatch = path.match(/^\/api\/printers\/([^/]+)\/power\/(on|off|cancel-shutdown)$/);
    if (pwrMatch && method === "POST") {
      const pid = decodeURIComponent(pwrMatch[1]);
      const action = pwrMatch[2];
      const p = db.printers.find((x) => x.id === pid);
      if (!p) return jsonRes({ detail: "Drucker nicht gefunden" }, 404);
      if (action === "on") {
        p.powerState = "ON";
        p.pendingShutdown = false;
        p.state = "READY";
        p.canAcceptJob = true;
        p.error = null;
        emitSSE("printers_updated", { printerId: p.id });
        return jsonRes({ ok: true, state: p.state, powerState: p.powerState });
      } else if (action === "off") {
        if (p.state === "PRINTING") return jsonRes({ ok: false, reason: "PRINTER_PRINTING" });
        if (p.state === "UPLOADING") return jsonRes({ ok: false, reason: "PRINTER_UPLOADING" });
        if ((p.nozzleTemp || 0) > p.shutdownTempThresholdC) {
          p.pendingShutdown = true;
          emitSSE("printers_updated", { printerId: p.id });
          return jsonRes({ ok: true, action: "cooldown_scheduled", nozzleTemp: p.nozzleTemp, thresholdC: p.shutdownTempThresholdC });
        }
        p.powerState = "OFF";
        p.pendingShutdown = false;
        p.state = "OFFLINE";
        p.canAcceptJob = false;
        p.error = "Ausgeschaltet";
        emitSSE("printers_updated", { printerId: p.id });
        return jsonRes({ ok: true, action: "powered_off", powerState: p.powerState });
      } else {
        p.pendingShutdown = false;
        emitSSE("printers_updated", { printerId: p.id });
        return jsonRes({ ok: true, pendingShutdown: false });
      }
    }

    if (path === "/api/dev/simulate-rfid" && method === "POST") {
      const uid = String(body.uid || "04AABBCCDD01").trim().toUpperCase();
      emitSSE("rfid_scan", { uid, ts: nowSec(), source: "demo" });
      return jsonRes({ ok: true, uid });
    }

    if (path === "/api/dev/upload-sample" && method === "POST") {
      const samples = [
        { slot: 1, name: "Halterung_LED", g: 24.5, mat: "PETG", sec: 3900, cents: 98, thumb: THUMB_BRACKET, pau: true },
        { slot: 2, name: "3DBenchy", g: 14.2, mat: "PLA", sec: 2820, cents: 57, thumb: THUMB_BENCHY, pau: true },
        { slot: 4, name: "XL_Werkzeughalter", g: 135.0, mat: "PETG", sec: 12600, cents: 540, thumb: THUMB_CASE, pau: false },
      ];
      const pick = samples[seq % samples.length];
      const id = "job_demo_" + (++seq);
      const pr = db.printers.find((x) => x.slot === pick.slot) || db.printers[0];
      const job = {
        id, printerSlot: pr.slot, printerId: pr.id, printerName: pr.name,
        filename: `${pick.name}_${pick.mat}.bgcode`, displayName: pick.name,
        status: "queued", filamentGrams: pick.g, filamentType: pick.mat,
        estimatedTimeSeconds: pick.sec, priceCents: pick.cents, priceSource: "slicer",
        printAfterUpload: pick.pau, thumbnailUrl: pick.thumb, expiresInSeconds: 300, createdAt: nowSec(),
      };
      db.jobs.unshift(job);
      emitSSE("queue_updated", { action: "uploaded", jobId: id, slot: pr.slot, autoRfid: true });
      return jsonRes({ ok: true, job });
    }

    if (path === "/api/dev/printer-state" && method === "POST") {
      const p = db.printers.find((x) => x.id === body.printerId) || db.printers[0];
      if (p) {
        p.state = body.state || "READY";
        p.canAcceptJob = p.state === "READY";
        p.progress = p.state === "PRINTING" ? 42 : null;
        p.activeJob = p.state === "PRINTING" ? "Demo_Druck" : null;
        emitSSE("printers_updated", { printerId: p.id });
      }
      return jsonRes({ ok: true });
    }

    return jsonRes({ ok: true });
  };

  // Wire up the Desktop Demo Control Panel once DOM is ready
  window.addEventListener("DOMContentLoaded", () => {
    const syncModeUi = () => {
      document.querySelectorAll("[data-demo-mode]").forEach((btn) => {
        btn.classList.toggle("active", btn.dataset.demoMode === db.paymentMode);
      });
      const isCash = db.paymentMode === "cashbox";
      const low = document.getElementById("demo-card-low");
      const unapp = document.getElementById("demo-card-unapp");
      if (low) low.style.display = isCash ? "none" : "";
      if (unapp) unapp.style.display = isCash ? "none" : "";
    };
    syncModeUi();

    document.getElementById("demo-panel")?.addEventListener("click", async (ev) => {
      const modeBtn = ev.target.closest("[data-demo-mode]");
      if (modeBtn) {
        db.paymentMode = modeBtn.dataset.demoMode;
        syncModeUi();
        emitSSE("printers_updated", { mode: db.paymentMode });
        emitSSE("queue_updated", { action: "mode_switch" });
        // Click Home/Back if open so the mode switch updates topbar immediately
        document.getElementById("btn-setup")?.classList.contains("is-home") && document.getElementById("btn-setup").click();
        return;
      }
      const rfidBtn = ev.target.closest("[data-demo-rfid]");
      if (rfidBtn) {
        await fetch("/api/dev/simulate-rfid", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ uid: rfidBtn.dataset.demoRfid }),
        });
        return;
      }
      if (ev.target.closest("#demo-send-job")) {
        await fetch("/api/dev/upload-sample", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ slot: 1 }),
        });
      }
    });
  });
})();
"""


def build() -> Path:
    html = (UI_DIR / "index.html").read_text(encoding="utf-8")
    css = (UI_DIR / "styles.css").read_text(encoding="utf-8")
    js = (UI_DIR / "app.js").read_text(encoding="utf-8")
    logo_light = _svg_data_uri(UI_DIR / "assets" / "freilab-logo.svg")
    logo_dark = _svg_data_uri(UI_DIR / "assets" / "freilab-logo-dark.svg")

    # Inline logos in both HTML and JS
    html = html.replace('src="assets/freilab-logo.svg"', f'src="{logo_light}"')
    js = js.replace('"assets/freilab-logo-dark.svg"', f'"{logo_dark}"')
    js = js.replace('"assets/freilab-logo.svg"', f'"{logo_light}"')

    # Inline stylesheet + demo panel styles
    html = html.replace(
        '<link rel="stylesheet" href="styles.css">',
        f"<style>\n{css}\n{DEMO_PANEL_CSS}\n</style>",
    )

    # Inject Demo panel beside .hmi-root and inline Mock Shim + real app.js
    html = html.replace(
        '  <script src="app.js"></script>',
        f"{DEMO_PANEL_HTML}\n  <script>\n{MOCK_SHIM_JS}\n  </script>\n  <script>\n{js}\n  </script>",
    )
    html = html.replace(
        "<title>SmartTerminal 3D-Print</title>",
        "<title>SmartTerminal 3D-Print — Interaktive FREILab Demo</title>",
    )

    OUT_FILE.parent.mkdir(parents=True, exist_ok=True)
    OUT_FILE.write_text(html, encoding="utf-8")
    return OUT_FILE


if __name__ == "__main__":
    out = build()
    print(f"Exported standalone demo: {out} ({out.stat().st_size // 1024} KB)")
