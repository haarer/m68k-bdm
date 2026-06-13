#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>

#define BOARD_TARGET_SIM

#define GPIO_A 0
#define GPIO_B 1
#define GPIO_C 2

/* Mirrors bridge pin mapping for direct wire connection */
#define DSCLK_PORT       GPIO_A
#define DSCLK_PIN        0

#define DSI_PORT         GPIO_A
#define DSI_PIN          1

#define TARGET_RESET_PORT GPIO_A
#define TARGET_RESET_PIN  2

#define DSO_PORT         GPIO_B
#define DSO_PIN          0

#define FREEZE_PORT      GPIO_B
#define FREEZE_PIN       1

#endif
