#include "portkit.h"

#include <assert.h>
#include <string.h>

#define CMD_SYSTEM_CLIP 0
#define CMD_LOCAL_COORD 1
#define CMD_SPRITE      2
#define CMD_END         3
#define CMD_COUNT       4

static vdp1_vram_partitions_t _vdp1_parts;
static vdp1_cmdt_t *_cmdts;
static uint8_t _sprite_frames[PORTKIT_SPRITE_FRAMES][PORTKIT_SPRITE_BYTES] __aligned(4);
static vdp1_clut_t _sprite_clut __aligned(32);

static void _vblank_out_handler(void *work);
static void _background_generate(void);
static void _sprite_frames_generate(void);
static void _sprite_clut_generate(void);
static void _vdp1_commands_init(void);

static inline uint8_t
_sprite_pen(uint32_t frame, int x, int y)
{
    const int cx = PORTKIT_SPRITE_WIDTH / 2;
    const int top = 8;
    const int bottom = 58;

    /* Transparent outside a simple procedural arcade-actor silhouette. */
    if (y < top || y >= bottom)
        return 0;

    /* Head */
    if (y < 22) {
        if (x >= cx - 10 && x < cx + 10)
            return (y < 12) ? 5 : 4;
        return 0;
    }

    /* Torso */
    if (y < 44 && x >= cx - 13 && x < cx + 13)
        return ((x + y) & 4) ? 2 : 3;

    /* Arms: frame 1 raises the right arm. */
    if (frame == 0) {
        if (y >= 27 && y < 39 && ((x >= cx - 22 && x < cx - 13) ||
                                  (x >= cx + 13 && x < cx + 22)))
            return 6;
    } else {
        if ((x >= cx - 22 && x < cx - 13 && y >= 27 && y < 39) ||
            (x >= cx + 13 && x < cx + 22 && y >= 18 && y < 30))
            return 6;
    }

    /* Legs */
    if (y >= 44) {
        if (x >= cx - 12 && x < cx - 2)
            return (frame == 0 || y < 53) ? 7 : 8;
        if (x >= cx + 2 && x < cx + 12)
            return (frame == 0 || y < 53) ? 8 : 7;
    }

    /* Eyes/visor. */
    if (y >= 14 && y < 17 && x >= cx - 7 && x < cx + 7)
        return 10;

    return 0;
}

static void
_background_generate(void)
{
    volatile uint16_t * const dst =
        (volatile uint16_t *)VDP2_VRAM_ADDR(0, 0x00000);

    for (int y = 0; y < PORTKIT_BITMAP_HEIGHT; y++) {
        for (int x = 0; x < PORTKIT_BITMAP_WIDTH; x++) {
            uint8_t r;
            uint8_t g;
            uint8_t b;

            if (y < 86) {
                /* Sky gradient. */
                r = 4 + (y >> 4);
                g = 5 + (y >> 5);
                b = 11 + (y >> 3);
            } else if (y < 184) {
                /* Repeating industrial blocks/windows, intentionally scroll-visible. */
                const int block = (x / 48) & 3;
                r = 5 + block * 3;
                g = 5 + ((x / 16) & 3);
                b = 7 + ((y / 16) & 3);

                if (((x + 9) % 48) < 8 && ((y + 3) % 28) < 10) {
                    r = 23;
                    g = 18;
                    b = 7;
                }
            } else {
                /* Ground with perspective-ish horizontal bands. */
                const int stripe = ((x / 24) + (y / 8)) & 3;
                r = 3 + stripe;
                g = 8 + stripe * 2;
                b = 7 + stripe;
            }

            /* Vertical markers make camera movement obvious. */
            if ((x & 63) == 0) {
                r = 25;
                g = 8;
                b = 5;
            }

            dst[y * PORTKIT_BITMAP_WIDTH + x] = RGB1555(1, r, g, b).raw;
        }
    }
}

static void
_sprite_frames_generate(void)
{
    for (uint32_t frame = 0; frame < PORTKIT_SPRITE_FRAMES; frame++) {
        uint8_t * const dst = _sprite_frames[frame];
        uint32_t out = 0;

        for (int y = 0; y < PORTKIT_SPRITE_HEIGHT; y++) {
            for (int x = 0; x < PORTKIT_SPRITE_WIDTH; x += 2) {
                const uint8_t p0 = _sprite_pen(frame, x, y) & 0x0F;
                const uint8_t p1 = _sprite_pen(frame, x + 1, y) & 0x0F;
                dst[out++] = (p0 << 4) | p1;
            }
        }
        assert(out == PORTKIT_SPRITE_BYTES);
    }
}

static void
_sprite_clut_generate(void)
{
    memset(&_sprite_clut, 0, sizeof(_sprite_clut));

    _sprite_clut.colors[0]  = RGB1555(0, 0, 0, 0);   /* transparent pen */
    _sprite_clut.colors[1]  = RGB1555(1, 5, 7, 10);
    _sprite_clut.colors[2]  = RGB1555(1, 7, 12, 24);
    _sprite_clut.colors[3]  = RGB1555(1, 10, 17, 30);
    _sprite_clut.colors[4]  = RGB1555(1, 22, 17, 10);
    _sprite_clut.colors[5]  = RGB1555(1, 28, 22, 14);
    _sprite_clut.colors[6]  = RGB1555(1, 16, 19, 22);
    _sprite_clut.colors[7]  = RGB1555(1, 8, 22, 12);
    _sprite_clut.colors[8]  = RGB1555(1, 5, 14, 8);
    _sprite_clut.colors[9]  = RGB1555(1, 18, 5, 6);
    _sprite_clut.colors[10] = RGB1555(1, 31, 6, 5);
    _sprite_clut.colors[11] = RGB1555(1, 24, 24, 24);
    _sprite_clut.colors[12] = RGB1555(1, 14, 14, 16);
    _sprite_clut.colors[13] = RGB1555(1, 9, 9, 11);
    _sprite_clut.colors[14] = RGB1555(1, 30, 30, 30);
    _sprite_clut.colors[15] = RGB1555(1, 18, 10, 28);
}

static void
_vdp1_commands_init(void)
{
    _cmdts = (vdp1_cmdt_t *)_vdp1_parts.cmdt_base;
    memset(_cmdts, 0, sizeof(vdp1_cmdt_t) * CMD_COUNT);

    const int16_vec2_t system_clip =
        INT16_VEC2_INITIALIZER(PORTKIT_SCREEN_WIDTH - 1, PORTKIT_SCREEN_HEIGHT - 1);
    const int16_vec2_t local_coord = INT16_VEC2_INITIALIZER(0, 0);

    vdp1_cmdt_system_clip_coord_set(&_cmdts[CMD_SYSTEM_CLIP]);
    vdp1_cmdt_vtx_system_clip_coord_set(&_cmdts[CMD_SYSTEM_CLIP], system_clip);

    vdp1_cmdt_local_coord_set(&_cmdts[CMD_LOCAL_COORD]);
    vdp1_cmdt_vtx_local_coord_set(&_cmdts[CMD_LOCAL_COORD], local_coord);

    const vdp1_cmdt_draw_mode_t draw_mode = {
        .color_mode = VDP1_CMDT_CM_CLUT_16,
        .end_code_disable = true,
        .trans_pixel_disable = false
    };

    vdp1_cmdt_normal_sprite_set(&_cmdts[CMD_SPRITE]);
    vdp1_cmdt_draw_mode_set(&_cmdts[CMD_SPRITE], draw_mode);
    vdp1_cmdt_color_mode1_set(&_cmdts[CMD_SPRITE],
        (vdp1_vram_t)_vdp1_parts.clut_base);
    vdp1_cmdt_char_size_set(&_cmdts[CMD_SPRITE],
        PORTKIT_SPRITE_WIDTH, PORTKIT_SPRITE_HEIGHT);
    vdp1_cmdt_char_base_set(&_cmdts[CMD_SPRITE],
        (vdp1_vram_t)_vdp1_parts.texture_base);

    vdp1_cmdt_end_set(&_cmdts[CMD_END]);
}

void
portkit_init(void)
{
    smpc_peripheral_init();

    /* Yaul automatically switches to the correct 28 MHz clock for 352-dot mode. */
    vdp2_tvmd_display_res_set(VDP2_TVMD_INTERLACE_NONE,
        VDP2_TVMD_HORZ_NORMAL_B, VDP2_TVMD_VERT_224);

    const vdp2_scrn_bitmap_format_t bitmap_format = {
        .scroll_screen = VDP2_SCRN_NBG0,
        .ccc = VDP2_SCRN_CCC_RGB_32768,
        .bitmap_size = VDP2_SCRN_BITMAP_SIZE_512X256,
        .palette_base = 0x00000000,
        .bitmap_base = VDP2_VRAM_ADDR(0, 0x00000)
    };

    vdp2_scrn_bitmap_format_set(&bitmap_format);
    vdp2_scrn_priority_set(VDP2_SCRN_NBG0, 3);
    vdp2_scrn_scroll_x_set(VDP2_SCRN_NBG0, FIX16(32.0f));
    vdp2_scrn_scroll_y_set(VDP2_SCRN_NBG0, FIX16(16.0f));

    /* Critical: zero means "sample one coordinate repeatedly" rather than 1:1. */
    vdp2_scrn_reduction_x_set(VDP2_SCRN_NBG0, FIX16(1.0f));
    vdp2_scrn_reduction_y_set(VDP2_SCRN_NBG0, FIX16(1.0f));

    const vdp2_vram_cycp_t cycp = {
        .pt[0].t0 = VDP2_VRAM_CYCP_CHPNDR_NBG0,
        .pt[0].t1 = VDP2_VRAM_CYCP_CHPNDR_NBG0,
        .pt[0].t2 = VDP2_VRAM_CYCP_CHPNDR_NBG0,
        .pt[0].t3 = VDP2_VRAM_CYCP_CHPNDR_NBG0,
        .pt[0].t4 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[0].t5 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[0].t6 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[0].t7 = VDP2_VRAM_CYCP_NO_ACCESS,

        .pt[1].t0 = VDP2_VRAM_CYCP_CHPNDR_NBG0,
        .pt[1].t1 = VDP2_VRAM_CYCP_CHPNDR_NBG0,
        .pt[1].t2 = VDP2_VRAM_CYCP_CHPNDR_NBG0,
        .pt[1].t3 = VDP2_VRAM_CYCP_CHPNDR_NBG0,
        .pt[1].t4 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[1].t5 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[1].t6 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[1].t7 = VDP2_VRAM_CYCP_NO_ACCESS,

        .pt[2].t0 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[2].t1 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[2].t2 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[2].t3 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[2].t4 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[2].t5 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[2].t6 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[2].t7 = VDP2_VRAM_CYCP_NO_ACCESS,

        .pt[3].t0 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[3].t1 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[3].t2 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[3].t3 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[3].t4 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[3].t5 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[3].t6 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[3].t7 = VDP2_VRAM_CYCP_NO_ACCESS
    };
    vdp2_vram_cycp_set(&cycp);

    vdp2_scrn_back_color_set(VDP2_VRAM_ADDR(3, 0x01FFFE),
        RGB1555(1, 1, 1, 2));
    vdp2_scrn_display_set(VDP2_SCRN_DISP_NBG0);

    vdp1_env_default_set();
    vdp2_sprite_priority_set(0, 6);

    /* Small explicit partitions are plenty for the procedural demo. */
    vdp1_vram_partitions_set(32, 0x00010000, 16, 16);
    vdp1_vram_partitions_get(&_vdp1_parts);
    vdp1_sync_interval_set(0);

    vdp_sync_vblank_out_set(_vblank_out_handler, NULL);

}

void
portkit_assets_generate(void)
{
    _background_generate();
    _sprite_frames_generate();
    _sprite_clut_generate();

    const uint32_t texture_bytes = PORTKIT_SPRITE_BYTES * PORTKIT_SPRITE_FRAMES;
    scu_dma_transfer(0, _vdp1_parts.texture_base, _sprite_frames, texture_bytes);
    scu_dma_transfer_wait(0);

    scu_dma_transfer(0, _vdp1_parts.clut_base, &_sprite_clut, sizeof(_sprite_clut));
    scu_dma_transfer_wait(0);

    _vdp1_commands_init();
}

void
portkit_display_enable(void)
{
    vdp2_tvmd_display_set();
    vdp2_sync();
    vdp2_sync_wait();
}

void
portkit_input_read(smpc_peripheral_digital_t *digital)
{
    smpc_peripheral_process();
    smpc_peripheral_digital_port(1, digital);
}

void
portkit_state_reset(portkit_state_t *state)
{
    state->actor_x = 80;
    state->actor_y = PORTKIT_SCREEN_HEIGHT - PORTKIT_SPRITE_HEIGHT - 16;
    state->camera_x = 32;
    state->frame_counter = 0;
    state->flip_x = false;
}

void
portkit_state_update(portkit_state_t *state, const smpc_peripheral_digital_t *digital)
{
    state->frame_counter++;

    if (digital->pressed.button.left)
        state->actor_x -= 2;
    if (digital->pressed.button.right)
        state->actor_x += 2;
    if (digital->pressed.button.up)
        state->actor_y -= 1;
    if (digital->pressed.button.down)
        state->actor_y += 1;

    if (digital->pressed.button.l)
        state->camera_x -= 2;
    if (digital->pressed.button.r)
        state->camera_x += 2;

    state->flip_x = digital->pressed.button.a != 0;

    if (digital->held.button.start)
        portkit_state_reset(state);

    if (state->actor_x < 0)
        state->actor_x = 0;
    if (state->actor_x > PORTKIT_SCREEN_WIDTH - PORTKIT_SPRITE_WIDTH)
        state->actor_x = PORTKIT_SCREEN_WIDTH - PORTKIT_SPRITE_WIDTH;

    if (state->actor_y < 0)
        state->actor_y = 0;
    if (state->actor_y > PORTKIT_SCREEN_HEIGHT - PORTKIT_SPRITE_HEIGHT)
        state->actor_y = PORTKIT_SCREEN_HEIGHT - PORTKIT_SPRITE_HEIGHT;

    if (state->camera_x < 0)
        state->camera_x = 0;
    if (state->camera_x > PORTKIT_BITMAP_WIDTH - PORTKIT_SCREEN_WIDTH)
        state->camera_x = PORTKIT_BITMAP_WIDTH - PORTKIT_SCREEN_WIDTH;
}

void
portkit_render(portkit_state_t *state)
{
    /* Do not edit command VRAM while VDP1 may still be consuming it. */
    vdp1_sync_wait();

    const uint32_t frame = (state->frame_counter / 12) & 1;
    const vdp1_vram_t texture =
        (vdp1_vram_t)_vdp1_parts.texture_base + (frame * PORTKIT_SPRITE_BYTES);

    vdp1_cmdt_char_base_set(&_cmdts[CMD_SPRITE], texture);
    vdp1_cmdt_char_flip_set(&_cmdts[CMD_SPRITE],
        state->flip_x ? VDP1_CMDT_CHAR_FLIP_H : VDP1_CMDT_CHAR_FLIP_NONE);
    _cmdts[CMD_SPRITE].cmd_xa = state->actor_x;
    _cmdts[CMD_SPRITE].cmd_ya = state->actor_y;

    vdp2_scrn_scroll_x_set(VDP2_SCRN_NBG0, FIX16(state->camera_x));

    vdp1_sync_render();
    vdp1_sync();
    vdp2_sync();
}

static void
_vblank_out_handler(void *work __unused)
{
    smpc_peripheral_intback_issue();
}
