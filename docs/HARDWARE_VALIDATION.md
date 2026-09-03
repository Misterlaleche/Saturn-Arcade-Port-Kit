# Hardware validation record

## Baseline

- Target: Sega Saturn NTSC-U
- Optical device: Terraonion MODE
- Date: 2026-09-03
- Display mode: 352×224 progressive
- RAM expansion cartridge: not required

## Proven subsystems

### Video timing

352×224 progressive is selected through Yaul using:

```c
vdp2_tvmd_display_res_set(
    VDP2_TVMD_INTERLACE_NONE,
    VDP2_TVMD_HORZ_NORMAL_B,
    VDP2_TVMD_VERT_224);
```

Yaul selects the required 28 MHz system clock when `HORZ_NORMAL_B` is requested.

### VDP1

The validated renderer uses:

- 4bpp textures.
- 16-entry CLUT in VDP1 VRAM.
- transparent pen 0.
- VDP1 sprite priority above the VDP2 background.
- runtime source-address changes for animation.
- runtime X/Y command updates.

### VDP2

A `512×256` RGB32768 NBG0 bitmap was displayed behind VDP1 actors.

Critical settings:

- four bitmap/character fetch opportunities per VRAM bank used by the 16-bit bitmap;
- 16-bit RGB555 pixel writes;
- X and Y reduction/increment registers set to exactly 1.0;
- NBG0 display enable and non-zero priority.

### SMPC

Digital Saturn pad input was validated for movement and buttons. The public demo uses Yaul's peripheral subsystem and issues input acquisition from the VBlank-out callback.

### Dynamic frame loop

Validated simultaneously:

- animated VDP1 actors;
- D-pad position changes;
- VDP2 horizontal scrolling;
- VBlank-synchronised updates.

## Why this checkpoint matters

This is a reusable minimum viable renderer/control loop for many 2D arcade ports. It removes uncertainty around boot, 352-dot video, VDP1/VDP2 composition and controller acquisition before game-specific reverse engineering begins.
