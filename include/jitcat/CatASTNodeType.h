/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#pragma once

namespace jitcat::AST
{

	enum class CatASTNodeType
	{
		ArrayIndex,
		AssignmentOperator,
		BuiltInFunctionCall,
		ClassDefinition,
		ForLoop,
		FunctionDefinition,
		FunctionOrConstructorCall,
		FunctionParameterDefinitions,
		Identifier,
		IfStatement,
		IndirectionConversion,
		InfixOperator,
		InheritanceDefinition,
		LinkedList,
		Literal,
		MemberAccess,
		MemberFunctionCall,
		OperatorNew,
		OperatorNewArray,
		ParameterList,
		PrefixOperator,
		Range,
		ReturnStatement,
		ScopeBlock,
		ScopeFunctionCall,
		ScopeRoot,
		SourceFile,
		StaticFunctionCall,
		StaticIdentifier,
		StaticScope,
		TypeName,
		TypeOrIdentifier,
		VariableDeclaration,
		VariableDefinition,
		OwnershipSemantics,
		ErrorExpression,
	};

} //End namespace jitcat::AST