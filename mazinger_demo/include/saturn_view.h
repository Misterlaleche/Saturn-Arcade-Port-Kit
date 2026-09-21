#ifndef MAZINGER_SATURN_VIEW_H
#define MAZINGER_SATURN_VIEW_H

#include "game.h"
#include <yaul.h>

void saturn_view_init(void);
void saturn_view_generate_assets(void);
void saturn_view_display_enable(void);
void saturn_view_read_input(game_input_t *input);
void saturn_view_render(const game_state_t *game);

#endif
