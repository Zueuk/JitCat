/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#pragma once

#include "jitcat/CatInfixOperatorType.h"
#include "jitcat/Lexeme.h"

#include <memory>

namespace jitcat
{
	class CatRuntimeContext;
	class ExpressionErrorManager;
}

namespace jitcat::AST 
{
	class CatTypedExpression;


	class InfixOperatorOptimizer
	{
	private:
		InfixOperatorOptimizer();
	public:
		static CatTypedExpression* tryCollapseInfixOperator(std::unique_ptr<CatTypedExpression>& lhs, 
															std::unique_ptr<CatTypedExpression>& rhs, 
															CatInfixOperatorType infixOperator,
															jitcat::CatRuntimeContext* compileTimeContext, 
															jitcat::ExpressionErrorManager* errorManager, 
															void* errorContext);
		static Tokenizer::Lexeme combineLexemes(std::unique_ptr<CatTypedExpression>& lhs, std::unique_ptr<CatTypedExpression>& rhs);

	private:
		static CatTypedExpression* tryCollapseMultiplication(std::unique_ptr<CatTypedExpression>& lhs, std::unique_ptr<CatTypedExpression>& rhs);
		static CatTypedExpression* tryCollapseAddition(std::unique_ptr<CatTypedExpression>& lhs, std::unique_ptr<CatTypedExpression>& rhs);
		static CatTypedExpression* tryCollapseSubtraction(std::unique_ptr<CatTypedExpression>& lhs, std::unique_ptr<CatTypedExpression>& rhs);
		static CatTypedExpression* tryCollapseDivision(std::unique_ptr<CatTypedExpression>& lhs, std::unique_ptr<CatTypedExpression>& rhs);
		static CatTypedExpression* tryCollapseLogicalAnd(std::unique_ptr<CatTypedExpression>& lhs, std::unique_ptr<CatTypedExpression>& rhs);
		static CatTypedExpression* tryCollapseLogicalOr(std::unique_ptr<CatTypedExpression>& lhs, std::unique_ptr<CatTypedExpression>& rhs);

		static bool typedExpressionEqualsConstant(CatTypedExpression* expression, double constant);
		static bool typedExpressionEqualsConstant(CatTypedExpression* expression, bool constant);
	};

} //End namespace jitcat::AST 