{
  lib,
  stdenv,
  fetchFromGitHub,
  cmake,
  boost,
  mpfr,
  cryptominisat,
  sbva,
}:
stdenv.mkDerivation {
  pname = "arjun";
  version = "2.5.4";

  outputs = [
    "out"
    "lib"
    "dev"
  ];

  src = fetchFromGitHub {
    owner = "meelgroup";
    repo = "arjun";
    rev = "f2b736f6b49425c95060868a0ec5b69b89c9c3b6";
    hash = "sha256-80DE1fRY4YKAv7d1YoOioUA7AvFsv5OKOobc/YSoqNo=";
  };

  patches = [ ./arjun-remove-feenableexcept.patch ];

  nativeBuildInputs = [ cmake ];

  buildInputs = [
    boost.dev
    mpfr.dev
    cryptominisat.dev
    sbva.dev
  ];

  meta = {
    mainProgram = "arjun";
    description = "CNF minimizer and minimal independent set calculator";
    homepage = "https://github.com/meelgroup/arjun";
    license = lib.licenses.mit;
    platforms = lib.platforms.unix ++ lib.platforms.windows;
  };
}
