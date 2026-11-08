# Changelog

All notable changes to Eglantine are documented here. Each release's notes are taken from its
`## [X.Y.Z]` section (release candidates use the section of the version they are a candidate for).
New entries go under `## [Unreleased]`; `tools/ci/bump-version.sh` turns them into a version section.

## [Unreleased]

## [0.1.1]

First release of the sebjager/Eglantine fork: upstream Eglantine nx2 at `74db237124`
(2026-11-08) plus this fork's own work and commits taken from other forks (see `CREDITS.md`).

### In this fork

- **Mario Kart World game profile**, applied automatically: GPU accuracy Low (GPU thread split
  on), ASTC to BC3, reactive flushing off, asynchronous shaders. Measured on a Ryzen 7 7800X3D
  with an RTX 4080 SUPER (attract loop, 3 x 120 s): 19.0 ms per frame (p95 24.5 ms), no crashes.
  `EDEN_NX2_GAME_PROFILE=0` turns profiles off.
- **Game profiles by title ID** (`dist/game_profiles/<id>.ini`, overridable in the config folder),
  also for eden-cli, which now loads `custom/<id>.ini` too.
- **Recommended settings from your hardware** (Emulation > Recommended Settings); only settings
  that were tested are offered. Switch 2 titles keep uncompressed ASTC only with a 20 GiB VRAM
  budget, so smaller cards no longer run out of video memory.
- **GPU thread split at GPU accuracy High**, opt-in with `EDEN_NX2_SPLIT_HIGH=1`: 28.0 ms against
  37.5 ms per frame in Mario Kart World at High + BC3.
- Audio: a short fade instead of a click when the audio buffer runs empty.
- Optional thread pinning (`EDEN_NX2_AFFINITY=1`) and unattended game tests with reports
  (`EDEN_NX2_COMPAT_REPORT`, `tools/nx2/compat_run.py`).
- From other forks, with their authors kept: shader recompiler speedups (Wael3rd), host memory
  when VRAM is full (Franseven, from Eden-NS2), an ASID fix for cached page runs, a fiber
  leak fix and a Windows timer fix (snowstormxd, Sssnowyyy), Mario Kart World grass fixes
  (snowstormxd, kraibse), a wider VRAM margin (Jonas).
- Every package ships `LICENSE.txt` and `LICENSES/`; the release notes link the exact source.

### From upstream since 0.1.0

- Added OOT Compatibility
- Improved MKW Performances
- Added StarFox Compatibility
- Less shader rebuilds
- Changed dir from eden to eglantine
- Overall better performances and fixes

## [0.1.0]

- Initial release candidate
