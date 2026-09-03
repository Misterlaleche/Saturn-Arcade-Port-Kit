# Architecture

## v0.1.0 demo

```text
                   +---------------------+
Saturn pad ------> | SMPC peripheral API |
                   +----------+----------+
                              |
                              v
                   +---------------------+
                   | frame/game state    |
                   | actor + camera      |
                   +-----+----------+----+
                         |          |
                  actor  |          | camera
                         v          v
                   +---------+  +---------+
                   |  VDP1   |  |  VDP2   |
                   | CLUT16  |  | NBG0    |
                   | sprites |  | RGB555  |
                   +----+----+  +----+----+
                        \          /
                         \        /
                          v      v
                         Saturn output
                           352x224p
```

## Production-port direction

The bitmap background is intentionally a bootstrap. A real port should usually replace it with VDP2 cell/tile layers and stage-local asset packs.

Suggested logical modules:

```text
rom_source/
extractor/
asset_pack/
world/
actors/
animation/
collision/
camera/
renderer_vdp1/
renderer_vdp2/
audio_scsp/
input_smpc/
```

Keep game-specific decoding separate from the Saturn renderer so the renderer can be reused for another arcade board or console source.
