/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#pragma once

#include <vector>


class CatLogListener
{
public:
	virtual void catLog(const char* message) = 0;
};


class CatLog
{
private:
	CatLog();
	~CatLog();
public:
	static void log(const char* message);
	template <typename T>
	static void log(T message);
	static void addListener(CatLogListener* listener);
	static void removeListener(CatLogListener* listener);

private:
	static std::vector<CatLogListener*> listeners;
};

#include <sstream>

template <typename T>
void CatLog::log(T message)
{
	std::stringstream stream;
	stream << message;
	log(stream.str().c_str());
}