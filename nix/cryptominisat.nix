{
  lib,
  stdenv,
  fetchFromGitHub,
  cmake,
  gmp,
  zlib,
  cadiback,
}:
stdenv.mkDerivation rec {
  pname = "cryptominisat";
  version = "5.11.22";

  outputs = [
    "out"
    "lib"
    "dev"
  ];

  src = fetchFromGitHub {
    owner = "msoos";
    repo = "cryptominisat";
    rev = version;
    hash = "sha256-9Uk2exQWCkL/eqF7d1++BkXyl/gxVi4ThB4kv7F7BbE=";
  };

  patches = [ ./cryptominisat-cadiback-include.patch ];

  nativeBuildInputs = [ cmake ];

  buildInputs = [
    zlib.dev
    cadiback.dev
    gmp.dev
  ];

  meta = {
    mainProgram = "cryptominisat5";
    description = "An advanced SAT solver";
    homepage = "https://github.com/msoos/cryptominisat";
    license = lib.licenses.mit;
    platforms = lib.platforms.unix ++ lib.platforms.windows;
  };
}
