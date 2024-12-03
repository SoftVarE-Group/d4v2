#include "../core/Instance.h"

#include "mtl/Sort.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>

using namespace Glucose1;
using namespace GPMC;
using namespace std;

template <class T_data>
Instance<T_data>::Instance() :
vars(0),
weighted(false),
projected(false),
npvars(0),
freevars(0),
gweight(1),
keepVarMap(false),
unsat(false)
{}

static inline Lit SignedIntToLit(int signed_int) {
	assert(signed_int != 0);
	int v = abs(signed_int)-1;
	return (signed_int > 0) ? mkLit(v) : ~mkLit(v);
}

static void Tokens(string buf, vector<string>& ret) {
	ret.push_back("");
	for (char c : buf) {
		if (std::isspace(c)) {
			if (!ret.back().empty()) {
				ret.push_back("");
			}
		} else {
			ret.back() += c;
		}
	}
	if (ret.back().empty()) ret.pop_back();
}

template <class T_data>
void Instance<T_data>::load(istream& in, bool weighted, bool projected, bool keepVarMap) {
	this->weighted = weighted;
	this->projected = projected;
	this->keepVarMap = keepVarMap;

	string buf;
	vector<Lit> ps;
	vector<string> tokens;
	bool parse_error = false;
	bool typeSpecified = false;
	npvars = 0;
	ispvars.clear();

	while (getline(in, buf)) {
		if (buf.empty()) continue;
		tokens.clear();
		Tokens(buf, tokens);

		if(tokens.size() == 0) continue;
		else if (tokens[0] == "p") {
			if (tokens.size() == 4 && tokens[1] == "cnf") {
				vars = stoi(tokens[2]);
				int ncls = stoi(tokens[3]);
				assigns.resize(vars, l_Undef);
				assignedLits.clear();

				if(projected) {
					if(ispvars.size() < vars) {
						ispvars.resize(vars, false);

					}
				} else {
					ispvars.resize(vars, true);
					npvars = vars;
				}

				if(weighted)
					lit_weights.resize(2*vars, 1);
			}
			else
				cerr << "c c Header Error!" << endl;
		}
		else if (tokens[0][0] == 'c') {
			if(tokens[0] == "c" && tokens[1] == "p") {
				if(tokens[2] == "weight") {	// Read the weight of a literal
					if(weighted && tokens.size() == 6 && tokens.back() == "0") {
						int lit = stoi(tokens[3]);
						Lit l = SignedIntToLit(lit);
						lit_weights[toInt(l)] = tokens[4];
					}
				}
				else if(tokens[2] == "show") { // Read the list of projected vars
					if(projected && tokens.back() == "0") {
						for(int i=3; i<tokens.size()-1; i++) {
							int v = stoi(tokens[i]);
							if(v-1 >= ispvars.size())
								ispvars.resize(v, false);
							ispvars[v-1] = true;
							if(keepVarMap)
								pvars.push_back(v-1);
							npvars++;
						}
					}
				}
				else if(tokens[2] == "gweight") {
					if(weighted && tokens.size() == 5 && tokens.back() == "0") {
						gweight = tokens[3];
					}
				}
			}
			else if(tokens[0] == "cr") {
				if(projected && tokens.back() == "0") {
					for(int i=1; i<tokens.size()-1; i++) {
						int v = stoi(tokens[i]);
						if(v-1 >= ispvars.size())
							ispvars.resize(v, false);
						ispvars[v-1] = true;
						if(keepVarMap)
							pvars.push_back(v-1);
						npvars++;
					}
				}
			}
		}
		else { // Read clause
			int lit = 0;
			assert(tokens.size() > 0);
			for(auto token : tokens) {
				lit = stoi(token);
				if(lit == 0) break;
				ps.push_back(SignedIntToLit(lit));
			}
			if(lit == 0) {
				if(!addClause(ps)) {
					return;
				}
				ps.clear();
			} else {
				cerr << "c c invalid Clause!" << endl;
			}
		}
	}

	learnts.clear();

	if(keepVarMap) {
		gmap.clear();

		if(projected) {
			for(auto v : pvars)
				gmap.push_back(mkLit(v));
		} else {
			for(int i=0; i<npvars; i++)
				gmap.push_back(mkLit(i));
		}
	}
}

template <class T_data>
bool Instance<T_data>::addClause(vector<Lit>& ps, bool learnt) {
	sort(ps.begin(), ps.end());

	vec<Lit> oc;
	oc.clear();

	Lit p; int i, j, flag = 0;

	for (i = j = 0, p = lit_Undef; i < ps.size(); i++)
		if (value(ps[i]) == l_True || ps[i] == ~p)
			return true;
		else if (value(ps[i]) != l_False && ps[i] != p)
			ps[j++] = p = ps[i];
	ps.resize(j);

	if (ps.size() == 0) {
		unsat = true;
		return false;
	}
	else if (ps.size() == 1) {
		assigns[var(ps[0])] = lbool(!sign(p));
		assignedLits.push_back(ps[0]);
		// No check by UP here. Preprocessor will do later.
	}
	else {
		clauses.push_back({});
		copy(ps.begin(), ps.end(), back_inserter(clauses.back()));
	}
	return true;
}

template <class T_data>
void Instance<T_data>::printVarMapStats() const {
	cout << "c o #PVars(Original)   " << gmap.size() << endl;
	cout << "c o #PVars(Simplified) " << npvars << endl;
	cout << "c o #Fixed_Literals    " << fixedLits.size() << endl;
	cout << "c o #FreeLitClasses    " << freeLitClasses.size() << endl;
	cout << "c o #DefinedVars       " << definedVars.size() << endl;
}

template <class T_data>
void Instance<T_data>::writeVarMap(std::ostream& out) {
	using namespace std;

	// c p vmap orignal_vars simplified_vars
	out << "p vmap " << gmap.size() << " " << npvars << " " << fixedLits.size() << " " << freeLitClasses.size() << " " << definedVars.size() << endl;

	// fixed literals:
	//  c fix l1 .... ln 0
	if(fixedLits.size() > 0) {
		out << "a ";
		if(projected) {
			for(Lit l : fixedLits)
				out << (sign(l)?"-":"") << (pvars[var(l)]+1) << " ";
		}
		else {
			for(Lit l : fixedLits)
				out << (sign(l)?"-":"") << (var(l)+1) << " ";
		}

		out << "0" << endl;
	}

	// free literals classes
	//	f l1 .... ln 0
	if(freeLitClasses.size() > 0) {
		if(projected) {
			for(auto flc : freeLitClasses) {
				out << "f ";
				for(Lit l : flc)
					out << (sign(l)?"-":"") << (pvars[var(l)]+1) << " ";
				out << "0" << endl;
			}
		} else {
			for(auto flc : freeLitClasses) {
				out << "f ";
				for(Lit l : flc)
					out << (sign(l)?"-":"") << (var(l)+1) << " ";
				out << "0" << endl;
			}
		}
	}

	// determined vars
	//	c det v1 ... vn 0
	if(definedVars.size() > 0) {
		out << "d ";
		if(projected) {
			for(Var v : definedVars)
				out << (pvars[v]+1) << " ";
		}
		else {
			for(Var v : definedVars)
				out << (v+1) << " ";
		}
		out << "0" << endl;
	}

	// var-to-lit renaming:
	//	v orig_var simplified_lit
	if(projected) {
		for(int i=0; i<gmap.size(); i++) {
			if(gmap[i] != lit_Undef)
				out << "v " << (pvars[i]+1) << " " << (sign(gmap[i])?"-":"") << (var(gmap[i])+1) << endl;
		}
	}
	else {
		for(int i=0; i<gmap.size(); i++)
			if(gmap[i] != lit_Undef)
				out << "v " << (i+1) << " " << (sign(gmap[i])?"-":"") << (var(gmap[i])+1) << endl;;
	}
}

template <typename T_data>
void Instance<T_data>::importVarScore(std::istream& in) {
	score.clear();
	score.resize(npvars, 0);

	vector<Glucose1::Var> rvmap;
	if(projected) {
		rvmap.clear();
		rvmap.resize(vars, -1);
		for(int i=0; i<pvars.size(); i++)
			rvmap[pvars[i]] = i;
	}

	// parse
	vector<string> tokens;
	string buf;
	int var;
	while (getline(in, buf)) {
		if (buf.empty()) continue;
		tokens.clear();
		Tokens(buf, tokens);

		if(tokens.size() == 0) continue;
		if(tokens.size() != 2) {
 			cerr << "c c Parse Error! Invalid line in the given varscore file!" << endl;
 			exit(1);
		}
		else {
			int var = stoi(tokens[0])-1;
			double s = stod(tokens[1]);

			assert(var <= vars);
			if(0 <= var && var > vars) {
				cerr << "c c Parse Error! Invalid line in the given varscore file!" << endl;
				exit(1);
			}
			if(projected)
				score[rvmap[var]] = s;
			else
				score[var] = s;
		}
	}
}

// For Debug
template <class T_data>
 void Instance<T_data>::toDimacs(std::ostream& out)
 {
	using namespace std;

	out << "c free vars: " << freevars << endl;
	out << "p cnf " << vars + freevars << " " << clauses.size() << endl;

	if(!weighted && !projected)
		out << "c t mc" << endl;
	else if(weighted && !projected)
		out << "c t wmc" << endl;
	else if(!weighted && projected)
		out << "c t pmc" << endl;
	else if(weighted && projected)
		out << "c t wpmc" << endl;

	if(projected) {
		out << "c p show ";
		for (int i=0; i<npvars; i++)
			out << (i+1) << " ";
		out << "0" << endl;
	}
	if(weighted) {
		for (int i=0; i<npvars; i++) {
			out << "c p weight " << (i+1) << " " << lit_weights[toInt(mkLit(i))] << " 0" << endl;
			out << "c p weight -" << (i+1) << " " << lit_weights[toInt(~mkLit(i))] << " 0" << endl;
		}

		if(gweight != 1) {
			out << "c p gweight " << gweight << " 0" << endl;
		}
	}

	for(auto c : clauses) {
		for (auto l : c)
			out << (sign(l) ? "-":"") << (var(l)+1) << " ";
		out << "0" << endl;
	}

	// For Debug
	/*
	for(auto c : learnts) {
		for (auto l : c)
			out << (sign(l) ? "-":"") << (var(l)+1) << " ";
		out << "0" << endl;
	}
	*/
}

template class GPMC::Instance<mpz_class>;
