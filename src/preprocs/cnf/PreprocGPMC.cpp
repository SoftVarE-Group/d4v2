
#include "PreprocGPMC.hpp"
#include "gpmc.hpp"

namespace d4 {

PreprocGPMC::PreprocGPMC(Config &config, std::ostream &out) {
 



  ws = WrapperSolver::makeWrapperSolverPreproc(config, out);
}

PreprocGPMC::~PreprocGPMC() { delete ws; }
ProblemManager *PreprocGPMC::run(ProblemManager *pin,
                                 LastBreathPreproc &lastBreath) {

  ProblemManagerCnf *cnf = (ProblemManagerCnf *)pin;


  
  int vars = cnf->getNbVar();
  int pvars = cnf->getSelectedVar().size();
  int freevars =0;
  std::vector<Lit> asignes;
  std::vector<Lit> gmap;
  std::vector<std::vector<Lit>> clauses = cnf->getClauses();
  if (!GPMC::simplify(clauses,pin->getSelectedVar(),lastBreath.learnt,vars,pvars,freevars,asignes,gmap,false,true)){
      return pin->getUnsatProblem();
  }
  std::vector<double> weight(vars+1+freevars,1.0); 
  std::vector<double> weightLit((vars+1+freevars)*2,1.0);
  std::vector<Var> selected;
  for(Var i = 1;i<pvars;i++){
      selected.push_back(i);
  }
  std::cout<<"Vars "<<vars<<" PVars"<<pvars<<std::endl;
  ProblemManagerCnf* out = new ProblemManagerCnf(vars,weightLit,weight,selected,freevars); 
  out->getClauses() = std::move(clauses);

  ws->initSolver(*out);
  lastBreath.panic = 0;
  lastBreath.countConflict.resize(out->getNbVar() + 1, 0);

  if (!ws->solve()) return out->getUnsatProblem();
  lastBreath.panic = ws->getNbConflict() > 100000;

  // get the activity given by the solver.
  for (unsigned i = 1; i <= out->getNbVar(); i++)
    lastBreath.countConflict[i] = ws->getCountConflict(i);

  std::vector<Lit> units;
  ws->getUnits(units);
  auto final =  out->getConditionedFormula(units); 

  delete out;
  return final;
}

} // namespace d4
