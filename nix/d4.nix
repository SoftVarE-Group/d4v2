{
  lib,
  stdenv,
  fetchgit,
  cmake,
  boost,
  gmp,
  mpfr,
  zlib,
  mt-kahypar,
  arjun,
  gpmc,
  glucose,
  sbva,
  cadical,
  cadiback,
}:
stdenv.mkDerivation rec {
  pname = "d4";
  version = "2.0";

  src = ./..;

  nativeBuildInputs = [ cmake ];

  buildInputs = [
    mt-kahypar.dev
    arjun.dev
    boost.dev
    gmp.dev
    mpfr.dev
    zlib.dev
    gpmc.dev
    glucose.dev
    cadical.dev
    cadiback.dev
  ];

  meta = with lib; {
    mainProgram = "d4";
    description = "A CNF to d-DNNF compiler";
    homepage = "https://github.com/SoftVarE-Group/d4v2";
    license = licenses.lgpl21Plus;
    platforms = platforms.unix ++ platforms.windows;
  };
}
