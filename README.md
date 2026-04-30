# Ripser Elixir NIF

Standalone Elixir NIF wrapper for the C++ [Ripser](https://github.com/Ripser/ripser) implementation.

Ripser is a high-performance C++ library for computing persistent homology via Vietoris–Rips persistence barcodes, commonly used in topological data analysis (TDA) to extract multiscale topological features from point-cloud or distance-matrix data.

The upstream `ripser.cpp` source is vendored under `c_src/vendor/` so the
project can compile without modifying or depending on the original Ripser
checkout.

## Usage

```sh
mix test
mix run -e 'IO.inspect(Ripser.compute("1 1 1", format: :lower_distance, dim: 1), limit: :infinity)'
```

Open `livebook-examples/ripser_showcase.livemd` in Livebook for an interactive
walkthrough with tables and barcode charts.

Supported input formats are `:distance`, `:lower_distance`, `:upper_distance`,
`:point_cloud`, `:sparse`, `:dipha`, and `:binary`.

Supported runtime options are `:format`, `:dim`, `:threshold`, `:ratio`, and
`:modulus`. The NIF is built with Ripser's coefficient support enabled, so
`:modulus` accepts prime integer moduli.

`Ripser.compute/2` returns `{:ok, %{intervals: intervals, output: output}}`,
where `intervals` is a map from homology dimension to parsed barcode intervals
and `output` is the raw Ripser output.

To build against a different `ripser.cpp`, override the source path:

```sh
RIPSER_CPP=/path/to/ripser.cpp mix test
```

## License

This project is licensed under the MIT License. See `LICENSE`.

This project includes or wraps code from
[Ripser](https://github.com/Ripser/ripser), which is also licensed under the
MIT License and is copyright (c) 2015–2021 Ulrich Bauer.
