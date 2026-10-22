/*
*  Copyright (c) 2026, SLikeNet contributors
*
*  This source code is  licensed under the MIT-style license found in the license.txt
*  file in the root directory of this source tree.
*/

// StringCompressor::DecodeString(char*, ...) returned on an unknown languageId before running
// output[0]=0. The RakString and std::string wrappers assign their scratch buffer to the output
// regardless of the return value, and that buffer is uninitialized alloca stack, so an unknown
// language id produced a "string" made of live stack contents.

#include "TestHarness.h"

#include "slikenet/BitStream.h"
#include "slikenet/StringCompressor.h"
#include "slikenet/string.h"

SLN_TEST(StringCompressor_UnknownLanguageIdTerminatesTheOutput)
{
	SLNet::StringCompressor::AddReference();

	// Fill the destination with non-NUL bytes; a correct implementation must terminate it even
	// on the failure path, rather than leaving the caller to strlen() whatever was there.
	char destination[256];
	memset(destination, 'x', sizeof(destination));

	SLNet::BitStream bs;
	SLNet::StringCompressor::Instance()->EncodeString("hello", 256, &bs, 0);

	// Only language id 0 is registered by default.
	const bool decoded = SLNet::StringCompressor::Instance()->DecodeString(
		destination, sizeof(destination), &bs, /*languageId*/ 7);

	SLN_CHECK_MSG(decoded == false, "an unregistered language id must fail");
	SLN_CHECK_MSG(destination[0] == 0, "the output must be terminated before the early return");

	SLNet::StringCompressor::RemoveReference();
}

SLN_TEST(StringCompressor_UnknownLanguageIdYieldsEmptyRakString)
{
	SLNet::StringCompressor::AddReference();

	SLNet::BitStream bs;
	SLNet::StringCompressor::Instance()->EncodeString("hello", 256, &bs, 0);

	SLNet::RakString output("previous contents");
	const bool decoded = SLNet::StringCompressor::Instance()->DecodeString(
		&output, 256, &bs, /*languageId*/ 7);

	SLN_CHECK(decoded == false);
	SLN_CHECK_MSG(output.GetLength() == 0, "a failed decode must not publish the scratch buffer");

	SLNet::StringCompressor::RemoveReference();
}

SLN_TEST(StringCompressor_KnownLanguageIdStillRoundTrips)
{
	SLNet::StringCompressor::AddReference();

	SLNet::BitStream bs;
	SLNet::StringCompressor::Instance()->EncodeString("hello", 256, &bs, 0);

	char destination[256];
	SLN_CHECK(SLNet::StringCompressor::Instance()->DecodeString(destination, sizeof(destination), &bs, 0));
	SLN_CHECK(strcmp(destination, "hello") == 0);

	SLNet::StringCompressor::RemoveReference();
}
