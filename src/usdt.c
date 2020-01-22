#include <stdlib.h> /* for malloc */
#include <Rinternals.h>
#include "libstapsdt.h"

struct provider {
  SDTProvider_t *provider;
  int loaded;
};

static void R_finalize_provider(SEXP ptr)
{
  Rprintf("In the finalizer...\n");
  struct provider *p = (struct provider *) R_ExternalPtrAddr(ptr);
  if (p) {
    providerDestroy(p->provider);
    p->provider = NULL;
    free(p);
    p = NULL;
  }
  R_ClearExternalPtr(ptr);
}

SEXP R_usdt_provider(SEXP name)
{
  const char *name_str = CHAR(Rf_asChar(name));
  struct provider *p = malloc(sizeof(struct provider));
  p->loaded = 0;
  p->provider = providerInit(name_str);
  if (!p->provider) {
    Rf_error("Failed to create USDT provider.\n");
  }

  SEXP ptr = PROTECT(R_MakeExternalPtr(p, R_NilValue, R_NilValue));
  R_RegisterCFinalizerEx(ptr, R_finalize_provider, 1);
  UNPROTECT(1);
  return ptr;
}

SEXP R_usdt_provider_is_enabled(SEXP ptr)
{
  struct provider *p = (struct provider *) R_ExternalPtrAddr(ptr);
  if (!p) {
    Rf_error("Invalid USDT provider.\n");
  }
  return Rf_ScalarLogical(p->loaded);
}

SEXP R_usdt_provider_enable(SEXP ptr)
{
  struct provider *p = (struct provider *) R_ExternalPtrAddr(ptr);
  if (!p) {
    Rf_error("Invalid USDT provider.\n");
  }
  if (p->loaded) {
    /* Already loaded. */
    return R_NilValue;
  }
  if (providerLoad(p->provider) < 0) {
    Rf_error("Failed to enable USDT provider: %s.\n", p->provider->error);
  }
  p->loaded = 1;

  return R_NilValue;
}

SEXP R_usdt_provider_disable(SEXP ptr)
{
  struct provider *p = (struct provider *) R_ExternalPtrAddr(ptr);
  if (!p) {
    Rf_error("Invalid USDT provider.\n");
  }
  if (!p->loaded) {
    /* Already unloaded. */
    return R_NilValue;
  }
  if (providerUnload(p->provider) < 0) {
    Rf_error("Failed to disable USDT provider: %s.\n", p->provider->error);
  }
  p->loaded = 0;

  return R_NilValue;
}

static const R_CallMethodDef usdt_entries[] = {
  {"R_usdt_provider", (DL_FUNC) &R_usdt_provider, 1},
  {"R_usdt_provider_is_enabled", (DL_FUNC) &R_usdt_provider_is_enabled, 1},
  {"R_usdt_provider_enable", (DL_FUNC) &R_usdt_provider_enable, 1},
  {"R_usdt_provider_disable", (DL_FUNC) &R_usdt_provider_disable, 1},
  {NULL, NULL, 0}
};

void R_init_usdt(DllInfo *info) {
  R_registerRoutines(info, NULL, usdt_entries, NULL, NULL);
  R_useDynamicSymbols(info, FALSE);
}
