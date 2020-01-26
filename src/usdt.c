#include <stdlib.h> /* for malloc */
#include <stdint.h> /* for uint64_t */
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

ArgType_t usdt_argtype_from_sexp(SEXP arg)
{
  switch(TYPEOF(arg)) {
  case LGLSXP:
    /* fallthrough */
  case INTSXP:
    return int32;
  case STRSXP:
    /* fallthrough */
  case REALSXP:
    return uint64;
  default:
    Rf_error("Can't pass R '%s' objects to a probe.", Rf_type2char(TYPEOF(arg)));
  }
}

SEXP R_usdt_add_probe(SEXP args)
{
  args = CDR(args);
  SEXP provider = CAR(args);
  SEXP name = CADR(args);
  struct provider *p = (struct provider *) R_ExternalPtrAddr(provider);
  const char *name_str = CHAR(Rf_asChar(name));
  if (!p) {
    Rf_error("Invalid USDT provider.\n");
  }
  if (p->loaded) {
    Rf_error("Probes cannot be added while the provider is loaded.\n");
  }
  args = CDDR(args);
  int arg_count = Rf_length(args);
  if (arg_count > 6) {
    Rf_error("Probes cannot accept more than 6 arguments at present.");
  }
  SDTProbe_t *probe;
  SEXP first;

  switch (arg_count) {
  case 1:
    probe = providerAddProbe(p->provider, name_str, 1,
                             usdt_argtype_from_sexp(CAR(args)));
    break;
  case 2:
    probe = providerAddProbe(p->provider, name_str, 2,
                             usdt_argtype_from_sexp(CAR(args)),
                             usdt_argtype_from_sexp(CADR(args)));
    break;
  case 3:
    probe = providerAddProbe(p->provider, name_str, 3,
                             usdt_argtype_from_sexp(CAR(args)),
                             usdt_argtype_from_sexp(CADR(args)),
                             usdt_argtype_from_sexp(CADDR(args)));
    break;
  case 4:
    probe = providerAddProbe(p->provider, name_str, 4,
                             usdt_argtype_from_sexp(CAR(args)),
                             usdt_argtype_from_sexp(CADR(args)),
                             usdt_argtype_from_sexp(CADDR(args)),
                             usdt_argtype_from_sexp(CADDDR(args)));
    break;
  case 5:
    first = CAR(args);
    args = CDR(args);
    probe = providerAddProbe(p->provider, name_str, 5,
                             usdt_argtype_from_sexp(first),
                             usdt_argtype_from_sexp(CAR(args)),
                             usdt_argtype_from_sexp(CADR(args)),
                             usdt_argtype_from_sexp(CADDR(args)),
                             usdt_argtype_from_sexp(CADDDR(args)));
    break;
  case 6:
    first = CAR(args);
    args = CDR(args);
    probe = providerAddProbe(p->provider, name_str, 6,
                             usdt_argtype_from_sexp(first),
                             usdt_argtype_from_sexp(CAR(args)),
                             usdt_argtype_from_sexp(CADR(args)),
                             usdt_argtype_from_sexp(CADDR(args)),
                             usdt_argtype_from_sexp(CADDDR(args)),
                             usdt_argtype_from_sexp(CAD4R(args)));
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

uint64_t usdt_arg_from_sexp(SEXP arg)
{
  switch(TYPEOF(arg)) {
  case LGLSXP:
    return (uint64_t) LOGICAL(arg)[0];
  case INTSXP:
    return (uint64_t) INTEGER(arg)[0];
  case STRSXP:
    return (uint64_t) CHAR(Rf_asChar(arg));
  case REALSXP: {
    /* Format reals as strings. */
    SEXP str = PROTECT(Rf_coerceVector(arg, STRSXP));
    UNPROTECT(1);
    return (uint64_t) CHAR(Rf_asChar(str));
  }
  default:
    Rf_error("Can't pass R '%s' objects to a probe.", Rf_type2char(TYPEOF(arg)));
  }
}

SEXP R_usdt_fire_probe(SEXP ptr, SEXP fun, SEXP env)
{
  SDTProbe_t *probe = (SDTProbe_t *) R_ExternalPtrAddr(ptr);
  if (ptr == R_NilValue || !probe) {
    Rf_error("Invalid USDT probe.\n");
  }
  if (!probeIsEnabled(probe)) {
    return Rf_ScalarLogical(FALSE);
  }
  SEXP fcall = PROTECT(Rf_lang1(fun));
  SEXP args = PROTECT(Rf_eval(fcall, env));
  if (!Rf_isNewList(args)) {
    Rf_error("Expected the callback to return a list, got '%s'.",
             Rf_type2char(TYPEOF(args)));
  }
  size_t arg_count = Rf_length(args);
  if (arg_count != probe->argCount) {
    Rf_error("Invalid number of probe arguments. Expected %d, got %d.\n",
             probe->argCount, arg_count);
  }

  switch (arg_count) {
  case 1:
    probeFire(probe, usdt_arg_from_sexp(VECTOR_ELT(args, 0)));
    break;
  case 2:
    probeFire(probe, usdt_arg_from_sexp(VECTOR_ELT(args, 0)),
              usdt_arg_from_sexp(VECTOR_ELT(args, 1)));
    break;
  case 3:
    probeFire(probe, usdt_arg_from_sexp(VECTOR_ELT(args, 0)),
              usdt_arg_from_sexp(VECTOR_ELT(args, 1)),
              usdt_arg_from_sexp(VECTOR_ELT(args, 2)));
    break;
  case 4:
    probeFire(probe, usdt_arg_from_sexp(VECTOR_ELT(args, 0)),
              usdt_arg_from_sexp(VECTOR_ELT(args, 1)),
              usdt_arg_from_sexp(VECTOR_ELT(args, 2)),
              usdt_arg_from_sexp(VECTOR_ELT(args, 3)));
    break;
  case 5:
    probeFire(probe, usdt_arg_from_sexp(VECTOR_ELT(args, 0)),
              usdt_arg_from_sexp(VECTOR_ELT(args, 1)),
              usdt_arg_from_sexp(VECTOR_ELT(args, 2)),
              usdt_arg_from_sexp(VECTOR_ELT(args, 3)),
              usdt_arg_from_sexp(VECTOR_ELT(args, 4)));
    break;
  case 6:
    probeFire(probe, usdt_arg_from_sexp(VECTOR_ELT(args, 0)),
              usdt_arg_from_sexp(VECTOR_ELT(args, 1)),
              usdt_arg_from_sexp(VECTOR_ELT(args, 2)),
              usdt_arg_from_sexp(VECTOR_ELT(args, 3)),
              usdt_arg_from_sexp(VECTOR_ELT(args, 4)),
              usdt_arg_from_sexp(VECTOR_ELT(args, 5)));
    break;
  default:
    probeFire(probe);
    break;
  }

  UNPROTECT(2);
  return Rf_ScalarLogical(TRUE);
}

static const R_CallMethodDef usdt_entries[] = {
  {"R_usdt_provider", (DL_FUNC) &R_usdt_provider, 1},
  {"R_usdt_provider_is_enabled", (DL_FUNC) &R_usdt_provider_is_enabled, 1},
  {"R_usdt_provider_enable", (DL_FUNC) &R_usdt_provider_enable, 1},
  {"R_usdt_provider_disable", (DL_FUNC) &R_usdt_provider_disable, 1},
  {"R_usdt_fire_probe", (DL_FUNC) &R_usdt_fire_probe, 3},
  {NULL, NULL, 0}
};

static const R_ExternalMethodDef usdt_entries_ext[] = {
  {"R_usdt_add_probe", (DL_FUNC) &R_usdt_add_probe, -1},
  {NULL, NULL, 0}
};

void R_init_usdt(DllInfo *info) {
  R_registerRoutines(info, NULL, usdt_entries, NULL, usdt_entries_ext);
  R_useDynamicSymbols(info, FALSE);
}
