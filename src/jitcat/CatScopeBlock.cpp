/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2019
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#include "jitcat/CatScopeBlock.h"
#include "jitcat/CatLog.h"
#include "jitcat/CatRuntimeContext.h"
#include "jitcat/CatTypeNode.h"
#include "jitcat/CatVariableDeclaration.h"
#include "jitcat/CustomTypeInfo.h"
#include "jitcat/CustomTypeInstance.h"
#include "jitcat/ExpressionErrorManager.h"

using namespace jitcat;
using namespace jitcat::AST;
using namespace jitcat::Reflection;


CatScopeBlock::CatScopeBlock(const std::vector<CatStatement*>& statementList, const Tokenizer::Lexeme& lexeme):
	CatStatement(lexeme),
	customType(new CustomTypeInfo(nullptr)),
	scopeId(InvalidScopeID)
{
	for (auto& iter : statementList)
	{
		statements.emplace_back(iter);
	}
}


CatScopeBlock::~CatScopeBlock()
{
}


void CatScopeBlock::print() const
{
	Tools::CatLog::log("{\n");
	for (auto& iter : statements)
	{
		iter->print();
		Tools::CatLog::log("\n");
	}
	Tools::CatLog::log("}\n");
}


CatASTNodeType CatScopeBlock::getNodeType()
{
	return CatASTNodeType::ScopeBlock;
}


bool jitcat::AST::CatScopeBlock::typeCheck(CatRuntimeContext* compiletimeContext, ExpressionErrorManager* errorManager, void* errorContext)
{
	CatScopeID myScopeId = compiletimeContext->addCustomTypeScope(customType.get());
	CatScopeBlock* previousScope = compiletimeContext->getCurrentScope();
	compiletimeContext->setCurrentScope(this);
	bool noErrors = true;
	for (auto& iter : statements)
	{
		noErrors &= iter->typeCheck(compiletimeContext, errorManager, errorContext);
	}
	compiletimeContext->removeScope(myScopeId);
	compiletimeContext->setCurrentScope(previousScope);
	return noErrors;
}


std::any jitcat::AST::CatScopeBlock::execute(CatRuntimeContext* runtimeContext)
{
	std::unique_ptr<CustomTypeInstance> scopeInstance;
	scopeInstance.reset(customType->createInstance());
	scopeId = runtimeContext->addCustomTypeScope(customType.get(), scopeInstance.get());
	CatScopeBlock* previousScope = runtimeContext->getCurrentScope();
	runtimeContext->setCurrentScope(this);
	std::any result = std::any();
	for (auto& iter : statements)
	{
		if (iter->getNodeType() == CatASTNodeType::ReturnStatement)
		{
			runtimeContext->setReturning(true);
		}
		result = iter->execute(runtimeContext);
		if (runtimeContext->getIsReturning())
		{
			break;
		}
	}
	runtimeContext->removeScope(scopeId);
	runtimeContext->setCurrentScope(previousScope);
	return result;
}


std::optional<bool> jitcat::AST::CatScopeBlock::checkControlFlow(CatRuntimeContext* compiletimeContext, ExpressionErrorManager* errorManager, void* errorContext, bool& unreachableCodeDetected) const
{
	bool controlFlowReturns = false;
	for (auto& iter : statements)
	{
		auto returns = iter->checkControlFlow(compiletimeContext, errorManager, errorContext, unreachableCodeDetected);
		if (!controlFlowReturns && returns.has_value() && (*returns))
		{
			controlFlowReturns = true;		
		}
		else if (controlFlowReturns)
		{
			unreachableCodeDetected = true;
			errorManager->compiledWithError("Code is unreachable.", errorContext, compiletimeContext->getContextName(), iter->getLexeme());
			return true;
		}
	}
	return controlFlowReturns;
}


bool jitcat::AST::CatScopeBlock::containsReturnStatement() const
{
	for (auto& iter : statements)
	{
		if (iter->getNodeType() == CatASTNodeType::ReturnStatement)
		{
			return true;
		}
	}
	return false;
}


Reflection::CustomTypeInfo* jitcat::AST::CatScopeBlock::getCustomType()
{
	return customType.get();
}


CatScopeID jitcat::AST::CatScopeBlock::getScopeId() const
{
	return scopeId;
}