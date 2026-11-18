#ifndef POP_A_SHOT_LIGHTS_H
#define POP_A_SHOT_LIGHTS_H

void lights_init(void);
void lights_off(void);

// Call these once per event, from the main loop (not an interrupt).
void lights_score(void);
void lights_countdown_warning(void);
void lights_game_over(void);

// Call every pass through the main loop to advance the flashes.
void lights_update(void);

#endif
