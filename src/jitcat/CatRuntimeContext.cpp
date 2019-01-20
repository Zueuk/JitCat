/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#include "CatRuntimeContext.h"
#include "CustomTypeInfo.h"
#include "CustomTypeInstance.h"
#include "ErrorContext.h"
#include "ExpressionErrorManager.h"
#ifdef ENABLE_LLVM
#include "LLVMCodeGenerator.h"
#endif

#include <cassert>
#include <sstream>


CatRuntimeContext::CatRuntimeContext(const std::string& contextName, ExpressionErrorManager* errorManager):
	contextName(contextName),
	errorManager(errorManager),
	ownsErrorManager(false),
#ifdef ENABLE_LLVM
	codeGenerator(new LLVMCodeGenerator(contextName)),
#endif
	nextFunctionIndex(0)
{
	if (errorManager == nullptr)
	{
		ownsErrorManager = true;
		errorManager = new ExpressionErrorManager();
	}
}


CatRuntimeContext::~CatRuntimeContext()
{
	if (ownsErrorManager)
	{
		delete errorManager;
	}
}


std::string CatRuntimeContext::getContextName()
{
	std::stringstream stream;
	for (ErrorContext* errorContext : errorContextStack)
	{
		stream << errorContext->getContextDescription() << " ";
	}
	if (errorContextStack.size() != 0)
	{
		return contextName + " " + stream.str();
	}
	else
	{
		return contextName;
	}
}


CatScopeID CatRuntimeContext::addScope(TypeInfo* typeInfo, Reflectable* scopeObject, bool isStatic)
{
	return createScope(scopeObject, typeInfo, isStatic);
}


CatScopeID CatRuntimeContext::addCustomTypeScope(CustomTypeInfo* typeInfo, CustomTypeInstance* scopeObject, bool isStatic)
{
	assert(typeInfo != nullptr);
	//If this is a static scope, scopeObject must not be nullptr.
	assert(!isStatic || scopeObject != nullptr);
	if (scopeObject != nullptr)
	{
		//The provided scopeObject must be of the same type as the typeInfo.
		assert(scopeObject->typeInfo == typeInfo);
	}
	return createScope(scopeObject, typeInfo, isStatic);	
}


int CatRuntimeContext::getNumScopes() const
{
	return (int)scopes.size();
}


void CatRuntimeContext::removeScope(CatScopeID id)
{
	assert(id >= 0 && id < scopes.size());
	scopes[id].reset(nullptr);
}


void CatRuntimeContext::setScopeObject(CatScopeID id, Reflectable* scopeObject)
{
	assert(id >= 0 && id < scopes.size());
	Scope* scope = scopes[id].get();
	if (scope->scopeType->isCustomType())
	{
		assert(static_cast<CustomTypeInstance*>(scopeObject)->typeInfo == scope->scopeType);
	}
	scope->scopeObject = scopeObject;
}


bool CatRuntimeContext::isStaticScope(CatScopeID id) const
{
	assert(id >= 0 && id < scopes.size());
	return scopes[id]->isStatic;
}


Reflectable* CatRuntimeContext::getScopeObject(CatScopeID id) const
{
	assert(id >= 0 && id < scopes.size());
	return scopes[id]->scopeObject.get();
}


TypeInfo* CatRuntimeContext::getScopeType(CatScopeID id) const
{
	assert(id >= 0 && id < scopes.size());
	return scopes[id]->scopeType;
}


ExpressionErrorManager* CatRuntimeContext::getErrorManager() const
{
	return errorManager;
}


void CatRuntimeContext::pushErrorContext(ErrorContext* context)
{
	errorContextStack.push_back(context);
}


void CatRuntimeContext::popErrorContext(ErrorContext* context)
{
	if (errorContextStack.back() == context)
	{
		errorContextStack.pop_back();
	}
	else
	{
		//This means that contexts were pushed in a different order than they were popped!
		assert(false);
	}
}


TypeMemberInfo* CatRuntimeContext::findVariable(const std::string& lowercaseName, CatScopeID& scopeId)
{
	for (int i = (int)scopes.size() - 1; i >= 0; i--)
	{
		TypeMemberInfo* memberInfo = scopes[i]->scopeType->getMemberInfo(lowercaseName);
		if (memberInfo != nullptr)
		{
			scopeId = i;
			return memberInfo;
		}
	}
	return nullptr;
}


MemberFunctionInfo* CatRuntimeContext::findFunction(const std::string& lowercaseName, CatScopeID& scopeId)
{
	for (int i = (int)scopes.size() - 1; i >= 0; i--)
	{
		MemberFunctionInfo* memberFunctionInfo = scopes[i]->scopeType->getMemberFunctionInfo(lowercaseName);
		if (memberFunctionInfo != nullptr)
		{
			scopeId = i;
			return memberFunctionInfo;
		}
	}
	return nullptr;
}


LLVMCodeGenerator* CatRuntimeContext::getCodeGenerator() const
{
#ifdef ENABLE_LLVM
	return codeGenerator.get();
#else
	return nullptr;
#endif
}


int CatRuntimeContext::getNextFunctionIndex()
{
	return nextFunctionIndex++;
}


CatScopeID CatRuntimeContext::createScope(Reflectable* scopeObject, TypeInfo* type, bool isStatic)
{
	Scope* scope = new Scope();
	scope->isStatic = isStatic;
	scope->scopeObject = scopeObject;
	scope->scopeType = type;
	scopes.emplace_back(scope);
	return static_cast<CatScopeID>((int)scopes.size() - 1);
}
