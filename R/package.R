#' USDT Providers
#'
#' @details
#'
#' User-level Statically Defined Tracing (USDT) consists of a "provider" and one
#' or more "probes". Probes can be "fired", in which case R will emit an event
#' -- but only if a tracer (such as BPF on Linux or Dtrace on Solaris and macOS)
#' is listening.
#'
#' USDT is a way to add opt-in runtime instrumentation to your application with
#' very little overhead.
#'
#' @examples
#'
#' p <- Provider$new("my-r-code")
#' p$add_probe("add-name", integer(), character(), character())
#' p$enable()
#'
#' # Fire an event. This will return FALSE, because nothing is
#' # listening at the moment.
#' p$fire("add-name", function() list(1L, "First", "Last"))
#'
#' @importFrom R6 R6Class
#' @export
Provider <- R6Class(
  "UsdtProvider",
  cloneable = FALSE,
  public = list(
    #' @param name A name for the provider.
    initialize = function(name) {
      stopifnot(is.character(name))
      private$ptr <- .Call(R_usdt_provider, name, PACKAGE = "usdt")
      private$name <- name
      private$enabled <- FALSE
    },

    #' @details Enable the provider, which makes it possible to fire probes.
    #'   New probes cannot be added while the provider is enabled.
    enable = function() {
      .Call(R_usdt_provider_enable, private$ptr, PACKAGE = "usdt")
      private$enabled <- TRUE
    },

    #' @details Disable the provider.
    disable = function() {
      .Call(R_usdt_provider_disable, private$ptr, PACKAGE = "usdt")
      private$enabled <- FALSE
    },

    #' @details Add a probe. Probes generally refer to a specific event -- e.g.
    #'   "request-start" or "item-updated" -- and can take up to six parameters.
    #'
    #' @param name A name for the probe.
    #' @param ... R types for parameters that will be passed to the probe when
    #'   fired. For example, \code{integer()} or \code{character()}.
    add_probe = function(name, ...) {
      stopifnot(is.character(name))
      probe <- .External(R_usdt_add_probe, private$ptr, name, ..., PACKAGE = "usdt")
      private$probes[[name]] <- probe
    },

    #' @details Fire a probe. If and only if a tracer is listening, this will
    #'   call the passed function and cause R to emit an event containing the
    #'   parameters it retuns. The number of parameters must match the call to
    #'   \code{add_probe()}.
    #'
    #' @param probe The name of a probe.
    #' @param fun A function that when called will return a list of parameters to
    #'   be emitted by R.
    fire = function(probe, fun) {
      p <- private$probes[[probe]]
      if (is.null(p)) {
        stop("'", probe, "' does not name a known probe.")
      }
      stopifnot(is.function(fun))
      .Call("R_usdt_fire_probe", p, fun, parent.frame(), PACKAGE = "usdt")
    }
  ),
  private = list(
    name = character(),
    ptr = NULL,
    enabled = FALSE,
    probes = setNames(list(), character())
  )
)

#' @useDynLib usdt, .registration = TRUE
NULL
