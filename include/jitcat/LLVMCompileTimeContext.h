#pragma once

class CatRuntimeContext;

#include <functional>
#include "LLVMCompileOptions.h"
#include "LLVMForwardDeclares.h"


struct LLVMCompileTimeContext
{
	LLVMCompileTimeContext(CatRuntimeContext* catContext);

	CatRuntimeContext* catContext;
	llvm::Function* currentFunction;

	std::vector<std::function<llvm::Value*()>> blockDestructorGenerators;

	LLVMCompileOptions options;
};