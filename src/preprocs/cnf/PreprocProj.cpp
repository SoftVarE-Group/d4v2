
#include "PreprocProj.hpp"
#include "src/problem/cnf/ProblemManagerCnf.hpp"

namespace d4 {

/**
   The constructor.

   @param[in] vm, the options used (solver).
 */
PreprocProj::PreprocProj(Config &d4Config, std::ostream &out) {

  ws = WrapperSolver::makeWrapperSolverPreproc(d4Config, out);
  config.ve_dve = d4Config.preproc_equiv;
  config.ve_check = d4Config.preproc_ve_check;
  config.ve_only_simpical = d4Config.preproc_ve_only_simpical;
  config.ve_prefer_simpical = d4Config.preproc_ve_prefer_simpical;
  config.ve_limit = d4Config.preproc_ve_limit;
  keep_map = false;
} // constructor

/**
   Destructor.
 */
PreprocProj::~PreprocProj() { delete ws; } // destructor

/**
 * @brief The preprocessing itself.
 * @param[out] p, the problem we want to preprocess.
 * @param[out] lastBreath gives information about the way the    preproc sees
 * the problem.
 */

ProblemManager *PreprocProj::run(ProblemManager *pin,
                                 LastBreathPreproc &lastBreath) {

  // TODO remove, only for testing
  if (pin->getSelectedVar().empty()) {
    for (int i = 1; i <= pin->getNbVar(); i++) {
      pin->getSelectedVar().push_back(i);
    }
  }
  ProblemManagerCnf *in = (ProblemManagerCnf *)pin;
  using namespace PRE;
  Instance<double> ins;
  ins.weighted = false;
  ins.vars = in->getNbVar();
  ins.npvars = in->getNbSelectedVar();
  ins.projected = ins.vars != ins.npvars;
  ins.ispvars.resize(ins.vars, false);
  ins.assigns.resize(ins.vars, Glucose::l_Undef);
  for (auto v : in->getSelectedVar()) {
    ins.pvars.push_back(v - 1);
    ins.ispvars[v - 1] = true;
  }
  for (auto &cl : in->getClauses()) {
    std::vector<Glucose::Lit> lits(cl.size());
    for (int i = 0; i < cl.size(); i++) {
      lits[i] = Glucose::mkLit(cl[i].var() - 1, cl[i].sign());
    }
    ins.addClause(lits, false);
  }
  if (ins.projected) {
    for (auto v : ins.pvars)
      ins.gmap.push_back(Glucose::mkLit(v));
  } else {
    for (int i = 0; i < ins.npvars; i++)
      ins.gmap.push_back(Glucose::mkLit(i));
  }
  ins.keepVarMap = keep_map;
  
    
  Preprocessor<double> preproc;
  preproc.setConfig(config);

  preproc.Simplify(&ins);
  if (ins.unsat) {
    return in->getUnsatProblem();
  }
  std::vector<double> weight(ins.vars + ins.freevars + 2, 2.0);
  std::vector<double> weightLit((ins.vars + ins.freevars + 2) << 2, 1.0);
  for (unsigned i = 0; i <= ins.vars + ins.freevars; i++)
    weight[i] = weightLit[i << 1] + weightLit[(i << 1) + 1];
  std::vector<Var> selected;
  for (Var i = 1; i <= ins.npvars; i++) {
    selected.push_back(i);
  }
  ProblemManagerCnf *out = new ProblemManagerCnf(ins.vars, weightLit, weight,
                                                 selected, ins.freevars);

  if (keep_map) {
    out->gmap().resize(ins.gmap.size());
    for (int i = 0; i < ins.gmap.size(); i++) {
      if (ins.gmap[i] == Glucose::lit_Undef) {
        out->gmap()[i] = lit_Undef;
      } else {
        out->gmap()[i] = Lit::makeLit(Glucose::var(ins.gmap[i]) + 1,
                                      Glucose::sign(ins.gmap[i]));
      }
    }
    std::cout<<"Fixed Lits: "<<std::endl;
    for(auto i: ins.fixedLits){
        std::cout<<(Glucose::sign(i)?"-":" ")<<Glucose::var(i)+1<<" ,";
    }
  }

  for (auto &cl : ins.clauses) {
    std::vector<Lit> clause(cl.size());
    for (int i = 0; i < cl.size(); i++) {
      clause[i] = Lit::makeLit(Glucose::var(cl[i]) + 1, Glucose::sign(cl[i]));
    }
    out->getClauses().push_back(clause);
  }
  for (auto &cl : ins.learnts) {
    std::vector<Lit> clause(cl.size());
    for (int i = 0; i < cl.size(); i++) {
      clause[i] = Lit::makeLit(Glucose::var(cl[i]) + 1, Glucose::sign(cl[i]));
    }
    lastBreath.learnt.push_back(clause);
  }
  ws->initSolver(*out);
  lastBreath.panic = 0;
  lastBreath.countConflict.resize(out->getNbVar() + 1, 0);

  if (!ws->solve())
    return out->getUnsatProblem();
  lastBreath.panic = ws->getNbConflict() > 100000;

  // get the activity given by the solver.
  for (unsigned i = 1; i <= out->getNbVar(); i++)
    lastBreath.countConflict[i] = ws->getCountConflict(i);

  std::vector<Lit> units;
  ws->getUnits(units);
  auto final = out->getConditionedFormula(units);

  delete out;
  return final;

} // run
} // namespace d4
