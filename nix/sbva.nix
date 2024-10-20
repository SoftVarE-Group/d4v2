{
  lib,
  stdenv,
  fetchFromGitHub,
  cmake,
}:
stdenv.mkDerivation {
  pname = "sbva";
  version = "0.0.0";

  outputs = [
    "out"
    "lib"
    "dev"
  ];

  src = fetchFromGitHub {
    owner = "meelgroup";
    repo = "SBVA";
    rev = "b1c46c0d234c99314f16841e01b874282c94f101";
    hash = "sha256-6VsxHfPpf7+exdeHDNlYYugtVIm/AIde03jnYmCcgjM=";
  };

  patches = [ ./sbva-remove-feenableexcept.patch ];

  nativeBuildInputs = [ cmake ];

  meta = {
    mainProgram = "sbva";
    description = "Structured BVA CNF rewriter";
    homepage = "https://github.com/meelgroup/SBVA";
    license = lib.licenses.mit;
    platforms = lib.platforms.unix ++ lib.platforms.windows;
  };
}
