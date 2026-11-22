(() => {
  if (new URLSearchParams(window.location.search).get("kiosk") === "1") {
    document.documentElement.classList.add("kiosk");
  }

  const RING_CIRCUMFERENCE = 2 * Math.PI * 19;

  const state = {
    terminalId: "",
    serviceName: "SmartTerminal 3D-Print",
    printers: [],
    jobs: [],
    sections: [],
    modes: { devMock: false },
    features: { topup: false, approval: false },
    connected: false,
    // Interactive flow state
    screen: "queue", // queue | rfid_wait | payment_confirm | insufficient | topup | club_section | pin_entry | sending | printer_busy | success | account | not_approved | bv_wait | approved_ok
    rfidPurpose: null, // "print" | "account"
    selectedJobId: null,
    selectedSectionId: null,
    pinMode: null, // "club" | "already_paid"
    pinValue: "",
    pinError: "",
    sendingMode: "personal",
    sendProgress: { stage: "uploading", percent: 0, sentBytes: 0, totalBytes: 0 },
    session: null,
    lastResult: null,
    topupReturnTo: "payment_confirm",
    timeoutEnd: null,
    timeoutTotal: 60,
    timeoutAction: null,
  };

  const $ = (id) => document.getElementById(id);

  function fmtEur(cents) {
    const val = (Number(cents || 0) / 100).toFixed(2).replace(".", ",");
    return `${val} €`;
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

  function updateSendingProgressDOM() {
    const fill = $("send-progress-fill");
    const pctEl = $("send-progress-pct");
    const bytesEl = $("send-progress-bytes");
    const stageEl = $("send-stage-text");
    const hintEl = $("send-mode-hint");
    if (!fill) return;
    const p = state.sendProgress || { stage: "uploading", percent: 0, sentBytes: 0, totalBytes: 0, bytesPerSec: 0 };
    const pct = Math.max(0, Math.min(100, Math.round(p.percent || 0)));
    fill.style.width = `${pct}%`;
    pctEl.textContent = `${pct} %`;
    const speed = fmtSpeed(p.bytesPerSec);
    bytesEl.textContent =
      p.totalBytes > 0
        ? `${fmtMB(p.sentBytes)} / ${fmtMB(p.totalBytes)}${speed ? ` · ${speed}` : ""}`
        : "";

    if (p.stage === "starting") {
      stageEl.textContent = "Druck wird am Drucker gestartet…";
    } else if (p.stage === "charging") {
      stageEl.textContent = "Zahlung wird gebucht…";
    } else {
      stageEl.textContent = "Datei wird an den Drucker übertragen…";
    }
    if (hintEl) {
      hintEl.textContent =
        "Fenster minimiert sich automatisch – die Übertragung läuft in der Warteschlange weiter.";
    }
  }

  function fmtDuration(sec) {
    sec = Math.max(0, Number(sec || 0));
    if (sec === 0) return "–";
    const h = Math.floor(sec / 3600);
    const m = Math.floor((sec % 3600) / 60);
    if (h > 0) return `${h}h ${m}m`;
    return `${Math.max(1, m)} min`;
  }

  function fmtCountdown(sec) {
    sec = Math.max(0, Math.floor(sec));
    const m = Math.floor(sec / 60);
    const s = sec % 60;
    return `${String(m).padStart(2, "0")}:${String(s).padStart(2, "0")}`;
  }

  function fmtTime(ts) {
    const d = new Date(ts * 1000);
    return d.toLocaleTimeString("de-DE", { hour: "2-digit", minute: "2-digit" });
  }

  function stateLabel(st) {
    const map = {
      READY: "Bereit",
      UPLOADING: "Überträgt",
      PRINTING: "Druckt",
      PAUSED: "Pausiert",
      ATTENTION: "Achtung",
      OFFLINE: "Offline",
    };
    return map[st] || st;
  }

  function escapeHtml(s) {
    return String(s ?? "")
      .replaceAll("&", "&amp;")
      .replaceAll("<", "&lt;")
      .replaceAll(">", "&gt;")
      .replaceAll('"', "&quot;");
  }

  let toastTimer = null;
  function showToast(msg) {
    const el = $("toast");
    el.textContent = msg;
    el.classList.remove("hidden");
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => el.classList.add("hidden"), 3500);
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
      $("timeout-ring").classList.add("hidden");
      return;
    }
    state.timeoutTotal = seconds;
    state.timeoutEnd = Date.now() + seconds * 1000;
    state.timeoutAction = onExpire;
    $("timeout-ring").classList.remove("hidden");
    updateTimeoutRing();
  }

  function updateTimeoutRing() {
    if (!state.timeoutEnd) return;
    const remMs = state.timeoutEnd - Date.now();
    if (remMs <= 0) {
      const action = state.timeoutAction;
      setTimeoutRing(null, null);
      if (action) action();
      return;
    }
    const remSec = Math.ceil(remMs / 1000);
    const ratio = Math.max(0, Math.min(1, remMs / (state.timeoutTotal * 1000)));
    $("ring-fg").style.strokeDashoffset = String(RING_CIRCUMFERENCE * (1 - ratio));
    $("ring-text").textContent = String(remSec);
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
    renderModal();
  }

  // --- Data Sync & SSE ---
  async function loadState() {
    try {
      const data = await api("/api/state");
      state.terminalId = data.terminalId;
      state.serviceName = data.serviceName || "3D-Print Terminal";
      state.printers = data.printers || [];
      state.jobs = (data.jobs || []).map((j) => ({
        ...j,
        _clientExpiresAt: Date.now() + (j.remainingSeconds || 0) * 1000,
      }));
      state.sections = data.sections || [];
      state.modes = data.modes || { devMock: false };
      state.features = data.features || {};
      state.connected = true;

      // If currently in a print flow and the selected job expired, inform user
      if (
        state.selectedJobId &&
        ["rfid_wait", "payment_confirm", "insufficient", "overdraft_confirm", "club_section", "pin_entry"].includes(state.screen) &&
        !selectedJob()
      ) {
        showToast("Der ausgewählte Druckjob ist abgelaufen.");
        await returnToQueue(true);
      }

      renderHeader();
      renderQueue();
      renderDevBar();
    } catch {
      state.connected = false;
      renderHeader();
    }
  }

  function connectSSE() {
    const es = new EventSource("/api/events");
    es.addEventListener("connected", () => {
      state.connected = true;
      renderHeader();
    });
    es.addEventListener("queue_updated", () => loadState());
    es.addEventListener("printers_updated", () => loadState());
    es.addEventListener("job_progress", (ev) => {
      try {
        const payload = JSON.parse(ev.data)?.data;
        if (!payload) return;
        // Update active modal progress if watching this job
        if (payload.jobId === state.selectedJobId) {
          state.sendProgress = {
            stage: payload.stage || "uploading",
            percent: Number(payload.percent || 0),
            sentBytes: Number(payload.sentBytes || 0),
            totalBytes: Number(payload.totalBytes || 0),
            bytesPerSec: Number(payload.bytesPerSec || 0),
          };
          if (state.screen === "sending") updateSendingProgressDOM();
        }
        // Update inline progress on main queue card & printer tile
        const job = state.jobs.find((j) => j.id === payload.jobId);
        if (job) {
          job.uploadProgress = payload;
          const fill = document.querySelector(`[data-upload-fill="${payload.jobId}"]`);
          const txt = document.querySelector(`[data-upload-text="${payload.jobId}"]`);
          if (fill) fill.style.width = `${Math.round(payload.percent || 0)}%`;
          if (txt) {
            const sp = fmtSpeed(payload.bytesPerSec);
            txt.textContent =
              payload.stage === "starting"
                ? "Starte Druck…"
                : `Überträgt ${Math.round(payload.percent || 0)} % · ${fmtMB(payload.sentBytes)} / ${fmtMB(payload.totalBytes)}${sp ? ` · ${sp}` : ""}`;
          }
        }
        const printer = state.printers.find((p) => p.id === payload.printerId);
        if (printer) {
          printer.state = "UPLOADING";
          printer.progress = payload.percent;
          const pSub = document.querySelector(`[data-printer-sub="${printer.id}"]`);
          if (pSub) pSub.textContent = `Überträgt · ${Math.round(payload.percent || 0)}%`;
        }
      } catch {}
    });
    es.addEventListener("job_done", (ev) => {
      try {
        const payload = JSON.parse(ev.data)?.data;
        loadState();
        if (!payload) return;
        if (state.screen === "sending" && payload.jobId === state.selectedJobId) {
          state.lastResult = { ...(state.lastResult || {}), ...payload };
          openScreen("success");
        } else {
          showToast(`Druck gestartet: ${payload.displayName || ""} auf ${payload.printerName || ""}`);
        }
      } catch {}
    });
    es.addEventListener("job_failed", (ev) => {
      try {
        const payload = JSON.parse(ev.data)?.data;
        loadState();
        if (!payload) return;
        const msg = payload.charged
          ? `Übertragung fehlgeschlagen (${payload.reason}) – beim erneuten Versuch „Bereits bezahlt“ nutzen.`
          : `Übertragung fehlgeschlagen (${payload.reason}).`;
        showToast(msg);
        if (state.screen === "sending" && payload.jobId === state.selectedJobId) {
          returnToQueue(false);
        }
      } catch {}
    });
    es.addEventListener("rfid_scan", (ev) => {
      try {
        const payload = JSON.parse(ev.data);
        const uid = payload?.data?.uid;
        if (uid) handleRfidScan(uid);
      } catch {}
    });
    es.onerror = () => {
      state.connected = false;
      renderHeader();
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
        if (!state.session.approved) {
          openScreen("not_approved");
          return;
        }
        if (state.rfidPurpose === "account") {
          openScreen("account");
        } else {
          openScreen("payment_confirm");
        }
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

    // Convenience: scanning a card on the idle queue screen opens Account directly
    if (state.screen === "queue") {
      state.rfidPurpose = "account";
      state.screen = "rfid_wait";
      await handleRfidScan(uid);
    }
  }

  // --- Job Actions ---
  async function executeStart(mode, extra = {}) {
    const job = selectedJob();
    if (!job || !state.session) {
      showToast("Job oder Sitzung nicht mehr gültig.");
      return returnToQueue(true);
    }
    state.sendingMode = mode;
    state.lastResult = null;
    state.sendProgress = {
      stage: mode === "personal" ? "charging" : "uploading",
      percent: 0,
      sentBytes: 0,
      totalBytes: Number(job.fileSize || 0),
      bytesPerSec: 0,
    };
    openScreen("sending");
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

      if (res.started && res.uploading) {
        state.lastResult = res;
        state.session = null;
        state.sendProgress.stage = "uploading";
        await loadState();
        renderModal();
        // Auto-minimize after 3s so the user sees payment confirmation and upload continues in queue
        setTimeoutRing(3, () => returnToQueue(false));
        return;
      }

      if (res.started) {
        state.lastResult = res;
        state.session = null;
        await loadState();
        openScreen("success");
        return;
      }

      if (res.reason === "PRINTER_BUSY" || res.reason === "PRINTER_OFFLINE") {
        state.lastResult = res;
        openScreen("printer_busy");
        return;
      }

      if (res.reason === "INSUFFICIENT_BALANCE") {
        state.session.balanceCents = res.balanceCents;
        state.lastResult = res;
        openScreen("insufficient");
        return;
      }

      if (res.reason === "PIN_INVALID" || res.reason === "PIN_REQUIRED") {
        state.pinValue = "";
        state.pinError = "PIN ist falsch – bitte erneut versuchen.";
        openScreen("pin_entry");
        return;
      }

      if (res.reason === "JOB_EXPIRED") {
        showToast("Job ist abgelaufen.");
        return returnToQueue(true);
      }

      showToast(`Fehler: ${res.reason || "Unbekannter Fehler"}`);
      openScreen("payment_confirm");
    } catch (err) {
      showToast(err.message);
      openScreen("payment_confirm");
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
      showToast(`+${fmtEur(amountCents)} aufgeladen`);
      openScreen(state.topupReturnTo);
    } catch (err) {
      showToast(err.message);
    }
  }

  // --- Screen Transitions ---
  function openScreen(name) {
    state.screen = name;
    if (name === "rfid_wait" || name === "bv_wait") {
      setTimeoutRing(60, () => returnToQueue(true));
    } else if (name === "payment_confirm" || name === "club_section" || name === "pin_entry" || name === "account" || name === "topup") {
      setTimeoutRing(60, () => returnToQueue(true));
    } else if (name === "insufficient" || name === "not_approved" || name === "overdraft_confirm") {
      setTimeoutRing(30, () => returnToQueue(true));
    } else if (name === "success") {
      setTimeoutRing(3, () => returnToQueue(false));
    } else if (name === "approved_ok") {
      setTimeoutRing(4, () => openScreen(state.rfidPurpose === "print" ? "payment_confirm" : "account"));
    } else {
      setTimeoutRing(null, null);
    }
    renderModal();
  }

  // --- Render Functions ---
  function renderHeader() {
    $("service-name").textContent = state.serviceName;
    $("conn-dot").classList.toggle("offline", !state.connected);

    const bar = $("printer-bar");
    bar.style.gridTemplateColumns = `repeat(${Math.max(1, state.printers.length)}, minmax(0, 1fr))`;
    bar.innerHTML = state.printers
      .map((p) => {
        const temp =
          p.nozzleTemp != null && p.bedTemp != null
            ? ` · ${Math.round(p.nozzleTemp)}°/${Math.round(p.bedTemp)}°`
            : "";
        let sub = stateLabel(p.state);
        if (p.state === "UPLOADING") {
          sub = `Überträgt · ${Math.round(p.progress || 0)}%`;
        } else if (p.state === "PRINTING" && p.progress != null) {
          sub = `${stateLabel(p.state)} · ${Math.round(p.progress)}%${temp}`;
        } else if (p.state === "ATTENTION" && p.error) {
          sub = p.error;
        } else if (p.state !== "OFFLINE") {
          sub = `${stateLabel(p.state)}${temp}`;
        }
        return `
          <div class="printer-tile state-${escapeHtml(p.state)}">
            <div class="printer-slot">${p.slot}</div>
            <div class="printer-info">
              <div class="printer-name">${escapeHtml(p.name)}</div>
              <div class="printer-state" data-printer-sub="${escapeHtml(p.id)}">${escapeHtml(sub)}</div>
            </div>
          </div>
        `;
      })
      .join("");
  }

  function renderQueue() {
    const list = $("queue-list");
    const empty = $("queue-empty");

    if (!state.jobs.length) {
      list.innerHTML = "";
      empty.classList.remove("hidden");
      return;
    }

    empty.classList.add("hidden");
    list.innerHTML = state.jobs
      .map((j) => {
        const rem = Math.max(0, Math.floor((j._clientExpiresAt - Date.now()) / 1000));
        const thumb = j.thumbnailUrl
          ? `<img src="${escapeHtml(j.thumbnailUrl)}" alt="">`
          : "🧊";
        const warnClass = rem <= 60 ? "warn" : "";
        const isSending = j.status === "sending";
        const up = j.uploadProgress || { percent: 0, sentBytes: 0, totalBytes: j.fileSize || 0, bytesPerSec: 0 };
        const sp = fmtSpeed(up.bytesPerSec);
        const actionsHtml = isSending
          ? `
            <div class="job-upload-box">
              <div class="job-upload-track">
                <div class="job-upload-fill" data-upload-fill="${escapeHtml(j.id)}" style="width:${Math.round(up.percent || 0)}%"></div>
              </div>
              <div class="job-upload-text" data-upload-text="${escapeHtml(j.id)}">
                Überträgt ${Math.round(up.percent || 0)} % · ${fmtMB(up.sentBytes)} / ${fmtMB(up.totalBytes || j.fileSize)}${sp ? ` · ${sp}` : ""}
              </div>
            </div>
            <button type="button" class="btn btn-delete-danger" data-del-id="${escapeHtml(j.id)}" title="Übertragung abbrechen">✕</button>
          `
          : `
            <div class="job-price">
              <div class="job-price-val">${fmtEur(j.priceCents)}</div>
              <div class="job-ttl ${warnClass}" data-ttl-id="${escapeHtml(j.id)}">Verfällt ${fmtCountdown(rem)}</div>
            </div>
            <button type="button" class="btn btn-primary" data-print-id="${escapeHtml(j.id)}">Drucken</button>
            <button type="button" class="btn btn-delete-danger" data-del-id="${escapeHtml(j.id)}" title="Job entfernen">✕</button>
          `;
        return `
          <div class="job-card-wrap">
            <div class="job-swipe-bg">✕ ${isSending ? "Übertragung abbrechen" : "Job entfernen"}</div>
            <div class="job-card" data-job-id="${escapeHtml(j.id)}">
              <div class="job-thumb">${thumb}</div>
              <div class="job-main">
                <div class="job-title-row">
                  <div class="job-filename">${escapeHtml(j.displayName || j.filename)}</div>
                  <span class="pill">Slot ${j.printerSlot} · ${escapeHtml(j.printerName)}</span>
                </div>
                <div class="job-meta">
                  <span>Upload: <strong>${fmtTime(j.createdAt)}</strong></span>
                  <span>Dauer: <strong>${fmtDuration(j.estimatedTimeSeconds)}</strong></span>
                  <span>Filament: <strong>${Number(j.filamentGrams || 0).toFixed(1)} g</strong> (${escapeHtml(j.filamentType)})</span>
                </div>
              </div>
              <div class="job-actions">${actionsHtml}</div>
            </div>
          </div>
        `;
      })
      .join("");
  }

  function renderDevBar() {
    $("dev-bar").classList.toggle("hidden", !state.modes.devMock);
    $("dev-toggle-p1").classList.toggle("hidden", state.modes.printer === "live");
  }

  function renderModal() {
    const overlay = $("overlay");
    if (state.screen === "queue") {
      overlay.classList.add("hidden");
      return;
    }
    overlay.classList.remove("hidden");
    const closeBtn = $("modal-close");
    const isUploading = state.screen === "sending" && Boolean(state.lastResult?.uploading);
    const hideClose = state.screen === "sending" && !isUploading;
    closeBtn.classList.toggle("hidden", hideClose);
    closeBtn.classList.toggle("minimize", isUploading);
    closeBtn.textContent = isUploading ? "−" : "✕";
    closeBtn.setAttribute("aria-label", isUploading ? "Minimieren (läuft im Hintergrund weiter)" : "Abbrechen");

    const title = $("modal-title");
    const body = $("modal-body");
    const footer = $("modal-footer");
    const job = selectedJob();
    const sess = state.session;

    if (state.screen === "rfid_wait") {
      title.textContent = "RFID-Karte vorhalten";
      body.innerHTML = `
        <div class="center-box">
          <div class="rfid-pulse">📡</div>
          <div style="font-size:21px;font-weight:700;">Bitte Mitgliedskarte an den Leser halten</div>
          ${job ? `<div style="color:var(--muted);font-size:16px;">Job: <strong>${escapeHtml(job.displayName || job.filename)}</strong> (${fmtEur(job.priceCents)})</div>` : ""}
        </div>
      `;
      footer.innerHTML = `<button class="btn btn-secondary" data-action="back-queue">Zurück</button>`;
      return;
    }

    if (state.screen === "payment_confirm" && job && sess) {
      const after = sess.balanceCents - job.priceCents;
      const enough = after >= 0;
      const maxNeg = Number(state.features.maxNegativeBalanceCents || 0);
      const canOverdraft =
        !enough && Boolean(state.features.allowNegativeBalance) && (maxNeg === 0 || after >= -maxNeg);
      title.textContent = "Zahlung & Druck bestätigen";
      body.innerHTML = `
        <div style="font-size:16px;color:var(--muted);margin-bottom:12px;">
          Nutzer: <strong style="color:var(--text);">${escapeHtml(sess.user.display_name)}</strong> ·
          Job: <strong style="color:var(--text);">${escapeHtml(job.displayName || job.filename)}</strong> (${escapeHtml(job.printerName)})
        </div>
        <div class="summary-grid">
          <div class="stat-card">
            <div class="stat-label">Aktuelles Guthaben</div>
            <div class="stat-value ${sess.balanceCents < 0 ? "negative" : ""}">${fmtEur(sess.balanceCents)}</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">Druckkosten (${Number(job.filamentGrams).toFixed(1)} g)</div>
            <div class="stat-value">${fmtEur(job.priceCents)}</div>
          </div>
          <div class="stat-card" style="grid-column: span 2;">
            <div class="stat-label">Guthaben nach Druck</div>
            <div class="stat-value ${enough ? "positive" : "negative"}">${fmtEur(after)}</div>
          </div>
        </div>
        ${
          !enough
            ? canOverdraft
              ? `<div class="alert-box alert-warn">Guthaben reicht nicht aus (es fehlen ${fmtEur(-after)}). Du kannst aufladen oder auf Guthaben-Minus trotzdem drucken.</div>`
              : `<div class="alert-box alert-error">Guthaben reicht nicht aus (es fehlen ${fmtEur(-after)}) — Überziehungslimit überschritten. Bitte zuerst aufladen.</div>`
            : ""
        }
      `;
      footer.innerHTML = `
        <button class="btn btn-ghost" data-action="back-queue">Zurück</button>
        <button class="btn btn-secondary" data-action="open-already-paid">Bereits bezahlt</button>
        <button class="btn btn-blue" data-action="open-club">Vereinsdruck</button>
        ${
          enough
            ? `<button class="btn btn-primary" data-action="start-personal">Druck starten (${fmtEur(job.priceCents)})</button>`
            : `
              ${state.features.topup ? `<button class="btn btn-primary" data-action="open-topup-print">Guthaben aufladen</button>` : ""}
              ${canOverdraft ? `<button class="btn btn-amber" data-action="open-overdraft-confirm">Trotzdem drucken</button>` : ""}
            `
        }
      `;
      return;
    }

    if (state.screen === "insufficient" && job && sess) {
      const missing = Math.max(0, job.priceCents - sess.balanceCents);
      const after = sess.balanceCents - job.priceCents;
      const maxNeg = Number(state.features.maxNegativeBalanceCents || 0);
      const canOverdraft =
        Boolean(state.features.allowNegativeBalance) && (maxNeg === 0 || after >= -maxNeg);
      title.textContent = "Guthaben zu gering";
      body.innerHTML = `
        <div class="center-box">
          <div style="font-size:52px;">⚠️</div>
          <div style="font-size:24px;font-weight:800;">Es fehlen ${fmtEur(missing)}</div>
          <div style="font-size:16px;color:var(--muted);">
            Aktuelles Guthaben: ${fmtEur(sess.balanceCents)} · Druckkosten: ${fmtEur(job.priceCents)}
          </div>
          ${
            !canOverdraft && state.features.allowNegativeBalance
              ? `<div class="alert-box alert-error">Max. Überziehungslimit (${fmtEur(-maxNeg)}) würde überschritten. Bitte zuerst aufladen.</div>`
              : !state.features.topup
              ? `<div class="alert-box alert-warn">Aufladen erfolgt über easyVerwaltung im Browser.</div>`
              : ""
          }
        </div>
      `;
      footer.innerHTML = `
        <button class="btn btn-secondary" data-action="back-payment">Zurück</button>
        ${canOverdraft ? `<button class="btn btn-amber" data-action="open-overdraft-confirm">Trotzdem drucken</button>` : ""}
        ${state.features.topup ? `<button class="btn btn-primary" data-action="open-topup-print">Guthaben aufladen</button>` : ""}
      `;
      return;
    }

    if (state.screen === "overdraft_confirm" && job && sess) {
      const after = sess.balanceCents - job.priceCents;
      title.textContent = "Auf Guthaben-Minus drucken?";
      body.innerHTML = `
        <div class="alert-box alert-warn">
          Dein Guthaben reicht für diesen Druck nicht aus. Dein Kontostand gerät dadurch ins Minus — bitte lade dein Guthaben zeitnah wieder auf.
        </div>
        <div class="summary-grid">
          <div class="stat-card">
            <div class="stat-label">Aktuelles Guthaben</div>
            <div class="stat-value">${fmtEur(sess.balanceCents)}</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">Druckkosten</div>
            <div class="stat-value">${fmtEur(job.priceCents)}</div>
          </div>
          <div class="stat-card" style="grid-column: span 2;">
            <div class="stat-label">Neuer Kontostand nach Druck</div>
            <div class="stat-value negative">${fmtEur(after)}</div>
          </div>
        </div>
      `;
      footer.innerHTML = `
        <button class="btn btn-secondary" data-action="back-payment">Zurück</button>
        <button class="btn btn-amber" data-action="start-personal-overdraft">Dennoch drucken – ich lade später auf</button>
      `;
      return;
    }

    if (state.screen === "topup" && sess) {
      title.textContent = "Guthaben aufladen";
      body.innerHTML = `
        <div style="font-size:16px;color:var(--muted);margin-bottom:14px;">
          Aktuelles Guthaben von <strong style="color:var(--text);">${escapeHtml(sess.user.display_name)}</strong>:
          <strong style="color:var(--primary);font-size:20px;">${fmtEur(sess.balanceCents)}</strong>
        </div>
        <div class="choice-grid">
          <button class="btn btn-blue choice-btn" data-topup="500">+ 5,00 € aufladen</button>
          <button class="btn btn-blue choice-btn" data-topup="1000">+ 10,00 € aufladen</button>
          <button class="btn btn-blue choice-btn" data-topup="2000">+ 20,00 € aufladen</button>
          <button class="btn btn-blue choice-btn" data-topup="5000">+ 50,00 € aufladen</button>
        </div>
      `;
      footer.innerHTML = `<button class="btn btn-secondary" data-action="topup-back">Zurück</button>`;
      return;
    }

    if (state.screen === "club_section") {
      title.textContent = "Vereinsdruck — Bereich wählen";
      body.innerHTML = `
        <div class="choice-grid">
          ${state.sections
            .map(
              (s) => `
              <button class="btn btn-secondary choice-btn" data-section-id="${escapeHtml(s.sectionId)}">
                🏛️ ${escapeHtml(s.name)}
              </button>
            `
            )
            .join("")}
        </div>
      `;
      footer.innerHTML = `<button class="btn btn-secondary" data-action="back-payment">Zurück</button>`;
      return;
    }

    if (state.screen === "pin_entry") {
      const isClub = state.pinMode === "club";
      const sec = state.sections.find((s) => s.sectionId === state.selectedSectionId);
      title.textContent = isClub ? "Vereinsdruck bestätigen (PIN)" : "Bereits bezahlt bestätigen (PIN)";
      const dots = Array.from({ length: 4 }, (_, i) => `<div class="pin-dot ${i < state.pinValue.length ? "filled" : ""}"></div>`).join("");
      body.innerHTML = `
        <div class="pin-layout">
          <div>
            <div style="font-size:17px;line-height:1.5;margin-bottom:12px;">
              ${
                isClub
                  ? `Vereinsdruck für Bereich <strong>${escapeHtml(sec?.name || "")}</strong>.<br>Bitte persönliche PIN eingeben.`
                  : `Ich bestätige, dass dieser Druck nach einem Fehldruck <strong>bereits bezahlt</strong> wurde.<br>Bitte persönliche PIN eingeben.`
              }
            </div>
            <div class="pin-dots">${dots}</div>
            ${state.pinError ? `<div class="alert-box alert-error">${escapeHtml(state.pinError)}</div>` : ""}
          </div>
          <div class="numpad">
            ${["1","2","3","4","5","6","7","8","9","⌫","0","✓"]
              .map((k) => `<button type="button" class="btn ${k === "✓" ? "btn-primary" : "btn-secondary"}" data-key="${k}">${k}</button>`)
              .join("")}
          </div>
        </div>
      `;
      footer.innerHTML = `<button class="btn btn-secondary" data-action="${isClub ? "open-club" : "back-payment"}">Zurück</button>`;
      return;
    }

    if (state.screen === "sending") {
      const r = state.lastResult;
      let paidBanner = "";
      if (r?.uploading) {
        if (r.mode === "club") {
          paidBanner = `<div class="alert-box alert-ok" style="width:min(560px,100%);">✅ Vereinsdruck (${escapeHtml(r.sectionName || "")}) bestätigt</div>`;
        } else if (r.mode === "already_paid") {
          paidBanner = `<div class="alert-box alert-ok" style="width:min(560px,100%);">✅ Nachdruck (Bereits bezahlt) – keine erneute Abbuchung</div>`;
        } else {
          paidBanner = `<div class="alert-box alert-ok" style="width:min(560px,100%);">✅ Bezahlt: ${fmtEur(r.chargedCents)} · Kontostand: ${fmtEur(r.remainingBalanceCents)}</div>`;
        }
      }
      title.textContent = "Druck wird an Drucker gesendet…";
      body.innerHTML = `
        <div class="center-box">
          ${paidBanner}
          <div style="font-size:18px;font-weight:700;" id="send-stage-text"></div>
          <div class="progress-wrap">
            <div class="progress-track">
              <div class="progress-fill" id="send-progress-fill"></div>
            </div>
            <div class="progress-meta">
              <span id="send-progress-pct">0 %</span>
              <span class="progress-bytes" id="send-progress-bytes"></span>
            </div>
          </div>
          <div style="font-size:14px;color:var(--muted);" id="send-mode-hint"></div>
        </div>
      `;
      footer.innerHTML = r?.uploading
        ? `
          <button class="btn btn-danger" style="margin-right:auto;" data-action="abort-upload">Übertragung abbrechen</button>
          <button class="btn btn-amber" data-action="close-success">Im Hintergrund weiterladen</button>
        `
        : "";
      updateSendingProgressDOM();
      return;
    }

    if (state.screen === "printer_busy") {
      const isOffline = state.lastResult?.reason === "PRINTER_OFFLINE";
      title.textContent = isOffline ? "Drucker nicht erreichbar" : "Drucker belegt";
      body.innerHTML = `
        <div class="center-box">
          <div style="font-size:52px;">🔴</div>
          <div style="font-size:20px;font-weight:700;">
            ${isOffline ? "Der Zieldrucker ist aktuell offline." : "Der Zieldrucker druckt bereits oder wartet auf Bestätigung."}
          </div>
          <div class="alert-box alert-ok">Es wurde kein Guthaben abgebucht.</div>
        </div>
      `;
      footer.innerHTML = `
        <button class="btn btn-ghost" data-action="back-queue">Abbrechen</button>
        <button class="btn btn-secondary" data-action="back-payment">Zurück</button>
      `;
      return;
    }

    if (state.screen === "success") {
      const r = state.lastResult || {};
      const detail =
        r.mode === "club"
          ? `Vereinsdruck (${escapeHtml(r.sectionName || "")})`
          : r.mode === "already_paid"
          ? "Nachdruck (Bereits bezahlt)"
          : `Abgebucht: <strong>${fmtEur(r.chargedCents)}</strong> · Kontostand: <strong style="color:${r.remainingBalanceCents < 0 ? "var(--amber)" : "var(--text)"}">${fmtEur(r.remainingBalanceCents)}</strong>${r.remainingBalanceCents < 0 ? " (bitte bald aufladen)" : ""}`;
      title.textContent = "Druck gestartet";
      body.innerHTML = `
        <div class="center-box">
          <div style="font-size:58px;">✅</div>
          <div style="font-size:24px;font-weight:800;">Auftrag an ${escapeHtml(r.printerName || "Drucker")} gesendet</div>
          <div style="font-size:18px;color:var(--muted);">${detail}</div>
        </div>
      `;
      footer.innerHTML = `<button class="btn btn-primary" data-action="close-success">Zur Warteschlange</button>`;
      return;
    }

    if (state.screen === "account" && sess) {
      title.textContent = "Konto & Guthaben";
      const txs = (sess.transactions || [])
        .map(
          (t) => `
          <div class="tx-row">
            <span>${escapeHtml(t.title)}</span>
            <strong style="color:${t.amountCents >= 0 ? "var(--primary)" : "var(--text)"}">
              ${t.amountCents >= 0 ? "+" : ""}${fmtEur(t.amountCents)}
            </strong>
          </div>
        `
        )
        .join("");
      body.innerHTML = `
        <div class="summary-grid">
          <div class="stat-card">
            <div class="stat-label">Mitglied</div>
            <div class="stat-value" style="font-size:21px;">${escapeHtml(sess.user.display_name)}</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">Aktuelles Guthaben</div>
            <div class="stat-value ${sess.balanceCents < 0 ? "negative" : "positive"}">${fmtEur(sess.balanceCents)}</div>
          </div>
        </div>
        <div style="font-size:14px;color:var(--muted);margin-bottom:8px;">Letzte Buchungen</div>
        <div class="tx-list">${txs || `<div class="tx-row"><span>Keine Buchungen vorhanden</span></div>`}</div>
      `;
      footer.innerHTML = `
        ${state.features.topup ? `<button class="btn btn-blue" data-action="open-topup-account">Guthaben aufladen</button>` : ""}
        <button class="btn btn-secondary" data-action="back-queue">Schließen</button>
      `;
      return;
    }

    if (state.screen === "not_approved" && sess) {
      title.textContent = "Keine 3D-Druck-Freigabe";
      body.innerHTML = `
        <div class="center-box">
          <div style="font-size:52px;">🚫</div>
          <div style="font-size:20px;font-weight:700;">Hallo ${escapeHtml(sess.user.display_name)}, für dein Konto ist noch keine 3D-Druck-Einweisung hinterlegt.</div>
          <div style="color:var(--muted);font-size:16px;">Bitte lass dich von einer berechtigten Person (BV / Einweiser) freischalten.</div>
        </div>
      `;
      footer.innerHTML = `
        <button class="btn btn-secondary" data-action="back-queue">Zurück</button>
        ${state.features.approval ? `<button class="btn btn-primary" data-action="open-bv-wait">Freigabe beantragen</button>` : ""}
      `;
      return;
    }

    if (state.screen === "bv_wait") {
      title.textContent = "BV / Einweisungs-Karte vorhalten";
      body.innerHTML = `
        <div class="center-box">
          <div class="rfid-pulse">🔑</div>
          <div style="font-size:20px;font-weight:700;">Bitte berechtigte Einweisungs-Karte scannen</div>
        </div>
      `;
      footer.innerHTML = `<button class="btn btn-secondary" data-action="back-not-approved">Zurück</button>`;
      return;
    }

    if (state.screen === "approved_ok") {
      title.textContent = "Nutzer freigeschaltet";
      body.innerHTML = `
        <div class="center-box">
          <div style="font-size:56px;">✅</div>
          <div style="font-size:22px;font-weight:800;">Freigabe erfolgreich erteilt</div>
          <div style="color:var(--muted);">Durch: ${escapeHtml(state.lastResult?.instructorName || "Einweiser")}</div>
        </div>
      `;
      footer.innerHTML = "";
    }
  }

  // --- Event Delegation ---
  $("modal-close").addEventListener("click", () => {
    returnToQueue(state.screen !== "success");
  });

  $("btn-open-account").addEventListener("click", () => {
    state.selectedJobId = null;
    state.rfidPurpose = "account";
    openScreen("rfid_wait");
  });

  async function cancelOrAbortJob(jobId) {
    try {
      const res = await api(`/api/jobs/${encodeURIComponent(jobId)}`, { method: "DELETE" });
      if (res.wasUploading) {
        showToast("Übertragung abgebrochen — falls bereits bezahlt, beim Neustart „Bereits bezahlt“ nutzen.");
      }
      await loadState();
    } catch (err) {
      showToast(err.message);
    }
  }

  $("queue-list").addEventListener("click", async (ev) => {
    const printBtn = ev.target.closest("[data-print-id]");
    if (printBtn) {
      state.selectedJobId = printBtn.dataset.printId;
      state.rfidPurpose = "print";
      openScreen("rfid_wait");
      return;
    }
    const delBtn = ev.target.closest("[data-del-id]");
    if (delBtn) {
      await cancelOrAbortJob(delBtn.dataset.delId);
    }
  });

  // Touch swipe-left on job cards to remove job or abort running transfer
  let swipeCard = null;
  let swipeStartX = 0;
  let swipeStartY = 0;
  let swipeDx = 0;
  let swipeLocked = false;

  $("queue-list").addEventListener("touchstart", (ev) => {
    if (ev.touches.length !== 1) return;
    const card = ev.target.closest(".job-card");
    if (!card) return;
    swipeCard = card;
    swipeStartX = ev.touches[0].clientX;
    swipeStartY = ev.touches[0].clientY;
    swipeDx = 0;
    swipeLocked = false;
  }, { passive: true });

  $("queue-list").addEventListener("touchmove", (ev) => {
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

  $("queue-list").addEventListener("touchend", async () => {
    if (!swipeCard) return;
    const card = swipeCard;
    const dx = swipeDx;
    swipeCard = null;
    card.classList.remove("swiping");
    if (dx <= -100) {
      card.style.transform = "translateX(-100%)";
      await cancelOrAbortJob(card.dataset.jobId);
    } else {
      card.style.transform = "";
    }
  });

  $("overlay").addEventListener("click", async (ev) => {
    const actionBtn = ev.target.closest("[data-action]");
    if (actionBtn) {
      const a = actionBtn.dataset.action;
      if (a === "back-queue") return returnToQueue(true);
      if (a === "close-success") return returnToQueue(false);
      if (a === "abort-upload") {
        const jid = state.selectedJobId;
        await returnToQueue(false);
        if (jid) await cancelOrAbortJob(jid);
        return;
      }
      if (a === "back-payment") return openScreen("payment_confirm");
      if (a === "open-insufficient") return openScreen("insufficient");
      if (a === "open-overdraft-confirm") return openScreen("overdraft_confirm");
      if (a === "start-personal-overdraft") return executeStart("personal", { allowOverdraft: true });
      if (a === "open-topup-print") {
        state.topupReturnTo = "payment_confirm";
        return openScreen("topup");
      }
      if (a === "open-topup-account") {
        state.topupReturnTo = "account";
        return openScreen("topup");
      }
      if (a === "topup-back") return openScreen(state.topupReturnTo);
      if (a === "open-club") return openScreen("club_section");
      if (a === "open-already-paid") {
        state.pinMode = "already_paid";
        state.pinValue = "";
        state.pinError = "";
        return openScreen("pin_entry");
      }
      if (a === "start-personal") return executeStart("personal");
      if (a === "open-bv-wait") return openScreen("bv_wait");
      if (a === "back-not-approved") return openScreen("not_approved");
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

    const keyBtn = ev.target.closest("[data-key]");
    if (keyBtn) {
      const k = keyBtn.dataset.key;
      state.pinError = "";
      if (k === "⌫") {
        state.pinValue = state.pinValue.slice(0, -1);
        renderModal();
      } else if (k === "✓") {
        if (state.pinValue.length < 4) {
          state.pinError = "Bitte mindestens 4 Ziffern eingeben.";
          renderModal();
          return;
        }
        await executeStart(state.pinMode, {
          pin: state.pinValue,
          sectionId: state.selectedSectionId,
        });
      } else if (state.pinValue.length < 8) {
        state.pinValue += k;
        renderModal();
      }
    }
  });

  // --- Dev Mock Bar Events ---
  $("dev-upload").addEventListener("click", async () => {
    try {
      await api("/api/dev/upload-sample", { method: "POST", body: JSON.stringify({ slot: 1 }) });
      showToast("Test-Job auf Slot 1 hochgeladen");
    } catch (err) {
      showToast(err.message);
    }
  });

  $("dev-toggle-p1").addEventListener("click", async () => {
    const p1 = state.printers.find((p) => p.slot === 1);
    if (!p1) return;
    const next = p1.state === "READY" ? "PRINTING" : "READY";
    try {
      await api("/api/dev/printer-state", {
        method: "POST",
        body: JSON.stringify({ printerId: p1.id, state: next }),
      });
      showToast(`${p1.name}: ${stateLabel(next)}`);
    } catch (err) {
      showToast(err.message);
    }
  });

  $("dev-bar").addEventListener("click", async (ev) => {
    const btn = ev.target.closest("[data-rfid]");
    if (!btn) return;
    try {
      await api("/api/dev/simulate-rfid", {
        method: "POST",
        body: JSON.stringify({ uid: btn.dataset.rfid }),
      });
    } catch (err) {
      showToast(err.message);
    }
  });

  // --- Periodic Tick (Clock, Job TTL Countdown, Modal Timeout Ring) ---
  setInterval(() => {
    $("clock").textContent = new Date().toLocaleTimeString("de-DE", {
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit",
    });
    updateTimeoutRing();
    for (const j of state.jobs) {
      const el = document.querySelector(`[data-ttl-id="${j.id}"]`);
      if (!el) continue;
      const rem = Math.max(0, Math.floor((j._clientExpiresAt - Date.now()) / 1000));
      el.textContent = `Verfällt ${fmtCountdown(rem)}`;
      el.classList.toggle("warn", rem <= 60);
    }
  }, 500);

  // Fallback periodic state refresh (safety net if SSE ever stalls)
  setInterval(loadState, 15000);

  loadState();
  connectSSE();
})();
