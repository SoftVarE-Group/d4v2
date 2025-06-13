{
  lib,
  stdenv,
  fetchFromGitHub,
  cmake,
  git,
  boost,
  hwloc,
  tbb_2022_0,
  windows,
}:
let
  shared-resources = fetchFromGitHub {
    owner = "kahypar";
    repo = "kahypar-shared-resources";
    rev = "6d5c8e2444e4310667ec1925e995f26179d7ee88";
    hash = "sha256-K3tQ9nSJrANdJPf7v/ko2etQLDq2f7Z0V/kvDuWKExM=";
  };

  WHFC = fetchFromGitHub {
    owner = "larsgottesbueren";
    repo = "WHFC";
    rev = "30b0eeb0e49577d06c3deb09a44b035d81c529d2";
    hash = "sha256-2+l3PGOT3dqtL39OZHNQGNwrvEH75xXJOK5SaXmDk8A=";
  };
in
stdenv.mkDerivation rec {
  pname = "mt-kahypar";
  version = "1.5.1";

  outputs = [
    "out"
    "dev"
  ];

  src = fetchFromGitHub {
    owner = "kahypar";
    repo = "mt-kahypar";
    rev = "v${version}";
    hash = "sha256-2USu34LV60boup+hDftMPpAWdrFyimZA6q5Rx40xW7s=";
  };

  nativeBuildInputs = [
    cmake
    git
  ];

  buildInputs = [
    boost.dev
    hwloc.dev
    tbb_2022_0.dev
  ] ++ lib.optionals stdenv.hostPlatform.isWindows [ windows.pthreads ];

  patches = [
    ./mt-kahypar-cmake.patch
    ./mt-kahypar-pc.patch
  ];

  cmakeFlags = [
    "-D FETCHCONTENT_SOURCE_DIR_KAHYPAR-SHARED-RESOURCES=${shared-resources}"
    "-D FETCHCONTENT_SOURCE_DIR_WHFC=${WHFC}"
  ];

  buildPhase = "cmake --build . --target mtkahypar --parallel $NIX_BUILD_CORES";
  installPhase = "cmake --install .";

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
