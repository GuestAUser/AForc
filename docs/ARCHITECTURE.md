# AForc Architecture

## Design Goals

AForc is an ASCII C game engine for 2D planes. Its architecture favors:

- Explicit ownership and predictable cleanup in plain C17.
- Bounded work and checked arithmetic at untrusted or allocation boundaries.
- Deterministic simulation that can be driven without a live terminal.
- Independent subsystems joined by narrow adapters in the application.
- Terminal restoration on normal exit, signals, and partial initialization.

AForc is not a full content editor, scripting runtime, network stack, or general
windowing abstraction. The current platform contract is a POSIX terminal.

## Dependency Layers

```r
                         application
                              |
       +----------+-----------+-----------+----------+
       |          |           |           |          |
    engine      world        ECS       effects       UI
       |          |           |           |          |
     scene        +-----------+-----------+----------+
       |                      common
       |
 input -------- renderer -------- terminal (POSIX)
       |
     assets ---------------------- common
```

`common` defines shared statuses, errors, allocators, logging, and geometry.
Higher layers do not depend on application state. The application owns the
adapters that convert UI/effect plot callbacks into renderer cells and input
events into scene events.

The library does not use a global service locator. The POSIX terminal backend
holds only the process-wide state necessary for signal-safe restoration and
permits one active terminal instance.

## Implementation Boundaries

The installed ABI is defined only by `include/aforc`. Each subsystem may split
its implementation across cohesive C translation units and narrow private
headers under `src`; those private declarations are not installed. Helpers that
must cross translation-unit boundaries use hidden visibility, while file-local
helpers remain static. CMake enumerates all engine units explicitly and the
Make build discovers the equivalent `src/<subsystem>/*.c` set.

## Subsystems

### Core And Scenes

`AFORC_Engine` owns timing, the fixed-step accumulator, frame counters, deferred
scene commands, and a scene stack. The application supplies hooks for event
polling, frame preparation, presentation, time, and sleep.

The frame/run loop, deferred command application, and scene-stack traversal
are separate implementation units joined by the engine's private state.

Scene transitions are requested during callbacks and applied at safe dispatch
boundaries. This prevents a scene callback from invalidating the stack that is
currently walking it. Scene flags independently control whether update,
render, and event dispatch continue to lower scenes.

### Terminal, Input, And Renderer

The terminal backend owns raw mode, nonblocking input, output modes, signal
handlers, and dimensions. Opening is transactional: a failure restores every
mode changed earlier in initialization. Closing accepts the active handle and
restores process state.

Terminal lifecycle, byte/mode I/O, and process-wide signal handling remain
separate implementation units so signal-safety and restoration rules stay
isolated from ordinary handle operations.

Input transforms terminal bytes into bounded `AFORC_InputEvent` queues. Frame
state distinguishes held, pressed, and released inputs. Escape decoding is
incremental so partial terminal sequences can span reads.

The public input facade coordinates private queue, state, protocol, and
incremental-parser units against one timestamp and bounded storage contract.

The renderer owns front and back cell buffers. Drawing mutates the back
buffer; presentation emits only changed cells and then synchronizes buffers.
Resize invalidates buffer pointers and forces a complete redraw.

Allocation/lifecycle, drawing, and ANSI diff encoding are separate units; only
a successful complete write commits the front buffer.

### World

The world layer owns dense, layered tile maps. Cameras translate between world
and screen coordinates and clamp a viewport to map bounds. Collision callbacks
let the application decide which tile values block movement or sight.

A* supports caller-owned output storage and a reusable workspace whose reserved
searches allocate no scratch. Field of view writes a caller-provided visibility
array and uses inline task storage, spilling through the map allocator only for
complex scans.

Tile storage, camera transforms, collision, raycast, A*, and field of view are
independent units sharing only checked coordinate/index helpers.

### ECS

Entities are `(index, generation)` handles. Destroying and later reusing an
index changes its generation, so stale handles fail safely. Each registered
component type uses dense component/entity arrays plus a sparse entity index.

Structural mutation invalidates component pointers and active views. Views
snapshot the ECS revision and select the smallest required component store as
their iteration driver. Game systems should finish a view before adding,
removing, or destroying entities.

Entity lifecycle, component sparse-set mutation, and revision-bound view
iteration are separate units over one private registry representation.

### Effects And UI

Effects and UI are renderer-independent. Both emit ASCII cells through small
plot callbacks. This keeps them usable in tests, off-screen renderers, and
custom frontends.

Effects include transformed sprites, frame animation, easing/tweens, and a
fixed-capacity fixed-point particle pool. UI includes clipped canvases,
deterministic layout helpers, widgets, and separate input state machines.

### Assets And Persistence

Asset paths are relative and lexically validated against traversal, depth,
component, and size policies before I/O. This is not filesystem confinement:
standard C file APIs cannot prevent symlink traversal, mount-point escape, or
path-replacement races. Load APIs return owned blobs or text with matching
release functions. See [`SECURITY.md`](../SECURITY.md) for the full trust
boundary.

The PCG random generator is explicitly seeded and stream-selected. The config
parser applies caller-provided byte and entry limits. Save writers produce a
versioned, checksummed container; readers validate the container before
exposing its bounded payload.

Lexical path/blob I/O, PCG, bounded config parsing, and save containers are
separate units; only result-release helpers remain in the assets facade.

## Frame Flow

The interactive examples drive one engine frame as follows:

1. `poll_events` starts a new input frame, polls terminal bytes, decodes all
   available events, and dispatches them through the scene stack.
2. The engine computes a clamped elapsed time and runs zero or more fixed
   updates without exceeding the configured catch-up limit.
3. The active scene performs its variable update.
4. `render` clears and fills the renderer back buffer using world, ECS,
   effects, and UI data.
5. `present` emits the renderer diff to the terminal.
6. Deferred scene commands are applied and the frame limiter sleeps for any
   remaining budget.

Each `--smoke` path uses its normal game scene, input decoder, renderer, engine
frame function, and relevant subsystems with an off-screen renderer and
deterministic timestamps. Only raw-terminal open/present is skipped, so CI does
not require a TTY.

## Roguelike Composition

| Engine surface | Showcase responsibility |
| --- | --- |
| Engine and scene | Frame ownership, event dispatch, rendering callback |
| Terminal | Raw interactive session, resize, safe restoration |
| Renderer | Map, actor, particle, and HUD cell output |
| Input | Key sequence decoding and per-frame event queue |
| World | Procedural tile map, camera, FOV, collision, enemy A* |
| ECS | Player and enemy position/actor components |
| Effects | Hit and defeat particle bursts |
| UI | Framed status area, health progress, help overlay |
| Assets | Seeded generation, config values, save/load container |

The Roguelike is a nested example under `examples/roguelike`. Its
`include/roguelike/internal.h` header is a private cross-translation-unit
contract, not installed AForc API. `app/` contains the engine, terminal, and
input adapters; `game/` contains state, rules, generation, actors, and turns;
`presentation/` contains rendering and effects; `persistence/` owns save/load
integration; and `qa/` contains the off-screen smoke checks. `main.c` remains
at the example root as the CLI entry point.

Procedural generation consumes only a floor-derived RNG stream. Loading can
therefore reconstruct a floor from the run seed and floor number before
restoring dynamic values. The save schema is versioned independently from the
engine library version.

## Surf-Man Composition

Surf-Man is a second, nested example under `examples/surf-man`. Its headers in
`examples/surf-man/include/surf_man` are private cross-translation-unit
contracts for the example, not installed AForc API. The application keeps
engine adapters separate from deterministic rules and presentation so the same
session can run interactively or off-screen.

The nested module boundaries are:

- `app/` owns CLI parsing, reverse-order terminal cleanup, engine hooks, input
  normalization, scene state, and pause/resize handling. It joins
  `AFORC_Engine`, terminal, input, and renderer.
- `game/` owns the Q16.16, 60 Hz surf rules, bounded line position and
  momentum, bounded wave-face motion, rider-local wave sampling, wave
  progression, scoring, and state hash. It consumes a seeded `AFORC_Rng`; it
  has no terminal, renderer, wall-clock, or cosmetic-RNG dependency.
- `presentation/` converts read-only app and simulation state into ASCII
  renderer cells, rider-local placement and poses, bounded effects particles,
  HUDs, menus, and modal panels. It does not change rules or scores.
- `qa/` drives the actual app, simulation, and renderer through deterministic
  schedules with an off-screen target. It checks input taps and leases,
  bounded motion and recovery, state hashes, visible cells, color modes,
  reduced motion, and resize framing without terminal I/O or filesystem
  effects.

The app maps terminal input into bounded directional and ride-action leases,
while confirm and back remain one-shot commands before fixed updates. A tap is
preserved even if its key-up arrives before the next fixed update; an explicit
release clears the matching lease immediately. `AFORC_Engine` owns the 60 Hz
simulation cadence, while active presentation composes at 60 Hz and static or
reduced-motion states remain dirty-driven. `AFORC_Input` supplies decoded key
and focus events; `AFORC_Renderer` supplies the diffed cell buffer; effects and
UI provide bounded particles, layouts, and overlays. The assets subsystem
provides the explicitly seeded PCG stream used to reproduce wave mechanics.

No engine extension was required for Surf-Man's responsive motion. Input lease
normalization belongs in the example adapter, and the Q16 line, face, air, and
recovery rules remain game-local state. The public AForc API and engine
ownership/error conventions therefore remain unchanged.

Surf-Man targets an 80 by 24 terminal and supports 60 by 20 or larger. Focus
loss and an undersized terminal pause authoritative time rather than consuming
wave time, score, or flow. Its smoke path validates the same scene off-screen
under deterministic timestamps and frame schedules, including renderer output,
so continuous integration does not need a TTY.

The example intentionally has no save data or other persistence, network
protocol, audio system, or arbitrary input-remapping layer. Session state and
accessibility choices remain local to the running process.

## Ownership And Failure Rules

- Every successful `create`, `open`, or `init` call has one matching destroy,
  close, release, deinit, or dispose path.
- The application unwinds resources in reverse construction order.
- Caller-provided fixed storage remains caller-owned.
- Functions validate user input, external bytes, path data, dimensions, and
  arithmetic that can overflow. Internal typed contracts are not revalidated
  speculatively.
- Result enums are checked at subsystem boundaries and translated into an
  `AFORC_Error` only when crossing into engine callbacks.
- The engine and subsystems are single-threaded unless a caller externally
  synchronizes independent instances. Terminal process state is not parallel.

## Extending AForc

New systems should depend on the narrowest lower layer available. Prefer an
application adapter over adding a renderer dependency to world, ECS, effects,
or UI. New platform backends should implement the terminal contract without
changing input or renderer APIs. New save schemas should retain bounded reads
and reject unsupported schema versions before consuming payload fields.

Keep public structs value-oriented where practical, opaque when invariants or
ownership require it, and pair every heap-returning API with an explicit
release function.
