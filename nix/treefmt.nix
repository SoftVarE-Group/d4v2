{ ... }:
{
  programs.nixfmt.enable = true;
  programs.clang-format.enable = true;
  settings.formatter.clang-format.excludes = [
    "3rdParty/*"
    "scripts/*"
  ];
}
