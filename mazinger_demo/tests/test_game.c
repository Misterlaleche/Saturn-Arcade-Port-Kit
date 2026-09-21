#include "game.h"
#include <assert.h>
#include <stdio.h>

static void tick(game_state_t *g, game_input_t in, int frames)
{
    for (int i = 0; i < frames; i++)
        game_update(g, &in);
}

int main(void)
{
    game_state_t g;
    game_reset(&g);
    assert(g.player_hp == 180);
    assert(g.player_energy == 100);
    assert(g.event_index == 0);

    game_input_t right = {0};
    right.right = true;
    tick(&g, right, 100);
    assert(g.encounter_locked);
    assert(game_active_enemy_count(&g) >= 1);

    for (int i = 0; i < GAME_MAX_ENEMIES; i++)
        g.enemies[i].active = false;
    game_input_t none = {0};
    game_update(&g, &none);
    assert(!g.encounter_locked);
    assert(g.event_index == 1);

    game_input_t breast = {0};
    breast.breast = true;
    game_update(&g, &breast);
    assert(g.player_energy == 75);
    assert(g.attack == ATTACK_BREAST);

    game_input_t start = {0};
    start.start = true;
    game_update(&g, &start);
    assert(g.camera_x == 0);
    assert(g.player_hp == 180);
    assert(!g.stage_clear && !g.game_over);

    puts("game logic tests: PASS");
    return 0;
}
