/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#include "jitcat/CatPrefixOperator.h"
#include "jitcat/ASTHelper.h"
#include "jitcat/CatLiteral.h"
#include "jitcat/CatLog.h"
#include "jitcat/ExpressionErrorManager.h"
#include "jitcat/Tools.h"

#include <cassert>

using namespace jitcat;
using namespace jitcat::AST;
using namespace jitcat::Tools;

const char* CatPrefixOperator::conversionTable[] = {"!", "-"};


CatGenericType CatPrefixOperator::getType() const 
{
	return resultType;
}


bool CatPrefixOperator::isConst() const 
{
	return rhs->isConst();
}


CatTypedExpression* CatPrefixOperator::constCollapse(CatRuntimeContext* compileTimeContext)
{
	ASTHelper::updatePointerIfChanged(rhs, rhs->constCollapse(compileTimeContext));
	if (rhs->isConst())
	{
		return new CatLiteral(calculateExpression(compileTimeContext), getType());
	}
	return this;
}


std::any CatPrefixOperator::execute(CatRuntimeContext* runtimeContext)
{
	return calculateExpression(runtimeContext);
}


bool CatPrefixOperator::typeCheck(CatRuntimeContext* compiletimeContext, ExpressionErrorManager* errorManager, void* errorContext)
{
	if (rhs->typeCheck(compiletimeContext, errorManager, errorContext))
	{
		CatGenericType rightType = rhs->getType();
		if (rightType.isBoolType()
			&& oper == Operator::Not)
		{
			resultType = CatGenericType::boolType;
			return true;
		}
		else if (rightType.isFloatType()
					&& oper == Operator::Minus)
		{
			resultType =  CatGenericType::floatType;
			return true;
		}
		else if (rightType.isIntType()
					&& oper == Operator::Minus)
		{
			resultType =  CatGenericType::intType;
			return true;
		}
		else
		{
			errorManager->compiledWithError(Tools::append("Error: invalid operation: ", conversionTable[(unsigned int)oper], rightType.toString()), errorContext);
			return false;
		}
	}
	return false;
}


void CatPrefixOperator::print() const
{
	CatLog::log("(");
	CatLog::log(conversionTable[(unsigned int)oper]);
	rhs->print();
	CatLog::log(")");
}


inline std::any CatPrefixOperator::calculateExpression(CatRuntimeContext* runtimeContext)
{
	std::any rValue = rhs->execute(runtimeContext);
	if (rhs->getType().isBoolType()
		&& oper == Operator::Not)
	{
		return std::any(!std::any_cast<bool>(rValue));
	}
	else if (rhs->getType().isFloatType()
				&& oper == Operator::Minus)
	{
		return std::any(-std::any_cast<float>(rValue));
	}
	else if (rhs->getType().isIntType()
				&& oper == Operator::Minus)
	{
		return std::any(-std::any_cast<int>(rValue));
	}
	assert(false);
	return std::any();
}