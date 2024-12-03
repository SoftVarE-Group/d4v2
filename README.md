# d4

## Installation

### Pre-built

There are prebuilt binaries available for each commit and release.
These binaries are bundled with their dependencies.
For details, see the README inside the release folder.

On (Linux) systems with an older glibc, the normal variant might not work.
In this case, there is also the portable variant which is a self-extracting archive with everything bundled in one binary.

### Nix

This project can be used and developed via a [Nix][nix] [flake][flake].

With Nix installed simply run the following for a build:

```
nix build
```

The result will be at `result`.

To build without the need to clone the repository, use:

```
nix build github:SoftVarE-Group/d4v2
```

### Container

There is also a container image for usage with [Docker][docker], [Podman][podman] or any other container tool.

For an overview, see [here][container].

There is a tag for each branch and for each tagged release.
To pull the container, use:

```
docker pull ghcr.io/softvare-group/d4v2
```

Then, you can use it like the standalone binary.
For d4 to be able to access files, you need to create a volume.
The following mounts `<local/directory>` on `/work` inside the container:

```
docker run -v <local/directory>:/work d4v2 --input /work/input.cnf --method ddnnf-compiler --dump-ddnnf /work/output.ddnnf
```

### Manual

#### Prerequisites

 - [CMake][cmake]
 - [GMP][gmp] (with C++ bindings)
 - [Boost][boost] (headers and `program_options`)
 - [zlib][zlib] (optional, for glucose)
 - [Mt-KaHyPar][mtkahypar]

#### Build

This is a CMake project.
To configure a debug build in the `build` directory (will be created), run:

```
cmake -B build
```

A generator can be specified, for example [Ninja][ninja]:

```
cmake -B build -G Ninja
```

Optionally, CMake variables can be set to alter the build:

```
cmake -D <variable>=<value> -D <variable>=<value> -B build
```

Of interest for this project are:

| Variable               | Value                                                | Description                                                       |
|------------------------|------------------------------------------------------|-------------------------------------------------------------------|
| `CMAKE_BUILD_TYPE`     | `Debug`, `Release`, `RelWithDebInfo` or `MinSizeRel` | Whether to create a debug or release build.                       |
| `D4_SOLVER`            | `minisat` or `glucose`                               | Which SAT solver to use. Defaults to `minisat`.                   |
| `D4_PREPROC_SOLVER`    | `minisat` or `glucose`                               | Which SAT solver to use for preprocessing. Defaults to `minisat`. |
| `CMAKE_INSTALL_PREFIX` | Path                                                 | Where to install built files to using `cmake --install`.          |
| `MtKaHyPar_ROOT`       | Path                                                 | Alternative root directory so search for Mt-KaHyPar.              |
| `glucose_ROOT`         | Path                                                 | Alternative root directory so search for glucose.                 |

After configuring, build the project with:

```
cmake --build build
```

The resulting executable will be built at `build/d4`.

#### Install

To install the built files, use:

```
cmake --install build
```

The installation prefix can be changed as described in the build section.

## Usage

See the help message:

```
d4 --help
```

To compile a CNF into a d-DNNF, use:

```
d4 --input /path/to/input.cnf --method ddnnf-compiler --dump-ddnnf /path/to/output.ddnnf
```

[nix]: https://nixos.org
[flake]: https://nixos.wiki/wiki/Flakes
[cmake]: https://cmake.org
[gmp]: https://gmplib.org
[boost]: https://boost.org
[zlib]: https://zlib.net
[ninja]: https://github.com/ninja-build/ninja
[mtkahypar]: https://github.com/kahypar/mt-kahypar
[msys2]: https://msys2.org
[docker]: https://docker.com
[podman]: https://podman.io
[container]: https://github.com/SoftVarE-Group/d4v2/pkgs/container/d4v2
