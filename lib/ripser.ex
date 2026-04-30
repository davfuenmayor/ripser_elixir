defmodule Ripser do
  @moduledoc """
  Elixir NIF bindings for Ripser.

  `compute/2` accepts input in the same runtime formats as the Ripser CLI and
  returns parsed persistence intervals together with the raw Ripser output.

  ## Examples

      iex> {:ok, result} = Ripser.compute("1 1 1", format: :lower_distance, dim: 1)
      iex> result.intervals[0]
      [{0.0, 1.0}, {0.0, 1.0}, {0.0, :infinity}]

      iex> {:ok, result} = Ripser.compute("0 0\\n1 0\\n0 1\\n", format: :point_cloud, dim: 1)
      iex> Map.has_key?(result.intervals, 0)
      true

  Supported formats are `:distance`, `:lower_distance`, `:upper_distance`,
  `:point_cloud`, `:sparse`, `:dipha`, and `:binary`.
  """

  @on_load :load_nif

  @type interval :: {float(), float() | :infinity}
  @type result :: %{intervals: %{integer() => [interval()]}, output: String.t()}

  @doc false
  def load_nif do
    path = :filename.join(:code.priv_dir(:ripser), ~c"ripser_nif")
    :erlang.load_nif(path, 0)
  end

  @doc """
  Computes Vietoris-Rips persistence barcodes.

  Options:

    * `:format` - input format, defaults to `:distance`
    * `:dim` - maximum homology dimension, defaults to `1`
    * `:threshold` - finite threshold, defaults to Ripser's enclosing radius behavior
    * `:ratio` - persistence ratio filter, defaults to `1.0`
    * `:modulus` - prime field coefficient modulus, defaults to `2`
  """
  @spec compute(binary(), keyword()) :: {:ok, result()} | {:error, String.t()}
  def compute(input, opts \\ []) when is_binary(input) and is_list(opts) do
    compute_nif(input, opts)
  end

  defp compute_nif(_input, _opts), do: :erlang.nif_error(:nif_not_loaded)
end
