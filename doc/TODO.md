# OFM-NeoPixel — Engine TODO

Offene Ideen/Aufgaben der NeoPixel-Engine (Effekte, Rendering, Datenmodell).
Reine Engine-Sicht — Hardware-Anbindung und ETS/Konfiguration liefert die jeweilige Anwendung.

---

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
Vorhanden (11, teils einzigartig ggü. WLED): Clock/Uhr, Cylon, Fire/Feuer, Matrix, Noise,
PlasmaNebula, Snake, StarfieldWarp, Tetris (mit KI), TRON, UFO-Swarm.

Kandidaten als **eigene, saubere Neuimplementierung** (Konzepte von WLED als Referenz, kein
Code-Copy — Lizenz sauber halten; passt zudem besser auf unser Segment/Topologie-Modell):
Game of Life, DNA/DNA-Spiral, Polar Lights/Aurora, Lissajous, Octopus/Metaballs (organische
Blobs), Drift/Colored Bursts. Audio-reaktive 2D (GEQ/Spektrum) hängt am Audio-TODO oben.

**Vorgehen:** Erst **Pilot „Game of Life 2D"** end-to-end (Effekt-Klasse → EffectPool →
`EffectIds.lock`-Append → Regen → compile+producer-grün → erscheint in ETS), Review; passt's,
dann Rest **einzeln als Background-Job** (jeweils verifiziert). Neue Effekt-IDs = append-only
(layout-sicher). **Nach** dem aktuellen Release angehen, nicht vermischen.
