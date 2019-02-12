/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#include "jitcat/CatGrammar.h"
#include "jitcat/ASTNodeParser.h"
#include "jitcat/CatASTNodes.h"
#include "jitcat/CatScopeRoot.h"
#include "jitcat/CatTokenIds.h"
#include "jitcat/ConstantToken.h"
#include "jitcat/Lexeme.h"
#include "jitcat/OneCharToken.h"
#include "jitcat/Tools.h"

#include <stdlib.h>
#include <type_traits>

using namespace jitcat;
using namespace jitcat::AST;
using namespace jitcat::Cat;
using namespace jitcat::Grammar;
using namespace jitcat::Parser;
using namespace jitcat::Reflection;
using namespace jitcat::Tokenizer;


CatGrammar::CatGrammar(TokenizerBase* tokenizer, CatGrammarType grammarType):
	GrammarBase(tokenizer)
{
	Prod rootProduction;
	switch (grammarType)
	{
		case CatGrammarType::Expression: rootProduction = Prod::Expression; break;
		case CatGrammarType::Full:		 rootProduction = Prod::SourceFile; break;
	}

	rule(Prod::Root, {prod(rootProduction)}, pass);

	if (grammarType == CatGrammarType::Full)
	{
		//A source file has one or more definitions
		rule(Prod::SourceFile, {prod(Prod::Definitions)}, sourceFile);

		//A list of definitions
		rule(Prod::Definitions, {prod(Prod::Definition), prod(Prod::Definitions)}, link);
		rule(Prod::Definitions, {prod(Prod::Definition)}, pass);

		//Possible definitions
		rule(Prod::Definition, {prod(Prod::ClassDefinition)}, pass);
		rule(Prod::Definition, {prod(Prod::FunctionDefinition)}, pass);
		
		//Class definition
		rule(Prod::ClassDefinition, {term(id, Identifier::Class), term(id, Identifier::Identifier), term(one, OneChar::BraceOpen)/*, prod(Prod::ClassContents)*/, term(one, OneChar::BraceClose)}, classDefinition);

		//Variable declaration
		rule(Prod::VariableDeclaration, {prod(Prod::Type), term(id, Identifier::Identifier)}, variableDeclaration);
		rule(Prod::VariableDeclaration, {prod(Prod::Type), term(id, Identifier::Identifier), term(one, OneChar::Assignment), prod(Prod::Expression)}, variableDeclaration);

		//Function definition
		rule(Prod::FunctionDefinition, {prod(Prod::Type), term(id, Identifier::Identifier), prod(Prod::FunctionParameters), prod(Prod::ScopeBlock)}, functionDefinition);
		rule(Prod::FunctionParameters, {term(one, OneChar::ParenthesesOpen), term(one, OneChar::ParenthesesClose)}, functionParameterDefinitions);
		rule(Prod::FunctionParameters, {term(one, OneChar::ParenthesesOpen), prod(Prod::FunctionParameterDefinitions), term(one, OneChar::ParenthesesClose)}, functionParameterDefinitions);
		rule(Prod::FunctionParameterDefinitions, {prod(Prod::VariableDeclaration), term(one, OneChar::Comma), prod(Prod::FunctionParameterDefinitions)}, link);
		rule(Prod::FunctionParameterDefinitions, {prod(Prod::VariableDeclaration)}, pass);
		
		//Scope block
		rule(Prod::ScopeBlock, {term(one, OneChar::BraceOpen), prod(Prod::ScopeBlockStatements), term(one, OneChar::BraceClose)}, scopeBlock);
		rule(Prod::ScopeBlock, {term(one, OneChar::BraceOpen), term(one, OneChar::BraceClose)}, scopeBlock);
		rule(Prod::ScopeBlockStatements, {prod(Prod::Statement), prod(Prod::ScopeBlockStatements)}, link);
		rule(Prod::ScopeBlockStatements, {prod(Prod::Statement)}, pass);

		//All possible statements
		rule(Prod::Statement, {prod(Prod::Return), term(one, OneChar::Semicolon)}, pass);
		rule(Prod::Statement, {prod(Prod::Expression), term(one, OneChar::Semicolon)}, pass);
		rule(Prod::Statement, {prod(Prod::VariableDeclaration), term(one, OneChar::Semicolon)}, pass);
		rule(Prod::Statement, {prod(Prod::IfThen)}, pass);

		//If statement
		rule(Prod::IfThen, {term(id, Identifier::If), term(one, OneChar::ParenthesesOpen), prod(Prod::Expression), term(one, OneChar::ParenthesesClose), prod(Prod::ScopeBlock)}, ifStatement);
		rule(Prod::IfThen, {term(id, Identifier::If), term(one, OneChar::ParenthesesOpen), prod(Prod::Expression), term(one, OneChar::ParenthesesClose), prod(Prod::ScopeBlock), prod(Prod::Else)}, ifStatement);
		rule(Prod::Else, {term(id, Identifier::Else), prod(Prod::IfThen)}, pass);
		rule(Prod::Else, {term(id, Identifier::Else), prod(Prod::ScopeBlock)}, pass);

		//Return statement
		rule(Prod::Return, {term(id, Identifier::Return)}, returnStatement);
		rule(Prod::Return, {term(id, Identifier::Return), prod(Prod::Expression)}, returnStatement);

		//Typename
		rule(Prod::Type, {term(id, Identifier::Identifier)}, typeName);
		rule(Prod::Type, {term(id, Identifier::Void)}, typeName);
		rule(Prod::Type, {term(id, Identifier::Bool)}, typeName);
		rule(Prod::Type, {term(id, Identifier::Int)}, typeName);
		rule(Prod::Type, {term(id, Identifier::Float)}, typeName);
		rule(Prod::Type, {term(id, Identifier::String)}, typeName);
	}

	//Expressions
	rule(Prod::Expression, {prod(Prod::OperatorP11)}, pass);
	//Operators
	//Precedence from low precedence to high precedence (based on C++ operator precedence)
	//See http://en.cppreference.com/w/cpp/language/operator_precedence

	//= (lowest precedence)
	rule(Prod::OperatorP11, {prod(Prod::OperatorP11), term(one, OneChar::Assignment), prod(Prod::OperatorP10)}, assignmentOperator);
	rule(Prod::OperatorP11, {prod(Prod::OperatorP10)}, pass);

	//|| 
	rule(Prod::OperatorP10, {prod(Prod::OperatorP10), term(two, TwoChar::LogicalOr), prod(Prod::OperatorP9)}, infixOperator);
	rule(Prod::OperatorP10, {prod(Prod::OperatorP9)}, pass);

	//&&
	rule(Prod::OperatorP9, {prod(Prod::OperatorP9), term(two, TwoChar::LogicalAnd), prod(Prod::OperatorP8)}, infixOperator);
	rule(Prod::OperatorP9, {prod(Prod::OperatorP8)}, pass);

	//== != operators
	rule(Prod::OperatorP8, {prod(Prod::OperatorP8), term(two, TwoChar::Equals), prod(Prod::OperatorP7)}, infixOperator);
	rule(Prod::OperatorP8, {prod(Prod::OperatorP8), term(two, TwoChar::NotEquals), prod(Prod::OperatorP7)}, infixOperator);
	rule(Prod::OperatorP8, {prod(Prod::OperatorP7)}, pass);

	//< <= > >= operators
	rule(Prod::OperatorP7, {prod(Prod::OperatorP7), term(one, OneChar::Greater), prod(Prod::OperatorP6)}, infixOperator);
	rule(Prod::OperatorP7, {prod(Prod::OperatorP7), term(one, OneChar::Smaller), prod(Prod::OperatorP6)}, infixOperator);
	rule(Prod::OperatorP7, {prod(Prod::OperatorP7), term(two, TwoChar::GreaterOrEqual), prod(Prod::OperatorP6)}, infixOperator);
	rule(Prod::OperatorP7, {prod(Prod::OperatorP7), term(two, TwoChar::SmallerOrEqual), prod(Prod::OperatorP6)}, infixOperator);
	rule(Prod::OperatorP7, {prod(Prod::OperatorP6)}, pass);

	//infix + - operators
	rule(Prod::OperatorP6, {prod(Prod::OperatorP6), term(one, OneChar::Plus), prod(Prod::OperatorP5)}, infixOperator);
	rule(Prod::OperatorP6, {prod(Prod::OperatorP6), term(one, OneChar::Minus), prod(Prod::OperatorP5)}, infixOperator);
	rule(Prod::OperatorP6, {prod(Prod::OperatorP5)}, pass);

	//* / % operators
	rule(Prod::OperatorP5, {prod(Prod::OperatorP5), term(one, OneChar::Times), prod(Prod::OperatorP4)}, infixOperator);
	rule(Prod::OperatorP5, {prod(Prod::OperatorP5), term(one, OneChar::Divide), prod(Prod::OperatorP4)}, infixOperator);
	rule(Prod::OperatorP5, {prod(Prod::OperatorP5), term(one, OneChar::Modulo), prod(Prod::OperatorP4)}, infixOperator);
	rule(Prod::OperatorP5, {prod(Prod::OperatorP4)}, pass);
	
	// ! and prefix - operators
	rule(Prod::OperatorP4, {term(one, OneChar::Not), prod(Prod::OperatorP3)}, prefixOperator);
	rule(Prod::OperatorP4, {term(one, OneChar::Minus), prod(Prod::OperatorP3)}, prefixOperator);
	rule(Prod::OperatorP4, {prod(Prod::OperatorP3)}, pass);

	// . operator (bla.bla)
	rule(Prod::OperatorP3, {prod(Prod::OperatorP3), term(one, OneChar::Dot), term(id, Identifier::Identifier)}, memberAccessToken);
	rule(Prod::OperatorP3, {prod(Prod::OperatorP3), term(one, OneChar::Dot), term(id, Identifier::Identifier), prod(Prod::FunctionCallArguments)}, memberFunctionCallToken);
	rule(Prod::OperatorP3, {prod(Prod::OperatorP3), term(one, OneChar::BracketOpen), prod(Prod::OperatorP10), term(one, OneChar::BracketClose)}, arrayIndexToken);
	rule(Prod::OperatorP3, {prod(Prod::OperatorP2)}, pass);

	// literals, identifiers and ( ) (highest precedence)
	rule(Prod::OperatorP2, {prod(Prod::Literal)}, pass);	
	rule(Prod::OperatorP2, {term(one, OneChar::ParenthesesOpen), prod(Prod::OperatorP11), term(one, OneChar::ParenthesesClose)}, pass);
	rule(Prod::OperatorP2, {term(id, Identifier::Identifier)}, identifierToken);
	rule(Prod::OperatorP2, {term(id, Identifier::Identifier), prod(Prod::FunctionCallArguments)}, functionCallToken);

	rule(Prod::FunctionCallArguments, {term(one, OneChar::ParenthesesOpen), term(one, OneChar::ParenthesesClose)}, argumentListToken);
	rule(Prod::FunctionCallArguments, {term(one, OneChar::ParenthesesOpen), prod(Prod::FunctionCallArgumentRepeat), term(one, OneChar::ParenthesesClose)}, argumentListToken);
	rule(Prod::FunctionCallArgumentRepeat, {prod(Prod::Expression), term(one, OneChar::Comma), prod(Prod::FunctionCallArgumentRepeat)}, link);
	rule(Prod::FunctionCallArgumentRepeat, {prod(Prod::Expression)}, pass);

	//Literals
	rule(Prod::Literal, {term(lit, ConstantType::Integer)}, literalToken);
	rule(Prod::Literal, {term(lit, ConstantType::FloatingPoint)}, literalToken);
	rule(Prod::Literal, {term(lit, ConstantType::String)}, literalToken);
	rule(Prod::Literal, {term(lit, ConstantType::Bool)}, literalToken);
	
	setRootProduction(Prod::Root, term(one, OneChar::Eof));

	build();
}


const char* CatGrammar::getProductionName(int production) const
{
	switch((Prod)production)
	{
		default:								return "unknown";
		case Prod::Root:						return "root";
		case Prod::SourceFile:					return "source file";
		case Prod::Definitions:					return "definitions";
		case Prod::Definition:					return "definition";
		case Prod::ClassDefinition:				return "class definition";
		case Prod::ClassContents:				return "class contents";
		case Prod::Declaration:					return "declaration";
		case Prod::FunctionDefinition:			return "function definition";
		case Prod::FunctionParameters:			return "function parameters";
		case Prod::FunctionParameterDefinitions:return "function parameter definitions";
		case Prod::VariableDeclaration:			return "variable declaration";
		case Prod::OperatorP2:					return "P2";
		case Prod::OperatorP3:					return "P3";
		case Prod::OperatorP4:					return "P4";
		case Prod::OperatorP5:					return "P5";
		case Prod::OperatorP6:					return "P6";
		case Prod::OperatorP7:					return "P7";
		case Prod::OperatorP8:					return "P8";
		case Prod::OperatorP9:					return "P9";
		case Prod::OperatorP10:					return "P10";
		case Prod::OperatorP11:					return "P11";
		case Prod::Expression:					return "expression";
		case Prod::ExpressionBlock:				return "expression block";
		case Prod::ExpressionBlockContents:		return "expression block contents";
		case Prod::IfThen:						return "if then";
		case Prod::Else:						return "else ";
		case Prod::ElseBody:					return "else body";
		case Prod::Type:						return "type";
		case Prod::FunctionCall:				return "function call";
		case Prod::FunctionCallArguments:		return "function call arguments";
		case Prod::FunctionCallArgumentRepeat:	return "function call arguments repeat";
		case Prod::Literal:						return "literal";
		case Prod::Assignment:					return "assignment";
		case Prod::ObjectMemberAccess:			return "object member access";
		case Prod::ObjectMemberAccessAction:	return "object member access action";
		case Prod::Return:						return "return";
	}
}


bool CatGrammar::isTypedExpression(CatASTNodeType node)
{
	return	node == CatASTNodeType::Literal
			|| node == CatASTNodeType::Identifier
			|| node == CatASTNodeType::InfixOperator
			|| node == CatASTNodeType::PrefixOperator
			|| node == CatASTNodeType::FunctionCall
			|| node == CatASTNodeType::MemberAccess
			|| node == CatASTNodeType::ArrayIndex
			|| node == CatASTNodeType::MemberFunctionCall;
}


bool jitcat::Grammar::CatGrammar::isDefinition(AST::CatASTNodeType node)
{
	return	  node == CatASTNodeType::ClassDefinition
		   || node == CatASTNodeType::FunctionDefinition;
}


template<typename ItemT>
void unLink(ASTNode* astNode, std::vector<ItemT*>& list)
{
	CatASTNode* catASTNode = static_cast<CatASTNode*>(astNode);
	while (catASTNode != nullptr)
	{
		if (catASTNode->getNodeType() == CatASTNodeType::LinkedList)
		{
			CatLinkNode* linkNode = static_cast<CatLinkNode*>(catASTNode);
			list.push_back(static_cast<ItemT*>(linkNode->me.release()));
			catASTNode = linkNode->next.release();
			delete linkNode;
		}
		else 
		{
			list.push_back(static_cast<ItemT*>(catASTNode));
			break;
		}
	} 
}


template<typename ItemT>
void unLink(ASTNode* astNode, std::vector<std::unique_ptr<ItemT>>& list)
{
	CatASTNode* catASTNode = static_cast<CatASTNode*>(astNode);
	while (catASTNode != nullptr)
	{
		if (catASTNode->getNodeType() == CatASTNodeType::LinkedList)
		{
			CatLinkNode* linkNode = static_cast<CatLinkNode*>(catASTNode);
			list.emplace_back(static_cast<ItemT*>(linkNode->me.release()));
			catASTNode = linkNode->next.release();
			delete linkNode;
		}
		else 
		{
			list.emplace_back(static_cast<ItemT*>(catASTNode));
			break;
		}
	} 
}


ASTNode* CatGrammar::pass(const ASTNodeParser& nodeParser)
{
	return nodeParser.getASTNodeByIndex(0);
}


ASTNode* CatGrammar::link(const ASTNodeParser& nodeParser)
{
	return new CatLinkNode(static_cast<CatASTNode*>(nodeParser.getASTNodeByIndex(0)),
						   static_cast<CatASTNode*>(nodeParser.getASTNodeByIndex(1)));
}


ASTNode* jitcat::Grammar::CatGrammar::sourceFile(const Parser::ASTNodeParser& nodeParser)
{
	CatASTNode* astNode = static_cast<CatASTNode*>(nodeParser.getASTNodeByIndex(0));
	std::vector<std::unique_ptr<CatDefinition>> definitions;
	unLink(nodeParser.getASTNodeByIndex(0), definitions);
	return new CatSourceFile("none", std::move(definitions));
}


ASTNode* jitcat::Grammar::CatGrammar::classDefinition(const Parser::ASTNodeParser& nodeParser)
{
	return new CatClassDefinition(nodeParser.getTerminalByIndex(1)->getLexeme()->toString());
}


ASTNode* jitcat::Grammar::CatGrammar::functionDefinition(const Parser::ASTNodeParser& nodeParser)
{
	return new CatFunctionDefinition(static_cast<CatTypeNode*>(nodeParser.getASTNodeByIndex(0)), 
									 nodeParser.getTerminalByIndex(0)->getLexeme()->toString(),
									 static_cast<CatFunctionParameterDefinitions*>(nodeParser.getASTNodeByIndex(1)),
									 static_cast<CatScopeBlock*>(nodeParser.getASTNodeByIndex(2)));
}


AST::ASTNode* jitcat::Grammar::CatGrammar::functionParameterDefinitions(const Parser::ASTNodeParser& nodeParser)
{
	std::vector<CatVariableDeclaration*> parameterDefinitions;
	unLink(nodeParser.getASTNodeByIndex(0), parameterDefinitions);
	return new CatFunctionParameterDefinitions(parameterDefinitions);
}


AST::ASTNode* jitcat::Grammar::CatGrammar::variableDeclaration(const Parser::ASTNodeParser& nodeParser)
{
	 CatTypeNode* type = static_cast<CatTypeNode*>(nodeParser.getASTNodeByIndex(0));
	 std::string name  = nodeParser.getTerminalByIndex(0)->getLexeme()->toString();
	 CatTypedExpression* initExpression = nullptr;
	 if (nodeParser.getNumItems() > 2)
	 {
		 //declaration has initialization
		 initExpression = static_cast<CatTypedExpression*>(nodeParser.getASTNodeByIndex(1));
	 }
	 return new CatVariableDeclaration(type, name, initExpression);
}


ASTNode* jitcat::Grammar::CatGrammar::typeName(const Parser::ASTNodeParser& nodeParser)
{
	Identifier identifierType = static_cast<Identifier>(nodeParser.getTerminalByIndex(0)->getTokenSubType());
	CatGenericType type;
	switch (identifierType)
	{
		case Identifier::Bool:		 type = CatGenericType::boolType; break;
		case Identifier::Int:		 type = CatGenericType::intType; break;
		case Identifier::Float:		 type = CatGenericType::floatType; break;
		case Identifier::String:	 type = CatGenericType::stringType; break;
		case Identifier::Identifier: return new CatTypeNode(nodeParser.getTerminalByIndex(0)->getLexeme()->toString());
	}
	return new CatTypeNode(type);
}


AST::ASTNode* jitcat::Grammar::CatGrammar::ifStatement(const Parser::ASTNodeParser& nodeParser)
{
	CatTypedExpression* condition = static_cast<CatTypedExpression*>(nodeParser.getASTNodeByIndex(0));
	CatScopeBlock* ifBody = static_cast<CatScopeBlock*>(nodeParser.getASTNodeByIndex(1));
	CatASTNode* elseNode = static_cast<CatASTNode*>(nodeParser.getASTNodeByIndex(2));
	return new CatIfStatement(condition, ifBody, elseNode);
}


AST::ASTNode* jitcat::Grammar::CatGrammar::returnStatement(const Parser::ASTNodeParser& nodeParser)
{
	CatTypedExpression* returnExpression = nullptr;
	if (nodeParser.getNumItems() > 0)
	{
		returnExpression = static_cast<CatTypedExpression*>(nodeParser.getASTNodeByIndex(0));
	}
	return new CatReturnStatement(returnExpression);;
}


AST::ASTNode* jitcat::Grammar::CatGrammar::scopeBlock(const Parser::ASTNodeParser& nodeParser)
{
	std::vector<CatStatement*> statements;
	unLink(nodeParser.getASTNodeByIndex(0), statements);
	return new CatScopeBlock(statements);
}


ASTNode* CatGrammar::assignmentOperator(const ASTNodeParser & nodeParser)
{
	return new CatAssignmentOperator(static_cast<CatTypedExpression*>(nodeParser.getASTNodeByIndex(0)),
									 static_cast<CatTypedExpression*>(nodeParser.getASTNodeByIndex(1)));

}


ASTNode* CatGrammar::infixOperator(const ASTNodeParser& nodeParser)
{
	CatTypedExpression* lhs = static_cast<CatTypedExpression*>(nodeParser.getASTNodeByIndex(0));
	CatTypedExpression* rhs = static_cast<CatTypedExpression*>(nodeParser.getASTNodeByIndex(1));
	
	const ParseToken* infix = nullptr;
	if (nodeParser.getNumItems() <= 3)
	{
		//This is a parse of the form "exp + exp"
		infix = nodeParser.getTerminalByIndex(0);
	}
	else
	{
		//This is a parse of the form "(exp) + exp"
		infix = nodeParser.getTerminalByIndex(2);
	}

	CatInfixOperatorType operatorType = CatInfixOperatorType::Plus;
	if (infix->getTokenID() == Cat::one)
	{
		switch ((OneChar)infix->getTokenSubType())
		{
			default:
			case OneChar::Plus:			operatorType = CatInfixOperatorType::Plus;		break;
			case OneChar::Minus:		operatorType = CatInfixOperatorType::Minus;		break;			
			case OneChar::Times:		operatorType = CatInfixOperatorType::Multiply;	break;			
			case OneChar::Divide:		operatorType = CatInfixOperatorType::Divide;		break;			
			case OneChar::Modulo:		operatorType = CatInfixOperatorType::Modulo;		break;
			case OneChar::Greater:		operatorType = CatInfixOperatorType::Greater;		break;
			case OneChar::Smaller:		operatorType = CatInfixOperatorType::Smaller;		break;
		}
	}
	else if (infix->getTokenID() == Cat::two)
	{
		switch ((TwoChar)infix->getTokenSubType())
		{
			case TwoChar::GreaterOrEqual:	operatorType = CatInfixOperatorType::GreaterOrEqual;	break;
			case TwoChar::SmallerOrEqual:	operatorType = CatInfixOperatorType::SmallerOrEqual;	break;
			case TwoChar::Equals:			operatorType = CatInfixOperatorType::Equals;			break;
			case TwoChar::NotEquals:		operatorType = CatInfixOperatorType::NotEquals;		break;
			case TwoChar::LogicalAnd:		operatorType = CatInfixOperatorType::LogicalAnd;		break;
			case TwoChar::LogicalOr:		operatorType = CatInfixOperatorType::LogicalOr;		break;
		}
	}

	return new CatInfixOperator(lhs, rhs, operatorType);
}


ASTNode* CatGrammar::prefixOperator(const ASTNodeParser& nodeParser)
{
	CatPrefixOperator* oper = new CatPrefixOperator();
	oper->rhs.reset(static_cast<CatTypedExpression*>(nodeParser.getASTNodeByIndex(0)));
	const ParseToken* prefix = nodeParser.getTerminalByIndex(0);
	oper->oper = CatPrefixOperator::Operator::Not;
	if (prefix->getTokenID() == Cat::one)
	{
		switch ((OneChar)prefix->getTokenSubType())
		{
			default:
			case OneChar::Not:
				oper->oper = CatPrefixOperator::Operator::Not;
				break;
			case OneChar::Minus:
				oper->oper = CatPrefixOperator::Operator::Minus;
				break;
		}
	}
	return oper;
}


ASTNode* CatGrammar::literalToken(const ASTNodeParser& nodeParser)
{
	const ParseToken* literalToken = nodeParser.getTerminalByIndex(0);
	if (literalToken->getTokenID() == Cat::lit)
	{
		switch ((ConstantType)literalToken->getTokenSubType())
		{
			default:
				return new CatLiteral();
			case ConstantType::Integer:
			{
				CatLiteral* intLiteral = new CatLiteral(atoi(literalToken->getLexeme()->getDataPointer()));
				return intLiteral;
			}
			case ConstantType::FloatingPoint:
			{
				CatLiteral* floatLiteral = new CatLiteral((float)atof(literalToken->getLexeme()->getDataPointer()));
				return floatLiteral;
			}
			case ConstantType::String:
			{
				CatLiteral* stringLiteral = new CatLiteral(std::string(literalToken->getLexeme()->getDataPointer() + 1, literalToken->getLexeme()->length - 2));
				return stringLiteral;
			}
			case ConstantType::Bool:
			{
				//if the first character of the match is 't' then it's "true"
				CatLiteral* boolLiteral = new CatLiteral(literalToken->getLexeme()->getDataPointer()[0] == 't'
														 || literalToken->getLexeme()->getDataPointer()[0] == 'T');
				
				return boolLiteral;
			}
			case ConstantType::Char:
			{
				CatLiteral* charLiteral = new CatLiteral(literalToken->getLexeme()->getDataPointer()[0]);
				return charLiteral;
			}			
		}
	}
	return new CatLiteral();
}


ASTNode* CatGrammar::identifierToken(const ASTNodeParser& nodeParser)
{
	const ParseToken* token = nodeParser.getTerminalByIndex(0);
	std::string variableName(token->getLexeme()->getDataPointer(), token->getLexeme()->length);
	return new CatIdentifier(variableName, static_cast<CatRuntimeContext*>(nodeParser.getContext()));
}


ASTNode* CatGrammar::argumentListToken(const ASTNodeParser& nodeParser)
{
	CatArgumentList* argumentList = new CatArgumentList();
	unLink(nodeParser.getASTNodeByIndex(0), argumentList->arguments);
	return argumentList;
}


ASTNode* CatGrammar::functionCallToken(const ASTNodeParser& nodeParser)
{
	const ParseToken* nameToken = nodeParser.getTerminalByIndex(0);
	std::string functionName(nameToken->getLexeme()->getDataPointer(), nameToken->getLexeme()->length);
	CatArgumentList* arguments = static_cast<CatArgumentList*>(nodeParser.getASTNodeByIndex(0));
	if (CatFunctionCall::isBuiltInFunction(functionName.c_str(), (int)arguments->arguments.size()))
	{
		return new CatFunctionCall(functionName, arguments);
	}
	else
	{
		CatRuntimeContext* runtimeContext = static_cast<CatRuntimeContext*>(nodeParser.getContext());
		if (runtimeContext != nullptr)
		{
			CatScopeID scopeId = InvalidScopeID;
			std::string lowerName = Tools::toLowerCase(functionName);
			MemberFunctionInfo* functionInfo = runtimeContext->findFunction(lowerName, scopeId);
			if (functionInfo != nullptr)
			{
				return new CatMemberFunctionCall(lowerName, new CatScopeRoot(scopeId, runtimeContext), arguments);
			}
		}
		return new CatFunctionCall(functionName, arguments);
	}
}


ASTNode* CatGrammar::memberAccessToken(const ASTNodeParser& nodeParser)
{
	const ParseToken* nameToken = nodeParser.getTerminalByIndex(1);
	std::string memberName(nameToken->getLexeme()->getDataPointer(), nameToken->getLexeme()->length);
	CatTypedExpression* base = static_cast<CatTypedExpression*>(nodeParser.getASTNodeByIndex(0));
	return new CatMemberAccess(base, memberName);
}


ASTNode* CatGrammar::memberFunctionCallToken(const ASTNodeParser& nodeParser)
{
	const ParseToken* nameToken = nodeParser.getTerminalByIndex(1);
	std::string functionName(nameToken->getLexeme()->getDataPointer(), nameToken->getLexeme()->length);
	CatTypedExpression* base = static_cast<CatTypedExpression*>(nodeParser.getASTNodeByIndex(0));
	CatArgumentList* arguments = static_cast<CatArgumentList*>(nodeParser.getASTNodeByIndex(1));
	return new CatMemberFunctionCall(functionName, base, arguments);
}


ASTNode* CatGrammar::arrayIndexToken(const ASTNodeParser& nodeParser)
{
	CatTypedExpression* base = static_cast<CatTypedExpression*>(nodeParser.getASTNodeByIndex(0));
	CatTypedExpression* index = static_cast<CatTypedExpression*>(nodeParser.getASTNodeByIndex(1));
	return new CatArrayIndex(base, index);
}
