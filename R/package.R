usdt_provider <- function(name) {
  stopifnot(is.character(name))
  ptr <- .Call(R_usdt_provider, name)
  structure(list(ptr = ptr, name = name), class = "usdt_provider")
}

print.usdt_provider <- function(x, ...) {
  enabled <- .Call(R_usdt_provider_is_enabled, x$ptr)
  cat("USDT Provider: ", x$name, if (enabled) " (loaded)", "\n", sep = "")
}

usdt_provider_enable <- function(provider) {
  stopifnot(inherits(provider, "usdt_provider"))
  .Call(R_usdt_provider_enable, provider$ptr)
  provider
}

usdt_provider_disable <- function(provider) {
  stopifnot(inherits(provider, "usdt_provider"))
  .Call(R_usdt_provider_disable, provider$ptr)
  provider
}

usdt_add_probe <- function(provider, name, ...) {
  stopifnot(inherits(provider, "usdt_provider"))
  ptr <- .External(R_usdt_add_probe, provider$ptr, name, ...)
  structure(list(ptr = ptr, name = name), class = "usdt_probe")
}

usdt_fire_probe <- function(probe, fun) {
  stopifnot(inherits(probe, "usdt_probe"))
  stopifnot(is.function(fun))
  .Call("R_usdt_fire_probe", probe$ptr, fun, parent.frame())
}

#' @useDynLib usdt, .registration = TRUE
NULL
