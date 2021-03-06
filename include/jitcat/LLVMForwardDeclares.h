/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#pragma once

namespace llvm
{
	class ArrayType;
	class CallInst;
	class Constant;
	class ConstantFolder;
	class Function;
	class FunctionType;
	class IRBuilderDefaultInserter;
	template<typename T, typename Inserter>
	class IRBuilder;
	class LLVMContext;
	class Module;
	class PointerType;
	class Type;
	class Value;

	namespace Intrinsic 
	{
		typedef unsigned ID;
	};

	namespace legacy
	{
		class FunctionPassManager;
	};

	namespace orc
	{
		class JITDylib;
		class RTDyldObjectLinkingLayer;
		class IRCompileLayer;
		class ExecutionSession;
		class MangleAndInterner;
	};
};