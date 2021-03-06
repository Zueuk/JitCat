/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#include "jitcat/ExpressionAny.h"
#include "jitcat/CatASTNodes.h"
#include "jitcat/Configuration.h"
#include "jitcat/Document.h"
#include "jitcat/ExpressionErrorManager.h"
#include "jitcat/JitCat.h"
#include "jitcat/SLRParseResult.h"
#include "jitcat/Tools.h"
#include "jitcat/TypeInfo.h"

using namespace jitcat;
using namespace jitcat::AST;


ExpressionAny::ExpressionAny():
	nativeFunctionAddress(0)
{
}


ExpressionAny::ExpressionAny(const char* expression):
	ExpressionBase(expression),
	nativeFunctionAddress(0)
{
}


ExpressionAny::ExpressionAny(const std::string& expression):
	ExpressionBase(expression),
	nativeFunctionAddress(0)
{
}


ExpressionAny::ExpressionAny(CatRuntimeContext* compileContext, const std::string& expression):
	ExpressionBase(compileContext, expression),
	nativeFunctionAddress(0)
{
	compile(compileContext);
}


const std::any ExpressionAny::getValue(CatRuntimeContext* runtimeContext)
{
	if (isConstant)
	{
		return cachedValue;
	}
	else if (parseResult->astRootNode != nullptr)
	{
		if (runtimeContext == nullptr)
		{
			runtimeContext = &CatRuntimeContext::defaultContext;
		}
		if constexpr (Configuration::enableLLVM)
		{
			if		(valueType.isIntType())		return std::any(reinterpret_cast<int(*)(CatRuntimeContext*)>(nativeFunctionAddress)(runtimeContext));
			else if (valueType.isVoidType())	{reinterpret_cast<void(*)(CatRuntimeContext*)>(nativeFunctionAddress)(runtimeContext); return std::any();}
			else if (valueType.isFloatType())	return std::any(reinterpret_cast<float(*)(CatRuntimeContext*)>(nativeFunctionAddress)(runtimeContext));
			else if (valueType.isDoubleType())	return std::any(reinterpret_cast<double(*)(CatRuntimeContext*)>(nativeFunctionAddress)(runtimeContext));
			else if (valueType.isBoolType())	return std::any(reinterpret_cast<bool(*)(CatRuntimeContext*)>(nativeFunctionAddress)(runtimeContext));
			else if (valueType.isPointerToReflectableObjectType())	return valueType.getPointeeType()->getObjectType()->getTypeCaster()->castFromRawPointer(reinterpret_cast<uintptr_t(*)(CatRuntimeContext*)>(nativeFunctionAddress)(runtimeContext));
			else 
			{
				return std::any();
			}
		}
		else
		{
			std::any result = parseResult->getNode<CatTypedExpression>()->execute(runtimeContext);
			runtimeContext->clearTemporaries();
			return result;
		}
	}
	else
	{
		return std::any();
	}
}


const std::any jitcat::ExpressionAny::getInterpretedValue(CatRuntimeContext* runtimeContext)
{
	if (isConstant)
	{
		return cachedValue;
	}
	else if (parseResult->astRootNode != nullptr)
	{
		std::any result = parseResult->getNode<CatTypedExpression>()->execute(runtimeContext);
		runtimeContext->clearTemporaries();
		return result;
	}
	else
	{
		return std::any();
	}
}


void ExpressionAny::compile(CatRuntimeContext* context)
{
	if (context == nullptr)
	{
		context = &CatRuntimeContext::defaultContext;
		context->getErrorManager()->clear();
	}
	if (parse(context, context->getErrorManager(), this, CatGenericType()) && isConstant)
	{
		cachedValue = parseResult->getNode<CatTypedExpression>()->execute(context);
	}
}


void ExpressionAny::handleCompiledFunction(uintptr_t functionAddress)
{
	nativeFunctionAddress = functionAddress;
}
