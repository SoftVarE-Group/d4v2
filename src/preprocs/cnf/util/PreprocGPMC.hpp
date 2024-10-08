#pragma once
#include "3rdParty/glucose-3.0/core/Solver.h"

#include "3rdParty/glucose-3.0/utils/System.h"
#include "lib_sharpsat_td/utils.hpp"
#include "TestSolver.hpp"
#include <vector>
// largely based on gpmc  https://git.trs.css.i.nagoya-u.ac.jp/k-hasimt/GPMC
namespace PRE {

struct ConfigPreprocessor {
	int varlimit;
	double timelim;
	double reps;
	int verb;

	// EE
	bool ee;
	int ee_varlim;

	// VE/DefVE
	bool ve;
    bool ve_dve;
	int ve_reps;
	int dve_reps;
    int ve_limit;
    bool ve_only_simpical;
	bool ve_more;
    bool ve_check;
    bool ve_prefer_simpical;
	double dve_timelimit;
    

	// CS
	bool cs;
    ConfigPreprocessor(){
	    varlimit = 200000;
	    timelim = 120;
	    reps = 20;
	    verb = 0;
	    ee = true;
	    ee_varlim = 150000;
	    ve = true;
	    ve_reps =  400;
	    dve_reps = 10;
	    ve_more = true;
        ve_dve = false;
        ve_check = true;
        ve_prefer_simpical=true;
	    dve_timelimit  = 60;
	    cs = true;
        ve_limit = 4;
        ve_only_simpical = false;
    }
};

template <class T_data>
class Instance {
public:
	Instance();
	bool addClause	(std::vector<Glucose::Lit>& lits, bool learnt=false);

	Glucose::lbool value (Glucose::Var x) const;
	Glucose::lbool value (Glucose::Lit p) const;

	// CNF formula
	int vars;
	std::vector<std::vector<Glucose::Lit>> clauses;
	std::vector<std::vector<Glucose::Lit>> learnts;

	// Counter Mode
	bool weighted;
	bool projected;

	// For WMC/WPMC
	std::vector<T_data> lit_weights;

	// For PMC/WPMC
	int npvars;
	std::vector<bool> ispvars;
	std::vector<Glucose::Var> pvars;

	// Instance keeps temporal fixed literals. Preprocessor will eliminate fixed variables.
	std::vector<Glucose::lbool>	assigns;
	std::vector<Glucose::Lit>	assignedLits;

	// additional information
	int freevars;
	T_data gweight;

	// information about correspondence with a given CNF
	bool keepVarMap;
	std::vector<Glucose::Lit> gmap;								// variable renaming (original (Var) -> simplified (Lit))
	std::vector<Glucose::Lit> fixedLits;						// fixed Literals
	std::vector<Glucose::Var> definedVars;						// variables defined by the other variables
	std::vector<std::vector<Glucose::Lit>> freeLitClasses;	// equivalence classes of free literals

	// extra var score
	std::vector<double> score;

	// State
	bool unsat;

};
template <class T_data>
class Preprocessor {
	friend class Solver;
public:
	Preprocessor() : ins(NULL) { };

	void setConfig(ConfigPreprocessor& conf) { this->config = conf; }

	bool Simplify(Instance<T_data>* ins);

private:
	bool SAT_FLE();
	bool Strengthen();
	bool MergeAdjEquivs();
	bool VariableEliminate(bool dve);
	void pickVars(std::vector<int>& vars);

	void pickDefVars(std::vector<int>& vars);

	int ElimVars(const std::vector<Glucose::Var>& del);

	void Compact(const Glucose::vec<Glucose::lbool>& assigns, const std::vector<Glucose::Var>& elimvars={});
	void CompactClauses(const Glucose::vec<Glucose::lbool>& assigns, std::vector<std::vector<Glucose::Lit>>& cls, 
            std::vector<bool>& occurred, int& varnum);
	void RewriteClauses(std::vector<std::vector<Glucose::Lit>>& cls, const std::vector<Glucose::Var>& map);
	void RewriteClauses(std::vector<std::vector<Glucose::Lit>>& cls, const std::vector<Glucose::Lit>& map);
	void Subsume();

	bool isVECandidate(Graph& G,bool simple, std::vector<int>& freq,std::vector<double>& cl_size, int i) const;

	void printCNFInfo(const char* ppname = "", bool initial = false);


	Instance<T_data> *ins;
	ConfigPreprocessor config;
};
template <class T_data>
inline void Preprocessor<T_data>::printCNFInfo(const char* ppname, bool initial)
{
	if(initial)
		printf("c o [%-7s] %d vars (%d pvars), %d cls\n", ppname, ins->vars, ins->npvars, ins->clauses.size());
	else
		printf("c o [%-7s] %d vars (%d pvars), %d cls, %d lrnts, %d fvars, elap. %.2lf s\n", ppname, ins->vars, ins->npvars, ins->clauses.size(), ins->learnts.size(), ins->freevars, Glucose::cpuTime());

	if(ins->weighted)
		std::cout << "c o gweight " << ins->gweight << std::endl;

	fflush(stdout);
}
}
