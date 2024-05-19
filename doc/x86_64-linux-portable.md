# d4

This is a portable version of d4.
The project is licensed under the LGPL-2.1 and the source can be found at https://github.com/SoftVarE-Group/d4v2

## Dependencies

In this version, all dependencies and the Nix package manager are bundled with d4 in a self-extracting archive.
Other than Git, there are no dependencies.
When Git is not found, the bundled Nix will fetch it automatically.

The self-extracting archive is generated using https://github.com/DavHau/nix-portable and will create the directory `$HOME/.nix-portable`.
This can be overwritten by the `NP_LOCATION` environment variable which defaults to `$HOME`.

## Usage

The binary `d4` is inside `bin`.
To show the help message, use:

```
d4 --help
```

### d-DNNF compilation

```
d4 --input /path/to/input.cnf --method ddnnf-compiler --dump-ddnnf /path/to/output.ddnnf
```

... will take a CNF from `/path/to/input.cnf`, compile it into a d-DNNF and write it to `/path/to/output.ddnnf`.
