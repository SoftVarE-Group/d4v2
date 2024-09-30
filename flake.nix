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

          cadical = pkgs.pkgsStatic.callPackage ./nix/cadical.nix { };
          cadiback = pkgs.pkgsStatic.callPackage ./nix/cadiback.nix {
            cadical = self.packages.${system}.cadical;
          };

          cryptominisat = pkgs.pkgsStatic.callPackage ./nix/cryptominisat.nix {
            cadiback = self.packages.${system}.cadiback;
          };

          sbva = pkgs.pkgsStatic.callPackage ./nix/sbva.nix { };

          arjun = pkgs.pkgsStatic.callPackage ./nix/arjun.nix {
            cryptominisat = self.packages.${system}.cryptominisat;
            sbva = self.packages.${system}.sbva;
          };

          tbb = tbb pkgs;
          tbb-windows = tbb pkgs-windows;

          mt-kahypar = pkgs.callPackage ./nix/mt-kahypar.nix { tbb = self.packages.${system}.tbb; };
          mt-kahypar-windows = pkgs-windows.callPackage ./nix/mt-kahypar.nix {
            tbb = self.packages.${system}.tbb-windows;
          };

          d4 = pkgs.callPackage ./nix/d4.nix {
            mt-kahypar = self.packages.${system}.mt-kahypar;
            arjun = self.packages.${system}.arjun;
          };

          # TODO: arjun on windows
          #d4-windows = pkgs-windows.callPackage ./nix/d4.nix {
          #  mt-kahypar = self.packages.${system}.mt-kahypar-windows;
          #  arjun = self.packages.${system}.arjun-windows;
          #};

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
