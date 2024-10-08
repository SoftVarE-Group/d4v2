#ifndef Preprocessor_h
#define Preprocessor_h

#include <vector>
#include <iostream>
#include <fstream>

#include "core/Config.h"
#include "core/Instance.h"
#include "gmpxx.h"

#include "preprocessor/IFlowCutter.h"
#include "preprocessor/TreeDecomposition.h"

#include "mtl/Vec.h"
#include "utils/System.h"

namespace PPMC {

class Identifier {
public:
	Identifier(int vars) { cidx.resize(2*vars, -1); num_elem = 0; }

	void identify(Glucose1::Lit l1, Glucose1::Lit l2);

	bool hasEquiv() { return num_elem > 0; }
	std::vector<std::vector<Glucose1::Lit>>& getEquivClasses() { return eqc; }
	Glucose1::Lit delegateLit(Glucose1::Lit l) { return (cidx[toInt(l)] == -1) ? l : eqc[cidx[toInt(l)]][0]; }
	int getIndex(Glucose1::Lit l) { return cidx[toInt(l)]; }
	void removeEquivClass(Glucose1::Lit l);

private:
	void MergeEquivClasses(int c1, int c2);

	std::vector<std::vector<Glucose1::Lit>> eqc;
	std::vector<int> cidx;
	int num_elem;
};

template <class T_data>
class Preprocessor {
	friend class Solver;
public:
	Preprocessor() : ins(NULL) { };

	void setConfig(GPMC::ConfigPreprocessor& conf) { this->config = conf; }

	bool Simplify(GPMC::Instance<T_data>* ins);

private:
	bool SAT_FLE();
	bool Strengthen();
	bool MergeAdjEquivs();
	bool VariableEliminate(bool dve);
	void pickVars(std::vector<int>& vars);

	void pickDefVars(std::vector<int>& vars);

	int ElimVars(const std::vector<Glucose1::Var>& del);

	void Compact(const Glucose1::vec<Glucose1::lbool>& assigns, const std::vector<Glucose1::Var>& elimvars={});
	void CompactClauses(const Glucose1::vec<Glucose1::lbool>& assigns, std::vector<std::vector<Glucose1::Lit>>& cls, std::vector<bool>& occurred, int& varnum);
	void RewriteClauses(std::vector<std::vector<Glucose1::Lit>>& cls, const std::vector<Glucose1::Var>& map);
	void RewriteClauses(std::vector<std::vector<Glucose1::Lit>>& cls, const std::vector<Glucose1::Lit>& map);
	void Subsume();

	bool isVECandidate(Graph& G,bool simple, std::vector<int>& freq,std::vector<float>& cl_size, int i) const;

	void printCNFInfo(const char* ppname = "", bool initial = false);

	GPMC::Instance<T_data> *ins;
	GPMC::ConfigPreprocessor config;
};

template <class T_data>
inline void Preprocessor<T_data>::printCNFInfo(const char* ppname, bool initial)
{
	if(initial)
		printf("c o [%-7s] %d vars (%d pvars), %d cls\n", ppname, ins->vars, ins->npvars, ins->clauses.size());
	else
		printf("c o [%-7s] %d vars (%d pvars), %d cls, %d lrnts, %d fvars, elap. %.2lf s\n", ppname, ins->vars, ins->npvars, ins->clauses.size(), ins->learnts.size(), ins->freevars, Glucose1::cpuTime());

	if(ins->weighted)
		std::cout << "c o gweight " << ins->gweight << std::endl;

	fflush(stdout);
}

}

#endif /* Preprocessor_h */
