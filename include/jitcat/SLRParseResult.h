/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#pragma once

namespace jitcat::AST
{
	class ASTNode;
}
#include <memory>
#include <string>


namespace jitcat::Parser
{

	struct SLRParseResult
	{
		SLRParseResult();
		~SLRParseResult();

		template<typename ASTNodeType>
		ASTNodeType* getNode() { return static_cast<ASTNodeType*>(astRootNode.get());}

		template<typename ASTNodeType>
		ASTNodeType* releaseNode() { return static_cast<ASTNodeType*>(astRootNode.release());}

		bool success;
		std::unique_ptr<AST::ASTNode> astRootNode;
	};

}