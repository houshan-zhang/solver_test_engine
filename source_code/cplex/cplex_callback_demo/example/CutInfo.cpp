

#include "CutInfo.h"


// MIP  callback调用
void CutInfo::MakeUserCuts(
    CPXCENVptr env,
    CPXCLPptr lp,
    void *cbdata,
    int wherefrom,
    const int& outputInfoLevel,
    const int probType,
    const int dataType,
    const int lpType) {
    //Init

    //获得LP中上下界,右端项等数据
    CPXsolution(env, lp, &lpStat, &objVal, gloSolVal, 0, 0, gloReducedCost);
    CPXgetlb(env, lp, gloLb, 0, gloNumCols - 1);
    CPXgetub(env, lp, gloUb, 0, gloNumCols - 1);
    CPXgetsense(env, lp, gloConSense, 0, gloNumRows - 1);
    CPXgetrhs(env, lp, gloConRhs, 0, gloNumRows - 1);

    // 中间如何生成割平面的过程省略


    //下面三种为加入割平面的方式.
    if (strcmp(addCutType,"CPX_USECUT_PURGE") == 0)
    {
        CPXcutcallbackadd (env, cbdata, wherefrom, numCutCoef, cutRhs, 'G' \
                        , cutIndex, cutCoefVal, CPX_USECUT_PURGE);
    }
    else if (strcmp(addCutType,"CPX_USECUT_FILTER") == 0)
    {
        CPXcutcallbackadd (env, cbdata, wherefrom, numCutCoef, cutRhs, 'G' \
                        , cutIndex, cutCoefVal, CPX_USECUT_FILTER);
    }
    else if (strcmp(addCutType,"CPX_USECUT_FORCE") == 0)
    {
        CPXcutcallbackadd (env, cbdata, wherefrom, numCutCoef, cutRhs, 'G' \
                        , cutIndex, cutCoefVal, CPX_USECUT_FORCE);
    }

} //END MakeUserCuts(MIP)




