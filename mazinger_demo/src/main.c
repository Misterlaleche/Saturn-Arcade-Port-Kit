#include "game.h"
#include "saturn_view.h"

int main(void)
{
    game_state_t game;
    game_input_t input;

    game_reset(&game);
    saturn_view_generate_assets();
    saturn_view_display_enable();

    while (true) {
        saturn_view_read_input(&input);
        game_update(&game, &input);
        saturn_view_render(&game);
    }

    return 0;
}

void user_init(void)
{
    saturn_view_init();
}
