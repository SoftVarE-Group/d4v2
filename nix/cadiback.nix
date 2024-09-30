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
    owner = "uulm-janbaudisch";
    repo = "cadiback";
    rev = "f538594d0497a2db127ab9fcfec655aaad5acf04";
    hash = "sha256-T+bMuQV9CPIidwjnwgVB0RYsKDD6hEVwZrCDrEhlVxk=";
  };

  nativeBuildInputs = [ cmake ];

  buildInputs = [ cadical.dev ];

  meta = {
    description = "CaDiCaL BackBone Analyzer";
    homepage = "https://github.com/meelgroup/cadiback";
    license = lib.licenses.mit;
    platforms = lib.platforms.unix ++ lib.platforms.windows;
  };
}
