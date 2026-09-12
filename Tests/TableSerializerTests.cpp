/*
*  Copyright (c) 2026, SLikeNet contributors
*
*  This source code is  licensed under the MIT-style license found in the license.txt
*  file in the root directory of this source tree.
*/

// TableSerializer::DeserializeRow() indexed row->cells and columns with a 32-bit cellIndex taken
// straight off the wire. DataStructures::List only range-checks operator[] under _DEBUG, so a
// remote peer could name an arbitrary Cell*, which DeserializeCell then frees and writes through.
//
// Run under AddressSanitizer (SLIKENET_ENABLE_ASAN=ON) for these to be meaningful.

#include "TestHarness.h"

#include "slikenet/BitStream.h"
#include "slikenet/DS_Table.h"
#include "slikenet/StringCompressor.h"
#include "slikenet/TableSerializer.h"

namespace
{
	/// Writes a one-column, one-row table, using \a cellIndex for the row's single cell.
	void BuildTable(SLNet::BitStream &bs, unsigned int cellIndex, unsigned int numEntries)
	{
		// Columns: count, then per column the name and type.
		bs.Write(static_cast<unsigned int>(1));
		SLNet::StringCompressor::Instance()->EncodeString("value", 32, &bs);
		bs.Write(static_cast<unsigned char>(DataStructures::Table::NUMERIC));

		// Rows.
		bs.Write(static_cast<unsigned int>(1)); // rowSize
		bs.Write(static_cast<unsigned int>(0)); // key
		bs.Write(numEntries);
		bs.Write(cellIndex);
		bs.Write(static_cast<unsigned char>(1)); // cell is non-empty
		bs.Write(static_cast<double>(1.0));
	}
}

SLN_TEST(TableSerializer_OutOfRangeCellIndexIsRejected)
{
	SLNet::StringCompressor::AddReference();

	SLNet::BitStream bs;
	BuildTable(bs, /*cellIndex*/ 0xFFFFFFFF, /*numEntries*/ 1);

	DataStructures::Table table;
	SLN_CHECK_MSG(SLNet::TableSerializer::DeserializeTable(&bs, &table) == false,
		"a cellIndex past the end of the row must be rejected, not indexed");

	SLNet::StringCompressor::RemoveReference();
}

SLN_TEST(TableSerializer_CellIndexJustPastTheEndIsRejected)
{
	// The off-by-one case: the table has exactly one column, so index 1 is one past the end.
	SLNet::StringCompressor::AddReference();

	SLNet::BitStream bs;
	BuildTable(bs, /*cellIndex*/ 1, /*numEntries*/ 1);

	DataStructures::Table table;
	SLN_CHECK_MSG(SLNet::TableSerializer::DeserializeTable(&bs, &table) == false,
		"cellIndex == columns.Size() is out of range");

	SLNet::StringCompressor::RemoveReference();
}

SLN_TEST(TableSerializer_MoreEntriesThanColumnsIsRejected)
{
	SLNet::StringCompressor::AddReference();

	SLNet::BitStream bs;
	BuildTable(bs, /*cellIndex*/ 0, /*numEntries*/ 5000);

	DataStructures::Table table;
	SLN_CHECK_MSG(SLNet::TableSerializer::DeserializeTable(&bs, &table) == false,
		"a row cannot hold more cells than the table has columns");

	SLNet::StringCompressor::RemoveReference();
}

SLN_TEST(TableSerializer_WellFormedTableStillDeserializes)
{
	// Guards against the bounds checks being too strict.
	SLNet::StringCompressor::AddReference();

	SLNet::BitStream bs;
	BuildTable(bs, /*cellIndex*/ 0, /*numEntries*/ 1);

	DataStructures::Table table;
	SLN_CHECK(SLNet::TableSerializer::DeserializeTable(&bs, &table));
	SLN_CHECK(table.GetColumns().Size() == 1);
	SLN_CHECK(table.GetRows().Size() == 1);

	SLNet::StringCompressor::RemoveReference();
}
