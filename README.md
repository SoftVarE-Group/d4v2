# d4

## Installation

### Pre-built

There are prebuilt binaries available for each release.

### Nix

This project can be used and developed via a [Nix][nix] [flake][flake].
There are two package outputs: One for a dynamically linked executable (`d4`) and one for a static build (`d4-static`)
which is used for the prebuilt binaries.

With Nix installed simply run the following to build a dynamically linked executable:

```
nix build
```

This will build the default output (`.#d4`).
For a static build use the following instead:

```
nix build .#d4-static
```

You might want to add `-L` for build logs.
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

## Development

For a development environment with the dependencies set up, run:

```
nix develop
```

### Formatting

We enforce formatting for C++ and Nix code.
Use the following to format all code:

```
nix fmt
```

A flake check will output any formatting issues:

```
nix flake check -L
```

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
[docker]: https://docker.com
[podman]: https://podman.io
[container]: https://github.com/SoftVarE-Group/d4v2/pkgs/container/d4v2
