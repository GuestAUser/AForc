# AForc

![AForc logo](docs/assets/aforc-logo.svg)

AForc is an ASCII C game engine for 2D planes. It provides the low-level
pieces that responsive terminal games repeatedly need, while leaving game
rules and data layouts under application control.

The repository includes two complete examples. The procedural roguelike
demonstrates the engine loop, terminal lifecycle, buffered renderer, decoded
input, tile world, ECS, effects, UI, deterministic random generation, and save
containers working together. Surf-Man is a deterministic, fixed-step terminal
surf session with responsive Q16 line and wave-face motion, an accessible
single-screen presentation, and off-screen QA.

## Features

- Restorable raw-terminal lifecycle with alternate-screen, resize, and signal
  handling.
- Double-buffered colored cell renderer with incremental terminal updates.
- Escape-sequence and UTF-8 input decoding with keyboard, mouse, focus, paste,
  and resize events.
- Fixed-step engine loop with deferred scene-stack transitions.
- Layered tile maps, cameras, collision queries, raycasts, A* pathfinding, and
  field of view.
- Generational ECS entities with sparse-set component storage and typed views.
- ASCII sprites, animation, easing, tweens, and fixed-point particles.
- Clipped UI layouts, panels, labels, progress bars, buttons, and menus.
- Bounded asset I/O with lexical relative-path validation, PCG random numbers,
  bounded configuration parsing, and versioned checksummed save data. Filesystem
  confinement remains the platform layer's responsibility because standard C
  cannot prevent symlink or mount-point escape.
- No runtime dependency beyond a C17/POSIX environment.

## Requirements

- A POSIX terminal on Linux, macOS, or another Unix-like system.
- A C17 compiler.
- CMake 3.16 or newer, or GNU Make.

## Build And Play

With CMake:

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake --parallel

# Roguelike
./build/cmake/aforc-roguelike

# Surf-Man
./build/cmake/aforc-surf-man
```

With GNU Make:

```sh
make release

# Roguelike
./build/make/bin/aforc-roguelike

# Surf-Man
./build/make/bin/aforc-surf-man
```

Run either game deterministically with `--seed`:

```sh
./build/make/bin/aforc-roguelike --seed 2026
./build/make/bin/aforc-surf-man --seed 2026
```

Both games need an interactive terminal. Their `--help` and `--smoke` modes do
not, which makes them suitable for scripts and continuous integration.

Run Surf-Man's deterministic non-interactive path directly, or run both
examples' smoke checks through Make:

```sh
./build/cmake/aforc-surf-man --smoke
make smoke
```

## Roguelike Controls

| Input | Action |
| --- | --- |
| Arrow keys, `WASD`, or `HJKL` | Move or attack |
| `.` or Space | Wait one turn |
| `>` | Descend while standing on the exit |
| `S` | Save to `.aforc-roguelike.sav` |
| `L` | Load `.aforc-roguelike.sav` |
| `R` | Start a new run after defeat or victory |
| `?` | Toggle the in-game help panel |
| `Q` or Escape | Quit |

Explore each procedurally generated floor, fight the roaming sentinels, and
reach the exit. Clear five floors to win. Movement is turn-based; enemies use
the engine's A* pathfinder whenever the player takes a turn. The same seed
reproduces the same sequence of floors.

## Surf-Man

Surf-Man is a single-screen, three-wave surf session that pairs a bounded,
fixed-step simulation with an ASCII instrument display. Bounded Q16 line and
wave-face motion make steering responsive and free without changing the engine
API. It demonstrates the engine loop, terminal lifecycle, decoded input,
renderer, effects, UI, and PCG random generation without adding persistence,
networking, or audio.

See [examples/surf-man/README.md](examples/surf-man/README.md) for build,
controls, scoring, accessibility, seed, and deterministic smoke-test details.

## Build Options

Important CMake options:

| Option | Default | Purpose |
| --- | --- | --- |
| `AFORC_BUILD_EXAMPLES` | `ON` | Build `aforc-roguelike` and `aforc-surf-man` |
| `AFORC_WARNINGS_AS_ERRORS` | `OFF` | Promote project warnings to errors |
| `AFORC_ENABLE_SANITIZERS` | `OFF` | Enable AddressSanitizer and UBSan |
| `AFORC_ENABLE_HARDENING` | Top-level `ON` | Harden supported project-owned targets |
| `BUILD_SHARED_LIBS` | `OFF` | Build AForc as a shared library |
| `BUILD_TESTING` | `ON` | Register smoke and all regression tests |

Useful Make targets:

```sh
make strict      # clean debug build, -Werror, then all tests
make sanitize    # strict ASan/UBSan build, then all tests
make smoke       # deterministic non-TTY integration checks
make run         # launch the roguelike
make run-surf-man  # launch Surf-Man
make help        # list all supported targets
```

To exercise the CMake test entry:

```sh
cmake -S . -B build/cmake -DAFORC_WARNINGS_AS_ERRORS=ON
cmake --build build/cmake --parallel
ctest --test-dir build/cmake --output-on-failure
```

### Build Hardening

Top-level CMake builds enable `AFORC_ENABLE_HARDENING` by default. Builds that
embed AForc with `add_subdirectory` default it to `OFF`, so project policy does
not silently alter a parent build. On Linux with GNU or Clang, AForc probes the
complete hardening set before applying any of it: Release sources use
`_FORTIFY_SOURCE=3`, project targets use the strong stack protector, shared
library internals use hidden visibility, executables use PIE, and linked shared
libraries and executables use full RELRO with immediate binding. These options
are private to AForc targets and are not exported through `aforc::aforc`.

GNU Make provides the equivalent `HARDEN` control, which defaults to `1` and
performs a compile-and-link capability probe. Use explicit Release commands to
exercise the complete set, including fortification:

```sh
cmake -S . -B build/hardened -DCMAKE_BUILD_TYPE=Release \
  -DAFORC_ENABLE_HARDENING=ON -DAFORC_WARNINGS_AS_ERRORS=ON
cmake --build build/hardened --parallel
ctest --test-dir build/hardened --output-on-failure

make BUILD_DIR=build/make-hardened MODE=release STRICT=1 HARDEN=1 test
```

Set `AFORC_ENABLE_HARDENING=OFF` or `HARDEN=0` to opt out. Sanitizer builds
automatically suppress this hardening set to avoid conflicting instrumentation.
Unsupported platforms or toolchains retain their own defaults instead of
receiving unrecognized flags.

## Using The Library

Public headers live under `include/aforc`. Link against the `aforc` target in the
same CMake build:

```cmake
add_subdirectory(path/to/aforc)
target_link_libraries(your_game PRIVATE aforc::aforc)
```

After `cmake --install`, consume the exported package with:

```cmake
find_package(aforc 0.1 CONFIG REQUIRED)
target_link_libraries(your_game PRIVATE aforc::aforc)
```

Installed consumers can include only the subsystems they use:

```c
#include <aforc/engine.h>
#include <aforc/input.h>
#include <aforc/renderer.h>
#include <aforc/world.h>
```

AForc uses explicit result values and caller-visible ownership. Constructors
document whether returned storage is heap-owned; matching destroy or release
functions accept the resources they own. Internal pointers, such as ECS
component addresses and renderer buffers, may be invalidated by structural
mutation or resize and must not be retained across those operations.

## Repository Layout

```text
include/aforc/     Public C API
src/core/          Allocators, errors, engine loop, and scenes
src/platform/      POSIX terminal backend
src/render/        Cell renderer
src/input/         Terminal input decoder
src/world/         Tile maps, cameras, collision, paths, and FOV
src/ecs/           Entity-component storage and views
src/effects/       Sprites, animation, tweens, and particles
src/ui/            Layout and widgets
src/assets/        Asset I/O, RNG, config, and save containers
examples/roguelike/ Structured roguelike integration example
examples/surf-man/  Surf-Man integration example
DESIGN.md          Brand and terminal-game surface contract
SECURITY.md        Security policy and integration constraints
docs/              Architecture and extension guidance
```

Public contracts remain in `include/aforc`; subsystem representations and
cross-translation-unit helpers remain private under `src`. CMake enumerates
every engine translation unit explicitly, while the Make build discovers the
same one-level `src/<subsystem>/*.c` units. Private headers are neither installed
nor part of the supported ABI.

Both examples keep cross-translation-unit contracts private. The Roguelike
separates its `app/`, `game/`, `presentation/`, `persistence/`, and `qa/`
modules under `examples/roguelike`, with private headers in
`include/roguelike/`; Surf-Man retains its separate `app/`, `game/`,
`presentation/`, `qa/`, and `include/surf_man/` boundaries. Both build
manifests enumerate their module units explicitly.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for subsystem boundaries,
frame flow, and extension guidance. See [SECURITY.md](SECURITY.md) for trust
boundaries, parser limits, filesystem caveats, and hardening guidance.

## License

AForc is available under the MIT License. See [LICENSE](LICENSE).
