
<!-- README.md is generated from README.Rmd. Please edit that file -->

# usdt

<!-- badges: start -->

[![CRAN
status](https://www.r-pkg.org/badges/version/usdt)](https://cran.r-project.org/package=usdt)
<!-- badges: end -->

`usdt` brings User-level Statically Defined Tracing (USDT) to R,
allowing users to create low-ovehead probes that can be enabled at
runtime with external tracing tools such as BPF on Linux and Dtrace on
macOS and Solaris.

USDT was first popularized by
[Dtrace](http://dtrace.org/guide/chp-usdt.html) on Solaris (and later
Illumos, the BSDs, and macOS). Recently, it has become possible to use
USDT probes on Linux via BPF interfaces (such as `perf(1)`,
[bcc](https://github.com/iovisor/bcc), and
[bpftrace](https://github.com/iovisor/bpftrace)).

Normally, USDT probes are only possible in statically-compiled
languages, but clever workarounds in
[`libstapsdt`](https://github.com/sthima/libstapsdt) and
[`libusdt`](https://github.com/chrisa/libusdt/) make it possible to use
them in interpreted languages such as R, which is how this package
works.

`usdt` is currently Linux-only, but support for macOS, Solaris, and the
BSDs is planned via `libusdt`.

## Installation

`usdt` is not yet available on [CRAN](https://CRAN.R-project.org). For
now, you can install the development version from
[GitHub](https://github.com/) with:

``` r
# install.packages("devtools")
devtools::install_github("atheriel/usdt")
```

## Usage

USDT consists of a “provider” and one or more “probes”. Probes can be
“fired”, in which case R will emit an event – but only if a tracer
(such as BPF on Linux or Dtrace on Solaris and macOS) is listening.

Suppose you want to enable tracing of certain events in an R program,
such as a Shiny app. You would like R to emit information on the user’s
name and the total number of users each time a new one is added.

`usdt` has an R6 interface for working with providers and probes (since
these objects have reference semantics):

``` r
library(usdt)

p <- Provider$new("my_r_app")
p$add_probe("add_user", integer(), character(), character())
p$enable()
```

The arguments passed to `add_probe()` give the types of the parameters
we will use when we fire the probe:

``` r
p$fire("add_user", function() list(1L, "First", "Last"))
#> [1] FALSE
```

Normally, this will do nothing and return `FALSE`. However, if an
external tracer (such as Dtrace or BPF) has enabled this probe, the
function will be called and the result emitted by R as an event – and
the method will return `TRUE`.

This might be used by a tool like `bpftrace` to count new users in real
time:

``` console
$ sudo bpftrace -p 22368 -e 'usdt:*:my_r_app:add_user { @users = count() }'
Attaching 1 probe...
^C

@users: 2
```

Or print the available parameters as they are
added:

``` console
$ sudo bpftrace -p 22368 -e 'BEGIN { printf("%5s %10s %10s\n", "ID", "FIRST", "LAST") } usdt:*:my_r_app:add_user { printf("%5d %10s %10s\n", arg0, str(arg1), str(arg2)) }'
Attaching 2 probes...
   ID      FIRST       LAST
    3      Jerry     Garcia
    4     Herman   Melville
^C
```

The `fire()` method takes a callback function for one reason:
performance. In the example above, it would make little difference if
you passed the arguments directly. However, sometimes this information
may be expensive to compute – for example, the user ID might require a
database lookup – that you want to avoid if no one is listening.

## Credits

`usdt` is built on top of `libstapsdt`.

The callback API for `fire()` was inspired by the
[`dtrace-provider`](https://github.com/chrisa/node-dtrace-provider)
package for Node.js.

## License

`usdt` is made available under the terms of the GPL, version 2 or later.
The bundled `libstapsdt` library is available under the terms of the MIT
license.
