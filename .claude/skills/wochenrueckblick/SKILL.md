---
name: wochenrueckblick
description: Wochenrückblick / weekly review der Git-Aktivität aller SDS_110-Repositories (Commits, Merges, offene PRs, offene Punkte). Ausgabe auf Deutsch oder Englisch. Use when the user asks for a Wochenrückblick, weekly review, weekly summary or "was ist diese Woche passiert".
argument-hint: "[de|en] [Tage|days, Standard 7] [seit YYYY-MM-DD]"
---

# Wochenrückblick / Weekly review

Erstelle einen Rückblick über die Git-Aktivität der SDS_110-Repositories.

## 1. Argumente auswerten

Argumente: `$ARGUMENTS` (Reihenfolge beliebig, alle optional)

| Argument | Bedeutung | Standard |
|---|---|---|
| `de`, `deutsch`, `german` | Ausgabe auf Deutsch | **de** |
| `en`, `englisch`, `english` | Ausgabe auf Englisch | |
| Zahl, z. B. `14` | Zeitraum in Tagen bis heute | **7** |
| Datum `YYYY-MM-DD` | Beginn des Zeitraums (statt Tagen) | |

Ohne Sprachangabe gilt Deutsch. Fragt der Benutzer ausdrücklich auf Englisch, gilt Englisch.

## 2. Repositories finden

Alle Git-Repositories im Arbeitsverzeichnis und eine Ebene darunter erfassen, z. B.:

```bash
for d in . */; do git -C "$d" rev-parse --show-toplevel 2>/dev/null; done | sort -u
```

Erwartet werden `SDS_110` und `SDS_110_STM32F746ZGT6`. Fehlt eines davon, im Bericht vermerken.

## 3. Daten sammeln

Für jedes Repository (`SINCE` = Startdatum):

```bash
git -C "$R" fetch --all -q
git -C "$R" log --all --since="$SINCE" --date=short --no-merges --pretty="%ad %h %an | %s"
git -C "$R" log --all --since="$SINCE" --merges --oneline | wc -l
```

Bei mehr als 60 Commits die Ausgabe vollständig lesen (in Teilen), nichts auslassen.

Offene und im Zeitraum gemergte Pull Requests über die GitHub-MCP-Tools holen
(`mcp__github__list_pull_requests`, Owner `drstefanhilger-commits`, `state: all`,
nur Felder `number, title, state, draft, merged_at, html_url, head`). Ohne
GitHub-Zugriff diesen Teil weglassen und das im Bericht sagen.

Für unklare Commit-Betreffzeilen bei Bedarf `git show --stat <hash>` ansehen.

## 4. Bericht schreiben

Struktur (Überschriften in der gewählten Sprache):

1. **Titel** mit Zeitraum und Kalenderwoche(n) – DE: „Wochenrückblick KW xx“, EN: „Weekly review CW xx“
2. **Je Repository ein Abschnitt**
   - Kopfzeile: Anzahl Commits, davon Merges, PR-Nummernbereich, Status
   - Thematisch nach Tagen gruppiert (z. B. „25.–26.09.: Analyse-Befunde“), nicht jeden Commit einzeln
   - Befund-Nummern, Modulnummern (118, 122, 124, 126, 128 …), USB-Kommando-Ids und Messwerte (z. B. Proc in ms) erhalten
3. **Offene Punkte / Open items**
   - offene PRs (Entwurf/bereit) als Markdown-Link mit voller URL
   - in Commits/Doku genannte offene Probleme (Hardwarefehler, Rechenlast-Reserven …)
   - Auffälligkeiten, z. B. Commits mit Autor „Your Name“ (nicht gesetzte Git-Identität)

Regeln:
- Fachbegriffe und Bezeichner (Befund → *finding*, Modulnamen, Kommandonamen) in Englisch sinngemäß übersetzen, Code-Bezeichner unverändert in Backticks.
- Datumsformat: DE `TT.MM.`, EN `MMM DD` (z. B. „Sep 25“).
- Nur Fakten aus Git/GitHub, nichts erfinden. Kommt ein Abschnitt leer heraus, „Keine Aktivität“ / „No activity“ schreiben.
- PRs nie als nackte `#123`, immer `owner/repo#123` oder als Link.

## 5. Abschluss

Am Ende in einer Zeile anbieten, den Rückblick als Dokument zu speichern.
