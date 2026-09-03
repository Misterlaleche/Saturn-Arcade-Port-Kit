# Saturn Arcade Port Kit

A clean, reusable Sega Saturn 2D arcade-porting foundation built around **Yaul/libyaul**, with a hardware-oriented VDP1/VDP2 architecture.

The repository intentionally contains **no commercial ROMs and no extracted game assets**. The bundled demo generates all graphics procedurally at runtime.

## v0.1.0 capabilities

- 352×224 progressive low-resolution video (`VDP2_TVMD_HORZ_NORMAL_B`, 28 MHz system clock selected by Yaul).
- VDP2 NBG0 `512×256` RGB32768 bitmap background.
- Correct 1.0 X/Y bitmap increments — required to avoid repeating one source pixel across the whole screen.
- Explicit VDP2 VRAM cycle pattern for RGB32768 bitmap fetches.
- VDP1 4bpp **CLUT16** sprite rendering with transparent pen 0.
- Procedural two-frame sprite animation by changing VDP1 character source address.
- Digital Saturn pad input through Yaul's SMPC peripheral layer.
- D-pad actor movement, L/R camera scroll and Start reset.
- VBlank-driven peripheral acquisition and frame updates.
- Python helpers for CPS-style palette conversion and 4bpp pen remapping.
- GitHub Actions build using the Yaul Docker image.

## Hardware validation basis

The architecture represented by this kit was validated on real **Sega Saturn NTSC-U hardware** using a **Terraonion MODE** during development of a private arcade-port prototype on **2026-09-03**.

Validated building blocks:

| Subsystem | Result |
|---|---|
| 352×224 progressive | Hardware validated |
| VDP1 4bpp CLUT16 | Hardware validated |
| VDP2 RGB32768 bitmap | Hardware validated |
| VDP1 + VDP2 composition | Hardware validated |
| SMPC digital pad input | Hardware validated |
| Runtime sprite animation | Hardware validated |
| Runtime background scroll | Hardware validated |
| VBlank-synchronised updates | Hardware validated |

See [`docs/HARDWARE_VALIDATION.md`](docs/HARDWARE_VALIDATION.md) and [`docs/PORTING_COOKBOOK.md`](docs/PORTING_COOKBOOK.md).

## Demo controls

- **D-pad** — move the procedural VDP1 actor.
- **L / R** — scroll the VDP2 background horizontally.
- **A** — flip the actor horizontally while held.
- **Start** — reset actor and camera positions.

The actor also alternates automatically between two procedural animation frames.

## Build with Yaul locally

With a working Yaul installation:

```sh
cd saturn
make -j2
```

Typical outputs include a Saturn `.bin`, `.cue`, `.iso`, ELF and map file.

## Build with Docker

```sh
docker pull ijacquez/yaul:latest
docker run --rm -v "$PWD/saturn:/work" -w /work ijacquez/yaul:latest make -j2
```

The included GitHub Action performs the same build automatically.

## Recommended architecture for a real port

```text
CD-ROM / source ROM extractor
          |
          v
   stage-local asset pack
          |
          v
      Work RAM
       /    \
      v      v
 VDP1 VRAM  VDP2 VRAM
 actors/FX  maps/backgrounds/HUD
```

For a production beat-'em-up or action game, use VDP1 for actors, objects and effects; use VDP2 cell/tile layers for scrolling stages and HUD. The bitmap demo in v0.1.0 is deliberately a simple known-good bootstrap before moving to tile/cell mode.

## Clean-room rule

Keep extraction tooling separate from copyrighted source material. Public repositories should contain code and documentation only. Users should supply any legally obtained source ROMs locally; generated commercial assets and distributable game images should stay outside the public repository unless you have rights to distribute them.

## License

This project is MIT licensed. Yaul/libyaul is a separate project and is also distributed under its own MIT license. No Yaul source code is vendored here.
