/*
*  Copyright (c) 2026, SLikeNet contributors
*
*  This source code is  licensed under the MIT-style license found in the license.txt
*  file in the root directory of this source tree.
*/

// Split packet reassembly sizes its fragment array from the splitPacketCount of whichever
// fragment arrives first, but every fragment carries its own count and index, and parse-time
// validation only compares an index against the count in the same fragment. A later fragment
// could therefore claim a larger count and an index that was in range for itself but out of
// range for the allocation.
//
// SplitPacketSort is the container that owns that array, so these tests drive it directly -
// reaching ReliabilityLayer::InsertIntoSplitPacketList would need a live connection.
//
// Run under AddressSanitizer (SLIKENET_ENABLE_ASAN=ON) for these to be meaningful.

#include "TestHarness.h"

#include "slikenet/ReliabilityLayer.h"
#include "slikenet/InternalPacket.h"
#include "slikenet/defines.h"

#include <cstring>

using SLNet::InternalPacket;
using SLNet::SplitPacketIdType;
using SLNet::SplitPacketIndexType;

namespace
{
	/// Minimal stand-in for a received fragment; SplitPacketSort only reads these three fields.
	void InitFragment(InternalPacket &packet, SplitPacketIdType id, SplitPacketIndexType index,
		SplitPacketIndexType count)
	{
		memset(&packet, 0, sizeof(packet));
		packet.splitPacketId = id;
		packet.splitPacketIndex = index;
		packet.splitPacketCount = count;
		packet.dataBitLength = 8;
	}
}

SLN_TEST(SplitPacketSort_IndexBeyondAllocationIsRejected)
{
	// The array is sized from the first fragment's count of 4. A second fragment claiming index
	// 1000 used to be written straight into m_data[1000] - the assert guarding it compiles to
	// nothing in a release build.
	InternalPacket first;
	InitFragment(first, 0x1234, 0, 4);

	SLNet::SplitPacketSort sorter;
	sorter.Preallocate(&first, _FILE_AND_LINE_);
	SLN_CHECK(sorter.GetAllocSize() == 4);
	SLN_CHECK(sorter.Add(&first));

	InternalPacket hostile;
	InitFragment(hostile, 0x1234, 1000, 0xFFFFFFFF);

	SLN_CHECK_MSG(sorter.Add(&hostile) == false,
		"an index past the allocation must be refused, not written");
	SLN_CHECK(sorter.GetNumAddedPackets() == 1);
}

SLN_TEST(SplitPacketSort_IndexExactlyAtAllocationSizeIsRejected)
{
	InternalPacket first;
	InitFragment(first, 0x1234, 0, 4);

	SLNet::SplitPacketSort sorter;
	sorter.Preallocate(&first, _FILE_AND_LINE_);

	InternalPacket edge;
	InitFragment(edge, 0x1234, 4, 8); // index == allocation size

	SLN_CHECK_MSG(sorter.Add(&edge) == false, "index == GetAllocSize() is out of range");
}

SLN_TEST(SplitPacketSort_MismatchedPacketIdIsRejected)
{
	InternalPacket first;
	InitFragment(first, 0x1234, 0, 4);

	SLNet::SplitPacketSort sorter;
	sorter.Preallocate(&first, _FILE_AND_LINE_);

	InternalPacket wrongChannel;
	InitFragment(wrongChannel, 0x9999, 1, 4);

	SLN_CHECK_MSG(sorter.Add(&wrongChannel) == false,
		"a fragment from a different split packet id must not be accepted");
}

SLN_TEST(SplitPacketSort_WellFormedFragmentsStillReassemble)
{
	// Guards against the checks rejecting legitimate traffic.
	InternalPacket fragments[4];
	SLNet::SplitPacketSort sorter;

	InitFragment(fragments[0], 0x1234, 0, 4);
	sorter.Preallocate(&fragments[0], _FILE_AND_LINE_);

	for (SplitPacketIndexType i = 0; i < 4; ++i)
	{
		InitFragment(fragments[i], 0x1234, i, 4);
		SLN_CHECK(sorter.Add(&fragments[i]));
	}

	SLN_CHECK(sorter.AllPacketsAdded());
	SLN_CHECK(sorter.GetNumAddedPackets() == 4);
}

SLN_TEST(SplitPacketSort_DuplicateIndexIsRejected)
{
	InternalPacket first, duplicate;
	InitFragment(first, 0x1234, 2, 4);

	SLNet::SplitPacketSort sorter;
	InternalPacket sizing;
	InitFragment(sizing, 0x1234, 0, 4);
	sorter.Preallocate(&sizing, _FILE_AND_LINE_);

	SLN_CHECK(sorter.Add(&first));

	InitFragment(duplicate, 0x1234, 2, 4);
	SLN_CHECK_MSG(sorter.Add(&duplicate) == false, "the same index must not be filled twice");
	SLN_CHECK(sorter.GetNumAddedPackets() == 1);
}
