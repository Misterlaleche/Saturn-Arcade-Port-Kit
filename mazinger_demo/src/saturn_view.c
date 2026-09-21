#include "saturn_view.h"

#include <assert.h>
#include <string.h>

#define BITMAP_W 512
#define BITMAP_H 256
#define SPR_W 80
#define SPR_H 128
#define SPR_BYTES ((SPR_W * SPR_H) / 2)

#define CMD_SYSTEM_CLIP 0
#define CMD_LOCAL_COORD 1
#define CMD_PLAYER 2
#define CMD_ENEMY0 3
#define CMD_ENEMY1 4
#define CMD_ENEMY2 5
#define CMD_ROCKET 6
#define CMD_EFFECT0 7
#define CMD_EFFECT1 8
#define CMD_END 9
#define CMD_COUNT 10

typedef enum {
    SPR_MAZ_IDLE = 0,
    SPR_MAZ_WALK,
    SPR_MAZ_PUNCH,
    SPR_MAZ_SPECIAL,
    SPR_SOLDIER,
    SPR_DRILL,
    SPR_MISSILE,
    SPR_DOUBLAS_IDLE,
    SPR_DOUBLAS_ATTACK,
    SPR_GARADA_IDLE,
    SPR_GARADA_ATTACK,
    SPR_ROCKET,
    SPR_BREAST,
    SPR_RUST,
    SPR_COUNT
} sprite_id_t;

static vdp1_vram_partitions_t s_vdp1_parts;
static vdp1_cmdt_t *s_cmdts;
static uint8_t s_sprite_bank[SPR_COUNT][SPR_BYTES] __aligned(4);
static vdp1_clut_t s_clut __aligned(32);
static volatile uint16_t * const s_bg = (volatile uint16_t *)VDP2_VRAM_ADDR(0, 0x00000);

static void vblank_out_handler(void *work);
static void background_generate(void);
static void sprites_generate(void);
static void clut_generate(void);
static void commands_init(void);

static inline int iabs_i(int v) { return v < 0 ? -v : v; }
static inline bool rect(int x, int y, int x0, int y0, int x1, int y1)
{
    return x >= x0 && x < x1 && y >= y0 && y < y1;
}

static inline bool diamond(int x, int y, int cx, int cy, int rx, int ry)
{
    return iabs_i(x - cx) * ry + iabs_i(y - cy) * rx <= rx * ry;
}

static inline bool diag_band(int x, int y, int ax, int ay, int bx, int by, int thickness)
{
    const int dx = bx - ax;
    const int dy = by - ay;
    const int px = x - ax;
    const int py = y - ay;
    const int cross = iabs_i(px * dy - py * dx);
    const int dot = px * dx + py * dy;
    const int len2 = dx * dx + dy * dy;
    return dot >= 0 && dot <= len2 && cross <= thickness * (iabs_i(dx) + iabs_i(dy));
}

static uint8_t pen_mazinger(sprite_id_t id, int x, int y)
{
    const int walk = (id == SPR_MAZ_WALK);
    const int punch = (id == SPR_MAZ_PUNCH);
    const int special = (id == SPR_MAZ_SPECIAL);

    if (rect(x,y,33,8,47,28)) return 4;
    if (rect(x,y,37,3,43,11)) return 5;
    if (diag_band(x,y,32,15,16,19,2) || diag_band(x,y,48,15,64,19,2)) return 9;
    if (rect(x,y,35,16,39,19) || rect(x,y,42,16,46,19)) return 9;
    if (rect(x,y,35,23,45,27)) return 2;

    if (diamond(x,y,40,49,24,25)) return 1;
    if (diag_band(x,y,22,34,39,52,4) || diag_band(x,y,58,34,41,52,4)) return 5;
    if (rect(x,y,34,52,46,70)) return 3;
    if (diamond(x,y,40,66,6,9)) return 9;

    if (rect(x,y,14,35,25,72)) return 1;
    if (rect(x,y,11,64,26,79)) return 2;
    if (punch) {
        if (rect(x,y,55,40,78,54)) return 1;
        if (rect(x,y,68,38,80,57)) return 2;
    } else {
        if (rect(x,y,55,35,66,72)) return 1;
        if (rect(x,y,54,64,69,79)) return 2;
    }
    if (special && (rect(x,y,20,43,30,51) || rect(x,y,50,43,60,51))) return 13;

    if (rect(x,y,25 + (walk ? -2 : 0),70,38,111)) return 3;
    if (rect(x,y,43 + (walk ? 2 : 0),70,56,111)) return 3;
    if (rect(x,y,21 + (walk ? -4 : 0),103,39,126)) return 1;
    if (rect(x,y,42 + (walk ? 4 : 0),103,61,126)) return 1;
    return 0;
}

static uint8_t pen_soldier(int x, int y)
{
    if (diamond(x,y,40,18,12,13)) return 14;
    if (rect(x,y,34,16,46,20)) return 9;
    if (diamond(x,y,40,49,24,25)) return 5;
    if (rect(x,y,13,37,25,76) || rect(x,y,55,37,67,76)) return 7;
    if (rect(x,y,24,70,37,116) || rect(x,y,43,70,56,116)) return 7;
    if (rect(x,y,20,106,38,126) || rect(x,y,42,106,61,126)) return 14;
    if (diamond(x,y,17,74,7,11) || diamond(x,y,63,74,7,11)) return 9;
    return 0;
}

static uint8_t pen_drill(int x, int y)
{
    if (diamond(x,y,40,20,12,15)) return 12;
    if (y < 17 && iabs_i(x-40) <= (17-y)/2 + 1) return 3;
    if (rect(x,y,35,18,46,21)) return 5;
    if (diamond(x,y,40,52,24,27)) return 12;
    if (rect(x,y,14,38,25,80)) return 12;
    if (rect(x,y,55,38,65,72)) return 12;
    if (y >= 67 && y < 88 && x >= 58 && x < 78 && iabs_i(y-77) <= (78-x)/2 + 2) return 9;
    if (rect(x,y,24,73,37,116) || rect(x,y,43,73,56,116)) return 6;
    if (rect(x,y,20,108,38,126) || rect(x,y,42,108,61,126)) return 1;
    if (rect(x,y,30,44,50,48)) return 9;
    return 0;
}

static uint8_t pen_missile(int x, int y)
{
    if (diamond(x,y,40,18,12,14)) return 14;
    if (rect(x,y,35,17,46,20)) return 5;
    if (diamond(x,y,40,51,24,27)) return 7;
    if (rect(x,y,15,38,27,76) || rect(x,y,53,38,65,76)) return 7;
    if (rect(x,y,9,48,22,62) || rect(x,y,58,48,71,62)) return 5;
    if (y >= 44 && y < 66 && (x < 14 || x > 66)) return 13;
    if (rect(x,y,24,73,37,116) || rect(x,y,43,73,56,116)) return 14;
    if (rect(x,y,20,108,38,126) || rect(x,y,42,108,61,126)) return 7;
    return 0;
}

static uint8_t pen_doublas(sprite_id_t id, int x, int y)
{
    const bool attack = (id == SPR_DOUBLAS_ATTACK);
    if (rect(x,y,18,9,29,47) || rect(x,y,51,9,62,47)) return 11;
    if ((y % 10) < 2 && ((x >= 18 && x < 29) || (x >= 51 && x < 62))) return 2;
    if (diamond(x,y,22 + (attack ? -3 : 0),11,12,8) || diamond(x,y,58 + (attack ? 3 : 0),11,12,8)) return 11;
    if (rect(x,y,16,10,24,13) || rect(x,y,56,10,64,13)) return 5;
    if (rect(x,y,18,17,28,20) || rect(x,y,52,17,62,20)) return 9;

    if (diamond(x,y,40,55,28,28)) return 10;
    if (rect(x,y,13,45,25,83) || rect(x,y,55,45,67,83)) return 11;
    if (diamond(x,y,18,82,10,10) || diamond(x,y,62,82,10,10)) return 2;
    if (rect(x,y,24,76,37,117) || rect(x,y,43,76,56,117)) return 11;
    if (rect(x,y,20,108,38,127) || rect(x,y,42,108,61,127)) return 2;
    if (diamond(x,y,31,56,6,12) || diamond(x,y,49,56,6,12)) return 9;
    if (rect(x,y,34,68,46,75)) return 5;
    return 0;
}

static uint8_t pen_garada(sprite_id_t id, int x, int y)
{
    const bool attack = (id == SPR_GARADA_ATTACK);
    if (diag_band(x,y,22,42,6 + (attack ? 3 : 0),8,3) ||
        diag_band(x,y,58,42,74 - (attack ? 3 : 0),8,3)) return 4;
    if (diag_band(x,y,22,42,10,10,6) || diag_band(x,y,58,42,70,10,6)) return 1;

    if (diamond(x,y,40,20,12,15)) return 8;
    if (rect(x,y,34,16,39,22) || rect(x,y,42,16,47,22)) return 1;
    if (rect(x,y,37,25,43,29)) return 1;
    if (rect(x,y,34,30,46,39)) return 10;

    if (diamond(x,y,40,55,27,25)) return 5;
    if (rect(x,y,27,45,53,49) || rect(x,y,29,54,51,58) || rect(x,y,31,63,49,67)) return 9;
    if (rect(x,y,13,45,25,84) || rect(x,y,55,45,67,84)) return 8;
    if (diamond(x,y,18,84,10,10) || diamond(x,y,62,84,10,10)) return 8;
    if (rect(x,y,25,74,37,116) || rect(x,y,43,74,55,116)) return 8;
    if (rect(x,y,21,108,38,127) || rect(x,y,42,108,59,127)) return 10;
    return 0;
}

static uint8_t pen_effect(sprite_id_t id, int x, int y)
{
    if (id == SPR_ROCKET) {
        if (rect(x,y,28,52,56,66)) return 14;
        if (rect(x,y,48,48,64,70)) return 3;
        if (diag_band(x,y,12,59,28,59,4)) return 13;
    } else if (id == SPR_BREAST) {
        if (rect(x,y,0,52,80,64)) return ((x/5)&1) ? 9 : 13;
        if (rect(x,y,0,55,80,61)) return 4;
    } else if (id == SPR_RUST) {
        if (rect(x,y,0,47,80,70)) {
            int wave = (x * 3 + y * 5) & 15;
            if (wave < 5) return 8;
            if (wave < 8) return 3;
        }
    }
    return 0;
}

static uint8_t sprite_pen(sprite_id_t id, int x, int y)
{
    switch (id) {
        case SPR_MAZ_IDLE:
        case SPR_MAZ_WALK:
        case SPR_MAZ_PUNCH:
        case SPR_MAZ_SPECIAL: return pen_mazinger(id, x, y);
        case SPR_SOLDIER: return pen_soldier(x,y);
        case SPR_DRILL: return pen_drill(x,y);
        case SPR_MISSILE: return pen_missile(x,y);
        case SPR_DOUBLAS_IDLE:
        case SPR_DOUBLAS_ATTACK: return pen_doublas(id,x,y);
        case SPR_GARADA_IDLE:
        case SPR_GARADA_ATTACK: return pen_garada(id,x,y);
        case SPR_ROCKET:
        case SPR_BREAST:
        case SPR_RUST: return pen_effect(id,x,y);
        default: return 0;
    }
}

static void sprites_generate(void)
{
    for (int sid = 0; sid < SPR_COUNT; sid++) {
        uint32_t out = 0;
        for (int y = 0; y < SPR_H; y++) {
            for (int x = 0; x < SPR_W; x += 2) {
                uint8_t a = sprite_pen((sprite_id_t)sid, x, y) & 0x0F;
                uint8_t b = sprite_pen((sprite_id_t)sid, x + 1, y) & 0x0F;
                s_sprite_bank[sid][out++] = (uint8_t)((a << 4) | b);
            }
        }
        assert(out == SPR_BYTES);
    }
}

static void clut_generate(void)
{
    memset(&s_clut, 0, sizeof(s_clut));
    s_clut.colors[0]  = RGB1555(0, 0, 0, 0);
    s_clut.colors[1]  = RGB1555(1, 2, 2, 4);
    s_clut.colors[2]  = RGB1555(1, 8, 9, 11);
    s_clut.colors[3]  = RGB1555(1, 20, 21, 22);
    s_clut.colors[4]  = RGB1555(1, 30, 30, 30);
    s_clut.colors[5]  = RGB1555(1, 28, 3, 5);
    s_clut.colors[6]  = RGB1555(1, 14, 5, 7);
    s_clut.colors[7]  = RGB1555(1, 3, 9, 24);
    s_clut.colors[8]  = RGB1555(1, 17, 21, 31);
    s_clut.colors[9]  = RGB1555(1, 31, 24, 3);
    s_clut.colors[10] = RGB1555(1, 14, 7, 22);
    s_clut.colors[11] = RGB1555(1, 3, 18, 18);
    s_clut.colors[12] = RGB1555(1, 5, 17, 7);
    s_clut.colors[13] = RGB1555(1, 31, 10, 2);
    s_clut.colors[14] = RGB1555(1, 3, 5, 12);
    s_clut.colors[15] = RGB1555(1, 25, 10, 21);
}

static void background_generate(void)
{
    for (int y = 0; y < BITMAP_H; y++) {
        for (int x = 0; x < BITMAP_W; x++) {
            uint8_t r, g, b;
            if (y < 104) {
                r = (uint8_t)(4 + y / 18);
                g = (uint8_t)(5 + y / 24);
                b = (uint8_t)(15 + y / 10);
                if (b > 30) b = 30;
            } else if (y < 178) {
                int block = x / 28;
                int h = 18 + ((block * 17 + 11) % 54);
                int top = 178 - h;
                if (y < top) {
                    r = 8; g = 8; b = 14;
                } else {
                    r = (uint8_t)(4 + (block & 3));
                    g = (uint8_t)(5 + ((block >> 1) & 3));
                    b = (uint8_t)(8 + (block & 1) * 3);
                    if (((x + 5) % 14) < 4 && ((y + 2) % 16) < 6) {
                        r = 27; g = 17; b = 4;
                    }
                }
            } else {
                int stripe = ((x / 24) + (y / 7)) & 3;
                r = (uint8_t)(5 + stripe);
                g = (uint8_t)(6 + stripe);
                b = (uint8_t)(7 + stripe);
            }

            if (y >= 74 && y < 128) {
                int mx = iabs_i((x % 256) - 128);
                int ridge = 76 + mx / 3;
                if (y >= ridge) {
                    r = 6; g = 7; b = 13;
                }
            }

            if (y >= 198 && ((x + y) % 52) < 4) {
                r = 19; g = 17; b = 10;
            }

            s_bg[y * BITMAP_W + x] = RGB1555(1, r, g, b).raw;
        }
    }
}

static void commands_init(void)
{
    s_cmdts = (vdp1_cmdt_t *)s_vdp1_parts.cmdt_base;
    memset(s_cmdts, 0, sizeof(vdp1_cmdt_t) * CMD_COUNT);

    const int16_vec2_t system_clip = INT16_VEC2_INITIALIZER(GAME_SCREEN_W - 1, GAME_SCREEN_H - 1);
    const int16_vec2_t local_coord = INT16_VEC2_INITIALIZER(0, 0);
    vdp1_cmdt_system_clip_coord_set(&s_cmdts[CMD_SYSTEM_CLIP]);
    vdp1_cmdt_vtx_system_clip_coord_set(&s_cmdts[CMD_SYSTEM_CLIP], system_clip);
    vdp1_cmdt_local_coord_set(&s_cmdts[CMD_LOCAL_COORD]);
    vdp1_cmdt_vtx_local_coord_set(&s_cmdts[CMD_LOCAL_COORD], local_coord);

    const vdp1_cmdt_draw_mode_t draw_mode = {
        .color_mode = VDP1_CMDT_CM_CLUT_16,
        .end_code_disable = true,
        .trans_pixel_disable = false
    };

    for (int i = CMD_PLAYER; i <= CMD_EFFECT1; i++) {
        vdp1_cmdt_normal_sprite_set(&s_cmdts[i]);
        vdp1_cmdt_draw_mode_set(&s_cmdts[i], draw_mode);
        vdp1_cmdt_color_mode1_set(&s_cmdts[i], (vdp1_vram_t)s_vdp1_parts.clut_base);
        vdp1_cmdt_char_size_set(&s_cmdts[i], SPR_W, SPR_H);
        vdp1_cmdt_char_base_set(&s_cmdts[i], (vdp1_vram_t)s_vdp1_parts.texture_base);
        s_cmdts[i].cmd_xa = -160;
        s_cmdts[i].cmd_ya = -160;
    }
    vdp1_cmdt_end_set(&s_cmdts[CMD_END]);
}

void saturn_view_init(void)
{
    smpc_peripheral_init();
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
    vdp2_scrn_scroll_x_set(VDP2_SCRN_NBG0, FIX16(0.0f));
    vdp2_scrn_scroll_y_set(VDP2_SCRN_NBG0, FIX16(0.0f));
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
    vdp2_scrn_back_color_set(VDP2_VRAM_ADDR(3, 0x01FFFE), RGB1555(1,1,1,2));
    vdp2_scrn_display_set(VDP2_SCRN_DISP_NBG0);

    vdp1_env_default_set();
    vdp2_sprite_priority_set(0, 6);
    vdp1_vram_partitions_set(32, 0x00020000, 16, 16);
    vdp1_vram_partitions_get(&s_vdp1_parts);
    vdp1_sync_interval_set(0);
    vdp_sync_vblank_out_set(vblank_out_handler, NULL);
}

void saturn_view_generate_assets(void)
{
    background_generate();
    sprites_generate();
    clut_generate();

    scu_dma_transfer(0, s_vdp1_parts.texture_base, s_sprite_bank, sizeof(s_sprite_bank));
    scu_dma_transfer_wait(0);
    scu_dma_transfer(0, s_vdp1_parts.clut_base, &s_clut, sizeof(s_clut));
    scu_dma_transfer_wait(0);
    commands_init();
}

void saturn_view_display_enable(void)
{
    vdp2_tvmd_display_set();
    vdp2_sync();
    vdp2_sync_wait();
}

void saturn_view_read_input(game_input_t *input)
{
    smpc_peripheral_digital_t d;
    memset(input, 0, sizeof(*input));
    smpc_peripheral_process();
    smpc_peripheral_digital_port(1, &d);

    input->left = d.pressed.button.left != 0;
    input->right = d.pressed.button.right != 0;
    input->punch = d.held.button.a != 0;
    input->heavy = d.held.button.b != 0;
    input->rocket = d.held.button.c != 0;
    input->breast = d.held.button.x != 0;
    input->rust = d.held.button.y != 0;
    input->start = d.held.button.start != 0;
}

static sprite_id_t enemy_sprite(const enemy_t *e)
{
    switch (e->kind) {
        case ENEMY_SOLDIER: return SPR_SOLDIER;
        case ENEMY_DRILL: return SPR_DRILL;
        case ENEMY_MISSILE: return SPR_MISSILE;
        case ENEMY_DOUBLAS: return e->attack_timer > 0 ? SPR_DOUBLAS_ATTACK : SPR_DOUBLAS_IDLE;
        case ENEMY_GARADA: return e->attack_timer > 0 ? SPR_GARADA_ATTACK : SPR_GARADA_IDLE;
        default: return SPR_SOLDIER;
    }
}

static void set_sprite_cmd(int cmd, sprite_id_t sid, int x, int y, bool flip)
{
    vdp1_cmdt_char_base_set(&s_cmdts[cmd],
        (vdp1_vram_t)s_vdp1_parts.texture_base + ((uint32_t)sid * SPR_BYTES));
    vdp1_cmdt_char_flip_set(&s_cmdts[cmd], flip ? VDP1_CMDT_CHAR_FLIP_H : VDP1_CMDT_CHAR_FLIP_NONE);
    s_cmdts[cmd].cmd_xa = (int16_t)x;
    s_cmdts[cmd].cmd_ya = (int16_t)y;
}

static void hide_cmd(int cmd)
{
    s_cmdts[cmd].cmd_xa = -160;
    s_cmdts[cmd].cmd_ya = -160;
}

static void hud_rect(int scroll, int x0, int y0, int x1, int y1, rgb1555_t color)
{
    if (x0 < 0) x0 = 0;
    if (x1 > GAME_SCREEN_W) x1 = GAME_SCREEN_W;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            s_bg[y * BITMAP_W + scroll + x] = color.raw;
}

static void hud_draw(const game_state_t *g, int scroll)
{
    const rgb1555_t dark = RGB1555(1,1,2,5);
    const rgb1555_t edge = RGB1555(1,18,20,24);
    const rgb1555_t life = RGB1555(1,28,4,4);
    const rgb1555_t energy = RGB1555(1,3,18,31);
    const rgb1555_t enemy = RGB1555(1,24,8,20);
    const rgb1555_t clearc = RGB1555(1,4,22,8);
    const rgb1555_t overc = RGB1555(1,24,3,3);

    hud_rect(scroll, 0, 0, GAME_SCREEN_W, 24, dark);
    hud_rect(scroll, 7, 5, 131, 11, edge);
    hud_rect(scroll, 9, 7, 9 + (120 * g->player_hp / 180), 9, life);
    hud_rect(scroll, 7, 14, 111, 20, edge);
    hud_rect(scroll, 9, 16, 9 + (100 * g->player_energy / 100), 18, energy);

    const enemy_t *pe = game_primary_enemy(g);
    if (pe) {
        hud_rect(scroll, 214, 5, 345, 13, edge);
        int w = 127 * pe->hp / pe->hp_max;
        if (w < 0) w = 0;
        hud_rect(scroll, 216, 7, 216 + w, 11, enemy);
    }

    int progress = 120 * g->camera_x / GAME_STAGE_END;
    hud_rect(scroll, 116, 16, 240, 20, edge);
    hud_rect(scroll, 118, 17, 118 + progress, 19, RGB1555(1,22,18,4));

    if (g->stage_clear)
        hud_rect(scroll, 134, 4, 210, 14, clearc);
    if (g->game_over)
        hud_rect(scroll, 134, 4, 210, 14, overc);
}

void saturn_view_render(const game_state_t *g)
{
    vdp1_sync_wait();

    int psx = game_player_screen_x(g);
    sprite_id_t player_sid = ((g->frame / 10) & 1) ? SPR_MAZ_WALK : SPR_MAZ_IDLE;
    if (g->attack == ATTACK_PUNCH || g->attack == ATTACK_HEAVY)
        player_sid = SPR_MAZ_PUNCH;
    else if (g->attack != ATTACK_NONE)
        player_sid = SPR_MAZ_SPECIAL;

    if (!(g->player_flash > 0 && (g->frame & 1)))
        set_sprite_cmd(CMD_PLAYER, player_sid, psx - 40, GAME_GROUND_Y - SPR_H, g->player_facing_left);
    else
        hide_cmd(CMD_PLAYER);

    for (int i = 0; i < GAME_MAX_ENEMIES; i++) {
        const enemy_t *e = &g->enemies[i];
        int cmd = CMD_ENEMY0 + i;
        int sx = game_enemy_screen_x(g, i);
        if (e->active && sx > -SPR_W && sx < GAME_SCREEN_W + SPR_W && !(e->flash > 0 && (g->frame & 1)))
            set_sprite_cmd(cmd, enemy_sprite(e), sx - 40, GAME_GROUND_Y - SPR_H, !e->facing_left);
        else
            hide_cmd(cmd);
    }

    if (g->rocket.active) {
        int rx = g->rocket.x - g->camera_x - 40;
        set_sprite_cmd(CMD_ROCKET, SPR_ROCKET, rx, GAME_GROUND_Y - SPR_H, g->player_facing_left);
    } else {
        hide_cmd(CMD_ROCKET);
    }

    hide_cmd(CMD_EFFECT0);
    hide_cmd(CMD_EFFECT1);
    if (g->attack == ATTACK_BREAST && g->attack_age >= 11 && g->attack_age <= 21) {
        int dir = g->player_facing_left ? -1 : 1;
        set_sprite_cmd(CMD_EFFECT0, SPR_BREAST, psx - 40 + dir * 64, GAME_GROUND_Y - SPR_H, g->player_facing_left);
        set_sprite_cmd(CMD_EFFECT1, SPR_BREAST, psx - 40 + dir * 132, GAME_GROUND_Y - SPR_H, g->player_facing_left);
    } else if (g->attack == ATTACK_RUST && g->attack_age >= 9 && g->attack_age <= 20) {
        int dir = g->player_facing_left ? -1 : 1;
        set_sprite_cmd(CMD_EFFECT0, SPR_RUST, psx - 40 + dir * 58, GAME_GROUND_Y - SPR_H, g->player_facing_left);
        set_sprite_cmd(CMD_EFFECT1, SPR_RUST, psx - 40 + dir * 118, GAME_GROUND_Y - SPR_H, g->player_facing_left);
    }

    const int scroll = g->camera_x % (BITMAP_W - GAME_SCREEN_W + 1);
    hud_draw(g, scroll);
    vdp2_scrn_scroll_x_set(VDP2_SCRN_NBG0, FIX16((float)scroll));

    vdp1_sync_render();
    vdp1_sync();
    vdp2_sync();
}

static void vblank_out_handler(void *work __unused)
{
    smpc_peripheral_intback_issue();
}
