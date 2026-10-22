/*
*  Copyright (c) 2026, SLikeNet contributors
*
*  This source code is  licensed under the MIT-style license found in the license.txt
*  file in the root directory of this source tree.
*/

// RakWString::Deserialize(wchar_t*, size_t, BitStream*) accepted the caller's buffer capacity and
// then never used it, so a 16-bit length chosen by the sender drove an unbounded write.
// RakString::Deserialize(char*, BitStream*) wrote its terminator at that sender-chosen offset
// even when the payload was too short to copy anything.
//
// Run under AddressSanitizer (SLIKENET_ENABLE_ASAN=ON) for these to be meaningful.

#include "TestHarness.h"

#include "slikenet/BitStream.h"
#include "slikenet/string.h"
#include "slikenet/wstring.h"

SLN_TEST(RakWString_LengthBeyondCapacityIsRejected)
{
	// Claim 0xFFFF wide characters against a 64-wchar_t destination. Before the fix this wrote
	// 65535 elements plus a terminator, regardless of how little payload followed.
	SLNet::BitStream bs;
	bs.WriteCasted<unsigned short, size_t>(0xFFFF);

	wchar_t destination[64];
	destination[0] = L'x';

	SLN_CHECK_MSG(SLNet::RakWString::Deserialize(destination, 64, &bs) == false,
		"a claimed length larger than the destination must be rejected");
}

SLN_TEST(RakWString_LengthExactlyFillingCapacityIsRejected)
{
	// The off-by-one: 64 characters plus a terminator does not fit in 64 wchar_t.
	SLNet::BitStream bs;
	bs.WriteCasted<unsigned short, size_t>(64);

	wchar_t destination[64];
	SLN_CHECK_MSG(SLNet::RakWString::Deserialize(destination, 64, &bs) == false,
		"length+terminator must fit within the stated capacity");
}

SLN_TEST(RakWString_WellFormedStringStillDeserializes)
{
	SLNet::RakWString source(L"hello");
	SLNet::BitStream bs;
	source.Serialize(&bs);

	wchar_t destination[64];
	SLN_CHECK(SLNet::RakWString::Deserialize(destination, 64, &bs));
	SLN_CHECK(destination[0] == L'h');
	SLN_CHECK(destination[5] == 0);
}

SLN_TEST(RakString_ShortPayloadDoesNotWriteTerminatorAtRemoteOffset)
{
	// The length says 4096 bytes but none follow. ReadAlignedBytes fails, and before the fix
	// str[4096]=0 still ran - a NUL written 4 KB past a small buffer.
	SLNet::BitStream bs;
	bs.Write(static_cast<unsigned short>(4096));

	char destination[64];
	memset(destination, 'x', sizeof(destination));

	SLN_CHECK_MSG(SLNet::RakString::Deserialize(destination, &bs) == false,
		"a truncated payload must report failure");
	SLN_CHECK_MSG(destination[0] == 0, "the failure path must terminate at offset 0");
}

SLN_TEST(RakString_UnreadableLengthDoesNotWriteAtGarbageOffset)
{
	// An empty stream leaves the length unread. It used to be uninitialized, so the terminator
	// landed at whatever offset the stack happened to hold.
	SLNet::BitStream bs;

	char destination[64];
	memset(destination, 'x', sizeof(destination));

	SLN_CHECK(SLNet::RakString::Deserialize(destination, &bs) == false);
	SLN_CHECK(destination[0] == 0);
}
