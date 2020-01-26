testthat::test_that("Providers behalve as expected", {
  p <- usdt_provider("testthat")

  testthat::expect_silent(usdt_provider_disable(p))
  testthat::expect_silent(usdt_provider_enable(p))
  testthat::expect_silent(usdt_provider_enable(p))
  testthat::expect_silent(usdt_provider_disable(p))
})

testthat::test_that("Probes behalve as expected", {
  p <- usdt_provider("testthat")
  probe <- usdt_add_probe(p, "p1", integer(), logical(), character(), numeric())

  testthat::expect_false(usdt_fire_probe(probe, 2L, FALSE, "string", 1.267))

  testthat::expect_error(
    usdt_add_probe(
      p, "p2", integer(), integer(), integer(), integer(), integer(), integer(),
      integer()
    ),
    regexp = "Probes cannot accept more than 6 arguments at present"
  )

  testthat::expect_error(
    usdt_add_probe(p, "p2", raw(), as.Date(Sys.time())),
    regexp = "Can't pass R 'raw' objects to a probe."
  )

  testthat::expect_error(
    usdt_add_probe(p, "p2", complex()),
    regexp = "Can't pass R 'complex' objects to a probe."
  )

  usdt_provider_enable(p)
  testthat::expect_error(
    usdt_add_probe(p, "p2", 1),
    regexp = "Probes cannot be added while the provider is loaded"
  )

  testthat::expect_false(usdt_fire_probe(probe, 2L, FALSE, "string", 1.267))
})
