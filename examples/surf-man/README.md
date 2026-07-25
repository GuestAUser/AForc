# Surf-Man

Surf-Man is a deterministic terminal surf-session example for AForc. It keeps
rules and active ride presentation on a bounded 60 Hz cadence while rendering a
readable, diffed single-screen ASCII instrument. Static and reduced-motion
surfaces are event-driven: unchanged frames do not recompose. An interactive
run uses the POSIX terminal; the same scene also runs through an off-screen
deterministic smoke path.

## Build And Run

From the repository root, build the Surf-Man target with CMake:

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake --target aforc_surf_man --parallel
./build/cmake/aforc-surf-man
```

With GNU Make, `run-surf-man` builds and launches the example:

```sh
make run-surf-man
```

Inspect the command-line contract or choose a repeatable session with:

```sh
./build/cmake/aforc-surf-man --help
./build/cmake/aforc-surf-man --seed 2026
```

`--seed` selects the mechanics seed. The same seed and command stream reproduce
the same wave sequence, scores, and simulation state hash across supported
frame schedules. The seed does not require a saved file and Surf-Man does not
create one.

## Smoke And Deterministic QA

Run the non-interactive smoke path directly or through the aggregate Make
target:

```sh
./build/cmake/aforc-surf-man --seed 2026 --smoke
make smoke
```

CMake also registers `surf_man_help`, `surf_man_smoke`,
`surf_man_invalid_seed`, and `surf_man_negative_seed`:

```sh
ctest --test-dir build/cmake -R '^surf_man_' --output-on-failure
```

Smoke mode creates no terminal session. It drives the normal app, scene,
simulation, renderer, and input-normalization paths using deterministic time
and an off-screen renderer. Simulation checks compare steady `{1}`,
alternating `{1, 3}`, and bounded-stall `{0, 0, 8}` frame schedules while
covering immediate directional response, bounded line and wave-face travel,
momentum reversals, rider-local wave sampling, recovery, scoring, saturation,
and state-hash coverage. Render checks assert the 80 by 24 shack, menu, HUD,
rider, and board contracts; dynamic rider position; ASCII, token, and
no-blink rules; high-contrast and no-color output; reduced-motion composition;
and bordered resize notices. Smoke checks cover directional taps, bounded
action leases and explicit releases, focus/resize pause invariants, the real
pushed off-screen scene, and no filesystem effects.

## Session Loop And Scoring

A normal surf day contains three 45-second waves. Each begins with a three-second
count-in; a wipeout uses a two-second recovery before the next state. The app
moves through the shack, count-in, riding, wipeout recovery, wave recap, and
day recap states. Practice uses the same rules but loops its timed wave instead
of advancing a normal day; Back banks pending points and returns to the shack.
While riding, move along the changing wave face, build and reverse line
momentum for carves, and use one unchorded action for lip snaps, airs, and
tubes.

Maneuvers create pending score. Consecutive varied maneuvers build flow to a
maximum of five, while immediate repetition earns half base score without
raising flow. Risky maneuvers mark the current stake. After 2.5 stable seconds,
a bank moves pending points into the day score with `uint64_t`
saturation. A wipeout clears pending score and resets flow and risk while
preserving the banked day score. The HUD keeps score, flow, risk, remaining wave
time, and the current state visible through labels, values, and glyphs rather
than color alone.

## Ride Technique

- Use `A`/`D` or Left/Right to build line momentum. Reverse an established
  line to carve and build FLOW.
- Use `W`/`S` or Up/Down to climb high or drop low on the wave face.
- At a high lip, press Space alone to launch an air. At a lower lip, Space
  snaps instead. While airborne, steer for rotations and press Space to grab.
- Hold Space through tube sections to ride them. Stay high or move the line to
  clear hazards.
- Land level, then stay stable for 2.5 seconds to bank pending points.
- Vary maneuvers. Repeating the immediately previous trick earns half base
  score and does not build FLOW.

## Controls And Accessibility

The main menu provides Surf, Practice, Help, Accessibility, and Quit. Arrow
keys and `WASD` provide the same directional input for menu navigation and
riding: Up/Down climb or drop on the face, while Left/Right build line
momentum. `Space` is the ride action and activates the selected menu item;
at a high lip it launches without requiring a directional chord. Directional
taps survive a key-up that arrives before the next fixed update. Held or
repeated Space renews a bounded ride-action lease; explicit release clears it
immediately.
`Enter` activates the selected menu item. `Escape` returns from a panel or
pauses the session, `P` pauses or resumes, `?` opens Help, and `Q` or `Ctrl-C`
requests quit. The in-game Help panel teaches riding technique during play.
Select Accessibility from the main menu with Up/Down and Enter or Space; in
the panel, Up/Down selects, Left/Right changes, and Escape returns.

Surf-Man targets 80 by 24 cells and remains playable at 60 by 20 or larger. A
smaller terminal, focus loss, or resize pause stops authoritative time without
consuming a wave, score, or flow. The Accessibility panel provides wide timing,
75-percent speed, landing assistance, high-contrast and no-color modes, and
reduced motion. Reduced motion freezes decorative beach and wave-phase motion,
suppresses cosmetic spray, and preserves all rule-state cues. Static and
reduced-motion surfaces compose only after a rule-state, input, resize, or
remaining-effect change.

Every action is available without chords, mouse input, rapid mashing, reliable
key-release events, color discrimination, audio, or ambiguous-width Unicode.
ASCII glyphs, labels, meter values, selection markers, and static modal text
provide redundant state cues.

## Module Boundaries And Scope

`app/` owns lifecycle, CLI, responsive input normalization, and engine hooks.
`game/` owns deterministic Q16.16 rules, bounded line and wave-face motion,
waves, and scoring. `presentation/` owns renderer composition, dynamic rider
placement, effects, HUDs, menus, and overlays. `qa/` owns deterministic
off-screen checks. The private headers in `include/surf_man/` connect those
modules but are not installed library headers. This redesign uses those
game-local responsibilities and adds no public AForc API.

The example demonstrates engine scenes and timing, terminal lifecycle, decoded
input, diffed cell rendering, effects, UI, and seeded PCG mechanics. It is
deliberately not a persistence, network, audio, or input-remapping example.
An interactive run owns the process terminal session; do not combine it with
another raw-mode or alternate-screen owner without the coordination described
in [SECURITY.md](../../SECURITY.md).
