# OFM-NeoPixel — Effects Reference

This reference describes all 33 available light effects in plain language.
It is aimed at end users without programming knowledge. For each effect you get:

- **What you see** — the visual result in one or two sentences.
- **Dimension** — whether the effect runs on a strip (1D), on a matrix (2D),
  or on both.
- **Options** — every adjustable value with an explanation. For selection fields
  (dropdowns), each choice is explained in plain text.

Notes:

- **Speed** always means: low value = slow, high value = fast.
- **Hue** is a color value from 0–255 (0 = red, approx. 85 = green, approx. 170 = blue).
- **Brightness / Intensity** controls how brightly the effect glows.
- A **switch** (On/Off) turns a function on or off.
- Most effects use the base color configured in the segment; if it is
  black (all channels 0), many effects automatically produce rainbow colors.

---

## 0 – Solid

**What you see:** A continuously lit surface in the configured color — calm, static light with no movement.
**Dimension:** 1D and 2D.

No additional options. Color and brightness are set via the general segment settings.

---

## 1 – Wipe

**What you see:** A color wipes across the strip and fills it step by step — like a brush stroke from one side to the other.
**Dimension:** 1D and 2D.

| Option | Meaning |
|--------|---------|
| Speed | How fast the wipe runs. |
| Direction | Which direction the wipe comes from: **Left to right**, **Right to left**, **Top to bottom**, **Bottom to top**, **Center to edges** or **Edges to center**. |

---

## 2 – Rainbow

**What you see:** A flowing rainbow that travels across the strip.
**Dimension:** 1D and 2D.

| Option | Meaning |
|--------|---------|
| Speed | How fast the rainbow travels. |
| Delta | Color distance between the LEDs (only in "Rainbow" mode). Higher value = more colors visible at once. |
| Saturation | Color intensity (0 = white/pale, 255 = vivid colors). |
| Density | Number of complete rainbows across the strip (only in "Cycle" mode, 1–10). |
| Mode | **Rainbow (Delta)** — color distance is controlled via "Delta". **Cycle (Density)** — fixed number of rainbows via "Density". |

---

## 3 – Pride2015

**What you see:** Softly undulating, rich rainbow colors that slowly shift in brightness and hue — very lively and colorful.
**Dimension:** 1D.

| Option | Meaning |
|--------|---------|
| Speed | How fast the color waves move. |

---

## 4 – Juggle

**What you see:** Several colored light dots swing back and forth and cross over one another, as if balls were being juggled.
**Dimension:** 1D.

| Option | Meaning |
|--------|---------|
| Speed | How fast the dots swing. |
| Dot count | How many juggling dots are shown (1–16). |
| Fade rate | How fast the trails of the dots fade. |
| Hue offset | Shifts the colors of all dots. |

---

## 5 – BPM

**What you see:** A pulsing color pattern that beats in time — like a music visualization with a beat.
**Dimension:** 1D.

| Option | Meaning |
|--------|---------|
| BPM | Speed in "beats per minute" — how fast the pulsing is. |
| Hue | Base color of the pulsing wave. |

---

## 6 – Cylon

**What you see:** A glowing eye travels back and forth, dragging a fading trail behind it — the classic "KITT/Cylon" effect.
**Dimension:** 1D and 2D.

| Option | Meaning |
|--------|---------|
| Speed | How fast the eye travels (0 = automatic). |
| Hue | Color of the eye. |
| Eye size | How many LEDs wide the eye is (1–20). |
| Trail speed | How fast the trail fades (low = long trail, high = short trail). |
| Direction (2D only) | **Horizontal** — the eye travels left/right. **Vertical** — it travels up/down. |
| Mode | **Larson** — classic back-and-forth. **Center to edges** — two mirrored eyes run from the center outward and back (gate effect). |

---

## 7 – Test (Channel test)

**What you see:** An automatic run-through of individual colors and mixes to check whether all color channels of the strip work correctly.
**Dimension:** 1D.

| Option | Meaning |
|--------|---------|
| Phase duration | How long each test phase is shown (in seconds, 1–60). |
| Mode | **RGBW** — 8-phase test for 4-channel strips (red, green, blue, white). **RGB+CCT** — 10-phase test for 5-channel strips (red, green, blue, warm white, cool white). |

---

## 8 – Fire

**What you see:** A realistically flickering fire with rising, glowing flames.
**Dimension:** 1D and 2D.

| Option | Meaning |
|--------|---------|
| Speed | Animation speed. |
| Cooling rate | How fast the fire cools down (higher = shorter flames). |
| Spark rate | How often new embers appear (higher = livelier fire). |
| Reverse direction | Switch: makes the fire burn downward instead of upward. |
| Blue fire mode | Switch: blue fire instead of orange. |

---

## 9 – Theater Chase

**What you see:** Running light dots with even spacing travel across the strip — like the chase lights on an old cinema marquee.
**Dimension:** 1D.

| Option | Meaning |
|--------|---------|
| Speed | How fast the dots run. |
| Spacing | Distance between the lit dots (1–10). |
| Dot size | How wide each light dot is (1–5). |
| Color mode | **Solid** — fixed color. **Rainbow** — the colors cycle through. |
| Color change speed | How fast the colors change in rainbow mode (1–20). |
| Bounce | **Loop** — the dots run around continuously. **Back and forth** — they swing back and forth (runway). |

---

## 10 – Sparkle

**What you see:** Randomly flashing sparks — depending on the mode, party-style flashing, gently twinkling like stars, or colorful confetti.
**Dimension:** 1D.

| Option | Meaning |
|--------|---------|
| Speed | Base tempo, i.e. how often new sparks appear. |
| Fade rate | How fast the sparks fade (for confetti, also the saturation). |
| Count/Density | Number of sparks (Sparkle) or density (Twinkle). |
| Probability | How likely a spark appears (50–200). |
| White only | Switch: white sparks only (Sparkle) or rainbow colors (Twinkle). |
| Burst | Switch: burst-style flashing (Sparkle) or random brightness (Twinkle). |
| Mode | **Sparkle** — fast, random sparks. **Twinkle** — softly fading-in-and-out stars. **Confetti** — colorful color dots that slowly fade. |

---

## 11 – Breathing

**What you see:** The light gently breathes in and out — it slowly brightens and dims again.
**Dimension:** 1D.

| Option | Meaning |
|--------|---------|
| Speed | How fast the breathing runs. |
| Min. brightness / Pulse width | Darkest point during breathing, or width of the pulse (depending on the waveform). |
| Curve / Gamma | How soft or hard the transition is (0 = linear, 255 = strongly emphasized). |
| Rainbow | Switch: the color cycles through during breathing. |
| Waveform | **Soft** — smooth inhale/exhale. **Hold at peak** — short pause at the brightest moment. **Pulse** — strong light pulse. **Sharp pulse** — pulse with a hard, abrupt onset. |

---

## 12 – Strobe

**What you see:** Fast, hard flashes of light like a strobe at a party.
**Dimension:** 1D.

| Option | Meaning |
|--------|---------|
| Speed | How fast it flashes. |
| On-time ratio | How long the light is on per flash (10–200 %). |
| Minimum brightness | Residual light during the off phases (0–100). |
| Random timing | Switch: irregular instead of even flashes. |
| Rainbow strobe | Switch: the flash color cycles through. |

---

## 13 – Comet

**What you see:** A moving light dot with a trail — depending on the mode, a comet, a meteor shower, or a gliding sine-wave dot.
**Dimension:** 1D.

| Option | Meaning |
|--------|---------|
| Speed | How fast the dot moves. |
| Fade rate / Meteor size | How fast the trail fades, or how large a meteor is. |
| Trail length / Dot size / Frequency | Trail length (Comet), dot size (Sinelon), or meteor frequency (Meteor). |
| Bounce / Random colors / Rainbow | Bounce mode (Comet), random colors (Meteor), or rainbow (Sinelon). |
| Rainbow / Multi-meteor / Bounce | Rainbow (Comet), multiple meteors (Meteor), or bounce mode (Sinelon). |
| Mode | **Comet** — a dot with a fading trail. **Meteor** — falling meteors. **Sinelon** — a dot that swings back and forth in a sine pattern. |

---

## 14 – Noise

**What you see:** Softly flowing, organic color gradients (Perlin noise) that slowly and irregularly change.
**Dimension:** 1D and 2D.

| Option | Meaning |
|--------|---------|
| Speed | How fast the pattern changes over time. |
| Scale | How fine or coarse the noise pattern is. |
| Saturation | Color intensity. |
| Hue offset | Shifts the base color. |
| Color mode | **HSV color wheel** — full color palette from the noise. **Primary color** — only the configured base color, whose brightness comes from the noise. |

---

## 15 – Palette

**What you see:** Smoothly scrolling color gradients from one of four predefined color palettes.
**Dimension:** 1D.

| Option | Meaning |
|--------|---------|
| Speed | How fast the colors scroll. |
| Palette | Choice of color palette: **Rainbow**, **Heat** (black–red–yellow–white), **Ocean** (blue/turquoise tones), or **Forest** (green tones). |
| Blending | Switch: smooth color gradient (on) or hard color steps (off). |
| Spacing | Color distance between the LEDs (0 = automatic). |

---

## 16 – Lightning

**What you see:** Random lightning strikes with short, bright flashes and decay — like a thunderstorm.
**Dimension:** 1D.

| Option | Meaning |
|--------|---------|
| Speed | Frequency of the strikes (low = rare and dramatic, high = very frequent). |
| Width | Extent of a strike in LEDs (0 = automatic). |
| Decay time | How fast a strike fades again. |
| Hue | Color of the strikes. |
| Intensity | Maximum brightness of the strikes. |

---

## 17 – Gradient

**What you see:** A smooth color gradient from a start color to an end color across the strip, slowly moving.
**Dimension:** 1D.

| Option | Meaning |
|--------|---------|
| Speed | How fast the gradient moves. |
| Start hue | Color at the beginning of the gradient. |
| End hue | Color at the end of the gradient. |
| Saturation | Color intensity. |

---

## 18 – Candle

**What you see:** Warmly flickering candlelight. With one zone everything flickers together; with multiple zones several independent candle flames are created.
**Dimension:** 1D.

| Option | Meaning |
|--------|---------|
| Speed | How fast the flicker is (calm to nervous). |
| Intensity | How strongly the flicker swings (calm to wild). |
| Zones | Number of independent flames (1 = shared flicker, higher = multiple candles). |

---

## 19 – Scroll Text

**What you see:** A scrolling text that scrolls across the matrix.
**Dimension:** 2D.

| Option | Meaning |
|--------|---------|
| Speed | How fast the text scrolls. |
| Gap | Distance between the characters (in pixels). |
| Loop | Switch: play text once (off) or repeat endlessly (on). |
| Text | The text to display (up to 14 characters in ETS, up to 240 via communication object). |
| Font | Character size: **5x7** (default), **4x6** or **3x5** (smallest). |

---

## 20 – Clock 2D

**What you see:** A clock on the matrix — as digital digits or as a binary representation, optionally with the date.
**Dimension:** 2D.

| Option | Meaning |
|--------|---------|
| Display mode | **Digital** — digits. **Binary BCD columns** — binary columns. **Binary rows** — binary rows. **Auto-switch (BCD)** — alternates digital/binary (columns). **Auto-switch (rows)** — alternates digital/binary (rows). |
| Show seconds | Switch: time as HH:MM (off) or HH:MM:SS (on). |
| Blink colon | Switch: colon permanently on (off) or blinking every second (on). |
| Time hue | Color of the digits. |
| Date display | **Off** — no date. **Time/date alternation** — alternates between time and date. **Date only** — shows only the date. |
| Date format | **DD.MM.** , **DD.MM.YY** or **WD DD.MM.** (with weekday). |
| Date hue | Separate color for the date (0 = same color as the time). |
| Alternation interval | How often the display alternates (2–30 seconds). |
| Scroll speed | Tempo when the content is wider than the matrix (0 = static). |
| Font | Character size: **5x7**, **4x6** or **3x5**. |

---

## 21 – Snake 2D

**What you see:** The classic Snake game runs on its own on the matrix: the snake eats food and grows.
**Dimension:** 2D.

| Option | Meaning |
|--------|---------|
| Speed | How fast the snake moves. |
| Head hue | Color of the snake's head. |
| Body mode | **Solid** — fixed body hue. **Rainbow** — the body shows a rainbow. **Gradient** — color gradient along the body. |
| Body hue | Color of the body (for "Solid"/"Gradient"). |

---

## 22 – Matrix 2D

**What you see:** Falling "digital rain" like in the movie Matrix — columns of glowing characters trickling downward.
**Dimension:** 2D.

| Option | Meaning |
|--------|---------|
| Speed | How fast the rain falls. |
| Density | How many columns are active at the same time (as a percentage). |
| Palette | Color of the rain: **Green** (classic), **Gold** or **Mixed**. |
| Glitch | Switch: occasional white interference flashes. |

---

## 23 – Tetris 2D

**What you see:** The classic Tetris runs on the matrix — falling pieces, full rows are cleared. On request, the game plays itself.
**Dimension:** 2D.

| Option | Meaning |
|--------|---------|
| Speed | Falling speed of the pieces (low = slow, high = fast). |
| Background brightness | Brightness of the background grid (0 = off, max. 64). |
| Ghost piece | Switch: shows where the piece would land. |
| Color mode | **Shape color** — each piece shape has its own color. **Random** — random colors. **Rainbow** — rainbow colors. |
| Flash rows | Switch: full rows flash before being cleared. |
| Self-play (Auto) | **Off (manual)** — no self-play. **Beginner**, **Normal** or **Pro** — the built-in AI plays by itself, with increasing skill. |

---

## 24 – TRON

**What you see:** A neon-colored grid with moving scan lines in TRON style.
**Dimension:** 2D.

| Option | Meaning |
|--------|---------|
| Speed | Tempo of the scan lines. |
| Hue | Neon color. |
| Grid spacing | Distance between the grid lines (2–12). |
| Glow | Strength of the glow effect. |

---

## 25 – Starfield Warp

**What you see:** A starfield flying toward the viewer — like a warp tunnel in space.
**Dimension:** 2D.

| Option | Meaning |
|--------|---------|
| Speed | Warp speed of the stars. |
| Density | Number of stars (as a percentage, 10–100). |
| Color mode | **White**, **Cyan** or **Rainbow**. |
| Warp pulse | Switch: periodic speed boost. |

---

## 26 – Plasma Nebula

**What you see:** A multi-layered, undulating plasma nebula animation with flowing colors.
**Dimension:** 2D.

| Option | Meaning |
|--------|---------|
| Speed | Tempo of the nebula movement. |
| Saturation | Color intensity (pale to vivid). |
| Contrast | Brightness difference between the nebula layers. |
| Palette shift | Shifts the hue of the palette. |

---

## 27 – UFO Swarm

**What you see:** Several UFOs fly across the matrix, optionally with a tractor beam.
**Dimension:** 2D.

| Option | Meaning |
|--------|---------|
| Speed | Flight speed of the UFOs. |
| Count | Number of UFOs in the swarm (1–8). |
| Hue | Color of the UFO lights. |
| Beam | Switch: show tractor beam (on) or hide it (off). |

---

## 28 – Game of Life 2D

**What you see:** Conway's "Game of Life" — cells are born, survive and die according to fixed rules. If the pattern dies out or freezes, it restarts automatically.
**Dimension:** 2D.

| Option | Meaning |
|--------|---------|
| Speed | Tempo of the generations. |
| Hue | Base color of the living cells. |
| Color mode | **Solid** — a single color. **Age heat** — color changes the longer a cell lives. **Position rainbow** — rainbow based on position. |
| Start density | How full the initial population is (as a percentage, 5–95). |

---

## 29 – DNA 2D

**What you see:** A rotating DNA double helix: two counter-running wave strands with cross connections (rungs).
**Dimension:** 2D.

| Option | Meaning |
|--------|---------|
| Speed | Rotation speed of the helix. |
| Hue | Base color of strand A (strand B is the complementary color). |
| Turns | Number of helix turns across the width (1–32). |
| Rung spacing | One rung every N columns (0 = no rungs). |

---

## 30 – Aurora 2D

**What you see:** Softly flowing curtains of light like an aurora (northern lights).
**Dimension:** 2D.

| Option | Meaning |
|--------|---------|
| Speed | Drift speed of the light curtains. |
| Hue | Base color (96 = green, the typical aurora). |
| Detail depth | Spatial detail depth / wave density (1–64). |
| Intensity | Maximum brightness. |

---

## 31 – Lissajous 2D

**What you see:** An animated Lissajous figure (oscillation pattern) with drifting phase and an afterglow trail.
**Dimension:** 2D.

| Option | Meaning |
|--------|---------|
| Speed | Drift speed of the phase. |
| Hue | Base color of the curve. |
| Horizontal frequency | Horizontal oscillation frequency (1–16). |
| Vertical frequency | Vertical oscillation frequency (1–16). |

---

## 32 – Metaballs 2D

**What you see:** Organic, droplet-like "blobs" that drift across the matrix, merging and splitting as they approach each other — like a lava lamp.
**Dimension:** 2D.

| Option | Meaning |
|--------|---------|
| Speed | Movement speed of the blobs. |
| Hue | Base color. |
| Blob count | Number of blobs (2–6). |
| Contrast | Field contrast / edge sharpness of the blobs. |

---

*Status: automatically generated from the effect descriptions. In case of
discrepancies, the settings in the ETS apply.*
