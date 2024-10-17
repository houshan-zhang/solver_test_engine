#ifndef VMCsolver_H
#define VMCsolver_H

#include "../include/cplex.h"   //  Header for
#include "../include/cplexl.h"
#include"CutInfo.h"


static int CPXPUBLIC myCutCallback (CPXCENVptr env, void *cbdata, int wherefrom,void *cbhandle,int *useraction_p); //

class VMCsolver{
    private:

    public:
    //  Public variables
        CutInfo* cutInfoSP;



        VMCsolver();


        ~VMCsolver();


        bool Run(const int& outputInfoLevel);


};


#endif // VMCsolver_H
