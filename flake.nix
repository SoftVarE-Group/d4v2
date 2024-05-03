{
  description = "Packages and development environments for d4";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-23.11";
    flake-utils.url = "github:numtide/flake-utils/v1.0.0";
  };

  outputs = { self, nixpkgs, flake-utils, ... }:
    let
      lib = nixpkgs.lib;
      systems = lib.systems.doubles.unix;
    in flake-utils.lib.eachSystem systems (system:
      let
        pkgs = import nixpkgs { inherit system; };

        tbb = if pkgs.stdenv.isDarwin
        && lib.versionOlder pkgs.stdenv.hostPlatform.darwinMinVersion
        "10.13" then
          pkgs.tbb
        else
          pkgs.tbb_2021_8;
      in {
        formatter = pkgs.nixfmt;

        packages = {
          default = self.packages.${system}.d4;

          d4 = pkgs.stdenv.mkDerivation {
            pname = "d4";
            version = "2.0";

            src = ./.;

            buildInputs = [
              self.packages.${system}.mt-kahypar
              pkgs.boost.dev
              pkgs.gmp.dev
            ];

            nativeBuildInputs = [ pkgs.cmake pkgs.ninja ];

            meta = with lib; {
              description = "A CNF to d-DNNF compiler";
              homepage = "https://github.com/SoftVarE-Group/d4v2";
              license = licenses.lgpl21Plus;
              platforms = systems;
            };
          };

          container = pkgs.dockerTools.buildLayeredImage {
            name = "d4v2";
            contents = [ self.packages.${system}.d4 ];
            config.Entrypoint = [ "/bin/d4" ];
          };

          mt-kahypar = pkgs.stdenv.mkDerivation {
            pname = "mt-kahypar";
            version = "1.4";

            src = pkgs.fetchgit {
              url = "https://github.com/kahypar/mt-kahypar.git";
              rev = "c51ffeaa3b1040530bf821b7f323e3790b147b33";
              hash = "sha256-MlF6ZGsqtGQxzDJHbvo5uFj+6w8ehr9V4Ul5oBIGzws=";
              fetchSubmodules = true;
            };

            outputs = [ "out" "dev" ];

            buildInputs = [ pkgs.boost.dev pkgs.git pkgs.hwloc.dev tbb.dev ];

            nativeBuildInputs = [ pkgs.cmake pkgs.ninja ];

            patches = [ ./mt-kahypar-pc.patch ];

            cmakeFlags = [
              "-D KAHYPAR_PYTHON=false"
              "-D MT_KAHYPAR_DISABLE_BOOST=true"
              "-D KAHYPAR_ENFORCE_MINIMUM_TBB_VERSION=false"
              "-G Ninja"
            ];

            buildPhase = "cmake --build . --target mtkahypar";
            installPhase = "cmake --install .";

            meta = with lib; {
              description =
                "A shared-memory multilevel graph and hypergraph partitioner";
              longDescription = ''
                Mt-KaHyPar (Multi-Threaded Karlsruhe Hypergraph Partitioner) is a shared-memory multilevel graph and hypergraph partitioner equipped with parallel implementations of techniques used in the best sequential partitioning algorithms.
                Mt-KaHyPar can partition extremely large hypergraphs very fast and with high quality.
              '';
              homepage = "https://github.com/kahypar/mt-kahypar";
              license = licenses.mit;
              platforms = systems;
            };
          };

          bundled-deps = pkgs.buildEnv {
            name = "bundled-deps";
            paths = [
              self.packages.${system}.mt-kahypar
              pkgs.hwloc.lib
              pkgs.boost
              pkgs.gmp
              tbb
            ] ++ lib.optionals pkgs.stdenv.cc.isClang [
              pkgs.libcxx
              pkgs.libcxxabi
            ];
          };

          bundled-doc = pkgs.stdenv.mkDerivation {
            name = "bundled-doc";
            src = ./doc;
            installPhase = ''
              mkdir $out
              cp ${system}.md $out/README.md
            '';
          };

          bundled = pkgs.buildEnv {
            name = "bundled";
            paths = [
              self.packages.${system}.d4
              self.packages.${system}.bundled-deps
              self.packages.${system}.bundled-doc
            ];
          };
        };
      });
}
