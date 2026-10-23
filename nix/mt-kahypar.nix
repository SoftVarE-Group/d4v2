{
  lib,
  stdenv,
  fetchFromGitHub,
  cmake,
  git,
  boost,
  tbb_2022,
  windows,
}:
let
  cli11 = fetchFromGitHub {
    owner = "CLIUtils";
    repo = "CLI11";
    rev = "v2.7.2";
    hash = "sha256-lFMv7WH/Gv+lRjs9tH6QYk+E/QC39ndzvtYKwpGNkmg=";
  };

  shared-resources = fetchFromGitHub {
    owner = "kahypar";
    repo = "kahypar-shared-resources";
    rev = "6d5c8e2444e4310667ec1925e995f26179d7ee88";
    hash = "sha256-K3tQ9nSJrANdJPf7v/ko2etQLDq2f7Z0V/kvDuWKExM=";
  };

  WHFC = fetchFromGitHub {
    owner = "larsgottesbueren";
    repo = "WHFC";
    rev = "dd3b7901f73ba9a1de44788172ded5ce33b83338";
    hash = "sha256-mGCjTp6QTyWFO16RfXYHSIQbVWgOAa37NRCKOHArGH4=";
  };

  parlay = fetchFromGitHub {
    owner = "cmuparlay";
    repo = "parlaylib";
    rev = "e1f1dc0ccf930492a2723f7fbef8510d35bf57f5";
    hash = "sha256-Hg4OVHC1LDvBf1tFUMW7YAugnFk0ju1VzgEzT2OoT8Y=";
  };

  enableParlay = !stdenv.hostPlatform.isWindows;

  tbb = tbb_2022.overrideAttrs (
    finalAttrs: previousAttrs: {
      meta.badPlatforms = [ ];
      postPatch = lib.optionalString stdenv.hostPlatform.isMusl ''
        substituteInPlace test/CMakeLists.txt \
          --replace-fail 'tbb_add_test(SUBDIR conformance NAME conformance_resumable_tasks DEPENDENCIES TBB::tbb)' "" \
          --replace-fail 'tbb_add_test(SUBDIR tbb NAME test_eh_algorithms DEPENDENCIES TBB::tbb)' ""
      '';
    }
  );
in
stdenv.mkDerivation rec {
  pname = "mt-kahypar";
  version = "1.7";

  src = fetchFromGitHub {
    owner = "kahypar";
    repo = "mt-kahypar";
    rev = "v${version}";
    hash = "sha256-sFx2FBlq9JvCYjZqpNo2Q6rffUt8PNY+L08l/dutvPs=";
  };

  nativeBuildInputs = [
    cmake
    git
  ];

  buildInputs = [
    boost.dev
    tbb.dev
  ]
  ++ lib.optionals stdenv.hostPlatform.isWindows [ windows.pthreads ];

  patches = [
    ./mt-kahypar-cmake.patch
    ./mt-kahypar-install.patch
    ./mt-kahypar-whfc.patch
  ];

  cmakeFlags = [
    (lib.cmakeFeature "FETCHCONTENT_SOURCE_DIR_CLI11" "${cli11}")
    (lib.cmakeFeature "FETCHCONTENT_SOURCE_DIR_KAHYPAR-SHARED-RESOURCES" "${shared-resources}")
    (lib.cmakeFeature "FETCHCONTENT_SOURCE_DIR_WHFC" "${WHFC}")
    (lib.cmakeFeature "FETCHCONTENT_SOURCE_DIR_PARLAY" "${parlay}")
    (lib.cmakeBool "KAHYPAR_DISABLE_HWLOC" true)
    (lib.cmakeBool "KAHYPAR_DISABLE_PARLAY" (!enableParlay))
  ];

  buildPhase = "cmake --build . --target mtkahypar --parallel $NIX_BUILD_CORES";

  installPhase =
    if enableParlay then
      ''
        cmake --install .
        rm -r $out/include/parlay
        rm -r $out/share
      ''
    else
      "cmake --install .";

  meta = {
    description = "A shared-memory multilevel graph and hypergraph partitioner";
    longDescription = ''
      Mt-KaHyPar (Multi-Threaded Karlsruhe Hypergraph Partitioner) is a shared-memory multilevel graph and hypergraph partitioner equipped with parallel implementations of techniques used in the best sequential partitioning algorithms.
      Mt-KaHyPar can partition extremely large hypergraphs very fast and with high quality.
    '';
    homepage = "https://github.com/kahypar/mt-kahypar";
    license = lib.licenses.mit;
    platforms = lib.platforms.unix ++ lib.platforms.windows;
  };
}
