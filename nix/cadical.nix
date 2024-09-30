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
    owner = "uulm-janbaudisch";
    repo = "cadical";
    rev = "31c08cf1feca40146b17a0db4d2e669914713183";
    hash = "sha256-8Grth+su2/oeLxshFxU2oT2VUHdFYdZa7XDaeyiUEJc=";
  };

  nativeBuildInputs = [ cmake ];

  meta = {
    mainProgram = "cadical";
    description = "CaDiCaL SAT Solver";
    homepage = "https://github.com/arminbiere/cadical";
    license = lib.licenses.mit;
    platforms = lib.platforms.unix ++ lib.platforms.windows;
  };
}
