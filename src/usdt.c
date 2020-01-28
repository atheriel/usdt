#include <stdlib.h> /* for malloc */
#include <stdint.h> /* for uint64_t */
#include <Rinternals.h>

#ifdef __linux
#include "libstapsdt.h"
#define HAS_LIBSTAPSDT
#elif !defined(_WIN32)
#include "usdt.h"
#define HAS_LIBUSDT
#endif

/*
 *  libstapsdt <-> libusdt shim.
 *
 *  In my view, libusdt has the nicer API, so prefer that when possible.
 */

#ifdef HAS_LIBUSDT

#define probe_t usdt_probedef_t
#define provider_t usdt_provider_t
#define arg_t char *

size_t probe_argc(probe_t *probe)
{
  return probe->argc;
}

int probeIsEnabled(probe_t *probe)
{
  return usdt_is_enabled(probe->probe);
}

probe_t * providerAddProbe(provider_t *provider, const char *name, int argCount,
                           ...)
{
  va_list ap;
  va_start(ap, argCount);
  arg_t args[6] = {0};
  for(int i = 0; i < argCount; i++) {
    args[i] = va_arg(ap, arg_t);
  }
  probe_t *probe = usdt_create_probe(name, name, (size_t) argCount,
                                     (const char **) &args);
  if (probe) {
    usdt_provider_add_probe(provider, probe);
  }
  return probe;
}

void probeFire(probe_t *probe, ...)
{
  va_list ap;
  va_start(ap, probe);
  uint64_t args[6] = {0};
  for(int i = 0; i < probe->argc; i++) {
    args[i] = va_arg(ap, uint64_t);
  }

  usdt_fire_probe(probe->probe, probe->argc, (void *) &args);
}

#elif defined(HAS_LIBSTAPSDT)

#define probe_t SDTProbe_t
#define provider_t SDTProvider_t
#define arg_t ArgType_t

size_t probe_argc(probe_t *probe)
{
  return probe->argCount;
}

provider_t * usdt_create_provider(const char *name, const char *module)
{
  return providerInit(name);
}

void usdt_provider_free(provider_t *provider)
{
    providerDestroy(provider);
}

int usdt_provider_enable(provider_t *provider)
{
  return providerLoad(provider);
}

int usdt_provider_disable(provider_t *provider)
{
  return providerUnload(provider);
}

char * usdt_errstr(provider_t *provider)
{
  return provider->error;
}

#else /* No-op implementation shim. */

typedef struct provider_impl {
  int dummy;
} provider_t;

#define probe_t provider_t
#define arg_t char *

provider_t * usdt_create_provider(const char *name, const char *module)
{
  provider_t *out = malloc(sizeof(provider_t));
  return out;
}

void usdt_provider_free(provider_t *provider)
{
    free(provider);
}

int usdt_provider_enable(provider_t *provider) { return 0; }
int usdt_provider_disable(provider_t *provider) { return 0; }
int probeIsEnabled(probe_t *probe) { return 0; }
char * usdt_errstr(provider_t *provider) { return ""; }

probe_t * providerAddProbe(provider_t *provider, const char *name, int argCount,
                           ...)
{
  return provider;
}

#endif /* Shim. */

struct provider {
  provider_t *provider;
  int loaded;
};

static void R_finalize_provider(SEXP ptr)
{
  Rprintf("In the finalizer...\n");
  struct provider *p = (struct provider *) R_ExternalPtrAddr(ptr);
  if (p) {
    usdt_provider_disable(p->provider);
    usdt_provider_free(p->provider);
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
  p->provider = usdt_create_provider(name_str, name_str);
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
  if (usdt_provider_enable(p->provider) < 0) {
    Rf_error("Failed to enable USDT provider: %s.\n", usdt_errstr(p->provider));
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
  if (usdt_provider_disable(p->provider) < 0) {
    Rf_error("Failed to disable USDT provider: %s.\n", usdt_errstr(p->provider));
  }
  p->loaded = 0;

  return R_NilValue;
}

arg_t usdt_argtype_from_sexp(SEXP arg)
{
  switch(TYPEOF(arg)) {
  case LGLSXP:
    /* fallthrough */
  case INTSXP:
#ifdef HAS_LIBSTAPSDT
    return int32;
#else
    return "int";
#endif
  case STRSXP:
    /* fallthrough */
  case REALSXP:
#ifdef HAS_LIBSTAPSDT
    return uint64;
#else
    return "char *";
#endif
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
  probe_t *probe;
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

#if defined(HAS_LIBSTAPSDT) || defined(HAS_LIBUSDT)
  return R_MakeExternalPtr(probe, R_NilValue, R_NilValue);
#else
  /* Dummy value. */
  return Rf_ScalarLogical(TRUE);
#endif
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
#if defined(HAS_LIBSTAPSDT) || defined(HAS_LIBUSDT)
  probe_t *probe = (probe_t *) R_ExternalPtrAddr(ptr);
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
  if (arg_count != probe_argc(probe)) {
    Rf_error("Invalid number of probe arguments. Expected %d, got %d.\n",
             probe_argc(probe), arg_count);
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
#else
  /* Dummy value. */
  return Rf_ScalarLogical(FALSE);
#endif
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
