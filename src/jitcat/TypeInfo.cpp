/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#include "jitcat/TypeInfo.h"
#include "jitcat/ArrayMemberFunctionInfo.h"
#include "jitcat/Configuration.h"
#include "jitcat/ContainerManipulator.h"
#include "jitcat/FunctionSignature.h"
#include "jitcat/MemberInfo.h"
#include "jitcat/MemberFunctionInfo.h"
#include "jitcat/StaticConstMemberInfo.h"
#include "jitcat/StaticMemberInfo.h"
#include "jitcat/StaticMemberFunctionInfo.h"
#include "jitcat/Tools.h"
#include "jitcat/TypeCaster.h"
#include "jitcat/TypeRegistry.h"
#include "jitcat/VariableEnumerator.h"

#include <iostream>
#include <sstream>

using namespace jitcat;
using namespace jitcat::Reflection;


TypeInfo::TypeInfo(const char* typeName, std::size_t typeSize, std::unique_ptr<TypeCaster> caster):
	typeName(typeName),
	caster(std::move(caster)),
	parentType(nullptr),
	typeSize(typeSize)
{
}


TypeInfo::~TypeInfo()
{
	for (auto& iter : members)
	{
		if ((iter.second->catType.isPointerToReflectableObjectType()
			|| iter.second->catType.isReflectableHandleType())
			&& (Tools::startsWith(iter.first, "$") || iter.second->catType.getOwnershipSemantics() == TypeOwnershipSemantics::Value))
		{
			TypeInfo* typeInfo = iter.second->catType.getPointeeType()->getObjectType();
			typeInfo->removeDependentType(this);
		}
	}
	if (parentType != nullptr)
	{
		parentType->removeType(getTypeName());
	}
	for (auto& iter : types)
	{
		iter.second->setParentType(nullptr);
	}
}


void TypeInfo::destroy(TypeInfo* type)
{
	if (type->canBeDeleted())
	{
		delete type;
	}
	else
	{
		typeDeletionList.push_back(type);
	}
	updateTypeDestruction();
}


void TypeInfo::updateTypeDestruction()
{
	bool foundDeletion = false;
	do
	{
		foundDeletion = false;
		int numTypes = (int)typeDeletionList.size();

		//Reverse iterate because that is typically the order in which dependent types are deleted.
		for (int i = numTypes - 1; i >= 0; i--)
		{
			if (typeDeletionList[i]->canBeDeleted())
			{
				delete typeDeletionList[i];
				typeDeletionList[i] = typeDeletionList.back();
				typeDeletionList.pop_back();
				foundDeletion = true;
			}
		}
	} while (foundDeletion);
}


bool TypeInfo::addType(TypeInfo* type)
{
	std::string lowercaseTypeName = Tools::toLowerCase(type->getTypeName());
	if (types.find(lowercaseTypeName) == types.end())
	{
		types[lowercaseTypeName] = type;
		type->setParentType(this);
		return true;
	}
	return false;
}


StaticConstMemberInfo* TypeInfo::addConstant(const std::string& name, const CatGenericType& type, const std::any& value)
{
	std::string lowercaseTypeName = Tools::toLowerCase(name);
	if (staticConstMembers.find(lowercaseTypeName) == staticConstMembers.end())
	{
		staticConstMembers.emplace(std::make_pair(lowercaseTypeName, std::make_unique<StaticConstMemberInfo>(name, type, value)));
		return staticConstMembers[lowercaseTypeName].get();
	}
	return nullptr;
}


void TypeInfo::setParentType(TypeInfo* type)
{
	parentType = type;
}


bool TypeInfo::removeType(const std::string& typeName)
{
	auto& iter = types.find(Tools::toLowerCase(typeName));
	if (iter != types.end())
	{
		iter->second->setParentType(nullptr);
		types.erase(iter);
		return true;
	}
	return false;
}


void TypeInfo::addDeserializedMember(TypeMemberInfo* memberInfo)
{
	std::string lowerCaseMemberName = Tools::toLowerCase(memberInfo->memberName);
	members.emplace(lowerCaseMemberName,  memberInfo);
}


void TypeInfo::addDeserializedMemberFunction(MemberFunctionInfo* memberFunction)
{
	std::string lowerCaseMemberFunctionName = Tools::toLowerCase(memberFunction->memberFunctionName);
	memberFunctions.emplace(lowerCaseMemberFunctionName, memberFunction);
}


std::size_t TypeInfo::getTypeSize() const
{
	return typeSize;
}


const CatGenericType& TypeInfo::getType(const std::string& dotNotation) const
{
	std::vector<std::string> indirectionList;
	Tools::split(dotNotation, ".", indirectionList, false);
	return getType(indirectionList, 0);
}


const CatGenericType& TypeInfo::getType(const std::vector<std::string>& indirectionList, int offset) const
{
	int indirectionListSize = (int)indirectionList.size();
	if (indirectionListSize > 0)
	{
		std::map<std::string, std::unique_ptr<TypeMemberInfo>>::const_iterator iter = members.find(indirectionList[offset]);
		if (iter != members.end())
		{
			TypeMemberInfo* memberInfo = iter->second.get();
			if (memberInfo->catType.isBasicType())
			{
				if (offset == indirectionListSize - 1)
				{
					return memberInfo->catType;
				}
			}
			else if (memberInfo->catType.isContainerType())
			{
				if (indirectionListSize > offset + 1)
				{
					offset++;
					if ((memberInfo->catType.isVectorType()
							&& Tools::isNumber(indirectionList[offset]))
						|| memberInfo->catType.isMapType())
					{
						if (offset == indirectionListSize - 1)
						{
							return memberInfo->catType;
						}
						else if (indirectionListSize > offset + 1)
						{
							return memberInfo->catType.getContainerItemType().getPointeeType()->getObjectType()->getType(indirectionList, offset + 1);
						}
					}
				}
			}
			else if (memberInfo->catType.isPointerToReflectableObjectType())
			{
				if (indirectionListSize > offset + 1)
				{
					return memberInfo->catType.getPointeeType()->getObjectType()->getType(indirectionList, offset + 1);
				}
			}
		}
	}
	return CatGenericType::unknownType;
}


TypeMemberInfo* TypeInfo::getMemberInfo(const std::string& identifier) const
{
	auto iter = members.find(Tools::toLowerCase(identifier));
	if (iter != members.end())
	{
		return iter->second.get();
	}
	else
	{
		return nullptr;
	}
}


StaticMemberInfo* TypeInfo::getStaticMemberInfo(const std::string& identifier) const
{
	auto iter = staticMembers.find(Tools::toLowerCase(identifier));
	if (iter != staticMembers.end())
	{
		return iter->second.get();
	}
	else
	{
		return nullptr;
	}
}


StaticConstMemberInfo* TypeInfo::getStaticConstMemberInfo(const std::string& identifier) const
{
	auto iter = staticConstMembers.find(Tools::toLowerCase(identifier));
	if (iter != staticConstMembers.end())
	{
		return iter->second.get();
	}
	else
	{
		return nullptr;
	}
}


MemberFunctionInfo* TypeInfo::getFirstMemberFunctionInfo(const std::string& identifier) const
{
	auto iter = memberFunctions.find(Tools::toLowerCase(identifier));
	if (iter != memberFunctions.end())
	{
		return iter->second.get();
	}
	else
	{
		return nullptr;
	}
}


MemberFunctionInfo* TypeInfo::getMemberFunctionInfo(const FunctionSignature* functionSignature) const
{
	return getMemberFunctionInfo(*functionSignature);
}


MemberFunctionInfo* TypeInfo::getMemberFunctionInfo(const FunctionSignature& functionSignature) const
{
	std::string lowerCaseFunctionName = functionSignature.getLowerCaseFunctionName();
	auto lowerBound = memberFunctions.lower_bound(lowerCaseFunctionName);
	auto upperBound = memberFunctions.upper_bound(lowerCaseFunctionName);
	for (auto& iter = lowerBound; iter != upperBound; ++iter)
	{
		if (iter->second->compare(functionSignature))
		{
			return iter->second.get();
		}
	}
	return nullptr;
}


StaticFunctionInfo* TypeInfo::getFirstStaticMemberFunctionInfo(const std::string& identifier) const
{
	auto iter = staticFunctions.find(Tools::toLowerCase(identifier));
	if (iter != staticFunctions.end())
	{
		return iter->second.get();
	}
	else
	{
		return nullptr;
	}
}


StaticFunctionInfo* TypeInfo::getStaticMemberFunctionInfo(const FunctionSignature* functionSignature) const
{
	return getStaticMemberFunctionInfo(*functionSignature);
}


StaticFunctionInfo* TypeInfo::getStaticMemberFunctionInfo(const FunctionSignature& functionSignature) const
{
	std::string lowerCaseFunctionName = functionSignature.getLowerCaseFunctionName();
	auto lowerBound = staticFunctions.lower_bound(lowerCaseFunctionName);
	auto upperBound = staticFunctions.upper_bound(lowerCaseFunctionName);
	for (auto& iter = lowerBound; iter != upperBound; ++iter)
	{
		if (iter->second->compare(functionSignature))
		{
			return iter->second.get();
		}
	}
	return nullptr;
}


TypeInfo* TypeInfo::getTypeInfo(const std::string& typeName) const
{
	auto iter = types.find(Tools::toLowerCase(typeName));
	if (iter != types.end())
	{
		return iter->second;
	}
	else
	{
		return nullptr;
	}
}


const char* TypeInfo::getTypeName() const
{
	return typeName;
}


void TypeInfo::setTypeName(const char* newTypeName)
{
	typeName = newTypeName;
}


void TypeInfo::enumerateVariables(VariableEnumerator* enumerator, bool allowEmptyStructs) const
{
	for (auto& iter : memberFunctions)
	{
		std::stringstream result;
		result << iter.second->memberFunctionName;
		result << "(";
		std::size_t numArguments = iter.second->getNumberOfArguments();
		for (std::size_t i = 0; i < numArguments; i++)
		{
			if (i > 0)
			{
				result << ", ";
			}
			result << iter.second->getArgumentType(i).toString();
		}
		result << ")";
		enumerator->addFunction(iter.second->memberFunctionName, result.str());
	}

	auto last = members.end();
	for (auto iter = members.begin(); iter != last; ++iter)
	{
		const CatGenericType& memberType = iter->second->catType;
		if (memberType.isBasicType())
		{
			std::string catTypeName = memberType.toString();
			enumerator->addVariable(iter->second->memberName, catTypeName, iter->second->catType.isWritable(), iter->second->catType.isConst());
			break;
		}
		else if (memberType.isPointerToReflectableObjectType() || memberType.isReflectableHandleType())
		{
			std::string nestedTypeName = memberType.toString();
			if (allowEmptyStructs || memberType.getPointeeType()->getObjectType()->getMembers().size() > 0)
			{
				enumerator->enterNameSpace(iter->second->memberName, nestedTypeName, NamespaceType::Object);
				if (!Tools::isInList(enumerator->loopDetectionTypeStack, nestedTypeName))
				{
					enumerator->loopDetectionTypeStack.push_back(nestedTypeName);
					memberType.getPointeeType()->getObjectType()->enumerateVariables(enumerator, allowEmptyStructs);
					enumerator->loopDetectionTypeStack.pop_back();
				}
				enumerator->exitNameSpace();
					
			}
		}
		else if (memberType.isContainerType())
		{
			std::string containerType = memberType.isMapType() ? "Map" : "List";
			std::string itemType = memberType.getContainerItemType().toString();
			enumerator->enterNameSpace(iter->second->memberName, Tools::append(containerType, ": ", itemType), memberType.isMapType() ? NamespaceType::Map : NamespaceType::Vector);
			if (!Tools::isInList(enumerator->loopDetectionTypeStack, itemType))
			{
				enumerator->loopDetectionTypeStack.push_back(itemType);
				memberType.getContainerItemType().getPointeeType()->getObjectType()->enumerateVariables(enumerator, allowEmptyStructs);
				enumerator->loopDetectionTypeStack.pop_back();
			}
			enumerator->exitNameSpace();
		}
	}
}


void TypeInfo::enumerateMemberVariables(std::function<void(const CatGenericType&, const std::string&)>& enumerator) const
{
	for (auto& iter : membersByOrdinal)
	{
		enumerator(iter.second->catType, iter.second->memberName);
	}
}


bool TypeInfo::isCustomType() const
{
	return false;
}


bool TypeInfo::isReflectedType() const
{
	return false;
}


bool TypeInfo::isArrayType() const
{
	return false;
}


bool TypeInfo::isTriviallyCopyable() const
{
	return false;
}


const std::map<std::string, std::unique_ptr<TypeMemberInfo>>& TypeInfo::getMembers() const
{
	return members;
}


const std::multimap<std::string, std::unique_ptr<MemberFunctionInfo>>& TypeInfo::getMemberFunctions() const
{
	return memberFunctions;
}


const std::map<std::string, TypeInfo*>& TypeInfo::getTypes() const
{
	return types;
}


const TypeCaster* TypeInfo::getTypeCaster() const
{
	return caster.get();
}


void TypeInfo::placementConstruct(unsigned char* buffer, std::size_t bufferSize) const
{
	assert(false);
}


unsigned char* TypeInfo::construct() const
{
	std::size_t typeSize = getTypeSize();
	unsigned char* buffer = new unsigned char[typeSize];
	if constexpr (Configuration::logJitCatObjectConstructionEvents)
	{
		std::cout << "(TypeInfo::construct) Allocated buffer of size " << std::dec << typeSize << ": " << std::hex << reinterpret_cast<uintptr_t>(buffer) << "\n";
	}
	placementConstruct(buffer, typeSize);
	return buffer;
}


void TypeInfo::destruct(unsigned char* object)
{
	placementDestruct(object, getTypeSize());
	delete[] object;
	if constexpr (Configuration::logJitCatObjectConstructionEvents)
	{
		std::cout << "(TypeInfo::destruct) Deallocated buffer of size " << std::dec << typeSize << ": " << std::hex << reinterpret_cast<uintptr_t>(object) << "\n";
	}
}


void TypeInfo::placementDestruct(unsigned char* buffer, std::size_t bufferSize)
{
	assert(false);
}


void TypeInfo::copyConstruct(unsigned char* targetBuffer, std::size_t targetBufferSize, const unsigned char* sourceBuffer, std::size_t sourceBufferSize)
{
	assert(false);
}


void TypeInfo::moveConstruct(unsigned char* targetBuffer, std::size_t targetBufferSize, unsigned char* sourceBuffer, std::size_t sourceBufferSize)
{
	assert(false);
}


void TypeInfo::toBuffer(const std::any& value, const unsigned char*& buffer, std::size_t& bufferSize) const
{
	if (caster != nullptr)
	{
		caster->toBuffer(value, buffer, bufferSize);
	}
	else
	{
		assert(false);
	}
}


bool TypeInfo::getAllowInheritance() const
{
	return true;
}


bool TypeInfo::inheritTypeCheck(CatRuntimeContext* context, AST::CatClassDefinition* childClass, ExpressionErrorManager* errorManager, void* errorContext)
{
	return true;
}


bool TypeInfo::getAllowConstruction() const
{
	return true;
}


bool TypeInfo::getAllowCopyConstruction() const
{
	return true;
}


bool TypeInfo::getAllowMoveConstruction() const
{
	return true;
}


bool TypeInfo::canBeDeleted() const
{
	return dependentTypes.size() == 0;
}


void TypeInfo::addDependentType(TypeInfo* otherType)
{
	assert(otherType != this);
	if (dependentTypes.find(otherType) == dependentTypes.end())
	{
		dependentTypes.insert(otherType);
	}
}


void TypeInfo::removeDependentType(TypeInfo* otherType)
{
	auto& iter = dependentTypes.find(otherType);
	if (iter != dependentTypes.end())
	{
		dependentTypes.erase(iter);
	}
}


void TypeInfo::addDeferredMembers(TypeMemberInfo* deferredMember)
{
	auto& deferredMembers = deferredMember->catType.getPointeeType()->getObjectType()->getMembers();
	auto& deferredMemberFunctions = deferredMember->catType.getPointeeType()->getObjectType()->getMemberFunctions();

	for (auto& member : deferredMembers)
	{
		if (member.second->visibility == MemberVisibility::Public
			|| member.second->visibility == MemberVisibility::Protected)
		{
			members.emplace(member.first, member.second->toDeferredTypeMemberInfo(deferredMember));
		}
	}
	for (auto& memberFunction : deferredMemberFunctions)
	{
		if (memberFunction.second->visibility == MemberVisibility::Public
			|| memberFunction.second->visibility == MemberVisibility::Protected)
		{
			memberFunctions.emplace(memberFunction.first, memberFunction.second->toDeferredMemberFunction(deferredMember));
		}
	}
}


void TypeInfo::addMember(const std::string& memberName, TypeMemberInfo* memberInfo)
{
	members.emplace(memberName, memberInfo);
	membersByOrdinal[memberInfo->getOrdinal()] = memberInfo;
}


void TypeInfo::renameMember(const std::string& oldMemberName, const std::string& newMemberName)
{
	auto iter = members.find(Tools::toLowerCase(oldMemberName));
	if (iter != members.end() && members.find(Tools::toLowerCase(newMemberName)) == members.end() && !iter->second->isDeferred())
	{
		std::unique_ptr<TypeMemberInfo> memberInfo = std::move(iter->second);
		memberInfo->memberName = newMemberName;
		members.erase(iter);
		std::string lowerCaseMemberName = Tools::toLowerCase(newMemberName);
		members.emplace(lowerCaseMemberName, std::move(memberInfo));
	}
}


TypeMemberInfo* TypeInfo::releaseMember(const std::string& memberName)
{
	auto& iter = members.find(memberName);
	if (iter != members.end())
	{
		TypeMemberInfo* memberInfo = iter->second.release();

		members.erase(iter);

		auto& oridinalIter = membersByOrdinal.find(memberInfo->getOrdinal());
		if (oridinalIter != membersByOrdinal.end())
		{
			membersByOrdinal.erase(oridinalIter);
		}

		return memberInfo;
	}
	return nullptr;
}


std::vector<TypeInfo*> TypeInfo::typeDeletionList = std::vector<TypeInfo*>();