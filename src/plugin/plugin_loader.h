#ifndef CORE_PLUGIN_LOADER_H
#define CORE_PLUGIN_LOADER_H

#include "plugin_api.h"

#define PLUGIN_LIB_DIR "plugin-libs"
#define PLUGIN_MANIFEST_FILE "manifest.config"
#define PLUGIN_DATA_FILE "types.data"

plugin_handle_t* plugin_load(const char *plugin_name, const char* path, const char* manifest_path, const char* data_path);
void plugin_unload(plugin_handle_t* h);

#endif // CORE_PLUGIN_LOADER_H
