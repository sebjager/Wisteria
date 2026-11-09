<!--
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2018 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
-->
<!-- lang: en-GB -->

<h1 align="center">
  <br>
  <br>
  <b>Wisteria</b>
  <br>
</h1>

<h4 align="center"><b>Wisteria</b> is a fork of <b>Eglantine</b>, a free and opensource (FOSS) Switch 2 emulator derived from Eden, Yuzu and Sudachi - started by developer Camille LaVey.
It's written in C++ with portability in mind, with builds for Windows, Linux and macOS
</h4>


> [!NOTE]
> **This is an unofficial fork:** Wisteria ([sebjager/Wisteria](https://github.com/sebjager/Wisteria)) is a
> modified version of [Eglantine](https://github.com/Eglantine-Community/Eglantine) (formerly
> Djihads80/Eglantine), which derives from Eden, yuzu and Sudachi. It is not made or endorsed by
> the Eglantine developers, and it has no ties to Nintendo. Please report problems with this fork
> here, not upstream or on the Eglantine Discord. The program and its files are still named
> `eglantine` for now.
>
> **What changed:** this fork adds game profiles, hardware-based recommended settings, performance
> work and commits taken from other Eglantine forks. Every change is listed in
> [CHANGELOG.md](./CHANGELOG.md) and in the git history; code from other forks keeps its original
> authors and is listed in [CREDITS.md](./CREDITS.md).
>
> The emulator contains no keys, firmware or games. You must dump them yourself from a console you
> own; this repository does not help you get them in any other way.

<p align="center">
    </a>
    <a href="https://discord.gg/XkS5NVPTUY">
        <img src="https://img.shields.io/discord/1367654015269339267?color=5865F2&label=Eglantine&logo=discord&logoColor=white"
            alt="Discord">
    </a>
  
</p>

<p align="center">
  <a href="#compatibility">Compatibility</a> |
  <a href="#development">Development</a> |
  <a href="#building">Building</a> |
  <a href="#download">Download</a> |
  <a href="#support">Support</a> |
  <a href="#license">License</a>
</p>

## Compatibility

The emulator is capable of running Mario Kart World at decent/full speed depending on specs and configurations, and Splatoon Raiders at variable performances

A list of supported games will be available in future. Please be patient.

## Development

For development discussions, please join us on [Discord](https://discord.gg/XkS5NVPTUY)

If you would like to contribute, we are open to new developers and pull requests. Please ensure that your work is of a high standard and properly documented. You can also contact any of the developers on Discord or Stoat to learn more about the current state of the emulator.

## Documentation

We have a user manual! See our [User Handbook](./docs/user/README.md).

## Building

See the [General Build Guide](docs/Build.md)

For information on provided development tooling, see the [Tools directory](./tools)

## Download

Builds of this fork are on its [releases page](https://github.com/sebjager/Wisteria/releases). Each release names the exact source commit it was built from; its source code is in this repository at that tag. Upstream Eglantine builds are [here](https://github.com/Eglantine-Community/Eglantine/releases).

## Support

We currently do not take in any donations for the time being

The Discord above belongs to upstream Eglantine. For problems with this fork, open an [issue in this repository](https://github.com/sebjager/Wisteria/issues).

## License

Eglantine is licensed under the GPLv3 (or any later version). Refer to the [LICENSE.txt](./LICENSE.txt) file; third-party parts carry their own licences, listed in [LICENSES](./LICENSES).
