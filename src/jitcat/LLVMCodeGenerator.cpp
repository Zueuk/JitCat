/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#include "jitcat/LLVMCodeGenerator.h"
#include "jitcat/CatASTNodes.h"
#include "jitcat/Configuration.h"
#include "jitcat/LLVMCodeGeneratorHelper.h"
#include "jitcat/LLVMCompileTimeContext.h"
#include "jitcat/LLVMCatIntrinsics.h"
#include "jitcat/LLVMJit.h"
#include "jitcat/LLVMTypes.h"
#include "jitcat/MemberFunctionInfo.h"
#include "jitcat/MemberInfo.h"

#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>


#include <iostream>

using namespace jitcat;
using namespace jitcat::AST;
using namespace jitcat::LLVM;
using namespace jitcat::Reflection;


LLVMCodeGenerator::LLVMCodeGenerator(const std::string& name):
	currentModule(new llvm::Module("JitCat", LLVMJit::get().getContext())),
	builder(new llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter>(LLVMJit::get().getContext())),
	executionSession(new llvm::orc::ExecutionSession()),
	helper(new LLVMCodeGeneratorHelper(builder.get(), currentModule.get())),
	mangler(new llvm::orc::MangleAndInterner(*executionSession, LLVMJit::get().getDataLayout())),
	objectLinkLayer(new llvm::orc::RTDyldObjectLinkingLayer(*executionSession.get(),
															[]() {	return llvm::make_unique<llvm::SectionMemoryManager>();})),
	compileLayer(new llvm::orc::IRCompileLayer(*executionSession.get(), *(objectLinkLayer.get()), llvm::orc::ConcurrentIRCompiler(LLVMJit::get().getTargetMachineBuilder())))
{
	
	llvm::orc::SymbolMap intrinsicSymbols;
	runtimeLibraryDyLib = &executionSession->createJITDylib("runtimeLibrary", false);

	llvm::JITSymbolFlags functionFlags;
	functionFlags |= llvm::JITSymbolFlags::Callable;
	functionFlags |= llvm::JITSymbolFlags::Exported;
	functionFlags |= llvm::JITSymbolFlags::Absolute;

	intrinsicSymbols[executionSession->intern("fmodf")] = llvm::JITEvaluatedSymbol(reinterpret_cast<llvm::JITTargetAddress>(&fmodf), functionFlags);
	intrinsicSymbols[executionSession->intern("_fmod")] = llvm::JITEvaluatedSymbol(reinterpret_cast<llvm::JITTargetAddress>(&fmodl), functionFlags);
	intrinsicSymbols[executionSession->intern("sinf")] = llvm::JITEvaluatedSymbol(reinterpret_cast<llvm::JITTargetAddress>(&sinf), functionFlags);
	intrinsicSymbols[executionSession->intern("_sin")] = llvm::JITEvaluatedSymbol(reinterpret_cast<llvm::JITTargetAddress>(&sinl), functionFlags);
	intrinsicSymbols[executionSession->intern("cosf")] = llvm::JITEvaluatedSymbol(reinterpret_cast<llvm::JITTargetAddress>(&cosf), functionFlags);
	intrinsicSymbols[executionSession->intern("_cos")] = llvm::JITEvaluatedSymbol(reinterpret_cast<llvm::JITTargetAddress>(&cosl), functionFlags);
	intrinsicSymbols[executionSession->intern("log10f")] = llvm::JITEvaluatedSymbol(reinterpret_cast<llvm::JITTargetAddress>(&log10f), functionFlags);
	intrinsicSymbols[executionSession->intern("_log10")] = llvm::JITEvaluatedSymbol(reinterpret_cast<llvm::JITTargetAddress>(&log10l), functionFlags);
	intrinsicSymbols[executionSession->intern("powf")] = llvm::JITEvaluatedSymbol(reinterpret_cast<llvm::JITTargetAddress>(&powf), functionFlags);
	intrinsicSymbols[executionSession->intern("_pow")] = llvm::JITEvaluatedSymbol(reinterpret_cast<llvm::JITTargetAddress>(&powl), functionFlags);
	intrinsicSymbols[executionSession->intern("ceilf")] = llvm::JITEvaluatedSymbol(reinterpret_cast<llvm::JITTargetAddress>(&ceilf), functionFlags);
	intrinsicSymbols[executionSession->intern("_ceil")] = llvm::JITEvaluatedSymbol(reinterpret_cast<llvm::JITTargetAddress>(&ceill), functionFlags);
	intrinsicSymbols[executionSession->intern("floorf")] = llvm::JITEvaluatedSymbol(reinterpret_cast<llvm::JITTargetAddress>(&floorf), functionFlags);
	intrinsicSymbols[executionSession->intern("_floor")] = llvm::JITEvaluatedSymbol(reinterpret_cast<llvm::JITTargetAddress>(&floorl), functionFlags);
	

	llvm::cantFail(runtimeLibraryDyLib->define(llvm::orc::absoluteSymbols(intrinsicSymbols)));

	dylib = &executionSession->createJITDylib(Tools::append(name, "_", 0), false);
	dylib->addToSearchOrder(*runtimeLibraryDyLib, false);

	if constexpr (Configuration::enableSymbolSearchWorkaround)
	{
		objectLinkLayer->setAutoClaimResponsibilityForObjectSymbols(true);
		objectLinkLayer->setOverrideObjectFlagsWithResponsibilityFlags(true);
	}

	std::string targetTriple = LLVMJit::get().getTargetMachine().getTargetTriple().str();
	currentModule->setTargetTriple(targetTriple);
	currentModule->setDataLayout(LLVMJit::get().getDataLayout());
	helper->setCurrentModule(currentModule.get());
	// Create a new pass manager attached to it.
	passManager = llvm::make_unique<llvm::legacy::FunctionPassManager>(currentModule.get());

	// Do simple "peephole" optimizations and bit-twiddling optzns.
	passManager->add(llvm::createInstructionCombiningPass());
	// Reassociate expressions.
	passManager->add(llvm::createReassociatePass());
	// Eliminate Common SubExpressions.
	passManager->add(llvm::createGVNPass());
	// Simplify the control flow graph (deleting unreachable blocks, etc).
	passManager->add(llvm::createCFGSimplificationPass());

	passManager->doInitialization();

}


LLVMCodeGenerator::~LLVMCodeGenerator()
{
}


llvm::Value* LLVMCodeGenerator::generate(CatTypedExpression* expression, LLVMCompileTimeContext* context)
{
	initContext(context);
	switch (expression->getNodeType())
	{
		case CatASTNodeType::Literal:				return generate(static_cast<CatLiteral*>(expression), context);			
		case CatASTNodeType::Identifier:			return generate(static_cast<CatIdentifier*>(expression), context);		
		case CatASTNodeType::InfixOperator:			return generate(static_cast<CatInfixOperator*>(expression), context);	
		case CatASTNodeType::AssignmentOperator:	return generate(static_cast<CatAssignmentOperator*>(expression), context);	
		case CatASTNodeType::PrefixOperator:		return generate(static_cast<CatPrefixOperator*>(expression), context);	
		case CatASTNodeType::FunctionCall:			return generate(static_cast<CatFunctionCall*>(expression), context);		
		case CatASTNodeType::MemberAccess:			return generate(static_cast<CatMemberAccess*>(expression), context);		
		case CatASTNodeType::ArrayIndex:			return generate(static_cast<CatArrayIndex*>(expression), context);		
		case CatASTNodeType::MemberFunctionCall:	return generate(static_cast<CatMemberFunctionCall*>(expression), context);
		case CatASTNodeType::ScopeRoot:				return generate(static_cast<CatScopeRoot*>(expression), context);		
	}
	assert(false);
	return nullptr;
}


llvm::Function* LLVMCodeGenerator::generateExpressionFunction(CatTypedExpression* expression, LLVMCompileTimeContext* context, const std::string& name)
{
	initContext(context);
	
	//Define the parameters for the function.
	//Each function will at least get a CatRuntimeContext* as a parameter.
	//If the function returns a string by value, the string is returned through a string pointer parameter and the function returns void.
	//The string pointer should point to pre-allocated memory where the returned string will be constructed.
	std::vector<llvm::Type*> parameters;
	llvm::FunctionType* functionType = nullptr;
	CatGenericType expressionType = expression->getType();
	if (expressionType.isStringType())
	{
		parameters.push_back(LLVMTypes::stringPtrType);
		parameters.push_back(LLVMTypes::pointerType);
		functionType = llvm::FunctionType::get(LLVMTypes::voidType, parameters, false);		
	}
	else
	{
		parameters.push_back(LLVMTypes::pointerType);
		functionType = llvm::FunctionType::get(helper->toLLVMType(expressionType), parameters, false);
	}

	//Create the function signature. No code is yet associated with the function at this time.
	llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::LinkageTypes::ExternalLinkage, name.c_str(), currentModule.get());

	//Attributes and names for the parameters can now be set on the function signature.
	//When returning a string, the StructRet attribute is set to indicate that the parameter is used for returning a structure by value.
	llvm::Argument* firstArg = function->arg_begin();
	if (expressionType.isStringType())
	{
		firstArg->setName("stringRet");
		firstArg++;
		firstArg->setName("RuntimeContext");
		function->addParamAttr(0, llvm::Attribute::AttrKind::StructRet);
		function->addParamAttr(0, llvm::Attribute::AttrKind::NoAlias);
	}
	else
	{
		firstArg->setName("RuntimeContext");
	}
	
	//Now, generate code for the function
	context->currentFunction = function;
	
	//Function entry block
	llvm::BasicBlock* block = llvm::BasicBlock::Create(LLVMJit::get().getContext(), "entry", function);
	builder->SetInsertPoint(&function->getEntryBlock());

	//Generate code for the expression.
	llvm::Value* expressionValue = generate(expression, context);

	//If the expression returns a string, copy construct it into the string StructRet parameter and return void.
	//If it is some other type, just return the value.
	if (expressionType.isStringType())
	{
		helper->createCall(context, &LLVMCatIntrinsics::stringCopyConstruct, {function->arg_begin(), expressionValue}, "stringCopyConstruct");
		helper->generateBlockDestructors(context);
		builder->CreateRetVoid();
	}
	else
	{
		helper->generateBlockDestructors(context);
		builder->CreateRet(expressionValue);
	}
	context->currentFunction = nullptr;

	//Verify the correctness of the function and execute optimization passes.
	return verifyAndOptimizeFunction(function);
}


llvm::Function* LLVMCodeGenerator::generateExpressionAssignFunction(CatAssignableExpression* expression, LLVMCompileTimeContext* context, const std::string& name)
{
	initContext(context);
	
	//Define the parameters for the function.
	//Assign functions will always have two parameters: a CatRuntimeContext* and a value that will be assigned to the result of the assignable expression.
	//It will always return void.
	std::vector<llvm::Type*> parameters = {LLVMTypes::pointerType, helper->toLLVMType(expression->getType())};
	llvm::FunctionType* functionType = llvm::FunctionType::get(LLVMTypes::voidType, parameters, false);		

	//Create the function signature. No code is yet associated with the function at this time.
	llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::LinkageTypes::ExternalLinkage, name.c_str(), currentModule.get());

	//Name the parameters
	llvm::Argument* argIter = function->arg_begin();
	argIter->setName("RuntimeContext");
	++argIter;
	argIter->setName("ValueToAssign");

	//Now, generate code for the function
	context->currentFunction = function;

	//Function entry block
	llvm::BasicBlock* block = llvm::BasicBlock::Create(LLVMJit::get().getContext(), "entry", function);
	builder->SetInsertPoint(&function->getEntryBlock());

	//Generate the assignment expression, passing in the second function parameter.
	generateAssign(expression, argIter, context);

	//Destruct any heap allocations done by the function
	helper->generateBlockDestructors(context);
	
	//Create the return statement, returning void.
	builder->CreateRetVoid();

	//Verify the correctness of the function and execute optimization passes.
	return verifyAndOptimizeFunction(function);
}


intptr_t LLVMCodeGenerator::generateAndGetFunctionAddress(CatTypedExpression* expression, LLVMCompileTimeContext* context)
{
	initContext(context);
	createNewModule(context);
	std::string functionName = getNextFunctionName(context);
	llvm::Function* function = generateExpressionFunction(expression, context, functionName);
	assert(function != nullptr);
	llvm::cantFail(compileLayer->add(*dylib, llvm::orc::ThreadSafeModule(std::move(currentModule), LLVMJit::get().getThreadSafeContext())));
	return (intptr_t)getSymbolAddress(functionName.c_str(), *dylib);
}


intptr_t LLVMCodeGenerator::generateAndGetAssignFunctionAddress(CatAssignableExpression* expression, LLVMCompileTimeContext* context)
{
	initContext(context);
	createNewModule(context);
	std::string functionName = getNextFunctionName(context);
	llvm::Function* function = generateExpressionAssignFunction(expression, context, functionName);
	assert(function != nullptr);
	llvm::cantFail(compileLayer->add(*dylib, llvm::orc::ThreadSafeModule(std::move(currentModule), LLVMJit::get().getThreadSafeContext())));
	return (intptr_t)getSymbolAddress(functionName.c_str(), *dylib);
}


llvm::Value* LLVMCodeGenerator::generate(CatIdentifier* identifier, LLVMCompileTimeContext* context)
{
	CatScopeID scopeId = identifier->getScopeId();
	llvm::Value* parentObjectAddress = getBaseAddress(scopeId, context);

	const TypeMemberInfo* memberInfo = identifier->getMemberInfo();

	llvm::Value* result = memberInfo->generateDereferenceCode(parentObjectAddress, context);
	if (result != nullptr)
	{
		return result;
	}
	else
	{
		assert(false);
		return LLVMJit::logError("ERROR: Not yet supported.");
	}
}


llvm::Value* LLVMCodeGenerator::generate(CatFunctionCall* functionCall, LLVMCompileTimeContext* context)
{
	CatArgumentList* arguments = functionCall->getArgumentList();
	switch (functionCall->getFunctionType())
	{
		case CatBuiltInFunctionType::ToVoid:	generate(arguments->arguments[0].get(), context); return nullptr;
		case CatBuiltInFunctionType::ToInt:		return helper->convertType(generate(arguments->arguments[0].get(), context), LLVMTypes::intType, context);
		case CatBuiltInFunctionType::ToFloat:	return helper->convertType(generate(arguments->arguments[0].get(), context), LLVMTypes::floatType, context);
		case CatBuiltInFunctionType::ToBool:	return helper->convertType(generate(arguments->arguments[0].get(), context), LLVMTypes::boolType, context);
		case CatBuiltInFunctionType::Select:
		{
			llvm::Value* conditionValue = helper->convertType(generate(arguments->arguments[0].get(), context), LLVMTypes::boolType, context);
			llvm::Value* trueValue = generate(arguments->arguments[1].get(), context);
			llvm::Value* falseValue = helper->convertType(generate(arguments->arguments[2].get(), context), trueValue->getType(), context);
			return builder->CreateSelect(conditionValue, trueValue, falseValue);
		}
		case CatBuiltInFunctionType::Abs:
		{
			if (arguments->arguments[0]->getType().isFloatType())
			{
				return helper->callIntrinsic(llvm::Intrinsic::fabs, CatGenericType::floatType, generate(arguments->arguments[0].get(), context), context);
			}
			else if (arguments->arguments[0]->getType() == CatGenericType::intType)
			{
				llvm::Value* intValue = generate(arguments->arguments[0].get(), context);
				llvm::Value* isGreaterOrEqualToZero = builder->CreateICmpSGT(intValue, helper->createConstant(-1));
				llvm::Value* negated = builder->CreateSub(helper->createConstant(0), intValue);
				return builder->CreateSelect(isGreaterOrEqualToZero, intValue, negated);
			}
			return nullptr;
		}
		case CatBuiltInFunctionType::Log:	return helper->callIntrinsic(llvm::Intrinsic::log10, CatGenericType::floatType, generate(arguments->arguments[0].get(), context), context);
		case CatBuiltInFunctionType::Sqrt:	return helper->callIntrinsic(llvm::Intrinsic::sqrt, CatGenericType::floatType, generate(arguments->arguments[0].get(), context), context);
		case CatBuiltInFunctionType::Pow:
		{
			return helper->callIntrinsic(llvm::Intrinsic::pow, CatGenericType::floatType, generate(arguments->arguments[0].get(), context), generate(arguments->arguments[1].get(), context), context);
		}
		case CatBuiltInFunctionType::Sin:		return helper->callIntrinsic(llvm::Intrinsic::sin, CatGenericType::floatType, generate(arguments->arguments[0].get(), context), context);
		case CatBuiltInFunctionType::Cos:		return helper->callIntrinsic(llvm::Intrinsic::cos, CatGenericType::floatType, generate(arguments->arguments[0].get(), context), context);
		case CatBuiltInFunctionType::Ceil:		return helper->callIntrinsic(llvm::Intrinsic::ceil, CatGenericType::floatType, generate(arguments->arguments[0].get(), context), context);
		case CatBuiltInFunctionType::Floor:		return helper->callIntrinsic(llvm::Intrinsic::floor, CatGenericType::floatType, generate(arguments->arguments[0].get(), context), context);
		case CatBuiltInFunctionType::Tan:		return builder->CreateFDiv(helper->callIntrinsic(llvm::Intrinsic::sin, CatGenericType::floatType, generate(arguments->arguments[0].get(), context), context), helper->callIntrinsic(llvm::Intrinsic::cos, CatGenericType::floatType, generate(arguments->arguments[0].get(), context), context));
		case CatBuiltInFunctionType::Cap:
		{
			llvm::Value* value = generate(arguments->arguments[0].get(), context);
			llvm::Value* minValue = generate(arguments->arguments[1].get(), context);
			llvm::Value* maxValue = generate(arguments->arguments[2].get(), context);
			if (arguments->arguments[0]->getType().isFloatType())
			{
				llvm::Value* convertedMin = helper->convertType(minValue, LLVMTypes::floatType, context);
				llvm::Value* convertedMax = helper->convertType(maxValue, LLVMTypes::floatType, context);
				llvm::Value* maxSmallerThanMin = builder->CreateFCmp(llvm::CmpInst::Predicate::FCMP_OLT, convertedMax, convertedMin);
				llvm::Value* finalMin = builder->CreateSelect(maxSmallerThanMin, convertedMax, convertedMin);
				llvm::Value* finalMax = builder->CreateSelect(maxSmallerThanMin, convertedMin, convertedMax);
				llvm::Value* smallerThanMin = builder->CreateFCmp(llvm::CmpInst::Predicate::FCMP_OLT, value, finalMin);
				llvm::Value* greaterThanMax = builder->CreateFCmp(llvm::CmpInst::Predicate::FCMP_OGT, value, finalMax);
				llvm::Value* cappedToMin = builder->CreateSelect(smallerThanMin, finalMin, value);
				llvm::Value* cappedToMax = builder->CreateSelect(greaterThanMax, finalMax, cappedToMin);
				return cappedToMax;
			}
			else if (arguments->arguments[0]->getType().isIntType())
			{
				llvm::Value* convertedMin = helper->convertType(minValue, LLVMTypes::intType, context);
				llvm::Value* convertedMax = helper->convertType(maxValue, LLVMTypes::intType, context);
				llvm::Value* maxSmallerThanMin = builder->CreateICmp(llvm::CmpInst::Predicate::ICMP_SLT, convertedMax, convertedMin);
				llvm::Value* finalMin = builder->CreateSelect(maxSmallerThanMin, convertedMax, convertedMin);
				llvm::Value* finalMax = builder->CreateSelect(maxSmallerThanMin, convertedMin, convertedMax);
				llvm::Value* smallerThanMin = builder->CreateICmp(llvm::CmpInst::Predicate::ICMP_SLT, value, finalMin);
				llvm::Value* greaterThanMax = builder->CreateICmp(llvm::CmpInst::Predicate::ICMP_SGT, value, finalMax);
				llvm::Value* cappedToMin = builder->CreateSelect(smallerThanMin, finalMin, value);
				llvm::Value* cappedToMax = builder->CreateSelect(greaterThanMax, finalMax, cappedToMin);
				return cappedToMax;
			}
			else
			{
				assert(false);
				return nullptr;
			}
		}
		case CatBuiltInFunctionType::Min:
		{
			llvm::Value* value1 = generate(arguments->arguments[0].get(), context);
			llvm::Value* value2 = generate(arguments->arguments[1].get(), context);
			if (arguments->arguments[0]->getType().isFloatType())
			{
				llvm::Value* convertedValue2 = helper->convertType(value2, LLVMTypes::floatType, context);
				llvm::Value* lessThan = builder->CreateFCmp(llvm::CmpInst::Predicate::FCMP_OLT, value1, convertedValue2);
				return builder->CreateSelect(lessThan, value1, convertedValue2);
			}
			else if (arguments->arguments[0]->getType().isIntType())
			{
				llvm::Value* convertedValue2 = helper->convertType(value2, LLVMTypes::intType, context);
				llvm::Value* lessThan = builder->CreateICmp(llvm::CmpInst::Predicate::ICMP_SLT, value1, convertedValue2);
				return builder->CreateSelect(lessThan, value1, convertedValue2);
			}
			else
			{
				assert(false);
				return nullptr;
			}
		}
		case CatBuiltInFunctionType::Max:
		{
			llvm::Value* value1 = generate(arguments->arguments[0].get(), context);
			llvm::Value* value2 = generate(arguments->arguments[1].get(), context);
			if (arguments->arguments[0]->getType().isFloatType())
			{
				llvm::Value* convertedValue2 = helper->convertType(value2, LLVMTypes::floatType, context);
				llvm::Value* greaterThan = builder->CreateFCmp(llvm::CmpInst::Predicate::FCMP_OGT, value1, convertedValue2);
				return builder->CreateSelect(greaterThan, value1, convertedValue2);
			}
			else if (arguments->arguments[0]->getType().isIntType())
			{
				llvm::Value* convertedValue2 = helper->convertType(value2, LLVMTypes::intType, context);
				llvm::Value* greaterThan = builder->CreateICmp(llvm::CmpInst::Predicate::ICMP_SGT, value1, convertedValue2);
				return builder->CreateSelect(greaterThan, value1, convertedValue2);
			}
			else
			{
				assert(false);
				return nullptr;
			}
		}
		case CatBuiltInFunctionType::FindInString:
		{
			llvm::Value* text = helper->convertType(generate(arguments->arguments[0].get(), context), LLVMTypes::stringPtrType, context);
			llvm::Value* textToFind = helper->convertType(generate(arguments->arguments[1].get(), context), LLVMTypes::stringPtrType, context);
			return helper->createCall(context, &LLVMCatIntrinsics::findInString, {text, textToFind}, "findInString");
		}
		case CatBuiltInFunctionType::ReplaceInString:
		{
			llvm::Value* sourceString = helper->convertType(generate(arguments->arguments[0].get(), context), LLVMTypes::stringPtrType, context);
			llvm::Value* stringToFind = helper->convertType(generate(arguments->arguments[1].get(), context), LLVMTypes::stringPtrType, context);
			llvm::Value* replacementString = helper->convertType(generate(arguments->arguments[2].get(), context), LLVMTypes::stringPtrType, context);
			return helper->createCall(context, &LLVMCatIntrinsics::replaceInString, {sourceString, stringToFind, replacementString}, "replaceInString");
		}
		case CatBuiltInFunctionType::StringLength:
		{
			llvm::Value* string = helper->convertType(generate(arguments->arguments[0].get(), context), LLVMTypes::stringPtrType, context);
			return helper->createCall(context, &LLVMCatIntrinsics::stringLength, {string}, "stringLength");
		}
		case CatBuiltInFunctionType::SubString:
		{
			llvm::Value* sourceString = helper->convertType(generate(arguments->arguments[0].get(), context), LLVMTypes::stringPtrType, context);
			llvm::Value* offset = helper->convertType(generate(arguments->arguments[1].get(), context), LLVMTypes::intType, context);
			llvm::Value* length = helper->convertType(generate(arguments->arguments[2].get(), context), LLVMTypes::intType, context);
			return helper->createCall(context, &LLVMCatIntrinsics::subString, {sourceString, offset, length}, "subString");
		}
		case CatBuiltInFunctionType::ToString:
		{
			return helper->convertType(generate(arguments->arguments[0].get(), context), LLVMTypes::stringPtrType, context);
		}
		case CatBuiltInFunctionType::ToPrettyString:
		{
			if (arguments->arguments[0]->getType().isIntType())
			{
				return helper->createCall(context, &LLVMCatIntrinsics::intToPrettyString, {generate(arguments->arguments[0].get(), context)}, "intToPrettyString");
			}
			else
			{
				return helper->convertType(generate(arguments->arguments[0].get(), context), LLVMTypes::stringPtrType, context);
			}
		}
		case CatBuiltInFunctionType::ToFixedLengthString:
		{
			llvm::Value* number = helper->convertType(generate(arguments->arguments[0].get(), context), LLVMTypes::intType, context);
			llvm::Value* length = helper->convertType(generate(arguments->arguments[1].get(), context), LLVMTypes::intType, context);
			return helper->createCall(context, &LLVMCatIntrinsics::intToFixedLengthString, {number, length}, "intToFixedLengthString");
		}
		case CatBuiltInFunctionType::Random:
		{
			return helper->createCall(context, &LLVMCatIntrinsics::getRandomFloat, {}, "getRandomFloat");
		}
		case CatBuiltInFunctionType::RandomRange:
		{
			llvm::Value* left = generate(arguments->arguments[0].get(), context);
			llvm::Value* right = generate(arguments->arguments[1].get(), context);
			if (arguments->arguments[0]->getType().isBoolType()
				&& arguments->arguments[1]->getType().isBoolType())
			{
				return helper->createCall(context, &LLVMCatIntrinsics::getRandomBoolean, {left, right}, "getRandomBoolean");
			}
			else if (arguments->arguments[0]->getType().isIntType()
					 && arguments->arguments[1]->getType().isIntType())
			{
				return helper->createCall(context, &LLVMCatIntrinsics::getRandomInt, {left, right}, "getRandomInt");
			}
			else				
			{
				llvm::Value* leftFloat = helper->convertType(left, LLVMTypes::floatType, context);
				llvm::Value* rightFloat = helper->convertType(right, LLVMTypes::floatType, context);
				return helper->createCall(context, &LLVMCatIntrinsics::getRandomFloatRange, {leftFloat, rightFloat}, "getRandomFloat");
			}
		}
		case CatBuiltInFunctionType::Round:
		{
			llvm::Value* left = generate(arguments->arguments[0].get(), context);
			llvm::Value* right = generate(arguments->arguments[1].get(), context);
			llvm::Value* leftFloat = helper->convertType(left, LLVMTypes::floatType, context);
			llvm::Value* rightInt = helper->convertType(right, LLVMTypes::intType, context);
			return helper->createCall(context, &LLVMCatIntrinsics::roundFloat, {leftFloat, rightInt}, "roundFloat");
		}
		case CatBuiltInFunctionType::StringRound:
		{
			llvm::Value* left = generate(arguments->arguments[0].get(), context);
			llvm::Value* right = generate(arguments->arguments[1].get(), context);
			llvm::Value* leftFloat = helper->convertType(left, LLVMTypes::floatType, context);
			llvm::Value* rightInt = helper->convertType(right, LLVMTypes::intType, context);
			return helper->createCall(context, &LLVMCatIntrinsics::roundFloatToString, {leftFloat, rightInt}, "roundFloatToString");
		}
		default:
		{
			// Look up the name in the global module table.
			llvm::Function* functionInModule = currentModule->getFunction(functionCall->getFunctionName());
			if (functionInModule == nullptr)
			{
				assert(false);
				return LLVMJit::logError("Unknown function referenced: ", functionCall->getFunctionName());
			}

			// If argument mismatch error.
			if (functionInModule->arg_size() != arguments->arguments.size())
			{
				assert(false);
				return LLVMJit::logError("Incorrect # arguments passed in function: ", functionCall->getFunctionName());
			}

			std::vector<llvm::Value*> argumentValues;
			for (std::size_t i = 0, e = arguments->arguments.size(); i != e; ++i)
			{
				llvm::Value* generatedValue = generate(arguments->arguments[i].get(), context);
				assert(generatedValue != nullptr);
				argumentValues.push_back(generatedValue);
			}

			return builder->CreateCall(functionInModule, argumentValues, "calltmp");
		}
	}
}


llvm::Value* LLVMCodeGenerator::generate(CatInfixOperator* infixOperator, LLVMCompileTimeContext* context)
{
	llvm::Value* left = generate(infixOperator->getLeft(), context);
	llvm::Value* right = generate(infixOperator->getRight(), context);
	CatInfixOperatorType oper = infixOperator->getOperatorType();

	assert(left != nullptr && right != nullptr);

	if (oper == CatInfixOperatorType::LogicalOr)
	{
		return builder->CreateOr(helper->convertType(left, LLVMTypes::boolType, context), helper->convertType(right, LLVMTypes::boolType, context), "or");
	}
	else if (oper == CatInfixOperatorType::LogicalAnd)
	{
		return builder->CreateAnd(helper->convertType(left, LLVMTypes::boolType, context), helper->convertType(right, LLVMTypes::boolType, context), "and");
	}
	else if (left->getType() != right->getType())
	{
		//Operators must always operate on identical types.
		//If types are different, we must first convert them.
		if (left->getType() == LLVMTypes::stringPtrType
			|| right->getType() == LLVMTypes::stringPtrType)
		{
			left = helper->convertType(left, LLVMTypes::stringPtrType, context);
			right = helper->convertType(right, LLVMTypes::stringPtrType, context);
		}
		else if (left->getType() == LLVMTypes::floatType
			|| right->getType() == LLVMTypes::floatType)
		{
			left = helper->convertType(left, LLVMTypes::floatType, context);
			right = helper->convertType(right, LLVMTypes::floatType, context);
		}
		else
		{
			//left and right are ints or booleans, booleans will be promoted to ints
			left = helper->convertType(left, LLVMTypes::intType, context);
			right = helper->convertType(right, LLVMTypes::intType, context);
		}
	}
	if (left->getType() == LLVMTypes::floatType)
	{
		switch (oper)
		{
			case CatInfixOperatorType::Plus:				return builder->CreateFAdd(left, right, "added");				
			case CatInfixOperatorType::Minus:				return builder->CreateFSub(left, right, "subtracted");		
			case CatInfixOperatorType::Multiply:			return builder->CreateFMul(left, right, "multiplied");		
			case CatInfixOperatorType::Divide:				return builder->CreateSelect(builder->CreateFCmpUEQ(right, helper->createConstant(0.0f)), helper->createConstant(0.0f), builder->CreateFDiv(left, right, "divided"));			
			case CatInfixOperatorType::Modulo:				return builder->CreateSelect(builder->CreateFCmpUEQ(right, helper->createConstant(0.0f)), helper->createConstant(0.0f), builder->CreateFRem(left, right, "divided"));
			case CatInfixOperatorType::Greater:				return builder->CreateFCmpUGT(left, right, "greater");		
			case CatInfixOperatorType::Smaller:				return builder->CreateFCmpULT(left, right, "smaller");		
			case CatInfixOperatorType::GreaterOrEqual:		return builder->CreateFCmpUGE(left, right, "greaterOrEqual");	
			case CatInfixOperatorType::SmallerOrEqual:		return builder->CreateFCmpULE(left, right, "lessOrEqual");	
			case CatInfixOperatorType::Equals:				return builder->CreateFCmpUEQ(left, right, "equal");			
			case CatInfixOperatorType::NotEquals:			return builder->CreateFCmpUNE(left, right, "notEqual");		
		}
	}
	else if (left->getType() == LLVMTypes::intType)
	{
		switch (oper)
		{
			case CatInfixOperatorType::Plus:				return builder->CreateAdd(left, right, "added");				
			case CatInfixOperatorType::Minus:				return builder->CreateSub(left, right, "subtracted");			
			case CatInfixOperatorType::Multiply:			return builder->CreateMul(left, right, "multiplied");			
			case CatInfixOperatorType::Divide:				return builder->CreateSelect(builder->CreateICmpEQ(right, helper->createConstant(0)), helper->createConstant(0), builder->CreateSDiv(left, right, "divided"));					 
			case CatInfixOperatorType::Modulo:				return builder->CreateSelect(builder->CreateICmpEQ(right, helper->createConstant(0)), helper->createConstant(0), builder->CreateSRem(left, right, "modulo"));			
			case CatInfixOperatorType::Greater:				return builder->CreateICmpSGT(left, right, "greater");		
			case CatInfixOperatorType::Smaller:				return builder->CreateICmpSLT(left, right, "smaller");		
			case CatInfixOperatorType::GreaterOrEqual:		return builder->CreateICmpSGE(left, right, "greaterOrEqual");	
			case CatInfixOperatorType::SmallerOrEqual:		return builder->CreateICmpSLE(left, right, "smallerOrEqual");	
			case CatInfixOperatorType::Equals:				return builder->CreateICmpEQ(left, right, "equal");
			case CatInfixOperatorType::NotEquals:			return builder->CreateICmpNE(left, right, "notEqual");		
		}
	}
	else if (left->getType() == LLVMTypes::boolType)
	{
		switch (oper)
		{
			case CatInfixOperatorType::Equals:				return builder->CreateICmpEQ(left, right, "equal");
			case CatInfixOperatorType::NotEquals:			return builder->CreateICmpNE(left, right, "notEqual");		
		}
	}
	else if (left->getType() == LLVMTypes::stringPtrType)
	{
		llvm::Value* stringNullCheck = nullptr;
		if (context->options.enableDereferenceNullChecks)
		{
			llvm::Value* leftNotNull = builder->CreateIsNotNull(left, "leftStringNotNull");
			llvm::Value* rightNotNull = builder->CreateIsNotNull(right, "rightStringNotNull");
			stringNullCheck = builder->CreateAnd(leftNotNull, rightNotNull, "neitherStringNull");
		}
		switch (oper)
		{
			case CatInfixOperatorType::Plus:
			{
				return helper->createOptionalNullCheckSelect(stringNullCheck, [&](LLVMCompileTimeContext* compileContext){return compileContext->helper->createCall(compileContext, &LLVMCatIntrinsics::stringAppend, {left, right}, "stringAppend");}, [&](LLVMCompileTimeContext* compileContext){return compileContext->helper->createEmptyStringPtrConstant();}, context);
			}
			case CatInfixOperatorType::Equals:
			{
				return helper->createOptionalNullCheckSelect(stringNullCheck, [&](LLVMCompileTimeContext* compileContext){return compileContext->helper->createCall(compileContext, &LLVMCatIntrinsics::stringEquals, {left, right}, "stringEquals");}, LLVMTypes::boolType, context);  
			}
			case CatInfixOperatorType::NotEquals:		
			{
				return helper->createOptionalNullCheckSelect(stringNullCheck, [&](LLVMCompileTimeContext* compileContext){return compileContext->helper->createCall(compileContext, &LLVMCatIntrinsics::stringNotEquals, {left, right}, "stringNotEquals");}, [&](LLVMCompileTimeContext* compileContext){return compileContext->helper->createConstant(true);}, context);
			}
		}
	}
	assert(false);
	return LLVMJit::logError("ERROR: Invalid operation.");
}


llvm::Value* LLVMCodeGenerator::generate(CatAssignmentOperator* assignmentOperator, LLVMCompileTimeContext* context)
{
	assert(assignmentOperator->getLhs()->isAssignable());
	return generateAssign(static_cast<CatAssignableExpression*>(assignmentOperator->getLhs()), generate(assignmentOperator->getRhs(), context), context);
}


llvm::Value* LLVMCodeGenerator::generate(CatLiteral* literal, LLVMCompileTimeContext* context)
{
	CatGenericType lhsType = literal->getType();
	if		(lhsType.isIntType())		return helper->createConstant(std::any_cast<int>(literal->getValue()));
	else if (lhsType.isFloatType())		return helper->createConstant(std::any_cast<float>(literal->getValue()));
	else if (lhsType.isBoolType())		return helper->createConstant(std::any_cast<bool>(literal->getValue()));
	else if (lhsType.isStringType())
	{
		const std::string& stringReference = std::any_cast<const std::string&>(literal->getValue());
		llvm::Value* stringObjectAddress = helper->createIntPtrConstant(reinterpret_cast<std::uintptr_t>(&stringReference), "stringLiteralAddress");
		return builder->CreateIntToPtr(stringObjectAddress, LLVMTypes::stringPtrType);			
	}
	else
	{
		assert(false); return LLVMJit::logError("ERROR: Not a basic type."); 
	}
}


llvm::Value* LLVMCodeGenerator::generate(CatMemberAccess* memberAccess, LLVMCompileTimeContext* context)
{
	llvm::Value* base = generate(memberAccess->getBase(), context);
	return memberAccess->getMemberInfo()->generateDereferenceCode(base, context);
}


llvm::Value* LLVMCodeGenerator::generate(CatMemberFunctionCall* memberFunctionCall, LLVMCompileTimeContext* context)
{
	MemberFunctionInfo* functionInfo = memberFunctionCall->getMemberFunctionInfo();
	CatTypedExpression* base = memberFunctionCall->getBase();
	CatArgumentList* arguments = memberFunctionCall->getArguments();
	CatGenericType& returnType = functionInfo->returnType;
	MemberFunctionCallData callData = functionInfo->getFunctionAddress();

	llvm::Value* baseObject = generate(base, context);

	auto notNullCodeGen = [=](LLVMCompileTimeContext* compileContext)
	{
		std::vector<llvm::Value*> argumentList;
		llvm::Value* functionThis = compileContext->helper->convertToPointer(baseObject, memberFunctionCall->getMemberFunctionInfo()->memberFunctionName + "_This_Ptr");
		argumentList.push_back(functionThis);
		std::vector<llvm::Type*> argumentTypes;
		argumentTypes.push_back(LLVMTypes::pointerType);
		if (!callData.makeDirectCall)
		{
			//Add an argument that contains a pointer to a MemberFunctionInfo object.
			llvm::Value* memberFunctionAddressValue = compileContext->helper->createIntPtrConstant(callData.functionInfoStructAddress, "MemberFunctionInfo_IntPtr");
			llvm::Value* memberFunctionPtrValue = compileContext->helper->convertToPointer(memberFunctionAddressValue, "MemberFunctionInfo_Ptr");
			argumentList.push_back(memberFunctionPtrValue);
			argumentTypes.push_back(LLVMTypes::pointerType);
		}

		for (auto& iter : arguments->arguments)
		{
			argumentList.push_back(generate(iter.get(), compileContext));
			argumentTypes.push_back(argumentList.back()->getType());
		}

		uintptr_t functionAddress = callData.makeDirectCall ? callData.memberFunctionAddress : callData.staticFunctionAddress;

		if (!returnType.isStringType() && !returnType.isContainerType())
		{
			llvm::Type* returnLLVMType = compileContext->helper->toLLVMType(returnType);
			llvm::FunctionType* functionType = llvm::FunctionType::get(returnLLVMType, argumentTypes, false);
			llvm::CallInst* call = static_cast<llvm::CallInst*>(compileContext->helper->createCall(functionType, functionAddress, argumentList, base->getType().toString() + "." + memberFunctionCall->getMemberFunctionInfo()->memberFunctionName));
			if (Configuration::useThisCall && callData.makeDirectCall)
			{
				call->setCallingConv(llvm::CallingConv::X86_ThisCall);
			}
			return static_cast<llvm::Value*>(call);
		}
		else if (returnType.isStringType())
		{
			llvm::Value* stringObjectAllocation = compileContext->helper->createStringAllocA(compileContext, memberFunctionCall->getMemberFunctionInfo()->memberFunctionName + "_Result", context->options.enableDereferenceNullChecks);
			auto sretTypeInsertPoint = argumentTypes.begin();
			if (!Configuration::sretBeforeThis && callData.makeDirectCall)
			{
				sretTypeInsertPoint++;
			}
			argumentTypes.insert(sretTypeInsertPoint, LLVMTypes::stringPtrType);
			if (context->options.enableDereferenceNullChecks)
			{
				//Delete the default constructed string object allocation
				compileContext->helper->createCall(context, &LLVMCatIntrinsics::stringDestruct, {stringObjectAllocation}, "stringDestruct");
			}
			llvm::FunctionType* functionType = llvm::FunctionType::get(LLVMTypes::voidType, argumentTypes, false);
			auto sretInsertPoint = argumentList.begin();
			if (!Configuration::sretBeforeThis && callData.makeDirectCall)
			{
				sretInsertPoint++;
			}
			argumentList.insert(sretInsertPoint, stringObjectAllocation);
			llvm::CallInst* call = static_cast<llvm::CallInst*>(compileContext->helper->createCall(functionType, functionAddress, argumentList, base->getType().toString() + "." + memberFunctionCall->getMemberFunctionInfo()->memberFunctionName));
			if (Configuration::useThisCall && callData.makeDirectCall)
			{
				call->setCallingConv(llvm::CallingConv::X86_ThisCall);
			}
			call->addParamAttr(Configuration::sretBeforeThis ? 0 : 1, llvm::Attribute::AttrKind::StructRet);
			call->addDereferenceableAttr(Configuration::sretBeforeThis ? 1 : 2, sizeof(std::string));
			return stringObjectAllocation;
		}
		else
		{
			return LLVMJit::logError("ERROR: Not yet supported.");
		}
	};
	return helper->createOptionalNullCheckSelect(baseObject, notNullCodeGen, helper->toLLVMType(returnType), context);
}


llvm::Value* LLVMCodeGenerator::generate(CatPrefixOperator* prefixOperator, LLVMCompileTimeContext* context)
{
	llvm::Value* right = generate(prefixOperator->rhs.get(), context);
	assert(right != nullptr);
	llvm::Type* rightType = right->getType();
	if (!(rightType == LLVMTypes::intType || rightType == LLVMTypes::boolType || rightType == LLVMTypes::floatType))
	{
		assert(false);
		return LLVMJit::logError("ERROR: Type not yet supported for prefix operators.");
	}
	if (prefixOperator->oper == CatPrefixOperator::Operator::Not)
	{
		return builder->CreateNot(helper->convertType(right, LLVMTypes::boolType, context), "not");
	}
	else if (prefixOperator->oper == CatPrefixOperator::Operator::Minus)
	{
		if (rightType == LLVMTypes::floatType)
		{
			return builder->CreateFNeg(right, "negative");
		}
		else
		{
			return builder->CreateNeg(helper->convertType(right, LLVMTypes::intType, context), "negative");
		}
	}
	else
	{
		assert(false);
		return LLVMJit::logError("ERROR: Operator not implemented.");
	}
}


llvm::Value* LLVMCodeGenerator::generate(CatArrayIndex* arrayIndex, LLVMCompileTimeContext* context)
{
	CatTypedExpression* base = arrayIndex->getBase();
	const TypeMemberInfo* memberInfo = nullptr;
	if (base->getNodeType() == CatASTNodeType::Identifier)
	{
		memberInfo = static_cast<CatIdentifier*>(base)->getMemberInfo();
	}
	else if (base->getNodeType() == CatASTNodeType::MemberAccess)
	{
		memberInfo = static_cast<CatMemberAccess*>(base)->getMemberInfo();
	}
	assert(memberInfo != nullptr);

	CatTypedExpression* index = arrayIndex->getIndex();
	llvm::Value* containerAddress = generate(base, context);
	llvm::Value* containerIndex = generate(index, context);
	return memberInfo->generateArrayIndexCode(containerAddress, containerIndex, context);
}


llvm::Value* LLVMCodeGenerator::generate(CatScopeRoot* scopeRoot, LLVMCompileTimeContext* context)
{
	return getBaseAddress(scopeRoot->getScopeId(), context);
}


llvm::Value* LLVMCodeGenerator::generateAssign(CatAssignableExpression* expression, llvm::Value* rValue, LLVMCompileTimeContext* context)
{
	context->helper = helper.get();
	context->currentDyLib = dylib;
	switch (expression->getNodeType())
	{
		case CatASTNodeType::Identifier:	return generateAssign(static_cast<CatIdentifier*>(expression), rValue, context);
		case CatASTNodeType::MemberAccess:	return generateAssign(static_cast<CatMemberAccess*>(expression), rValue, context);
	}
	assert(false);
	return nullptr;
}


llvm::Value* LLVMCodeGenerator::generateAssign(CatIdentifier* identifier, llvm::Value* rValue, LLVMCompileTimeContext* context)
{
	CatScopeID scopeId = identifier->getScopeId();
	llvm::Value* parentObjectAddress = getBaseAddress(scopeId, context);

	const TypeMemberInfo* memberInfo = identifier->getMemberInfo();

	llvm::Value* result = memberInfo->generateAssignCode(parentObjectAddress, rValue, context);
	if (result != nullptr)
	{
		return result;
	}
	else
	{
		assert(false);
		return LLVMJit::logError("ERROR: Not supported.");
	}
}


llvm::Value* LLVMCodeGenerator::generateAssign(CatMemberAccess* memberAccess, llvm::Value* rValue, LLVMCompileTimeContext* context)
{
	llvm::Value* base = generate(memberAccess->getBase(), context);
	return memberAccess->getMemberInfo()->generateAssignCode(base, rValue, context);
}


llvm::Value* LLVMCodeGenerator::getBaseAddress(CatScopeID scopeId, LLVMCompileTimeContext* context)
{
	llvm::Value* parentObjectAddress = nullptr;
	if (context->catContext->isStaticScope(scopeId))
	{
		Reflectable* object = context->catContext->getScopeObject(scopeId);
		parentObjectAddress = llvm::ConstantInt::get(LLVMJit::get().getContext(), llvm::APInt(sizeof(std::uintptr_t) * 8, (uint64_t)reinterpret_cast<std::uintptr_t>(object), false));
	}
	else
	{
		//Get the CatRuntimeContext argument from the current function
		assert(context->currentFunction != nullptr);
		assert(context->currentFunction->arg_size() > 0);
		llvm::Argument* argument = context->currentFunction->arg_begin();
		if (context->currentFunction->arg_size() > 0 && context->currentFunction->hasAttribute(1, llvm::Attribute::StructRet))
		{
			argument = context->currentFunction->arg_begin() + 1;
		}
		assert(argument->getName() == "RuntimeContext");
		assert(argument->getType() == LLVMTypes::pointerType);
		llvm::Value* scopeIdValue = context->helper->createConstant((int)scopeId);
		llvm::Value* address = address = helper->createCall(context, &LLVMCatIntrinsics::getScopePointerFromContext, {argument, scopeIdValue}, "getScopePointerFromContext"); 
		assert(address != nullptr);
		parentObjectAddress = helper->convertToIntPtr(address, "CustomThis_IntPtr");
	}

	return parentObjectAddress;
}


void LLVMCodeGenerator::initContext(LLVMCompileTimeContext* context)
{
	context->helper = helper.get();
	context->currentDyLib = dylib;
}


void LLVMCodeGenerator::createNewModule(LLVMCompileTimeContext* context)
{
	currentModule.reset(new llvm::Module(context->catContext->getContextName(), LLVMJit::get().getContext()));
	currentModule->setTargetTriple(LLVMJit::get().getTargetMachine().getTargetTriple().str());
	currentModule->setDataLayout(LLVMJit::get().getDataLayout());
	helper->setCurrentModule(currentModule.get());

}


std::string LLVMCodeGenerator::getNextFunctionName(LLVMCompileTimeContext * context)
{
	return Tools::append("expression_", context->catContext->getContextName(), "_", context->catContext->getNextFunctionIndex());
}


llvm::Function* LLVMCodeGenerator::verifyAndOptimizeFunction(llvm::Function* function)
{
	if (!llvm::verifyFunction(*function, &llvm::outs()))
	{
		passManager->run(*function);
		if constexpr (Configuration::dumpFunctionIR)
		{
			function->dump();
		}		
		return function;
	}
	else
	{
#ifdef _DEBUG
		function->dump();
#endif
		assert(false);
		LLVMJit::logError("Function contains errors.");
		return nullptr;
	}
}


llvm::Expected<llvm::JITEvaluatedSymbol> jitcat::LLVM::LLVMCodeGenerator::findSymbol(const std::string& name, llvm::orc::JITDylib& dyLib) const
{
	return executionSession->lookup({&dyLib}, mangler->operator()(name));
}


llvm::JITTargetAddress jitcat::LLVM::LLVMCodeGenerator::getSymbolAddress(const std::string& name, llvm::orc::JITDylib& dyLib) const
{
	return llvm::cantFail(findSymbol(name, dyLib)).getAddress();
}
