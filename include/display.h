#pragma once

#include <stdbool.h>

#include <xcb/xcb.h>
#include <xcb/xfixes.h>
#include <xcb/randr.h>

void unagi_display_init_event_handlers(void);

void unagi_display_init_extensions(void);
void unagi_display_init_extensions_finalise(void);

void unagi_display_event_set_owner_property(xcb_property_notify_event_t *);
void unagi_display_register_cm(void);
bool unagi_display_register_cm_finalise(void);

void unagi_display_init_atoms(void);
bool unagi_display_init_atoms_finalise(void);

void unagi_display_init_redirect(void);
void unagi_display_init_redirect_finalise(void);

void unagi_display_add_damaged_region(xcb_xfixes_region_t *, bool);
void unagi_display_reset_damaged(void);

void unagi_display_update_screen_information(xcb_randr_get_screen_info_cookie_t,
                                             xcb_randr_get_screen_resources_cookie_t);

bool display_vsync_drm_init(void);
int display_vsync_drm_wait(void);
void display_vsync_drm_cleanup(void);