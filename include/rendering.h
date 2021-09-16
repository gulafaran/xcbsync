#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "window.h"

/** Functions exported by the rendering backend */
typedef struct
{
  /** Initialisation routine */
  bool (*init) (void);
  /** Second step of the initialisation routine */
  bool (*init_finalise) (void);
  /** Reset the root Window background */
  void (*reset_background) (void);
  /** Paint the root background to the root window */
  void (*paint_background) (void);
  /** Paint a given window */
  void (*paint_window) (unagi_window_t *);
  /** Paint all the windows on the root window */
  void (*paint_all) (void);
  /** Check whether the given request is backend-specific */
  bool (*is_request) (const uint8_t);
  /** Get the request label of a backend request */
  const char *(*get_request_label) (const uint16_t);
  /** Get the error label of a backend error */
  const char *(*get_error_label) (const uint8_t);
  /** Free resources associated with a window when the Pixmap is freed */
  void (*free_window_pixmap) (unagi_window_t *);
  /** Free resources associated with a window */
  void (*free_window) (unagi_window_t *);
} unagi_rendering_t;

bool unagi_rendering_load(void);
void unagi_rendering_unload(void);
