/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2020
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#include "jitcat/TypeInfoDeleter.h"
#include "jitcat/TypeInfo.h"


void jitcat::Reflection::TypeInfoDeleter::operator()(TypeInfo* typeInfo) const
{
	TypeInfo::destroy(typeInfo);
}
