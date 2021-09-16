#pragma once

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>

void unagi_key_lock_mask_get_reply(xcb_get_modifier_mapping_cookie_t cookie);
xcb_keysym_t unagi_key_getkeysym(const xcb_keycode_t, const uint16_t);