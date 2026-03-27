<p align="center"><img src="https://user-images.githubusercontent.com/661798/36482670-f81601c0-170b-11e8-8adb-2365b346ac27.png" /></p>

> **This is an unofficial community fork of [RenderDoc](https://github.com/baldurk/renderdoc) maintained at [alandtse/renderdoc](https://github.com/alandtse/renderdoc).**
>
> This fork has two purposes. First, it permits the **appropriate** use of LLMs and AI-assisted tooling in development — the one policy where it departs from upstream. AI-generated code submitted without review or understanding is not acceptable, and contributors must be able to explain their changes. All other upstream contribution guidelines are followed as closely as possible.
>
> Second, it is structured to protect the community's contributions. Fork-specific code is GPL-3.0-or-later, ensuring it remains open source regardless of any future change in upstream's direction. Reintegration with upstream is the goal if upstream policy allows, but if upstream ever moves away from an open-source license, no further contributions from this fork will be eligible for integration. See [COPYING](COPYING) and the [Contributor License Agreement](CONTRIBUTOR_LICENSE_AGREEMENT.md) for details.
>
> This fork tracks upstream `v1.x` closely. Issues arising from fork-specific changes must be reported here and **not** to the upstream project — do not file issues or pull requests related to this fork's changes against the upstream repository.

[![Upstream license: MIT](https://img.shields.io/badge/upstream%20license-MIT-green.svg)](LICENSE.md)
[![Fork additions: GPL v3](https://img.shields.io/badge/fork%20additions-GPL--3.0--or--later-blue.svg)](COPYING)
[![CI](https://github.com/alandtse/renderdoc/actions/workflows/ci.yml/badge.svg?branch=dev&event=push)](https://github.com/alandtse/renderdoc/actions)
[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-v2.0%20adopted-ff69b4.svg)](docs/CODE_OF_CONDUCT.md) 

RenderDoc is a frame-capture based graphics debugger, currently available for Vulkan, D3D11, D3D12, OpenGL, and OpenGL ES development on Windows, Linux, Android, and Nintendo Switch&trade;. It is completely open-source.

RenderDoc is intended for debugging your own programs only. Any discussion of capturing programs that you did not create will not be allowed in any official public RenderDoc setting, including the issue tracker, discord, or via email. For example this includes capturing commercial games that you did not create, or capturing Google Maps or Google Earth. Note: Capturing projects you created that use a third party engine like Unreal or Unity, or open source and free projects is completely fine and supported.

If you have any questions, suggestions or problems with this fork you can [create an issue](https://github.com/alandtse/renderdoc/issues/new/choose) here on github. For questions about upstream RenderDoc, see the [upstream repository](https://github.com/baldurk/renderdoc).

To install on windows run the appropriate installer for your OS ([64-bit](https://renderdoc.org/stable/latest/RenderDoc_latest_64.msi) | [32-bit](https://renderdoc.org/stable/latest/RenderDoc_latest_32.msi)) or download the portable zip from the [builds page](https://renderdoc.org/builds). The 64-bit windows build fully supports capturing from 32-bit programs. On linux only 64-bit x86 is supported - there is a precompiled [binary tarball](https://renderdoc.org/stable/latest/renderdoc_latest.tar.gz) available, or your distribution may package it. If not you can [build from source](docs/CONTRIBUTING/Compiling.md).

* **Downloads**: Stable and nightly builds: https://renderdoc.org/builds ( [Symbol server](https://renderdoc.org/symbols) )
* **Documentation**: [HTML online](https://renderdoc.org/docs), [CHM in builds](https://renderdoc.org/docs/renderdoc.chm), [Videos](https://www.youtube.com/user/baldurkarlsson)
* **Fork issues**: [alandtse/renderdoc issue tracker](https://github.com/alandtse/renderdoc/issues)
* **Upstream contact**: [baldurk@baldurk.org](mailto:baldurk@baldurk.org), [#renderdoc on OFTC IRC](https://webchat.oftc.net/?channels=renderdoc), [Discord server](https://discord.gg/ahq6yRB)
* **Code of Conduct**: [Contributor Covenant](docs/CODE_OF_CONDUCT.md)
* **Information for contributors**: [All contribution information](docs/CONTRIBUTING.md), [Compilation instructions](docs/CONTRIBUTING/Compiling.md)
* **Community extensions**: [Extensions repository](https://github.com/baldurk/renderdoc-contrib)

Screenshots
--------------

| [ ![Texture view](https://renderdoc.org/fp/ts_screen1.jpg?2) ](https://renderdoc.org/fp/screen1.jpg) | [ ![Pixel history & shader debug](https://renderdoc.org/fp/ts_screen2.jpg?2) ](https://renderdoc.org/fp/screen2.png) |
| --- | --- |
| [ ![Mesh viewer](https://renderdoc.org/fp/ts_screen3.jpg?2) ](https://renderdoc.org/fp/screen3.png) | [ ![Pipeline viewer & constants](https://renderdoc.org/fp/ts_screen4.jpg?2) ](https://renderdoc.org/fp/screen4.png) |

API Support
--------------

|                          | Windows                  | Linux                    | Android                   |
| ------------------------ | ------------------------ | ------------------------ | ------------------------  |
| Vulkan                   | :heavy_check_mark:       | :heavy_check_mark:       | :heavy_check_mark:        |
| OpenGL ES 2.0 - 3.2      | :heavy_check_mark:       | :heavy_check_mark:       | :heavy_check_mark:        |
| OpenGL 3.2 - 4.6 Core    | :heavy_check_mark:       | :heavy_check_mark:       |  N/A                      |
| D3D11 & D3D12            | :heavy_check_mark:       |  N/A                     |  N/A                      |
| OpenGL 1.0 - 2.0 Compat  | :heavy_multiplication_x: | :heavy_multiplication_x: |  N/A                      |
| D3D9 & 10                | :heavy_multiplication_x: |  N/A                     |  N/A                      |
| Metal                    |  N/A                     |  N/A                     |  N/A                      |

* Nintendo Switch&trade; support is distributed separately for authorized developers as part of the NintendoSDK. For more information, consult the Nintendo Developer Portal.

Downloads
--------------

There are [binary releases](https://renderdoc.org/builds) available, built from the release targets. If you just want to use the program and you ended up here, this is what you want :).

It's recommended that if you're new you start with the stable builds. Nightly builds are available every day from the [v1.x branch here](https://renderdoc.org/builds#nightly) if you need it, but correspondingly may be less stable.

Documentation
--------------

The text documentation is available [online for the latest stable version](https://renderdoc.org/docs/), as well as in [renderdoc.chm](https://renderdoc.org/docs/renderdoc.chm) in any build. It's built from [restructured text with sphinx](docs).

As mentioned above there are some [youtube videos](https://www.youtube.com/user/baldurkarlsson) showing the use of some basic features and an introduction/overview.

There is also a great presentation by [@Icetigris](https://twitter.com/Icetigris) which goes into some details of how RenderDoc can be used in real world situations: [slides are up here](https://docs.google.com/presentation/d/1LQUMIld4SGoQVthnhT1scoA3k4Sg0as14G4NeSiSgFU/edit#slide=id.p).

License
--------------

This repository contains code under two licenses:

- **Files from upstream RenderDoc** are licensed under the **MIT License** — see [LICENSE.md](LICENSE.md). This covers all files present in [upstream](https://github.com/baldurk/renderdoc) at the time they were incorporated; their copyright notices must not be modified.
- **Fork-specific contributions** (new files and modifications made in this fork) are licensed under **GPL-3.0-or-later** — see [COPYING](COPYING).

By contributing to this fork you accept the [Contributor License Agreement](CONTRIBUTOR_LICENSE_AGREEMENT.md). For upstream reintegration, the CLA pre-authorizes a **copyright assignment to Baldur Karlsson** — the upstream project requires full copyright assignment, not merely a license. Read the full CLA before contributing.

Compiling
---------

Building RenderDoc is fairly straight forward on most platforms. See [Compiling.md](docs/CONTRIBUTING/Compiling.md) for more details.

Contributing & Development
--------------

I've added some notes on how to contribute, as well as where to get started looking through the code in [Developing-Change.md](docs/CONTRIBUTING/Developing-Change.md). All contribution information is available under [CONTRIBUTING.md](docs/CONTRIBUTING.md).

