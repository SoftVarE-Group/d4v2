# d4

This is a built version of d4.
The project is licensed under the LGPL-2.1 and the source can be found at https://github.com/SoftVarE-Group/d4v2

## Dependencies

All dependencies are bundled with this build inside the `lib` directory.
To use the binary, they have to be visible to the linker.
This can either be accomplished by moving the `lib` directories contents to the global library path (such as `/usr/lib`)
or by setting the `LD_LIBRARY_PATH` environment variable to include the `lib` directory.

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
