testthat::test_that("Providers behalve as expected", {
  p <- usdt_provider("testthat")

  testthat::expect_silent(usdt_provider_disable(p))
  testthat::expect_silent(usdt_provider_enable(p))
  testthat::expect_silent(usdt_provider_enable(p))
  testthat::expect_silent(usdt_provider_disable(p))
})

testthat::test_that("Probes behalve as expected", {
  p <- usdt_provider("testthat")
  probe <- usdt_add_probe(p, "p1", 1)

  testthat::expect_false(usdt_fire_probe(probe, 2L))

  testthat::expect_error(
    usdt_add_probe(p, "p2", 8),
    regexp = "Probes cannot accept more than 6 arguments at present"
  )

  usdt_provider_enable(p)
  testthat::expect_error(
    usdt_add_probe(p, "p2", 1),
    regexp = "Probes cannot be added while the provider is loaded"
  )

  testthat::expect_false(usdt_fire_probe(probe, 2L))
})
