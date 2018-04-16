/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#pragma once

#include "MemberInfo.h"
#include "TypeRegistry.h"


template <typename T>
class MemberTypeInfoCreator
{
public:
	template<typename ClassType>
	static inline TypeMemberInfo* getMemberInfo(const std::string& memberName, T ClassType::* member, bool isConst, bool isWritable) 
	{
		static_assert(std::is_base_of<Reflectable, T>::value, "Unsupported reflectable type.");
		TypeInfo* nestedType = TypeRegistry::get()->registerType<T>();
		return new ClassObjectMemberInfo<ClassType, T>(memberName, member, nestedType, isConst, isWritable);
	}
};


template <>
class MemberTypeInfoCreator<void>
{
public:
	template<typename ClassType>
	static inline TypeMemberInfo* getMemberInfo(const std::string& memberName, int ClassType::* member, bool isConst, bool isWritable) { return nullptr;}
};


template <>
class MemberTypeInfoCreator<float>
{
public:
	template<typename ClassType>
	static TypeMemberInfo* getMemberInfo(const std::string& memberName, float ClassType::* member, bool isConst, bool isWritable) 
	{
		return new BasicTypeMemberInfo<ClassType, float>(memberName, member, CatType::Float, isConst, isWritable);
	}
};


template <>
class MemberTypeInfoCreator<int>
{
public:
	template<typename ClassType>
	static TypeMemberInfo* getMemberInfo(const std::string& memberName, int ClassType::* member, bool isConst, bool isWritable) 
	{
		return new BasicTypeMemberInfo<ClassType, int>(memberName, member, CatType::Int, isConst, isWritable);
	}
};


template <>
class MemberTypeInfoCreator<bool>
{
public:
	template<typename ClassType>
	static TypeMemberInfo* getMemberInfo(const std::string& memberName, bool ClassType::* member, bool isConst, bool isWritable) 
	{
		return new BasicTypeMemberInfo<ClassType, bool>(memberName, member, CatType::Bool, isConst, isWritable);
	}
};


template <>
class MemberTypeInfoCreator<std::string>
{
public:
	template<typename ClassType>
	static TypeMemberInfo* getMemberInfo(const std::string& memberName, std::string ClassType::* member, bool isConst, bool isWritable) 
	{
		return new BasicTypeMemberInfo<ClassType, std::string>(memberName, member, CatType::String, isConst, isWritable);
	}
};


template <typename U>
class MemberTypeInfoCreator<std::unique_ptr<U>>
{
public:
	template<typename ClassType>
	static TypeMemberInfo* getMemberInfo(const std::string& memberName, std::unique_ptr<U> ClassType::* member, bool isConst, bool isWritable) 
	{
		TypeInfo* nestedType = TypeRegistry::get()->registerType<U>();
		return new ClassUniquePtrMemberInfo<ClassType, U>(memberName, member, nestedType, isConst, isWritable);
	}
};


template <typename U>
class MemberTypeInfoCreator<U*>
{
public:
	template<typename ClassType>
	static TypeMemberInfo* getMemberInfo(const std::string& memberName, U* ClassType::* member, bool isConst, bool isWritable) 
	{
		TypeInfo* nestedType = TypeRegistry::get()->registerType<U>();
		return new ClassPointerMemberInfo<ClassType, U>(memberName, member, nestedType, isConst, isWritable);
	}
};


template <typename ItemType>
class MemberTypeInfoCreator<std::vector<ItemType> >
{
public:
	template<typename ClassType>
	static TypeMemberInfo* getMemberInfo(const std::string& memberName, std::vector<ItemType> ClassType::* member, bool isConst, bool isWritable) 
	{
		TypeInfo* nestedType = TypeTraits<ItemType>::getTypeInfo();
		return new ContainerMemberInfo<ClassType, std::vector<ItemType> >(memberName, member, ContainerType::Vector, nestedType, isConst);
	}
};


template <typename ItemType>
class MemberTypeInfoCreator<std::map<std::string, ItemType> >
{
public:
	template<typename ClassType>
	static TypeMemberInfo* getMemberInfo(const std::string& memberName, std::map<std::string, ItemType> ClassType::* member, bool isConst, bool isWritable) 
	{
		TypeInfo* nestedType = TypeTraits<ItemType>::getTypeInfo();
		return new ContainerMemberInfo<ClassType, std::map<std::string, ItemType> >(memberName, member, ContainerType::StringMap, nestedType, isConst);
	}
};
