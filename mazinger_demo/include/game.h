#ifndef MAZINGER_GAME_H
#define MAZINGER_GAME_H

#include <stdbool.h>
#include <stdint.h>

#define GAME_SCREEN_W 352
#define GAME_SCREEN_H 224
#define GAME_GROUND_Y 216
#define GAME_MAX_ENEMIES 3
#define GAME_STAGE_END 1260

typedef enum {
    ENEMY_NONE = 0,
    ENEMY_SOLDIER,
    ENEMY_DRILL,
    ENEMY_MISSILE,
    ENEMY_DOUBLAS,
    ENEMY_GARADA
} enemy_kind_t;

typedef enum {
    ATTACK_NONE = 0,
    ATTACK_PUNCH,
    ATTACK_HEAVY,
    ATTACK_ROCKET,
    ATTACK_BREAST,
    ATTACK_RUST
} attack_kind_t;

typedef struct {
    bool left;
    bool right;
    bool punch;
    bool heavy;
    bool rocket;
    bool breast;
    bool rust;
    bool start;
} game_input_t;

typedef struct {
    enemy_kind_t kind;
    bool active;
    bool facing_left;
    int16_t x;
    int16_t hp;
    int16_t hp_max;
    int16_t hitstun;
    int16_t flash;
    int16_t attack_timer;
    int16_t cooldown;
    bool attack_hit_done;
} enemy_t;

typedef struct {
    bool active;
    bool returning;
    bool hit_done;
    int16_t x;
} rocket_t;

typedef struct {
    uint32_t frame;
    int16_t camera_x;
    int16_t player_x;
    int16_t player_hp;
    int16_t player_energy;
    int16_t player_hitstun;
    int16_t player_invuln;
    int16_t player_flash;
    bool player_facing_left;

    attack_kind_t attack;
    int16_t attack_age;
    bool attack_hit_done;

    rocket_t rocket;
    enemy_t enemies[GAME_MAX_ENEMIES];

    uint8_t event_index;
    bool encounter_locked;
    bool stage_clear;
    bool game_over;
} game_state_t;

void game_reset(game_state_t *g);
void game_update(game_state_t *g, const game_input_t *in);
int game_active_enemy_count(const game_state_t *g);
const enemy_t *game_primary_enemy(const game_state_t *g);
int game_player_screen_x(const game_state_t *g);
int game_enemy_screen_x(const game_state_t *g, int idx);

#endif
