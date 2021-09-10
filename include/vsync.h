#pragma once

#include <stdbool.h>

bool vsync_init(void);
int vsync_wait(void);
void vsync_cleanup(void);