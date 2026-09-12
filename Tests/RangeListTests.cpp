/*
*  Copyright (c) 2026, SLikeNet contributors
*
*  This source code is  licensed under the MIT-style license found in the license.txt
*  file in the root directory of this source tree.
*/

// RangeList::Deserialize parses the acknowledgement and negative-acknowledgement ranges out of
// every incoming datagram, and its verdict is what ReliabilityLayer reports through the
// isMaliciousDatagram out-parameter. These tests pin down the rejection rules it relies on.

#include "TestHarness.h"

#include "slikenet/BitStream.h"
#include "slikenet/DS_RangeList.h"

namespace
{
	// Mirrors RangeList<>::Serialize's wire format for a single range.
	void WriteRange(SLNet::BitStream &bs, unsigned int minIndex, unsigned int maxIndex)
	{
		const unsigned char maxEqualToMin = (minIndex == maxIndex) ? 1 : 0;
		bs.Write(maxEqualToMin);
		bs.Write(minIndex);
		if (maxEqualToMin == 0)
			bs.Write(maxIndex);
	}
}

SLN_TEST(RangeList_AcceptsWellFormedAscendingRanges)
{
	SLNet::BitStream bs;
	bs.Write(static_cast<unsigned short>(2));
	WriteRange(bs, 10, 20);
	WriteRange(bs, 30, 40);

	DataStructures::RangeList<unsigned int> rangeList;
	SLN_CHECK(rangeList.Deserialize(&bs));
	SLN_CHECK(rangeList.ranges.Size() == 2);
}

SLN_TEST(RangeList_RejectsInvertedRange)
{
	// maxIndex < minIndex would make the caller's iteration bounds nonsensical.
	SLNet::BitStream bs;
	bs.Write(static_cast<unsigned short>(1));
	WriteRange(bs, 500, 100);

	DataStructures::RangeList<unsigned int> rangeList;
	SLN_CHECK_MSG(rangeList.Deserialize(&bs) == false, "a range with max < min must be rejected");
}

SLN_TEST(RangeList_RejectsNonAscendingRanges)
{
	// Overlapping or repeated ranges are not producible by Serialize, so they indicate a
	// hand-crafted datagram rather than a lossy network.
	SLNet::BitStream bs;
	bs.Write(static_cast<unsigned short>(2));
	WriteRange(bs, 10, 20);
	WriteRange(bs, 15, 25);

	DataStructures::RangeList<unsigned int> rangeList;
	SLN_CHECK_MSG(rangeList.Deserialize(&bs) == false, "ranges must be strictly ascending");
}

SLN_TEST(RangeList_RejectsTruncatedPayload)
{
	// The count claims two ranges but only one is present - the classic truncated-message shape.
	SLNet::BitStream bs;
	bs.Write(static_cast<unsigned short>(2));
	WriteRange(bs, 10, 20);

	DataStructures::RangeList<unsigned int> rangeList;
	SLN_CHECK_MSG(rangeList.Deserialize(&bs) == false, "a short read must fail rather than invent a range");
}
