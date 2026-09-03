# Saturn arcade-porting cookbook

This document records practical failure modes discovered while bringing a 2D arcade renderer up on real Saturn hardware.

## 1. Establish a minimal progression

Do not begin with a complete stage. Validate in this order:

1. Boot and 352×224 video.
2. One VDP1 sprite.
3. Two VDP1 sprites and independent CLUTs.
4. VDP2 background + VDP1 composition.
5. Dynamic animation.
6. SMPC input.
7. Scrolling and camera.
8. Only then migrate the stage renderer to cell/tile mode and add more layers.

Keep known-good checkpoints. A static renderer is extremely useful when a later dynamic build breaks.

## 2. VDP2 bitmap: the one-pixel-screen trap

A particularly misleading failure mode is a completely uniform background even though NBG0 is enabled and VRAM contains valid image data.

For NBG0/NBG1, initialise the reduction/increment values:

```c
vdp2_scrn_reduction_x_set(VDP2_SCRN_NBG0, FIX16(1.0f));
vdp2_scrn_reduction_y_set(VDP2_SCRN_NBG0, FIX16(1.0f));
```

If these remain zero, VDP2 may repeatedly sample the same source coordinate. A dark first pixel looks like a black background; a white diagnostic first pixel looks like a white background. This can easily be mistaken for failed VRAM transfers.

## 3. VDP2 RGB32768 writes

For the known-good path:

- use `512×256` RGB32768;
- write pixels as 16-bit RGB555 values;
- use the MSB convention expected by the chosen VDP2 path (the demo uses `RGB1555(1, ...)`);
- supply enough VRAM fetch cycles for 32K-color bitmap data.

The bitmap consumes exactly 262,144 bytes, spanning VRAM A0+A1 in the standard four-bank organisation.

## 4. VDP2 cycle patterns are not optional

32K-color bitmap data requires four accesses. The demo dedicates T0-T3 in A0 and A1 to NBG0 bitmap/character fetches and leaves the remaining slots unused.

When a layer is configured correctly but still renders nothing or stale/unexpected data, inspect the VRAM cycle pattern before rewriting the asset decoder.

## 5. VDP1 CLUT16 conventions

A useful 4bpp convention for arcade conversions is:

```text
source transparent pen -> VDP1 pen 0
source visible pens     -> VDP1 pens 1..15
CLUT[0]                 -> transparent/zero
```

For a 16-color CLUT sprite, disable VDP1's end-code interpretation if source pen values may include `0xF` as ordinary image data after remapping:

```c
vdp1_cmdt_draw_mode_t mode = {
    .color_mode = VDP1_CMDT_CM_CLUT_16,
    .end_code_disable = true,
    .trans_pixel_disable = false
};
```

VDP1 character widths must be a multiple of 8 pixels.

## 6. Update command tables safely

Wait for the prior VDP1 operation before modifying command data that hardware may still be consuming. Perform visible-state changes on frame boundaries and render the next command list after updates.

A useful runtime split is:

```text
input -> simulation -> camera -> animation -> VDP1 command update -> VDP2 scroll update -> render/sync
```

## 7. SMPC input

The public example uses Yaul's peripheral manager rather than custom raw SMPC polling:

```c
static void vblank_out(void *work)
{
    smpc_peripheral_intback_issue();
}

// main loop
smpc_peripheral_process();
smpc_peripheral_digital_port(1, &pad);
```

For continuous movement, Yaul's `pressed` representation is the current button state. Its `held` field is useful for edge-triggered actions in the current API implementation.

## 8. 384-wide arcade worlds on 352-wide Saturn output

Do not automatically scale all sprites horizontally. A better starting strategy for many arcade games is:

- retain the original world coordinate system;
- expose a 352-pixel camera viewport;
- crop/reframe the stage with camera logic;
- keep actor pixels 1:1.

Only introduce resampling when a game specifically requires it.

## 9. Move to VDP2 tile/cell mode for production

Bitmap mode is excellent for proving the renderer. It is usually not the final architecture for a scrolling CPS-like game.

Production direction:

```text
VDP2 NBGx: stage tilemaps, parallax, HUD
VDP1:      actors, enemies, weapons, pickups, effects
```

Benefits include lower VRAM pressure, efficient scrolling, reusable tiles and natural multi-layer parallax.

## 10. Keep copyrighted assets out of public Git

A public porting framework should contain:

- extraction code;
- parsers;
- converters;
- engine code;
- documentation;
- synthetic test assets.

Do not commit commercial ROMs, extracted graphics/audio, derived stage bitmaps or distributable game images unless you have distribution rights.
