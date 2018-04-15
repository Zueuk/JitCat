/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#include "ExpressionErrorManager.h"
#include "Tools.h"
#include "TypeInfo.h"


ExpressionErrorManager::ExpressionErrorManager(std::function<void(const std::string&)> errorHandler):
	errorsRevision(0),
	errorHandler(errorHandler)
{
}


ExpressionErrorManager::~ExpressionErrorManager()
{
	clear();
}


void ExpressionErrorManager::clear()
{
	Tools::deleteElements(errors);
	errors.clear();
	errorsRevision++;
}


void ExpressionErrorManager::compiledWithError(const std::string& errorMessage, void* expression)
{
	deleteErrorsFromExpression(expression);

	Error* error = new Error();
	error->message = errorMessage;
	error->expression = expression;
	errors.push_back(error);
	errorsRevision++;

	if (errorHandler)
	{
		errorHandler(errorMessage);
	}
}


void ExpressionErrorManager::compiledWithoutErrors(void* expression)
{
	deleteErrorsFromExpression(expression);
}


void ExpressionErrorManager::expressionDeleted(void* expression)
{
	deleteErrorsFromExpression(expression);
}


const std::vector<ExpressionErrorManager::Error*>& ExpressionErrorManager::getErrors() const
{
	return errors;
}


unsigned int ExpressionErrorManager::getErrorsRevision() const
{
	return errorsRevision;
}


void ExpressionErrorManager::reflect(TypeInfo& typeInfo)
{
	typeInfo.addMember("errors", &ExpressionErrorManager::errors);
}


const char* ExpressionErrorManager::getTypeName()
{
	return "ExpressionErrorManager";
}


void ExpressionErrorManager::deleteErrorsFromExpression(void* expression)
{
	for (int i = 0; i < (int)errors.size(); i++)
	{
		if (errors[(unsigned int)i]->expression == expression)
		{
			delete errors[(unsigned int)i];
			errors.erase(errors.begin() + i);
			errorsRevision++;
			i--;
		}
	}
}


void ExpressionErrorManager::Error::reflect(TypeInfo& typeInfo)
{
	typeInfo.addMember("message", &ExpressionErrorManager::Error::message);
}


const char* ExpressionErrorManager::Error::getTypeName()
{
	return "ExpressionError";
}