#pragma once

#include <vector>
#include <map>

#include "../lib_sharpsat_td/utils.hpp"
#include "core/SolverTypes.h"

namespace sspp {
class Subsumer{
private:
	vector<int> ALU;
	vector<int> ALI;
	int ALIt = 1;
	vector<int> ALF;
	const size_t BSconst = 18; // magic constant
	
	struct vecP {
		size_t B, E, O;
		size_t size() const {
			return E - B;
		}
	};
	
	vector<int> data;

	bool isPrefix(vecP a, vecP b);
	void assumeSize(size_t size);
	bool CSO1(const vector<vecP>& D, size_t b, size_t e, const vecP S, 
					   size_t j, size_t d, const vector<int>& d0Index);
	
public:
	vector<vector<Glucose1::Lit>> Subsume(const vector<vector<Glucose1::Lit>>& clauses);
};
} // namespace sspp
