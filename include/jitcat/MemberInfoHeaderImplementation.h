/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#pragma once

#include "LLVMCodeGeneratorHelper.h"
#include "LLVMCompileTimeContext.h"
#include "LLVMTypes.h"
#include "MemberInfo.h"
#include "Tools.h"


template<typename T, typename U>
inline std::any ContainerMemberInfo<T, U>::getMemberReference(Reflectable* base)
{
	T* baseObject = static_cast<T*>(base);
	if (baseObject != nullptr)
	{
		U& container = baseObject->*memberPointer;
		return &container;
	}
	return (U*)nullptr;
}


template<typename T, typename U>
inline llvm::Value* ContainerMemberInfo<T, U>::generateDereferenceCode(llvm::Value* parentObjectPointer, LLVMCompileTimeContext* context) const
{
#ifdef ENABLE_LLVM
	static_assert(sizeof(memberPointer) == 4 || sizeof(memberPointer) == 8, "Expected a 4 or 8 byte member pointer. Object may use virtual inheritance which is not supported.");
	unsigned long long offset = 0;
	if constexpr (sizeof(memberPointer) == 4)
	{
		unsigned int smallOffset = 0;
		memcpy(&smallOffset, &memberPointer, 4);
		offset = smallOffset;
	}
	else
	{
		memcpy(&offset, &memberPointer, 8);
	}
	auto notNullCodeGen = [=](LLVMCompileTimeContext* compileContext)
	{
		llvm::Value* memberOffset = context->helper->createIntPtrConstant(offset, "offsetTo_" + memberName);
		llvm::Value* parentObjectPointerInt = context->helper->convertToIntPtr(parentObjectPointer, memberName + "_Parent_IntPtr");
		llvm::Value* addressValue = context->helper->createAdd(parentObjectPointerInt, memberOffset, memberName + "_IntPtr");
		return context->helper->convertToPointer(addressValue, memberName + "_Ptr");
	};
	return context->helper->createOptionalNullCheckSelect(parentObjectPointer, notNullCodeGen, LLVMTypes::pointerType, context);
#else 
	return nullptr;
#endif // ENABLE_LLVM
}


template<typename T, typename U>
template<typename ContainerItemType>
inline ContainerItemType ContainerMemberInfo<T, U>::getMapIntIndex(std::map<std::string, ContainerItemType>* map, int index)
{
	int count = 0;
	for (auto& iter : (*map))
	{
		if (count == index)
		{
			return iter.second;
		}
		count++;
	}
	return nullptr;
}


template<typename T, typename U>
template<typename ContainerItemType>
inline ContainerItemType ContainerMemberInfo<T, U>::getMapStringIndex(std::map<std::string, ContainerItemType>* map, std::string* index)
{
	std::string lowerCaseIdx = Tools::toLowerCase(*index);
	auto iter = map->find(lowerCaseIdx);
	if (iter != map->end())
	{
		return iter->second;
	}
	return nullptr;
}


template<typename T, typename U>
template<typename ContainerItemType>
inline ContainerItemType ContainerMemberInfo<T, U>::getVectorIndex(std::vector<ContainerItemType>* vector, int index)
{
	if (index >= 0 && index < (int)vector->size())
	{
		return vector->operator[](index);
	}
	return nullptr;
}


template<typename T, typename U>
template<typename ContainerItemType>
inline llvm::Value* ContainerMemberInfo<T, U>::generateIndex(std::map<std::string, ContainerItemType>* map, llvm::Value* containerPtr, llvm::Value* index, LLVMCompileTimeContext* context) const
{
#ifdef ENABLE_LLVM
	if (context->helper->isStringPointer(index))
	{
		auto notNullCodeGen = [=](LLVMCompileTimeContext* compileContext)
		{
			static auto functionPointer = &ContainerMemberInfo<T, U>::getMapStringIndex<ContainerItemType>;
			return compileContext->helper->createCall(LLVMTypes::functionRetPtrArgPtr_StringPtr, reinterpret_cast<uintptr_t>(functionPointer), {containerPtr, index}, "getMapStringIndex");
		};
		return context->helper->createOptionalNullCheckSelect(containerPtr, notNullCodeGen, LLVMTypes::getLLVMType<ContainerItemType>(), context);
	}
	else
	{
		auto notNullCodeGen = [=](LLVMCompileTimeContext* compileContext)
		{
			static auto functionPointer = &ContainerMemberInfo<T, U>::getMapIntIndex<ContainerItemType>;
			return compileContext->helper->createCall(LLVMTypes::functionRetPtrArgPtr_Int, reinterpret_cast<uintptr_t>(functionPointer), {containerPtr, index}, "getMapIntIndex");
		};
		return context->helper->createOptionalNullCheckSelect(containerPtr, notNullCodeGen, LLVMTypes::getLLVMType<ContainerItemType>(), context);
	}
#else
	return nullptr;
#endif //ENABLE_LLVM
}


template<typename T, typename U>
template<typename ContainerItemType>
inline llvm::Value* ContainerMemberInfo<T, U>::generateIndex(std::vector<ContainerItemType>* vector, llvm::Value* containerPtr, llvm::Value* index, LLVMCompileTimeContext* context) const
{
#ifdef ENABLE_LLVM
	auto notNullCodeGen = [=](LLVMCompileTimeContext* compileContext)
	{
		static auto functionPointer = &ContainerMemberInfo<T, U>::getVectorIndex<ContainerItemType>;
		return compileContext->helper->createCall(LLVMTypes::functionRetPtrArgPtr_Int, reinterpret_cast<uintptr_t>(functionPointer), {containerPtr, index}, "getVectorIndex");
	};
	return context->helper->createOptionalNullCheckSelect(containerPtr, notNullCodeGen, LLVMTypes::getLLVMType<ContainerItemType>(), context);
#else
	return nullptr;
#endif //ENABLE_LLVM
}


template<typename T, typename U>
inline llvm::Value* ContainerMemberInfo<T, U>::generateArrayIndexCode(llvm::Value* container, llvm::Value* index, LLVMCompileTimeContext* context) const
{
	//Index can either be an int or a string
	//container is a pointer to a vector or a map (of type T)
	U* nullContainer = nullptr;
	return generateIndex(nullContainer, container, index, context);
}


template<typename T, typename U>
inline std::any ClassPointerMemberInfo<T, U>::getMemberReference(Reflectable* base)
{
	T* baseObject = static_cast<T*>(base);
	if (baseObject != nullptr)
	{
		U* member = baseObject->*memberPointer;
		return static_cast<Reflectable*>(member);
	}
	return static_cast<Reflectable*>(nullptr);
}


template<typename T, typename U>
inline llvm::Value* ClassPointerMemberInfo<T, U>::generateDereferenceCode(llvm::Value* parentObjectPointer, LLVMCompileTimeContext* context) const
{
#ifdef ENABLE_LLVM
	static_assert(sizeof(memberPointer) == 4 || sizeof(memberPointer) == 8, "Expected a 4 or 8 byte member pointer. Object may use virtual inheritance which is not supported.");
	unsigned long long offset = 0;
	if constexpr (sizeof(memberPointer) == 4)
	{
		unsigned int smallOffset = 0;
		memcpy(&smallOffset, &memberPointer, 4);
		offset = smallOffset;
	}
	else
	{
		memcpy(&offset, &memberPointer, 8);
	}
	auto notNullCodeGen = [=](LLVMCompileTimeContext* compileContext)
	{
		llvm::Value* memberOffset = context->helper->createIntPtrConstant(offset, "offsetTo_" + memberName);
		llvm::Value* parentObjectPointerInt = context->helper->convertToIntPtr(parentObjectPointer, memberName + "_Parent_IntPtr");
		llvm::Value* addressValue = context->helper->createAdd(parentObjectPointerInt, memberOffset, memberName + "_IntPtr");
		return context->helper->loadPointerAtAddress(addressValue, memberName);
	};
	return context->helper->createOptionalNullCheckSelect(parentObjectPointer, notNullCodeGen, LLVMTypes::pointerType, context);
#else 
	return nullptr;
#endif // ENABLE_LLVM
}


template<typename T, typename U>
inline std::any ClassObjectMemberInfo<T, U>::getMemberReference(Reflectable* base)
{
	T* baseObject = static_cast<T*>(base);
	if (baseObject != nullptr)
	{
		return static_cast<Reflectable*>(&(baseObject->*memberPointer));
	}
	return static_cast<Reflectable*>(nullptr);
}


template<typename T, typename U>
inline llvm::Value* ClassObjectMemberInfo<T, U>::generateDereferenceCode(llvm::Value* parentObjectPointer, LLVMCompileTimeContext* context) const
{
#ifdef ENABLE_LLVM
	static_assert(sizeof(memberPointer) == 4 || sizeof(memberPointer) == 8, "Expected a 4 or 8 byte member pointer. Object may use virtual inheritance which is not supported.");
	unsigned long long offset = 0;
	if constexpr (sizeof(memberPointer) == 4)
	{
		unsigned int smallOffset = 0;
		memcpy(&smallOffset, &memberPointer, 4);
		offset = smallOffset;
	}
	else
	{
		memcpy(&offset, &memberPointer, 8);
	}
	auto notNullCodeGen = [=](LLVMCompileTimeContext* compileContext)
	{
		llvm::Value* memberOffset = context->helper->createIntPtrConstant(offset, "offsetTo_" + memberName);
		llvm::Value* parentObjectPointerInt = context->helper->convertToIntPtr(parentObjectPointer, memberName + "_Parent_IntPtr");
		llvm::Value* addressValue = context->helper->createAdd(parentObjectPointerInt, memberOffset, memberName + "_Ptr");
		return context->helper->convertToPointer(addressValue, memberName);
	};
	return context->helper->createOptionalNullCheckSelect(parentObjectPointer, notNullCodeGen, LLVMTypes::pointerType, context);
#else 
	return nullptr;
#endif // ENABLE_LLVM
}


template<typename T, typename U>
inline U* ClassUniquePtrMemberInfo<T, U>::getPointer(T* parentObject, ClassUniquePtrMemberInfo<T, U>* info)
 {
	std::unique_ptr<U> T::* memberPointer = info->memberPointer;
	return (parentObject->*memberPointer).get();
}


template<typename T, typename U>
inline std::any ClassUniquePtrMemberInfo<T, U>::getMemberReference(Reflectable* base)
{
	T* baseObject = static_cast<T*>(base);
	if (baseObject != nullptr)
	{
		return static_cast<Reflectable*>((baseObject->*memberPointer).get());
	}
	return static_cast<Reflectable*>(nullptr);
}


template<typename T, typename U>
inline llvm::Value* ClassUniquePtrMemberInfo<T, U>::generateDereferenceCode(llvm::Value* parentObjectPointer, LLVMCompileTimeContext* context) const
{
#ifdef ENABLE_LLVM
	llvm::Value* thisPointerAsInt = context->helper->createIntPtrConstant(reinterpret_cast<uintptr_t>(this), "ClassUniquePtrMemberInfoIntPtr");
	if (!context->helper->isPointer(parentObjectPointer))
	{
		parentObjectPointer = context->helper->convertToPointer(parentObjectPointer, memberName + "_Parent_Ptr");
	}
	auto notNullCodeGen = [=](LLVMCompileTimeContext* compileContext)
	{
		llvm::Value* thisPointer = context->helper->convertToPointer(thisPointerAsInt, "ClassUniquePtrMemberInfoPtr");
		return context->helper->createCall(LLVMTypes::functionRetPtrArgPtr_Ptr, reinterpret_cast<uintptr_t>(&ClassUniquePtrMemberInfo<T,U>::getPointer), {parentObjectPointer, thisPointer}, "getUniquePtr");
	};
	return context->helper->createOptionalNullCheckSelect(parentObjectPointer, notNullCodeGen, LLVMTypes::pointerType, context);
#else 
	return nullptr;
#endif // ENABLE_LLVM
}


template<typename T, typename U>
inline std::any BasicTypeMemberInfo<T, U>::getMemberReference(Reflectable* base)
{
	T* objectPointer = static_cast<T*>(base);
	if (objectPointer != nullptr)
	{
		U& value = objectPointer->*memberPointer;
		return value;
	}
	return U();
}


template<typename T, typename U>
inline llvm::Value* BasicTypeMemberInfo<T, U>::generateDereferenceCode(llvm::Value* parentObjectPointer, LLVMCompileTimeContext* context) const
{
#ifdef ENABLE_LLVM
	static_assert(sizeof(memberPointer) == 4 || sizeof(memberPointer) == 8, "Expected a 4 or 8 byte member pointer. Object may use virtual inheritance which is not supported.");
	unsigned long long offset = 0;
	if constexpr (sizeof(memberPointer) == 4)
	{
		unsigned int smallOffset = 0;
		memcpy(&smallOffset, &memberPointer, 4);
		offset = smallOffset;
	}
	else
	{
		memcpy(&offset, &memberPointer, 8);
	}
	auto notNullCodeGen = [=](LLVMCompileTimeContext* compileContext)
	{	
		llvm::Value* memberOffset = context->helper->createIntPtrConstant(offset, "offsetTo_" + memberName);
		llvm::Value* parentObjectPointerInt = context->helper->convertToIntPtr(parentObjectPointer, memberName + "_Parent_IntPtr");
		llvm::Value* addressValue = context->helper->createAdd(parentObjectPointerInt, memberOffset, memberName + "_IntPtr");
		if constexpr (std::is_same<U, std::string>::value)
		{
			//std::string case (returns a pointer to the std::string)
			return context->helper->convertToPointer(addressValue, memberName, LLVMTypes::stringPtrType);
		}
		else
		{
			//int, bool, float case	(returns by value)
			return context->helper->loadBasicType(context->helper->toLLVMType(catType), addressValue, memberName);
		}
	};
	return context->helper->createOptionalNullCheckSelect(parentObjectPointer, notNullCodeGen, context->helper->toLLVMType(catType), context);
#else 
	return nullptr;
#endif // ENABLE_LLVM
}