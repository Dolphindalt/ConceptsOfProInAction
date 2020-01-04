#include "node.h"
#include "codegen.h"
#include "parser.hpp"

using namespace std;

void CodeGenContext::generateCode(NBlock &root)
{
    std::cout << "Generating le code...\n";

    vector<Type *> argTypes;
    FunctionType *ftype = FunctionType::get(Type::getVoidTy(global_context), makeArrayRef(argTypes), false);
    mainFunction = Function::Create(ftype, GlobalValue::InternalLinkage, "main", module);
    BasicBlock *bblock = BasicBlock::Create(global_context, "entry", mainFunction, 0);

    pushBlock(bblock);
    root.codeGen(*this);
    ReturnInst::Create(global_context, bblock);
    popBlock();

    std::cout << "Code is generated.\n";

    legacy::PassManager pm;
    pm.add(createPrintModulePass(outs()));
    pm.run(*module);
}

GenericValue CodeGenContext::runCode()
{
    std::cout << "Running code...\n";
    ExecutionEngine *ee = EngineBuilder(unique_ptr<Module>(module)).create();
    ee->finalizeObject();
    vector<GenericValue> noargs;
    GenericValue v = ee->runFunction(mainFunction, noargs);
    std::cout << "Code was run.\n";
    return v;
}

static Type *typeOf(const NIdentifier &type)
{
    if (type.name.compare("int") == 0)
        return Type::getInt64Ty(global_context);
    else if (type.name.compare("double") == 0)
        return Type::getDoubleTy(global_context);
    return Type::getVoidTy(global_context);
}

// Code generation.

Value *NInteger::codeGen(CodeGenContext &context)
{
    std::cout << "Creating integer: " << value << std::endl;
    return ConstantInt::get(Type::getInt64Ty(global_context), value, true);
}

Value *NDouble::codeGen(CodeGenContext &context)
{
    std::cout << "Creating double: " << value << std::endl;
    return ConstantFP::get(Type::getDoubleTy(global_context), value);
}

Value *NIdentifier::codeGen(CodeGenContext &context)
{
    std::cout << "Creating identifier reference: " << name << std::endl;
    if (context.locals().find(name) == context.locals().end())
    {
        std::cerr << "undeclared variable " << name << endl;
        return NULL;
    }
    return new LoadInst(context.locals()[name], "", false, context.currentBlock());
}

Value *NMethodCall::codeGen(CodeGenContext &context)
{
    Function *function = context.module->getFunction(id.name.c_str());
    if (function == NULL)
    {
        std::cerr << "no function called " << id.name << std::endl;
        return NULL;
    }
    std::vector<Value *> args;
    for (auto it = arguments.begin(); it != arguments.end(); it++)
    {
        args.push_back((**it).codeGen(context));
    }
    CallInst *call = CallInst::Create(function, makeArrayRef(args), "", context.currentBlock());
    std::cout << "Creating function call: " << id.name << std::endl;
    return call;
}

Value *NBinaryOperator::codeGen(CodeGenContext &context)
{
    std::cout << "Creating binary operation: " << op << std::endl;
    Instruction::BinaryOps instr;
    switch (op)
    {
        case TPLUS: instr = Instruction::Add; break;
        case TMINUS: instr = Instruction::Sub; break;
        case TMUL: instr = Instruction::Mul; break;
        case TDIV: instr = Instruction::SDiv; break;
        default: return NULL;
    }
    return BinaryOperator::Create(instr, lhs.codeGen(context), rhs.codeGen(context),
        "", context.currentBlock());
}

Value *NAssignment::codeGen(CodeGenContext &context)
{
    std::cout << "Creating assignment for " << lhs.name << std::endl;
    if (context.locals().find(lhs.name) == context.locals().end())
    {
        std::cerr << "undeclared variable " << lhs.name << std::endl;
        return NULL;
    }
    return new StoreInst(rhs.codeGen(context), context.locals()[lhs.name], false, context.currentBlock());
}

Value *NBlock::codeGen(CodeGenContext &context)
{
    Value *last = NULL;
    for (auto it = statements.begin(); it != statements.end(); it++)
    {
        std::cout << "Generating code for " << typeid(**it).name() << std::endl;
        last = (**it).codeGen(context);
    }
    std::cout << "Creating block" << std::endl;
    return last;
}

Value *NExpressionStatement::codeGen(CodeGenContext &context)
{
    std::cout << "Generating code for " << typeid(expression).name() << std::endl;
    return expression.codeGen(context);
}

Value *NVariableDeclaration::codeGen(CodeGenContext &context)
{
    std::cout << "Creating variable declaration " << type.name << " " << id.name << std::endl;
    AllocaInst *alloc = new AllocaInst(typeOf(type), id.name.c_str(), context.currentBlock());
    context.locals()[id.name] = alloc;
    if (assignmentExpr != NULL)
    {
        NAssignment assn(id, *assignmentExpr);
        assn.codeGen(context);
    }
    return alloc;
}

Value *NFunctionDeclaration::codeGen(CodeGenContext &context)
{
    vector<Type *> argTypes;
    for (auto it = arguments.begin(); it != arguments.end(); it++)
    {
        argTypes.push_back(typeOf((**it).type));
    }
    FunctionType *ftype = FunctionType::get(typeOf(type), makeArrayRef(argTypes), false);
    Function *function = Function::Create(ftype, GlobalValue::InternalLinkage, id.name.c_str(), context.module);
    BasicBlock *bblock = BasicBlock::Create(global_context, "entry", function, 0);

    context.pushBlock(bblock);

    Function::arg_iterator argsValues = function->arg_begin();
    Value *argumentValue;
    for (auto it = arguments.begin(); it != arguments.end(); it++)
    {
        (**it).codeGen(context);
        argumentValue = &*argsValues++;
        argumentValue->setName((*it)->id.name.c_str());
        StoreInst *inst = new StoreInst(argumentValue, context.locals()[(*it)->id.name], false, bblock);
    }

    block.codeGen(context);
    ReturnInst::Create(global_context, context.getCurrentReturnValue(), bblock);

    context.popBlock();
    std::cout << "Creating function: " << id.name << std::endl;
    return function;
}

Value *NReturnStatement::codeGen(CodeGenContext &context)
{
    std::cout << "Generating code for " << typeid(expression).name() << std::endl;
    Value *returnValue = expression.codeGen(context);
    context.setCurrentReturnValue(returnValue);
    return returnValue;
}

Value *NExternDeclaration::codeGen(CodeGenContext &context)
{
    vector<Type *> argTypes;
    for (auto it = arguments.begin(); it != arguments.end(); it++)
    {
        argTypes.push_back(typeOf((**it).type));
    }
    FunctionType *ftype = FunctionType::get(typeOf(type), makeArrayRef(argTypes), false);
    Function *function = Function::Create(ftype, GlobalValue::ExternalLinkage, id.name.c_str(), context.module);
    return function;
}

Value *NEqualityExpression::codeGen(CodeGenContext &context)
{
    CmpInst::OtherOps ops = CmpInst::ICmp;
    CmpInst::Predicate pred;
    switch (op)
    {
        case (TCLT):
            pred = ICmpInst::ICMP_SLT; break;
        case (TCLE):
            pred = ICmpInst::ICMP_SLE; break;
        case (TCGT):
            pred = ICmpInst::ICMP_SGT; break;
        case (TCGE):
            pred = ICmpInst::ICMP_SGE; break;
        case(TCNE):
            pred = ICmpInst::ICMP_NE; break;
        case (TCEQ):
            pred = ICmpInst::ICMP_EQ; break;
        default:
            std::cerr << "failed to match expression comparator" << std::endl;
            break;
    }
    
    return CmpInst::Create(ops, pred, lhs.codeGen(context), rhs.codeGen(context), "", context.currentBlock());
}