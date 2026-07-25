# Security Policy

## Supported Releases

AForc is pre-1.0 software. Security fixes are applied to the current mainline
release; older snapshots are not maintained.

## Trust Boundaries

AForc is an in-process game engine, not a sandbox. Game callbacks, custom
allocators, tile predicates, asset roots, and native code linked by the game are
trusted. The engine does not provide process isolation, privilege separation,
network security, cryptography, or anti-cheat controls.

Asset files, configuration text, save containers, and terminal input may be
untrusted. Their parsers use explicit size limits, checked arithmetic, bounded
queues, version fields, and checksums where applicable. Applications should set
limits lower than the defaults when their data model permits it.

Relative asset-path validation is lexical only. Standard C file APIs cannot
prevent symlink traversal, mount-point escape, path replacement races, special
files, or blocking I/O. Games that process attacker-controlled paths must add a
platform confinement layer, such as directory-handle-relative opens with
no-follow policies and an allowlisted asset tree.

The POSIX terminal backend changes process-global terminal and signal state.
Use one terminal instance per process, initialize it on the main thread, and
always execute the matching shutdown path. Do not mix it with another library
that independently owns raw mode or the alternate screen without explicit
coordination.

## Secure Integration Checklist

- Run games without elevated privileges and keep writable save data outside the
  read-only asset tree.
- Treat checksum failures as corruption detection, not proof of authenticity.
  AForc save-container CRC-32 is not a message authentication code.
- Cap map dimensions, entity counts, pathfinding visits, file sizes, config
  entries, and save payloads according to the game's actual requirements.
- Validate game-specific semantics after parsing; syntactically valid data may
  still be unsafe for a particular game rule set.
- Keep callbacks non-reentrant unless their API explicitly permits reentrancy.
  Do not retain pointers documented as invalidated by mutation or resize.
- Scrub secrets before logging. AForc errors are diagnostic text and must not be
  treated as safe for direct display across a trust boundary.
- Build release artifacts with compiler hardening supported by the target
  platform and run ASan/UBSan builds during development.

## Reporting A Vulnerability

Report suspected vulnerabilities privately to the project maintainer. Include
the affected version, platform, minimal reproducer, impact, and any proposed
fix. Avoid publishing exploit details until a corrected release is available.
