defmodule RipserTest do
  use ExUnit.Case, async: true

  test "computes intervals from a lower distance matrix" do
    assert {:ok, %{intervals: intervals, output: output}} =
             Ripser.compute("1 1 1", format: :lower_distance, dim: 1)

    assert is_binary(output)
    assert intervals[0] == [{0.0, 1.0}, {0.0, 1.0}, {0.0, :infinity}]
  end

  test "computes intervals from a full distance matrix" do
    input = """
    0 1 1
    1 0 1
    1 1 0
    """

    assert {:ok, %{intervals: intervals}} = Ripser.compute(input, format: :distance, dim: 1)
    assert intervals[0] == [{0.0, 1.0}, {0.0, 1.0}, {0.0, :infinity}]
  end

  test "computes intervals from an upper distance matrix" do
    assert {:ok, %{intervals: intervals}} =
             Ripser.compute("1 1 1", format: :upper_distance, dim: 1)

    assert intervals[0] == [{0.0, 1.0}, {0.0, 1.0}, {0.0, :infinity}]
  end

  test "computes intervals from a point cloud" do
    input = """
    0 0
    1 0
    0 1
    """

    assert {:ok, %{intervals: intervals}} = Ripser.compute(input, format: :point_cloud, dim: 1)
    assert [{0.0, 1.0}, {0.0, 1.0}, {0.0, :infinity}] = intervals[0]
  end

  test "computes intervals from sparse triplets" do
    input = """
    0 1 1
    0 2 1
    1 2 1
    """

    assert {:ok, %{intervals: intervals}} = Ripser.compute(input, format: :sparse, dim: 1)
    assert intervals[0] == [{0.0, 1.0}, {0.0, 1.0}, {0.0, :infinity}]
  end

  test "computes intervals from binary lower distance matrices" do
    input = <<1.0::float-32-little, 1.0::float-32-little, 1.0::float-32-little>>

    assert {:ok, %{intervals: intervals}} = Ripser.compute(input, format: :binary, dim: 1)
    assert intervals[0] == [{0.0, 1.0}, {0.0, 1.0}, {0.0, :infinity}]
  end

  test "computes intervals from DIPHA distance matrices" do
    input =
      [
        <<8_067_171_840::signed-64-little>>,
        <<7::signed-64-little>>,
        <<3::signed-64-little>>,
        for(
          value <- [0.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 0.0],
          do: <<value::float-64-little>>
        )
      ]
      |> IO.iodata_to_binary()

    assert {:ok, %{intervals: intervals}} = Ripser.compute(input, format: :dipha, dim: 1)
    assert intervals[0] == [{0.0, 1.0}, {0.0, 1.0}, {0.0, :infinity}]
  end

  test "accepts prime coefficient modulus" do
    assert {:ok, %{intervals: intervals}} =
             Ripser.compute("1 1 1", format: :lower_distance, dim: 1, modulus: 3)

    assert intervals[0] == [{0.0, 1.0}, {0.0, 1.0}, {0.0, :infinity}]
  end

  test "rejects non-prime coefficient modulus" do
    assert {:error, "modulus must be a prime integer"} =
             Ripser.compute("1 1 1", format: :lower_distance, modulus: 4)
  end

  test "rejects unknown formats" do
    assert {:error, "unknown format"} = Ripser.compute("1 1 1", format: :bad_format)
  end
end
