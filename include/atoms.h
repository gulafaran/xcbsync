#pragma once

#include <stdbool.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_ewmh.h>

extern xcb_atom_t UNAGI__NET_WM_WINDOW_OPACITY;
extern xcb_atom_t UNAGI__XROOTPMAP_ID;
extern xcb_atom_t UNAGI__XSETROOT_ID;

extern const xcb_atom_t *unagi_background_properties_atoms[];

xcb_intern_atom_cookie_t *unagi_atoms_init(void);
bool unagi_atoms_init_finalise(xcb_intern_atom_cookie_t *);
bool unagi_atoms_is_background_atom(const xcb_atom_t);
void unagi_atoms_update_supported(const xcb_property_notify_event_t *);
bool unagi_atoms_is_supported(const xcb_atom_t);