/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#pragma once

namespace jitcat
{
	class CatRuntimeContext;
}
#include "jitcat/LLVMCompileOptions.h"
#include "jitcat/LLVMForwardDeclares.h"

#include <functional>
#include <vector>

namespace jitcat::LLVM
{
	class LLVMCodeGeneratorHelper;


	struct LLVMCompileTimeContext
	{
		LLVMCompileTimeContext(CatRuntimeContext* catContext);

		CatRuntimeContext* catContext;
		llvm::Function* currentFunction;
		llvm::orc::JITDylib* currentDyLib;

		LLVMCodeGeneratorHelper* helper;
		std::vector<std::function<llvm::Value*()>> blockDestructorGenerators;
	
		LLVMCompileOptions options;
	};

} //End namespace jitcat::LLVM