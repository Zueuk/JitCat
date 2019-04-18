/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#include "jitcat/CatGenericType.h"
#include "jitcat/CatLog.h"
#include "jitcat/Tools.h"
#include "jitcat/TypeInfo.h"
#include "jitcat/XMLHelper.h"

#include <cassert>
#include <cctype>
#include <algorithm>
#include <iostream>
#include <locale>
#include <string>

using namespace jitcat;
using namespace jitcat::AST;
using namespace jitcat::Reflection;
using namespace jitcat::Tools;


CatGenericType::CatGenericType(SpecificType specificType, BasicType basicType, TypeInfo* nestedType, ContainerType containerType, Reflection::ContainerManipulator* containerManipulator, bool writable, bool constant, const CatError* error_):
	specificType(specificType),
	basicType(basicType),
	nestedType(nestedType),
	containerType(containerType),
	containerManipulator(containerManipulator),
	writable(writable),
	constant(constant)
{
	if (error_ != nullptr)
	{
		error.reset(new CatError(*error_));
	}
}


CatGenericType::CatGenericType(BasicType basicType, bool writable, bool constant):
	specificType(SpecificType::Basic),
	basicType(basicType),
	nestedType(nullptr),
	containerType(ContainerType::None),
	containerManipulator(nullptr),
	error(nullptr),
	writable(writable),
	constant(constant)
{
}


CatGenericType::CatGenericType():
	specificType(SpecificType::None),
	basicType(BasicType::None),
	nestedType(nullptr),
	containerType(ContainerType::None),
	containerManipulator(nullptr),
	error(nullptr),
	writable(false),
	constant(false)
{
}


CatGenericType::CatGenericType(TypeInfo* objectType, bool writable, bool constant):
	specificType(SpecificType::Object),
	basicType(BasicType::None),
	nestedType(objectType),
	containerType(ContainerType::None),
	containerManipulator(nullptr),
	error(nullptr),
	writable(writable),
	constant(constant)
{
}


CatGenericType::CatGenericType(ContainerType containerType, Reflection::ContainerManipulator* containerManipulator, TypeInfo* itemType, bool writable, bool constant):
	specificType(SpecificType::Container),
	basicType(BasicType::None),
	nestedType(itemType),
	containerType(containerType),
	containerManipulator(containerManipulator),
	error(nullptr),
	writable(writable),
	constant(constant)
{
}


CatGenericType::CatGenericType(const CatError& error):
	specificType(SpecificType::Error),
	basicType(BasicType::None),
	nestedType(nullptr),
	containerType(ContainerType::None),
	containerManipulator(nullptr),
	error(new CatError(error)),
	writable(false),
	constant(false)
{
}


CatGenericType::CatGenericType(const CatGenericType& other):
	specificType(other.specificType),
	basicType(other.basicType),
	nestedType(other.nestedType),
	containerType(other.containerType),
	containerManipulator(other.containerManipulator),
	error(nullptr),
	writable(other.writable),
	constant(other.constant)
{
	if (other.error.get() != nullptr)
	{
		error.reset(new CatError(*other.error.get()));
	}
}


CatGenericType& CatGenericType::operator=(const CatGenericType& other)
{
	specificType = other.specificType;
	basicType = other.basicType;
	nestedType = other.nestedType;
	containerType = other.containerType;
	containerManipulator = other.containerManipulator;
	writable = other.writable;
	constant = other.constant;
	if (other.error.get() != nullptr)
	{
		error.reset(new CatError(other.getError()));
	}
	else
	{
		error.reset(nullptr);
	}
	return *this;
}


bool CatGenericType::operator==(const CatGenericType& other) const
{
	if (specificType == other.specificType)
	{
		switch (specificType)
		{
			default:
			case SpecificType::None:		return true;
			case SpecificType::Error:		return error == other.error;
			case SpecificType::Basic:		return basicType == other.basicType;
			case SpecificType::Object:		return nestedType->getTypeName() == other.nestedType->getTypeName();
			case SpecificType::Container:	return containerType == other.containerType && nestedType->getTypeName() == other.nestedType->getTypeName() && containerManipulator == other.containerManipulator;
		}
	}
	else
	{
		return false;
	}
}


bool CatGenericType::operator!=(const CatGenericType& other) const
{
	return !this->operator==(other);
}


bool CatGenericType::isUnknown() const
{
	return specificType == SpecificType::None;
}


bool CatGenericType::isValidType() const
{
	return specificType != SpecificType::Error 
		   && specificType != SpecificType::None 
		   && (specificType != SpecificType::Basic || (basicType != BasicType::None))
		   && (specificType != SpecificType::Object || nestedType != nullptr);
}


bool CatGenericType::isError() const
{
	return specificType == SpecificType::Error;
}


bool CatGenericType::isBasicType() const
{
	return specificType == SpecificType::Basic 
						   && (	  basicType == BasicType::Bool
							   || basicType == BasicType::Int
							   || basicType == BasicType::Float
							   || basicType == BasicType::String);
}


bool CatGenericType::isBoolType() const
{
	return specificType == SpecificType::Basic && basicType == BasicType::Bool;
}


bool CatGenericType::isIntType() const
{
	return specificType == SpecificType::Basic && basicType == BasicType::Int;
}


bool CatGenericType::isFloatType() const
{
	return specificType == SpecificType::Basic && basicType == BasicType::Float;
}


bool CatGenericType::isStringType() const
{
	return specificType == SpecificType::Basic && basicType == BasicType::String;
}


bool CatGenericType::isScalarType() const
{
	return specificType == SpecificType::Basic 
						   && (	  basicType == BasicType::Float 
							   || basicType == BasicType::Int);
}


bool CatGenericType::isVoidType() const
{
	return specificType == SpecificType::Basic && basicType == BasicType::Void;
}


bool CatGenericType::isObjectType() const
{
	return specificType == SpecificType::Object;
}


bool CatGenericType::isContainerType() const
{
	return specificType == SpecificType::Container;
}


bool CatGenericType::isVectorType() const
{
	return specificType == SpecificType::Container && containerType == ContainerType::Vector;
}


bool CatGenericType::isMapType() const
{
	return specificType == SpecificType::Container && containerType == ContainerType::StringMap;
}


bool CatGenericType::isWritable() const
{
	return writable;
}


bool CatGenericType::isConst() const
{
	return constant;
}


CatGenericType CatGenericType::toUnmodified() const
{
	return CatGenericType(specificType, basicType, nestedType, containerType, containerManipulator, false, false, error.get());
}


CatGenericType CatGenericType::toUnwritable() const
{
	return CatGenericType(specificType, basicType, nestedType, containerType, containerManipulator, false, constant, error.get());
}


CatGenericType CatGenericType::toWritable() const
{
	return CatGenericType(specificType, basicType, nestedType, containerType, containerManipulator, true, constant, error.get());
}


Reflection::ContainerManipulator* jitcat::CatGenericType::getContainerManipulator() const
{
	return containerManipulator;
}


CatGenericType CatGenericType::getContainerItemType() const
{
	if (specificType == SpecificType::Container
		&& nestedType != nullptr)
	{
		return CatGenericType(nestedType);
	}
	else
	{
		return CatGenericType(CatError("Not a container."));
	}
}


const char* CatGenericType::getObjectTypeName() const
{
	if (specificType == SpecificType::Object
		&& nestedType != nullptr)
	{
		return nestedType->getTypeName();
	}
	else
	{
		return nullptr;
	}
}


const CatError& CatGenericType::getError() const
{
	if (error.get() != nullptr)
	{
		return *error.get();
	}
	else
	{
		return CatError::defaultError;
	}
}


CatGenericType CatGenericType::getInfixOperatorResultType(CatInfixOperatorType oper, const CatGenericType& rightType)
{
	if (!rightType.isValidType())
	{
		return rightType;
	}
	else if (!isValidType())
	{
		return *this;
	}
	else if (isBasicType()
			 && rightType.isBasicType())
	{
		switch (oper)
		{
			default:
			case CatInfixOperatorType::Plus:			
				if (isStringType() || rightType.isStringType())
				{
					return CatGenericType::stringType;
				}
				//Intentional lack of break
			case CatInfixOperatorType::Minus:
			case CatInfixOperatorType::Multiply:
			case CatInfixOperatorType::Divide:
				if (isScalarType() && rightType.isScalarType())
				{
					return CatGenericType((isIntType() && rightType.isIntType()) ? BasicType::Int : BasicType::Float);
				}
				break;
			case CatInfixOperatorType::Modulo:
				if (isScalarType() && rightType.isScalarType())
				{
					return CatGenericType(basicType);
				}
				break;
			case CatInfixOperatorType::Greater:
			case CatInfixOperatorType::Smaller:
			case CatInfixOperatorType::GreaterOrEqual:
			case CatInfixOperatorType::SmallerOrEqual:
				if (isScalarType() && rightType.isScalarType())
				{
					return CatGenericType::boolType;
				}
				break;
			case CatInfixOperatorType::Equals:
			case CatInfixOperatorType::NotEquals:
				if (*this == rightType
					|| (isScalarType() && rightType.isScalarType()))
				{
					return CatGenericType::boolType;
				}
				break;
			case CatInfixOperatorType::LogicalAnd:
			case CatInfixOperatorType::LogicalOr:
				if (isBoolType() && rightType.isBoolType())
				{
					return CatGenericType::boolType;
				}
				break;
		}
	}

	return CatGenericType(Tools::append("Invalid operation: ", toString(), " ", ::toString(oper), " ", rightType.toString()));
}


std::string CatGenericType::toString() const
{
	switch (specificType)
	{
		default:						return "Unknown";
		case SpecificType::Error:		return "Error";
		case SpecificType::Basic:		return toString(basicType);
		case SpecificType::Object:		return nestedType->getTypeName();
		case SpecificType::Container:
			if (containerType == ContainerType::Vector)
			{
				return  std::string("list of ") + nestedType->getTypeName();
			}
			else if (containerType == ContainerType::StringMap)
			{
				return std::string("map of ") + nestedType->getTypeName();
			}
			else
			{
				return "Unknown";
			}
	}
}


TypeInfo* CatGenericType::getObjectType() const
{
	return nestedType;
}


std::any CatGenericType::createAnyOfType(uintptr_t pointer)
{
	switch (specificType)
	{
		case SpecificType::Basic:
		{
			switch (basicType)
			{
				case BasicType::Int:		return std::any(reinterpret_cast<int*>(pointer));
				case BasicType::Float:	return std::any(reinterpret_cast<float*>(pointer));
				case BasicType::Bool:		return std::any(reinterpret_cast<bool*>(pointer));
				case BasicType::String:	return std::any(reinterpret_cast<std::string*>(pointer));
			}
		} break;
		case SpecificType::Object:
		{
			return nestedType->getTypeCaster()->cast(pointer);
		} break;
		case SpecificType::Container:
		{
			return containerManipulator->createAnyPointer(pointer);
		} break;
	}
	return std::any();
}


std::any CatGenericType::createDefault() const
{
	switch (specificType)
	{
		case SpecificType::Basic:
		{
			switch (basicType)
			{
				case BasicType::Int:		return 0;
				case BasicType::Float:	return 0.0f;
				case BasicType::Bool:	return false;
				case BasicType::String:	return std::string();
			}
		} break;
		case SpecificType::Object:
		{
			return nestedType->getTypeCaster()->getNull();
		} break;
		case SpecificType::Container:
		{
			return containerManipulator->createAnyPointer(0);
		} break;
	}
	return std::any();
}


std::any CatGenericType::convertToType(std::any value, const CatGenericType& valueType) const
{
	if (this->operator==(valueType))
	{
		return value;
	}
	else if (isBasicType() && valueType.isBasicType())
	{
		switch(basicType)
		{
			case BasicType::Int:
			{
				switch (valueType.basicType)
				{
					case BasicType::Float:	return (int)std::any_cast<float>(value);
					case BasicType::Bool:	return std::any_cast<bool>(value) ? 1 : 0;
					case BasicType::String:	return atoi(std::any_cast<std::string>(value).c_str());
				}
			} break;
			case BasicType::Float:
			{
				switch (valueType.basicType)
				{
					case BasicType::Int:		return (float)std::any_cast<int>(value);
					case BasicType::Bool:	return std::any_cast<bool>(value) ? 1.0f : 0.0f;
					case BasicType::String:	return (float)atof(std::any_cast<std::string>(value).c_str());
				}
			} break;
			case BasicType::Bool:
			{
				switch (valueType.basicType)
				{
					case BasicType::Float:	return std::any_cast<float>(value) > 0.0f;
					case BasicType::Int:		return std::any_cast<int>(value) > 0;
					case BasicType::String:
					{
						std::string strValue = std::any_cast<std::string>(value);
						return  strValue == "true" || atoi(strValue.c_str()) > 0;
					}
				}
			} break;
			case BasicType::String:
			{
				switch (valueType.basicType)
				{
					case BasicType::Float:
					{
						std::ostringstream ss;
						ss << std::any_cast<float>(value);
						return std::string(ss.str());
					}
					case BasicType::Int:
					{
						std::ostringstream ss;
						ss << std::any_cast<int>(value);
						return std::string(ss.str());
					}
					case BasicType::Bool:		
					{
						return std::any_cast<bool>(value) ? std::string("1") : std::string("0");
					}
				}
			} break;
		}
	}
	assert(false);
	return createDefault();
}


void CatGenericType::printValue(std::any& value)
{
	switch (specificType)
	{
		case SpecificType::Basic:
		{
			switch (basicType)
			{
				case BasicType::Int:		CatLog::log(std::any_cast<int>(value)); break;
				case BasicType::Float:	CatLog::log(std::any_cast<float>(value)); break;
				case BasicType::Bool:	CatLog::log(std::any_cast<bool>(value) ? "true" : "false"); break;
				case BasicType::String:	CatLog::log("\""); CatLog::log(std::any_cast<std::string>(value)); CatLog::log("\""); break;
			}
		} break;
		case SpecificType::Object:
		{
			 CatLog::log(Tools::makeString(std::any_cast<Reflectable*>(value))); 		
		} break;
		case SpecificType::Container:
		{
			if (containerType == ContainerType::Vector)
			{
				CatLog::log("Vector of ");
				CatLog::log(getContainerItemType().toString());
			}
			else if (containerType == ContainerType::StringMap)
			{
				CatLog::log("Map of string to ");
				CatLog::log(getContainerItemType().toString());
			}
		} break;
	}
}


float CatGenericType::convertToFloat(std::any value, const CatGenericType& valueType)
{
	return std::any_cast<float>(floatType.convertToType(value, valueType));
}


int CatGenericType::convertToInt(std::any value, const CatGenericType& valueType)
{
	return std::any_cast<int>(intType.convertToType(value, valueType));
}


bool CatGenericType::convertToBoolean(std::any value, const CatGenericType& valueType)
{
	return std::any_cast<bool>(boolType.convertToType(value, valueType));
}


std::string CatGenericType::convertToString(std::any value, const CatGenericType& valueType)
{
	return std::any_cast<std::string>(stringType.convertToType(value, valueType));
}


CatGenericType CatGenericType::readFromXML(std::ifstream& xmlFile, const std::string& closingTag, std::map<std::string, TypeInfo*>& typeInfos)
{
	SpecificType specificType = SpecificType::None;
	BasicType basicType = BasicType::None;
	std::string objectTypeName = "";
	std::string containerItemTypeName = "";
	ContainerType containerType = ContainerType::None;
	bool writable = false;
	bool constant = false;
	while (true)
	{
		XMLLineType tagType;
		std::string contents;
		std::string tagName = XMLHelper::readXMLLine(xmlFile, tagType, contents);
		if (tagType == XMLLineType::OpenCloseWithContent)
		{
			if (tagName == "Type")
			{
				specificType = toSpecificType(contents.c_str());
			}
			else if (tagName == "BasicType")
			{
				basicType = toBasicType(contents.c_str());
			}
			else if (tagName == "ObjectTypeName")
			{
				objectTypeName = contents;
			}
			else if (tagName == "ContainerType")
			{
				containerType = toContainerType(contents.c_str());
			}
			else if (tagName == "ContainerItemTypeName")
			{
				containerItemTypeName = contents;
			}
			else
			{
				return CatGenericType::unknownType;
			}
		}
		else if (tagType == XMLLineType::SelfClosingTag)
		{
			if (tagName == "const")
			{
				constant = true;
			}
			else if (tagName == "writable")
			{
				writable = true;
			}
			else
			{
				return CatGenericType::unknownType;
			}
		}
		else if (tagType == XMLLineType::CloseTag && tagName == closingTag)
		{
			switch (specificType)
			{
				case SpecificType::Basic:
					if (basicType != BasicType::None)
					{
						return CatGenericType(basicType, writable, constant);
					}
					else
					{
						return CatGenericType::unknownType;
					}
					break;
				case SpecificType::Object:
					if (objectTypeName != "")
					{
						TypeInfo* objectType = XMLHelper::findOrCreateTypeInfo(objectTypeName, typeInfos);
						return CatGenericType(objectType, writable, constant);
}
					else
					{
						return CatGenericType::unknownType;
					}
					break;
				case SpecificType::Container:
					if (containerType != ContainerType::None
						&& containerItemTypeName != "")
					{
						TypeInfo* itemType = XMLHelper::findOrCreateTypeInfo(containerItemTypeName, typeInfos);
						return CatGenericType(containerType, nullptr, itemType, writable, constant);
					}
					else
					{
						return CatGenericType::unknownType;
					}
					break;
				case SpecificType::None:
					return CatGenericType();
				default: 
					return CatGenericType::unknownType;
			}
		}
		else
		{
			return CatGenericType::unknownType;
		}
	}
}


void CatGenericType::writeToXML(std::ofstream& xmlFile, const char* linePrefixCharacters)
{
	if (constant)
	{
		xmlFile << linePrefixCharacters << "<const/>\n";
	}
	if (writable)
	{
		xmlFile << linePrefixCharacters << "<writable/>\n";
	}
	if (isBasicType() || isVoidType())
	{
		xmlFile << linePrefixCharacters << "<Type>BasicType</Type>\n";		
		xmlFile << linePrefixCharacters << "<BasicType>" << toString(basicType) << "</BasicType>\n";		
	}
	else if (isContainerType())
	{
		xmlFile << linePrefixCharacters << "<Type>ContainerType</Type>\n";		
		if (isMapType())
		{
			xmlFile << linePrefixCharacters << "<ContainerType>StringMap</ContainerType>\n";		
		}
		else
		{
			xmlFile << linePrefixCharacters << "<ContainerType>Vector</ContainerType>\n";		
		}
		xmlFile << linePrefixCharacters << "<ContainerItemTypeName>" << getContainerItemType().getObjectTypeName() << "</ContainerItemTypeName>\n";		
	}
	else if (isObjectType())
	{
		xmlFile << linePrefixCharacters << "<Type>ObjectType</Type>\n";		
		xmlFile << linePrefixCharacters << "<ObjectTypeName>" << getObjectTypeName() << "</ObjectTypeName>\n";		
	}
	else
	{
		xmlFile << linePrefixCharacters << "<Type>None</Type>\n";		
	}
}


CatGenericType CatGenericType::createIntType(bool isWritable, bool isConst)
{
	return CatGenericType(BasicType::Int, isWritable, isConst);
}


CatGenericType CatGenericType::createFloatType(bool isWritable, bool isConst)
{
	return CatGenericType(BasicType::Float, isWritable, isConst);
}


CatGenericType CatGenericType::createBoolType(bool isWritable, bool isConst)
{
	return CatGenericType(BasicType::Bool, isWritable, isConst);
}


CatGenericType CatGenericType::createStringType(bool isWritable, bool isConst)
{
	return CatGenericType(BasicType::String, isWritable, isConst);
}


const char* CatGenericType::toString(BasicType type)
{
	switch (type)
	{
		case BasicType::Int:		return "int";
		case BasicType::Float:		return "float";
		case BasicType::String:		return "string";
		case BasicType::Bool:		return "bool";
		case BasicType::Void:		return "void";
		default:
		case BasicType::None:		return "none";
	}
}


CatGenericType::BasicType CatGenericType::toBasicType(const char* value)
{
	std::string str(value);
	std::size_t length = str.length();
	for (std::size_t i = 0; i < length; i++)
	{
		str[i] = std::tolower(str[i], std::locale());
	}
	for (int i = 0; i < (int)BasicType::Count; i++)
	{
		if (str == toString((BasicType)i))
		{
			return (BasicType)i;
		}
	}
	return BasicType::None;
}


const char* CatGenericType::toString(SpecificType type)
{
	switch (type)
	{
		default:
		case SpecificType::None:		return "none";
		case SpecificType::Error:		return "error";
		case SpecificType::Basic:		return "basic";
		case SpecificType::Object:		return "object";
		case SpecificType::Container:	return "container";
	}
}


CatGenericType::SpecificType CatGenericType::toSpecificType(const char * value)
{
	std::string str(value);
	std::size_t length = str.length();
	for (std::size_t i = 0; i < length; i++)
	{
		str[i] = std::tolower(str[i], std::locale());
	}
	for (int i = 0; i < (int)SpecificType::Count; i++)
	{
		if (str == toString((SpecificType)i))
		{
			return (SpecificType)i;
		}
	}
	return SpecificType::None;
}


const CatGenericType CatGenericType::intType		= CatGenericType(BasicType::Int);
const CatGenericType CatGenericType::floatType		= CatGenericType(BasicType::Float);
const CatGenericType CatGenericType::boolType		= CatGenericType(BasicType::Bool);
const CatGenericType CatGenericType::stringType		= CatGenericType(BasicType::String);
const CatGenericType CatGenericType::voidType		= CatGenericType(BasicType::Void);
const CatGenericType CatGenericType::errorType		= CatGenericType("Generic error.");
const CatGenericType CatGenericType::unknownType	= CatGenericType();
