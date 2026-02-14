#ifndef LOADER_H
#define LOADER_H

#include "ngcc_api.h"

typedef struct {
    void *handle;
    ngcc_api_t api;
} ngcc_library_t;

int ngcc_load_library(const char *lib_path, ngcc_library_t *out_lib);
void ngcc_unload_library(ngcc_library_t *lib);

#endif
