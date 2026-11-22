# Projektregeln – SmartTerminal 3D-Print

## Arbeitsweise & Stil
- **Kurz & kompakt:** Doku gehört prägnant in READMEs direkt im Repo. Keine Ausschweifungen, kein AI-Bloat.
- **Alles in GitHub:** Code, Doku, Konfigurationen (Templates) und Entscheidungen leben versioniert in diesem Repository.
- **Nachbar-Repo `easyVerwaltung` strikt read-only:** Niemals Änderungen am `easyVerwaltung`-Repository vornehmen – dort ausschließlich lesen/nachschauen.
- **Zielgruppe:** Der Maintainer ist Hardware-Entwickler – Software-Architektur klar, pragmatisch und auf Block-/Schnittstellenebene erklären statt in Framework-Jargon.
- **Iterativ testbar:** Jeder Schritt soll direkt auf dem Tisch testbar sein (Mac lokal + Raspberry Pi mit DSI-Display + Prusa-Drucker).

## Technische Leitprinzipien
- **Maximale Uptime & Zuverlässigkeit (24/7-Betrieb):**
  - Kein Single-Point-of-Failure im lokalen Ablauf; Ausfälle von Netzwerk, Server oder einzelnen Druckern müssen sauber abgefangen werden.
  - Immer Timeouts, Retries mit Backoff, Healthchecks und sauberes Auto-Recovery vorsehen.
  - Persistenter Zustand (z. B. SQLite mit WAL-Mode) statt reinem In-Memory-State, damit Stromausfälle/Neustarts keinen Inkonsistenz-Zustand hinterlassen.
  - Begrenzte Logs & Speicherbereinigung (z. B. alte G-Code-Dateien/Jobs aufräumen), damit die SD-Karte nicht vollläuft und geschont wird.
  - Lieber einfach, robust und wartbar als über-engineered.
