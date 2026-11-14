# Game profiles

Per-game optimization defaults, loaded automatically when a game starts (wisteria-cli and Qt).
A profile is `<program id>.ini` (16 hex digits, e.g. `0400C3F00006E000.ini`). Every `.ini` in
this folder is compiled into the build; `<config dir>/game_profiles/<id>.ini` replaces the
bundled one without rebuilding.

Order, last wins: global config, game profile, per-game config (`config/custom/<id>.ini`),
command line flags. A setting that the per-game config sets is never touched by the profile.

```ini
[Profile]
name=Example title
notes=Measured on <date>, <host GPU>; why these values

[Settings]
; Same keys as config.ini / custom/<id>.ini. Enums take their name or index.
gpu_accuracy=High
accelerate_astc=Gpu
astc_recompression=Uncompressed
dma_accuracy=Default
use_asynchronous_gpu_emulation=true
resolution_setup=Res1X
use_vsync=Mailbox

[Environment]
; EDEN_* toggles read through getenv. A variable already set in the environment wins.
EDEN_NX2_AFFINITY=1
```

Only per-game (switchable) settings can be set; unknown keys and bad values are logged and
skipped. The log shows which profile was used and each value it set.

`EDEN_NX2_GAME_PROFILE=0` starts without a profile, `EDEN_NX2_GAME_PROFILE=<file>` uses that
file instead (handy to compare variants in benchmark runs).
