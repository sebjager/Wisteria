<!--
SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Credits

This fork (sebjager/Wisteria) builds on
[Eglantine](https://github.com/Eglantine-Community/Eglantine) (formerly Djihads80/Eglantine),
which builds on [Eden](https://git.eden-emu.dev/eden-emu/eden) and yuzu. All of it is GPL-3.0;
see `LICENSE.txt` and `LICENSES/`.

Besides upstream, this fork takes commits from other Eglantine forks. Each one keeps its original
author, and its commit message names where it came from (`cherry picked from`, `Taken from:` or
`Source:`). The forks and authors used so far:

| Fork | Authors of the commits taken | What |
| --- | --- | --- |
| [Franseven/PokeDinner](https://git.eden-emu.dev/Franseven/PokeDinner-a-Eglantine-Fork) | Wael3rd, Franseven | Shader recompiler speedups (SSA rewrite, CFG lookup, SPIR-V constant ids), host-memory fallback when VRAM is full (from 0x2593/Eden-NS2) |
| [snowstormxd/Eglantine-mkw-performance](https://github.com/snowstormxd/Eglantine-mkw-performance) | snowstormxd, Sssnowyyy | ASID fix for cached page runs (from zortos293), fiber weak-reference leak fix, Windows timer thread release, IADD3 immediates for Mario Kart World's grass |
| [jojo-chaechae/Eglantine](https://github.com/jojo-chaechae/Eglantine) | Jonas | Lower bound for the texture cache's VRAM margin |
| Upstream branch `fix/missing-mkw-effects` | kraibse | HSETP2 constant-buffer form (Mario Kart World's grass) |

Forks that were looked at, and why commits were or were not taken, are tracked outside the
repository in the project's fork-scan reports.
