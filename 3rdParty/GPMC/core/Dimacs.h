/****************************************************************************************[Dimacs.h]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 **************************************************************************************************/
// Modified so that our counter can accept dimacs cnf files with a specification of projection variables
// by Kenji Hashimoto


#ifndef Glucose1_Dimacs_h
#define Glucose1_Dimacs_h

#include <stdio.h>

#include "utils/ParseUtils.h"
#include "core/SolverTypes.h"

using namespace Glucose1;

namespace GPMC {

//=================================================================================================
// DIMACS Parser:

template<class B, class Solver>
static void readClause(B& in, Solver& S, vec<Lit>& lits) {
	int     parsed_lit, var;
	lits.clear();
	for (;;){
		parsed_lit = parseInt(in);
		if (parsed_lit == 0) break;
		var = abs(parsed_lit)-1;
		while (var >= S.nVars()) S.newVar();
		lits.push( (parsed_lit > 0) ? mkLit(var) : ~mkLit(var) );
	}
}

// --- Added by k-hasimt --- BEGIN
// Read projection vars
template<class B, class Solver>
static void readProjVars(B& in, Solver&S) {
	if(S.mc) { skipLine(in); return; }

	int     parsed_var;
	for (;;){
		parsed_var = parseInt(in);
		if (parsed_var == 0) break;
		while (parsed_var-1 >= S.nVars()) S.newVar();
		S.registerAsPVar(parsed_var-1, true);
	}
}
template<class B, class Solver>
static void readWeight(B& in, Solver &S) {
	int parsed_lit, var;
	double weight;

	parsed_lit = parseInt(in);
	var = abs(parsed_lit)-1;
	weight = parseDoubleWeight(in);
	Lit lit = (parsed_lit > 0) ? mkLit(var) : ~mkLit(var);
	S.setWeight(lit, weight);
	// S.setWeight(~lit, (double)1-weight);

	parsed_lit = parseInt(in);
	if(parsed_lit != 0)
		printf("c o PARSE ERROR! Unexpected char: %c\n", *in), exit(3);
}
// --- Added by k-hasimt --- END

template<class B, class Solver>
static void parse_DIMACS_main(B& in, Solver& S) {
	vec<Lit> lits;
	int vars    = 0;
	int clauses = 0;
	int cnt     = 0;
	int pvars   = 0;

	enum FILEFORMAT { CNFCR, MC2020, MC2021 };
	enum FILEFORMAT format = MC2021;

	for (;;){
		skipWhitespace(in);
		if (*in == EOF) break;
		else if (*in == 'p'){
			B in_b = in;
			if (eagerMatch(in, "p cnf")){
				vars    = parseInt(in);
				clauses = parseInt(in);
				if(S.wc) S.initWeights(2*vars);
				// pvars   = parseInt(in);
				// SATRACE'06 hack
				// if (clauses > 4000000)
				//     S.eliminate(true);
			} else if (eagerMatch(in_b, "p pcnf")) { // added by k-hasimt
				in = in_b;
				vars    = parseInt(in);
				clauses = parseInt(in);
				pvars   = parseInt(in);
				format = MC2020;	// mcc2020
			}else{
				printf("c o PARSE ERROR! Unexpected char: %c\n", *in), exit(3);
			}
			// } else if (*in == 'c' || *in == 'p')
		} else if (*in == 'c') {
			if (eagerMatch(in, "c p")) {
				skipWhitespace(in);
				if (*in == 's') {
					if (eagerMatch(in, "show")) {
						readProjVars(in, S);
						format = MC2021;
					}
				}
				else if(*in == 'w') {
					if (eagerMatch(in, "weight")) {
						readWeight(in, S);
					}
				}
/*
			} else if (eagerMatch(in_c, "cr")) {
				in = in_c;
				readProjVars(in, S);
				format = CNFCR;
*/
			} else
				skipLine(in);
		} else if (*in == 'v'){
			if (eagerMatch(in, "vp")) {  // added by k-hasimt
				readProjVars(in, S);
			} else {
				printf("c o PARSE ERROR! Unexpected char: %c\n", *in), exit(3);
			}
		}
		else{
			cnt++;
			readClause(in, S, lits);
			S.addClause_(lits); }
	}
	if (vars < S.nVars())
		fprintf(stderr, "c o WARNING! DIMACS header mismatch: wrong number of variables.\n");
	if (cnt  != clauses)
		fprintf(stderr, "c o WARNING! DIMACS header mismatch: wrong number of clauses.\n");
	if (format == MC2020 && pvars == S.nPVars())
		fprintf(stderr, "c o WARNING! DIMACS header mismatch: wrong number of pvars.\n");

	if(S.mc) {
		while (vars > S.nVars()) S.newVar();
		S.registerAllVarsAsPVar(vars);
		// Note: We adopt the varnum in the header(p cnf ...) as the number of all variables.
		//       Sometimes the varnum in the header and the number of variables really appearing in the clauses are different.
		//       In such cases, we prefer the header.
	}
	else
		S.registerAsPVar(S.nVars(), false); // Added by k-hasimt  (Assertion:  S.ispvar.size() == nVars()"+1")

	if(S.verbosity_c) {
		printf("c o Input format: ");
		switch(format) {
		case CNFCR: printf("CNF+CR\n"); break;
		case MC2020: printf("MC2020\n");break;
		case MC2021: printf("MC2021\n");break;
		default: printf("Unknown\n");
		}
	}
}

// Inserts problem into solver.
//
template<class Solver>
static void parse_DIMACS(gzFile input_stream, Solver& S) {
	StreamBuffer in(input_stream);
	parse_DIMACS_main(in, S); }

//=================================================================================================
}

#endif
