{
  description = "Packages and development environments for d4";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05";

  outputs =
    { self, nixpkgs, ... }:
    let
      lib = nixpkgs.lib;

      # All supported build systems, the Windows build is cross-compiled.
      systems = [
        "aarch64-darwin"
        "aarch64-linux"
        "x86_64-darwin"
        "x86_64-linux"
      ];

      # Different platforms require different TBB versions and the Windows one is not in upstream Nixpkgs yet.
      tbb =
        pkgs:
        if pkgs.stdenv.isDarwin && lib.versionOlder pkgs.stdenv.hostPlatform.darwinMinVersion "10.13" then
          pkgs.tbb
        else if pkgs.stdenv.hostPlatform.isWindows then
          pkgs.callPackage ./nix/tbb-windows.nix { }
        else
          pkgs.tbb_2021_11;

      mt-kahypar = pkgs: pkgs.callPackage ./nix/mt-kahypar.nix { tbb = tbb pkgs; };
      d4 = pkgs: pkgs.callPackage ./nix/d4.nix { mt-kahypar = mt-kahypar pkgs; };

      # All required runtime dependencies.
      dependencies =
        pkgs:
        pkgs.buildEnv {
          name = "d4-dependencies";
          paths = [
            (mt-kahypar pkgs)
            (tbb pkgs)
            pkgs.hwloc.lib
            pkgs.gmp
          ] ++ lib.optionals pkgs.stdenv.cc.isClang [ pkgs.libcxx ];
        };

      # A simple README explaining how to setup the built directories to run the binary.
      documentation =
        pkgs:
        pkgs.stdenv.mkDerivation {
          name = "d4-documentation";
          src = ./doc;
          installPhase = ''
            mkdir $out
            cp ${pkgs.stdenv.system}.md $out/README.md
          '';
        };

      # The binary with all dependencies.
      bundled =
        pkgs:
        pkgs.buildEnv {
          name = "d4";
          paths = [
            (d4 pkgs)
            (dependencies pkgs)
            (documentation pkgs)
          ];
        };
    in
    {
      formatter = lib.genAttrs systems (system: nixpkgs.legacyPackages.${system}.nixfmt-rfc-style);
      packages = lib.genAttrs systems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          pkgs-windows = pkgs.pkgsCross.mingwW64;
        in
        {
          default = self.packages.${system}.d4;

          mt-kahypar = mt-kahypar pkgs;
          mt-kahypar-windows = mt-kahypar pkgs-windows;

          d4 = d4 pkgs;
          d4-windows = d4 pkgs-windows;

          container = pkgs.dockerTools.buildLayeredImage {
            name = "d4";
            contents = [ self.packages.${system}.d4 ];
            config = {
              Entrypoint = [ "/bin/d4" ];
              Labels = {
                "org.opencontainers.image.source" = "https://github.com/SoftVarE-Group/d4v2";
                "org.opencontainers.image.description" = "A CNF to d-DNNF compiler";
                "org.opencontainers.image.licenses" = "LGPL-2.1-or-later";
              };
            };
          };

          dependencies = dependencies pkgs;
          dependencies-windows = dependencies pkgs-windows;

          bundled = bundled pkgs;
          bundled-windows = bundled pkgs-windows;
        }
      );
    };
}
