/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#include "jitcat/CatArrayIndex.h"
#include "jitcat/CatLog.h"
#include "jitcat/ExpressionErrorManager.h"
#include "jitcat/MemberInfo.h"
#include "jitcat/ASTHelper.h"
#include "jitcat/TypeInfo.h"
#include "jitcat/Tools.h"

using namespace jitcat;
using namespace jitcat::AST;
using namespace jitcat::Tools;


CatArrayIndex::CatArrayIndex(CatTypedExpression* base, CatTypedExpression* arrayIndex):
	array(base),
	index(arrayIndex),
	arrayType(CatGenericType::errorType),
	indexType(CatGenericType::errorType),
	containerItemType(CatGenericType::errorType)
{

}


void CatArrayIndex::print() const
{
	array->print();
	CatLog::log("[");
	index->print();
	CatLog::log("]");
}


CatASTNodeType CatArrayIndex::getNodeType()
{
	return CatASTNodeType::ArrayIndex;
}


std::any CatArrayIndex::execute(CatRuntimeContext* runtimeContext)
{
	std::any arrayValue = array->execute(runtimeContext);
	std::any indexValue = index->execute(runtimeContext);
	if (arrayType.isMapType())
	{
		if (indexType.isIntType())
		{
			return containerItemType.getObjectType()->getTypeCaster()->getItemOfStringIndexedMapOf(arrayValue, std::any_cast<int>(indexValue));
		}
		else if (indexType.isStringType())
		{
			return containerItemType.getObjectType()->getTypeCaster()->getItemOfStringIndexedMapOf(arrayValue, std::any_cast<std::string>(indexValue));
		}
	}
	else if (arrayType.isVectorType())
	{
		return containerItemType.getObjectType()->getTypeCaster()->getItemOfVectorOf(arrayValue, std::any_cast<int>(indexValue));
	}
	return containerItemType.createDefault();
}


bool CatArrayIndex::typeCheck(CatRuntimeContext* compiletimeContext, ExpressionErrorManager* errorManager, void* errorContext)
{
	if (array->typeCheck(compiletimeContext, errorManager, errorContext)
		&& index->typeCheck(compiletimeContext, errorManager, errorContext))
	{
		arrayType = array->getType();
		indexType = index->getType();
		if (!arrayType.isContainerType())
		{
			errorManager->compiledWithError(Tools::append(arrayType.toString(), " is not a list."), errorContext);
			return false;
		}
		containerItemType = arrayType.getContainerItemType();
		if (!containerItemType.isObjectType())
		{
			errorManager->compiledWithError(Tools::append(arrayType.toString(), " not supported."), errorContext);
		}
		else if (arrayType.isIntType() && !indexType.isScalarType())
		{
			errorManager->compiledWithError(Tools::append(arrayType.toString(), " should be indexed by a number."), errorContext);
		}
		else if (arrayType.isMapType() && (!indexType.isScalarType() && !indexType.isStringType()))
		{
			errorManager->compiledWithError(Tools::append(arrayType.toString(), " should be indexed by a string or a number."), errorContext);
		}
		else
		{
			return true;
		}
	}
	return false;
}


CatGenericType CatArrayIndex::getType() const
{
	return containerItemType;
}


bool CatArrayIndex::isConst() const
{
	return false;
}


CatTypedExpression* CatArrayIndex::constCollapse(CatRuntimeContext* compileTimeContext)
{
	ASTHelper::updatePointerIfChanged(array, array->constCollapse(compileTimeContext));
	ASTHelper::updatePointerIfChanged(index, index->constCollapse(compileTimeContext));
	return this;
}


CatTypedExpression* CatArrayIndex::getBase() const
{
	return array.get();
}


CatTypedExpression* CatArrayIndex::getIndex() const
{
	return index.get();
}
