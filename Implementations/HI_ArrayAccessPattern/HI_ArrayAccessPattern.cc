#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Pass.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "HI_print.h"
#include "HI_ArrayAccessPattern.h"
#include <stdio.h>
#include <string>
#include <ios>
#include <stdlib.h>

using namespace llvm;
 
bool HI_ArrayAccessPattern::runOnFunction(Function &F) // The runOnModule declaration will overide the virtual one in ModulePass, which will be executed for each Module.
{
    DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
    ScalarEvolution &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();
    bool changed = false;
    bool ActionTaken = true;
    TraceMemoryAccessinFunction(F);
    while (ActionTaken)
    {
        ActionTaken = false;
        for (auto &B : F)
        {
            for (auto &I : B)
            {
            //    ActionTaken = LSR_Mul(&I,&SE);
                ArrayAccessOffset(&I,&SE);
                changed |= ActionTaken;
                if (ActionTaken)
                    break;
            }
            if (ActionTaken)
                break;
        }
    }

    // return false;
    return changed;
}



char HI_ArrayAccessPattern::ID = 0;  // the ID for pass should be initialized but the value does not matter, since LLVM uses the address of this variable as label instead of its value.

void HI_ArrayAccessPattern::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
    AU.addRequired<TargetTransformInfoWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.setPreservesCFG();
}


// check whether the instruction is Multiplication suitable for LSR
// If suitable, process it
bool HI_ArrayAccessPattern::ArrayAccessOffset(Instruction *I, ScalarEvolution *SE)
{
/*
1.  get the incremental value by using SCEV
2.  insert a new PHI (carefully select the initial constant)
3.  replace multiplication with addition
*/
    if (I->getOpcode() != Instruction:: Add)
        return false;
    if (Inst_AccessRelated.find(I) == Inst_AccessRelated.end())
        return false;

    auto ITP_I = dyn_cast<IntToPtrInst>(I->use_begin()->getUser());

    if (!ITP_I)
        return false;

    // 1.  get the incremental value by using SCEV
    const SCEV *tmp_S = SE->getSCEV(I);
    const SCEVAddRecExpr *SARE = dyn_cast<SCEVAddRecExpr>(tmp_S);
    if (SARE)
    {
        if (SARE->isAffine())
        {
            *AggrLSRLog << *I << " --> is add rec Affine Add: " << *SARE  << " it operand (0) " << *SARE->getOperand(0)  << " it operand (1) " << *SARE->getOperand(1) << "\n";
            if (const SCEVConstant *start_V = dyn_cast<SCEVConstant>(SARE->getOperand(0)))
            {
                if (const SCEVConstant *step_V = dyn_cast<SCEVConstant>(SARE->getOperand(1)))
                {
                    // int start_val = start_V->getAPInt().getSExtValue();
                    // int step_val = step_V->getAPInt().getSExtValue();
                    // APInt start_val_APInt = start_V->getAPInt();
                    // APInt step_val_APInt = step_V->getAPInt();
                    // LSR_Process(I, start_val_APInt, step_val_APInt);
                    // return true;
                }
            }
        }
        if (SARE->isQuadratic())
        {
            *AggrLSRLog << *I << " --> is add rec Quadratic Add: " << *SARE  << " it operand (0) " << *SARE->getOperand(0)  << " it operand (1) " << *SARE->getOperand(1)  << " it operand (2) " << *SARE->getOperand(2) << "\n";
            if (const SCEVConstant *start_V = dyn_cast<SCEVConstant>(SARE->getOperand(0)))
            {
                if (const SCEVConstant *step_V = dyn_cast<SCEVConstant>(SARE->getOperand(1)))
                {
                    // int start_val = start_V->getAPInt().getSExtValue();
                    // int step_val = step_V->getAPInt().getSExtValue();
                    // APInt start_val_APInt = start_V->getAPInt();
                    // APInt step_val_APInt = step_V->getAPInt();
                    // LSR_Process(I, start_val_APInt, step_val_APInt);
                    // return true;
                }
            }
        }
    }
    return false;
}


// find the instruction operand of the Mul operation
Instruction* HI_ArrayAccessPattern::find_Incremental_op(Instruction *Mul_I)
{
    for (int i = 0; i < Mul_I->getNumOperands(); i++)
    {
        if (auto res_I = dyn_cast<Instruction>(Mul_I->getOperand(i)))
            return res_I;
    }
    return nullptr;
}

// find the constant operand of the Mul operation
ConstantInt* HI_ArrayAccessPattern::find_Constant_op(Instruction *Mul_I)
{
    for (int i = 0; i < Mul_I->getNumOperands(); i++)
    {
        if (auto res_I = dyn_cast<ConstantInt>(Mul_I->getOperand(i)))
            return res_I;
    }
    return nullptr;
}

// check the memory access in the function
void HI_ArrayAccessPattern::TraceMemoryAccessinFunction(Function &F)
{
    if (F.getName().find("llvm.")!=std::string::npos) // bypass the "llvm.xxx" functions..
        return;
    findMemoryAccessin(&F);        
    
}


// find the array access in the function F and trace the accesses to them
void HI_ArrayAccessPattern::findMemoryAccessin(Function *F)
{
    *AggrLSRLog << "checking the Memory Access information in Function: " << F->getName() << "\n";
    ValueVisited.clear();


    // for general function in HLS, arrays in functions are usually declared with alloca instruction
    for (auto &B: *F)
    {
        for (auto &I: B)
        {
            if (IntToPtrInst *ITP_I = dyn_cast<IntToPtrInst>(&I))
            {
                *AggrLSRLog << "find a IntToPtrInst: [" << *ITP_I << "] backtrace to its operands.\n";
                TraceAccessForTarget(ITP_I);
            }
        }
    }
    *AggrLSRLog << "-------------------------------------------------" << "\n\n\n\n";
    AggrLSRLog->flush();
}

// find out which instrctuins are related to the array, going through PtrToInt, Add, IntToPtr, Store, Load instructions
void HI_ArrayAccessPattern::TraceAccessForTarget(Value *cur_node)
{
    *AggrLSRLog << "looking for the operands of " << *cur_node<< "\n";
    if (Instruction* tmpI = dyn_cast<Instruction>(cur_node))
    {       
        Inst_AccessRelated.insert(tmpI);
    }
    else
    {
        return;
    }    

    Instruction* curI = dyn_cast<Instruction>(cur_node);
    AggrLSRLog->flush();

    // we are doing DFS now
    if (ValueVisited.find(cur_node)!=ValueVisited.end())
        return;

    ValueVisited.insert(cur_node);

    // Trace the uses of the pointer value or integer generaed by PtrToInt
    for (int i = 0; i < curI->getNumOperands(); ++i)
    {
        Value * tmp_op = curI->getOperand(i);
        TraceAccessForTarget(tmp_op);
    }
    ValueVisited.erase(cur_node);
}


// trace back to find the original PHI operator, bypassing SExt and ZExt operations
// according to which, we can generate new PHI node for the MUL operation
PHINode* HI_ArrayAccessPattern::byPassBack_BitcastOp_findPHINode(Value* cur_I_value)
{
    auto cur_I = dyn_cast<Instruction>(cur_I_value);
    assert(cur_I && "This should be an instruction.\n");
    // For ZExt/SExt Instruction, we do not need to consider those constant bits
    if (cur_I->getOpcode() == Instruction::ZExt || cur_I->getOpcode() == Instruction::SExt )
    {
        if (auto next_I = dyn_cast<Instruction>(cur_I->getOperand(0)))
        {
            return byPassBack_BitcastOp_findPHINode(next_I);
        }
        else
        {
            assert(false && "Predecessor of bitcast operator should be found.\n");
        }
    }
    else
    {
        if (auto PHI_I = dyn_cast<PHINode>(cur_I))
        {
            return PHI_I;
        }
        else
        {
            return nullptr;
        }     
    }    
}
