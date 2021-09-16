#pragma once

#include <xcb/xcb.h>
#include "display.h"

void unagi_event_handle_startup(xcb_generic_event_t *);
void unagi_event_handle(xcb_generic_event_t *);
void unagi_event_handle_poll_loop(void (*handler)(xcb_generic_event_t *));