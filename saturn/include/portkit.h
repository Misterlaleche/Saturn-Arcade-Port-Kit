#ifndef SATURN_ARCADE_PORT_KIT_H
#define SATURN_ARCADE_PORT_KIT_H

#include <yaul.h>

#include <stdbool.h>
#include <stdint.h>

#define PORTKIT_SCREEN_WIDTH   352
#define PORTKIT_SCREEN_HEIGHT  224
#define PORTKIT_BITMAP_WIDTH   512
#define PORTKIT_BITMAP_HEIGHT  256
#define PORTKIT_SPRITE_WIDTH    64
#define PORTKIT_SPRITE_HEIGHT   64
#define PORTKIT_SPRITE_BYTES  ((PORTKIT_SPRITE_WIDTH * PORTKIT_SPRITE_HEIGHT) / 2)
#define PORTKIT_SPRITE_FRAMES    2

typedef struct portkit_state {
    int16_t actor_x;
    int16_t actor_y;
    int16_t camera_x;
    uint32_t frame_counter;
    bool flip_x;
} portkit_state_t;

void portkit_init(void);
void portkit_assets_generate(void);
void portkit_display_enable(void);
void portkit_input_read(smpc_peripheral_digital_t *digital);
void portkit_state_reset(portkit_state_t *state);
void portkit_state_update(portkit_state_t *state, const smpc_peripheral_digital_t *digital);
void portkit_render(portkit_state_t *state);

#endif
