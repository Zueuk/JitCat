/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace jitcat::Tools
{
	template <typename StringT>
	class StringConstants
	{
	};

	template <class TraitsT, class AllocatorT>
	class StringConstants<std::basic_string<char, TraitsT, AllocatorT>>
	{
		using StringT = std::basic_string<char, TraitsT, AllocatorT>;
		using StringStreamT = std::basic_stringstream<char, TraitsT, AllocatorT>;
	public:
		static inline const StringT empty = "";
		static inline const StringT trueStr = "true";
		static inline const StringT falseStr = "false";
		static inline const StringT oneStr = "1";
		static inline const StringT zeroStr = "0";
		static inline const char space = ' ';
		static inline const char dot = '.';
		static inline const char comma = ',';
		static inline const char zero = '0';
		static inline int stringToInt(const StringT& string)
		{
			return atoi(string.c_str());
		}
		static inline float stringToFloat(const StringT& string)
		{
			return (float)atof(string.c_str());
		}
		static inline double stringToDouble(const StringT& string)
		{
			return atof(string.c_str());
		}
		template<typename ContentT>
		static inline StringT makeString(const ContentT& content)
		{
			StringStreamT result;
			result << content;
			return result.str();
		}
	};


	template <class TraitsT, class AllocatorT>
	class StringConstants<std::basic_string<wchar_t, TraitsT, AllocatorT>>
	{
		using StringT = std::basic_string<wchar_t, TraitsT, AllocatorT>;
		using OStreamT = std::basic_ostringstream<wchar_t, TraitsT, AllocatorT>;
		using StringStreamT = std::basic_stringstream<char, TraitsT, AllocatorT>;
	public:
		static inline const StringT empty = L"";
		static inline const StringT trueStr = L"true";
		static inline const StringT falseStr = L"false";
		static inline const wchar_t space = L' ';
		static inline const wchar_t dot = L'.';
		static inline const wchar_t comma = L',';
		static inline const wchar_t one = L'1';
		static inline const wchar_t zero = L'0';
		static inline int stringToInt(const StringT& string)
		{
			OStreamT stream(string);
			int out = 0;
			out << stream;
			return out;
		}
		static inline float stringToFloat(const StringT& string)
		{
			OStreamT stream(string);
			float out = 0;
			out << stream;
			return out;
		}
		static inline double stringToDouble(const StringT& string)
		{
			OStreamT stream(string);
			double out = 0;
			out << stream;
			return out;
		}
		template<typename ContentT>
		static inline StringT makeString(const ContentT& content)
		{
			StringStreamT result;
			result << content;
			return result.str();
		}
	};


	template<typename T>
	void deleteElements(std::vector<T>& vector);

	template <typename ContainerType>
	void deleteSecondElementsAndClear(ContainerType& map);

	template <typename T1, typename T2>
	bool isInList(const std::vector<T1>& vector, const T2& element);

	template <typename T>
	T convert(const std::string& text);

	template <typename T>
	T convert(const std::wstring& text);

	template <typename T>
	T convert(const wchar_t* text);

	template <typename T>
	T convert(const char* text);

	template <typename T>
	std::string makeString(const T& content);

	template <typename T>
	std::wstring makeWString(const T& content);

	void split(const std::string& stringToSplit, const std::string& delims, std::vector<std::string>& stringsOut, bool allowEmpty = false);

	bool startsWith(const std::string& text, const std::string& prefix);

	bool isNumber(const std::string& text);

	char toUpperCase(char text);
	std::string toUpperCase(std::string text);
	std::string toUpperCase(const char* text);
	std::string toUpperCase(const std::string_view& text);
	char toLowerCase(char text);
	std::string toLowerCase(std::string text);
	std::string toLowerCase(const char* text);
	std::string toLowerCase(const std::string_view& text);

	wchar_t toUpperCase(wchar_t text);
	std::wstring toUpperCase(std::wstring text);
	std::wstring toUpperCase(const wchar_t* text);
	std::wstring toUpperCase(const std::wstring_view& text);
	wchar_t toLowerCase(wchar_t text);
	std::wstring toLowerCase(std::wstring text);
	std::wstring toLowerCase(const wchar_t* text);
	std::wstring toLowerCase(const std::wstring_view& text);

	uintptr_t alignPointer(uintptr_t pointer, std::size_t alignment);
	std::size_t roundUp(std::size_t size, std::size_t multiple);

	bool equalsWhileIgnoringCase(const std::string& text1, const std::string& text2);
	bool equalsWhileIgnoringCase(const std::string& text1, const char* text2);
	bool equalsWhileIgnoringCase(const char* text1, const std::string& text2);
	bool equalsWhileIgnoringCase(const char* text1, const char* text2);
	bool lessWhileIgnoringCase(const std::string& first, const std::string& second);

	bool equalsWhileIgnoringCase(const std::wstring& text1, const std::wstring& text2);
	bool equalsWhileIgnoringCase(const std::wstring& text1, const wchar_t* text2);
	bool equalsWhileIgnoringCase(const wchar_t* text1, const std::wstring& text2);
	bool equalsWhileIgnoringCase(const wchar_t* text1, const wchar_t* text2);
	bool lessWhileIgnoringCase(const std::wstring& first, const std::wstring& second);

	std::string toHexBytes(const unsigned char* data, int length);

	template <typename EnumT>
	constexpr int enumToInt(EnumT enumValue);

	template <typename... ValueTypes>
	std::string append(ValueTypes&&... values);

	const std::string empty = "";

} //End namespace jitcat::Tools
#include "jitcat/ToolsHeaderImplementation.h"
