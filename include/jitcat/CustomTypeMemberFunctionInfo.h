/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#pragma once


#include "jitcat/CatGenericType.h"
#include "jitcat/MemberFunctionInfo.h"

namespace jitcat
{
	class CatRuntimeContext;
}
namespace jitcat::AST
{
	class CatFunctionDefinition;
}

namespace jitcat::Reflection
{
	struct CustomTypeMemberFunctionInfo: public MemberFunctionInfo
	{
		CustomTypeMemberFunctionInfo(AST::CatFunctionDefinition* functionDefinition, const CatGenericType& thisType);
		virtual ~CustomTypeMemberFunctionInfo();

		virtual std::any call(CatRuntimeContext* runtimeContext, std::any& base, const std::vector<std::any>& parameters) override final;
		
		virtual MemberFunctionCallData getFunctionAddress() const override final;

		virtual std::string getMangledName() const override final;

		CatGenericType thisType;
		AST::CatFunctionDefinition* functionDefinition;
		intptr_t nativeAddress;
	};
}