{
  lib,
  stdenv,
  fetchFromGitHub,
  cmake,
}:
stdenv.mkDerivation {
  pname = "cadical";
  version = "2.0.0";

  outputs = [
    "out"
    "lib"
    "dev"
  ];

  src = fetchFromGitHub {
    owner = "arminbiere";
    repo = "cadical";
    rev = "rel-2.1.0";
    hash = "sha256-sSvJgHxsRaJ/xHEK32fox0MFI7u+pj5ERLfNn2s8kC8=";
  };

  patches = [ ./cadical-cmake.patch ];

  nativeBuildInputs = [ cmake ];

  meta = {
    mainProgram = "cadical";
    description = "CaDiCaL SAT Solver";
    homepage = "https://github.com/arminbiere/cadical";
    license = lib.licenses.mit;
    platforms = lib.platforms.unix ++ lib.platforms.windows;
  };
}
