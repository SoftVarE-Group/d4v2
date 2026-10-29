{
  description = "Packages and development environments for d4";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    treefmt-nix = {
      url = "github:numtide/treefmt-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      treefmt-nix,
      ...
    }:
    let
      lib = nixpkgs.lib;

      # All supported build systems, the Windows build is cross-compiled.
      systems = [
        "aarch64-darwin"
        "aarch64-linux"
        "x86_64-darwin"
        "x86_64-linux"
      ];

      treefmt =
        system:
        (treefmt-nix.lib.evalModule nixpkgs.legacyPackages.${system} ./nix/treefmt.nix).config.build;
    in
    {
      formatter = lib.genAttrs systems (system: (treefmt system).wrapper);

      overlays.default = final: prev: {
        # Enable TBB to be built statically.
        tbb_2022 = prev.tbb_2022.overrideAttrs (
          finalAttrs: previousAttrs: {
            meta.badPlatforms = [ ];
            postPatch = lib.optionalString prev.stdenv.hostPlatform.isMusl ''
              substituteInPlace test/CMakeLists.txt \
                --replace-fail 'tbb_add_test(SUBDIR conformance NAME conformance_resumable_tasks DEPENDENCIES TBB::tbb)' "" \
                --replace-fail 'tbb_add_test(SUBDIR tbb NAME test_eh_algorithms DEPENDENCIES TBB::tbb)' ""
            '';
          }
        );

        glucose = final.callPackage ./nix/glucose.nix { };
        cadical = final.callPackage ./nix/cadical.nix { };
        cadiback = final.callPackage ./nix/cadiback.nix { };
        cryptominisat = final.callPackage ./nix/cryptominisat.nix { };
        sbva = final.callPackage ./nix/sbva.nix { };
        arjun = final.callPackage ./nix/arjun.nix { };
        gpmc = final.callPackage ./nix/gpmc.nix { };
        mt-kahypar = final.callPackage ./nix/mt-kahypar.nix { };
        d4 = final.callPackage ./nix/d4.nix { };
      };

      packages = lib.genAttrs systems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system}.extend self.overlays.default;
        in
        {
          default = self.packages.${system}.d4;

          d4 = pkgs.d4;
          d4-static = pkgs.pkgsStatic.d4;
          # TODO: Windows build is currently broken
          # d4-windows = pkgs.pkgsCross.mingwW64.d4;

          container = pkgs.dockerTools.buildImage {
            name = "d4";
            copyToRoot = pkgs.buildEnv {
              name = "d4";
              paths = [
                pkgs.pkgsStatic.d4
                pkgs.time
              ];
              pathsToLink = [ "/bin" ];
            };
            config = {
              Entrypoint = [ "d4" ];
              Labels = {
                "org.opencontainers.image.source" = "https://github.com/SoftVarE-Group/d4v2";
                "org.opencontainers.image.description" = "A CNF to d-DNNF compiler";
                "org.opencontainers.image.licenses" = "LGPL-2.1-or-later";
              };
            };
          };
        }
      );

      checks = lib.genAttrs systems (system: {
        formatting = (treefmt system).check self;
      });

      devShells = lib.genAttrs systems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system}.extend self.overlays.default;
        in
        {
          default = pkgs.mkShell {
            nativeBuildInputs = [
              pkgs.cmake
            ];

            buildInputs = [
              pkgs.boost.dev
              pkgs.glucose.dev
              pkgs.gmp.dev
              pkgs.gpmc.dev
              pkgs.mt-kahypar
            ];
          };
        }
      );
    };
}
