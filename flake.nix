{
  description = "Packages and development environments for d4";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";

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

      # If building for Windows, appends -windows, otherwise nothing.
      windowsSuffix = pkgs: name: if pkgs.stdenv.hostPlatform.isWindows then "${name}-windows" else name;

      # Different platforms require different TBB versions and the Windows one is not in upstream Nixpkgs yet.
      tbb =
        pkgs:
        if pkgs.stdenv.isDarwin && lib.versionOlder pkgs.stdenv.hostPlatform.darwinMinVersion "10.13" then
          pkgs.tbb
        else if pkgs.stdenv.hostPlatform.isWindows then
          pkgs.callPackage ./nix/tbb-windows.nix { }
        else
          pkgs.tbb_2021_11;

      # All required runtime dependencies.
      dependencies =
        pkgs:
        let
          system = pkgs.buildPlatform.system;
          windowsSuffix' = windowsSuffix pkgs;
        in
        pkgs.buildEnv {
          name = "d4-dependencies";
          paths = [
            self.packages.${system}.${windowsSuffix' "mt-kahypar"}
            self.packages.${system}.${windowsSuffix' "tbb"}
            pkgs.hwloc.lib
          ] ++ lib.optionals pkgs.stdenv.cc.isClang [ pkgs.libcxx ];
        };

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
        let
          system = pkgs.buildPlatform.system;
          windowsSuffix' = windowsSuffix pkgs;
        in
        pkgs.buildEnv {
          name = "d4";
          paths = [
            self.packages.${system}.${windowsSuffix' "d4"}
            self.packages.${system}.${windowsSuffix' "dependencies"}
            self.packages.${system}.${windowsSuffix' "documentation"}
          ];
        };
    in
    {
      formatter = lib.genAttrs systems (system: nixpkgs.legacyPackages.${system}.nixfmt-rfc-style);
      packages = lib.genAttrs systems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          pkgs-static = if pkgs.stdenv.isDarwin then pkgs else pkgs.pkgsStatic;
          pkgs-windows = pkgs.pkgsCross.mingwW64;
        in
        {
          default = self.packages.${system}.d4;

          glucose = pkgs-static.callPackage ./nix/glucose.nix { };
          glucose-windows = pkgs-windows.callPackage ./nix/glucose.nix { };

          cadical = pkgs-static.callPackage ./nix/cadical.nix { };
          cadical-windows = pkgs-windows.callPackage ./nix/cadical.nix { };

          cadiback = pkgs-static.callPackage ./nix/cadiback.nix {
            cadical = self.packages.${system}.cadical;
          };

          cadiback-windows = pkgs-windows.callPackage ./nix/cadiback.nix {
            cadical = self.packages.${system}.cadical-windows;
          };

          cryptominisat = pkgs-static.callPackage ./nix/cryptominisat.nix {
            cadical = self.packages.${system}.cadical;
            cadiback = self.packages.${system}.cadiback;
          };

          cryptominisat-windows = pkgs-windows.callPackage ./nix/cryptominisat.nix {
            cadical = self.packages.${system}.cadical-windows;
            cadiback = self.packages.${system}.cadiback-windows;
          };

          sbva = pkgs-static.callPackage ./nix/sbva.nix { };
          sbva-windows = pkgs-windows.callPackage ./nix/sbva.nix { };

          arjun = pkgs-static.callPackage ./nix/arjun.nix {
            cryptominisat = self.packages.${system}.cryptominisat;
            sbva = self.packages.${system}.sbva;
          };

          arjun-windows = pkgs-windows.callPackage ./nix/arjun.nix {
            cryptominisat = self.packages.${system}.cryptominisat-windows;
            sbva = self.packages.${system}.sbva-windows;
          };

          gpmc = pkgs-static.callPackage ./nix/gpmc.nix {
            arjun = self.packages.${system}.arjun;
            cryptominisat = self.packages.${system}.cryptominisat;
          };

          gpmc-windows = pkgs-windows.callPackage ./nix/gpmc.nix {
            arjun = self.packages.${system}.arjun-windows;
            cryptominisat = self.packages.${system}.cryptominisat-windows;
          };

          tbb = tbb pkgs;
          tbb-windows = tbb pkgs-windows;

          mt-kahypar = pkgs.callPackage ./nix/mt-kahypar.nix { tbb = self.packages.${system}.tbb; };
          mt-kahypar-windows = pkgs-windows.callPackage ./nix/mt-kahypar.nix {
            tbb = self.packages.${system}.tbb-windows;
          };

          d4 = pkgs.callPackage ./nix/d4.nix {
            mt-kahypar = self.packages.${system}.mt-kahypar;
            cryptominisat = self.packages.${system}.cryptominisat;
            sbva = self.packages.${system}.sbva;
            arjun = self.packages.${system}.arjun;
            gpmc = self.packages.${system}.gpmc;
            glucose = self.packages.${system}.glucose;
            cadical = self.packages.${system}.cadical;
            cadiback = self.packages.${system}.cadiback;
          };

          d4-windows = pkgs-windows.callPackage ./nix/d4.nix {
            mt-kahypar = self.packages.${system}.mt-kahypar-windows;
            cryptominisat = self.packages.${system}.cryptominisat-windows;
            sbva = self.packages.${system}.sbva-windows;
            arjun = self.packages.${system}.arjun-windows;
            gpmc = self.packages.${system}.gpmc-windows;
            glucose = self.packages.${system}.glucose-windows;
            cadical = self.packages.${system}.cadical-windows;
            cadiback = self.packages.${system}.cadiback-windows;
          };

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

          documentation = documentation pkgs;
          documentation-windows = documentation pkgs-windows;

          bundled = bundled pkgs;
          bundled-windows = bundled pkgs-windows;
        }
      );
      devShells = lib.genAttrs systems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          selfPkgs = self.packages.${system};
        in
        {
          default = pkgs.mkShell {
            MTKAHYPAR_LIB_DIR = "${selfPkgs.mt-kahypar}/lib";
            ARJUN_LIB_DIR = "${selfPkgs.arjun.lib}/lib";
            CADIBACK_LIB_DIR = "${selfPkgs.cadiback}/lib";
            CADICAL_LIB_DIR = "${selfPkgs.cadical.lib}/lib";
            CRYPTOMINISAT5_LIB_DIR = "${selfPkgs.cryptominisat.lib}/lib";
            GLUCOSE_LIB_DIR = "${selfPkgs.glucose.lib}/lib";
            GMP_LIB_DIR = "${pkgs.gmp}/lib";
            GPMC_LIB_DIR = "${selfPkgs.gpmc}/lib";
            SBVA_LIB_DIR = "${selfPkgs.sbva.lib}/lib";
            buildInputs = [
              pkgs.boost.dev
              pkgs.gmp.dev
              pkgs.mpfr.dev
              pkgs.zlib.dev
              selfPkgs.mt-kahypar.dev
              selfPkgs.arjun.dev
              selfPkgs.cadiback.dev
              selfPkgs.cadical.dev
              selfPkgs.cryptominisat.dev
              selfPkgs.glucose.dev
              selfPkgs.gpmc.dev
              selfPkgs.sbva.dev
            ];
          };
        }
      );
    };
}
