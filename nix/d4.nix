{
  lib,
  stdenv,
  cmake,
  boost,
  gmp,
  mpfr,
  zlib,
  mt-kahypar,
  cryptominisat,
  sbva,
  arjun,
  gpmc,
  glucose,
  cadical,
  cadiback,
  tbb_2022,
}:
let
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
stdenv.mkDerivation {
  pname = "d4";
  version = "2.0";

  src = ./..;

  nativeBuildInputs = [ cmake ];

  buildInputs = [
    mt-kahypar
    cryptominisat.dev
    sbva.dev
    arjun.dev
    boost.dev
    gmp.dev
    mpfr.dev
    zlib.dev
    gpmc.dev
    glucose.dev
    cadical.dev
    cadiback.dev
    tbb.dev
  ];

  meta = with lib; {
    mainProgram = "d4";
    description = "A CNF to d-DNNF compiler";
    homepage = "https://github.com/SoftVarE-Group/d4v2";
    license = licenses.lgpl21Plus;
    platforms = platforms.unix ++ platforms.windows;
  };
}
