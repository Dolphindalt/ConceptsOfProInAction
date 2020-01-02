#include <iostream>
#include "codegen.h"
#include "node.h"

using namespace std;
using namespace llvm;

extern int yyparse();
extern NBlock *programBlock;

Function *createPrintfFunction(CodeGenContext &context)
{
    vector<Type *> printf_arg_types;
    printf_arg_types.push_back(Type::getInt8PtrTy(global_context));
    FunctionType *printf_type = FunctionType::get(Type::getInt32Ty(global_context), printf_arg_types, true);
    Function *func = Function::Create(printf_type, Function::ExternalLinkage, Twine("printf"), context.module);
    func->setCallingConv(CallingConv::C);
    return func;
}

void createEchoFunction(CodeGenContext &context, Function *printFn)
{
    vector<Type *> echo_arg_types;
    echo_arg_types.push_back(Type::getInt64Ty(global_context));
    FunctionType *echo_type = FunctionType::get(Type::getVoidTy(global_context), echo_arg_types, false);
    Function *func = Function::Create(echo_type, Function::InternalLinkage, Twine("echo"), context.module);
    BasicBlock *bblock = BasicBlock::Create(global_context, "entry", func, 0);
    context.pushBlock(bblock);

    const char *constValue = "%d\n";
    Constant *format_const = ConstantDataArray::getString(global_context, constValue);
    GlobalVariable *var = new GlobalVariable(*context.module,
        ArrayType::get(IntegerType::get(global_context, 8), strlen(constValue)+1),
        true, GlobalValue::PrivateLinkage, format_const, ".str");
    Constant *zero = Constant::getNullValue(IntegerType::getInt32Ty(global_context));

    vector<Constant *> indices;
    indices.push_back(zero);
    indices.push_back(zero);
    Constant *var_ref = ConstantExpr::getGetElementPtr(
        ArrayType::get(IntegerType::get(global_context, 8), strlen(constValue)+1),
        var, indices);
    
    vector<Value *> args;
    args.push_back(var_ref);

    auto argsValues = func->arg_begin();
    Value *toPrint = &*argsValues++;
    toPrint->setName("toPrint");
    args.push_back(toPrint);

    CallInst *call = CallInst::Create(printFn, makeArrayRef(args), "", bblock);
    ReturnInst::Create(global_context, bblock);
    context.popBlock();
}

void createCoreFunctions(CodeGenContext &context)
{
    Function *printFn = createPrintfFunction(context);
    createEchoFunction(context, printFn);
}