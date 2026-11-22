(() => {
  if (new URLSearchParams(window.location.search).get("kiosk") === "1") {
    document.documentElement.classList.add("kiosk");
  }

  // Persisted theme: "light" (default for bright workshop) | "dark"
  const savedTheme = localStorage.getItem("st3d_theme") === "dark" ? "dark" : "light";
  document.documentElement.setAttribute("data-theme", savedTheme);

  const RING_CIRC = 113.1; // 2 * pi * 18

  const state = {
    theme: savedTheme,
    terminalId: "3dprint-01",
    terminalIp: "localhost",
    version: "0.3.0",
    serviceName: "SmartTerminal 3D-Print",
    printers: [],
    jobs: [],
    sections: [],
    modes: { devMock: false, printer: "mock" },
    features: { topup: false, approval: false, allowNegativeBalance: true, maxNegativeBalanceCents: 2000 },
    connected: false,

    // Screen state: queue | rfid_wait | payment_confirm | overdraft_confirm | topup | club_section | pin_entry | printer_busy | account | settings | admin | not_approved | bv_wait | approved_ok
    screen: "queue",
    rfidPurpose: null, // "print" | "account"
    selectedJobId: null,
    selectedSectionId: null,
    pinMode: null, // "club" | "already_paid"
    pinValue: "",
    pinError: "",
    session: null,
    lastResult: null,
    topupReturnTo: "payment_confirm",
    adminConfig: null,

    // Popups on top of current screen
    uploadPopupJobId: null,
    printerPopupSlot: null,

    // Timeout countdown
    timeoutEnd: null,
    timeoutTotal: 60,
    timeoutAction: null,
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
    return end.getDate() !== now.getDate() ? `Morgen ${hhmm}` : `ca. ${hhmm} Uhr`;
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
    if (sec == null) return "Offline";
    const d = Math.floor(sec / 86400);
    const h = Math.floor((sec % 86400) / 3600);
    const m = Math.floor((sec % 3600) / 60);
    if (d > 0) return `${d}d ${h}h`;
    if (h > 0) return `${h}h ${m}m`;
    return `${Math.max(1, m)} min`;
  }

  function fmtDateTime(ts) {
    const d = new Date(Number(ts || 0) * 1000);
    const date = d.toLocaleDateString("de-DE", { day: "2-digit", month: "2-digit", year: "numeric" });
    const time = d.toLocaleTimeString("de-DE", { hour: "2-digit", minute: "2-digit" });
    return `${date} · ${time}`;
  }

  function printerStatText(p) {
    if (p.state === "READY") return "● Bereit";
    if (p.state === "UPLOADING") return `▲ Überträgt ${Math.round(p.progress || 0)} %`;
    if (p.state === "PRINTING") return p.progress != null ? `▲ Druckt ${Math.round(p.progress)} %` : "▲ Druckt";
    if (p.state === "PAUSED") return "▲ Pausiert";
    if (p.state === "ATTENTION") return `▲ ${p.error || "Prüfen"}`;
    return "✕ Offline";
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
      <!-- Outer gantry frame -->
      <rect x="10" y="10" width="76" height="68" rx="6" fill="none" stroke="var(--text)" stroke-width="5" opacity="0.82"/>
      <!-- Base / print bed -->
      <rect x="6" y="74" width="84" height="8" rx="3" fill="var(--text)" opacity="0.88"/>
      <rect x="20" y="65" width="56" height="5" rx="2" fill="var(--border-strong)"/>
      <!-- X-axis rail -->
      <line x1="12" y1="28" x2="84" y2="28" stroke="#ea580c" stroke-width="4.5" stroke-linecap="round"/>
      <!-- Extruder carriage & nozzle -->
      <rect x="38" y="19" width="20" height="17" rx="3" fill="var(--text)"/>
      <polygon points="44,36 52,36 48,43" fill="#ea580c"/>
      <!-- 3D cube on build plate -->
      <polygon points="48,45 60,51 48,57 36,51" fill="#fb923c"/>
      <polygon points="36,51 48,57 48,65 36,59" fill="#ea580c"/>
      <polygon points="60,51 48,57 48,65 60,59" fill="#c2410c"/>
    </svg>`;
  }

  function printerSvg(model) {
    const m = String(model || "").toUpperCase();
    const isXL = m.includes("XL");
    const isMini = m.includes("MINI");
    const accent = "#ea580c";
    if (isMini) {
      return `<svg viewBox="0 0 120 110" width="100%" height="100%">
        <rect x="14" y="88" width="92" height="12" rx="3" fill="var(--border-strong)"/>
        <rect x="26" y="76" width="66" height="8" rx="2" fill="var(--text)" opacity="0.85"/>
        <rect x="86" y="20" width="10" height="68" rx="2" fill="var(--text)" opacity="0.75"/>
        <rect x="30" y="34" width="62" height="9" rx="2" fill="${accent}"/>
        <rect x="48" y="43" width="16" height="18" rx="3" fill="var(--text)"/>
        <polygon points="53,61 59,61 56,67" fill="${accent}"/>
      </svg>`;
    }
    return `<svg viewBox="0 0 120 110" width="100%" height="100%">
      <rect x="14" y="14" width="92" height="84" rx="5" fill="none" stroke="var(--text)" stroke-width="${isXL ? 7 : 6}" opacity="0.8"/>
      <rect x="22" y="78" width="76" height="9" rx="2" fill="var(--border-strong)"/>
      <rect x="20" y="38" width="80" height="7" rx="2" fill="${accent}"/>
      <rect x="48" y="32" width="24" height="22" rx="3" fill="var(--text)"/>
      ${isXL ? `<rect x="26" y="20" width="10" height="12" rx="2" fill="${accent}"/><rect x="40" y="20" width="10" height="12" rx="2" fill="${accent}"/><rect x="54" y="20" width="10" height="12" rx="2" fill="${accent}"/>` : ""}
      <polygon points="56,54 64,54 60,61" fill="${accent}"/>
      <rect x="24" y="92" width="26" height="10" rx="2" fill="${accent}"/>
    </svg>`;
  }

  let toastTimer = null;
  function showToast(msg) {
    const el = $("toast");
    el.textContent = msg;
    el.classList.remove("hidden");
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => el.classList.add("hidden"), 3400);
  }

  function setTheme(next) {
    state.theme = next === "dark" ? "dark" : "light";
    document.documentElement.setAttribute("data-theme", state.theme);
    localStorage.setItem("st3d_theme", state.theme);
    showToast(state.theme === "dark" ? "Ansicht: Dunkel" : "Ansicht: Hell");
    if (state.screen === "settings") renderStage();
  }

  async function api(path, opts = {}) {
    const res = await fetch(path, {
      headers: { "Content-Type": "application/json", ...(opts.headers || {}) },
      ...opts,
    });
    const data = await res.json().catch(() => ({}));
    if (!res.ok) {
      const msg = data?.detail?.message || data?.detail || `HTTP ${res.status}`;
      throw new Error(msg);
    }
    return data;
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

  function timeoutPillHtml(label = "Timeout:") {
    const rem = state.timeoutEnd ? Math.max(0, Math.ceil((state.timeoutEnd - Date.now()) / 1000)) : state.timeoutTotal;
    return `
      <div class="timeout-pill">
        <div class="ring-badge">
          <svg viewBox="0 0 40 40">
            <circle class="ring-bg" cx="20" cy="20" r="18"></circle>
            <circle class="ring-fg" id="active-timeout-fg" cx="20" cy="20" r="18" style="stroke-dashoffset:0;"></circle>
          </svg>
        </div>
        <span class="timeout-pill-lbl">${label} <strong id="active-timeout-sec">${rem} s</strong></span>
      </div>
    `;
  }

  async function returnToQueue(cancelSess = true) {
    if (cancelSess && state.session?.sessionId) {
      const sid = state.session.sessionId;
      state.session = null;
      api(`/api/session/cancel?sessionId=${encodeURIComponent(sid)}`, { method: "POST" }).catch(() => {});
    }
    state.screen = "queue";
    state.selectedJobId = null;
    state.selectedSectionId = null;
    state.pinValue = "";
    state.pinError = "";
    setTimeoutRing(null, null);
    renderAll();
  }

  function openScreen(name) {
    state.screen = name;
    state.printerPopupSlot = null;
    state.uploadPopupJobId = null;
    if (["rfid_wait", "bv_wait", "payment_confirm", "club_section", "pin_entry", "account", "topup"].includes(name)) {
      setTimeoutRing(60, () => returnToQueue(true));
    } else if (["overdraft_confirm", "not_approved", "printer_busy"].includes(name)) {
      setTimeoutRing(30, () => returnToQueue(true));
    } else if (name === "approved_ok") {
      setTimeoutRing(4, () => openScreen(state.rfidPurpose === "print" ? "payment_confirm" : "account"));
    } else {
      setTimeoutRing(null, null);
    }
    renderAll();
  }

  function goBackOneStep() {
    const s = state.screen;
    if (s === "overdraft_confirm" || s === "club_section") return openScreen("payment_confirm");
    if (s === "pin_entry") return openScreen(state.pinMode === "club" ? "club_section" : "payment_confirm");
    if (s === "topup") return openScreen(state.topupReturnTo || "payment_confirm");
    if (s === "printer_busy") return openScreen(state.session ? "payment_confirm" : "queue");
    if (s === "bv_wait") return openScreen("not_approved");
    if (s === "admin") return openScreen("settings");
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
      state.connected = true;

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
      renderPopups();
      if (state.screen === "queue") renderStage();
    } catch {
      state.connected = false;
      renderTopbar();
    }
  }

  function connectSSE() {
    const es = new EventSource("/api/events");
    es.addEventListener("connected", () => {
      state.connected = true;
      renderTopbar();
    });
    es.addEventListener("queue_updated", () => loadState());
    es.addEventListener("printers_updated", () => loadState());
    es.addEventListener("rfid_scan", (ev) => {
      try {
        const uid = JSON.parse(ev.data)?.data?.uid;
        if (uid) handleRfidScan(uid);
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
          if (pSub) pSub.textContent = `▲ Überträgt ${Math.round(p.percent || 0)} %`;
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
        if (p.printAfterUpload === false) {
          showToast(`✓ Auf Drucker ${p.printerSlot || ""} hochgeladen — am Drucker starten`);
        } else {
          showToast(`✓ Druck gestartet: ${p.displayName || ""} (Drucker ${p.printerSlot || ""})`);
        }
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
    };
  }

  // --- RFID Handling ---
  async function handleRfidScan(uid) {
    if (state.screen === "rfid_wait") {
      try {
        const res = await api("/api/session/authorize", {
          method: "POST",
          body: JSON.stringify({ rfid: uid }),
        });
        if (!res.authorized) {
          showToast(`Karte abgelehnt (${res.reason || "unbekannt"})`);
          return;
        }
        state.session = res.session;
        if (!state.session.approved) return openScreen("not_approved");
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

  // --- Rendering ---
  function initMarquees() {
    requestAnimationFrame(() => {
      document.querySelectorAll(".job-name-wrap").forEach((wrap) => {
        const span = wrap.querySelector(".job-name");
        if (!span) return;
        const overflow = Math.ceil(span.scrollWidth - wrap.clientWidth);
        if (overflow > 4) {
          wrap.classList.add("is-overflowing");
          wrap.style.setProperty("--marquee-dist", `${overflow + 12}px`);
          const dur = Math.max(4.5, Math.min(12, 3 + overflow / 35));
          wrap.style.setProperty("--marquee-dur", `${dur.toFixed(1)}s`);
        } else {
          wrap.classList.remove("is-overflowing");
        }
      });
    });
  }

  function renderTopbar() {
    $("conn-dot").classList.toggle("offline", !state.connected);
    const backBtn = $("btn-back-top");
    const accBtn = $("btn-account");
    const setupBtn = $("btn-setup");
    const titleEl = $("sb-title");
    const subEl = $("sb-sub");

    const isHome = state.screen === "queue";
    const isAccount = state.screen === "account";
    const isSettings = state.screen === "settings";
    const isSettingsGroup = isSettings || state.screen === "admin";

    backBtn.classList.toggle("hidden", isHome || isAccount || isSettings);
    accBtn.classList.toggle("is-home", isAccount);
    accBtn.textContent = isAccount ? "⌂ Home" : "👤 Konto";
    setupBtn.classList.toggle("is-home", isSettingsGroup);
    setupBtn.textContent = isSettingsGroup ? "⌂" : "⚙";

    const titles = {
      queue: ["Warteschlange", state.jobs.length ? `· ${state.jobs.length} ${state.jobs.length === 1 ? "Job" : "Jobs"}` : ""],
      rfid_wait: ["RFID-Karte vorhalten", selectedJob() ? `· ${selectedJob().displayName}` : "· Konto"],
      payment_confirm: ["Zahlung bestätigen", state.session ? `· ${state.session.user.display_name}` : ""],
      overdraft_confirm: ["Auf Guthaben-Minus drucken?", state.session ? `· ${state.session.user.display_name}` : ""],
      topup: ["Guthaben aufladen", state.session ? `· ${state.session.user.display_name}` : ""],
      club_section: ["Vereinsdruck — Bereich", ""],
      pin_entry: ["PIN bestätigen", state.pinMode === "club" ? "· Vereinsdruck" : "· Bereits bezahlt"],
      printer_busy: ["Drucker nicht bereit", ""],
      account: ["Konto & Guthaben", state.session ? `· ${state.session.user.display_name}` : ""],
      settings: ["Setup & Einstellungen", `· ${state.terminalId}`],
      admin: ["Admin", "· Server & Token"],
      not_approved: ["Keine 3D-Druck-Freigabe", ""],
      bv_wait: ["Einweisungs-Karte vorhalten", ""],
      approved_ok: ["Nutzer freigeschaltet", ""],
    };
    const [t, s] = titles[state.screen] || ["SmartTerminal", ""];
    titleEl.textContent = t;
    subEl.textContent = s;
  }

  function renderDock() {
    const dock = $("printer-dock");
    dock.style.gridTemplateColumns = `repeat(${Math.max(1, state.printers.length)}, minmax(0, 1fr))`;
    dock.innerHTML = state.printers
      .map(
        (p) => `
        <div class="p-tile st-${escapeHtml(p.state)}" data-open-printer="${p.slot}">
          <div class="p-body">
            <div class="p-name">Drucker ${p.slot} <span class="p-model">${escapeHtml(p.model || p.name)}</span></div>
            <div class="p-stat" data-printer-sub="${escapeHtml(p.id)}">${escapeHtml(printerStatText(p))}</div>
          </div>
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
                      <div class="job-swipe-bg">✕ Übertragung abbrechen</div>
                      <div class="job-card is-uploading" data-job-id="${escapeHtml(j.id)}" data-open-upload="${escapeHtml(j.id)}">
                        <div class="job-thumb">${thumb}</div>
                        <div class="job-info">
                          <div class="job-row1">
                            <div class="job-name-wrap"><span class="job-name">${escapeHtml(j.displayName || j.filename)}</span></div>
                            <span class="slot-badge">Drucker ${j.printerSlot}</span>
                          </div>
                          <div class="job-meta">
                            <span>Dauer <strong>${fmtDur(j.estimatedTimeSeconds)}</strong></span>
                            <span>Antippen für Details…</span>
                          </div>
                        </div>
                        <div class="job-cta">
                          <div class="inline-up">
                            <div class="inline-up-track">
                              <div class="inline-up-fill" data-upload-fill="${escapeHtml(j.id)}" style="width:${Math.round(up.percent || 0)}%"></div>
                            </div>
                            <div class="inline-up-txt">
                              <span data-upload-pct="${escapeHtml(j.id)}">${Math.round(up.percent || 0)} %</span>
                              <span data-upload-mb="${escapeHtml(j.id)}">${fmtMB(up.sentBytes)}/${fmtMB(up.totalBytes || j.fileSize)}</span>
                            </div>
                          </div>
                          <button type="button" class="btn-abort-x" data-del-id="${escapeHtml(j.id)}" title="Übertragung abbrechen">✕</button>
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
                      <div class="job-thumb">${thumb}</div>
                      <div class="job-info">
                        <div class="job-row1">
                          <div class="job-name-wrap"><span class="job-name">${escapeHtml(j.displayName || j.filename)}</span></div>
                          <span class="slot-badge">Drucker ${j.printerSlot}</span>
                        </div>
                        <div class="job-meta">
                          <span>Dauer <strong>${fmtDur(j.estimatedTimeSeconds)}</strong></span>
                          <span class="ttl-chip ${warn ? "warn" : ""}" data-ttl-wrap="${escapeHtml(j.id)}">
                            <svg class="ttl-ring" viewBox="0 0 24 24">
                              <circle cx="12" cy="12" r="10" fill="none" stroke="var(--border)" stroke-width="3.5"></circle>
                              <circle data-ttl-fg="${escapeHtml(j.id)}" cx="12" cy="12" r="10" fill="none" stroke="${warn ? "var(--orange)" : "var(--blue)"}" stroke-width="3.5" stroke-linecap="round" stroke-dasharray="62.8" stroke-dashoffset="${dash}"></circle>
                            </svg>
                            <span data-ttl-txt="${escapeHtml(j.id)}">Timeout: ${fmtCountdown(rem)}</span>
                          </span>
                        </div>
                      </div>
                      <div class="job-cta">
                        <button type="button" class="btn-print-job" data-print-id="${escapeHtml(j.id)}">
                          <span class="lbl">${ctaLabel}</span>
                          <span class="prc">${fmtEur(j.priceCents)}</span>
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
      stage.innerHTML = `
        <div class="subscreen">
          <div class="rfid-center">
            <div class="rfid-circle">${nfcIconSvg()}</div>
            <div class="rfid-text">
              <h2>Bitte Mitgliedskarte scannen</h2>
              ${
                job
                  ? `<p>Job: <strong>${escapeHtml(job.displayName || job.filename)}</strong> (Drucker ${job.printerSlot})<br>
                     Kosten: <strong>${fmtEur(job.priceCents)}</strong> · Fertig <strong>${fmtEta(job.estimatedTimeSeconds)}</strong></p>`
                  : `<p>Halte deine Karte an den Leser, um dein Konto &amp; Guthaben anzuzeigen.</p>`
              }
              <div style="margin-top:2cqh;">${timeoutPillHtml("Timeout:")}</div>
            </div>
          </div>
        </div>
      `;
      return;
    }

    if (state.screen === "payment_confirm" && job && sess) {
      const after = sess.balanceCents - job.priceCents;
      const enough = after >= 0;
      const maxNeg = Number(state.features.maxNegativeBalanceCents || 0);
      const canOverdraft = !enough && Boolean(state.features.allowNegativeBalance) && (maxNeg === 0 || after >= -maxNeg);
      const thumb = job.thumbnailUrl ? `<img src="${escapeHtml(job.thumbnailUrl)}" alt="">` : "🧊";
      const verb = job.printAfterUpload === false ? "HOCHLADEN" : "DRUCKEN";
      const modeHint = job.printAfterUpload === false ? " · Nur Upload (Start am Drucker)" : "";
      stage.innerHTML = `
        <div class="subscreen">
          <div class="panel pay-top-with-thumb">
            <div class="pay-thumb">${thumb}</div>
            <div style="display:flex;flex-direction:column;gap:1.4cqh;min-width:0;">
              <div class="pay-head">
                <div style="min-width:0;">
                  <div class="pay-job-title">${escapeHtml(job.displayName || job.filename)}</div>
                  <div class="pay-job-sub">Drucker ${job.printerSlot} · ${Number(job.filamentGrams || 0).toFixed(1)} g ${escapeHtml(job.filamentType)} · Dauer ${fmtDur(job.estimatedTimeSeconds)}${modeHint}</div>
                </div>
                <div class="pay-eta-pill">⏱ Fertig ${fmtEta(job.estimatedTimeSeconds)}</div>
              </div>
              <div class="pay-numbers">
                <div>
                  <div class="num-lbl">Guthaben</div>
                  <div class="num-val ${sess.balanceCents < 0 ? "neg" : ""}">${fmtEur(sess.balanceCents)}</div>
                </div>
                <div>
                  <div class="num-lbl">Druckkosten</div>
                  <div class="num-val">${fmtEur(job.priceCents)}</div>
                </div>
                <div>
                  <div class="num-lbl">Danach auf Konto</div>
                  <div class="num-val ${enough ? "pos" : "neg"}">${fmtEur(after)}</div>
                </div>
              </div>
            </div>
          </div>
          <div class="pay-actions">
            <div class="pay-secondary-stack">
              <button type="button" class="btn btn-secondary" data-action="open-club">🏛️ Vereinsdruck</button>
              <button type="button" class="btn btn-secondary" data-action="open-already-paid">✓ Bereits bezahlt</button>
            </div>
            ${
              enough
                ? `<button type="button" class="btn btn-primary pay-cta-big" data-action="start-personal">${verb} · ${fmtEur(job.priceCents)}</button>`
                : `
                  <div class="pay-primary-split" style="${!state.features.topup || !canOverdraft ? "grid-template-columns:1fr;" : ""}">
                    ${state.features.topup ? `<button type="button" class="btn btn-blue" style="font-size:3.6cqh;" data-action="open-topup-print">+ Aufladen</button>` : ""}
                    ${canOverdraft ? `<button type="button" class="btn btn-orange" style="font-size:3.9cqh;" data-action="open-overdraft">Trotzdem ${verb.toLowerCase()} →</button>` : ""}
                  </div>
                `
            }
          </div>
        </div>
      `;
      return;
    }

    if (state.screen === "overdraft_confirm" && job && sess) {
      const after = sess.balanceCents - job.priceCents;
      stage.innerHTML = `
        <div class="subscreen">
          <div class="panel" style="border-color:var(--orange-border);">
            <div style="font-size:4.1cqh;font-weight:800;color:var(--orange-ink);">
              ⚠ Guthaben reicht nicht aus — dein Konto gerät ins Minus.
            </div>
            <div class="pay-numbers">
              <div><div class="num-lbl">Aktuell</div><div class="num-val">${fmtEur(sess.balanceCents)}</div></div>
              <div><div class="num-lbl">Druckkosten</div><div class="num-val">${fmtEur(job.priceCents)}</div></div>
              <div><div class="num-lbl">Neuer Kontostand</div><div class="num-val neg">${fmtEur(after)}</div></div>
            </div>
            <div style="font-size:3.1cqh;color:var(--muted);font-weight:600;">
              Geschätztes Druckende: <strong>${fmtEta(job.estimatedTimeSeconds)}</strong> · Bitte lade dein Guthaben zeitnah wieder auf.
            </div>
          </div>
          <div class="pay-actions">
            <div class="pay-secondary-stack">
              <button type="button" class="btn btn-secondary" data-action="open-club">🏛️ Vereinsdruck</button>
              <button type="button" class="btn btn-secondary" data-action="open-already-paid">✓ Bereits bezahlt</button>
            </div>
            <button type="button" class="btn btn-orange" style="font-size:4.1cqh;" data-action="start-personal-overdraft">Dennoch drucken – ich lade später auf</button>
          </div>
        </div>
      `;
      return;
    }

    if (state.screen === "topup" && sess) {
      stage.innerHTML = `
        <div class="subscreen">
          <div class="panel" style="flex-direction:row;align-items:center;justify-content:space-between;">
            <div style="font-size:3.8cqh;font-weight:800;">${escapeHtml(sess.user.display_name)}</div>
            <div style="font-size:4.4cqh;font-weight:900;color:var(--green);">Aktuell: ${fmtEur(sess.balanceCents)}</div>
          </div>
          <div class="choice-grid">
            <button type="button" class="btn btn-blue" data-topup="500">+ 5,00 €</button>
            <button type="button" class="btn btn-blue" data-topup="1000">+ 10,00 €</button>
            <button type="button" class="btn btn-blue" data-topup="2000">+ 20,00 €</button>
            <button type="button" class="btn btn-blue" data-topup="5000">+ 50,00 €</button>
          </div>
        </div>
      `;
      return;
    }

    if (state.screen === "club_section") {
      stage.innerHTML = `
        <div class="subscreen">
          <div class="choice-grid">
            ${state.sections
              .map(
                (s) => `
                <button type="button" class="btn btn-secondary" data-section-id="${escapeHtml(s.sectionId)}">
                  🏛️ ${escapeHtml(s.name)}
                </button>
              `
              )
              .join("")}
          </div>
        </div>
      `;
      return;
    }

    if (state.screen === "pin_entry" && job) {
      const isClub = state.pinMode === "club";
      const sec = state.sections.find((s) => s.sectionId === state.selectedSectionId);
      const dots = Array.from({ length: 4 }, (_, i) => `<div class="pin-dot ${i < state.pinValue.length ? "filled" : ""}"></div>`).join("");
      stage.innerHTML = `
        <div class="subscreen">
          <div class="pin-grid">
            <div class="pin-left">
              <div>
                <h2>Persönliche PIN</h2>
                <p>${
                  isClub
                    ? `Vereinsdruck für <strong>${escapeHtml(sec?.name || "")}</strong> · <strong>${escapeHtml(job.displayName)}</strong>.`
                    : `Ich bestätige den Nachdruck (<strong>Bereits bezahlt</strong>) für <strong>${escapeHtml(job.displayName)}</strong>.`
                }<br>Fertig <strong>${fmtEta(job.estimatedTimeSeconds)}</strong>.</p>
                <div class="pin-dots">${dots}</div>
                ${state.pinError ? `<div class="pin-err">${escapeHtml(state.pinError)}</div>` : ""}
              </div>
              <div style="align-self:flex-start;">${timeoutPillHtml("Timeout:")}</div>
            </div>
            <div class="numpad">
              ${["1", "2", "3", "4", "5", "6", "7", "8", "9", "⌫", "0", "✓"]
                .map((k) => {
                  const cls = k === "✓" ? "btn-primary" : k === "⌫" ? "btn-back" : "btn-secondary";
                  return `<button type="button" class="btn ${cls}" data-pin-key="${k}">${k}</button>`;
                })
                .join("")}
            </div>
          </div>
        </div>
      `;
      return;
    }

    if (state.screen === "printer_busy") {
      const isOffline = state.lastResult?.reason === "PRINTER_OFFLINE";
      stage.innerHTML = `
        <div class="subscreen">
          <div class="panel" style="flex:1;align-items:center;justify-content:center;text-align:center;">
            <div style="font-size:10cqh;">🔴</div>
            <div style="font-size:4.8cqh;font-weight:900;">${isOffline ? "Drucker ist offline" : "Drucker ist belegt"}</div>
            <div style="font-size:3.5cqh;color:var(--green);font-weight:800;">✓ Es wurde kein Guthaben abgebucht.</div>
          </div>
        </div>
      `;
      return;
    }

    if (state.screen === "account" && sess) {
      const txs = (sess.transactions || [])
        .map((t) => {
          const cls = t.amountCents > 0 ? "plus" : t.amountCents === 0 ? "zero" : "";
          const amt = t.amountCents > 0 ? `+${fmtEur(t.amountCents)}` : fmtEur(t.amountCents);
          return `
            <div class="tx-item">
              <div class="tx-left">
                <div class="tx-title">${escapeHtml(t.title)}</div>
                <div class="tx-meta">${fmtDateTime(t.ts)}</div>
              </div>
              <div class="tx-amount ${cls}">${amt}</div>
            </div>
          `;
        })
        .join("");

      stage.innerHTML = `
        <div class="subscreen">
          <div class="panel" style="flex:1;min-height:0;gap:1.4cqh;">
            <div style="display:flex;align-items:center;justify-content:space-between;gap:2cqw;border-bottom:1px solid var(--border);padding-bottom:1.2cqh;">
              <div>
                <div class="num-lbl">Mitglied</div>
                <div style="font-size:4.6cqh;font-weight:900;">${escapeHtml(sess.user.display_name)}</div>
              </div>
              <div style="text-align:right;">
                <div class="num-lbl">Aktuelles Guthaben</div>
                <div class="num-val ${sess.balanceCents < 0 ? "neg" : "pos"}" style="font-size:5.2cqh;">${fmtEur(sess.balanceCents)}</div>
              </div>
              ${state.features.topup ? `<button type="button" class="btn btn-blue" style="height:10.5cqh;padding:0 2.2cqw;font-size:3.4cqh;" data-action="open-topup-account">+ Aufladen</button>` : ""}
            </div>
            <div class="num-lbl">Letzte Drucke &amp; Buchungen</div>
            <div class="tx-scroll">
              ${txs || `<div class="tx-item"><span class="tx-meta">Keine Buchungen vorhanden</span></div>`}
            </div>
          </div>
        </div>
      `;
      return;
    }

    if (state.screen === "settings") {
      stage.innerHTML = `
        <div class="subscreen">
          <div class="panel" style="flex:1;justify-content:space-between;">
            <div style="font-size:3.6cqh;font-weight:800;">Anzeige-Modus</div>
            <div class="settings-grid">
              <button type="button" class="theme-card ${state.theme === "light" ? "active" : ""}" data-set-theme="light">
                <span style="font-size:6cqh;">☀️</span>
                <span>Hell (Standard)</span>
              </button>
              <button type="button" class="theme-card ${state.theme === "dark" ? "active" : ""}" data-set-theme="dark">
                <span style="font-size:6cqh;">🌙</span>
                <span>Dunkel</span>
              </button>
            </div>
            <div style="display:flex;align-items:center;justify-content:space-between;gap:1.5cqw;border-top:1px solid var(--border);padding-top:1.5cqh;">
              <div style="font-size:3cqh;color:var(--muted);">
                Terminal: <strong>${escapeHtml(state.terminalId)}</strong> · IP: <strong>${escapeHtml(state.terminalIp)}</strong> · v${escapeHtml(state.version)}
              </div>
              <button type="button" class="btn btn-secondary" style="height:11cqh;padding:0 2.6cqw;font-size:3.5cqh;" data-action="open-admin">🔐 Admin</button>
            </div>
          </div>
        </div>
      `;
      return;
    }

    if (state.screen === "admin") {
      const cfg = state.adminConfig || {
        terminalId: state.terminalId,
        easyverwaltungUrl: "",
        easyverwaltungServiceId: "",
        hasToken: false,
      };
      stage.innerHTML = `
        <div class="subscreen">
          <div class="panel" style="flex:1;justify-content:space-between;">
            <div class="admin-grid">
              <div class="field-box">
                <label>Terminal-ID</label>
                <input class="field-input" id="adm-terminal-id" type="text" value="${escapeHtml(cfg.terminalId)}">
              </div>
              <div class="field-box">
                <label>easyVerwaltung Service-ID</label>
                <input class="field-input" id="adm-service-id" type="text" value="${escapeHtml(cfg.easyverwaltungServiceId)}">
              </div>
              <div class="field-box">
                <label>Server-URL</label>
                <input class="field-input" id="adm-server-url" type="text" value="${escapeHtml(cfg.easyverwaltungUrl)}" placeholder="https://verwaltung.freilab.de">
              </div>
              <div class="field-box">
                <label>Service-Token (${cfg.hasToken ? "gesetzt" : "nicht gesetzt"})</label>
                <input class="field-input" id="adm-token" type="password" value="" placeholder="${cfg.hasToken ? "•••••••• (leer = behalten)" : "Token eingeben"}">
              </div>
            </div>
            <div style="font-size:2.8cqh;color:var(--muted);font-weight:600;">
              💡 Tipp für lange Tokens: Auch im Browser unter <strong>http://${escapeHtml(state.terminalIp)}</strong> → ⚙ → Admin konfigurierbar.
            </div>
          </div>
          <div style="display:grid;grid-template-columns:1fr 1.3fr;gap:1.6cqw;height:14cqh;">
            <button type="button" class="btn btn-blue" style="font-size:3.5cqh;" data-action="admin-test">Verbindung testen</button>
            <button type="button" class="btn btn-primary" style="font-size:3.9cqh;" data-action="admin-save">Speichern</button>
          </div>
        </div>
      `;
      return;
    }

    if (state.screen === "not_approved" && sess) {
      stage.innerHTML = `
        <div class="subscreen">
          <div class="panel" style="flex:1;align-items:center;justify-content:center;text-align:center;">
            <div style="font-size:9cqh;">🚫</div>
            <div style="font-size:4.4cqh;font-weight:900;">Keine 3D-Druck-Freigabe für ${escapeHtml(sess.user.display_name)}</div>
            <div style="font-size:3.2cqh;color:var(--muted);">Bitte lass dich von einer berechtigten Person (BV / Einweiser) freischalten.</div>
          </div>
          ${state.features.approval ? `<button type="button" class="btn btn-primary" style="height:15cqh;font-size:4cqh;" data-action="open-bv-wait">Freigabe vor Ort beantragen</button>` : ""}
        </div>
      `;
      return;
    }

    if (state.screen === "bv_wait") {
      stage.innerHTML = `
        <div class="subscreen">
          <div class="rfid-center">
            <div class="rfid-circle">${nfcIconSvg()}</div>
            <div class="rfid-text">
              <h2>Einweisungs-Karte vorhalten</h2>
              <p>Bitte berechtigte BV- / Einweiser-Karte an den Leser halten.</p>
              <div style="margin-top:2cqh;">${timeoutPillHtml("Timeout:")}</div>
            </div>
          </div>
        </div>
      `;
      return;
    }

    if (state.screen === "approved_ok") {
      stage.innerHTML = `
        <div class="subscreen">
          <div class="panel" style="flex:1;align-items:center;justify-content:center;text-align:center;">
            <div style="font-size:10cqh;">✅</div>
            <div style="font-size:4.8cqh;font-weight:900;">Nutzer freigeschaltet</div>
            <div style="font-size:3.3cqh;color:var(--muted);">Durch: ${escapeHtml(state.lastResult?.instructorName || "Einweiser")}</div>
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
            <button type="button" class="btn-minimize" data-close-upload="1" title="Minimieren">−</button>
            <h3>Übertragung an Drucker ${job.printerSlot}</h3>
          </div>
        </div>
        <div class="popup-body">
          <div class="upload-preview">${thumb}</div>
          <div class="upload-details">
            <div>
              <div class="upload-job-name">${escapeHtml(job.displayName || job.filename)}</div>
              <div class="upload-meta-line">
                <span class="ok-check">✓ Freigegeben</span>
                <span>· Dauer <strong>${fmtDur(job.estimatedTimeSeconds)}</strong></span>
                <span>· Fertig <strong>${fmtEta(job.estimatedTimeSeconds)}</strong></span>
              </div>
            </div>
            <div class="big-prog-track"><div class="big-prog-fill" style="width:${Math.round(up.percent || 0)}%"></div></div>
            <div class="big-prog-meta">
              <span>${Math.round(up.percent || 0)} % übertragen</span>
              <span>${fmtMB(up.sentBytes)} / ${fmtMB(up.totalBytes || job.fileSize)}${speed ? ` · ${speed}` : ""}</span>
            </div>
          </div>
        </div>
        <div class="popup-foot">
          <button type="button" class="btn btn-danger-soft" data-abort-upload="${escapeHtml(job.id)}">✕ Übertragung abbrechen</button>
          <button type="button" class="btn btn-orange" data-close-upload="1">− Im Hintergrund weiterladen</button>
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
    const hasActive = (p.state === "PRINTING" || p.state === "UPLOADING") && (p.activeJob || p.uploadProgress);
    const activeName = p.activeJob || state.jobs.find((j) => j.id === p.uploadProgress?.jobId)?.displayName || "Auftrag";
    const remTxt = p.timeRemainingSeconds ? `Restzeit: ${fmtDur(p.timeRemainingSeconds)} · Fertig ${fmtEta(p.timeRemainingSeconds)}` : "";

    el.classList.remove("hidden");
    el.innerHTML = `
      <div class="popup-card" style="width:85cqw;">
        <div class="popup-head">
          <div class="popup-head-left">
            <button type="button" class="back-top-btn" style="height:8.2cqh;" data-close-printer="1">‹ Zurück</button>
            <h3>Drucker ${p.slot} · ${escapeHtml(p.model || p.name)}</h3>
          </div>
          <span class="p-status-pill st-${escapeHtml(p.state)}">${escapeHtml(printerStatText(p))}</span>
        </div>
        <div class="printer-popup-body">
          <div class="printer-photo">
            <div style="width:17cqw;height:15cqw;">${printerSvg(p.model || p.name)}</div>
            <div style="font-size:2.6cqh;font-weight:800;color:var(--muted);">${escapeHtml(p.ip || "–")}</div>
          </div>
          <div class="printer-spec-grid">
            <div class="spec-chip"><div class="spec-k">Düse &amp; Temp.</div><div class="spec-v">${nozzleStr} · ${nozzleTemp}</div></div>
            <div class="spec-chip"><div class="spec-k">Heizbett</div><div class="spec-v">${bedTemp}</div></div>
            <div class="spec-chip"><div class="spec-k">Druckraum (mm)</div><div class="spec-v">${escapeHtml(p.buildVolume || "–")}</div></div>
            <div class="spec-chip"><div class="spec-k">On-Time</div><div class="spec-v">${fmtOnTime(p.onTimeSeconds)}</div></div>
            <div class="spec-chip"><div class="spec-k">Gesamt-Druckzeit</div><div class="spec-v">${totalH}</div></div>
            <div class="spec-chip"><div class="spec-k">Gesamt-Filament</div><div class="spec-v">${totalKm}</div></div>
            ${
              hasActive
                ? `
                  <div class="p-job-banner">
                    <div style="display:flex;justify-content:space-between;font-size:3cqh;font-weight:800;">
                      <span>${p.state === "UPLOADING" ? "Überträgt:" : "Aktiver Druck:"} ${escapeHtml(activeName)}</span>
                      <span>${Math.round(p.progress || 0)} %</span>
                    </div>
                    ${remTxt ? `<div style="font-size:2.7cqh;font-weight:700;color:var(--orange-ink);">${remTxt}</div>` : ""}
                  </div>
                `
                : `
                  <div class="spec-chip" style="grid-column:span 3;display:flex;justify-content:space-between;align-items:center;">
                    <span class="spec-k">Aktiver Druck</span>
                    <span style="font-size:3cqh;font-weight:800;color:var(--muted);">${p.state === "OFFLINE" ? "Drucker nicht erreichbar" : "Kein laufender Auftrag"}</span>
                  </div>
                `
            }
          </div>
        </div>
      </div>
    `;
  }

  function renderPopups() {
    renderUploadPopup();
    renderPrinterPopup();
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

  // --- Events ---
  $("btn-back-top").addEventListener("click", goBackOneStep);
  $("btn-theme-eye").addEventListener("click", () => setTheme(state.theme === "light" ? "dark" : "light"));

  $("btn-account").addEventListener("click", () => {
    if (state.screen === "account") return returnToQueue(true);
    state.selectedJobId = null;
    state.rfidPurpose = "account";
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

  $("printer-popup").addEventListener("click", (ev) => {
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
    const secBtn = ev.target.closest("[data-section-id]");
    if (secBtn) {
      state.selectedSectionId = secBtn.dataset.sectionId;
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
      } else if (state.pinValue.length < 8) {
        state.pinValue += k;
        renderStage();
      }
      return;
    }
    const actBtn = ev.target.closest("[data-action]");
    if (actBtn) {
      const a = actBtn.dataset.action;
      if (a === "start-personal") return executeStart("personal");
      if (a === "open-overdraft") return openScreen("overdraft_confirm");
      if (a === "start-personal-overdraft") return executeStart("personal", { allowOverdraft: true });
      if (a === "open-club") return openScreen("club_section");
      if (a === "open-already-paid") {
        state.pinMode = "already_paid";
        state.pinValue = "";
        state.pinError = "";
        return openScreen("pin_entry");
      }
      if (a === "open-topup-print") {
        state.topupReturnTo = "payment_confirm";
        return openScreen("topup");
      }
      if (a === "open-topup-account") {
        state.topupReturnTo = "account";
        return openScreen("topup");
      }
      if (a === "open-bv-wait") return openScreen("bv_wait");
      if (a === "open-admin") {
        try {
          state.adminConfig = await api("/api/admin/config");
        } catch {}
        return openScreen("admin");
      }
      if (a === "admin-test") {
        try {
          const r = await api("/api/admin/test-connection", { method: "POST" });
          showToast(r.ok ? `✓ ${r.message}` : `✕ ${r.message}`);
        } catch (err) {
          showToast(err.message);
        }
        return;
      }
      if (a === "admin-save") {
        try {
          const payload = {
            terminalId: $("adm-terminal-id").value.trim() || state.terminalId,
            easyverwaltungServiceId: $("adm-service-id").value.trim(),
            easyverwaltungUrl: $("adm-server-url").value.trim(),
            easyverwaltungServiceToken: $("adm-token").value.trim() || null,
          };
          const r = await api("/api/admin/config", { method: "POST", body: JSON.stringify(payload) });
          state.adminConfig = r.config;
          state.terminalId = r.config.terminalId;
          showToast("✓ Server-ID & Token gespeichert");
          openScreen("settings");
        } catch (err) {
          showToast(err.message);
        }
      }
    }
  });

  // Touch swipe-left on job cards
  let swipeCard = null;
  let swipeStartX = 0;
  let swipeStartY = 0;
  let swipeDx = 0;
  let swipeLocked = false;

  $("stage").addEventListener("touchstart", (ev) => {
    if (ev.touches.length !== 1) return;
    const card = ev.target.closest(".job-card");
    if (!card) return;
    swipeCard = card;
    swipeStartX = ev.touches[0].clientX;
    swipeStartY = ev.touches[0].clientY;
    swipeDx = 0;
    swipeLocked = false;
  }, { passive: true });

  $("stage").addEventListener("touchmove", (ev) => {
    if (!swipeCard || ev.touches.length !== 1) return;
    const dx = ev.touches[0].clientX - swipeStartX;
    const dy = ev.touches[0].clientY - swipeStartY;
    if (!swipeLocked) {
      if (Math.abs(dy) > Math.abs(dx) && Math.abs(dy) > 10) {
        swipeCard = null;
        return;
      }
      if (Math.abs(dx) > 12) {
        swipeLocked = true;
        swipeCard.classList.add("swiping");
      }
    }
    if (swipeLocked && dx < 0) {
      swipeDx = Math.max(-180, dx);
      swipeCard.style.transform = `translateX(${swipeDx}px)`;
    }
  }, { passive: true });

  $("stage").addEventListener("touchend", async () => {
    if (!swipeCard) return;
    const card = swipeCard;
    const dx = swipeDx;
    swipeCard = null;
    card.classList.remove("swiping");
    if (dx <= -95) {
      card.style.transform = "translateX(-100%)";
      await cancelOrAbortJob(card.dataset.jobId);
    } else {
      card.style.transform = "";
    }
  });

  // Collapsible DEV Drawer
  $("dev-pill").addEventListener("click", () => {
    $("dev-drawer").classList.remove("hidden");
    $("dev-pill").classList.add("hidden");
  });
  $("dev-close").addEventListener("click", () => {
    $("dev-drawer").classList.add("hidden");
    renderDevControls();
  });
  $("dev-upload").addEventListener("click", async () => {
    try {
      await api("/api/dev/upload-sample", { method: "POST", body: JSON.stringify({ slot: 1 }) });
      showToast("Test-Job auf Drucker 1 hochgeladen");
    } catch (err) {
      showToast(err.message);
    }
  });
  $("dev-toggle-p1").addEventListener("click", async () => {
    const p1 = state.printers.find((p) => p.slot === 1);
    if (!p1) return;
    const next = p1.state === "READY" ? "PRINTING" : "READY";
    try {
      await api("/api/dev/printer-state", { method: "POST", body: JSON.stringify({ printerId: p1.id, state: next }) });
    } catch (err) {
      showToast(err.message);
    }
  });
  $("dev-drawer").addEventListener("click", async (ev) => {
    const btn = ev.target.closest("[data-rfid]");
    if (!btn) return;
    try {
      await api("/api/dev/simulate-rfid", { method: "POST", body: JSON.stringify({ uid: btn.dataset.rfid }) });
    } catch (err) {
      showToast(err.message);
    }
  });

  // Periodic tick: Clock, Job TTL rings, active timeout ring
  setInterval(() => {
    $("sb-clock").textContent = new Date().toLocaleTimeString("de-DE", { hour: "2-digit", minute: "2-digit" });
    updateTimeoutDOM();
    if (state.screen === "queue") {
      for (const j of state.jobs) {
        if (j.status === "sending") continue;
        const rem = Math.max(0, Math.floor((j._clientExpiresAt - Date.now()) / 1000));
        const pct = Math.max(0, Math.min(100, (rem / (j._ttlTotal || 300)) * 100));
        const txt = document.querySelector(`[data-ttl-txt="${j.id}"]`);
        const fg = document.querySelector(`[data-ttl-fg="${j.id}"]`);
        const wrap = document.querySelector(`[data-ttl-wrap="${j.id}"]`);
        if (txt) txt.textContent = `Timeout: ${fmtCountdown(rem)}`;
        if (fg) {
          fg.style.strokeDashoffset = ((100 - pct) * 0.628).toFixed(1);
          fg.setAttribute("stroke", rem <= 60 ? "var(--orange)" : "var(--blue)");
        }
        if (wrap) wrap.classList.toggle("warn", rem <= 60);
      }
    }
  }, 500);

  window.addEventListener("resize", () => {
    if (state.screen === "queue") initMarquees();
  });

  setInterval(loadState, 15000);
  loadState();
  connectSSE();
})();
