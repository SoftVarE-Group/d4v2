#pragma once
#include "ProblemManagerCnf.hpp"
#include "src/preprocs/cnf/PreprocGPMC.hpp"
#include "3rdParty/GPMC/core/Instance.h"


namespace d4{
class ProblemManagerGPMC:public ProblemManagerCnf{
    
public:
    ProblemManagerGPMC(GPMCInstance& instance,IDIDFunc proj_map){
        m_nbVar = instance.vars;
        m_projMap = proj_map;
        for(Var i = 1;i<=instance.npvars;i++){ 
            m_selected.push_back(i);
        }
        m_weightVar.resize((m_nbVar+1)+2,1.0);
        m_weightVar.resize((m_nbVar+2));
        m_isUnsat = instance.unsat;
        m_clauses.resize(instance.clauses.size());
        for(int i =0;i<instance.clauses.size();i++){
            auto &cl = instance.clauses[i];
            std::vector<Lit> ncl(cl.size());
            for(int k = 0;k<cl.size();k++){
                ncl[k] = Lit::makeLit( Glucose::var( cl[k])+1,Glucose::sign(cl[k]));
            }
        }
        


    }

};
}
