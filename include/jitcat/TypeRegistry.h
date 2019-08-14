/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#pragma once

namespace jitcat
{
	class CatGenericType;
}
#include "jitcat/Tools.h"
#include "jitcat/TypeCaster.h"

#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>


namespace jitcat::Reflection
{
	class ReflectedTypeInfo;
	class TypeInfo;


	class TypeRegistry
	{
	private:
		TypeRegistry();
		TypeRegistry(const TypeRegistry&);
		~TypeRegistry();

	public:
		static TypeRegistry* get();
		static void recreate();

		//Returns nullptr if type wasn't found, type names are case sensitive
		TypeInfo* getTypeInfo(const std::string& typeName);
		//Never returns nullptr, creates a new empty TypeInfo if typeName does not exist.
		TypeInfo* getOrCreateTypeInfo(const char* typeName, std::size_t typeSize, TypeCaster* caster, bool allowConstruction,
									  bool allowCopyConstruction, bool allowMoveConstruction, bool triviallyCopyable,
									  std::function<void(unsigned char* buffer, std::size_t bufferSize)>& placementConstructor,
									  std::function<void(unsigned char* targetBuffer, std::size_t targetBufferSize, const unsigned char* sourceBuffer, std::size_t sourceBufferSize)>& copyConstructor,
									  std::function<void(unsigned char* targetBuffer, std::size_t targetBufferSize, unsigned char* sourceBuffer, std::size_t sourceBufferSize)>& moveConstructor,
									  std::function<void(unsigned char* buffer, std::size_t bufferSize)>& placementDestructor);

		const std::map<std::string, TypeInfo*>& getTypes() const;
	
		//If the type is already registered, it will just return the TypeInfo.
		//Never returns nullptr
		template<typename T>
		TypeInfo* registerType();
		void registerType(const char* typeName, TypeInfo* typeInfo);

		void removeType(const char* typeName);
		void renameType(const std::string& oldName, const char* newTypeName);

		//Type registry loaded this way is not sutable for executing expressions, only for expression syntax and type checking
		bool loadRegistryFromXML(const std::string& filepath);
		//Exports all registered types to XML. Intended for use in external tools.
		void exportRegistyToXML(const std::string& filepath);

	private:
		//This function exists to prevent circular includes via TypeInfo.h
		static ReflectedTypeInfo* createTypeInfo(const char* typeName, std::size_t typeSize, TypeCaster* typeCaster, bool allowConstruction,
												 bool allowCopyConstruction, bool allowMoveConstruction, bool triviallyCopyable,
												 std::function<void(unsigned char* buffer, std::size_t bufferSize)>& placementConstructor,
												 std::function<void(unsigned char* targetBuffer, std::size_t targetBufferSize, const unsigned char* sourceBuffer, std::size_t sourceBufferSize)>& copyConstructor,
												 std::function<void(unsigned char* targetBuffer, std::size_t targetBufferSize, unsigned char* sourceBuffer, std::size_t sourceBufferSize)>& moveConstructor,
												 std::function<void(unsigned char* buffer, std::size_t bufferSize)>& placementDestructor);
		static TypeInfo* castToTypeInfo(ReflectedTypeInfo* reflectedTypeInfo);

	private:
		std::map<std::string, TypeInfo*> types;
		std::vector<TypeInfo*> ownedTypes;
		static TypeRegistry* instance;
	};


	template<typename ReflectableCVT>
	inline jitcat::Reflection::TypeInfo* jitcat::Reflection::TypeRegistry::registerType()
	{
		typedef typename std::remove_cv<ReflectableCVT>::type ReflectableT;

		jitcat::Reflection::TypeInfo* typeInfo = nullptr;
		//A compile error on this line usually means that there was an attempt to reflect a type that is not reflectable (or an unsupported basic type).
		const char* typeName = ReflectableT::getTypeName();
		std::string lowerTypeName = Tools::toLowerCase(typeName);
		std::map<std::string, TypeInfo*>::iterator iter = types.find(lowerTypeName);
		if (iter != types.end())
		{
			return iter->second;
		}
		else
		{

			std::function<void(unsigned char* buffer, std::size_t bufferSize)> placementConstructor;
			std::function<void(unsigned char* targetBuffer, std::size_t targetBufferSize, const unsigned char* sourceBuffer, std::size_t sourceBufferSize)> copyConstructor;
			std::function<void(unsigned char* targetBuffer, std::size_t targetBufferSize, unsigned char* sourceBuffer, std::size_t sourceBufferSize)> moveConstructor;
			std::function<void(unsigned char* buffer, std::size_t bufferSize)> placementDestructor;
			std::size_t typeSize = sizeof(ReflectableT);
			jitcat::Reflection::ObjectTypeCaster<ReflectableT>* typeCaster = new jitcat::Reflection::ObjectTypeCaster<ReflectableT>();

			constexpr bool isConstructible = std::is_default_constructible<ReflectableT>::value
											 && std::is_destructible<ReflectableT>::value;
			if constexpr (isConstructible)
			{
				placementConstructor = [](unsigned char* buffer, std::size_t bufferSize) {assert(sizeof(ReflectableT) <= bufferSize); new(buffer) ReflectableT();};
			}
			else
			{
				placementConstructor = [](unsigned char* buffer, std::size_t bufferSize) {};
			}

			constexpr bool isCopyConstructible = std::is_copy_constructible<ReflectableT>::value;
			if constexpr (isCopyConstructible)
			{
				copyConstructor = [](unsigned char* targetBuffer, std::size_t targetBufferSize, const unsigned char* sourceBuffer, std::size_t sourceBufferSize)
								  {
									new(targetBuffer) ReflectableT(*reinterpret_cast<const ReflectableT*>(sourceBuffer));
								  };
			}
			else
			{
				copyConstructor = [](unsigned char* targetBuffer, std::size_t targetBufferSize, const unsigned char* sourceBuffer, std::size_t sourceBufferSize) {};
			}

			constexpr bool isMoveConstructible = std::is_move_constructible<ReflectableT>::value;
			if constexpr (isMoveConstructible)
			{
				moveConstructor = [](unsigned char* targetBuffer, std::size_t targetBufferSize, unsigned char* sourceBuffer, std::size_t sourceBufferSize)
								  {
									new(targetBuffer) ReflectableT(std::move(*reinterpret_cast<ReflectableT*>(sourceBuffer)));
								  };
			}
			else
			{
				moveConstructor = [](unsigned char* targetBuffer, std::size_t targetBufferSize, unsigned char* sourceBuffer, std::size_t sourceBufferSize) {};
			}

			if constexpr (std::is_destructible<ReflectableT>::value)
			{
				placementDestructor = [](unsigned char* buffer, std::size_t bufferSize){reinterpret_cast<ReflectableT*>(buffer)->~ReflectableT();};
			}
			else
			{
				placementDestructor = [](unsigned char* buffer, std::size_t bufferSize){};
			}

			//When a type within JitCat is triviallyCopyable it implies that it is also trivially destructible.
			//This is not always true for C++ types and so we also need to check for is_trivially_destructible.
			constexpr bool triviallyCopyable = std::is_trivially_copyable<ReflectableT>::value && std::is_trivially_destructible<ReflectableT>::value;

			jitcat::Reflection::ReflectedTypeInfo* reflectedInfo = createTypeInfo(typeName, typeSize, typeCaster, isConstructible, isCopyConstructible || triviallyCopyable, isMoveConstructible || triviallyCopyable, triviallyCopyable,
																				  placementConstructor, copyConstructor, moveConstructor, placementDestructor);
			typeInfo = castToTypeInfo(reflectedInfo);
			types[lowerTypeName] = typeInfo;
			ownedTypes.push_back(typeInfo);
			ReflectableT::reflect(*reflectedInfo);
			return typeInfo;
		}
	}


} //End namespace jitcat::Reflection