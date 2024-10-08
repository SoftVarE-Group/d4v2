#ifndef CONFIG_H_
#define CONFIG_H_

#include <string>
#include <float.h>
#include "utils/Options.h"

namespace GPMC {

using namespace Glucose1;

enum Mode { MC, WMC, PMC, WPMC };

struct ConfigCounter {
	Mode mode;
	bool ddnnf;
	int precision;
	double coef_tdscore;
	bool backjump;
	double bj_threshold;
	bool remove_sat_cls;
	bool rmvsatcl_threshold;
	bool natw;
	std::string pp_outfile;
	std::string td_outfile;
	std::string nnf_outfile;
	bool doPreprocss;
	bool doTD;
	bool alwTD;
	bool watchCand;
	std::string vs_infile;
	bool keepVarMap;
};

struct ConfigComponentManager {
	bool weighted;
	bool ddnnf;
	int varSelectionHueristics;
	int cachesize;
};

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
	bool ve_more;
    bool ve_check;
	double dve_timelimit;

	// CS
	bool cs;
};

struct ConfigTreeDecomposition {
	double varlim;		// Limit on #Vars
	double denselim;		// Limit on density of graph
	double ratiolim;		// Limit on ratio (edges/vars) of graph
	double twvarlim;		// Limit on tw/vars
	double timelim;		// Time Limit
};

class Configuration {
public:
	ConfigCounter cntr;
	ConfigComponentManager cm;
	ConfigPreprocessor pp;
	ConfigTreeDecomposition td;

	Configuration();
	~Configuration() { }
};
}
#endif /* CONFIG_H_ */
