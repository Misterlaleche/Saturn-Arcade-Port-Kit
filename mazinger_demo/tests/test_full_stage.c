#include "game.h"
#include <stdio.h>

int main(void)
{
    game_state_t g;
    game_reset(&g);

    for (int f = 0; f < 16000 && !g.stage_clear && !g.game_over; f++) {
        game_input_t in = {0};
        const enemy_t *e = game_primary_enemy(&g);

        if (!e) {
            in.right = true;
        } else {
            const int dx = e->x - g.player_x;

            /* Conservative player path: maintain giant-robot spacing and
             * use long-range weapons. This proves every encounter can be
             * reached, defeated, unlocked and advanced through. */
            if (dx < 100) {
                in.left = true;
            } else if (dx > 210) {
                in.right = true;
            } else if (g.attack == ATTACK_NONE) {
                if (g.player_energy >= 25)
                    in.breast = true;
                else if (!g.rocket.active)
                    in.rocket = true;
                else
                    in.heavy = true;
            }
        }

        game_update(&g, &in);
    }

    if (!g.stage_clear || g.game_over || g.event_index != 6) {
        fprintf(stderr,
            "full-stage test failed: clear=%d over=%d event=%u frame=%u hp=%d\n",
            g.stage_clear, g.game_over, g.event_index, g.frame, g.player_hp);
        return 1;
    }

    printf("full-stage progression: PASS (frame=%u hp=%d energy=%d)\n",
        g.frame, g.player_hp, g.player_energy);
    return 0;
}
