{
  lib,
  stdenv,
  cmake,
  zlib,
}:
stdenv.mkDerivation {
  pname = "glucose";
  version = "3.0.0";

  outputs = [
    "out"
    "lib"
    "dev"
  ];

  src = ./../3rdParty/glucose-3.0;

  nativeBuildInputs = [ cmake ];

  buildInputs = [ zlib.dev ];

  meta = {
    description = "SAT solver";
    homepage = "https://github.com/audemard/glucose";
    license = lib.licenses.mit;
    platforms = lib.platforms.unix ++ lib.platforms.windows;
  };
}
