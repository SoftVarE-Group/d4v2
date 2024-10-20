{
  lib,
  stdenv,
  cmake,
  gmp,
  zlib,
  mpfr,
  arjun,
  cryptominisat,
}:
stdenv.mkDerivation {
  pname = "gpmc";
  version = "1.0.1";

  outputs = [
    "out"
    "dev"
  ];

  src = ./..;

  nativeBuildInputs = [ cmake ];

  preConfigure = "cd 3rdParty/GPMC";

  buildInputs = [
    gmp.dev
    mpfr.dev
    zlib.dev
    arjun.dev
    cryptominisat.dev
  ];

  meta = {
    description = "Exact model counter for CNF formulas";
    homepage = "https://git.trs.css.i.nagoya-u.ac.jp/k-hasimt/GPMC";
    license = lib.licenses.mit;
    platforms = lib.platforms.unix ++ lib.platforms.windows;
  };
}
