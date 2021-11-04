
#include <stdarg.h>
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>

#include "errors.h"

const char *sdtErrors[] = {
  "failed to create Elf shared library for provider '%s'",
  "failed to create temporary file '%s'",
  "failed to open shared library '%s': %s",
  "failed to load symbol '%s' for shared library '%s': %s",
  "failed to close shared library '%s' for provider '%s': %s",
};

void sdtSetError(SDTProvider_t *provider, SDTError_t error, ...) {
  va_list argp;

  if(provider->error != NULL) {
    free(provider->error);
    provider->error = NULL;
    provider->errno = noError;
  }

  va_start(argp, error);
  provider->errno = error;
  // Silence warnings about vasprintf's return value.
  if (vasprintf(&provider->error, sdtErrors[error], argp) < 0) {
    provider->error = "";
  }
  va_end(argp);
}
