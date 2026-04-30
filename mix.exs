defmodule Ripser.MixProject do
  use Mix.Project

  def project do
    [
      app: :ripser,
      version: "0.1.0",
      elixir: "~> 1.14",
      compilers: [:ripser_nif] ++ Mix.compilers(),
      description: description(),
      package: package(),
      start_permanent: Mix.env() == :prod,
      deps: []
    ]
  end

  def application do
    [
      extra_applications: [:logger]
    ]
  end

  defp description do
    "Elixir NIF bindings for Ripser, a C++ implementation for Vietoris-Rips persistence barcodes."
  end

  defp package do
    [
      licenses: ["MIT"],
      files: [
        "c_src/Makefile",
        "c_src/ripser_nif.cpp",
        "c_src/vendor/ripser.cpp",
        "lib",
        "livebook-examples",
        "test",
        ".formatter.exs",
        "LICENSE",
        "mix.exs",
        "README.md"
      ],
      links: %{
        "Ripser" => "https://github.com/Ripser/ripser"
      }
    ]
  end
end

defmodule Mix.Tasks.Compile.RipserNif do
  use Mix.Task.Compiler

  @recursive true
  @manifest "compile.ripser_nif"

  @impl true
  def run(_args) do
    File.mkdir_p!("priv")

    {_output, exit_code} =
      System.cmd("make", ["-C", "c_src"], into: IO.stream(:stdio, :line), stderr_to_stdout: true)

    if exit_code == 0 do
      source_nif = Path.expand("c_src/build/ripser_nif.so")
      source_priv = Path.expand("priv")
      app_priv = Path.join(Mix.Project.app_path(), "priv")

      File.mkdir_p!(source_priv)
      File.mkdir_p!(app_priv)
      File.cp!(source_nif, Path.join(source_priv, "ripser_nif.so"))
      File.cp!(source_nif, Path.join(app_priv, "ripser_nif.so"))
      File.mkdir_p!(Mix.Project.manifest_path())
      File.write!(Path.join(Mix.Project.manifest_path(), @manifest), "ok")
      {:ok, []}
    else
      {:error, []}
    end
  end

  @impl true
  def manifests, do: [Path.join(Mix.Project.manifest_path(), @manifest)]

  @impl true
  def clean do
    System.cmd("make", ["-C", "c_src", "clean"], stderr_to_stdout: true)
    File.rm(Path.join(Mix.Project.manifest_path(), @manifest))
    :ok
  end
end
