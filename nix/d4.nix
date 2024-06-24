{
  lib,
  stdenv,
  fetchgit,
  cmake,
  boost,
  gmp,
  mt-kahypar,
}:
stdenv.mkDerivation rec {
  pname = "d4";
  version = "2.0";

  src = ./..;

  nativeBuildInputs = [ cmake ];

  buildInputs = [
    mt-kahypar.dev
    boost.dev
    gmp.dev
  ];

  meta = with lib; {
    mainProgram = "d4";
    description = "A CNF to d-DNNF compiler";
    homepage = "https://github.com/SoftVarE-Group/d4v2";
    license = licenses.lgpl21Plus;
    platforms = platforms.unix ++ platforms.windows;
  };
}
