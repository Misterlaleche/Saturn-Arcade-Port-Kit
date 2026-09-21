#include "game.h"

#include <string.h>

#define PLAYER_HP_MAX 180
#define PLAYER_ENERGY_MAX 100
#define PLAYER_SPEED 2
#define PLAYER_LEFT_MARGIN 12
#define PLAYER_CAMERA_ANCHOR 112
#define PLAYER_LOCK_RIGHT 178

typedef struct {
    int16_t trigger_x;
    enemy_kind_t a;
    int16_t a_offset;
    enemy_kind_t b;
    int16_t b_offset;
} stage_event_t;

static const stage_event_t k_events[] = {
    { 120, ENEMY_SOLDIER, 292, ENEMY_NONE,      0 },
    { 300, ENEMY_DRILL,   278, ENEMY_SOLDIER, 338 },
    { 500, ENEMY_DOUBLAS, 286, ENEMY_NONE,      0 },
    { 760, ENEMY_MISSILE, 276, ENEMY_DRILL,   340 },
    { 950, ENEMY_SOLDIER, 270, ENEMY_MISSILE, 334 },
    {1160, ENEMY_GARADA,  286, ENEMY_NONE,      0 }
};

#define EVENT_COUNT ((int)(sizeof(k_events) / sizeof(k_events[0])))

static int iabs(int v) { return v < 0 ? -v : v; }
static int signi(int v) { return v < 0 ? -1 : 1; }

static int enemy_hp(enemy_kind_t k)
{
    switch (k) {
        case ENEMY_SOLDIER: return 38;
        case ENEMY_DRILL:   return 52;
        case ENEMY_MISSILE: return 48;
        case ENEMY_DOUBLAS: return 190;
        case ENEMY_GARADA:  return 280;
        default: return 1;
    }
}

static int enemy_reach(enemy_kind_t k)
{
    switch (k) {
        case ENEMY_DOUBLAS: return 92;
        case ENEMY_GARADA:  return 104;
        case ENEMY_MISSILE: return 72;
        default: return 54;
    }
}

static int enemy_damage(enemy_kind_t k)
{
    switch (k) {
        case ENEMY_SOLDIER: return 6;
        case ENEMY_DRILL:   return 8;
        case ENEMY_MISSILE: return 7;
        case ENEMY_DOUBLAS: return 13;
        case ENEMY_GARADA:  return 16;
        default: return 0;
    }
}

static int enemy_speed(enemy_kind_t k)
{
    switch (k) {
        case ENEMY_GARADA: return 1;
        case ENEMY_DOUBLAS: return 1;
        default: return 2;
    }
}

static int enemy_cooldown(enemy_kind_t k)
{
    switch (k) {
        case ENEMY_DOUBLAS: return 48;
        case ENEMY_GARADA:  return 42;
        default: return 58;
    }
}

static enemy_t *alloc_enemy(game_state_t *g)
{
    for (int i = 0; i < GAME_MAX_ENEMIES; i++)
        if (!g->enemies[i].active)
            return &g->enemies[i];
    return NULL;
}

static void spawn_enemy(game_state_t *g, enemy_kind_t kind, int16_t offset)
{
    if (kind == ENEMY_NONE)
        return;
    enemy_t *e = alloc_enemy(g);
    if (!e)
        return;
    memset(e, 0, sizeof(*e));
    e->kind = kind;
    e->active = true;
    e->facing_left = true;
    e->x = (int16_t)(g->camera_x + offset);
    e->hp = e->hp_max = (int16_t)enemy_hp(kind);
    e->cooldown = 20;
}

static void spawn_event(game_state_t *g)
{
    if (g->event_index >= EVENT_COUNT)
        return;
    const stage_event_t *ev = &k_events[g->event_index];
    spawn_enemy(g, ev->a, ev->a_offset);
    spawn_enemy(g, ev->b, ev->b_offset);
    g->encounter_locked = true;
}

int game_active_enemy_count(const game_state_t *g)
{
    int n = 0;
    for (int i = 0; i < GAME_MAX_ENEMIES; i++)
        if (g->enemies[i].active)
            n++;
    return n;
}

const enemy_t *game_primary_enemy(const game_state_t *g)
{
    const enemy_t *best = NULL;
    for (int i = 0; i < GAME_MAX_ENEMIES; i++) {
        const enemy_t *e = &g->enemies[i];
        if (!e->active)
            continue;
        if (!best || e->hp_max > best->hp_max)
            best = e;
    }
    return best;
}

int game_player_screen_x(const game_state_t *g)
{
    return g->player_x - g->camera_x;
}

int game_enemy_screen_x(const game_state_t *g, int idx)
{
    if (idx < 0 || idx >= GAME_MAX_ENEMIES)
        return -200;
    return g->enemies[idx].x - g->camera_x;
}

void game_reset(game_state_t *g)
{
    memset(g, 0, sizeof(*g));
    g->player_x = 64;
    g->player_hp = PLAYER_HP_MAX;
    g->player_energy = PLAYER_ENERGY_MAX;
}

static void damage_enemy(enemy_t *e, int damage, int knockback, int stun)
{
    if (!e->active || e->hitstun > 0)
        return;
    e->hp -= (int16_t)damage;
    e->hitstun = (int16_t)stun;
    e->flash = 5;
    e->x += (int16_t)knockback;
    e->attack_timer = 0;
    e->attack_hit_done = false;
    if (e->hp <= 0) {
        e->hp = 0;
        e->active = false;
    }
}

static bool target_ahead(const game_state_t *g, const enemy_t *e, int range)
{
    const int dx = e->x - g->player_x;
    if (g->player_facing_left)
        return dx <= -26 && dx >= -range;
    return dx >= 26 && dx <= range;
}

static void hit_first_in_range(game_state_t *g, int range, int damage, int knockback, int stun)
{
    enemy_t *best = NULL;
    int best_dist = 32767;
    for (int i = 0; i < GAME_MAX_ENEMIES; i++) {
        enemy_t *e = &g->enemies[i];
        if (!e->active || !target_ahead(g, e, range))
            continue;
        int d = iabs(e->x - g->player_x);
        if (d < best_dist) {
            best = e;
            best_dist = d;
        }
    }
    if (best)
        damage_enemy(best, damage, g->player_facing_left ? -knockback : knockback, stun);
}

static void hit_all_in_range(game_state_t *g, int range, int damage, int knockback, int stun)
{
    for (int i = 0; i < GAME_MAX_ENEMIES; i++) {
        enemy_t *e = &g->enemies[i];
        if (e->active && target_ahead(g, e, range))
            damage_enemy(e, damage, g->player_facing_left ? -knockback : knockback, stun);
    }
}

static void start_attack(game_state_t *g, attack_kind_t a)
{
    g->attack = a;
    g->attack_age = 0;
    g->attack_hit_done = false;
}

static int attack_duration(attack_kind_t a)
{
    switch (a) {
        case ATTACK_PUNCH:  return 13;
        case ATTACK_HEAVY:  return 19;
        case ATTACK_ROCKET: return 22;
        case ATTACK_BREAST: return 34;
        case ATTACK_RUST:   return 30;
        default: return 0;
    }
}

static void update_player_attack(game_state_t *g)
{
    if (g->attack == ATTACK_NONE)
        return;

    g->attack_age++;

    if (!g->attack_hit_done) {
        if (g->attack == ATTACK_PUNCH && g->attack_age == 6) {
            hit_first_in_range(g, 64, 12, 11, 10);
            g->attack_hit_done = true;
        } else if (g->attack == ATTACK_HEAVY && g->attack_age == 9) {
            hit_first_in_range(g, 72, 20, 18, 16);
            g->attack_hit_done = true;
        } else if (g->attack == ATTACK_ROCKET && g->attack_age == 6) {
            if (!g->rocket.active) {
                g->rocket.active = true;
                g->rocket.returning = false;
                g->rocket.hit_done = false;
                g->rocket.x = (int16_t)(g->player_x + (g->player_facing_left ? -44 : 44));
            }
            g->attack_hit_done = true;
        } else if (g->attack == ATTACK_BREAST && g->attack_age == 15) {
            hit_all_in_range(g, 236, 42, 24, 24);
            g->attack_hit_done = true;
        } else if (g->attack == ATTACK_RUST && g->attack_age == 13) {
            hit_all_in_range(g, 150, 24, 12, 34);
            g->attack_hit_done = true;
        }
    }

    if (g->attack_age >= attack_duration(g->attack)) {
        g->attack = ATTACK_NONE;
        g->attack_age = 0;
        g->attack_hit_done = false;
    }
}

static void update_rocket(game_state_t *g)
{
    if (!g->rocket.active)
        return;

    const int dir = g->player_facing_left ? -1 : 1;
    if (!g->rocket.returning) {
        g->rocket.x += (int16_t)(dir * 9);
        for (int i = 0; i < GAME_MAX_ENEMIES; i++) {
            enemy_t *e = &g->enemies[i];
            if (!e->active || g->rocket.hit_done)
                continue;
            if (iabs(e->x - g->rocket.x) < 38) {
                damage_enemy(e, 30, dir * 22, 22);
                g->rocket.hit_done = true;
                g->rocket.returning = true;
            }
        }
        if (iabs(g->rocket.x - g->player_x) > 230)
            g->rocket.returning = true;
    } else {
        int dx = g->player_x - g->rocket.x;
        if (iabs(dx) <= 14) {
            g->rocket.active = false;
        } else {
            g->rocket.x += (int16_t)(signi(dx) * 12);
        }
    }
}

static void damage_player(game_state_t *g, int damage, int from_x)
{
    if (g->player_invuln > 0 || g->game_over)
        return;
    g->player_hp -= (int16_t)damage;
    g->player_hitstun = 16;
    g->player_invuln = 38;
    g->player_flash = 7;
    g->player_x += (int16_t)(from_x > g->player_x ? -12 : 12);
    if (g->player_x < g->camera_x + PLAYER_LEFT_MARGIN)
        g->player_x = (int16_t)(g->camera_x + PLAYER_LEFT_MARGIN);
    if (g->player_hp <= 0) {
        g->player_hp = 0;
        g->game_over = true;
        g->attack = ATTACK_NONE;
        g->rocket.active = false;
    }
}

static void update_enemy(game_state_t *g, enemy_t *e)
{
    if (!e->active)
        return;
    if (e->flash > 0) e->flash--;
    if (e->hitstun > 0) {
        e->hitstun--;
        return;
    }
    if (e->cooldown > 0) e->cooldown--;

    const int dx = g->player_x - e->x;
    e->facing_left = dx < 0;

    if (e->attack_timer > 0) {
        e->attack_timer--;
        if (!e->attack_hit_done && e->attack_timer == 8) {
            if (iabs(g->player_x - e->x) <= enemy_reach(e->kind) + 10)
                damage_player(g, enemy_damage(e->kind), e->x);
            e->attack_hit_done = true;
        }
        if (e->attack_timer == 0) {
            e->cooldown = (int16_t)enemy_cooldown(e->kind);
            e->attack_hit_done = false;
        }
        return;
    }

    const int dist = iabs(dx);
    if (dist > enemy_reach(e->kind)) {
        int step = enemy_speed(e->kind);
        e->x += (int16_t)(signi(dx) * step);
    } else if (e->cooldown == 0) {
        e->attack_timer = (int16_t)((e->kind == ENEMY_GARADA) ? 27 : 23);
        e->attack_hit_done = false;
    }
}

void game_update(game_state_t *g, const game_input_t *in)
{
    if (in->start) {
        game_reset(g);
        return;
    }

    g->frame++;
    if (g->player_invuln > 0) g->player_invuln--;
    if (g->player_flash > 0) g->player_flash--;
    if (g->player_hitstun > 0) g->player_hitstun--;
    if ((g->frame % 10) == 0 && g->player_energy < PLAYER_ENERGY_MAX)
        g->player_energy++;

    if (g->game_over || g->stage_clear)
        return;

    if (g->player_hitstun == 0) {
        if (g->attack == ATTACK_NONE) {
            if (in->left) {
                g->player_x -= PLAYER_SPEED;
                g->player_facing_left = true;
            }
            if (in->right) {
                g->player_x += PLAYER_SPEED;
                g->player_facing_left = false;
            }

            if (in->breast && g->player_energy >= 25) {
                g->player_energy -= 25;
                start_attack(g, ATTACK_BREAST);
            } else if (in->rust && g->player_energy >= 18) {
                g->player_energy -= 18;
                start_attack(g, ATTACK_RUST);
            } else if (in->rocket && !g->rocket.active) {
                start_attack(g, ATTACK_ROCKET);
            } else if (in->heavy) {
                start_attack(g, ATTACK_HEAVY);
            } else if (in->punch) {
                start_attack(g, ATTACK_PUNCH);
            }
        }
    }

    if (g->player_x < g->camera_x + PLAYER_LEFT_MARGIN)
        g->player_x = (int16_t)(g->camera_x + PLAYER_LEFT_MARGIN);

    if (g->encounter_locked) {
        int maxx = g->camera_x + PLAYER_LOCK_RIGHT;
        if (g->player_x > maxx)
            g->player_x = (int16_t)maxx;
    } else {
        int screen_x = g->player_x - g->camera_x;
        if (screen_x > PLAYER_CAMERA_ANCHOR && g->camera_x < GAME_STAGE_END) {
            int advance = screen_x - PLAYER_CAMERA_ANCHOR;
            if (advance > PLAYER_SPEED) advance = PLAYER_SPEED;
            g->camera_x += (int16_t)advance;
            if (g->camera_x > GAME_STAGE_END) g->camera_x = GAME_STAGE_END;
        }
    }

    update_player_attack(g);
    update_rocket(g);

    for (int i = 0; i < GAME_MAX_ENEMIES; i++)
        update_enemy(g, &g->enemies[i]);

    if (g->encounter_locked && game_active_enemy_count(g) == 0) {
        g->encounter_locked = false;
        g->event_index++;
        if (g->event_index >= EVENT_COUNT) {
            g->stage_clear = true;
            return;
        }
    }

    if (!g->encounter_locked && g->event_index < EVENT_COUNT) {
        if (g->camera_x >= k_events[g->event_index].trigger_x)
            spawn_event(g);
    }
}
