(() => {
  if (new URLSearchParams(window.location.search).get("kiosk") === "1") {
    document.documentElement.classList.add("kiosk");
  }

  const savedTheme = localStorage.getItem("st3d_theme") === "dark" ? "dark" : "light";
  document.documentElement.setAttribute("data-theme", savedTheme);

  const RING_CIRC = 113.1; // 2 * pi * 18

  const state = {
    theme: savedTheme,
    terminalId: "3dprint-01",
    terminalIp: "localhost",
    version: "0.4.0",
    serviceName: "SmartTerminal 3D-Print",
    printers: [],
    jobs: [],
    sections: [],
    modes: { devMock: false, printer: "mock" },
    features: { topup: false, approval: false, allowNegativeBalance: true, maxNegativeBalanceCents: 2000 },
    displaySettings: { brightness: 90, dimDelaySeconds: 120, sleepDelaySeconds: 1800, buzzerMode: "all" },
    connected: false,
    networkOnline: true,

    // Screen: queue | rfid_wait | payment_confirm | overdraft_confirm | topup | club_section | pin_entry | printer_busy | account | settings | admin | not_approved | bv_wait | approved_ok
    screen: "queue",
    rfidPurpose: null, // "print" | "account" | "power_on" | "admin"
    rfidAutoTriggered: false,
    rfidError: "",
    selectedJobId: null,
    powerPrinterId: null,
    selectedSectionId: null,
    pinMode: null, // "club" | "already_paid"
    pinValue: "",
    pinError: "",
    session: null,
    lastResult: null,
    topupReturnTo: "payment_confirm",

    // BV Admin state
    adminCode: null,
    adminUser: null,
    adminTab: "history", // "history" | "config"
    adminConfig: null,
    adminHistory: [],

    // Popups
    uploadPopupJobId: null,
    printerPopupSlot: null,
    donePopup: null,

    // Timeout ring
    timeoutEnd: null,
    timeoutTotal: 60,
    timeoutAction: null,

    // Display power state: "active" | "dim" | "sleep"
    powerState: "active",
    lastActivityAt: Date.now(),
  };

  const $ = (id) => document.getElementById(id);

  // --- Formatting Helpers ---
  function escapeHtml(s) {
    return String(s ?? "")
      .replaceAll("&", "&amp;")
      .replaceAll("<", "&lt;")
      .replaceAll(">", "&gt;")
      .replaceAll('"', "&quot;");
  }

  function fmtEur(cents) {
    return `${(Number(cents || 0) / 100).toFixed(2).replace(".", ",")} €`;
  }

  function fmtDur(sec) {
    sec = Math.max(0, Number(sec || 0));
    if (sec === 0) return "–";
    const h = Math.floor(sec / 3600);
    const m = Math.floor((sec % 3600) / 60);
    return h > 0 ? `${h}h ${m}m` : `${Math.max(1, m)} min`;
  }

  function fmtEta(sec) {
    sec = Math.max(0, Number(sec || 0));
    if (sec === 0) return "–";
    const now = new Date();
    const end = new Date(now.getTime() + sec * 1000);
    const hhmm = end.toLocaleTimeString("de-DE", { hour: "2-digit", minute: "2-digit" });
    return end.getDate() !== now.getDate() ? `Morgen ${hhmm}` : `ca. ${hhmm}`;
  }

  function fmtCountdown(sec) {
    sec = Math.max(0, Math.floor(sec));
    const m = Math.floor(sec / 60);
    const s = sec % 60;
    return `${m}:${String(s).padStart(2, "0")}`;
  }

  function fmtMB(bytes) {
    const mb = Number(bytes || 0) / (1024 * 1024);
    if (mb < 0.1) return `${Math.max(1, Math.round(Number(bytes || 0) / 1024))} KB`;
    return `${mb.toFixed(1).replace(".", ",")} MB`;
  }

  function fmtSpeed(bps) {
    bps = Number(bps || 0);
    if (bps <= 0) return "";
    if (bps >= 1024 * 1024) return `${(bps / (1024 * 1024)).toFixed(1).replace(".", ",")} MB/s`;
    return `${Math.max(1, Math.round(bps / 1024))} KB/s`;
  }

  function fmtOnTime(sec) {
    if (sec == null) return "Aus";
    const d = Math.floor(sec / 86400);
    const h = Math.floor((sec % 86400) / 3600);
    const m = Math.floor((sec % 3600) / 60);
    if (d > 0) return `${d}d ${h}h`;
    if (h > 0) return `${h}h ${m}m`;
    return `${Math.max(1, m)} min`;
  }

  function fmtDateTime(ts) {
    if (!ts) return "–";
    const d = new Date(Number(ts) * 1000);
    const date = d.toLocaleDateString("de-DE", { day: "2-digit", month: "2-digit", year: "2-digit" });
    const time = d.toLocaleTimeString("de-DE", { hour: "2-digit", minute: "2-digit" });
    return `${date} · ${time}`;
  }

  function printerStatText(p) {
    if (p.pendingShutdown) {
      const t = p.nozzleTemp != null ? `${Math.round(p.nozzleTemp)}°` : "heiß";
      return `🌡 Kühlt ${t} → Aus`;
    }
    if (p.powerState === "OFF" && p.state === "OFFLINE") return "⏻ Ausgeschaltet";
    if (p.powerState === "BOOTING" && p.state === "OFFLINE") return "▲ Startet…";
    if (p.state === "READY") return "● Bereit";
    if (p.state === "UPLOADING") return `▲ Upload ${Math.round(p.progress || 0)} %`;
    if (p.state === "PRINTING") return p.progress != null ? `▲ Druckt ${Math.round(p.progress)} %` : "▲ Druckt";
    if (p.state === "PAUSED") return "▲ Pausiert";
    if (p.state === "ATTENTION") return `▲ ${p.error || "Dialog am Drucker"}`;
    return "✕ Offline";
  }

  function printerShortPill(p) {
    if (p.pendingShutdown) return "Kühlt ab";
    if (p.powerState === "OFF" && p.state === "OFFLINE") return "Aus";
    if (p.state === "READY") return "Bereit";
    if (p.state === "UPLOADING") return "Upload";
    if (p.state === "PRINTING") return "Druckt";
    if (p.state === "PAUSED") return "Pausiert";
    if (p.state === "ATTENTION") return "Dialog";
    return "Offline";
  }

  function nfcIconSvg() {
    return `<svg viewBox="0 0 80 80" fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round">
      <rect x="8" y="22" width="38" height="36" rx="5" stroke-width="4" fill="var(--surface)"/>
      <rect x="15" y="32" width="10" height="8" rx="1.5" stroke-width="2.6"/>
      <line x1="15" y1="48" x2="36" y2="48" stroke-width="3"/>
      <path d="M53 33a10 10 0 0 1 0 14" stroke-width="4"/>
      <path d="M60 26a19 19 0 0 1 0 28" stroke-width="4"/>
      <path d="M67 19a28 28 0 0 1 0 42" stroke-width="4"/>
    </svg>`;
  }

  function printer3dIconSvg() {
    return `<svg viewBox="0 0 96 88" width="100%" height="100%" aria-hidden="true">
      <rect x="10" y="10" width="76" height="68" rx="6" fill="none" stroke="var(--text)" stroke-width="5" opacity="0.82"/>
      <rect x="6" y="74" width="84" height="8" rx="3" fill="var(--text)" opacity="0.88"/>
      <rect x="20" y="65" width="56" height="5" rx="2" fill="var(--border-strong)"/>
      <line x1="12" y1="28" x2="84" y2="28" stroke="#ea580c" stroke-width="4.5" stroke-linecap="round"/>
      <rect x="38" y="19" width="20" height="17" rx="3" fill="var(--text)"/>
      <polygon points="44,36 52,36 48,43" fill="#ea580c"/>
      <polygon points="48,45 60,51 48,57 36,51" fill="#fb923c"/>
      <polygon points="36,51 48,57 48,65 36,59" fill="#ea580c"/>
      <polygon points="60,51 48,57 48,65 60,59" fill="#c2410c"/>
    </svg>`;
  }

  let toastTimer = null;
  function showToast(msg) {
    const el = $("toast");
    el.textContent = msg;
    el.classList.remove("hidden");
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => el.classList.add("hidden"), 3200);
  }

  function updateBrandLogo() {
    const logo = $("brand-logo");
    if (logo) logo.src = state.theme === "dark" ? "assets/freilab-logo-dark.svg" : "assets/freilab-logo.svg";
  }

  function setTheme(theme) {
    state.theme = theme === "dark" ? "dark" : "light";
    document.documentElement.setAttribute("data-theme", state.theme);
    localStorage.setItem("st3d_theme", state.theme);
    updateBrandLogo();
    if (state.screen === "settings") {
      document.querySelectorAll("[data-set-theme]").forEach((btn) => {
        const active = btn.dataset.setTheme === state.theme;
        btn.classList.toggle("btn-primary", active);
        btn.classList.toggle("btn-secondary", !active);
      });
    }
  }

  async function api(path, opts = {}) {
    const extraHeaders = opts.adminCode ? { "X-Admin-Code": opts.adminCode } : {};
    const res = await fetch(path, {
      headers: { "Content-Type": "application/json", ...extraHeaders, ...(opts.headers || {}) },
      ...opts,
    });
    const data = await res.json().catch(() => ({}));
    if (!res.ok) {
      const msg = data?.detail?.message || data?.detail || `HTTP ${res.status}`;
      throw new Error(msg);
    }
    return data;
  }

  function fireTouchFeedback() {
    if (state.displaySettings.buzzerMode !== "all") return;
    fetch("/api/feedback/touch", { method: "POST" }).catch(() => {});
  }

  function setLedHint(mode) {
    fetch("/api/feedback/led", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ mode }),
    }).catch(() => {});
  }

  // --- Display Power / Dimming / 30-min Burn-In Sleep ---
  function applyPowerOverlay() {
    const ov = $("power-overlay");
    if (!ov) return;
    ov.classList.remove("hidden", "mode-dim", "mode-sleep");
    if (state.powerState === "active") {
      ov.classList.add("hidden");
    } else if (state.powerState === "dim") {
      ov.classList.add("mode-dim");
    } else {
      ov.classList.add("mode-sleep");
    }
  }

  function setDisplayPower(target) {
    if (state.powerState === target) return;
    state.powerState = target;
    applyPowerOverlay();
    fetch("/api/display/power", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ state: target }),
    }).catch(() => {});
  }

  function markActivity(wakeOnly = false) {
    state.lastActivityAt = Date.now();
    if (state.powerState !== "active") {
      setDisplayPower("active");
      return true;
    }
    return wakeOnly ? false : false;
  }

  let saveSettingsTimer = null;
  let lastSettingsSentAt = 0;
  function _sendDisplaySettings() {
    lastSettingsSentAt = Date.now();
    api("/api/display/settings", {
      method: "POST",
      body: JSON.stringify(state.displaySettings),
    }).catch(() => {});
  }
  function persistDisplaySettings(immediate = false) {
    clearTimeout(saveSettingsTimer);
    const elapsed = Date.now() - lastSettingsSentAt;
    if (immediate || elapsed >= 65) {
      _sendDisplaySettings();
    } else {
      saveSettingsTimer = setTimeout(_sendDisplaySettings, 65 - elapsed);
    }
  }

  function selectedJob() {
    return state.jobs.find((j) => j.id === state.selectedJobId) || null;
  }

  function setTimeoutRing(seconds, onExpire) {
    if (!seconds) {
      state.timeoutEnd = null;
      state.timeoutAction = null;
      return;
    }
    state.timeoutTotal = seconds;
    state.timeoutEnd = Date.now() + seconds * 1000;
    state.timeoutAction = onExpire;
  }

  function updateTimeoutDOM() {
    if (!state.timeoutEnd) return;
    const remMs = state.timeoutEnd - Date.now();
    if (remMs <= 0) {
      const act = state.timeoutAction;
      setTimeoutRing(null, null);
      if (act) act();
      return;
    }
    const remSec = Math.ceil(remMs / 1000);
    const ratio = Math.max(0, Math.min(1, remMs / (state.timeoutTotal * 1000)));
    const fg = $("active-timeout-fg");
    const txt = $("active-timeout-sec");
    if (fg) fg.style.strokeDashoffset = String((RING_CIRC * (1 - ratio)).toFixed(1));
    if (txt) txt.textContent = `${remSec} s`;
  }

  function timeoutPillHtml() {
    const rem = state.timeoutEnd ? Math.max(0, Math.ceil((state.timeoutEnd - Date.now()) / 1000)) : state.timeoutTotal;
    return `
      <div class="timeout-pill">
        <div class="ring-badge">
          <svg viewBox="0 0 40 40">
            <circle cx="20" cy="20" r="18" fill="none" stroke="var(--border)" stroke-width="4"></circle>
            <circle id="active-timeout-fg" cx="20" cy="20" r="18" fill="none" stroke="var(--blue)" stroke-width="4" stroke-linecap="round" stroke-dasharray="${RING_CIRC}" stroke-dashoffset="0"></circle>
          </svg>
        </div>
        <span id="active-timeout-sec">${rem} s</span>
      </div>
    `;
  }

  async function cancelActiveSession() {
    if (!state.session) return;
    const sid = state.session.sessionId;
    state.session = null;
    try {
      await api(`/api/session/cancel?sessionId=${encodeURIComponent(sid)}`, { method: "POST" });
    } catch {}
  }

  async function returnToQueue(cancelSession = true) {
    if (cancelSession) await cancelActiveSession();
    state.screen = "queue";
    state.rfidPurpose = null;
    state.rfidAutoTriggered = false;
    state.rfidError = "";
    state.selectedJobId = null;
    state.powerPrinterId = null;
    state.selectedSectionId = null;
    state.pinMode = null;
    state.pinValue = "";
    state.pinError = "";
    state.adminCode = null;
    state.adminUser = null;
    state.printerPopupSlot = null;
    setTimeoutRing(null, null);
    setLedHint("idle");
    renderAll();
  }

  function openScreen(name) {
    markActivity();
    state.screen = name;
    state.printerPopupSlot = null;
    state.uploadPopupJobId = null;
    if (["rfid_wait", "bv_wait", "payment_confirm", "club_section", "pin_entry", "account", "topup"].includes(name)) {
      setTimeoutRing(60, () => returnToQueue(true));
    } else if (["overdraft_confirm", "not_approved", "printer_busy"].includes(name)) {
      setTimeoutRing(30, () => returnToQueue(true));
    } else if (name === "admin") {
      setTimeoutRing(180, () => returnToQueue(true));
    } else if (name === "approved_ok") {
      setTimeoutRing(4, () => {
        if (state.rfidPurpose === "power_on") return executePowerOn();
        openScreen(state.rfidPurpose === "print" ? "payment_confirm" : "account");
      });
    } else {
      setTimeoutRing(null, null);
    }

    // LED choreography hints per screen
    if (name === "rfid_wait" || name === "bv_wait") setLedHint("rfid");
    else if (name === "overdraft_confirm" || name === "not_approved" || name === "printer_busy") setLedHint("warn");
    else if (name === "payment_confirm" || name === "account" || name === "admin") setLedHint("pay");
    else if (name === "queue") setLedHint("idle");

    renderAll();
  }

  function goBackOneStep() {
    const s = state.screen;
    if (s === "overdraft_confirm" || s === "club_section") return openScreen("payment_confirm");
    if (s === "pin_entry") return openScreen(state.pinMode === "club" ? "club_section" : "payment_confirm");
    if (s === "topup") return openScreen(state.topupReturnTo || "payment_confirm");
    if (s === "printer_busy") return openScreen(state.session ? "payment_confirm" : "queue");
    if (s === "bv_wait") return openScreen("not_approved");
    if (s === "admin") {
      state.adminCode = null;
      return openScreen("settings");
    }
    if (s === "rfid_wait" && state.rfidPurpose === "admin") return openScreen("settings");
    return returnToQueue(true);
  }

  // --- Data Sync & SSE ---
  async function loadState() {
    try {
      const data = await api("/api/state");
      state.terminalId = data.terminalId || state.terminalId;
      state.terminalIp = data.terminalIp || state.terminalIp;
      state.version = data.version || state.version;
      state.serviceName = data.serviceName || "SmartTerminal 3D-Print";
      state.printers = data.printers || [];
      state.jobs = (data.jobs || []).map((j) => ({
        ...j,
        _ttlTotal: Math.max(60, Number(j.expiresAt - j.createdAt) || 300),
        _clientExpiresAt: Date.now() + (j.remainingSeconds || 0) * 1000,
      }));
      state.sections = data.sections || [];
      state.modes = data.modes || { devMock: false };
      state.features = data.features || {};
      if (data.displaySettings) {
        state.displaySettings = { ...state.displaySettings, ...data.displaySettings };
      }
      state.connected = true;
      state.networkOnline = data.networkOnline !== false;

      if (
        state.selectedJobId &&
        ["rfid_wait", "payment_confirm", "overdraft_confirm", "club_section", "pin_entry"].includes(state.screen) &&
        !selectedJob()
      ) {
        showToast("Der ausgewählte Druckjob ist abgelaufen.");
        await returnToQueue(true);
        return;
      }

      if (state.uploadPopupJobId && !state.jobs.some((j) => j.id === state.uploadPopupJobId && j.status === "sending")) {
        state.uploadPopupJobId = null;
      }

      renderTopbar();
      renderDock();
      renderDevControls();
      if (state.screen === "queue") renderStage();
      if (state.printerPopupSlot != null) renderPrinterPopup();
      if (state.uploadPopupJobId != null) renderUploadPopup();
    } catch {
      state.connected = false;
      renderTopbar();
    }
  }

  async function loadAdminData() {
    if (!state.adminCode) return;
    try {
      const [cfg, hist] = await Promise.all([
        api("/api/admin/config", { adminCode: state.adminCode }),
        api("/api/admin/history?limit=50", { adminCode: state.adminCode }),
      ]);
      state.adminConfig = cfg;
      state.adminHistory = hist.items || [];
      if (state.screen === "admin") renderStage();
    } catch (err) {
      showToast(err.message);
      state.adminCode = null;
      openScreen("settings");
    }
  }

  let eventSource = null;
  let sseReconnectTimer = null;

  function scheduleSseReconnect() {
    if (sseReconnectTimer) return;
    sseReconnectTimer = setTimeout(() => {
      sseReconnectTimer = null;
      connectEvents();
    }, 2000);
  }

  function connectEvents() {
    if (eventSource) {
      try { eventSource.close(); } catch {}
      eventSource = null;
    }
    const es = new EventSource("/api/events");
    eventSource = es;
    es.addEventListener("connected", () => {
      state.connected = true;
      renderTopbar();
      loadState();
    });
    es.addEventListener("queue_updated", async (ev) => {
      let payload = {};
      try {
        payload = JSON.parse(ev.data)?.data || {};
      } catch {}
      await loadState();

      // Auto-RFID on incoming PrusaSlicer upload when idle on Home screen
      if (payload.action === "uploaded" && payload.autoRfid && payload.jobId) {
        markActivity();
        const isIdleOnHome =
          state.screen === "queue" &&
          state.printerPopupSlot == null &&
          state.uploadPopupJobId == null &&
          !state.donePopup;
        if (isIdleOnHome && state.jobs.some((j) => j.id === payload.jobId)) {
          state.selectedJobId = payload.jobId;
          state.rfidPurpose = "print";
          state.rfidAutoTriggered = true;
          state.rfidError = "";
          openScreen("rfid_wait");
        } else if (!isIdleOnHome) {
          showToast(`Neuer Auftrag für Drucker ${payload.slot || ""} eingetroffen`);
        }
      }
    });
    es.addEventListener("printers_updated", () => loadState());
    es.addEventListener("printer_power_changed", (ev) => {
      try {
        const p = JSON.parse(ev.data)?.data;
        loadState();
        if (p?.action === "powered_off" && p?.reason === "COOLDOWN_COMPLETE") {
          showToast(`⏻ Drucker ${p.printerSlot || ""} abgekühlt und automatisch ausgeschaltet`);
        }
      } catch {}
    });
    es.addEventListener("rfid_scan", (ev) => {
      try {
        const uid = JSON.parse(ev.data)?.data?.uid;
        if (uid) {
          markActivity();
          handleRfidScan(uid);
        }
      } catch {}
    });
    es.addEventListener("job_progress", (ev) => {
      try {
        const p = JSON.parse(ev.data)?.data;
        if (!p) return;
        const job = state.jobs.find((j) => j.id === p.jobId);
        if (job) {
          job.uploadProgress = p;
          const fill = document.querySelector(`[data-upload-fill="${p.jobId}"]`);
          const pctEl = document.querySelector(`[data-upload-pct="${p.jobId}"]`);
          const mbEl = document.querySelector(`[data-upload-mb="${p.jobId}"]`);
          if (fill) fill.style.width = `${Math.round(p.percent || 0)}%`;
          if (pctEl) pctEl.textContent = p.stage === "starting" ? "Startet…" : `${Math.round(p.percent || 0)} %`;
          if (mbEl) mbEl.textContent = `${fmtMB(p.sentBytes)}/${fmtMB(p.totalBytes)}`;
        }
        const printer = state.printers.find((x) => x.id === p.printerId);
        if (printer) {
          printer.state = "UPLOADING";
          printer.progress = p.percent;
          const pSub = document.querySelector(`[data-printer-sub="${printer.id}"]`);
          if (pSub) pSub.textContent = `▲ Upload ${Math.round(p.percent || 0)} %`;
        }
        if (state.uploadPopupJobId === p.jobId) renderUploadPopup();
      } catch {}
    });
    es.addEventListener("job_done", (ev) => {
      try {
        const p = JSON.parse(ev.data)?.data;
        if (state.uploadPopupJobId === p?.jobId) state.uploadPopupJobId = null;
        loadState();
        if (!p) return;
        showDonePopup({
          displayName: p.displayName || "",
          printerSlot: p.printerSlot || "",
          printAfterUpload: p.printAfterUpload !== false,
        });
      } catch {}
    });
    es.addEventListener("job_failed", (ev) => {
      try {
        const p = JSON.parse(ev.data)?.data;
        if (state.uploadPopupJobId === p?.jobId) state.uploadPopupJobId = null;
        loadState();
        if (!p) return;
        showToast(
          p.charged
            ? `Übertragung fehlgeschlagen (${p.reason}) — beim Neustart „Bereits bezahlt“ nutzen.`
            : `Übertragung fehlgeschlagen (${p.reason}).`
        );
      } catch {}
    });
    es.onerror = () => {
      state.connected = false;
      renderTopbar();
      // Always force clean reconnect (HTML5 EventSource never auto-retries after HTTP 502/503!)
      try { es.close(); } catch {}
      if (eventSource === es) eventSource = null;
      scheduleSseReconnect();
    };
  }

  // --- RFID Handling ---
  async function handleRfidScan(uid) {
    if (state.screen === "rfid_wait" && state.rfidPurpose === "admin") {
      try {
        const res = await api("/api/admin/authorize", {
          method: "POST",
          body: JSON.stringify({ rfid: uid }),
        });
        if (!res.authorized) {
          state.rfidError =
            res.reason === "BV_REQUIRED"
              ? "Keine BV-Berechtigung — Admin nur mit BV-Karte!"
              : `Karte abgelehnt (${res.reason || "unbekannt"})`;
          renderStage();
          return;
        }
        state.rfidError = "";
        state.adminCode = res.adminCode;
        state.adminUser = res.user;
        state.adminTab = "history";
        openScreen("admin");
        await loadAdminData();
      } catch (err) {
        showToast(err.message);
      }
      return;
    }

    if (state.screen === "rfid_wait") {
      try {
        const res = await api("/api/session/authorize", {
          method: "POST",
          body: JSON.stringify({ rfid: uid }),
        });
        if (!res.authorized) {
          state.rfidError = `Karte abgelehnt (${res.reason || "unbekannt"})`;
          renderStage();
          return;
        }
        state.rfidError = "";
        state.session = res.session;
        if (!state.session.approved) return openScreen("not_approved");
        if (state.rfidPurpose === "power_on") return executePowerOn();
        openScreen(state.rfidPurpose === "account" ? "account" : "payment_confirm");
      } catch (err) {
        showToast(err.message);
      }
      return;
    }

    if (state.screen === "bv_wait" && state.session) {
      try {
        const res = await api("/api/session/approve", {
          method: "POST",
          body: JSON.stringify({ sessionId: state.session.sessionId, instructorRfid: uid }),
        });
        if (!res.approved) {
          showToast("Diese Karte hat keine Einweisungsberechtigung.");
          return;
        }
        state.session = res.session;
        state.lastResult = { instructorName: res.instructorName };
        openScreen("approved_ok");
      } catch (err) {
        showToast(err.message);
      }
      return;
    }

    if (state.screen === "queue") {
      state.rfidPurpose = "account";
      state.rfidAutoTriggered = false;
      state.rfidError = "";
      state.screen = "rfid_wait";
      await handleRfidScan(uid);
    }
  }

  // --- Actions ---
  async function executeStart(mode, extra = {}) {
    const job = selectedJob();
    if (!job || !state.session) {
      showToast("Job oder Sitzung abgelaufen.");
      return returnToQueue(true);
    }
    try {
      const res = await api(`/api/jobs/${encodeURIComponent(job.id)}/start`, {
        method: "POST",
        body: JSON.stringify({
          sessionId: state.session.sessionId,
          mode,
          pin: extra.pin || null,
          sectionId: extra.sectionId || null,
          allowOverdraft: Boolean(extra.allowOverdraft),
        }),
      });

      if (res.started) {
        state.session = null;
        const actionWord = job.printAfterUpload === false ? "Upload" : "Übertragung";
        showToast(`✓ Freigegeben — ${actionWord} an Drucker ${job.printerSlot} läuft`);
        await returnToQueue(false);
        await loadState();
        return;
      }

      if (res.reason === "PRINTER_BUSY" || res.reason === "PRINTER_OFFLINE") {
        state.lastResult = res;
        return openScreen("printer_busy");
      }
      if (res.reason === "INSUFFICIENT_BALANCE") {
        state.session.balanceCents = res.balanceCents;
        return openScreen("payment_confirm");
      }
      if (res.reason === "PIN_INVALID" || res.reason === "PIN_REQUIRED") {
        state.pinValue = "";
        state.pinError = "PIN ist falsch — bitte erneut versuchen.";
        return renderStage();
      }
      if (res.reason === "JOB_EXPIRED") {
        showToast("Druckjob ist abgelaufen.");
        return returnToQueue(true);
      }
      showToast(`Fehler: ${res.reason || "Unbekannt"}`);
    } catch (err) {
      showToast(err.message);
    }
  }

  async function executeTopup(amountCents) {
    if (!state.session) return;
    try {
      const res = await api("/api/session/topup", {
        method: "POST",
        body: JSON.stringify({ sessionId: state.session.sessionId, amountCents }),
      });
      state.session = res.session;
      showToast(`✓ +${fmtEur(amountCents)} aufgeladen`);
      openScreen(state.topupReturnTo || "payment_confirm");
    } catch (err) {
      showToast(err.message);
    }
  }

  async function cancelOrAbortJob(jobId) {
    try {
      const res = await api(`/api/jobs/${encodeURIComponent(jobId)}`, { method: "DELETE" });
      if (state.uploadPopupJobId === jobId) state.uploadPopupJobId = null;
      if (res.wasUploading) {
        showToast("Übertragung abgebrochen — falls bezahlt, beim Neustart „Bereits bezahlt“ nutzen.");
      }
      await loadState();
    } catch (err) {
      showToast(err.message);
    }
  }

  async function executePowerOn() {
    const pid = state.powerPrinterId;
    const p = state.printers.find((x) => x.id === pid);
    if (!pid || !state.session) {
      showToast("Sitzung abgelaufen.");
      return returnToQueue(true);
    }
    try {
      const res = await api(`/api/printers/${encodeURIComponent(pid)}/power/on`, {
        method: "POST",
        body: JSON.stringify({ sessionId: state.session.sessionId }),
      });
      if (!res.ok) {
        if (res.reason === "NOT_APPROVED") return openScreen("not_approved");
        showToast(`Einschalten fehlgeschlagen (${res.reason || "Fehler"})`);
        return returnToQueue(true);
      }
      state.session = null;
      state.powerPrinterId = null;
      showToast(`✓ Drucker ${p ? p.slot : ""} eingeschaltet${res.state === "OFFLINE" ? " — fährt hoch…" : ""}`);
      await returnToQueue(false);
      await loadState();
    } catch (err) {
      showToast(err.message);
    }
  }

  async function executePowerOff(printerId) {
    const p = state.printers.find((x) => x.id === printerId);
    try {
      const res = await api(`/api/printers/${encodeURIComponent(printerId)}/power/off`, { method: "POST" });
      if (!res.ok) {
        const msgMap = {
          PRINTER_PRINTING: "Drucker druckt gerade — Ausschalten gesperrt.",
          PRINTER_UPLOADING: "Übertragung läuft — Ausschalten gesperrt.",
        };
        showToast(msgMap[res.reason] || `Ausschalten fehlgeschlagen (${res.reason || "Fehler"})`);
        return;
      }
      if (res.action === "cooldown_scheduled") {
        showToast(
          `🌡 Hotend ${Math.round(res.nozzleTemp)} °C (> ${Math.round(res.thresholdC)} °C) — Drucker ${p ? p.slot : ""} schaltet nach Abkühlen aus`
        );
      } else {
        showToast(`⏻ Drucker ${p ? p.slot : ""} ausgeschaltet`);
        state.printerPopupSlot = null;
      }
      await loadState();
    } catch (err) {
      showToast(err.message);
    }
  }

  async function executeCancelShutdown(printerId) {
    try {
      await api(`/api/printers/${encodeURIComponent(printerId)}/power/cancel-shutdown`, { method: "POST" });
      showToast("Automatisches Ausschalten abgebrochen");
      await loadState();
    } catch (err) {
      showToast(err.message);
    }
  }

  // --- Rendering ---
  function initMarquees() {
    requestAnimationFrame(() => {
      document.querySelectorAll(".job-name-wrap").forEach((wrap) => {
        const span = wrap.querySelector(".job-name");
        if (!span) return;
        const overflow = Math.ceil(span.scrollWidth - wrap.clientWidth);
        if (overflow > 4) {
          wrap.classList.add("is-overflowing");
          wrap.style.setProperty("--marquee-dist", `${overflow + 10}px`);
          const dur = Math.max(4.5, Math.min(11, 3 + overflow / 35));
          wrap.style.setProperty("--marquee-dur", `${dur.toFixed(1)}s`);
        } else {
          wrap.classList.remove("is-overflowing");
        }
      });
    });
  }

  function renderTopbar() {
    const wifiEl = $("wifi-ind");
    if (wifiEl) wifiEl.classList.toggle("offline", !(state.connected && state.networkOnline));
    updateBrandLogo();
    const brandSub = $("brand-sub");
    if (brandSub) {
      const isPilot = Boolean(state.modes.cashbox);
      brandSub.textContent = isPilot ? "Pilot · Kasse" : "3D-Print";
      brandSub.classList.toggle("is-pilot", isPilot);
    }
    const backBtn = $("btn-back-top");
    const accBtn = $("btn-account");
    const setupBtn = $("btn-setup");
    const titleEl = $("sb-title");

    const isHome = state.screen === "queue";
    const isSettings = state.screen === "settings";
    const isSettingsGroup = isSettings || state.screen === "admin";
    const hasBack = !isHome && !isSettings;
    const accShowsHome = !isHome && !isSettingsGroup;
    const hideAcc = Boolean(state.modes.cashbox) && !accShowsHome;

    backBtn.classList.toggle("hidden", !hasBack);
    titleEl.classList.toggle("hidden", hasBack);
    if (isHome) titleEl.textContent = "Warteschlange";
    else if (isSettings) titleEl.textContent = "Einstellungen";

    accBtn.classList.toggle("hidden", hideAcc);
    accBtn.classList.toggle("is-home", accShowsHome);
    accBtn.textContent = accShowsHome ? "⌂ Home" : "👤 Konto";
    setupBtn.classList.toggle("is-home", isSettingsGroup);
    setupBtn.textContent = isSettingsGroup ? "⌂" : "⚙";
  }

  function renderDock() {
    const dock = $("printer-dock");
    dock.style.gridTemplateColumns = state.printers.length <= 1 ? "1fr" : "repeat(2, minmax(0, 1fr))";
    dock.innerHTML = state.printers
      .map(
        (p) => `
        <div class="p-tile st-${escapeHtml(p.state)} ${p.pendingShutdown ? "is-cooling" : ""}" data-open-printer="${p.slot}">
          <div class="p-name">
            <span>Drucker ${p.slot}</span>
            <span class="p-model">${escapeHtml(p.model || p.name)}</span>
          </div>
          <div class="p-stat" data-printer-sub="${escapeHtml(p.id)}">${escapeHtml(printerStatText(p))}</div>
        </div>
      `
      )
      .join("");
  }

  function renderStage() {
    const stage = $("stage");
    const job = selectedJob();
    const sess = state.session;

    if (state.screen === "queue") {
      if (!state.jobs.length) {
        stage.innerHTML = `
          <div class="queue-empty">
            <div class="empty-icon">${printer3dIconSvg()}</div>
            <h2>Warte auf Druckaufträge</h2>
            <p>Sende deinen Auftrag im <strong>PrusaSlicer</strong> per <strong>Upload</strong> oder <strong>Upload &amp; Print</strong> an den gewünschten Drucker.</p>
          </div>
        `;
        return;
      }

      stage.innerHTML = `
        <div class="queue-wrap">
          <div class="queue-scroll" id="queue-scroll">
            ${state.jobs
              .map((j) => {
                const isSending = j.status === "sending";
                const thumb = j.thumbnailUrl ? `<img src="${escapeHtml(j.thumbnailUrl)}" alt="">` : "🧊";
                if (isSending) {
                  const up = j.uploadProgress || { percent: 0, sentBytes: 0, totalBytes: j.fileSize || 0 };
                  return `
                    <div class="job-card-wrap">
                      <div class="job-swipe-bg">✕ Abbrechen</div>
                      <div class="job-card is-uploading" data-job-id="${escapeHtml(j.id)}" data-open-upload="${escapeHtml(j.id)}">
                        <div class="j-info-row">
                          <div class="job-thumb">${thumb}</div>
                          <div class="j-info-col">
                            <div class="job-row1">
                              <div class="job-name-wrap"><span class="job-name">${escapeHtml(j.displayName || j.filename)}</span></div>
                              <span class="slot-badge">Drucker ${j.printerSlot}</span>
                            </div>
                            <div class="job-meta">
                              <span>⏱ <strong>${fmtDur(j.estimatedTimeSeconds)}</strong></span>
                              <span>Tippen für Details…</span>
                            </div>
                          </div>
                        </div>
                        <div class="j-action-row">
                          <div class="inline-up">
                            <div class="inline-up-track">
                              <div class="inline-up-fill" data-upload-fill="${escapeHtml(j.id)}" style="width:${Math.round(up.percent || 0)}%"></div>
                            </div>
                            <div class="inline-up-txt">
                              <span data-upload-pct="${escapeHtml(j.id)}">${Math.round(up.percent || 0)} %</span>
                              <span data-upload-mb="${escapeHtml(j.id)}">${fmtMB(up.sentBytes)}/${fmtMB(up.totalBytes || j.fileSize)}</span>
                            </div>
                          </div>
                          <button type="button" class="btn-abort-x" data-del-id="${escapeHtml(j.id)}" title="Abbrechen">✕</button>
                        </div>
                      </div>
                    </div>
                  `;
                }

                const rem = Math.max(0, Math.floor((j._clientExpiresAt - Date.now()) / 1000));
                const pct = Math.max(0, Math.min(100, (rem / (j._ttlTotal || 300)) * 100));
                const dash = ((100 - pct) * 0.628).toFixed(1);
                const warn = rem <= 60;
                const ctaLabel = j.printAfterUpload === false ? "HOCHLADEN" : "DRUCKEN";
                return `
                  <div class="job-card-wrap">
                    <div class="job-swipe-bg">✕ Job entfernen</div>
                    <div class="job-card" data-job-id="${escapeHtml(j.id)}">
                      <div class="j-info-row">
                        <div class="job-thumb">${thumb}</div>
                        <div class="j-info-col">
                          <div class="job-row1">
                            <div class="job-name-wrap"><span class="job-name">${escapeHtml(j.displayName || j.filename)}</span></div>
                            <span class="slot-badge">Drucker ${j.printerSlot}</span>
                          </div>
                          <div class="job-meta">
                            <span>⏱ <strong>${fmtDur(j.estimatedTimeSeconds)}</strong></span>
                            <span class="ttl-chip ${warn ? "warn" : ""}" data-ttl-wrap="${escapeHtml(j.id)}">
                              <svg class="ttl-ring" viewBox="0 0 24 24">
                                <circle cx="12" cy="12" r="10" fill="none" stroke="var(--border)" stroke-width="3.5"></circle>
                                <circle data-ttl-fg="${escapeHtml(j.id)}" cx="12" cy="12" r="10" fill="none" stroke="${warn ? "var(--orange)" : "var(--blue)"}" stroke-width="3.5" stroke-linecap="round" stroke-dasharray="62.8" stroke-dashoffset="${dash}"></circle>
                              </svg>
                              <span data-ttl-txt="${escapeHtml(j.id)}">${fmtCountdown(rem)}</span>
                            </span>
                          </div>
                        </div>
                      </div>
                      <div class="j-action-row">
                        <button type="button" class="btn-print-job" data-print-id="${escapeHtml(j.id)}">
                          <span>${ctaLabel} · ${fmtEur(j.priceCents)}</span>
                        </button>
                        <button type="button" class="btn-abort-x" data-del-id="${escapeHtml(j.id)}" title="Job entfernen">✕</button>
                      </div>
                    </div>
                  </div>
                `;
              })
              .join("")}
          </div>
        </div>
      `;
      initMarquees();
      return;
    }

    if (state.screen === "rfid_wait") {
      const powerPrinter = state.printers.find((x) => x.id === state.powerPrinterId);
      const isAdm = state.rfidPurpose === "admin";
      const isAcc = state.rfidPurpose === "account";
      const isPwr = state.rfidPurpose === "power_on" && powerPrinter;
      const heading = isAdm
        ? "Admin · BV-Karte"
        : isPwr
          ? `Drucker ${powerPrinter.slot} einschalten`
          : isAcc
            ? "Konto · Karte vorhalten"
            : "Karte vorhalten";
      const title = isAdm ? "BV-Karte vorhalten" : "Bitte Mitgliedskarte scannen";
      const desc = isAdm
        ? "Nur für Bereichsverantwortliche (BV): Halte deine BV-Karte an den NFC-Leser."
        : isPwr
          ? `Drucker ${powerPrinter.slot} (${escapeHtml(powerPrinter.model || powerPrinter.name)}) über easyVerwaltung einschalten.`
          : isAcc
            ? "Halte deine Karte an den NFC-Leser, um Guthaben & Buchungen anzuzeigen."
            : "Halte deine Karte an den leuchtenden NFC-Ring unterhalb des Displays.";

      const jobCardHtml =
        job && state.rfidPurpose === "print"
          ? `
            <div class="panel">
              ${state.rfidAutoTriggered ? `<div class="auto-badge">⚡ Neuer Auftrag aus PrusaSlicer empfangen</div>` : ""}
              <div class="j-info-row">
                <div class="job-thumb">${job.thumbnailUrl ? `<img src="${escapeHtml(job.thumbnailUrl)}" alt="">` : "🧊"}</div>
                <div class="j-info-col">
                  <div class="job-row1">
                    <div class="job-name-wrap"><span class="job-name">${escapeHtml(job.displayName || job.filename)}</span></div>
                    <span class="slot-badge">Drucker ${job.printerSlot}</span>
                  </div>
                  <div class="job-meta">
                    <span>${Math.round(job.filamentGrams || 0)} g ${escapeHtml(job.filamentType || "")} · ${fmtDur(job.estimatedTimeSeconds)}</span>
                  </div>
                  <div style="font-size:4.4cqw;font-weight:900;color:var(--green-ink);">${fmtEur(job.priceCents)} · Fertig ${fmtEta(job.estimatedTimeSeconds)}</div>
                </div>
              </div>
            </div>
          `
          : "";

      stage.innerHTML = `
        <div class="subscreen no-scroll">
          <div class="sub-heading">
            <h1>${heading}</h1>
            ${timeoutPillHtml()}
          </div>
          ${jobCardHtml}
          ${state.rfidError ? `<div class="panel err" style="text-align:center;font-size:3.6cqw;font-weight:800;">🚫 ${escapeHtml(state.rfidError)}</div>` : ""}
          <div class="rfid-center">
            <div class="rfid-circle">${nfcIconSvg()}</div>
            <h2>${title}</h2>
            <p>${desc}</p>
            <div class="arrow-down-hint">▼ NFC-Leser unten ▼</div>
          </div>
          ${job && state.rfidPurpose === "print" ? `<button type="button" class="btn btn-secondary" style="height:6.4cqh;" data-action="go-home">Zur Warteschlange</button>` : ""}
        </div>
      `;
      initMarquees();
      return;
    }

    if (state.screen === "payment_confirm" && job && sess) {
      const verb = job.printAfterUpload === false ? "HOCHLADEN" : "DRUCKEN";
      const thumb = job.thumbnailUrl ? `<img src="${escapeHtml(job.thumbnailUrl)}" alt="">` : "🧊";

      if (state.modes.cashbox) {
        stage.innerHTML = `
          <div class="subscreen">
            <div class="sub-heading">
              <h1>Zahlung an der Kasse</h1>
              ${timeoutPillHtml()}
            </div>

            <div class="panel">
              <div class="j-info-row">
                <div class="job-thumb">${thumb}</div>
                <div class="j-info-col">
                  <div class="job-row1">
                    <div class="job-name-wrap"><span class="job-name">${escapeHtml(job.displayName || job.filename)}</span></div>
                    <span class="slot-badge">Drucker ${job.printerSlot}</span>
                  </div>
                  <div class="job-meta"><span>${Math.round(job.filamentGrams || 0)} g ${escapeHtml(job.filamentType || "")} · ${fmtDur(job.estimatedTimeSeconds)}</span></div>
                  <div class="job-meta"><span>Fertig: <strong>${fmtEta(job.estimatedTimeSeconds)}</strong>${job.printAfterUpload === false ? " · Nur Upload" : ""}</span></div>
                </div>
              </div>
            </div>

            <div class="panel cash">
              <div class="cash-uid-pill">✓ Karte erkannt · ${escapeHtml(sess.user.display_name)}</div>
              <div class="cash-head">
                <div>
                  <div class="helper-txt">Bitte in die 3D-Druck-Kasse</div>
                  <div class="cash-amount">${fmtEur(job.priceCents)}</div>
                </div>
                <div class="cash-coin">🪙</div>
              </div>
              <div class="cash-question">Hast du ${fmtEur(job.priceCents)} in die Kasse eingeworfen?</div>
              <div class="helper-txt">Pilotbetrieb — bald wird der Betrag automatisch über dein FREILab-Konto abgebucht.</div>
            </div>

            <button type="button" class="btn btn-primary" data-action="pay-cashbox">✓ Ja, eingeworfen · ${verb}</button>
            <div class="btn-row2">
              <button type="button" class="btn btn-secondary" data-action="open-club">Vereinsdruck</button>
              <button type="button" class="btn btn-secondary" data-action="open-paid">Bereits bezahlt</button>
            </div>
          </div>
        `;
        initMarquees();
        return;
      }

      const after = sess.balanceCents - job.priceCents;
      const enough = after >= 0;
      const maxNeg = Number(state.features.maxNegativeBalanceCents || 0);
      const canOverdraft = !enough && Boolean(state.features.allowNegativeBalance) && (maxNeg === 0 || after >= -maxNeg);

      stage.innerHTML = `
        <div class="subscreen">
          <div class="sub-heading">
            <h1>Zahlung bestätigen</h1>
            ${timeoutPillHtml()}
          </div>

          <div class="panel">
            <div class="j-info-row">
              <div class="job-thumb">${thumb}</div>
              <div class="j-info-col">
                <div class="job-row1">
                  <div class="job-name-wrap"><span class="job-name">${escapeHtml(job.displayName || job.filename)}</span></div>
                  <span class="slot-badge">Drucker ${job.printerSlot}</span>
                </div>
                <div class="job-meta"><span>${Math.round(job.filamentGrams || 0)} g ${escapeHtml(job.filamentType || "")} · ${fmtDur(job.estimatedTimeSeconds)}</span></div>
                <div class="job-meta"><span>Fertig: <strong>${fmtEta(job.estimatedTimeSeconds)}</strong>${job.printAfterUpload === false ? " · Nur Upload" : ""}</span></div>
              </div>
            </div>
          </div>

          <div class="panel ${!enough ? "warn" : ""}">
            <div class="pay-line">
              <span>👤 ${escapeHtml(sess.user.display_name)}</span>
              <span>${fmtEur(sess.balanceCents)}</span>
            </div>
            <div class="pay-line divider">
              <span style="color:var(--muted);">Druckkosten</span>
              <span class="pay-price-big">${fmtEur(job.priceCents)}</span>
            </div>
            <div class="pay-line" style="color:${enough ? "var(--green-ink)" : "var(--red-ink)"};">
              <span>Neues Guthaben</span>
              <span>${fmtEur(after)}${!enough ? " (Minus)" : ""}</span>
            </div>
          </div>

          ${
            enough
              ? `<button type="button" class="btn btn-primary" data-action="pay-personal">✓ JETZT ${verb} · ${fmtEur(job.priceCents)}</button>`
              : canOverdraft
                ? `<button type="button" class="btn btn-orange" data-action="open-overdraft">⚠ Auf Minus ${verb.toLowerCase()} (${fmtEur(job.priceCents)})</button>`
                : `<div class="panel err" style="font-size:3.5cqw;font-weight:800;text-align:center;">Guthaben reicht nicht (Limit −${fmtEur(maxNeg)}). Bitte aufladen.</div>`
          }

          ${!enough && state.features.topup ? `<button type="button" class="btn btn-blue" style="height:6.4cqh;" data-action="open-topup">+ Guthaben aufladen</button>` : ""}

          <div class="btn-row2">
            <button type="button" class="btn btn-secondary" data-action="open-club">Vereinsdruck</button>
            <button type="button" class="btn btn-secondary" data-action="open-paid">Bereits bezahlt</button>
          </div>
        </div>
      `;
      initMarquees();
      return;
    }

    if (state.screen === "overdraft_confirm" && job && sess) {
      const after = sess.balanceCents - job.priceCents;
      const verb = job.printAfterUpload === false ? "hochladen" : "drucken";
      stage.innerHTML = `
        <div class="subscreen">
          <div class="sub-heading">
            <h1>Auf Guthaben-Minus?</h1>
            ${timeoutPillHtml()}
          </div>
          <div class="panel warn">
            <div style="font-size:4.2cqw;font-weight:900;">⚠ Dein Guthaben geht ins Minus</div>
            <div class="pay-line"><span>Aktuelles Guthaben</span><span>${fmtEur(sess.balanceCents)}</span></div>
            <div class="pay-line"><span>Druckkosten</span><span>${fmtEur(job.priceCents)}</span></div>
            <div class="pay-line divider" style="color:var(--red-ink);">
              <span>Neuer Stand</span>
              <span class="pay-price-big">${fmtEur(after)}</span>
            </div>
            <div class="helper-txt" style="color:var(--orange-ink);">Bitte lade dein Guthaben zeitnah wieder auf.</div>
          </div>
          <button type="button" class="btn btn-orange" data-action="pay-overdraft">Ja, auf Minus ${verb}</button>
          ${state.features.topup ? `<button type="button" class="btn btn-blue" style="height:6.4cqh;" data-action="open-topup">+ Jetzt aufladen</button>` : ""}
          <div class="btn-row2">
            <button type="button" class="btn btn-secondary" data-action="open-club">Vereinsdruck</button>
            <button type="button" class="btn btn-secondary" data-action="open-paid">Bereits bezahlt</button>
          </div>
        </div>
      `;
      return;
    }

    if (state.screen === "club_section" && job) {
      const hint = state.modes.cashbox
        ? "Für welchen FREILab-Bereich ist der Druck? (Kein Einwurf nötig, wird im Log vermerkt.)"
        : "Wähle den Bereich, auf den der Druck verbucht wird (anschließend PIN-Bestätigung):";
      stage.innerHTML = `
        <div class="subscreen no-scroll">
          <div class="sub-heading">
            <h1>Vereinsdruck · Bereich</h1>
            ${timeoutPillHtml()}
          </div>
          <div class="helper-txt">${hint}</div>
          <div class="list-scroll">
            <div class="choice-grid">
              ${state.sections
                .map((s) => `<button type="button" class="btn btn-secondary" style="height:7.6cqh;font-size:3.7cqw;" data-section-id="${escapeHtml(s.sectionId)}">${escapeHtml(s.name)}</button>`)
                .join("")}
            </div>
          </div>
        </div>
      `;
      return;
    }

    if (state.screen === "pin_entry") {
      const secName = state.sections.find((s) => s.sectionId === state.selectedSectionId)?.name || "";
      const subLabel = state.pinMode === "club" ? `Vereinsdruck · ${secName}` : "Bereits bezahlt (Wiederholungsdruck)";
      stage.innerHTML = `
        <div class="subscreen no-scroll">
          <div class="sub-heading">
            <h1>PIN bestätigen</h1>
            ${timeoutPillHtml()}
          </div>
          <div class="panel" style="align-items:center;text-align:center;">
            <div style="font-size:3.5cqw;font-weight:800;color:var(--muted);">${escapeHtml(subLabel)}</div>
            <div class="pin-dots">
              ${[0, 1, 2, 3].map((i) => `<div class="pin-dot ${i < state.pinValue.length ? "filled" : ""}"></div>`).join("")}
            </div>
            ${state.pinError ? `<div style="color:var(--red-ink);font-size:3.4cqw;font-weight:800;">${escapeHtml(state.pinError)}</div>` : `<div class="helper-txt">4-stellige Mitglieder-PIN eingeben</div>`}
          </div>
          <div class="numpad">
            ${["1", "2", "3", "4", "5", "6", "7", "8", "9", "⌫", "0", "✓"]
              .map(
                (k) =>
                  `<button type="button" class="btn ${k === "✓" ? "btn-primary" : "btn-secondary"}" data-pin-key="${k}">${k}</button>`
              )
              .join("")}
          </div>
        </div>
      `;
      return;
    }

    if (state.screen === "topup" && sess) {
      stage.innerHTML = `
        <div class="subscreen">
          <div class="sub-heading">
            <h1>Guthaben aufladen</h1>
            ${timeoutPillHtml()}
          </div>
          <div class="panel">
            <div class="pay-line">
              <span>👤 ${escapeHtml(sess.user.display_name)}</span>
              <span style="color:var(--green-ink);">${fmtEur(sess.balanceCents)}</span>
            </div>
          </div>
          <div class="choice-grid">
            ${[500, 1000, 2000, 5000]
              .map((c) => `<button type="button" class="btn btn-primary" data-topup="${c}">+${fmtEur(c)}</button>`)
              .join("")}
          </div>
        </div>
      `;
      return;
    }

    if (state.screen === "account" && sess) {
      const txs = sess.transactions || [];
      stage.innerHTML = `
        <div class="subscreen no-scroll">
          <div class="sub-heading">
            <h1>Konto &amp; Guthaben</h1>
            ${timeoutPillHtml()}
          </div>
          <div class="panel">
            <div class="pay-line">
              <div>
                <div class="helper-txt">Angemeldet</div>
                <div style="font-size:4.5cqw;font-weight:900;">${escapeHtml(sess.user.display_name)}</div>
              </div>
              <div style="text-align:right;">
                <div class="helper-txt">Guthaben</div>
                <div class="pay-price-big" style="color:${sess.balanceCents >= 0 ? "var(--green-ink)" : "var(--red-ink)"};">${fmtEur(sess.balanceCents)}</div>
              </div>
            </div>
          </div>
          <div class="panel" style="flex:1;min-height:0;overflow:hidden;">
            <div style="font-size:3.5cqw;font-weight:800;color:var(--muted);">Letzte Buchungen</div>
            <div class="list-scroll">
              ${
                txs.length
                  ? txs
                      .map(
                        (tx) => `
                          <div class="tx-row">
                            <div style="min-width:0;">
                              <div style="white-space:nowrap;overflow:hidden;text-overflow:ellipsis;">${escapeHtml(tx.label || tx.description || "Buchung")}</div>
                              <div class="tx-sub">${fmtDateTime(tx.ts || tx.createdAt)}</div>
                            </div>
                            <span style="flex-shrink:0;color:${tx.amountCents >= 0 ? "var(--green-ink)" : "var(--red-ink)"};">${tx.amountCents >= 0 ? "+" : ""}${fmtEur(tx.amountCents)}</span>
                          </div>
                        `
                      )
                      .join("")
                  : `<div class="helper-txt">Noch keine Buchungen vorhanden.</div>`
              }
            </div>
          </div>
          ${state.features.topup ? `<button type="button" class="btn btn-primary" style="height:6.6cqh;" data-action="open-topup-account">+ Guthaben aufladen</button>` : ""}
        </div>
      `;
      return;
    }

    if (state.screen === "settings") {
      const ds = state.displaySettings;
      stage.innerHTML = `
        <div class="subscreen">
          <div class="panel">
            <div class="slider-head"><span>Darstellung</span></div>
            <div class="btn-row2">
              <button type="button" class="btn ${state.theme === "light" ? "btn-primary" : "btn-secondary"}" data-set-theme="light">☀ Hell</button>
              <button type="button" class="btn ${state.theme === "dark" ? "btn-primary" : "btn-secondary"}" data-set-theme="dark">🌙 Dunkel</button>
            </div>
          </div>

          <div class="panel">
            <div class="slider-head">
              <span>☀ Display-Helligkeit</span>
              <span id="brightness-val">${ds.brightness} %</span>
            </div>
            <input type="range" id="brightness-slider" min="15" max="100" step="5" value="${ds.brightness}">
          </div>

          <div class="panel" style="flex-direction:row;align-items:center;justify-content:space-between;">
            <div class="helper-txt">Terminal <strong>${escapeHtml(state.terminalId)}</strong><br>${escapeHtml(state.terminalIp)} · v${escapeHtml(state.version)}${state.modes.cashbox ? " · Pilot" : ""}</div>
            <button type="button" class="seg-btn" style="padding:0 3.5cqw;flex-shrink:0;" data-action="open-admin-auth">🔐 Admin (BV)</button>
          </div>
        </div>
      `;
      return;
    }

    if (state.screen === "admin") {
      const cfg = state.adminConfig || {};
      const hist = state.adminHistory || [];
      const ds = state.displaySettings;
      stage.innerHTML = `
        <div class="subscreen no-scroll">
          <div class="sub-heading">
            <h1>Admin${state.adminUser?.display_name ? ` · ${escapeHtml(state.adminUser.display_name)}` : ""}</h1>
            ${timeoutPillHtml()}
          </div>
          <div class="admin-tabs-3">
            <button type="button" class="seg-btn ${state.adminTab === "history" ? "active" : ""}" data-admin-tab="history">📋 Verlauf</button>
            <button type="button" class="seg-btn ${state.adminTab === "config" ? "active" : ""}" data-admin-tab="config">⚙ QR/Drucker</button>
            <button type="button" class="seg-btn ${state.adminTab === "device" ? "active" : ""}" data-admin-tab="device">🎛️ Gerät</button>
          </div>
          ${
            state.adminTab === "history"
              ? `
                <div class="list-scroll">
                  ${
                    hist.length
                      ? hist
                          .map((h) => {
                            const modeTxt =
                              h.paymentMode === "club"
                                ? `Verein${h.sectionName ? `: ${h.sectionName}` : ""}`
                                : h.paymentMode === "already_paid"
                                  ? "Bereits bezahlt"
                                  : h.paymentMode === "cashbox"
                                    ? "Kasse (Pilot)"
                                    : "Privat";
                            return `
                              <div class="panel" style="padding:2.4cqw;gap:1cqw;">
                                <div class="job-row1">
                                  <strong style="font-size:3.8cqw;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;">${escapeHtml(h.displayName)}</strong>
                                  <span class="slot-badge">Drucker ${h.printerSlot}</span>
                                </div>
                                <div class="pay-line" style="font-size:3.3cqw;">
                                  <span>👤 ${escapeHtml(h.userDisplayName)}</span>
                                  <span style="color:var(--muted);">${escapeHtml(modeTxt)}</span>
                                </div>
                                <div class="pay-line" style="font-size:3.1cqw;color:var(--muted);">
                                  <span>${fmtDateTime(h.startedAt)} · ${Math.round(h.filamentGrams || 0)} g ${escapeHtml(h.filamentType)}</span>
                                  <strong style="color:var(--text);">${fmtEur(h.priceCents)}</strong>
                                </div>
                              </div>
                            `;
                          })
                          .join("")
                      : `<div class="panel helper-txt" style="text-align:center;">Noch keine gestarteten Druckjobs im Log.</div>`
                  }
                </div>
              `
              : state.adminTab === "config"
                ? `
                  <div class="list-scroll">
                    <div class="panel qr-box">
                      <div style="font-size:3.8cqw;font-weight:900;">📱 Mit Smartphone konfigurieren</div>
                      ${
                        cfg.qrUrl
                          ? `<div class="qr-img-wrap"><img src="${escapeHtml(cfg.qrUrl)}" alt="Admin Setup QR-Code"></div>`
                          : ""
                      }
                      <div class="url-pill">${escapeHtml(cfg.setupUrl || `http://${state.terminalIp}/admin.html`)}</div>
                      <div class="helper-txt">QR-Code im FREILab-WLAN scannen, um <strong>Drucker (IP &amp; Keys)</strong>, Server-URL, Token &amp; CSV-Log bequem am Handy zu verwalten (10 min gültig).</div>
                    </div>
                    <div class="panel" style="gap:1.4cqw;">
                      <div class="pay-line" style="font-size:3.4cqw;"><span style="color:var(--muted);">Konfigurierte Drucker</span><span>${state.printers.length}</span></div>
                      ${state.printers
                        .map(
                          (p) =>
                            `<div class="pay-line" style="font-size:3.1cqw;"><span style="color:var(--muted);">Drucker ${p.slot} (${escapeHtml(p.model)})</span><span>${escapeHtml(p.ip || "–")}</span></div>`
                        )
                        .join("")}
                    </div>
                    <div class="panel" style="gap:1.4cqw;">
                      <div class="pay-line" style="font-size:3.4cqw;"><span style="color:var(--muted);">Zahlungsmodus</span><span>${escapeHtml(cfg.paymentMode || state.modes.payment || "mock")}</span></div>
                      <div class="pay-line" style="font-size:3.4cqw;"><span style="color:var(--muted);">Terminal-ID</span><span>${escapeHtml(cfg.terminalId || state.terminalId)}</span></div>
                      <div class="pay-line" style="font-size:3.4cqw;"><span style="color:var(--muted);">Server-URL</span><span style="overflow:hidden;text-overflow:ellipsis;max-width:58%;">${escapeHtml(cfg.easyverwaltungUrl || "–")}</span></div>
                      <div class="pay-line" style="font-size:3.4cqw;"><span style="color:var(--muted);">Service-Token</span><span style="color:${cfg.hasToken ? "var(--green-ink)" : "var(--red-ink)"};">${cfg.hasToken ? "✓ Gesetzt" : "✕ Fehlt"}</span></div>
                      <button type="button" class="btn btn-secondary" style="height:6.2cqh;margin-top:1cqw;" data-action="admin-test">Verbindung testen</button>
                    </div>
                  </div>
                `
                : `
                  <div class="list-scroll">
                    <div class="panel">
                      <div class="slider-head">
                        <span>☀ Display-Helligkeit</span>
                        <span id="brightness-val">${ds.brightness} %</span>
                      </div>
                      <input type="range" id="brightness-slider" min="15" max="100" step="5" value="${ds.brightness}">
                    </div>
                    <div class="panel">
                      <div class="slider-head"><span>🌙 Dimmen &amp; 💤 Display aus</span></div>
                      <div class="helper-txt">Dimmen auf 20 % nach Inaktivität:</div>
                      <div class="seg-row">
                        ${[
                          [0, "Aus"],
                          [60, "1 min"],
                          [120, "2 min"],
                          [300, "5 min"],
                        ]
                          .map(
                            ([v, lbl]) =>
                              `<button type="button" class="seg-btn ${ds.dimDelaySeconds === v ? "active" : ""}" data-dim-delay="${v}">${lbl}</button>`
                          )
                          .join("")}
                      </div>
                      <div class="helper-txt" style="margin-top:0.6cqh;">Display komplett aus (0 % · Burn-in-Schutz) nach:</div>
                      <div class="seg-row">
                        ${[
                          [0, "Aus"],
                          [900, "15 min"],
                          [1800, "30 min"],
                          [3600, "60 min"],
                        ]
                          .map(
                            ([v, lbl]) =>
                              `<button type="button" class="seg-btn ${ds.sleepDelaySeconds === v ? "active" : ""}" data-sleep-delay="${v}">${lbl}</button>`
                          )
                          .join("")}
                      </div>
                    </div>
                    <div class="panel">
                      <div class="slider-head"><span>🔊 Buzzer-Feedback</span></div>
                      <div class="seg-row seg-3">
                        ${[
                          ["all", "Alle (inkl. Touch)"],
                          ["events", "Nur Hinweise"],
                          ["off", "Aus"],
                        ]
                          .map(
                            ([v, lbl]) =>
                              `<button type="button" class="seg-btn ${ds.buzzerMode === v ? "active" : ""}" data-buzzer-mode="${v}">${lbl}</button>`
                          )
                          .join("")}
                      </div>
                    </div>
                  </div>
                `
          }
        </div>
      `;
      return;
    }

    if (state.screen === "printer_busy") {
      stage.innerHTML = `
        <div class="subscreen">
          <div class="sub-heading">
            <h1>Drucker nicht bereit</h1>
            ${timeoutPillHtml()}
          </div>
          <div class="panel warn" style="align-items:center;text-align:center;padding:5cqh 4cqw;">
            <div style="font-size:11cqw;">⚠️</div>
            <div style="font-size:4.6cqw;font-weight:900;">${state.lastResult?.reason === "PRINTER_OFFLINE" ? "Drucker ist offline / ausgeschaltet" : "Drucker ist aktuell belegt"}</div>
            <div class="helper-txt" style="color:var(--orange-ink);">Es wurde noch nichts abgebucht. Bitte am Drucker prüfen (z. B. Dialog bestätigen oder einschalten).</div>
          </div>
          <button type="button" class="btn btn-secondary" data-action="go-home">Zur Warteschlange</button>
        </div>
      `;
      return;
    }

    if (state.screen === "not_approved" && sess) {
      stage.innerHTML = `
        <div class="subscreen">
          <div class="sub-heading">
            <h1>Keine 3D-Freigabe</h1>
            ${timeoutPillHtml()}
          </div>
          <div class="panel err" style="align-items:center;text-align:center;padding:5cqh 4cqw;">
            <div style="font-size:11cqw;">🚫</div>
            <div style="font-size:4.4cqw;font-weight:900;">${escapeHtml(sess.user.display_name)} ist noch nicht für 3D-Druck freigeschaltet</div>
            <div class="helper-txt" style="color:var(--red-ink);">Bitte lass dich von einem Einweiser / BV vor Ort freischalten.</div>
          </div>
          ${state.features.approval ? `<button type="button" class="btn btn-primary" data-action="open-bv-wait">Freigabe vor Ort (BV-Karte)</button>` : ""}
        </div>
      `;
      return;
    }

    if (state.screen === "bv_wait") {
      stage.innerHTML = `
        <div class="subscreen no-scroll">
          <div class="sub-heading">
            <h1>Einweiser-Karte</h1>
            ${timeoutPillHtml()}
          </div>
          <div class="rfid-center">
            <div class="rfid-circle">${nfcIconSvg()}</div>
            <h2>BV- / Einweiser-Karte scannen</h2>
            <p>Bitte berechtigte Einweisungs-Karte an den NFC-Leser halten.</p>
            <div class="arrow-down-hint">▼ NFC-Leser unten ▼</div>
          </div>
        </div>
      `;
      return;
    }

    if (state.screen === "approved_ok") {
      stage.innerHTML = `
        <div class="subscreen">
          <div class="panel" style="flex:1;align-items:center;justify-content:center;text-align:center;">
            <div class="done-icon">✓</div>
            <div style="font-size:5cqw;font-weight:900;">Nutzer freigeschaltet</div>
            <div class="helper-txt">Durch: ${escapeHtml(state.lastResult?.instructorName || "Einweiser")}</div>
          </div>
        </div>
      `;
    }
  }

  function renderUploadPopup() {
    const el = $("upload-popup");
    const job = state.jobs.find((j) => j.id === state.uploadPopupJobId && j.status === "sending");
    if (!job) {
      el.classList.add("hidden");
      return;
    }
    const up = job.uploadProgress || { percent: 0, sentBytes: 0, totalBytes: job.fileSize || 0, bytesPerSec: 0 };
    const thumb = job.thumbnailUrl ? `<img src="${escapeHtml(job.thumbnailUrl)}" alt="">` : "🧊";
    const speed = fmtSpeed(up.bytesPerSec);
    el.classList.remove("hidden");
    el.innerHTML = `
      <div class="popup-card">
        <div class="popup-head">
          <div class="popup-head-left">
            <button type="button" class="back-top-btn" data-close-upload="1">‹ Zurück</button>
            <h3>Übertragung · Drucker ${job.printerSlot}</h3>
          </div>
        </div>
        <div style="padding:3cqw;display:flex;flex-direction:column;gap:2.6cqw;">
          <div class="j-info-row">
            <div class="job-thumb">${thumb}</div>
            <div class="j-info-col">
              <strong style="font-size:4.2cqw;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;">${escapeHtml(job.displayName || job.filename)}</strong>
              <div class="helper-txt">✓ Freigegeben · ${fmtDur(job.estimatedTimeSeconds)}</div>
            </div>
          </div>
          <div class="inline-up">
            <div class="inline-up-track" style="height:2.6cqh;">
              <div class="inline-up-fill" style="width:${Math.round(up.percent || 0)}%"></div>
            </div>
            <div class="inline-up-txt" style="font-size:3.6cqw;">
              <span>${Math.round(up.percent || 0)} %</span>
              <span>${fmtMB(up.sentBytes)} / ${fmtMB(up.totalBytes || job.fileSize)}${speed ? ` · ${speed}` : ""}</span>
            </div>
          </div>
        </div>
        <div class="popup-foot">
          <button type="button" class="btn btn-danger-soft" style="height:6.4cqh;" data-abort-upload="${escapeHtml(job.id)}">✕ Übertragung abbrechen</button>
        </div>
      </div>
    `;
  }

  function renderPrinterPopup() {
    const el = $("printer-popup");
    if (state.printerPopupSlot == null) {
      el.classList.add("hidden");
      return;
    }
    const p = state.printers.find((x) => x.slot === state.printerPopupSlot);
    if (!p) {
      el.classList.add("hidden");
      return;
    }
    const nozzleStr = p.nozzleDiameter ? `${String(p.nozzleDiameter).replace(".", ",")} mm` : "–";
    const nozzleTemp = p.nozzleTemp != null ? `${Math.round(p.nozzleTemp)}°/${Math.round(p.targetNozzle || 0)}°` : "–";
    const bedTemp = p.bedTemp != null ? `${Math.round(p.bedTemp)}°/${Math.round(p.targetBed || 0)}°` : "–";
    const totalH = p.totalPrintHours != null ? `${p.totalPrintHours.toLocaleString("de-DE")} h` : "–";
    const totalKm = p.totalFilamentKm != null ? `${p.totalFilamentKm.toLocaleString("de-DE")} km` : "–";
    const hasActive = (p.state === "PRINTING" || p.state === "UPLOADING" || p.state === "PAUSED") && (p.activeJob || p.uploadProgress);
    const activeName = p.activeJob || state.jobs.find((j) => j.id === p.uploadProgress?.jobId)?.displayName || "Auftrag";
    const ownerLabel = p.activeJobOwner
      ? p.activeJobSection
        ? `${p.activeJobOwner} (Verein: ${p.activeJobSection})`
        : p.activeJobOwner
      : "Direkt am Drucker";
    const remTxt = p.timeRemainingSeconds ? `${fmtDur(p.timeRemainingSeconds)} (Fertig ${fmtEta(p.timeRemainingSeconds)})` : "";
    const thresh = Math.round(p.shutdownTempThresholdC || 50);
    const isHot = p.nozzleTemp != null && p.nozzleTemp > thresh;
    const isOff = p.state === "OFFLINE" || p.powerState === "OFF";
    const isBusyPower = p.state === "PRINTING" || p.state === "UPLOADING";

    let powerFootHtml = "";
    if (p.pendingShutdown) {
      const curT = p.nozzleTemp != null ? `${Math.round(p.nozzleTemp)} °C` : "heiß";
      powerFootHtml = `
        <div class="popup-foot">
          <div class="cooldown-note">🌡 Kühlt ab (${curT} → ≤ ${thresh} °C) — schaltet automatisch aus</div>
          <button type="button" class="btn btn-secondary" style="height:6.4cqh;" data-power-cancel="${escapeHtml(p.id)}">Auto-Aus abbrechen</button>
        </div>
      `;
    } else if (isOff) {
      powerFootHtml = `
        <div class="popup-foot">
          <button type="button" class="btn btn-primary" style="height:6.6cqh;" data-power-on="${escapeHtml(p.id)}">⏻ Einschalten (RFID-Karte)</button>
        </div>
      `;
    } else if (isBusyPower) {
      powerFootHtml = `
        <div class="popup-foot">
          <button type="button" class="btn btn-secondary" style="height:6.4cqh;" disabled>⏻ Druckt gerade — Ausschalten gesperrt</button>
        </div>
      `;
    } else {
      powerFootHtml = `
        <div class="popup-foot">
          <button type="button" class="btn btn-danger-soft" style="height:6.6cqh;" data-power-off="${escapeHtml(p.id)}">⏻ ${isHot ? `Nach Abkühlen aus (> ${thresh}°)` : "Drucker ausschalten"}</button>
        </div>
      `;
    }

    el.classList.remove("hidden");
    el.innerHTML = `
      <div class="popup-card">
        <div class="popup-head">
          <div class="popup-head-left">
            <button type="button" class="back-top-btn" data-close-printer="1">‹ Zurück</button>
            <h3>Drucker ${p.slot} · ${escapeHtml(p.model || p.name)}</h3>
          </div>
          <span class="p-status-pill st-${escapeHtml(p.state)} ${p.pendingShutdown ? "is-cooling" : ""}">${escapeHtml(printerShortPill(p))}</span>
        </div>
        <div class="printer-spec-grid">
          <div class="spec-chip"><div class="spec-k">Düse &amp; Temp.</div><div class="spec-v">${nozzleStr} · ${nozzleTemp}</div></div>
          <div class="spec-chip"><div class="spec-k">Heizbett</div><div class="spec-v">${bedTemp}</div></div>
          <div class="spec-chip"><div class="spec-k">Druckraum</div><div class="spec-v">${escapeHtml(p.buildVolume || "–")}</div></div>
          <div class="spec-chip"><div class="spec-k">On-Time</div><div class="spec-v">${fmtOnTime(p.onTimeSeconds)}</div></div>
          <div class="spec-chip"><div class="spec-k">Gesamtzeit</div><div class="spec-v">${totalH}</div></div>
          <div class="spec-chip"><div class="spec-k">Filament</div><div class="spec-v">${totalKm}</div></div>
          ${
            hasActive
              ? `
                <div class="p-job-banner">
                  <div style="display:flex;justify-content:space-between;gap:2cqw;font-size:3.6cqw;font-weight:900;">
                    <span style="overflow:hidden;text-overflow:ellipsis;white-space:nowrap;">${p.state === "UPLOADING" ? "Upload:" : p.state === "PAUSED" ? "Pausiert:" : "Aktiver Druck:"} ${escapeHtml(activeName)}</span>
                    <span style="flex-shrink:0;">${Math.round(p.progress || 0)} %</span>
                  </div>
                  <div class="p-job-meta-row">
                    <span>👤 Verantwortlich: <strong>${escapeHtml(ownerLabel)}</strong></span>
                    ${remTxt ? `<span>⏱ Verbleibend: ${escapeHtml(remTxt)}</span>` : ""}
                  </div>
                </div>
              `
              : `
                <div class="spec-chip" style="grid-column:span 2;display:flex;justify-content:space-between;align-items:center;">
                  <span class="spec-k">Status</span>
                  <span style="font-size:3.4cqw;font-weight:800;color:var(--muted);">${escapeHtml(p.error || (p.powerState === "OFF" ? "Ausgeschaltet" : "Kein aktiver Druck"))}</span>
                </div>
              `
          }
        </div>
        ${powerFootHtml}
      </div>
    `;
  }

  let donePopupTimer = null;
  function showDonePopup(info) {
    state.donePopup = info;
    renderDonePopup();
    clearTimeout(donePopupTimer);
    donePopupTimer = setTimeout(() => closeDonePopup(), 4000);
  }

  function closeDonePopup() {
    clearTimeout(donePopupTimer);
    if (!state.donePopup) return;
    state.donePopup = null;
    renderDonePopup();
    if (state.screen !== "queue") returnToQueue(false);
    setLedHint("idle");
  }

  function renderDonePopup() {
    const el = $("done-popup");
    if (!el) return;
    const d = state.donePopup;
    if (!d) {
      el.classList.add("hidden");
      return;
    }
    el.classList.remove("hidden");
    const slotTxt = d.printerSlot ? `Drucker ${d.printerSlot}` : "den Drucker";
    const subTxt = d.printAfterUpload
      ? `${d.displayName ? `„${escapeHtml(d.displayName)}“ — ` : ""}Druck wurde gestartet.`
      : `${d.displayName ? `„${escapeHtml(d.displayName)}“ — ` : ""}Datei gespeichert (Start am Drucker).`;
    el.innerHTML = `
      <div class="popup-card done-card">
        <div class="done-icon">✓</div>
        <h3>Druckjob erfolgreich an ${escapeHtml(slotTxt)} übertragen</h3>
        <p>${subTxt}</p>
        <button type="button" class="btn btn-primary" style="height:6.4cqh;" data-close-done="1">OK</button>
      </div>
    `;
  }

  function renderPopups() {
    renderUploadPopup();
    renderPrinterPopup();
    renderDonePopup();
  }

  function renderDevControls() {
    const show = Boolean(state.modes.devMock) && state.screen !== "admin";
    const drawerOpen = !$("dev-drawer").classList.contains("hidden");
    $("dev-pill").classList.toggle("hidden", !show || drawerOpen);
    $("dev-toggle-p1").classList.toggle("hidden", state.modes.printer === "live");
    if (!show) $("dev-drawer").classList.add("hidden");
  }

  function renderAll() {
    renderTopbar();
    renderStage();
    renderDock();
    renderPopups();
    renderDevControls();
  }

  // --- Global Activity, Wake Swallow & Tactile Touch Buzzer ---
  $("power-overlay").addEventListener("pointerdown", (ev) => {
    ev.preventDefault();
    ev.stopPropagation();
    markActivity();
  });

  $("hmi-root").addEventListener("pointerdown", (ev) => {
    if (ev.target.closest("#power-overlay")) return;
    markActivity();
    if (ev.target.closest("button, .p-tile, .job-card.is-uploading, input[type='range']")) {
      fireTouchFeedback();
    }
  });

  // --- Topbar & Dock & Popup Events ---
  $("btn-back-top").addEventListener("click", goBackOneStep);
  $("btn-theme-eye").addEventListener("click", () => setTheme(state.theme === "light" ? "dark" : "light"));

  $("btn-account").addEventListener("click", () => {
    if (state.screen !== "queue" && state.screen !== "settings" && state.screen !== "admin") {
      return returnToQueue(true);
    }
    state.selectedJobId = null;
    state.powerPrinterId = null;
    state.rfidPurpose = "account";
    state.rfidAutoTriggered = false;
    state.rfidError = "";
    openScreen(state.session ? "account" : "rfid_wait");
  });

  $("btn-setup").addEventListener("click", () => {
    if (state.screen === "settings" || state.screen === "admin") return returnToQueue(false);
    openScreen("settings");
  });

  $("printer-dock").addEventListener("click", (ev) => {
    const tile = ev.target.closest("[data-open-printer]");
    if (!tile) return;
    state.printerPopupSlot = Number(tile.dataset.openPrinter);
    renderPrinterPopup();
  });

  $("printer-popup").addEventListener("click", async (ev) => {
    const onBtn = ev.target.closest("[data-power-on]");
    if (onBtn) {
      state.powerPrinterId = onBtn.dataset.powerOn;
      state.selectedJobId = null;
      state.rfidPurpose = "power_on";
      state.rfidAutoTriggered = false;
      state.rfidError = "";
      state.printerPopupSlot = null;
      openScreen("rfid_wait");
      return;
    }
    const offBtn = ev.target.closest("[data-power-off]");
    if (offBtn) {
      await executePowerOff(offBtn.dataset.powerOff);
      return;
    }
    const cancelBtn = ev.target.closest("[data-power-cancel]");
    if (cancelBtn) {
      await executeCancelShutdown(cancelBtn.dataset.powerCancel);
      return;
    }
    if (ev.target.id === "printer-popup" || ev.target.closest("[data-close-printer]")) {
      state.printerPopupSlot = null;
      renderPrinterPopup();
    }
  });

  $("upload-popup").addEventListener("click", async (ev) => {
    const abortBtn = ev.target.closest("[data-abort-upload]");
    if (abortBtn) {
      await cancelOrAbortJob(abortBtn.dataset.abortUpload);
      return;
    }
    if (ev.target.id === "upload-popup" || ev.target.closest("[data-close-upload]")) {
      state.uploadPopupJobId = null;
      renderUploadPopup();
    }
  });

  $("done-popup").addEventListener("click", (ev) => {
    if (ev.target.id === "done-popup" || ev.target.closest("[data-close-done]")) {
      closeDonePopup();
    }
  });

  // Brightness range slider live input
  $("stage").addEventListener("input", (ev) => {
    if (ev.target.id === "brightness-slider") {
      const val = Number(ev.target.value);
      state.displaySettings.brightness = val;
      const lbl = $("brightness-val");
      if (lbl) lbl.textContent = `${val} %`;
      persistDisplaySettings();
    }
  });

  // Main stage clicks
  $("stage").addEventListener("click", async (ev) => {
    const delBtn = ev.target.closest("[data-del-id]");
    if (delBtn) {
      ev.stopPropagation();
      await cancelOrAbortJob(delBtn.dataset.delId);
      return;
    }
    const printBtn = ev.target.closest("[data-print-id]");
    if (printBtn) {
      state.selectedJobId = printBtn.dataset.printId;
      state.rfidPurpose = "print";
      state.rfidAutoTriggered = false;
      state.rfidError = "";
      openScreen("rfid_wait");
      return;
    }
    const upCard = ev.target.closest("[data-open-upload]");
    if (upCard) {
      state.uploadPopupJobId = upCard.dataset.openUpload;
      renderUploadPopup();
      return;
    }
    const themeBtn = ev.target.closest("[data-set-theme]");
    if (themeBtn) {
      setTheme(themeBtn.dataset.setTheme);
      return;
    }
    const dimBtn = ev.target.closest("[data-dim-delay]");
    if (dimBtn) {
      state.displaySettings.dimDelaySeconds = Number(dimBtn.dataset.dimDelay);
      persistDisplaySettings();
      document.querySelectorAll("[data-dim-delay]").forEach((b) =>
        b.classList.toggle("active", Number(b.dataset.dimDelay) === state.displaySettings.dimDelaySeconds)
      );
      return;
    }
    const sleepBtn = ev.target.closest("[data-sleep-delay]");
    if (sleepBtn) {
      state.displaySettings.sleepDelaySeconds = Number(sleepBtn.dataset.sleepDelay);
      persistDisplaySettings();
      document.querySelectorAll("[data-sleep-delay]").forEach((b) =>
        b.classList.toggle("active", Number(b.dataset.sleepDelay) === state.displaySettings.sleepDelaySeconds)
      );
      return;
    }
    const buzBtn = ev.target.closest("[data-buzzer-mode]");
    if (buzBtn) {
      state.displaySettings.buzzerMode = buzBtn.dataset.buzzerMode;
      persistDisplaySettings();
      document.querySelectorAll("[data-buzzer-mode]").forEach((b) =>
        b.classList.toggle("active", b.dataset.buzzerMode === state.displaySettings.buzzerMode)
      );
      return;
    }
    const adminTabBtn = ev.target.closest("[data-admin-tab]");
    if (adminTabBtn) {
      state.adminTab = adminTabBtn.dataset.adminTab;
      renderStage();
      return;
    }
    const secBtn = ev.target.closest("[data-section-id]");
    if (secBtn) {
      state.selectedSectionId = secBtn.dataset.sectionId;
      if (state.modes.cashbox) {
        await executeStart("club", { sectionId: state.selectedSectionId });
        return;
      }
      state.pinMode = "club";
      state.pinValue = "";
      state.pinError = "";
      openScreen("pin_entry");
      return;
    }
    const topupBtn = ev.target.closest("[data-topup]");
    if (topupBtn) {
      await executeTopup(Number(topupBtn.dataset.topup));
      return;
    }
    const pinBtn = ev.target.closest("[data-pin-key]");
    if (pinBtn) {
      const k = pinBtn.dataset.pinKey;
      state.pinError = "";
      if (k === "⌫") {
        state.pinValue = state.pinValue.slice(0, -1);
        renderStage();
      } else if (k === "✓") {
        if (state.pinValue.length < 4) {
          state.pinError = "Bitte mindestens 4 Ziffern eingeben.";
          renderStage();
          return;
        }
        await executeStart(state.pinMode, { pin: state.pinValue, sectionId: state.selectedSectionId });
      } else if (state.pinValue.length < 6) {
        state.pinValue += k;
        renderStage();
      }
      return;
    }

    const actBtn = ev.target.closest("[data-action]");
    if (!actBtn) return;
    const act = actBtn.dataset.action;

    if (act === "go-home") return returnToQueue(true);
    if (act === "pay-personal") return executeStart("personal");
    if (act === "pay-cashbox") return executeStart("cashbox");
    if (act === "open-overdraft") return openScreen("overdraft_confirm");
    if (act === "pay-overdraft") return executeStart("personal", { allowOverdraft: true });
    if (act === "open-topup") {
      state.topupReturnTo = "payment_confirm";
      return openScreen("topup");
    }
    if (act === "open-topup-account") {
      state.topupReturnTo = "account";
      return openScreen("topup");
    }
    if (act === "open-club") return openScreen("club_section");
    if (act === "open-paid") {
      if (state.modes.cashbox) return executeStart("already_paid");
      state.pinMode = "already_paid";
      state.pinValue = "";
      state.pinError = "";
      return openScreen("pin_entry");
    }
    if (act === "open-bv-wait") return openScreen("bv_wait");
    if (act === "open-admin-auth") {
      state.rfidPurpose = "admin";
      state.rfidAutoTriggered = false;
      state.rfidError = "";
      return openScreen("rfid_wait");
    }
    if (act === "admin-test") {
      try {
        const res = await api("/api/admin/test-connection", { method: "POST", adminCode: state.adminCode });
        showToast(res.message || (res.ok ? "Verbindung OK" : "Fehlgeschlagen"));
      } catch (err) {
        showToast(err.message);
      }
    }
  });

  // Swipe-left to delete/abort on job cards
  let swipeCard = null;
  let swipeStartX = 0;
  let swipeStartY = 0;
  let swipeDx = 0;
  let swipeActive = false;

  $("stage").addEventListener("pointerdown", (ev) => {
    if (ev.target.closest("button")) return;
    const card = ev.target.closest(".job-card");
    if (!card) return;
    swipeCard = card;
    swipeStartX = ev.clientX;
    swipeStartY = ev.clientY;
    swipeDx = 0;
    swipeActive = false;
  });

  window.addEventListener("pointermove", (ev) => {
    if (!swipeCard) return;
    const dx = ev.clientX - swipeStartX;
    const dy = ev.clientY - swipeStartY;
    if (!swipeActive) {
      if (Math.abs(dx) > 14 && Math.abs(dx) > Math.abs(dy) * 1.4 && dx < 0) {
        swipeActive = true;
        swipeCard.classList.add("swiping");
      } else if (Math.abs(dy) > 14) {
        swipeCard = null;
        return;
      }
    }
    if (swipeActive) {
      swipeDx = Math.min(0, Math.max(-160, dx));
      swipeCard.style.transform = `translateX(${swipeDx}px)`;
    }
  });

  window.addEventListener("pointerup", async () => {
    if (!swipeCard) return;
    const card = swipeCard;
    const dx = swipeDx;
    swipeCard = null;
    card.classList.remove("swiping");
    if (swipeActive && dx < -90) {
      card.style.transform = "translateX(-100%)";
      await cancelOrAbortJob(card.dataset.jobId);
    } else {
      card.style.transform = "";
    }
    swipeActive = false;
  });

  // Mouse/VNC drag-to-scroll fallback for scrollable lists (native touch uses pan-y)
  let dragBox = null;
  let dragStartY = 0;
  let dragStartTop = 0;
  let dragMoved = false;

  $("stage").addEventListener("pointerdown", (ev) => {
    if (ev.pointerType === "touch") return;
    if (ev.target.closest("input, select, textarea")) return;
    const box = ev.target.closest(".list-scroll, .queue-scroll, .subscreen:not(.no-scroll)");
    if (!box || box.scrollHeight <= box.clientHeight + 2) return;
    dragBox = box;
    dragStartY = ev.clientY;
    dragStartTop = box.scrollTop;
    dragMoved = false;
  });

  window.addEventListener("pointermove", (ev) => {
    if (!dragBox) return;
    const dy = ev.clientY - dragStartY;
    if (Math.abs(dy) > 6) dragMoved = true;
    if (dragMoved) dragBox.scrollTop = dragStartTop - dy;
  });

  window.addEventListener("pointerup", () => {
    if (!dragBox) return;
    dragBox = null;
    if (dragMoved) {
      const swallow = (e) => { e.stopPropagation(); e.preventDefault(); };
      window.addEventListener("click", swallow, { capture: true, once: true });
      setTimeout(() => window.removeEventListener("click", swallow, { capture: true }), 80);
    }
    dragMoved = false;
  });

  // Dev Simulator Drawer
  $("dev-pill").addEventListener("click", () => {
    $("dev-drawer").classList.remove("hidden");
    renderDevControls();
  });
  $("dev-close").addEventListener("click", () => {
    $("dev-drawer").classList.add("hidden");
    renderDevControls();
  });
  $("dev-upload").addEventListener("click", async () => {
    try {
      const slot = state.printers[0]?.slot || 1;
      await api("/api/dev/upload-sample", { method: "POST", body: JSON.stringify({ slot }) });
    } catch (err) {
      showToast(err.message);
    }
  });
  $("dev-drawer").addEventListener("click", async (ev) => {
    const rfidBtn = ev.target.closest("[data-rfid]");
    if (!rfidBtn) return;
    try {
      await api("/api/dev/simulate-rfid", {
        method: "POST",
        body: JSON.stringify({ uid: rfidBtn.dataset.rfid }),
      });
    } catch (err) {
      showToast(err.message);
    }
  });
  $("dev-toggle-p1").addEventListener("click", async () => {
    const p1 = state.printers[0];
    if (!p1) return;
    const next = p1.state === "READY" ? "PRINTING" : "READY";
    try {
      await api("/api/dev/printer-state", {
        method: "POST",
        body: JSON.stringify({ printerId: p1.id, state: next }),
      });
    } catch (err) {
      showToast(err.message);
    }
  });

  // --- Clock, Job TTL, Timeout Ring & 2-Stage Auto-Dim / 30-min Sleep Tick ---
  function tick() {
    const now = new Date();
    $("sb-clock").textContent = now.toLocaleTimeString("de-DE", { hour: "2-digit", minute: "2-digit" });

    if (state.screen === "queue") {
      const nowMs = Date.now();
      for (const j of state.jobs) {
        if (j.status === "sending") continue;
        const rem = Math.max(0, Math.floor((j._clientExpiresAt - nowMs) / 1000));
        const pct = Math.max(0, Math.min(100, (rem / (j._ttlTotal || 300)) * 100));
        const txt = document.querySelector(`[data-ttl-txt="${j.id}"]`);
        const fg = document.querySelector(`[data-ttl-fg="${j.id}"]`);
        const wrap = document.querySelector(`[data-ttl-wrap="${j.id}"]`);
        if (txt) txt.textContent = fmtCountdown(rem);
        if (fg) {
          fg.setAttribute("stroke-dashoffset", ((100 - pct) * 0.628).toFixed(1));
          fg.setAttribute("stroke", rem <= 60 ? "var(--orange)" : "var(--blue)");
        }
        if (wrap) wrap.classList.toggle("warn", rem <= 60);
      }
    }

    updateTimeoutDOM();

    // Auto-Dim & 30-min Display Sleep only when idle on Queue with no active upload or open popup
    const hasActiveUpload = state.jobs.some((j) => j.status === "sending");
    const isIdle =
      state.screen === "queue" &&
      !hasActiveUpload &&
      state.printerPopupSlot == null &&
      state.uploadPopupJobId == null &&
      !state.donePopup;

    if (isIdle) {
      const idleSec = (Date.now() - state.lastActivityAt) / 1000;
      const sleepSec = Number(state.displaySettings.sleepDelaySeconds || 0);
      const dimSec = Number(state.displaySettings.dimDelaySeconds || 0);
      if (sleepSec > 0 && idleSec >= sleepSec) {
        setDisplayPower("sleep");
      } else if (dimSec > 0 && idleSec >= dimSec && state.powerState !== "sleep") {
        setDisplayPower("dim");
      }
    }
  }

  setInterval(tick, 1000);
  setInterval(() => loadState(), 15000);

  // Boot
  updateBrandLogo();
  tick();
  loadState().then(() => {
    renderAll();
    connectEvents();
  });
})();
