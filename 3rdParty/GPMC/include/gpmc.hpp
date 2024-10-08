#include "src/problem/ProblemTypes.hpp"
#include <span>
#include <vector>
namespace GPMC {
bool simplify(std::vector<std::vector<d4::Lit>> &clauses,
              std::vector<d4::Var> &sel,
              std::vector<std::vector<d4::Lit>> &learnts, int &vars, int &pvars,
              int &free_vars, std::vector<d4::Lit> &assigns,
              std::vector<d4::Lit> &gmap, bool equiv,bool check);

}
