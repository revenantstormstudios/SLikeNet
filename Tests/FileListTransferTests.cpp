/*
*  Copyright (c) 2026, SLikeNet contributors
*
*  This source code is  licensed under the MIT-style license found in the license.txt
*  file in the root directory of this source tree.
*/

// FileListTransfer::OnReferencePush() writes an incoming chunk into a heap block that is sized
// from byteLengthOfThisFile, but the destination offset and the chunk length are read straight
// off the wire. Without a bound relating them to the allocation, the sender chooses both the
// write offset and the contents - an arbitrary heap write.
//
// Run under AddressSanitizer (SLIKENET_ENABLE_ASAN=ON) for these to be meaningful.

#include "TestHarness.h"

#include "slikenet/BitStream.h"
#include "slikenet/FileListTransfer.h"
#include "slikenet/FileListTransferCBInterface.h"
#include "slikenet/FileListNodeContext.h"
#include "slikenet/StringCompressor.h"
#include "slikenet/MessageIdentifiers.h"
#include "slikenet/types.h"

#include <cstring>

namespace
{
	class NullFileCallback : public SLNet::FileListTransferCBInterface
	{
	public:
		bool OnFile(OnFileStruct *) override { return true; }
		void OnFileProgress(FileProgressStruct *) override {}
	};

	/// Builds an ID_FILE_LIST_REFERENCE_PUSH body using the same primitives the sender uses,
	/// so that only the fields under test differ from a well-formed message.
	void BuildReferencePush(SLNet::BitStream &bs, unsigned short setID, unsigned int fileIndex,
		unsigned int byteLengthOfThisFile, unsigned int offset, unsigned int chunkLength,
		const char *payload, unsigned int payloadLength)
	{
		FileListNodeContext context(0, 0, 0, 0); // declared at global scope, not in SLNet

		bs.Write(static_cast<SLNet::MessageID>(ID_FILE_LIST_REFERENCE_PUSH));
		bs << context;
		bs.Write(setID);
		SLNet::StringCompressor::Instance()->EncodeString("pushed.dat", 512, &bs);
		bs.WriteCompressed(fileIndex);
		bs.WriteCompressed(byteLengthOfThisFile);
		bs.WriteCompressed(offset);
		bs.WriteCompressed(chunkLength);
		bs.Write(false); // lastChunk
		bs.WriteAlignedBytes(reinterpret_cast<const unsigned char*>(payload), payloadLength);
	}

	void DeliverToPlugin(SLNet::FileListTransfer &plugin, SLNet::BitStream &bs, SLNet::SystemAddress sender)
	{
		SLNet::Packet packet;
		memset(&packet, 0, sizeof(packet));
		packet.systemAddress = sender;
		packet.guid = SLNet::UNASSIGNED_RAKNET_GUID;
		packet.length = bs.GetNumberOfBytesUsed();
		packet.bitSize = bs.GetNumberOfBitsUsed();
		packet.data = bs.GetData();
		packet.deleteData = false;
		packet.wasGeneratedLocally = false;

		plugin.OnReceive(&packet);
	}
}

SLN_TEST(FileListTransfer_ChunkOffsetBeyondAllocationIsRejected)
{
	SLNet::StringCompressor::AddReference();

	SLNet::SystemAddress sender;
	sender.FromStringExplicitPort("127.0.0.1", 12345);

	NullFileCallback callback;
	SLNet::FileListTransfer plugin;
	const unsigned short setID = plugin.SetupReceive(&callback, false, sender);

	// A 16-byte file, but the chunk claims to start 1 MiB into it. Before the bounds check this
	// allocated 16 bytes and then memcpy'd 512 attacker-chosen bytes to block + 1 MiB.
	char payload[512];
	memset(payload, 'B', sizeof(payload));

	SLNet::BitStream bs;
	BuildReferencePush(bs, setID, 0, /*byteLengthOfThisFile*/ 16, /*offset*/ 0x00100000,
		/*chunkLength*/ sizeof(payload), payload, sizeof(payload));

	DeliverToPlugin(plugin, bs, sender);

	SLN_CHECK_MSG(true, "reaching here without an ASan report is the assertion");

	SLNet::StringCompressor::RemoveReference();
}

SLN_TEST(FileListTransfer_ChunkLongerThanTheFileIsRejected)
{
	SLNet::StringCompressor::AddReference();

	SLNet::SystemAddress sender;
	sender.FromStringExplicitPort("127.0.0.1", 12345);

	NullFileCallback callback;
	SLNet::FileListTransfer plugin;
	const unsigned short setID = plugin.SetupReceive(&callback, false, sender);

	// offset 0 is in range, but the chunk is far longer than the file it belongs to.
	char payload[512];
	memset(payload, 'C', sizeof(payload));

	SLNet::BitStream bs;
	BuildReferencePush(bs, setID, 0, /*byteLengthOfThisFile*/ 8, /*offset*/ 0,
		/*chunkLength*/ sizeof(payload), payload, sizeof(payload));

	DeliverToPlugin(plugin, bs, sender);

	SLN_CHECK_MSG(true, "reaching here without an ASan report is the assertion");

	SLNet::StringCompressor::RemoveReference();
}

SLN_TEST(FileListTransfer_ChunkLongerThanTheDatagramIsRejected)
{
	SLNet::StringCompressor::AddReference();

	SLNet::SystemAddress sender;
	sender.FromStringExplicitPort("127.0.0.1", 12345);

	NullFileCallback callback;
	SLNet::FileListTransfer plugin;
	const unsigned short setID = plugin.SetupReceive(&callback, false, sender);

	// Consistent with the file length, but the datagram carries only 4 of the claimed 4096
	// bytes, so copying chunkLength would read past the end of the receive buffer.
	char payload[4];
	memset(payload, 'D', sizeof(payload));

	SLNet::BitStream bs;
	BuildReferencePush(bs, setID, 0, /*byteLengthOfThisFile*/ 8192, /*offset*/ 0,
		/*chunkLength*/ 4096, payload, sizeof(payload));

	DeliverToPlugin(plugin, bs, sender);

	SLN_CHECK_MSG(true, "reaching here without an ASan report is the assertion");

	SLNet::StringCompressor::RemoveReference();
}
