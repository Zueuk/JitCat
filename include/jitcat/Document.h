/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#pragma once

#include <cstddef>
#include <memory>

namespace jitcat::Tokenizer
{

	class Document
	{
	public: 
		Document(const char* fileData, std::size_t fileSize);
		~Document();
		const char* getDocumentData() const;
		std::size_t getDocumentSize() const;
	private:
		std::unique_ptr<char[]> data;
		std::size_t size;
	};

} //End namespace jitcat::Tokenizer