{
  lib,
  stdenv,
  cmake,
  gmp,
  arjun
}:
stdenv.mkDerivation {
  pname = "gmpc";
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
    arjun.dev
  ];

  meta = {
    description = "Exact model counter for CNF formulas";
    homepage = "https://git.trs.css.i.nagoya-u.ac.jp/k-hasimt/GPMC";
    license = lib.licenses.mit;
    platforms = lib.platforms.unix ++ lib.platforms.windows;
  };
}
