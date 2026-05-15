# 24/7 Stabilitaets-Review - Improvements

Datum: 15.05.2026
Scope: DOOR_legacy, RFID_BOX_legacy, RFID_BOX_easyVerwaltung, LASERSAUR_HMI_legacy

## Prioritaet: Kritisch

### 1) Netzwerk- und I/O-Operationen aus ISR entfernen
Problem:
- In LASERSAUR_HMI_legacy laufen TCP-Connect, HTTP-Write und Serial-Ausgaben im Timer-Interrupt.
- Das kann Interrupt-Latenzen, Deadlocks, Timing-Drift und sporadische Hangs verursachen.

Betroffene Stellen:
- firmware/projects/LASERSAUR_HMI_legacy/src/main.cpp:165
- firmware/projects/LASERSAUR_HMI_legacy/src/main.cpp:176
- firmware/projects/LASERSAUR_HMI_legacy/src/main.cpp:188
- firmware/projects/LASERSAUR_HMI_legacy/src/main.cpp:242

Verbesserung:
- ISR nur als Trigger nutzen (Flag setzen, Zaehler inkrementieren).
- Netzwerk/Serial ausschliesslich im loop() ausfuehren.
- Ringbuffer oder Event-Queue fuer Messwerte verwenden.

Akzeptanzkriterium:
- Keine blockierenden Aufrufe in ISR-Kontext.
- Zyklische Tasks laufen auch bei Netzwerk-Ausfall deterministisch weiter.

### 2) JSON-Logging korrigieren und speichersicher machen
Problem:
- JsonObject wird erstellt, aber Payload-String nicht korrekt serialisiert.
- Content-Length basiert auf JSONString, das im gezeigten Pfad nicht befuellt wird.
- Repetitives createObject ohne klaren Reset ist langzeitkritisch.

Betroffene Stellen:
- firmware/projects/LASERSAUR_HMI_legacy/src/main.cpp:101
- firmware/projects/LASERSAUR_HMI_legacy/src/main.cpp:103
- firmware/projects/LASERSAUR_HMI_legacy/src/main.cpp:177
- firmware/projects/LASERSAUR_HMI_legacy/src/main.cpp:193
- firmware/projects/LASERSAUR_HMI_legacy/src/main.cpp:195

Verbesserung:
- Auf ArduinoJson v6/v7 migrieren (StaticJsonDocument, serializeJson).
- Payload in festen char-Buffer serialisieren und dessen Laenge fuer Content-Length nutzen.
- Nach jedem Sendezyklus Dokument/Buffer sauber zuruecksetzen.

Akzeptanzkriterium:
- Jede POST-Nachricht hat konsistente Content-Length und gueltigen JSON-Body.
- Keine schleichende Speichernutzung ueber lange Laufzeit.

## Prioritaet: Hoch

### 3) session_id immer nullterminieren
Problem:
- strncpy mit sizeof-1 kann bei langen Eingaben ohne '\0' enden.
- Danach wird session_id weiter genutzt (Heartbeat), was zu undefiniertem Verhalten fuehren kann.

Betroffene Stellen:
- firmware/projects/RFID_BOX_easyVerwaltung/src/main.cpp:423
- firmware/projects/RFID_BOX_easyVerwaltung/src/main.cpp:436

Verbesserung:
- Nach strncpy explizit terminieren:
  - session_id[sizeof(session_id) - 1] = '\0';
- Optional: vor Copy memset(session_id, 0, sizeof(session_id)).

Akzeptanzkriterium:
- session_id ist immer gueltig nullterminiert.

### 4) Harte Endlosschleifen durch kontrollierte Recovery ersetzen
Problem:
- stopSystem() blockiert dauerhaft mit while(1).
- Ohne Self-Recovery bleibt das System im Feld haengen.

Betroffene Stellen:
- firmware/projects/LASERSAUR_HMI_legacy/src/myFunctions.cpp:374
- firmware/projects/LASERSAUR_HMI_legacy/src/myFunctions.cpp:376

Verbesserung:
- Fehlerzustand mit periodischem Re-Init oder Watchdog-Reboot.
- Fehlergrund persistent loggen (EEPROM/RTC-RAM, falls sinnvoll).

Akzeptanzkriterium:
- Bei transienten Fehlern kommt das System ohne manuellen Eingriff wieder hoch.

## Prioritaet: Mittel

### 5) WLAN-Reconnect entblocken
Problem:
- connectToWiFi wartet bis zu 10 Sekunden mit delay(1000).
- Das reduziert Reaktionsfaehigkeit bei Verbindungsproblemen.

Betroffene Stellen:
- firmware/projects/RFID_BOX_legacy/src/main.cpp:304
- firmware/projects/RFID_BOX_easyVerwaltung/src/main.cpp:331

Verbesserung:
- Nicht-blockierende Reconnect-Strategie mit Retry-Intervall (millis-basiert).
- Backoff und Max-Retry-Fenster fuer stabile Netze.

Akzeptanzkriterium:
- Haupt-Loop bleibt responsiv, auch wenn WLAN laenger ausfaellt.

### 6) Dynamische String-Nutzung in Hot Paths reduzieren
Problem:
- Wiederholte String-Allokationen koennen Heap fragmentieren.

Betroffene Stellen:
- firmware/projects/RFID_BOX_legacy/src/main.cpp:114
- firmware/projects/RFID_BOX_legacy/src/main.cpp:396
- firmware/projects/RFID_BOX_easyVerwaltung/src/main.cpp:121

Verbesserung:
- Kritische Pfade auf feste char-Buffer umstellen (snprintf, strlcpy/strncpy + Terminierung).
- Falls String bleibt: reserve() fuer bekannte Groessen setzen.

Akzeptanzkriterium:
- Keine steigende Fragmentierung bei Langzeittest.

### 7) HTTP-Antworten robust validieren
Problem:
- Entscheidungslogik prueft nur auf Substring "true".
- Fehlerhafte oder unerwartete Antworten koennen falsch klassifiziert werden.

Betroffene Stelle:
- firmware/projects/DOOR_legacy/src/main.cpp:356

Verbesserung:
- HTTP-Statuscode, Header und klaren Body-Parser verwenden (JSON/Token-Parser).
- Negative/Timeout/Fallback-Pfade eindeutig behandeln.

Akzeptanzkriterium:
- Auth-Entscheidungen sind deterministisch und robust gegen Antwortrauschen.

## Empfohlene Umsetzungsreihenfolge
1. LASERSAUR_HMI_legacy: ISR entkoppeln + Logging-Pfad korrigieren.
2. RFID_BOX_easyVerwaltung: session_id-Fix + Reconnect entblocken.
3. RFID_BOX_legacy: String- und Reconnect-Haertung.
4. DOOR_legacy: Antwort-Parsing haerten.

## Testplan fuer 24/7-Freigabe
- 72h Soak-Test mit zyklischem Netzwerk-Ausfall (AP aus/an).
- Langzeittest mit Server-Timeouts und Paketverlust.
- Heap- und Stack-Monitoring (falls verfuegbar) jede Minute loggen.
- Auth-Regressionstests: valid, invalid, malformed, timeout.
- OTA-Test unter Last (nur wo OTA aktiviert ist).
