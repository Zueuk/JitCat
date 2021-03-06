/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2020
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#pragma once

#include "jitcat/CatASTNode.h"

#include <any>
#include <cassert>
#include <optional>

namespace jitcat::AST
{

	class CatStatement: public CatASTNode
	{
	public:
		CatStatement(const Tokenizer::Lexeme& lexeme): CatASTNode(lexeme) {}
		CatStatement(const CatStatement& other): CatASTNode(other) {}

		virtual ~CatStatement() {};
		virtual bool typeCheck(CatRuntimeContext* compiletimeContext, ExpressionErrorManager* errorManager, void* errorContext) = 0;
		virtual bool isTypedExpression() const { return false;}
		virtual CatStatement* constCollapse(CatRuntimeContext* compiletimeContext, ExpressionErrorManager* errorManager, void* errorContext) = 0;

		virtual std::any execute(jitcat::CatRuntimeContext* runtimeContext) = 0;
		virtual std::optional<bool> checkControlFlow(CatRuntimeContext* compiletimeContext, ExpressionErrorManager* errorManager, void* errorContext, bool& unreachableCodeDetected) {return std::nullopt;}

		bool getAllControlPathsReturn() const {return allControlPathsReturn.has_value() && allControlPathsReturn.value(); }

	protected:
		std::optional<bool> allControlPathsReturn;
	};

};
