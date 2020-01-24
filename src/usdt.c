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

SEXP R_usdt_add_probe(SEXP provider, SEXP name, SEXP args)
{
  const char *name_str = CHAR(Rf_asChar(name));
  struct provider *p = (struct provider *) R_ExternalPtrAddr(provider);
  if (!p) {
    Rf_error("Invalid USDT provider.\n");
  }
  if (p->loaded) {
    Rf_error("Probes cannot be added while the provider is loaded.\n");
  }
  int arg_count = Rf_asInteger(args);
  if (arg_count > 6) {
    Rf_error("Probes cannot accept more than 6 arguments at present.");
  }
  SDTProbe_t *probe;

  switch (arg_count) {
  case 1:
    probe = providerAddProbe(p->provider, name_str, 1, int32);
    break;
  case 2:
    probe = providerAddProbe(p->provider, name_str, 2, int32, int32);
    break;
  case 3:
    probe = providerAddProbe(p->provider, name_str, 3, int32, int32, int32);
    break;
  case 4:
    probe = providerAddProbe(p->provider, name_str, 4, int32, int32, int32,
                             int32);
    break;
  case 5:
    probe = providerAddProbe(p->provider, name_str, 5, int32, int32, int32,
                             int32, int32);
    break;
  case 6:
    probe = providerAddProbe(p->provider, name_str, 6, int32, int32, int32,
                             int32, int32, int32);
    break;
  default:
    probe = providerAddProbe(p->provider, name_str, 0);
    break;
  }
  if (!probe) {
    Rf_error("Failed to create USDT probe.\n");
  }

  SEXP ptr = PROTECT(R_MakeExternalPtr(probe, R_NilValue, R_NilValue));
  UNPROTECT(1);
  return ptr;
}

SEXP R_usdt_fire_probe(SEXP args)
{
  args = CDR(args);
  SEXP ptr = CAR(args);
  SDTProbe_t *probe = (SDTProbe_t *) R_ExternalPtrAddr(ptr);
  if (ptr == R_NilValue || !probe) {
    Rf_error("Invalid USDT probe.\n");
  }
  if (!probeIsEnabled(probe)) {
    return Rf_ScalarLogical(FALSE);
  }
  args = CDR(args);
  size_t arg_count = Rf_length(args);
  if (arg_count != probe->argCount) {
    Rf_error("Invalid number of arguments. Expected %d, got %d.\n",
             probe->argCount, arg_count);
  }
  SEXP first;

  switch (arg_count) {
  case 1:
    probeFire(probe, Rf_asInteger(CAR(args)));
    break;
  case 2:
    probeFire(probe, Rf_asInteger(CAR(args)), Rf_asInteger(CADR(args)));
    break;
  case 3:
    probeFire(probe, Rf_asInteger(CAR(args)), Rf_asInteger(CADR(args)),
              Rf_asInteger(CADDR(args)));
    break;
  case 4:
    probeFire(probe, Rf_asInteger(CAR(args)), Rf_asInteger(CADR(args)),
              Rf_asInteger(CADDR(args)), Rf_asInteger(CADDDR(args)));
    break;
  case 5:
    first = CAR(args);
    args = CDR(args);
    probeFire(probe, first, Rf_asInteger(CAR(args)),
              Rf_asInteger(CADR(args)), Rf_asInteger(CADDR(args)),
              Rf_asInteger(CADDDR(args)));
    break;
  case 6:
    first = CAR(args);
    args = CDR(args);
    probeFire(probe, first, Rf_asInteger(CAR(args)),
              Rf_asInteger(CADR(args)), Rf_asInteger(CADDR(args)),
              Rf_asInteger(CADDDR(args)), Rf_asInteger(CAD4R(args)));
    break;
  default:
    probeFire(probe);
    break;
  }

  return Rf_ScalarLogical(TRUE);
}

static const R_CallMethodDef usdt_entries[] = {
  {"R_usdt_provider", (DL_FUNC) &R_usdt_provider, 1},
  {"R_usdt_provider_is_enabled", (DL_FUNC) &R_usdt_provider_is_enabled, 1},
  {"R_usdt_provider_enable", (DL_FUNC) &R_usdt_provider_enable, 1},
  {"R_usdt_provider_disable", (DL_FUNC) &R_usdt_provider_disable, 1},
  {"R_usdt_add_probe", (DL_FUNC) &R_usdt_add_probe, 3},
  {NULL, NULL, 0}
};

static const R_ExternalMethodDef usdt_entries_ext[] = {
  {"R_usdt_fire_probe", (DL_FUNC) &R_usdt_fire_probe, 1},
  {NULL, NULL, 0}
};

void R_init_usdt(DllInfo *info) {
  R_registerRoutines(info, NULL, usdt_entries, NULL, usdt_entries_ext);
  R_useDynamicSymbols(info, FALSE);
}
