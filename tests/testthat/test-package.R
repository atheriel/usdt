testthat::test_that("Providers behave as expected", {
  p <- Provider$new("testthat")
  testthat::expect_silent(p$disable())

  p$add_probe("p1", integer(), logical(), character(), numeric())

  cb <- function() list(2L, FALSE, "string", 1.267)
  testthat::expect_false(p$fire("p1", cb))

  testthat::expect_error(
    p$add_probe(
      "p2", integer(), integer(), integer(), integer(), integer(), integer(),
      integer()
    ),
    regexp = "Probes cannot accept more than 6 arguments at present"
  )

  testthat::expect_error(
    p$add_probe("p2", raw()),
    regexp = "Can't pass R 'raw' objects to a probe."
  )

  testthat::expect_error(
    p$add_probe("p2", complex()),
    regexp = "Can't pass R 'complex' objects to a probe."
  )

  p$enable()
  testthat::expect_error(
    p$add_probe("p2", 1),
    regexp = "Probes cannot be added while the provider is loaded"
  )

  testthat::expect_false(p$fire("p1", cb))
})
