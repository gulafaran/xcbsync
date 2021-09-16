#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#include "plugin_common.h"

/** Compute  the plugin  location by  concatenating the  directory and
 *  plugin name and then call dlopen()
 *
 * \param dir The plugin directory
 * \param name The plugin name
 * \return Handle for the plugin
 */
void *
unagi_plugin_common_dlopen(const char *dir, const char *name)
{
  /* Get the length of the plugin filename */
  const size_t path_len = strlen(name) + strlen(dir) + sizeof(".so");

  /* Get the actual plugin filename */
  char path[path_len];
  snprintf(path, path_len, "%s%s.so", dir, name);

  return dlopen(path, RTLD_LAZY);
}
