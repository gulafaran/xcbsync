#include <stdlib.h>

#include <xcb/xcb.h>
#include <xcb/composite.h>
#include <xcb/xcb_event.h>

#include "event.h"
#include "structs.h"
#include "util.h"
#include "window.h"
#include "atoms.h"
#include "key.h"

/** Requests label of Composite extension for X error reporting, which
 *  are uniquely  identified according to their  minor opcode starting
 *  from 0 */
static const char *composite_request_label[] = {
  "CompositeQueryVersion",
  "CompositeRedirectWindow",
  "CompositeRedirectSubwindows",
  "CompositeUnredirectWindow",
  "CompositeUnredirectWindows",
  "CompositeCreateRegionFromBorderClip",
  "CompositeNameWindowPixmap",
  "CompositeCompositeGetOverlayWindow",
  "CompositeCompositeReleaseOverlayWindow",
  "CompositeRedirectCoordinate",
  "CompositeTransformCoordinate"
};

/** Requests label  of XFixes extension  for X error  reporting, which
 *  are uniquely  identified according to their  minor opcode starting
 *  from 0 */
static const char *xfixes_request_label[] = {
  "XFixesQueryVersion",
  "XFixesChangeSaveSet",
  "XFixesSelectSelectionInput",
  "XFixesSelectCursorInput",
  "XFixesGetCursorImage",
  "XFixesCreateRegion",
  "XFixesCreateRegionFromBitmap",
  "XFixesCreateRegionFromWindow",
  "XFixesCreateRegionFromGC",
  "XFixesCreateRegionFromPicture",
  "XFixesDestroyRegion",
  "XFixesSetRegion",
  "XFixesCopyRegion",
  "XFixesUnionRegion",
  "XFixesIntersectRegion",
  "XFixesSubtractRegion",
  "XFixesInvertRegion",
  "XFixesTranslateRegion",
  "XFixesRegionExtents",
  "XFixesFetchRegion",
  "XFixesSetGCClipRegion",
  "XFixesSetWindowShapeRegion",
  "XFixesSetPictureClipRegion",
  "XFixesSetCursorName",
  "XFixesGetCursorName",
  "XFixesGetCursorImageAndName",
  "XFixesChangeCursor",
  "XFixesChangeCursorByName",
  "XFixesExpandRegion",
  "XFixesHideCursor",
  "XFixesShowCursor"
};

/** Requests label  of Damage extension  for X error  reporting, which
 *  are uniquely  identified according to their  minor opcode starting
 *  from 0 */
static const char *damage_request_label[] = {
  "DamageQueryVersion",
  "DamageCreate",
  "DamageDestroy",
  "DamageSubtract",
  "DamageAdd",
};

/** Error label of XFixes specific error */
static const char *xfixes_error_label = "BadRegion";

/** Error label of Damage specific error */
static const char *damage_error_label = "BadDamage";

/** Get  the  request  label  from  the minor  opcode  if  it  exists,
    otherwise returns NULL */
#define ERROR_EXTENSION_GET_REQUEST_LABEL(labels, minor_code) \
  (minor_code < unagi_countof(labels) ? labels[minor_code] : NULL)

/** Get the request label from the major and minor codes of the failed
 *  request.  The  major codes 0 through  127 are reserved  for X core
 *  requests whereas the major codes  128 through 255 are reserved for
 *  X extensions. If this is an  extension, the minor code gives the X
 *  extension request which failed.
 *
 * \param request_major_code The X error request major opcode
 * \param request_minor_code The X error request minor opcode
 * \return The error label associated with the major and minor opcodes
 */
static const char *
error_get_request_label(const uint8_t request_major_code,
			const uint16_t request_minor_code)
{
  if((*globalconf.rendering->is_request)(request_major_code))
    return (*globalconf.rendering->get_request_label)(request_minor_code);

  else if(request_major_code == globalconf.extensions.composite->major_opcode)
    return ERROR_EXTENSION_GET_REQUEST_LABEL(composite_request_label,
					     request_minor_code);

  else if(request_major_code == globalconf.extensions.xfixes->major_opcode)
    return ERROR_EXTENSION_GET_REQUEST_LABEL(xfixes_request_label,
					     request_minor_code);

  else if(request_major_code == globalconf.extensions.damage->major_opcode)
    return ERROR_EXTENSION_GET_REQUEST_LABEL(damage_request_label,
					     request_minor_code);

  else
      return xcb_event_get_request_label(request_major_code);
}

/** Handler for X  errors.  Every error includes an  8-bit error code.
 *  Error codes 128 through 255 are reserved for extensions.
 *  
 *  For requests  with side-effects, the  failing resource ID  is also
 *  returned:  Colormap, Cursor,  Drawable, Font,  GContext, IDChoice,
 *  Pixmap,  and  Window.   For  Atom  errors,  the  failing  atom  is
 *  returned.  For Value errors, the failing value is returned.  Other
 *  core  errors return no  additional data.   Unused bytes  within an
 *  error are not guaranteed to be zero.
 *
 * \see error_get_request_label
 * \param error The X error
 */
static void
event_handle_error(xcb_generic_error_t *error)
{
  /* To determine  whether the error comes from  an extension request,
     it use the 'first_error'  field of QueryExtension reply, plus the
     first error code of the extension */
  const uint8_t xfixes_bad_region =
    globalconf.extensions.xfixes->first_error + XCB_XFIXES_BAD_REGION;

  const uint8_t damage_bad_damage =
    globalconf.extensions.damage->first_error + XCB_DAMAGE_BAD_DAMAGE;

  const char *error_label = (*globalconf.rendering->get_error_label)(error->error_code);

  if(!error_label)
    {
      if(error->error_code == xfixes_bad_region)
	error_label = xfixes_error_label;
      else if(error->error_code == damage_bad_damage)
	error_label = damage_error_label;
      else
	error_label = xcb_event_get_error_label(error->error_code);
    }

  unagi_warn("X error: request=%s (major=%ju, minor=%ju, resource=%jx), error=%s",
             error_get_request_label(error->major_code, error->minor_code),
             (uintmax_t) error->major_code, (uintmax_t) error->minor_code,
             (uintmax_t) error->resource_id, error_label);
}

/** Handler for X events  during initialisation (any error encountered
 *  will exit  the program).
 *
 * \see event_handle_error
 * \see unagi_display_event_set_owner_property
 * \param error The X error
 */
void
unagi_event_handle_startup(xcb_generic_event_t *event)
{
  switch(XCB_EVENT_RESPONSE_TYPE(event))
    {
    case 0:
      {
	xcb_generic_error_t *error = (xcb_generic_error_t *) event;

	/* If  the redirection  of  existing windows  in the  off-screen
	   buffer failed, then it means that another program has already
	   redirected   the  windows,   certainly  another   compositing
	   manager... */
	if(error->major_code == globalconf.extensions.composite->major_opcode &&
	   error->minor_code == XCB_COMPOSITE_REDIRECT_SUBWINDOWS)
	  {
	    free(error);
	    unagi_fatal("Another compositing manager is already running");
	  }

	event_handle_error(error);
	free(error);
	unagi_fatal("Unexpected X error during startup");
      }

      break;

    case XCB_PROPERTY_NOTIFY:
      unagi_display_event_set_owner_property((void *) event);
      break;
    }
}

/** Maximum number  of DamageNotify events received  before repainting
    the full window */
#define DAMAGE_NOTIFY_MAX 24

/** Handler for DamageNotify events
 *
 * \param event The X DamageNotify event
 */
static void
event_handle_damage_notify(xcb_damage_notify_event_t *event)
{
  unagi_debug("DamageNotify: area: %jux%ju %+jd %+jd "
              "(drawable=%jx,geometry=%jux%ju +%jd +%jd)",
              (uintmax_t) event->area.width, (uintmax_t) event->area.height,
              (intmax_t) event->area.x, (intmax_t) event->area.y,
              (uintmax_t) event->drawable,
              (uintmax_t) event->geometry.width, (uintmax_t) event->geometry.height,
              (uintmax_t) event->geometry.x, (uintmax_t) event->geometry.y);

  unagi_window_t *window = unagi_window_list_get(event->drawable);
  /* The window may have disappeared in the meantime or is not visible
     so do nothing */
  if(!window || !unagi_window_is_visible(window))
    return;

  UNAGI_PLUGINS_EVENT_HANDLE(event, damage, window);

  xcb_xfixes_region_t damaged_region;
  bool is_temporary_region = false;

  /* If the Window has never been  damaged, then it means it has never
     be painted on the screen yet, thus paint its entire content */
  if(!window->damaged)
    {
      damaged_region = window->region;
      window->damaged = true;
      window->damaged_ratio = 1.0;
    }
  /* Do nothing if the window is already fully damaged */
  else if(window->damaged_ratio >= UNAGI_WINDOW_FULLY_DAMAGED_RATIO)
    {
      unagi_debug("Window %jx fully damaged (cached)", (uintmax_t) window->id);
      return;
    }
  /* If  the   window  is  considered   fully  damaged  or   too  many
     DamageNotify  events   have  been   received,  then   repaint  it
     completely */
  else if(window->damage_notify_counter++ > DAMAGE_NOTIFY_MAX ||
          window_get_damaged_ratio(window, event) >= UNAGI_WINDOW_FULLY_DAMAGED_RATIO)
    {
      unagi_debug("Window %jx damaged ratio: %.2f, counter: %d",
                  (uintmax_t) window->id,
                  window->damaged_ratio,
                  window->damage_notify_counter);

      /* @todo:  Perhaps  xcb_damage_add()  could  be  used  to  avoid
         further events to  be sent as the window  is considered fully
         damaged? */
      damaged_region = window->region;
      window->damaged_ratio = 1.0;
    }
  /* Otherwise, just paint the damaged Region (which may be the entire
     Window or part of it */
  else
    {
      damaged_region = xcb_generate_id(globalconf.connection);

      event->area.x += event->geometry.x;
      event->area.y += event->geometry.y;
      xcb_xfixes_create_region(globalconf.connection, damaged_region,
                               1, &event->area);

      is_temporary_region = true;
    }

  unagi_display_add_damaged_region(&damaged_region, is_temporary_region);
}

/** Handler for RRScreenChangeNotify events reported when the screen
 *  configuration change and is meaningful to get the new refresh rate
 *
 * \param event The X RRScreenChangeNotify event
 */
static void
event_handle_randr_screen_change_notify(xcb_randr_screen_change_notify_event_t *event)
{
  unagi_debug("RandrScreenChangeNotify: root=%jx", (uintmax_t) event->root);

  unagi_display_update_screen_information(xcb_randr_get_screen_info_unchecked(globalconf.connection,
                                                                              globalconf.screen->root),
                                          xcb_randr_get_screen_resources_unchecked(globalconf.connection,
                                                                                   globalconf.screen->root));

  UNAGI_PLUGINS_EVENT_HANDLE(event, randr_screen_change_notify, NULL);
}

/** Handler for KeyPress events reported once a key is pressed. Only
 *  handle when GrabKeyBoard has been issued beforehand.
 *
 * \param event The X KeyPress event
 */
static void
event_handle_key_press(xcb_key_press_event_t *event)
{
  unagi_debug("KeyPress: detail=%ju, event=%jx, state=%jx",
              (uintmax_t) event->detail, (uintmax_t) event->event,
              (uintmax_t) event->state);

  UNAGI_PLUGINS_EVENT_HANDLE(event, key_press, unagi_window_list_get(event->event));
}

/** Handler for KeyRelease events reported once a key is released. Only
 *  handle when GrabKeyBoard has been issued beforehand.
 *
 * \param event The X KeyRelease event
 */
static void
event_handle_key_release(xcb_key_release_event_t *event)
{
  unagi_debug("KeyRelease: detail=%ju, event=%jx, state=%jx",
              (uintmax_t) event->detail, (uintmax_t) event->event,
              (uintmax_t) event->state);

  UNAGI_PLUGINS_EVENT_HANDLE(event, key_release, unagi_window_list_get(event->event));
}

/** Handler for ButtonRelease events reported once a pointer button is
 *  released. Only handle when GrabPointer has been issued beforehand.
 */
static void
event_handle_button_release(xcb_button_release_event_t *event)
{
  unagi_debug("ButtonRelease: detail=%ju, event=%jx, state=%jx",
              (uintmax_t) event->detail, (uintmax_t) event->event,
              (uintmax_t) event->state);

  UNAGI_PLUGINS_EVENT_HANDLE(event, button_release, unagi_window_list_get(event->event));
}

/** Handler for MotionNotify events reported when the pointer
 *  moves. As this event mask is not set on the Root Window because
 *  some windows may have already grabbed the pointer (Emacs,
 *  Firefox...), a GrabPointer request must be issued to receive them.
 *
 * \param event The X MotionNotify event
 */
static void
event_handle_motion_notify(xcb_motion_notify_event_t *event)
{
  unagi_debug("detail=%ju, event=%jx, root=%jx, child=%jx, state=%jx, "
              "root_x=%d, root_y=%d, event_x=%d, event_y=%d, same_screen=%d",
              (uintmax_t) event->detail, (uintmax_t) event->event,
              (uintmax_t) event->event, (uintmax_t) event->child,
              (uintmax_t) event->state,
              event->root_x, event->root_y, event->event_x, event->event_y,
              event->same_screen);

  UNAGI_PLUGINS_EVENT_HANDLE(event, motion_notify, NULL);
}

/** Handler for CirculateNotify events  reported when a window changes
 *  its position in the stack (either  Top if the window is now on top
 *  of all siblings or Bottom)
 *
 * \param event The X CirculateNotify event
 */
static void
event_handle_circulate_notify(xcb_circulate_notify_event_t *event)
{
  unagi_debug("CirculateNotify: event=%jx, window=%jx",
              (uintmax_t) event->event, (uintmax_t) event->window);

  unagi_window_t *window = unagi_window_list_get(event->window);

  /* Above window  of None means that  the window is  placed below all
     its siblings */
  if(event->place == XCB_PLACE_ON_BOTTOM)
    unagi_window_restack(window, XCB_NONE);
  else
    {
      /* Get the identifier of the topmost window of the stack */
      unagi_window_t *windows_tail;
      for(windows_tail = globalconf.windows;
	  windows_tail && windows_tail->next;
	  windows_tail = windows_tail->next)
	;

      unagi_window_restack(window, windows_tail->id);
    }

  UNAGI_PLUGINS_EVENT_HANDLE(event, circulate, window);
}

/** Handler for ConfigureNotify events reported when a windows changes
 *  its size, position and/or position in the stack
 *
 * \param event The X ConfigureNotify event
 */
static void
event_handle_configure_notify(xcb_configure_notify_event_t *event)
{
  unagi_debug("ConfigureNotify: event=%jx, window=%jx above=%jx (%jux%ju +%jd +%jd, "
              "border=%ju)",
              (uintmax_t) event->event, (uintmax_t) event->window,
              (uintmax_t) event->above_sibling, 
              (uintmax_t) event->width, (uintmax_t) event->height,
              (intmax_t) event->x, (intmax_t) event->y,
              (uintmax_t) event->border_width);

  /* If  this is  the root  window, then  just create  again  the root
     background picture */
  if(event->window == globalconf.screen->root)
    {
      globalconf.screen->width_in_pixels = event->width;
      globalconf.screen->height_in_pixels = event->height;

      globalconf.background_reset = true;
      (*globalconf.rendering->reset_background)();

      return;
    }

  unagi_window_t *window = unagi_window_list_get(event->window);
  if(!window)
    {
      unagi_debug("No such window %jx", (uintmax_t) event->window);
      return;
    }

  /* Add the Window  Region to the damaged region  to clear old window
     position or size and re-create the Window Region as well

     @todo: Perhaps further checks  could be done to avoid re-creating
            the Window Region but would it really change anything from
            a performance POV?
  */
  bool is_not_visible = false;
  if(unagi_window_is_visible(window))
    {
      unagi_display_add_damaged_region(&window->region, true);
      window->damaged_ratio = 1.0;
    }
  else
    is_not_visible = true;

  window->geometry->x = event->x;
  window->geometry->y = event->y;

  bool update_pixmap = false;

  /* Invalidate  Pixmap and  Picture if  the window  has  been resized
     because  a  new  pixmap  is  allocated everytime  the  window  is
     resized (only meaningful when the window is viewable) */
  if(window->attributes->map_state == XCB_MAP_STATE_VIEWABLE &&
     (window->geometry->width != event->width ||
      window->geometry->height != event->height ||
      window->geometry->border_width != event->border_width))
    update_pixmap = true;

  window->geometry->width = event->width;
  window->geometry->height = event->height;
  window->geometry->border_width = event->border_width;
  window->attributes->override_redirect = event->override_redirect;

  if(unagi_window_is_visible(window))
    {
      window->region = unagi_window_get_region(window, true, false);

      /* This is needed to ensure that a window that was mapped
         outside the screen, and moved inside after, will be shown. An
         example is the gnome panel */
      if(update_pixmap || is_not_visible)
        {
          unagi_window_free_pixmap(window);
          window->pixmap = unagi_window_get_pixmap(window);
        }

      /* Whatever happens (restack/resizing/moving Windows), this
         should be added to damaged area... */
      unagi_display_add_damaged_region(&window->region, false);
      window->damaged_ratio = 1.0;
    }

  unagi_window_restack(window, event->above_sibling);

  UNAGI_PLUGINS_EVENT_HANDLE(event, configure, window);
}

/** Handler  for  CreateNotify  event  reported  when  a  CreateWindow
 *  request is issued. It's worth noticing that this request specifies
 *  the new window  geometry but some programs such  as xterm create a
 *  1x1 window and then issue a ConfigureNotify
 *
 * \param event The X CreateNotify event
 */
static void
event_handle_create_notify(xcb_create_notify_event_t *event)
{
  unagi_debug("CreateNotify: parent=%jx, window=%jx (%jux%ju +%jd +%jd, border=%ju)",
              (uintmax_t) event->parent, (uintmax_t) event->window,
              (uintmax_t) event->width, (uintmax_t) event->height,
              (intmax_t) event->x, (intmax_t) event->y,
              (uintmax_t) event->border_width);

  /* Add  the  new window  whose  identifier  is  given in  the  event
     itself and  */
  unagi_window_t *new_window = window_add(event->window, false);
  if(!new_window)
    {
      unagi_debug("Cannot create window %jx", (uintmax_t) event->window);
      return;
    }

  /* No need  to do  a GetGeometry request  as the window  geometry is
     given in the CreateNotify event itself */
  new_window->geometry = calloc(1, sizeof(xcb_get_geometry_reply_t));
  new_window->geometry->x = event->x;
  new_window->geometry->y = event->y;
  new_window->geometry->width = event->width;
  new_window->geometry->height = event->height;
  new_window->geometry->border_width = event->border_width;

  UNAGI_PLUGINS_EVENT_HANDLE(event, create, new_window);
}

/** Handler  for  DestroyNotify event  reported  when a  DestroyWindow
 *  request is issued
 *
 * \param event The X DestroyNotify event
 */
static void
event_handle_destroy_notify(xcb_destroy_notify_event_t *event)
{
  unagi_debug("DestroyNotify: parent=%jx, window=%jx",
              (uintmax_t) event->event, (uintmax_t) event->window);

  unagi_window_t *window = unagi_window_list_get(event->window);
  if(!window)
    {
      unagi_debug("Can't destroy window %jx", (uintmax_t) event->window);
      return;
    }

  /* If a DestroyNotify has been received, then the damage object have
     been freed automatically in the meantime */
  window->damage = XCB_NONE;

  UNAGI_PLUGINS_EVENT_HANDLE(event, destroy, window);

  unagi_window_list_remove_window(window, true);
}

/** Handler for  MapNotify event reported when a  MapWindow request is
 *  issued
 *
 * \param event The X MapNotify event
 */
static void
event_handle_map_notify(xcb_map_notify_event_t *event)
{
  unagi_debug("MapNotify: event=%jx, window=%jx",
              (uintmax_t) event->event, (uintmax_t) event->window);

  unagi_window_t *window = unagi_window_list_get(event->window);
  if(!window)
    {
      unagi_debug("Window %jx disappeared", (uintmax_t) event->window);
      return;
    }

  window->attributes->map_state = XCB_MAP_STATE_VIEWABLE;

  if(unagi_window_is_visible(window))
    {
      /* Create and store the region associated with the window to
         avoid creating regions all the time, this Region will be
         destroyed only upon DestroyNotify or re-created upon
         ConfigureNotify */
      window->region = unagi_window_get_region(window, true, true);

      /* Everytime a window is mapped, a new pixmap is created */
      unagi_window_free_pixmap(window);
      window->pixmap = unagi_window_get_pixmap(window);
    }

  window->damaged = false;

  UNAGI_PLUGINS_EVENT_HANDLE(event, map, window);
}

/** Handler  for ReparentNotify event  reported when  a ReparentWindow
 * request is  issued which reparents  the window to new  given parent
 * after unmapping it if it is already mapped (which is then mapped at
 * the end)
 *
 * \param event The X ReparentNotify event
 */
static void
event_handle_reparent_notify(xcb_reparent_notify_event_t *event)
{
  unagi_debug("ReparentNotify: event=%jx, window=%jx, parent=%jx",
              (uintmax_t) event->event, (uintmax_t) event->window,
              (uintmax_t) event->parent);

  unagi_window_t *window = unagi_window_list_get(event->window);

  /* Add the window if it is not already managed */ 
  if(event->parent == globalconf.screen->root ||
     !unagi_window_list_get(event->window))
    window_add(event->window, true);
  /* Don't manage the window if the parent is not the root window */
  else
    unagi_window_list_remove_window(window, true);

  UNAGI_PLUGINS_EVENT_HANDLE(event, reparent, window);
}

/** Handler for UnmapNotify event  reported when a UnmapWindow request
 *  is issued
 *
 * \param event The X UnmapNotify event
 */
static void
event_handle_unmap_notify(xcb_unmap_notify_event_t *event)
{
  unagi_debug("UnmapNotify: event=%jx, window=%jx",
              (uintmax_t) event->event, (uintmax_t) event->window);

  unagi_window_t *window = unagi_window_list_get(event->window);
  if(!window)
    {
      unagi_warn("Window %jx disappeared", (uintmax_t) event->window);
      return;
    }

  if(unagi_window_is_visible(window))
    {
      unagi_display_add_damaged_region(&window->region, true);
      window->damaged_ratio = 1.0;
    }

  /* Update window state */
  window->attributes->map_state = XCB_MAP_STATE_UNMAPPED;

  /* The window is not damaged anymore as it is not visible */
  window->damaged = false;

  UNAGI_PLUGINS_EVENT_HANDLE(event, unmap, window);
}

/** Handler  for PropertyNotify event  reported when  a ChangeProperty
 *  request is issued
 *
 * \param event The X PropertyNotify event
 */
static void
event_handle_property_notify(xcb_property_notify_event_t *event)
{
  unagi_debug("PropertyNotify: window=%jx, atom=%ju",
              (uintmax_t) event->window, (uintmax_t) event->atom);

  /* If the background image has been updated */
  if(unagi_atoms_is_background_atom(event->atom) &&
     event->window == globalconf.screen->root)
    {
      unagi_debug("New background Pixmap set");
      globalconf.background_reset = true;
      (*globalconf.rendering->reset_background)();
    }

  /* Update _NET_SUPPORTED value */
  if(event->atom == globalconf.ewmh._NET_SUPPORTED)
    unagi_atoms_update_supported(event);

  /* As plugins  requirements are  only atoms, if  the plugin  did not
     meet the requirements on startup, it can try again... */
  unagi_window_t *window = unagi_window_list_get(event->window);

  for(unagi_plugin_t *plugin = globalconf.plugins; plugin; plugin = plugin->next)
    if(plugin->vtable->events.property)
      {
	(*plugin->vtable->events.property)(event, window);

	if(!plugin->enable && plugin->vtable->check_requirements)
	  plugin->enable = (*plugin->vtable->check_requirements)();
      }
}

/** Handler for  Mapping event reported  when the keyboard  mapping is
 *  modified
 *
 * \param event The X Mapping event
 */
static void
event_handle_mapping_notify(xcb_mapping_notify_event_t *event)
{
  unagi_debug("MappingNotify: request=%ju, first_keycode=%ju, count=%ju",
              (uintmax_t) event->request, (uintmax_t) event->first_keycode,
              (uintmax_t) event->count);

  if(event->request != XCB_MAPPING_MODIFIER &&
     event->request != XCB_MAPPING_KEYBOARD)
    return;

  xcb_get_modifier_mapping_cookie_t key_mapping_cookie =
    xcb_get_modifier_mapping_unchecked(globalconf.connection);

  xcb_key_symbols_free(globalconf.keysyms);
  globalconf.keysyms = xcb_key_symbols_alloc(globalconf.connection);

  unagi_key_lock_mask_get_reply(key_mapping_cookie);

  UNAGI_PLUGINS_EVENT_HANDLE(event, mapping, NULL);
}

/** Initialise errors and events handlers
 *
 * \see unagi_display_init_redirect
 */
void
unagi_event_handle(xcb_generic_event_t *event)
{
  const uint8_t response_type = XCB_EVENT_RESPONSE_TYPE(event);

  if(response_type == 0)
    {
      event_handle_error((void *) event);
      return;
    }
  else if(response_type == (globalconf.extensions.damage->first_event +
                            XCB_DAMAGE_NOTIFY))
    {
      event_handle_damage_notify((void *) event);
      return;
    }
  else if(globalconf.extensions.randr &&
          response_type == (globalconf.extensions.randr->first_event +
                            XCB_RANDR_SCREEN_CHANGE_NOTIFY))
    {
      event_handle_randr_screen_change_notify((void *) event);
      return;
    }

  switch(response_type)
    {
#define EVENT(type, callback) case type: callback((void *) event); return
      EVENT(XCB_KEY_PRESS, event_handle_key_press);
      EVENT(XCB_KEY_RELEASE, event_handle_key_release);
      EVENT(XCB_BUTTON_RELEASE, event_handle_button_release);
      EVENT(XCB_MOTION_NOTIFY, event_handle_motion_notify);
      EVENT(XCB_CIRCULATE_NOTIFY, event_handle_circulate_notify);
      EVENT(XCB_CONFIGURE_NOTIFY, event_handle_configure_notify);
      EVENT(XCB_CREATE_NOTIFY, event_handle_create_notify);
      EVENT(XCB_DESTROY_NOTIFY, event_handle_destroy_notify);
      EVENT(XCB_MAP_NOTIFY, event_handle_map_notify);
      EVENT(XCB_REPARENT_NOTIFY, event_handle_reparent_notify);
      EVENT(XCB_UNMAP_NOTIFY, event_handle_unmap_notify);
      EVENT(XCB_PROPERTY_NOTIFY, event_handle_property_notify);
      EVENT(XCB_MAPPING_NOTIFY, event_handle_mapping_notify);
#undef EVENT
    }
}

/** Handle all events in the queue
 *
 * \param event_handler The event handler function to call for each event
 */
void
unagi_event_handle_poll_loop(void (*event_handler)(xcb_generic_event_t *))
{
  xcb_generic_event_t *event;
  while((event = xcb_poll_for_event(globalconf.connection)) != NULL)
    {
      event_handler(event);
      free(event);
    }
}
