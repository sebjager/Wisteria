# Legal

Rules that keep this fork legally clean. `tools/license-check.sh` and the `license-check`
workflow check the first three for every commit on top of `nx2`.

1. Every new source file starts with an SPDX header, for example:

   ```
   // SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
   // SPDX-License-Identifier: GPL-3.0-or-later
   ```

   Code taken from another project keeps its own copyright line and license, which must be
   compatible with GPL-3.0-or-later (for example MIT, BSD, GPL-2.0-or-later). Write down where it
   came from in the header.
2. Never commit keys, firmware, game files, saves or dumps (`*.keys`, `*.nsp`, `*.xci`, `*.nca`,
   `*.nso`, ExeFS/RomFS folders and the like), not even for tests.
3. AI help is disclosed with an `Assisted-by:` trailer. An AI is never listed as
   `Co-authored-by:` or `Signed-off-by:`.
4. Do not use leaked Nintendo SDK material, NVN headers or NVIDIA sources, not even as a
   reference. Notes on reverse engineering stay short: names and offsets, no disassembly dumps or
   decompiled game code.
5. No links to keys, firmware or game downloads in the app, the docs or release notes.
6. No Nintendo logos or artwork. Name third-party technology honestly (for example, do not call a
   feature FSR 3 if it does not use AMD FSR 3).
7. Every binary release ships `LICENSE.txt` and `LICENSES/` and links the exact source commit
   (see `.github/workflows/build-*.yml` and `tools/ci/release-notes.sh`).
