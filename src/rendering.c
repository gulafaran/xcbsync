#include <dlfcn.h>

#include "rendering.h"
#include "structs.h"
#include "plugin_common.h"
#include "util.h"

/** Load the  default backend or fallback  on another one  if there is
 *  any error
 *
 * \return True if a rendering backend was successfully loaded
 */
bool
unagi_rendering_load(void)
{
  /* Clear any existing error */
  dlerror();

  globalconf.rendering_dlhandle = unagi_plugin_common_dlopen(globalconf.rendering_dir, "render");

  char *error;
  if((error = dlerror()))
    {
      unagi_fatal_no_exit("Can't load rendering backend: %s", error);
      return false;
    }

  /* Get the backend functions addresses given in a structure in it */
  globalconf.rendering = dlsym(globalconf.rendering_dlhandle,
			       "rendering_functions");

  if((error = dlerror()))
    {
      unagi_fatal_no_exit("%s", error);
      return false;
    }

  return true;
}

/** Unload the rendering backend */
void
unagi_rendering_unload(void)
{
  if(!globalconf.rendering_dlhandle)
    return;

  dlclose(globalconf.rendering_dlhandle);
}
