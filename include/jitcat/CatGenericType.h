/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#pragma once

class TypeInfo;
#include "CatError.h"
#include "CatInfixOperatorType.h"
#include "ContainerType.h"

#include <any>
#include <fstream>
#include <map>
#include <memory>
#include <string>


class CatGenericType
{
private:
	enum class SpecificType
	{
		None,
		Error,
		Basic,
		Object,
		Container,
		Count
	};
	
	enum class BasicType
	{
		None,
		Int,
		Float,
		String,
		Bool,
		Void,
		Count
	};


	CatGenericType(BasicType catType, bool writable = false, bool constant = false);

public:
	CatGenericType();
	CatGenericType(TypeInfo* objectType, bool writable = false, bool constant = false);
	CatGenericType(ContainerType containerType, TypeInfo* itemType, bool writable = false, bool constant = false);
	CatGenericType(const CatError& error);
	CatGenericType(const CatGenericType& other);

	CatGenericType& operator=(const CatGenericType& other);
	bool operator== (const CatGenericType& other) const;
	bool operator!= (const CatGenericType& other) const;

	bool isUnknown() const;
	bool isValidType() const;
	bool isBasicType() const;
	bool isBoolType() const;
	bool isIntType() const;
	bool isFloatType() const;
	bool isStringType() const;
	bool isScalarType() const;
	bool isVoidType() const;
	bool isObjectType() const;
	bool isContainerType() const;
	bool isVectorType() const;
	bool isMapType() const;

	bool isWritable() const;
	bool isConst() const;

	CatGenericType getContainerItemType() const;
	const char* getObjectTypeName() const;

	const CatError& getError() const;

	CatGenericType getInfixOperatorResultType(CatInfixOperatorType oper, const CatGenericType& rightType);

	std::string toString() const;

	TypeInfo* getObjectType() const;

	//This will cast the pointer to the C++ type associated with this CatGenericType and returns it as a std::any
	std::any createAnyOfType(uintptr_t pointer);
	std::any createDefault() const;

	//Converts value of valueType to this type if possible, otherwise returns default value
	std::any convertToType(std::any value, const CatGenericType& valueType) const;

	void printValue(std::any& value);

	static float convertToFloat(std::any value, const CatGenericType& valueType);
	static int convertToInt(std::any value, const CatGenericType& valueType);
	static bool convertToBoolean(std::any value, const CatGenericType& valueType);
	static std::string convertToString(std::any value, const CatGenericType& valueType);
	static CatGenericType readFromXML(std::ifstream& xmlFile, const std::string& closingTag, std::map<std::string, TypeInfo*>& typeInfos);
	void writeToXML(std::ofstream& xmlFile, const char* linePrefixCharacters);

	static CatGenericType createIntType(bool isWritable, bool isConst);
	static CatGenericType createFloatType(bool isWritable, bool isConst);
	static CatGenericType createBoolType(bool isWritable, bool isConst);
	static CatGenericType createStringType(bool isWritable, bool isConst);

private:
	static const char* toString(BasicType type);
	static BasicType toBasicType(const char* value);
	static const char* toString(SpecificType type);
	static SpecificType toSpecificType(const char* value);

public:
	static const CatGenericType intType;
	static const CatGenericType floatType;
	static const CatGenericType boolType;
	static const CatGenericType stringType;
	static const CatGenericType voidType;
	static const CatGenericType errorType;
	static const CatGenericType unknownType;

private:
	SpecificType specificType;
	BasicType basicType;
	//not owned
	TypeInfo* nestedType;

	//When the member is a container, catType or nestedType will be set to the item type of the container
	ContainerType containerType;

	bool writable;
	bool constant;

	std::unique_ptr<CatError> error;
};
