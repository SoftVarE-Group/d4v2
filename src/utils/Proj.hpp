#pragma once
#include "src/problem/ProblemTypes.hpp"
#include <span>
namespace d4 {
struct ProjVars {
  std::vector<d4::Var> vars;
  int nbProj = 0;
  std::span<d4::Var> iter_proj() {
    return {vars.begin(), vars.begin() + nbProj};
  }
  std::span<d4::Var> iter_nproj() {
    return {vars.begin() + nbProj, vars.end()};
  }
};
} // namespace d4
