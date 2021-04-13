/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#pragma once

namespace jitcat::LLVM
{
	class LLVMCodeGenerator;
}
namespace jitcat::Reflection
{
	class CustomTypeInfo;
	class FunctionSignature;
	struct MemberFunctionInfo;
	class ReflectedTypeInfo;
	class StaticConstMemberInfo;
	class Reflectable;
	class TypeInfo;
	struct TypeMemberInfo;
}
#include "jitcat/CatScopeID.h"
#include "jitcat/ReflectableHandle.h"
#include "jitcat/RuntimeContext.h"
#include "jitcat/TypeRegistry.h"

#include <any>
#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <vector>


namespace jitcat
{
	class CatRuntimeContext;
	class CatScope;
	class ErrorContext;
	class ExpressionErrorManager;
	namespace AST
	{
		class CatFunctionDefinition;
		class CatClassDefinition;
		class CatScopeBlock;
	}
	namespace Reflection
	{
		class ObjectInstance;
	}

	//A CatRuntimeContext provides variables and functions for use in expressions. (See Expression.h, ExpressionAny.h)
	//It can contain multiple "scopes" of variables.
	//Variables can come from classes that inherit from Reflectable (and implement the static functions required for reflection, see TypeInfo.h) or
	//from a CustomTypeInfo instance, which represent a struct that is defined at runtime.
	//It also provides a context for errors that are generated when expressions are compiled using a CatRuntimeContext.
	//It does this by providing a context name as well as a stack of error contexts that provide better descriptions for any errors that are generated.
	//Errors are managed by the ExpressionErrorManager. An ExpressionErrorManager can be passed to the constructor to use one ExpressionErrorManager for multiple CatRuntimeContexts.
	//If no errorManager is passed, a new one will be created.
	class CatRuntimeContext: public RuntimeContext
	{
		struct Scope
		{
			Scope(Reflection::TypeInfo* objectType, unsigned char* object, bool isStatic):
				scopeObject(object, objectType),
				isStatic(isStatic)
			{
			}
			~Scope() {};
			Reflection::ReflectableHandle scopeObject;
			bool isStatic;
		};
	public:
		//contextName is used to provide better descriptions for errors that are generated by expressions compiled using this context.
		//errorManager manages the list of errors generated by expressions. If errorManager is null, a new errorManager will be created just for this context.
		CatRuntimeContext(const std::string& contextName, ExpressionErrorManager* errorManager = nullptr);
		CatRuntimeContext(const CatRuntimeContext&) = delete;
		virtual ~CatRuntimeContext();

		//Clone this context. Does not include state variables such as currentFunctionDefinition etc.
		std::unique_ptr<CatRuntimeContext> clone() const;

		static const char* getTypeName();
		static void reflect(jitcat::Reflection::ReflectedTypeInfo& typeInfo);

		//Returns the name of the context for the purpose of error messages.
		virtual std::string getContextName() override final;

		//addScope adds a scope containing variables and/or functions that can be used by an expression. 
		//If isStatic is true, scopeObject must not be null when compiling an expression and is assumed to be "static", that is, 
		//the address of the scopeObject must not change between compile time and run time of an expression.
		//If a static scope object is deleted, any expressions that were compiled using it should either also be deleted, or recompiled using a new static scope.
		//A dynamic scope object is assumed to potentially change after each execution of an expression and may be null at compile time.
		//Variables and functions are looked up in the reverse order in which the scopes were added. The most recently added scope is searched first.
		//The scopeObject must inherit from Reflectable and it must implement the static functions required for reflection (see TypeInfo.h).
		//scopeObjects are not owned/deleted by the CatRuntimeContext.
		//Adding a scope returns a ScopeID that can later be used to remove or change the scope object.
		template<typename ReflectableType>
		CatScopeID addScope(ReflectableType* scopeObject, bool isStatic);
		//Same as above addObject, but explicitly specify type and scopeObject instead of deriving typeInfo from the scopeObject;
		CatScopeID addScope(Reflection::TypeInfo* typeInfo, unsigned char* scopeObject, bool isStatic);
		//Same as above addObject, but type and scopeObject are contained in an objectInstance
		CatScopeID addScope(const Reflection::ObjectInstance& objectInstance, bool isStatic);

		void pushStackFrame();
		void popStackFrame();


		int getNumScopes() const;
		//When a scope is removed, any expressions that were compiled using this context should be recompiled.
		void removeScope(CatScopeID id);
		//If the ScopeID refers to a static scope, any expressions that were compiled using this context should be recompiled.
		void setScopeObject(CatScopeID id, unsigned char* scopeObject);
		//Returns weither or not the provided ScopeID is a static scope.
		bool isStaticScope(CatScopeID id) const;
	
		unsigned char* getScopeObject(CatScopeID id) const;
		Reflection::TypeInfo* getScopeType(CatScopeID id) const;

		ExpressionErrorManager* getErrorManager() const;
		void pushErrorContext(ErrorContext* context);
		void popErrorContext(ErrorContext* context);

		//Tries to find a variable by name, starting from the most recently added scope and going backwards through
		//the scopes until the variable is found or there are no more scopes.
		Reflection::TypeMemberInfo* findVariable(const std::string& lowercaseName, CatScopeID& scopeId);
		//Tries to find a static variable by name, starting from the most recently added scope and going backwards through
		//the scopes until the variable is found or there are no more scopes.
		Reflection::StaticMemberInfo* findStaticVariable(const std::string& lowercaseName, CatScopeID& scopeId);
		//Tries to find a static constant by name, starting from the most recently added scope and going backwards through
		//the scopes until the variable is found or there are no more scopes.
		Reflection::StaticConstMemberInfo* findStaticConstant(const std::string& lowercaseName, CatScopeID& scopeId);
		//Tries to find a function by name, starting from the most recently added scope and going backwards through
		//the scopes until the function is found or there are no more scopes.
		Reflection::MemberFunctionInfo* findFirstMemberFunction(const std::string& lowercaseName, CatScopeID& scopeId);
		//Tries to find a function by signature, starting from the most recently added scope and going backwards through
		//the scopes until the function is found or there are no more scopes.
		Reflection::MemberFunctionInfo* findMemberFunction(const Reflection::FunctionSignature* functionSignature, CatScopeID& scopeId);
		//Tries to find a static function by signature, starting from the most recently added scope and going backwards through
		//the scopes until the function is found or there are no more scopes.
		Reflection::StaticFunctionInfo* findStaticFunction(const Reflection::FunctionSignature* functionSignature, CatScopeID& scopeId);

		Reflection::TypeInfo* findType(const std::string& lowercaseName, CatScopeID& scopeId);

		std::shared_ptr<LLVM::LLVMCodeGenerator> getCodeGenerator();
		int getNextFunctionIndex();

		void setCurrentFunction(AST::CatFunctionDefinition* function);
		AST::CatFunctionDefinition* getCurrentFunction() const;

		void setCurrentClass(AST::CatClassDefinition* function);
		AST::CatClassDefinition* getCurrentClass() const;

		void setCurrentScope(CatScope* scope);
		CatScope* getCurrentScope() const;
		unsigned  char* getCurrentScopeObject() const;

		bool getIsReturning() const;
		void setReturning(bool isReturning);

		std::any& addTemporary(const std::any& value);
		void clearTemporaries();

	private:
		CatScopeID createScope(unsigned char* scopeObject, Reflection::TypeInfo* type, bool isStatic);
		CatRuntimeContext::Scope* getScope(CatScopeID scopeId) const;

	public:
		static CatRuntimeContext defaultContext;
	private:
		int nextFunctionIndex;
		AST::CatFunctionDefinition* currentFunctionDefinition;
		AST::CatClassDefinition* currentClassDefinition;
		CatScope* currentScope;

		bool returning;

		bool ownsErrorManager;
		ExpressionErrorManager* errorManager;

		std::string contextName;

		//Scopes used for looking up symbols. Also serves as a runtime stack for the interpreter.
		std::vector<std::unique_ptr<CatRuntimeContext::Scope>> scopes;
		//A separate list of static scopes because static scopes are available accross function calls.
		std::vector<std::unique_ptr<CatRuntimeContext::Scope>> staticScopes;

		std::vector<std::unique_ptr<std::any>> temporaries;

		CatScopeID currentStackFrameOffset;
		std::vector<CatScopeID> stackFrameOffsets;

	#ifdef ENABLE_LLVM
		std::shared_ptr<LLVM::LLVMCodeGenerator> codeGenerator;
	#endif
		std::vector<ErrorContext*> errorContextStack;
	};


	template<typename ReflectableType>
	inline CatScopeID CatRuntimeContext::addScope(ReflectableType* scopeObject, bool isStatic)
	{
		static_assert(std::is_base_of<Reflection::Reflectable, ReflectableType>::value, "scopeObject must inherit from Reflectable");
		//scopeObject must not be nullptr if it is static
		assert(!isStatic || scopeObject != nullptr);
		Reflection::TypeInfo* typeInfo = Reflection::TypeRegistry::get()->registerType<ReflectableType>();
		return createScope(reinterpret_cast<unsigned char*>(scopeObject), typeInfo, isStatic);
	}

} //End namespace jitcat
