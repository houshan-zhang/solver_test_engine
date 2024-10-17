#include"VMCsolver.h"


static int CPXPUBLIC myCutCallback (CPXCENVptr env, void *cbdata, int wherefrom,void *cbhandle,int *useraction_p)
{
    *useraction_p = CPX_CALLBACK_DEFAULT;
    CutInfo *cutInfo = (CutInfo*) cbhandle;
    CPXLPptr curNodeLp; //The current node lp.
    int depth = 0;
    int isHaveFeasiSol = 0;
    double currTime = 0;
    CPXgetcallbacknodeinfo(env, cbdata, wherefrom, 0, CPX_CALLBACK_INFO_NODE_DEPTH, &depth);
    CPXgetcallbacknodelp(env, cbdata, wherefrom, &curNodeLp);
    double objVal = -INF;
    CPXgetcallbackinfo(env, cbdata, wherefrom, CPX_CALLBACK_INFO_BEST_REMAINING, &objVal);
    if (depth > 0) //表明非根节点了
    {
        return 0;
    }

    if (fabs(objVal - cutInfo->GetLastObjVal()) < cutInfo->GetLpImprPerc() * fabs(cutInfo->GetLastObjVal())) //相对改进百分比
    {
        cutInfo->SetLastObjVal(objVal);
        return 0;
    }
    else
    {
        cutInfo->SetLastObjVal(objVal);
    }

#ifdef ADDCUT   //The exact separation procedure
    if (cutInfo->GetOutputInfoLevel() <= OUTPUT_LEVEL_FIFTH) // <= 5
    {
        cout << "Callback success! "<<endl;
    }
    //进行精确分离加用户割平面的函数
    cutInfo->MakeUserCuts(env, curNodeLp, cbdata, wherefrom, cutInfo->GetOutputInfoLevel() \
                        , cutInfo->GetProbType(), cutInfo->GetDataType(), cutInfo->GetLpType());
#endif  //END ADDCUT

    return 0;
}
#endif //END #if (defined ONLY_CPLEX) || (defined CUT_AND_SOLVE)

void VMCsolver::SetUserCutCallBack(CPXENVptr env, CutInfo* cutInfo)
{
    status = CPXsetusercutcallbackfunc(env, myCutCallback, cutInfo);
    OutputCplexErrorMsg(status, "Failed to set cut callback function.");

}

bool VMCsolver::Run(const int& outputInfoLevel)
{
    //cutInfoSP是自己创建的类, callback中获得的lp信息如何处理都在cutInfoSP中的函数里处理
    cutInfoSP = new CutInfo(spEnv, sparseProblem, outputInfoLevel, 0);
    //调用Callback
    SetUserCutCallBack(spEnv, cutInfoSP);
    status = CPXmipopt(spEnv, sparseProblem);
    delete cutInfoSP;

}



