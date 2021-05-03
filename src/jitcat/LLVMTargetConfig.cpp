/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2021
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#include "jitcat/Configuration.h"
#include "jitcat/LLVMTargetConfig.h"
#include "jitcat/LLVMTypes.h"

#include <llvm/IR/CallingConv.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Target/TargetMachine.h>

using namespace jitcat;
using namespace jitcat::LLVM;


LLVMTargetConfig::LLVMTargetConfig(bool isJITTarget, bool sretBeforeThis, bool useThisCall, bool callerDestroysTemporaryArguments, 
								   bool enableSymbolSearchWorkaround, bool is64BitTarget, unsigned int sizeOfBoolInBits, 
								   unsigned int defaultLLVMCallingConvention, const std::string& targetTripple, const std::string& cpuName,
								   std::string objectFileExtension, const llvm::TargetOptions& targetOptions, const llvm::SubtargetFeatures& subtargetFeatures, 
								   llvm::CodeGenOpt::Level optimizationLevel, llvm::Optional<llvm::Reloc::Model> relocationModel,
								   llvm::Optional<llvm::CodeModel::Model> codeModel):
	isJITTarget(isJITTarget),
	is64BitTarget(is64BitTarget),
	sretBeforeThis(sretBeforeThis),
	useThisCall(useThisCall),
	callerDestroysTemporaryArguments(callerDestroysTemporaryArguments),
	enableSymbolSearchWorkaround(enableSymbolSearchWorkaround),
	sizeOfBoolInBits(sizeOfBoolInBits),
	defaultLLVMCallingConvention(defaultLLVMCallingConvention),
	llvmTypes(std::make_unique<LLVMTypes>(is64BitTarget, sizeOfBoolInBits)),
	targetTripple(targetTripple),
	cpuName(cpuName),
	objectFileExtension(objectFileExtension),
	targetOptions(targetOptions),
	subtargetFeatures(subtargetFeatures),
	optimizationLevel(optimizationLevel),
	relocationModel(relocationModel),
	codeModel(codeModel)
{
	std::string errorMessage;
	const llvm::Target* target = llvm::TargetRegistry::lookupTarget(targetTripple, errorMessage);
	if (target != nullptr)
	{
		targetMachine.reset(target->createTargetMachine(targetTripple, cpuName, subtargetFeatures.getString(), targetOptions, 
													   relocationModel, codeModel, optimizationLevel, isJITTarget));
		dataLayout = std::make_unique<llvm::DataLayout>(targetMachine->createDataLayout());
	}
	if (isJITTarget)
	{
		targetMachineBuilder = llvm::cantFail(llvm::orc::JITTargetMachineBuilder::detectHost());
	}
}


LLVMTargetConfig::~LLVMTargetConfig()
{
}


std::unique_ptr<LLVMTargetConfig> LLVMTargetConfig::createJITTargetConfig()
{
	return createTargetConfigForCurrentMachine(true);
}


std::unique_ptr<LLVMTargetConfig> LLVMTargetConfig::createConfigForPreconfiguredTarget(LLVMTarget target)
{
	switch (target)
	{
		case LLVMTarget::CurrentMachine:		return createTargetConfigForCurrentMachine(false);
		case LLVMTarget::CurrentMachineJIT:		return createTargetConfigForCurrentMachine(true);
		case LLVMTarget::Windows_X64:			return createGenericWindowsx64Target();
		case LLVMTarget::Playstation4:			return createPS4Target();
		case LLVMTarget::XboxOne:				return createXboxOneTarget();
	}
	return nullptr;
}


llvm::TargetMachine& LLVMTargetConfig::getTargetMachine() const
{
	return *targetMachine.get();
}


const llvm::DataLayout& LLVMTargetConfig::getDataLayout() const
{
	return *dataLayout.get();
}


llvm::Expected<const llvm::orc::JITTargetMachineBuilder&> LLVMTargetConfig::getTargetMachineBuilder() const
{
	if (targetMachineBuilder.hasValue())
	{
		return targetMachineBuilder.getValue();
	}
	else
	{
		return llvm::make_error<llvm::StringError>("Not a JIT target configuration.", llvm::inconvertibleErrorCode());
	}
}


const LLVMTypes& LLVMTargetConfig::getLLVMTypes() const
{
	return *llvmTypes.get();
}


std::unique_ptr<LLVMTargetConfig> LLVMTargetConfig::createTargetConfigForCurrentMachine(bool isJITTarget)
{
	constexpr bool isWin32 = 
	#ifdef WIN32
			true;
	#else
			false;
	#endif

	bool sretBeforeThis = !isWin32;
	bool callerDestroysTemporaryArguments =  !isWin32;
	bool useThisCall = isWin32;
	bool enableSymbolSearchWorkaround = isWin32;
	
	unsigned int defaultCallingConvention = llvm::CallingConv::C;
	#ifdef _WIN64
		defaultCallingConvention = llvm::CallingConv::Win64;
	#endif

	const char* objectFileExtension = "o";
	if (isWin32)
	{
		objectFileExtension = "obj";
	}
	std::string targetTripple = llvm::sys::getProcessTriple();
	std::string cpuName = llvm::sys::getHostCPUName();

	llvm::SubtargetFeatures features;

	llvm::StringMap<bool> featureMap;
	llvm::sys::getHostCPUFeatures(featureMap);
	for (auto &Feature : featureMap)
	{
		features.AddFeature(Feature.first(), Feature.second);
	}

	llvm::TargetOptions options;
	if (isJITTarget)
	{
		options.EmulatedTLS = true;
		options.ExplicitEmulatedTLS = true;
	}

	return std::make_unique<LLVMTargetConfig>(isJITTarget, sretBeforeThis, useThisCall, callerDestroysTemporaryArguments, 
											  enableSymbolSearchWorkaround, sizeof(uintptr_t) == 8, (unsigned int)sizeof(bool) * 8,
											  defaultCallingConvention, targetTripple, cpuName, objectFileExtension,
											  options, features, llvm::CodeGenOpt::Level::Default);	
}


std::unique_ptr<LLVMTargetConfig> LLVMTargetConfig::createGenericWindowsx64Target()
{
	constexpr bool isWin32 = true;

	bool sretBeforeThis = !isWin32;
	bool callerDestroysTemporaryArguments =  !isWin32;
	bool useThisCall = isWin32;
	bool enableSymbolSearchWorkaround = isWin32;
	
	unsigned int defaultCallingConvention = llvm::CallingConv::Win64;

	std::string targetTripple = "x86_64-pc-windows-msvc";
	std::string cpuName = "x86-64";

	//Use the default features of x86-64
	llvm::SubtargetFeatures features;
	//Don't specify any additional options
	llvm::TargetOptions options;

	return std::make_unique<LLVMTargetConfig>(false, sretBeforeThis, useThisCall, callerDestroysTemporaryArguments, 
											  enableSymbolSearchWorkaround, true, 8,
											  defaultCallingConvention, targetTripple, cpuName, "obj",
											  options, features, llvm::CodeGenOpt::Level::Default);		
}


std::unique_ptr<LLVMTargetConfig> LLVMTargetConfig::createXboxOneTarget()
{
	constexpr bool isWin32 = true;

	bool sretBeforeThis = !isWin32;
	bool callerDestroysTemporaryArguments =  !isWin32;
	bool useThisCall = isWin32;
	bool enableSymbolSearchWorkaround = isWin32;
	
	unsigned int defaultCallingConvention = llvm::CallingConv::X86_FastCall;

	std::string targetTripple = "x86_64-pc-win32";
	std::string cpuName = "btver2";

	//Use the default features of btver2
	llvm::SubtargetFeatures features;
	//Don't specify any additional options
	llvm::TargetOptions options;
	options.UnsafeFPMath = true;
	options.NoInfsFPMath = true;
	options.NoNaNsFPMath = true;
	options.UnsafeFPMath = true;
	options.NoSignedZerosFPMath = true;
	options.ThreadModel = llvm::ThreadModel::POSIX;
	options.DebuggerTuning = llvm::DebuggerKind::Default;
	options.DataSections = true;
	return std::make_unique<LLVMTargetConfig>(false, sretBeforeThis, useThisCall, callerDestroysTemporaryArguments, 
											  enableSymbolSearchWorkaround, true, 8,
											  defaultCallingConvention, targetTripple, cpuName, "obj",
											  options, features, llvm::CodeGenOpt::Level::Default, 
											  llvm::Reloc::Model::PIC_, llvm::CodeModel::Small);	

}


std::unique_ptr<LLVMTargetConfig> LLVMTargetConfig::createPS4Target()
{
	constexpr bool isWin32 = false;

	bool sretBeforeThis = !isWin32;
	bool callerDestroysTemporaryArguments =  !isWin32;
	bool useThisCall = isWin32;
	bool enableSymbolSearchWorkaround = isWin32;
	
	unsigned int defaultCallingConvention = llvm::CallingConv::X86_FastCall;

	std::string targetTripple = "x86_64-scei-ps4";
	std::string cpuName = "btver2";

	//Use the default features of btver2
	llvm::SubtargetFeatures features;
	//Don't specify any additional options
	llvm::TargetOptions options;
	options.UnsafeFPMath = true;
	options.RelaxELFRelocations = true;
	options.NoInfsFPMath = true;
	options.NoNaNsFPMath = true;
	options.UnsafeFPMath = true;
	options.NoSignedZerosFPMath = true;
	options.ThreadModel = llvm::ThreadModel::POSIX;
	options.DebuggerTuning = llvm::DebuggerKind::SCE;
	options.DataSections = true;
	return std::make_unique<LLVMTargetConfig>(false, sretBeforeThis, useThisCall, callerDestroysTemporaryArguments, 
											  enableSymbolSearchWorkaround, true, 8, 
											  defaultCallingConvention, targetTripple, cpuName, "o", 
											  options, features, llvm::CodeGenOpt::Level::Default, 
											  llvm::Reloc::Model::PIC_, llvm::CodeModel::Small);
}