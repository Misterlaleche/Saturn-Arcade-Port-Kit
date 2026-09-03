#include "portkit.h"

int
main(void)
{
    portkit_state_t state;
    smpc_peripheral_digital_t digital;

    portkit_assets_generate();
    portkit_state_reset(&state);
    portkit_display_enable();

    while (true) {
        portkit_input_read(&digital);
        portkit_state_update(&state, &digital);
        portkit_render(&state);
    }

    return 0;
}

void
user_init(void)
{
    portkit_init();
}
