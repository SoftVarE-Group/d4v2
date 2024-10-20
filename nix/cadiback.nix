{
  lib,
  stdenv,
  fetchFromGitHub,
  cadical,
  cmake,
}:
stdenv.mkDerivation {
  pname = "cadiback";
  version = "0.2.1";

  outputs = [
    "out"
    "dev"
  ];

  src = fetchFromGitHub {
    owner = "meelgroup";
    repo = "cadiback";
    rev = "ea65a9442fc2604ee5f4ffd0f0fdd0bf481d5b42";
    hash = "sha256-r9SoZMS1vA+Ggfca6lOmAo9zsAqBu0v0YTXcTyTN9v8=";
  };

  patches = [
    ./cadiback-cmake.patch
    ./cadiback-remove-cadical-internals.patch
  ];

  nativeBuildInputs = [ cmake ];

  buildInputs = [ cadical.dev ];

  meta = {
    description = "CaDiCaL BackBone Analyzer";
    homepage = "https://github.com/meelgroup/cadiback";
    license = lib.licenses.mit;
    platforms = lib.platforms.unix ++ lib.platforms.windows;
  };
}
