#pragma once

class CatRuntimeContext;
struct LLVMCompileTimeContext;
class Reflectable;
class LLVMCodeGeneratorHelper;
#include "LLVMForwardDeclares.h"
#include "LLVMTypes.h"
#include <string>
#include <vector>


//This class contains static functions that are called directly from jitted code. 
//They represent some of the built-in/intrinsic functions available in JitCat.
//It is a run-time library of sorts.
class LLVMCatIntrinsics
{
	LLVMCatIntrinsics();
	~LLVMCatIntrinsics() = delete;

public:
	static Reflectable* getThisPointerFromContext(CatRuntimeContext* context);
	static Reflectable* getCustomThisPointerFromContext(CatRuntimeContext* context);
	static bool stringEquals(const std::string& left, const std::string& right);
	static bool stringNotEquals(const std::string& left, const std::string& right);
	static std::string stringAppend(const std::string& left, const std::string& right);
	static std::string floatToString(float number);
	static std::string intToString(int number);
	static std::string intToPrettyString(int number);
	static std::string intToFixedLengthString(int number, int stringLength);
	static void stringCopyConstruct(std::string* destination, const std::string& string);
	static void stringDestruct(std::string* target);
	static int findInString(const std::string& text, const std::string& textToFind);
	static std::string replaceInString(const std::string& text, const std::string& textToFind, const std::string& replacement);
	static int stringLength(const std::string& text);
	static	std::string subString(const std::string& text, int start, int length);
	static float getRandomFloat();
	static bool getRandomBoolean(bool first, bool second);
	static int getRandomInt(int min, int max);
	static float getRandomFloatRange(float min, float max);
	static float roundFloat(float number, int decimals);
	static std::string roundFloatToString(float number, int decimals);
	//static	round:
	//static	stringRound:
};

