# Writing a contest for qTLF

> Quicker way: give the contest rules (PDF) and [AI-PROMPT-CONTEST.txt](AI-PROMPT-CONTEST.txt) to an AI —
> see *Create a contest folder with an AI* in the [README](../README.md#create-a-contest-folder-with-an-ai).

qTLF has two parts that talk through one small API:

* **the main program** (the executable) — entry line, keyer, log, database, the generic
  checks of each QSO, data files, network download, Cabrillo export;
* **a contest** — one folder in `rules/` describing everything specific to that contest:
  `contest.json` (data), `scoring.js` (points and multipliers) and optional QML windows.

A new contest never needs a new executable: copy a folder, edit it, ship the folder.

```
rules/RENCONTRES-UFT/          folder name = contest ID
├── contest.json               definition (this document)
├── scoring.js                 score(qsos, contest) → points, multipliers, total…
├── windows/Score.qml          custom windows (docks), any number
├── windows/Station.qml
├── UFT_mini_2021.csv          call_exchange file: always CSV …
└── cty.dat                    … except the country file (universal cty.dat format)
```

Start by copying `rules/GENERIC` (serial number) or `rules/SKM` / `rules/RENCONTRES-UFT`.
While qTLF runs, **Ctrl+R** (or `:reload`) reloads `scoring.js`, the data files and the QML
windows; changes to bands, modes or entry fields need the contest to be reopened.

If a `contest.json` cannot be read, the reason is shown when the contest is opened and qTLF goes
back to the contest list.

---

## 1. contest.json — complete reference

```json
{
    "name": "Rencontres UFT",
    "short_name": "RENCONTRES-UFT",
    "description": "Text shown in the contest selection dialog",
    "url": "https://… (rules page, optional)",

    "schedule":  { … },          // §1.1
    "modes":     ["CW"],         // §1.2
    "bands":     [ … ],          // §1.2
    "exchange":  { … },          // §1.3
    "dupe":      "band",         // §1.4
    "band_change": { … },        // §1.4b
    "call_exchange": { … },      // §1.5  CSV
    "country_file":  { … },      // §1.6  cty.dat
    "remote_update": "on_open",  // §1.7
    "scoring":   "scoring.js",   // §2
    "windows":   [ … ],          // §3
    "cabrillo":  { … }           // §4
}
```

| Key | Default | Meaning |
|---|---|---|
| `name` | folder name | Long name (selection dialog, window title). |
| `short_name` | folder name | Default log name, default Cabrillo `CONTEST:`. |
| `description`, `url` | — | Shown in the selection dialog. |

### 1.1 `schedule` — dates and times, always `start_utc` / `end_utc`

```json
"schedule": {
    "default_date": { "month": 11, "day": 7 },
    "periods": [
        { "day": 0, "start_utc": "05:00", "end_utc": "08:00" },
        { "day": 0, "start_utc": "15:00", "end_utc": "18:00" },
        { "day": 1, "start_utc": "07:00", "end_utc": "10:00" }
    ]
}
```

* `default_date` (optional): pre-fills the date when a contest is created (next occurrence).
* `periods`: one or more operating windows. `day` = days after the contest date (0 = the date
  itself). `start_utc` / `end_utc` are `"HH:mm"` UTC. If `end_utc` ≤ `start_utc` the period ends
  the next day (`"12:00"` → `"12:00"` = 24 h).
* `end_day` (optional): day of the end, for periods longer than 24 h. Saturday 06:00 → Sunday 18:00
  (36 h, contest date = the Saturday): `{ "day": 0, "start_utc": "06:00", "end_day": 1, "end_utc": "18:00" }`.
  48 h from Saturday 06:00: `"end_day": 2, "end_utc": "06:00"`.
* There is no `duration` key any more: a 3-hour contest is `"06:00"` → `"09:00"`.
* Uses: status-bar clock (*Contest running (period 1/3) — 02:14:52 left* / *starts in…* /
  *Contest over*), the **OUT OF PERIOD** check (§5), `contest.start`, `contest.end`,
  `contest.periods` for the script. Without `periods` there is no time check.

### 1.2 `modes`, `bands`

```json
"modes": ["CW"],
"bands": [
    { "name": "80m", "low_khz": 3500, "high_khz": 3800, "segment": [3520, 3560], "cabrillo": "3500" }
]
```

* `modes` (default `["CW"]`): more than one adds a Mode selector to the entry line.
* `bands` (required): `low_khz`/`high_khz` map a typed frequency to a band; band selector order =
  list order. `segment` (optional): the frequency field turns amber outside it (warning only).
  `cabrillo` (default = `low_khz`): frequency written in Cabrillo when the QSO has no frequency.

### 1.3 `exchange`

```json
"exchange": {
    "rst": { "sent": "599", "received": "599" },
    "sent": {
        "label": "My UFT Nr",
        "type": "fixed",                 // "fixed" or "serial"
        "prompt": "Your UFT member number, or NM",
        "serial_start": 1,               // serial only
        "serial_digits": 3               // serial only → 001, 002…
    },
    "received": {
        "label": "UFT Nr",
        "type": "text",                  // "text" | "number" | "callsign"
        "required": true,
        "pattern": "[0-9]+|NM",
        "prefill": "call_exchange",      // see §1.3b
        "completion": false              // see §1.3b
    }
}
```

| Key | Default | Meaning |
|---|---|---|
| `rst.sent`, `rst.received` | `599` | Default reports (PgUp/PgDn change them when Call is filled). |
| `sent.type` | `serial` | `serial`: 001, 002… computed from the log. `fixed`: asked once when the contest is created (`prompt` = hint). |
| `sent.label`, `received.label` | `Exch Sent` / `Exch Rcvd` | Entry-line and log column titles. |
| `received.type` | `text` | `number` → digits only; `callsign` → must look like a callsign. Checked in §5 (**BAD EXCH**). |
| `received.required` | `false` | Empty exchange → **NO EXCH**. |
| `received.pattern` | — | Regular expression the whole exchange must match (case-insensitive, anchored automatically). Example `[0-9]+\|NM`. |
| `received.prefill` | see §1.3b | Where the exchange is pre-filled from. |
| `received.completion` | `false` | Drop-down list on the exchange field (§1.3b). |

### 1.3b Completion (drop-down lists) and pre-fill

The lists show every line **containing** what is typed, in this order, without duplicates.
/P /M /MM… are ignored to recognise a station (F5ABC/P = F5ABC).

| What | Source 1 | Source 2 | Setting |
|---|---|---|---|
| **Call** drop-down | calls of the log of the contest (newest first) | column `call` of `call_exchange` | always on |
| **Exchange received** drop-down | exchanges of the log of the contest | column `exchange_field` of `call_exchange` | `"completion": true` (default `false`: useless for numbers) |
| **Exchange received** pre-fill (TAB or ENTER from Call, field empty) | newest QSO with this station in the log of the contest | `"prefill"`: `"call_exchange"` → column `exchange_field` of `call_exchange`; `"cqz"`, `"ituz"`, `"cont"`, `"dxcc"`, `"country"` → that field of cty.dat (§1.6) | `"prefill"` (default `"call_exchange"` if the file exists, else `"none"`) |

* `"prefill": "none"`: no pre-fill at all, not even from the log — for exchanges that change at each
  QSO (serial numbers).
* `"prefill": "cqz"` (CQ WW…), `"ituz"` (IARU HF…), `"cont"` need a `country_file`.
* In a list, ↓ then ENTER takes a line and continues like ENTER in the field; ENTER without choosing
  a line closes the list and acts on what is typed. TAB, ESC, +, PgUp/PgDn work as in the field.

### 1.4 `dupe`

`band` (default) · `band_mode` · `contest` · `none`. Used both for the live DUPE warning while
typing (ENTER then sends CW message 7) and for the **DUPE** status of logged QSOs (§5).

The operating suffixes **/P /M /MM /AM /QRP** are ignored: F5ABC, F5ABC/P and F5ABC/MM are the
same station (dupe, "worked on", call_exchange lookup). Prefixes and call areas are kept: EA8/F5ABC
and W1ABC/6 are other stations. The log, the Cabrillo file and `scoring.js` (`q.call`) always get the
call exactly as typed.

### 1.4b `band_change` — minimum time on a band

```json
"band_change": { "min_minutes": 10 }
```

Forces the operator to stay at least `min_minutes` on a band (the "10-minute rule").

| Key | Default | Meaning |
|---|---|---|
| `min_minutes` | `0` (no rule) | Minimum time on a band, counted from the **first** QSO made on that band, before a QSO on another band is accepted. |

A QSO on another band made too early gets the status **BAND CHANGE** (§5) and the operator is
considered to be still on the previous band. Each new operating period (§1.1) starts with a free
choice of band. While typing, the status bar shows *40m locked 3:37*
(amber), in red with a warning on the entry line if another band is selected (by hand or by the
TRX). The script receives the settings as `contest.band_change`.

Example, 10 min: 12:00 20m ✓ · 12:05 40m ✗ BAND CHANGE · 12:09 20m ✓ ·
12:11 40m ✓ (11 min after 12:00) · 12:15 20m ✗ (4 min after 12:11) · 12:22 20m ✓.

### 1.5 `call_exchange` — CSV file of known stations and their exchange

```json
"call_exchange": {
    "file": "UFT_mini_2021.csv",
    "fields": ["call", "nr", "name", "type"],
    "exchange_field": "nr",
    "url": "https://raw.githubusercontent.com/…/UFT_mini_2021.csv"
}
```

First column = callsign, `exchange_field` (default: second column) = the exchange that station
sends; any other columns are free. The file name is free (`call_exchange.csv` in SKM). qTLF uses it to:

* complete the Call field (column `call`) and the exchange field (column `exchange_field`), §1.3b;
* pre-fill the received exchange (`"prefill": "call_exchange"`), §1.3b;
* give **all the fields** of the record of each QSO to the script (`q.db.nr`, `q.db.name`…, §2);
* give **all the fields** of the record of the call being typed to the QML windows
  (`contest.callInfo.nr`, `contest.callInfo.name`…, §3).

Example `call_exchange.csv` (SKM, columns `call;sk`):

```
call;sk
F1XYZ;HA6NA
DL1ABC;YT7MM
```

### 1.6 `country_file` — cty.dat

```json
"country_file": { "file": "cty.dat", "url": "https://www.country-files.com/cty/cty.dat" }
```

The only non-CSV file: the universal [cty.dat](https://www.country-files.com/cty-dat-format/)
format (AD1C). Lookup: exact `=CALL` first, then longest prefix; `/P /M /QRP…` ignored,
`EA8/F8EHJ` and `F8EHJ/EA8` → EA8, `W1ABC/6` → W6, `/MM` `/AM` → no entity; zone `(#)` `[#]`
and continent `{XX}` overrides applied. Gives `q.cont`, `q.country`, `q.dxcc`, `q.cqz`,
`q.ituz`, `contest.my` to the script and `callInfo.cty`, `myCty` to QML.

If the file is missing (never downloaded, no local copy), qTLF still works but without country
data: `contest.ctyLoaded` is `false`, `q.cont`… are absent, cty pre-fill gives nothing. The status
bar shows a permanent red **cty.dat missing** (hover: reason; click: download again) until the
file is loaded. Once downloaded, the local copy is kept for offline use.

### 1.7 `remote_update` — data files from the network (GitHub…)

`call_exchange` and `country_file` may have a `url`.

* `on_open` (default): background download each time the contest is opened.
  `manual`: only with *Contest › Update data files from network* (Ctrl+U) or `:update`.
* Use **raw** URLs (`raw.githubusercontent.com/…`), not `github.com/…/blob/…` pages.
* The download replaces the local file only if valid (HTTP 200, text, not empty, < 5 MB). No
  network → the local copy is used (message in the status bar).
* If `rules/<ID>/` is read-only, the copy goes to the user data folder
  (`~/.local/share/qTLF/qTLF/rules/<ID>/`, `%APPDATA%\qTLF\qTLF\rules\<ID>\`) and the newest copy
  is used.
* Only plain file names (no `/`, no `..`) and only `http://` / `https://` URLs are accepted.

### 1.8 CSV file block (same for every data file except cty.dat)

| Key | Default | Meaning |
|---|---|---|
| `file` | required | Plain file name in the contest folder. |
| `fields` | `["call","exch"]` | Column names, in order (the first column is used when no field is named). |
| `separator` | `;` | Column separator (`,` is allowed). |
| `encoding` | `auto` | `auto` (UTF-8 if valid, else Windows-1252, the encoding of Excel files), `utf-8`, `windows-1252`, `latin1`. |
| `url` | — | Download address, §1.7. |

File rules: one record per line, blank lines and lines starting with `#` ignored, spaces around
values trimmed, CRLF accepted, UTF-8 BOM accepted. An optional first line equal to the field
names (`call;nr;name;type`) is a header and is skipped. Values are compared upper-case.

---

## 2. scoring.js — the score API

The main program calls `score(qsos, contest)` after every change of the log. The script runs in
a sandbox (no file, no network) and must be fast. `console.log()` prints to the terminal.

**Before** the call, qTLF has already checked every QSO (§5): `q.error` is `""` for a valid QSO,
else the error text. A script normally just does `if (q.error) return;` and counts points.

### Inputs

```js
// qsos: chronological array of
{ id, datetime: "2026-11-07T05:30:00Z", date: "2026-11-07", time: "05:30:00",
  band: "80m", freq: 3525, mode: "CW", call: "F6CYK",
  rst_s, rst_r, exch_s, exch_r, tagged, comment,
  error: "",                      // "" | "DUPE" | "OUT OF PERIOD" | "BAD EXCH" | …   (§5)
  cont, country, dxcc, cqz, ituz, // from country_file (absent without it)
  db: { call, nr, name, type } }  // call_exchange record, all its fields ({} if unknown)

// contest:
{ id, name, rules, rulesName, shortName, date: "2026-11-07", mycall, category, exch_sent,
  start, end,                     // first start / last end, ISO UTC
  periods: [{ start, end }],      // every operating window, ISO UTC, end exclusive
  fields: { "CATEGORY-POWER": "LOW", "CLUB": "UFT", … },   // Cabrillo fields (§4)
  my: { valid, cont, country, prefix, cqz, ituz },        // my call in cty.dat
  ctyLoaded: true, bands: ["80m", …], modes: ["CW"],
  band_change: { min_minutes: 10 } }
```

### Helpers (object `qtlf`, always available)

| Helper | Returns |
|---|---|
| `qtlf.isCallsign(s)` | `true` if `s` looks like a callsign (same test as qTLF). |
| `qtlf.stationCall(call)` | Call without the suffixes /P /M /MM /AM /QRP (`F5ABC/P` → `F5ABC`), the rule used by the qTLF dupe check. |
| `qtlf.baseCall(call)` | Longest part of the call, prefixes removed too (`EA8/F5ABC/P` → `F5ABC`). |
| `qtlf.perBand(contest, init)` | `{ "80m": init("80m"), … }` for every contest band. |
| `qtlf.count(obj)` | Number of keys of an object (e.g. multiplier sets). |

### Output — keys read by the main program

| Key | Used for |
|---|---|
| `total` | Final score: status bar *Score:*, Cabrillo `CLAIMED-SCORE`. |
| `points`, `multipliers` | Status bar *P pts x M mults* (or *P pts*). |
| `summary` | Replaces that status-bar text if present. |
| `status` | `{ <qso id>: "TEXT" }` contest-specific status for valid QSOs (generic errors always win; default `OK`; anything but `OK` is shown red). |
| *anything else* | Free: read by your QML windows as `contest.score.<key>` (e.g. `bands`, `continents`). |

A script error is shown in the status bar and in `contest.score.error`.

```js
function score(qsos, contest) {
    var perBand = qtlf.perBand(contest, function () { return 0; });
    var points = 0;
    qsos.forEach(function (q) {
        if (q.error) return;            // already checked by qTLF
        points++; perBand[q.band]++;
    });
    return { points: points, total: points,
             bands: contest.bands.map(function (b) { return { band: b, points: perBand[b] }; }) };
}
```

---

## 3. windows — custom QML docks

```json
"windows": [
    { "id": "score", "title": "UFT score", "qml": "windows/Score.qml", "area": "right", "visible": true }
]
```

`area`: `left` · `right` (default) · `top` · `bottom` · `float`. Each window is listed in the
Windows menu. The QML receives one object, **`contest`**:

| Property | Content |
|---|---|
| `contest.score` | Object returned by `score()`. |
| `contest.info` | The `contest` object given to `score()`. |
| `contest.qsos` | All QSOs (fields of §2 + `status`). |
| `contest.bandCounts` | `[{ band, qsos }]`. |
| `contest.qsoCount` | Number of QSOs. |
| `contest.currentBand` | Band selected on the entry line. |
| `contest.entryCall` | Call being typed (`""` if empty). |
| `contest.callInfo` | `{ call, known, <all call_exchange fields>, cty: { valid, cont, country, prefix, cqz, ituz }, workedBands: [...], dupe }` |
| `contest.myCty` | My call in cty.dat. |
| `contest.ctyLoaded` | Country file loaded. |
| signal `updated()` | After every recomputation. |
| signal `currentBandChanged()` | Band changed. |
| signal `entryChanged()` | While the call is typed. |

Use `import QtQuick` / `QtQuick.Layouts` and `SystemPalette` (light/dark theme). Docks can be
narrow (min. 220 px): keep layouts flexible.

---

## 4. cabrillo — fields defined by the contest, file written by qTLF

```json
"cabrillo": {
    "contest": "RENCONTRES-UFT",
    "fields": [
        { "tag": "CATEGORY-OPERATOR", "label": "Operator", "values": ["SINGLE-OP", "MULTI-OP"] },
        { "tag": "CATEGORY-POWER",    "label": "Power",    "values": ["HIGH", "LOW", "QRP"], "default": "LOW" },
        { "tag": "CLUB",              "label": "Club",     "values": ["UFT", "NM"] },
        { "tag": "OPERATORS",         "label": "Operators (multi-op)" }
    ],
    "header": { "CATEGORY-MODE": "CW" },
    "qso_template": "{freq:>5} {mode:2} {date} {time} {mycall:13} {rst_s:3} {exch_s:6} {call:13} {rst_r:3} {exch_r:6}"
}
```

* `contest` (default `short_name`): the `CONTEST:` line.
* `fields`: chosen by the operator in the contest creation dialog, editable at any time in
  *Configuration › Contest options…* (only these options: date, call, exchange sent and QSOs are
  never changed there) and confirmed at export. `values` = drop-down list, no `values` = free text, `default` = first value if absent.
  Stored with the contest; given to the script as `contest.fields` — a change is applied at once
  (checks, score, QML windows).
* `header`: fixed lines, always written (they override a field with the same tag).
* `qso_template`: tokens `{freq} {mode} {date} {time} {mycall} {rst_s} {exch_s} {call} {rst_r}
  {exch_r} {band}`; `{call:13}` pads left-aligned, `{freq:>5}` right-aligned.

The main program writes: `START-OF-LOG: 3.0`, `CREATED-BY`, `CONTEST`, `CALLSIGN`, the `fields`,
the `header`, `CLAIMED-SCORE` (= `total`), `GRID-LOCATOR`, `OPERATORS` (field if filled, else the
contest call), `NAME`, `ADDRESS` (one line per address line), `EMAIL` (from *Configuration ›
Station*), one `QSO:` line per QSO, `END-OF-LOG:`. Export: *Contest › Export Cabrillo*
(Ctrl+Shift+E).

---

## 5. Generic checks done by the main program

Each QSO gets at most one error (first one found), shown in the Status column and given to the
script as `q.error`:

| Error | Condition |
|---|---|
| `BAND NOT ALLOWED` | Band not in `bands`. |
| `MODE NOT ALLOWED` | Mode not in `modes`. |
| `OUT OF PERIOD` | Time outside every `schedule.periods` window. |
| `BAND CHANGE` | Band changed before `band_change.min_minutes` (§1.4b). |
| `BAD CALL` | Call does not look like a callsign. |
| `NO EXCH` | `received.required` and the exchange is empty. |
| `BAD EXCH` | Fails `received.type` (`number`, `callsign`) or `received.pattern`. |
| `DUPE` | Same station already worked according to `dupe`, /P /M /MM /AM /QRP ignored (only valid QSOs count as first contact). |

Everything else (points, multipliers, special stations, contest-specific statuses) belongs to
`scoring.js`.

---

## 6. Keys of older qTLF versions

| Old | Now |
|---|---|
| `"format_version": 2` | not needed (ignored) |
| `call_database` | `call_exchange` (same block; qTLF refuses the old name with a message) |
| `exchange.received.completion: { file … }` | `exchange.received.completion: true` (values come from the log and `call_exchange`) |
| `exchange.received.prefill: true / false` | `"call_exchange"` / `"none"` (the old values still work) |
| `period`, `duration_minutes`, top-level `periods` | `schedule.periods: [{ day, start_utc, end_utc, end_day }]` |
| `exchange_sent`, `exchange_received`, `rst` | `exchange.sent`, `exchange.received`, `exchange.rst` |
| `completion_file`, `lookup_file` | `call_exchange` |
| `cty_file`, `remote_files` | `country_file.file`, `url` inside each file block |
| `categories`, `cabrillo_fields` | `cabrillo.fields` |
| `scoring_script` | `scoring` |
