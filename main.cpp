#include <iostream>
#include "node.h"
#include "codegen.h"

extern NBlock *programBlock;
extern int yyparse();

void createCoreFunctions(CodeGenContext& context);

int main(int argc, char **argv)
{
    yyparse();
    std::cout << programBlock << std::endl;
    InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	InitializeNativeTargetAsmParser();
    CodeGenContext context;
    createCoreFunctions(context);
    context.generateCode(*programBlock);
    context.runCode();

    return 0;
}