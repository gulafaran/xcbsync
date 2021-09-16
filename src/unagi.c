/* -*-mode:c;coding:utf-8; c-basic-offset:2;fill-column:70;c-file-style:"gnu"-*-
 *
 * Copyright (C) 2009 Arnaud "arnau" Fontaine <arnau@mini-dweeb.org>
 *
 * This  program is  free  software: you  can  redistribute it  and/or
 * modify  it under the  terms of  the GNU  General Public  License as
 * published by the Free Software  Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 *
 * You should have  received a copy of the  GNU General Public License
 *  along      with      this      program.      If      not,      see
 *  <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>

#include <xcb/xcb.h>
#include <xcb/composite.h>
#include <xcb/xfixes.h>
#include <xcb/damage.h>
#include <xcb/randr.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_keysyms.h>

#include <basedir.h>
#include <basedir_fs.h>
#include <confuse.h>

#include "structs.h"
#include "display.h"
#include "event.h"
#include "atoms.h"
#include "util.h"
#include "plugin.h"
#include "key.h"
#include "vsync.h"

unagi_conf_t globalconf;

static inline void display_help(void)
{
    printf("Usage: " PACKAGE_NAME "[options]\n\
    -h, --help                show help\n\
    -v, --vsync               turn on vsync (default drm)\n\
    -o, --opacity             turn on opacity (default off)\n\
    -d, --drm                 use libdrm for vsync\n\
    -g, --opengl              use opengl for vsync\n\
    -k, --vulkan              use vulkan for vsync\n");
    exit(EXIT_SUCCESS);
}

static void parse_command_line_parameters(int argc, char **argv) {
    const struct option long_options[] = {
        { "help", 0, NULL, 'h' },
        { "vsync", 0, NULL, 'v' },
        { "opacity", 0, NULL, 'o' },
        { "drm", 0, NULL, 'd' },
        { "opengl", 0, NULL, 'g' },
        { "vulkan", 0, NULL, 'k' },
        { NULL, 0, NULL, 0 }
    };

    int opt;
    while((opt = getopt_long(argc, argv, "hvodgk:", long_options, NULL)) != -1) {
        switch(opt) {
        case 'h':
            display_help();
        break;
        case 'v':
            globalconf.vsync = true;
        break;
        case 'o':
            //#TODO enable opacity
        break;
        case 'd':
            globalconf.vsync_drm = true;
        break;
        case 'g':
            globalconf.vsync_gl = true;
        break;
        case 'k':
            globalconf.vsync_vulkan = true;
        break;
        default:
            display_help();
        break;
        }
    }

    //#TODO remove the dlloading buisness.
    /* Get the rendering backend path if not given in the command line parameters */
    if(!globalconf.rendering_dir)
        globalconf.rendering_dir = strdup(RENDERING_DIR);

    /* Get  the  plugins   path  if  not  given  in   the  command  line parameters */
    if(!globalconf.plugins_dir)
        globalconf.plugins_dir = strdup(PLUGINS_DIR);
}

static void exit_cleanup(void) {
    unagi_debug("Cleaning resources up");

    unagi_plugin_unload_all();
    unagi_window_list_cleanup();
    unagi_rendering_unload();

    xcb_key_symbols_free(globalconf.keysyms);
    xcb_ewmh_connection_wipe(&globalconf.ewmh);

    for(unsigned int i = 0; i < globalconf.crtc_len; i++)
        free(globalconf.crtc[i]);

    free(globalconf.crtc);
    free(globalconf.rendering_dir);
    free(globalconf.plugins_dir);

    if(globalconf.connection) {
        /* Destroy CM window, thus giving up _NET_WM_CM_Sn ownership */
        if(globalconf.cm_window != XCB_NONE)
            xcb_destroy_window(globalconf.connection, globalconf.cm_window);


        xcb_aux_sync(globalconf.connection);
        xcb_disconnect(globalconf.connection);
    }

    ev_loop_destroy(globalconf.event_loop);
    vsync_cleanup();
}

static void exit_on_signal(struct ev_loop *loop, ev_signal *w, int revents) {
  ev_break(loop, EVBREAK_ALL);
}

static void
_unagi_paint_callback(EV_P_ ev_timer *w, int revents)
{
  for(unagi_plugin_t *plugin = globalconf.plugins; plugin; plugin = plugin->next)
    if(plugin->enable && plugin->vtable->activated && plugin->vtable->pre_paint)
      (*plugin->vtable->pre_paint)();

  /* Now paint the windows */
  if(globalconf.damaged || globalconf.force_repaint)
    {
      if(globalconf.force_repaint)
        unagi_display_reset_damaged();

      unagi_window_paint_all(globalconf.windows);
      if(!globalconf.force_repaint)
        unagi_display_reset_damaged();

      const float paint_time = (float) (ev_time() - ev_now(globalconf.event_loop));

      if(!globalconf.force_repaint)
        {
          globalconf.paint_time_sum += paint_time;

          const float current_average = globalconf.paint_time_sum /
            (float) ++globalconf.paint_counter;

          /* The next repaint  interval is computed from  the refresh rate
             interval and repaint global average time */
          const float current_interval = globalconf.refresh_rate_interval -
            current_average;

          /* When repainting the whole screen, the painting may have taken
             a  long time  but the  next repaint  should not  be too  soon
             neither */
          if(current_interval < MINIMUM_REPAINT_INTERVAL)
            globalconf.repaint_interval = globalconf.refresh_rate_interval;
          else
            globalconf.repaint_interval = current_interval;
        }

      for(unagi_plugin_t *plugin = globalconf.plugins; plugin; plugin = plugin->next)
        if(plugin->enable && plugin->vtable->activated && plugin->vtable->post_paint)
          (*plugin->vtable->post_paint)();

      /* Rearm the paint timer watcher */
      globalconf.event_paint_timer_watcher.repeat = globalconf.repaint_interval;
      ev_timer_again(globalconf.event_loop, &globalconf.event_paint_timer_watcher);

      /* Some events may have been queued while calling this callback,
         so make sure by calling this watcher again */
      ev_invoke(globalconf.event_loop, &globalconf.event_io_watcher, 0);
      globalconf.force_repaint = false;
    }
}

static void
_unagi_io_callback(EV_P_ ev_io *w, int revents)
{
  /* ev_now_update()  is   only  called  through  ev_run()   but  with
     ev_invoke  (as used  on startup  and to  process events  received
     during painting function call */
  if(revents <= 0)
    ev_now_update(globalconf.event_loop);

  ev_tstamp now = ev_now(globalconf.event_loop);

  /* Check X connection to avoid SIGSEGV */
  if(xcb_connection_has_error(globalconf.connection))
    unagi_fatal("X connection invalid");

  /* Process all events in the queue because before painting, all the
     DamageNotify have to be received */
  xcb_generic_event_t *event;
  while((event = xcb_poll_for_event(globalconf.connection)) != NULL)
    {
      unagi_event_handle(event);
      free(event);

      /* Stop processing events (but not  on startup as all the events
         must be processed) if the  repaint interval has been reached,
         otherwise DamageNotify  keep being processed forever  if many
         are received */
      if(revents != -1 && (ev_time() - now + 0.001) > globalconf.repaint_interval)
        {
          /* Process events remaining in the queue without polling the
             X connection */
          while((event = xcb_poll_for_queued_event(globalconf.connection)))
            {
              unagi_event_handle(event);
              free(event);
            }
          break;
        }
    }
}

static void init_ev(void) {
    /* libev event loop */
    globalconf.event_loop = ev_default_loop(EVFLAG_NOINOTIFY | EVFLAG_NOSIGMASK);

    /* Set up signal handlers */
    ev_signal sighup;
    ev_signal_init(&sighup, exit_on_signal, SIGHUP);
    ev_signal_start(globalconf.event_loop, &sighup);
    ev_unref(globalconf.event_loop);

    ev_signal sigint;
    ev_signal_init(&sigint, exit_on_signal, SIGINT);
    ev_signal_start(globalconf.event_loop, &sigint);
    ev_unref(globalconf.event_loop);

    ev_signal sigterm;
    ev_signal_init(&sigterm, exit_on_signal, SIGTERM);
    ev_signal_start(globalconf.event_loop, &sigterm);
    ev_unref(globalconf.event_loop);

    /* Cleanup resources upon normal exit */
    atexit(exit_cleanup);
}

static void compositor_connect(void) {
    globalconf.connection = xcb_connect(NULL, &globalconf.screen_nbr);
    if(xcb_connection_has_error(globalconf.connection)) {
        unagi_fatal("Cannot open display");
    }

    /* Get the root window */
    globalconf.screen = xcb_aux_get_screen(globalconf.connection, globalconf.screen_nbr);
}

static void compositor_check_owner(void) {
    /* First check whether there is already a Compositing Manager (ICCCM) */
    xcb_get_selection_owner_cookie_t wm_cm_owner_cookie = xcb_ewmh_get_wm_cm_owner(&globalconf.ewmh, globalconf.screen_nbr);
    /* Check ownership for WM_CM_Sn before actually claiming it (ICCCM) */
    xcb_window_t wm_cm_owner_win;
    if(xcb_ewmh_get_wm_cm_owner_reply(&globalconf.ewmh, wm_cm_owner_cookie, &wm_cm_owner_win, NULL) && wm_cm_owner_win != XCB_NONE)
        unagi_fatal("A compositing manager is already active (window=%jx)", (uintmax_t) wm_cm_owner_win);
}

int main(int argc, char **argv) {
    memset(&globalconf, 0, sizeof(globalconf));

    globalconf.vsync = false;
    globalconf.vsync_drm = false;
    globalconf.vsync_gl = false;
    globalconf.vsync_vulkan = false;

    parse_command_line_parameters(argc, argv);
    init_ev();
    compositor_connect();

    if(globalconf.vsync){
        vsync_init();
    }

    /* Send requests for EWMH atoms initialisation */
    xcb_intern_atom_cookie_t *ewmh_cookies = unagi_atoms_init();

    /* Pre-initialisation of the rendering backend */
    if(!unagi_rendering_load()) {
        free(ewmh_cookies);
        unagi_fatal("Can't initialise rendering backend");
    }

    /* Get replies for EWMH atoms initialisation */
    if(!unagi_atoms_init_finalise(ewmh_cookies))
    /* No need to  free ewmh_cookies in case of  error as it's already
       handles by xcb-ewmh when getting the replies */
        unagi_fatal("Cannot initialise atoms");

    /* Initialiase libev event watcher on XCB connection */
    ev_io_init(&globalconf.event_io_watcher, _unagi_io_callback, xcb_get_file_descriptor(globalconf.connection), EV_READ);
    ev_io_start(globalconf.event_loop, &globalconf.event_io_watcher);

    /* Flush the X events queue before blocking */
    xcb_flush(globalconf.connection);

  /* All the plugins given in the configuration file

     TODO: Only there because render_init() needs to be able to look
     for opacity plugin */
    unagi_plugin_load_all();

    /* Initialise   extensions   based on the cache and perform initialisation of the rendering backend */
    unagi_display_init_extensions();
    if(!(*globalconf.rendering->init)())
        return EXIT_FAILURE;

    compositor_check_owner();

    /* Now send requests to register the CM */
    unagi_display_register_cm();
  
    /* Check  extensions  version   and  finish  initialisation  of  the rendering backend */
    unagi_display_init_extensions_finalise();
    if(!(*globalconf.rendering->init_finalise)())
        return EXIT_FAILURE;

    xcb_randr_get_screen_info_cookie_t randr_screen_info_cookie = { .sequence = 0 };
    xcb_randr_get_screen_resources_cookie_t randr_screen_resources_cookie = { .sequence = 0 };
    if(globalconf.extensions.randr) {
      /* Get the screen refresh rate to calculate the interval between
         painting */
        randr_screen_info_cookie = xcb_randr_get_screen_info_unchecked(globalconf.connection, globalconf.screen->root);
        randr_screen_resources_cookie = xcb_randr_get_screen_resources_unchecked(globalconf.connection, globalconf.screen->root);
        xcb_randr_select_input(globalconf.connection, globalconf.screen->root, XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
    }

  /* Validate  errors   and  get  PropertyNotify   needed  to  acquire
     _NET_WM_CM_Sn ownership */
    xcb_aux_sync(globalconf.connection);
    unagi_event_handle_poll_loop(unagi_event_handle_startup);

    globalconf.keysyms = xcb_key_symbols_alloc(globalconf.connection);
    xcb_get_modifier_mapping_cookie_t key_mapping_cookie = xcb_get_modifier_mapping_unchecked(globalconf.connection);

    /* Finish CM X registration */
    if(!unagi_display_register_cm_finalise())
        unagi_fatal("Could not acquire _NET_WM_CM_Sn ownership");

    /* Grab the server before performing redirection and get the tree of
       windows  to ensure  there  won't  be anything  else  at the  same time */
    xcb_grab_server(globalconf.connection);

    /* Set the refresh rate (necessary to define painting intervals) and
       screen sizes and geometries */
    unagi_display_update_screen_information(randr_screen_info_cookie, randr_screen_resources_cookie);

    /* Now redirect windows and add existing windows */
    unagi_display_init_redirect();

    /* Validate errors handlers during redirect */
    xcb_aux_sync(globalconf.connection);
    unagi_event_handle_poll_loop(unagi_event_handle_startup);

    /* Manage existing windows */
    unagi_display_init_redirect_finalise();

    xcb_ungrab_server(globalconf.connection);

    unagi_plugin_check_requirements();

    globalconf.repaint_interval = globalconf.refresh_rate_interval;

    /* Initialise painting timer depending on the screen refresh rate */
    ev_init(&globalconf.event_paint_timer_watcher, _unagi_paint_callback);

    /* Painting must have precedence over events processing */
    ev_set_priority(&globalconf.event_paint_timer_watcher, EV_MAXPRI);

    /* Set the initial repaint interval to the screen refresh rate, it
       will be adjust later on according to the repaint times */
    globalconf.event_paint_timer_watcher.repeat = globalconf.repaint_interval;
    ev_timer_again(globalconf.event_loop, &globalconf.event_paint_timer_watcher);
 
    /* Get the lock masks reply of the request previously sent */ 
    unagi_key_lock_mask_get_reply(key_mapping_cookie);

    /* Flush existing  requests before  the loop as  DamageNotify events
       may have been received in the meantime */
    xcb_flush(globalconf.connection);

    unagi_window_paint_all(globalconf.windows);
    ev_invoke(globalconf.event_loop, &globalconf.event_io_watcher, -1);

    /* Main event and error loop */
    ev_run(globalconf.event_loop, 0);

    ev_io_stop(globalconf.event_loop, &globalconf.event_io_watcher);
    ev_timer_stop(globalconf.event_loop, &globalconf.event_paint_timer_watcher);

    return EXIT_SUCCESS;
}
