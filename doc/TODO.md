# OFM-NeoPixel — Engine TODO

Offene Ideen/Aufgaben der NeoPixel-Engine (Effekte, Rendering, Datenmodell).
Reine Engine-Sicht — Hardware-Anbindung und ETS/Konfiguration liefert die jeweilige Anwendung.

---

## Effekt-Texte: Text-Pool + Platzhalter  ·  Prio: HOCH

Die aktuelle Effekt-Text-Implementierung gefällt nicht und wird überarbeitet.

**Problem:**

- Pro Cue ein 14-Zeichen-Textfeld stört (`CUE_PPP+17`, +34): die Firmware lädt es, aber das ETS-Feld
  ist **gar nicht eingeblendet** → 14 B × 10 Cues = **140 B/EM totes Flash**, fix kurz, unflexibel.
- Pro-Effekt/-Cue Platz vorzureservieren skaliert schlecht.

**Bessere Strategie (Ziel):** **zentraler Text-Pool + Platzhalter** statt per-Cue-Reservierung.

- Texte **beliebig lang**, einmal definiert, in mehreren Cues/Effekten per Index wiederverwendbar.
- **Platzhalter/Variablen**: `{Datum}`, `{Uhrzeit}`, `{KO:n}` (Text-/Wert-KOs), Formatierung →
  dynamischer Inhalt ohne Neuprogrammierung.

**Sofort-Lösung (heute schon, ohne neue Firmware):**

- **Logikmodul → KO „Effekt-Text" (DPT 16.001, bis 240 Zeichen)**: das Logikmodul setzt Texte
  zusammen (Datum/Uhrzeit, KO-Werte, Format) und sendet sie an NeoPixel. Beliebig lang + dynamisch.
- → für dynamische Texte braucht es den internen Pool gar nicht; nur verdrahten + dokumentieren.

**Ausbaustufe (Engine-Feature, geräte-lokal ohne Logikmodul):**

- Engine: Text-Slot-Lookup + kleiner **Platzhalter-Resolver** (`{date}`/`{time}`/`{ko:n}`) +
  Quellen-Bindung (KNX-Zeit/Datum-KO, Text-KOs). Überschneidet sich mit dem Logikmodul-Textsystem
  → dort vorhandene Logik wiederverwenden statt doppeln.
- ETS/Anwendung (OAM): Text-Listen-Block (z. B. 8–16 Slots, je ~60–100 Zeichen), Effekt referenziert per Index.

**Hinweis:** Das tote per-Cue-Feld (`CUE_PPP+17`) **vorerst reserviert lassen** (für späteren EM-Ausbau),
nicht entfernen — erst beim Umbau des Text-Systems sauber mit auflösen.

## Sound-reaktive Effekte (Audio-Reactive)  ·  Prio: niedrig · Aufwand: mittel-groß · SPÄTER

Effekte, die auf Audio reagieren (VU-Meter, Spektrum/Bänder, Beat-Puls, Energie-Glow …) —
analog zu **WLED** (dort der „AudioReactive"-Usermod: I2S/PDM-Mic → FFT → Lautstärke + Bänder).
WLED ist Open Source (MIT) → Konzepte/Algorithmen als Referenz nutzbar; eigene, saubere
Implementierung in der Engine, ggf. besser/zugeschnitten.

**Auslöser:** Manche Boards haben ein **PDM-Mikrofon** (z.B. Gledopto GL-C-017WL: CLK/DATA).

**Architektur (Engine-Schnitt):**

- Audio-**Erfassung ist HAL/Board-spezifisch** (PDM-Mic via I2S auf ESP32-Klasse) → NICHT Aufgabe
  der Engine. Die Engine konsumiert eine abstrakte **Audio-Feature-Struktur**.
- Vorschlag Audio-Feature-Input (vom Board befüllt, z.B. ~50 Hz):
  - `level` (0..255, geglättete Lautstärke / „Volume")
  - optional `bands[N]` (FFT-Frequenzbänder, z.B. 8–16) für Spektrum-Effekte
  - optional `beat`-Flag / `bpmEstimate`
- Neue Effekt-Kategorie **„audio-reactive"**: lesen den Feature-Input statt (oder zusätzlich zu)
  Zeit/Speed. Beispiele: VU-Bar, Spektrum-Balken, Beat-Flash, Energie-abhängige Helligkeit.
- **Board-Gating:** nur auf Hardware mit Mic verfügbar; ohne Audio-Quelle liefern die Effekte
  einen sinnvollen Fallback (statisch/0).

**Offene Punkte:**

- Echtzeit-FFT-Last auf ESP32 bewerten (arduinoFFT o.ä.), RP2040-Boards haben i.d.R. kein Mic.
- Feature-Struktur + Update-Pfad definieren (wer ruft wann, Glättung, Normalisierung).
- Mapping Audio→Farbe/Helligkeit/Geometrie (1D + 2D-Matrix).
- Lizenz-/Herkunft sauber halten, falls Algorithmen an WLED angelehnt werden.

## Dynamisches Laden von Effekten ("Effekt-Slot" / Online-Effekte)  ·  Prio: Vision · Aufwand: SEHR groß · SPÄTER

Idee: Effekte zur Laufzeit laden/aktualisieren, statt fest einkompiliert — ein "Upload-Slot"
oder Online-Bezug. **Großer Architektur-Eingriff**, da Effekte aktuell C++-Klassen sind
(`EffectPool`/`Effect`-Interface, statisch registriert).

Optionen (grob, von einfach→hart):

- **Slot mit Parametern statt Code:** ein generischer "Custom"-Effekt mit datengetriebenen
  Parametern/Kurven (kein neuer Code, nur Werte) — klein, aber begrenzt.
- **Mini-Skript/Bytecode-VM:** kleine Skriptsprache/Bytecode für Pixelberechnung, zur Laufzeit
  ladbar (Flash/KO/Datei). Mittel-groß, RAM/Performance auf RP2040/ESP32 bewerten.
- **Echte Plugins:** native Module nachladen — auf MCU praktisch kaum machbar.

Voraussetzung: Effekt-Engine müsste eine **stabile, datengetriebene Effekt-Schnittstelle**
bekommen. Erst Konzept, dann bewerten ob Aufwand/Nutzen passt.

## 2D-Effekt-Parität (WLED-inspiriert)  ·  Prio: niedrig · Aufwand: pro Effekt klein-mittel

Vorhanden (12, teils einzigartig ggü. WLED): Clock/Uhr, Cylon, Fire/Feuer, Game of Life,
Matrix, Noise, PlasmaNebula, Snake, StarfieldWarp, Tetris (mit KI), TRON, UFO-Swarm.

Kandidaten als **eigene, saubere Neuimplementierung** (Konzepte von WLED als Referenz, kein
Code-Copy — Lizenz sauber halten; passt zudem besser auf unser Segment/Topologie-Modell):
DNA/DNA-Spiral, Polar Lights/Aurora, Lissajous, Octopus/Metaballs (organische
Blobs), Drift/Colored Bursts. Audio-reaktive 2D (GEQ/Spektrum) hängt am Audio-TODO oben.

**Pilot „Game of Life 2D" erledigt** (Effekt-ID 41, append-only; B3/S23 toroidal, Auto-Re-Seed
bei Aussterben/Stillstand, 3 Farbmodi; Generator/Producer/Compile grün). Bestätigt damit das
Muster: 1 Effekt = `*2DEffect.h` (am Snake-Effekt spiegeln) + EffectPool-Append +
`Build-EffectParameters.ps1` (regeneriert Mapping/Param-XML/Help, hängt Lock an). Wichtig:
Min/Max je Index **explizit** als `case` angeben (Generator parst keine `default:`-Fallthroughs);
der Generator **entdeckt Effekte durch Scannen von `src/effects/*2DEffect.h`** (nicht über EffectPool).

### ⛔ BLOCKER: Effekt-Parameter-ID-Raum ist voll (Layout-Entscheidung nötig)

Die Effekt-Union-Param-IDs (`UP-…NNN`) laufen sequentiell und enden nach GoL bei **239**. IDs
**240–254 sind bereits von Kern-Segment-Parametern belegt** (HCLMode, SegmentTopology,
MatrixWidth/Height/Depth, SyncMode, VirtualOffset, StartupEM, SceneCount …). **GoL hat die letzten
4 freien Slots (236–239) exakt gefüllt** → jeder weitere Effekt kollidiert („Duplicate Parameter
240…"). Der Producer bricht ab.

**Damit ist GoL der letzte Effekt, der ohne Layout-Änderung passte.** Die 4 nächsten Entwürfe
(DNA, Aurora, Lissajous, Metaballs — fertig in `doc/pending-2d-effects/`, am GoL-Muster gespiegelt,
aber **noch nicht compile-verifiziert**) brauchen zuerst eine **Erweiterung des Param-ID-Raums**:

- Option A: Effekt-Union-Param-IDs auf eine **freie ID-Basis oberhalb** der Segment/Scene/Cue/Relais-
  Params verschieben (Generator-Anpassung). Wenn die **Flash-Offsets gleich bleiben**, evtl. ohne
  Reprogramm — **prüfen!**
- Option B: Segment-Param-IDs nach oben schieben (Layout-Change → ApplicationVersion-Bump + Reprogramm).

→ **Bewusste Layout-Entscheidung des Eigentümers** (Append-only-Constraint). Erst entscheiden,
dann die 4 Entwürfe aus `doc/pending-2d-effects/` zurück nach `src/effects/` + EffectPool-Append.

**✅ UPDATE (2026-06): GELÖST + 4 Effekte drin.** Effekt-Param-ID-Lane auf **600** (Marker in
`Segment.templ.xml`), Segment-**Union 584→704 B** (`SizeInBit=5632`) für Offset-/Spillover-Budget.
DNA(42)/Aurora(43)/Lissajous(44)/Metaballs(45) registriert, Producer + Compile grün, Lane-Reserve
für ~50+ Effekte. Append-only ab hier (IDs nie verschieben). Achtung: Union-Bump = Layout-Migration
(Offsets wandern → ETS-Re-Import; per Producer-Regel kompatibel, RefIds/KO-Nummern bleiben).

## Formula2D — Per-Pixel-Formel-Effekt („Shader-Slot")  ·  Prio: Vision · Aufwand: mittel-groß

**Idee (Eigentümer):** EIN Effekt mit Formel-Slot statt vieler hartcodierter Mathe-Effekte —
`Farbe/Helligkeit = f(x, y, t)`, Formel zur Laufzeit ausgewertet, ohne neu zu flashen.

- **Deckt ab:** die rein gerechnete Familie (Plasma, Wellen, Ringe, Verläufe, Lissajous, Ripples …).
- **Ersetzt NICHT:** zustandsbehaftete Effekte (Game of Life, Snake, Tetris, Metaballs, Feuer) —
  die brauchen Frame-zu-Frame-State, keine reine `f(x,y,t)`. Also **Ergänzung, nicht Ersatz**.
- **Umsetzung:** **TinyExpr wiederverwenden** (etablierte MIT-Lib, die das OFM-LogicModul schon nutzt —
  vendored in `OFM-LogicModule/src/tinyexpr.{c,h}`; NICHT selbst parsen!). `te_compile` **einmal** bei
  Formel-Änderung → `te_eval` pro Pixel; Vars `x,y,t,w,h` binden (statt e1/e2/a). Eingabe: ETS-Langtext
  (~99 Zeichen, `\n`-Trick wie LogicModul). Reiner Ausdruck → sandboxed; NaN/Fehler → Pixel aus.
- **⚠️ Lib-Hürde:** TinyExpr ist im LogicModul vendored. Eine **zweite Kopie in OFM-NeoPixel = doppelte
Symbole (Link-Fehler)**, da beide Module in dieselbe Firmware linken; direkter Zugriff aufs LogicModul
  verletzt OAM/OFM-Trennung. → **Sauber: TinyExpr in eine gemeinsame Lib auslagern** (LogicModul + NeoPixel
  teilen sich eine Kopie). **Mit Waldemar/OpenKNX abstimmen** (anti-duplizierung, kommt allen zugute).
- **⚠️ RP-Performance (harte Vorgabe):** `double`/Pixel ohne HW-double → Caps (Matrixgröße, FPS, nur
  Zeitschritt rechnen), evtl. Fixed-Point-Fallback. ESP32 unkritisch.

## Wert-gesteuerte Effekte (Variante B) — „Pegel/Wert"-Effekt  ·  Prio: mittel · GEPARKT

Abgrenzung zu Formula2D (A): **ein KO trägt EINEN Wert, kein per-Pixel-x/y.** Also kein Muster, aber:
ein Live-Wert (vom **LogicModul-Formel-KO** oder Sensoren — Wetter/Zisterne/CO2 …) treibt die LEDs.

- **Geht schon HEUTE ohne neuen Effekt:** Wert → Segment-**Helligkeits-KO** bzw. **Farb/RGB-KO** (nur Verdrahtung).
- **Neuer Effekt rechtfertigt sich für:** `Pegel/Wert` mit **Eingangs-KO** (DPT 5.001 % o.ä.) + Anzeige-Modi
  **Balken / Farb-Zonen (Schwellen grün-gelb-rot) / Hue-Mapping / Gauge-Zeiger / Zahl-Anzeige** + Min/Max-Skalierung.
  Deckt Zisterne (Füllstand), Temperatur (Hue/Zahl), jeden Sensorwert ab. **RP-sicher** (ein Wert, kein per-Pixel).
- Ist zugleich die **Basis für BusEqualizer** (Aktivität = Wert → Pegel) und nutzt die LogicModul-Synergie voll.
- **Impl.-Hinweis:** neuer Eingangs-KO am Segment → verschiebt KO-Nummern (Beta/Re-Import: ok).
- **Entscheidung Eigentümer:** verworfen für jetzt, Idee als Check für später behalten.

## BusEqualizer2D — KNX-Bus-Aktivität als Equalizer  ·  Prio: Vision (MEGA) · Aufwand: klein-mittel

**Idee (Eigentümer):** Bus-Telegramm-Aktivität visualisieren (VU/scrollendes Spektrum auf der Matrix).

- **Variante A (app-only, sofort):** Telegramme/Zeitfenster über `processInputKo(GroupObject&)` zählen
  (+ ausgehende Writes) → Aktivität der **eigenen GAs**. Kein Stack-Eingriff. **Damit starten.**
- **Variante B (ganzer Bus):** Hook/Zähler an `DataLinkLayer::frameReceived(CemiFrame&)`
  (`lib/knx/src/knx/data_link_layer.h`) — sieht jedes Frame vor Adressfilterung. Kleine knx-Lib-
  Änderung (ggf. upstream), oder cEMI-/Monitor-Pfad falls als IP/USB-Interface aktiv.
- **Architektur:** OAM füllt ein „Bus-Aktivitäts-Feature" (Level + Historie), Engine-Effekt rendert —
  analog zum sound-reaktiven Feature-Input oben. Darstellung: VU-Bar / scrollende Spalten / Bänder
  nach GA-Hauptgruppe·DPT·Quell-Bereich.

## Performance / max. FPS herausholen  ·  Prio: mittel · Aufwand: pipelining ~1 Tag, Rest variabel

**Ziel:** Das technisch machbare Maximum an Bildrate herausholen (nicht nur 20–30 FPS).
**Aktueller Stand (Messung):** Frame ≈ **CPU (Compute+Sync) + Wire (LED-Datenrate)** *seriell*.
Bei 512 LEDs @ ~960 kHz sind das ~13 ms Wire + ~4 ms CPU ≈ 17 ms/Frame → ~58 FPS gemessen.
Beweis dass **nicht** Compute-bound: Solid-Effekt im FTL läuft ~gleich schnell wie animierte Effekte
→ Dual-Core/Overclock bringen aktuell nichts (und Overclock desynct PIO/UART/Flash-Timing → riskant).
Der parallele DMA-Push (alle Strips starten, dann alle abwarten) ist bereits drin (~35→58 FPS, ~2×).

1. **Pipelining (Frame N+1 rechnen während Frame N per DMA rausgeht)** — der eigentliche Software-Hebel.
   
   - Frame ≈ **max(Compute, Wire)** statt Summe → Richtung Wire-Decke ~70–76 FPS (512-LED-Strip @960 kHz).
   - **Buffer-Sicherheit ist schon gegeben:** der Treiber sendet aus einem *separaten Snapshot*
     (`packDataToDMABuffer` RGB/RGBW bzw. `bufferSending`-memcpy RGBCCT) → Arbeitspuffer ist während
     der Übertragung frei und darf neu berechnet werden.
   - **Kern-Änderung klein** (~10–20 Z.): den End-of-Frame-Wait in `NeoPixelManager::showAll()` entfernen
     und auf das Start-of-Frame `while(isBusy())` in `show()` verlassen.
   - **Mitzuziehen:** Perf-Reporting auf **Frame-zu-Frame-Delta** umstellen (nicht mehr Compute-Zeit messen,
     da Compute jetzt unter dem Wire versteckt ist); Edge-Cases bei `stop`/`clear`/Strip-Remove
     (laufenden DMA sauber abwarten/abbrechen bevor Buffer/Strip weg ist).
   - **Aufwand:** Kern klein, Test ist die eigentliche Arbeit (~1 Tag, mittleres Risiko).
2. **Danach: Compute wird der nächste Engpass** (erst wenn Pipelining den Wire versteckt, und nur bei
   schweren 2D-Effekten Compute > Wire) → dann lohnt sich:
   
   - **Dual-Core-Rendering (Core1):** Compute auf den zweiten M33 auslagern, Core0 fährt DMA/Bus.
   - **Hot-Path-Tuning in 2D-Effekten:** Fixpunkt statt float, vorberechnete Tabellen (sin/noise),
     weniger Per-Pixel-trig/-noise-Aufrufe.
3. **Wire-Decke senken (harte Grenze):** lange Strips auf mehr GPIO-Pins / kürzere Parallel-Strips
   aufteilen → kürzere Wire-Zeit pro Strip → höhere Decke. (Verdrahtung + Matrix über mehrere
   Physical-Strips spannen.)
4. **Sync/PowerLimit-CPU senken (~4 ms/Frame):** `syncAll` + `applyPowerLimit` zu einem Pass
   zusammenziehen, Fixpunkt-Helligkeitsskalierung, unveränderte Virtual-Buffer überspringen.

