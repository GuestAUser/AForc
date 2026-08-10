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
