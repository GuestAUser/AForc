# AForc Design System

## 0. Asset Provenance

- The AForc logo was authored for this project as deterministic SVG geometry.
- It contains no embedded third-party images, fonts, scripts, or remote resources.
- The asset is distributed under the repository's MIT License and carries SPDX
  metadata in its XML comment.

## 1. Atmosphere & Identity

AForc feels like a calibrated field instrument running inside a warm terminal: exact, durable, and quietly active. Its signature is an `A` assembled on a two-dimensional coordinate plane, where one phosphor-green square identifies the live origin. The plate remains sparse enough for README use while the left instrument cell rewards close inspection.

## 2. Color

| Role | Token | Value | Usage |
| --- | --- | --- | --- |
| Canvas | `--aforc-canvas` | `#171612` | Fixed warm near-black substrate |
| Ink | `--aforc-ink` | `#FAF9F6` | Wordmark and primary glyph |
| Framework | `--aforc-frame` | `#6A655C` | Border, axes, registration grid |
| Live origin | `--aforc-signal` | `#62D776` | The single active coordinate cell |

Rules:

- Keep the system warm and nearly monochrome. Never replace the canvas with pure black or the ink with pure white.
- Green is a status signal, not a decorative brand wash. It may occupy only one small origin/cursor element in a composition.
- Grid lines may reduce framework opacity for hierarchy; primary boundaries remain full-strength.
- Do not introduce another color without revising this contract first.

## 3. Typography

- **Wordmark:** custom vector geometry spelling `AForc`; it contains no SVG `<text>` and has no font dependency.
- **Interface and code:** `ui-monospace, "Cascadia Mono", "Liberation Mono", monospace`.
- **Supporting prose:** `system-ui, -apple-system, "Segoe UI", sans-serif`.
- Technical labels use uppercase with `0.08em` tracking. Prose uses sentence case. No faux-hacker, rounded sci-fi, or externally loaded typeface is permitted.

## 4. Spacing & Layout

All spacing derives from a 4-unit module: `4`, `8`, `12`, `16`, `24`, `32`, `48`, and `64`.

- The primary lockup uses matching `960 x 280` intrinsic dimensions and viewBox, a 4-unit inset border, and a 40-unit content gutter.
- The instrument cell is a 200-unit square. The wordmark begins after 48 units of clear separation.
- Preserve at least 24 units of clear space around the plate when composing it with other content.
- Keep the full lockup uncropped and preserve its aspect ratio. A displayed width of 320 CSS pixels or greater is preferred.

## 5. Logo Primitive

### Primary Lockup

- **Structure:** fixed canvas, boundary rule, left coordinate-plane `A` cell, and exact-case `AForc` vector wordmark.
- **Geometry:** only straight orthogonal segments, square endpoints, and 90-degree corners. No radius, diagonal flourish, or freehand contour.
- **Theme behavior:** the fixed warm-black plate protects contrast on both light and dark host pages; do not make the canvas transparent.
- **States:** one static primary state. No hover, pressed, loading, animated cursor, or alternate-color state exists.
- **Accessibility:** retain the SVG `role="img"`, `<title>`, `<desc>`, and `aria-labelledby`. When nearby visible text already supplies the same name, the embedding document may mark its image element decorative.
- **Integrity:** do not recolor individual letters, detach the green origin, stretch, rotate, add effects, or typeset a substitute wordmark.

### Terminal UI Primitives

Interactive terminal primitives and their required states are defined in
Section 9. They extend the same tokens without altering the primary lockup.

## 6. Motion & Interaction

The identity never moves. Do not add SVG animation, CSS animation, scripted motion, blinking, scanline travel, hover transformation, or cursor pulsing. The green cell means "active origin" through position and shape, not motion.

## 7. Depth & Surface

Use a borders-only strategy. Depth comes from the warm fixed plate, full-strength enclosure, quieter internal grid, and dense-to-sparse composition. Gradients, shadows, glows, blur, transparency effects, noise filters, and simulated CRT damage are prohibited.

## 8. Accessibility Constraints & Accepted Debt

- Target WCAG 2.2 AA. On `--aforc-canvas`, ink contrast is `17.20:1`, framework contrast is `3.13:1`, and signal contrast is `9.92:1`.
- Color does not carry identity alone: the green signal is also a discrete square at the coordinate intersection.
- The fixed canvas makes the asset readable in light and dark embeddings. The full wordmark remains vector geometry at every scale.
- Accessibility metadata must stay user-facing and descriptive; author and license provenance stays in the separate XML comment.
- **Accepted debt:** none.

## 9. Terminal Game Surfaces

Interactive examples extend the identity into terminal cells without changing
the logo contract above. They remain warm, sparse instruments whose motion
communicates simulation state rather than decorating empty space.

### Terminal Tokens

| Role | Indexed color | Required redundant cue |
| --- | ---: | --- |
| Canvas | `234` | Blank-space field |
| Ink | `255` | Primary ASCII glyph or uppercase label |
| Framework | `243` | Border, horizon, inactive meter, or dim style |
| Signal | `77` | Unique active glyph, marker, or filled meter cell |

Terminal emulators may remap indexed colors. Every state must therefore remain
understandable when all four tokens render as one monochrome ramp. Raw RGB and
additional indexed colors are not part of the example-game contract.

### Surf-Man Primitives

Reference direction: the sports and motion silhouettes at
`https://ascii.co.uk/art/stickman`. Browser capture was unavailable in the WSL
toolchain because Chrome is not installed, so only the fetched ASCII source is
used. Surf-Man derives the reference's classic head/torso/limb grammar without
copying individual signed artworks.

- **Instrument frame:** rectangular ASCII boundary dividing playfield, status,
  and modal regions. The border never carries gameplay meaning by color alone.
- **Wave field:** layered `~`, `=`, `-`, `^`, `#`, `*`, `+`, `.`, `:`, `o`,
  `/`, `\\`, `|`, and `V` geometry. A quiet horizon and rear swell establish depth; the
  playable face, lip, pocket, tube, shoulder, and foam remain distinct in
  monochrome through density and contour rather than color.
- **Rider sprite:** a classic sports-stickman silhouette uses `O` for the head,
  `|` for an unmistakable torso, `/` and `\\` for limbs, and `_` or `-` only
  for deliberate arm extension. The body occupies exactly three rows: head,
  connected arms/torso, then legs directly adjacent to the separate board. It
  keeps one stable center and one stable foot line without an intermediate
  torso-only or hip row. Count-in keeps the compact neutral arm row; it does not
  grow alternating underscore hand extensions. Paddle, neutral ride, toe-side
  carve,
  heel-side carve, air, grab, landing, tube, and wipeout must read without the
  HUD and must never resemble a face embedded in wave texture.
- **Surfboard:** a separate seven-to-eleven-cell rail with curved nose and tail,
  visible deck center, direction, pitch, and landing contact. The board never
  collapses into the rider's feet or a generic underscore.
- **Kinetic effects:** bounded spray fans, lip crumble, tube streaks, board wake,
  landing impact, and wipeout debris use `.`, `*`, `+`, `!`, `x`, `/`, `\\`,
  `^`, `~`, and `=` sequences tied
  to the corresponding simulation event. No effect obscures the rider, board,
  pocket, or hazard cue.
- **Meter:** labelled progress row with filled and empty glyphs. Labels and
  numeric values remain present in no-color mode. Surf-Man's BANK meter always
  uses both `=`/`-` fill and a percentage; color may reinforce but never replace
  its progress.
- **Menu row:** one text action per row. Selection uses both a leading marker
  and bold style; disabled state uses a reason label rather than dimming alone.
  Pause uses the same primitive for Resume, Help, Accessibility, End Session,
  and Quit.
- **Modal panel:** centered bordered region for help, pause, accessibility,
  recap, and resize requirements. It must preserve the underlying context when
  that context remains relevant. Help and Accessibility opened from Pause
  return to Pause instead of implicitly resuming authoritative time.

### Surf-Man Changed And Retained Contract

The responsive surf redesign changes the interaction model, not the terminal
identity:

- `A`/`D` and Left/Right accelerate a bounded, authoritative Q16 line position
  and velocity. Reversing an established line produces a carve; a single press
  still changes motion on the next fixed update.
- `W`/`S` and Up/Down move a bounded, authoritative wave-face offset. The
  rider samples the wave at the current line position, so line and face choices
  change both rule outcomes and the visible rider location.
- Space is a bounded action command. At a high lip it launches an air without a
  directional chord; at a lower lip it snaps. Repeats can renew the bounded
  action, while explicit key release clears it.
- Arrow and WASD aliases are independent held sources. Releasing one alias
  cannot cancel another held alias; when opposite directions are held, the
  latest non-repeat press wins and its release restores the remaining source.
- Count-in copy states that ride controls start at GO. It never instructs the
  player to set a line while directional commands are inert.
- The anchored wave field remains readable while the rider, board, wake, and
  spray move through adjacent cells from authoritative line, face, and air
  state.

The redesign retains the 60 Hz fixed-step simulation, 80 by 24 target and 60
by 20 supported layout, four terminal tokens, ASCII-only glyph vocabulary,
three-row rider and separate board contracts, labelled monochrome cues, and
focus or undersize pauses that do not consume gameplay time.

### Layout And Motion

- Design target: `80 x 24` cells. Play is supported at `60 x 20` or larger.
  Smaller terminals pause authoritative time and display the exact requirement.
- A four-cell rhythm governs major horizontal and vertical regions; one-cell
  spacing is permitted only inside sprites, meters, and compact status rows.
- Authoritative simulation and active normal-play visual composition run at 60
  ticks per second. Clean frames skip composition. Renderer presentation may
  still scan the cell diff, but it emits and writes no unchanged cells;
  reduced-motion and static menus remain event-driven.
- Motion must explain speed, wave phase, score banking, landing, wipeout, or
  scene transition. No blinking, idle scanlines, random shimmer, or perpetual
  motion without gameplay meaning.
- The riding HUD samples the rider-local wave and names the immediate lip,
  tube, or hazard action. It exposes labelled BANK fill and percentage in every
  color mode. Practice names the Pause > End Session path; Shack replaces live
  day, wave, and timer telemetry with menu, best-score, and settings state.
- Wave layers advance at different deterministic rates derived from active
  visual time and board speed. The field remains anchored while rider and board
  position, pose, wake, and spray follow rider-local wave samples, bounded line
  and face motion, airborne state, tube, landing, and recovery. Short easing
  through adjacent cells is preferred over teleporting the focal object.
- Limb animation may swap one anchored arm or leg row, but the head, torso,
  feet, and board must not oscillate independently.
- Playable wave crests form continuous rising and falling contours. Sparse
  texture stays below the crest, foam follows the lip, and background swells
  move more slowly than the playable face. Random-looking full-width noise is
  not an acceptable substitute for wave shape.
- Reduced-motion mode freezes decorative beach motion, disables camera shake,
  and suppresses cosmetic spray while preserving all rule-state cues.

### Accessibility Constraints

- No action requires a chord, reliable key-release event, rapid mashing, mouse,
  audio, color distinction, or ambiguous-width Unicode glyph.
- Directional taps must reach one fixed update even if key-up arrives first.
  Bounded action repeats support tubes and grabs, while explicit release clears
  the action immediately. A high lip plus Space launches an air; direction can
  modify the resulting motion but cannot gate eligibility.
- Arrow and WASD controls, persistent help, wide timing,
  75-percent speed, landing assistance, no-color/high-contrast, and reduced
  motion are first-class states rather than undocumented debug switches.
- Focus loss and undersized-terminal states pause gameplay without consuming
  score, combo, or wave time.
- **Accepted debt:** terminal applications cannot control emulator palette,
  font metrics, or key-repeat policy. Redundant glyphs, normalized commands,
  bounded layouts, and deterministic smoke tests mitigate these host variables.
