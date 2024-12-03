{
  lib,
  stdenv,
  fetchFromGitHub,
  cmake,
  git,
  boost,
  hwloc,
  tbb,
  windows,
}:
stdenv.mkDerivation rec {
  pname = "mt-kahypar";
  version = "1.4";

  outputs = [
    "out"
    "dev"
  ];

  src = fetchFromGitHub {
    owner = "kahypar";
    repo = "mt-kahypar";
    rev = "c51ffeaa3b1040530bf821b7f323e3790b147b33";
    hash = "sha256-MlF6ZGsqtGQxzDJHbvo5uFj+6w8ehr9V4Ul5oBIGzws=";
    fetchSubmodules = true;
  };

  nativeBuildInputs = [
    cmake
    git
  ];

  buildInputs = [
    boost.dev
    hwloc.dev
    tbb.dev
  ] ++ lib.optionals stdenv.hostPlatform.isWindows [ windows.pthreads ];

  patches =
    # Mt-KaHyPar's CMake build does not properly configure the pkg-config files leading to a build error.
    [ ./mt-kahypar-pc.patch ]
    ++ lib.optionals (stdenv.hostPlatform.system == "aarch64-linux") [
      # SSE4 is not supported on aarch64.
      ./mt-kahypar-disable-sse.patch
      # growt seems to not be compatible with aarch64.
      ./mt-kahypar-remove-growt.patch
    ];

  cmakeFlags = [
    "-D KAHYPAR_PYTHON=false"
    "-D MT_KAHYPAR_DISABLE_BOOST=true"
    "-D KAHYPAR_ENFORCE_MINIMUM_TBB_VERSION=false"
  ];

  buildPhase = "cmake --build . --target mtkahypar --parallel $NIX_BUILD_CORES";
  installPhase = "cmake --install .";

  meta = with lib; {
    description = "A shared-memory multilevel graph and hypergraph partitioner";
    longDescription = ''
      Mt-KaHyPar (Multi-Threaded Karlsruhe Hypergraph Partitioner) is a shared-memory multilevel graph and hypergraph partitioner equipped with parallel implementations of techniques used in the best sequential partitioning algorithms.
      Mt-KaHyPar can partition extremely large hypergraphs very fast and with high quality.
    '';
    homepage = "https://github.com/kahypar/mt-kahypar";
    license = licenses.mit;
    platforms = platforms.unix ++ platforms.windows;
  };
}
