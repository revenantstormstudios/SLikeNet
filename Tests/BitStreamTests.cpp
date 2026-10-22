/*
*  Copyright (c) 2026, SLikeNet contributors
*
*  This source code is  licensed under the MIT-style license found in the license.txt
*  file in the root directory of this source tree.
*/

// Tests for the BitStream read primitives that every network-facing deserializer is built on.
//
// The cases here lock in behaviour that is already correct, so that later hardening work
// cannot regress it. Tests covering specific vulnerability fixes are added alongside the
// commit that fixes them.

#include "TestHarness.h"

#include "slikenet/BitStream.h"

SLN_TEST(BitStream_RoundTripsIntegers)
{
	SLNet::BitStream bs;
	bs.Write(static_cast<unsigned int>(0xDEADBEEF));
	bs.Write(static_cast<unsigned short>(0x1234));

	unsigned int readUInt = 0;
	unsigned short readUShort = 0;
	SLN_CHECK(bs.Read(readUInt));
	SLN_CHECK(readUInt == 0xDEADBEEF);
	SLN_CHECK(bs.Read(readUShort));
	SLN_CHECK(readUShort == 0x1234);
}

SLN_TEST(BitStream_ShortReadFailsAndLeavesDestinationUntouched)
{
	// A truncated payload is the single most common malicious input shape: the attacker sends
	// a message that claims a field is present but stops short. Read() must report failure AND
	// must not partially write the destination, because callers routinely ignore the result and
	// would otherwise consume an uninitialized value as a length or an index.
	SLNet::BitStream bs;
	bs.Write(static_cast<unsigned char>(0x7F));

	unsigned int destination = 0xAAAAAAAA;
	SLN_CHECK_MSG(bs.Read(destination) == false, "a 1-byte stream must not satisfy a 4-byte read");
	SLN_CHECK_MSG(destination == 0xAAAAAAAA, "failed Read() must leave the destination untouched");
}

SLN_TEST(BitStream_UnreadBitCountDoesNotUnderflowPastEnd)
{
	// IgnoreBits/IgnoreBytes/SetReadOffset do not clamp the read offset, so GetNumberOfUnreadBits()
	// is what stops an over-advanced offset from turning every later "unread < needed" guard into
	// a pass. It must saturate at zero rather than wrapping around.
	SLNet::BitStream bs;
	bs.Write(static_cast<unsigned int>(0));

	bs.IgnoreBits(1000);
	SLN_CHECK_MSG(bs.GetNumberOfUnreadBits() == 0, "unread bit count must saturate at 0, not underflow");

	unsigned int destination = 0x5A5A5A5A;
	SLN_CHECK_MSG(bs.Read(destination) == false, "reads past the end must fail");
	SLN_CHECK(destination == 0x5A5A5A5A);
}
