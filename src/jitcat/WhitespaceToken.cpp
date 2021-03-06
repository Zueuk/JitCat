/*
  This file is part of the JitCat library.
	
  Copyright (C) Machiel van Hooren 2018
  Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).
*/

#include "jitcat/WhitespaceToken.h"
#include "jitcat/Document.h"
#include "jitcat/Lexeme.h"

using namespace jitcat::Tokenizer;


ParseToken* WhitespaceToken::createIfMatch(Document* document, const char* currentPosition) const
{
	std::size_t offset = 0;
	std::size_t docOffset = currentPosition - document->getDocumentData().c_str();
	std::size_t documentLength = document->getDocumentSize() - docOffset;
	bool seenCarriageReturn = false;
	while (offset < documentLength
		   && (   currentPosition[offset] == ' '
			   || currentPosition[offset] == '\t'
			   || currentPosition[offset] == '\n'
			   || currentPosition[offset] == '\r'))
	{
		if (currentPosition[offset] == '\r' && !seenCarriageReturn)
		{
			seenCarriageReturn = true;
		}
		else if (currentPosition[offset] == '\r' || currentPosition[offset] == '\n')
		{
			seenCarriageReturn = false;
			document->addNewLine((int)(docOffset + offset));
		}
		offset++;
		
	}
	if (offset > 0)
	{
		return new WhitespaceToken(document->createLexeme(docOffset, offset));
	}
	else
	{
		return nullptr;
	}
}