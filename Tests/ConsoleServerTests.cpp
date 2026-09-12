/*
*  Copyright (c) 2026, SLikeNet contributors
*
*  This source code is  licensed under the MIT-style license found in the license.txt
*  file in the root directory of this source tree.
*/

// ConsoleServer::Update() copies the incoming console line into a fixed REMOTE_MAX_TEXT_INPUT
// stack buffer. The length comes straight off the wire and RakNetTransport2 imposes no limit on
// it, so without a clamp an oversized ID_TRANSPORT_STRING smashes the stack frame.
//
// These tests drive ConsoleServer through a stub transport. Run them under AddressSanitizer
// (SLIKENET_ENABLE_ASAN=ON) - without it, the oversized copy usually just scribbles over
// adjacent stack and the test still passes.

#include "TestHarness.h"

#include "slikenet/ConsoleServer.h"
#include "slikenet/TransportInterface.h"
#include "slikenet/CommandParserInterface.h"
#include "slikenet/types.h"
#include "slikenet/MemoryOverride.h"

#include <cstring>

namespace
{
	/// Hands ConsoleServer exactly one packet, built to order.
	class StubTransport : public SLNet::TransportInterface
	{
	public:
		StubTransport() : m_pending(nullptr) {}
		~StubTransport() override { if (m_pending != nullptr) DeallocatePacket(m_pending); }

		/// Queues a packet of \a length bytes. The buffer is allocated to exactly \a length bytes
		/// with no terminator, matching what RakNetTransport2::OnReceive produces.
		void QueueUnterminated(const char *contents, unsigned int length)
		{
			m_pending = SLNet::OP_NEW<SLNet::Packet>(_FILE_AND_LINE_);
			m_pending->length = length;
			m_pending->bitSize = BYTES_TO_BITS(length);
			m_pending->systemAddress = SLNet::UNASSIGNED_SYSTEM_ADDRESS;
			m_pending->guid = SLNet::UNASSIGNED_RAKNET_GUID;
			m_pending->deleteData = false;
			m_pending->wasGeneratedLocally = false;
			m_pending->data = static_cast<unsigned char*>(rakMalloc_Ex(length, _FILE_AND_LINE_));
			memcpy(m_pending->data, contents, length);
		}

		bool Start(unsigned short, bool) override { return true; }
		void Stop(void) override {}
		void Send(SLNet::SystemAddress, const char *, ...) override {}
		void CloseConnection(SLNet::SystemAddress) override {}

		SLNet::Packet* Receive(void) override
		{
			SLNet::Packet *next = m_pending;
			m_pending = nullptr;
			return next;
		}

		void DeallocatePacket(SLNet::Packet *packet) override
		{
			if (packet == nullptr)
				return;
			rakFree_Ex(packet->data, _FILE_AND_LINE_);
			SLNet::OP_DELETE(packet, _FILE_AND_LINE_);
		}

		SLNet::SystemAddress HasNewIncomingConnection(void) override { return SLNet::UNASSIGNED_SYSTEM_ADDRESS; }
		SLNet::SystemAddress HasLostConnection(void) override { return SLNet::UNASSIGNED_SYSTEM_ADDRESS; }
		SLNet::CommandParserInterface* GetCommandParser(void) override { return nullptr; }

	private:
		SLNet::Packet *m_pending;
	};
}

SLN_TEST(ConsoleServer_OversizedLineDoesNotOverflowStackBuffer)
{
	// 64 KB of console text against a 2048-byte buffer. Before the clamp this wrote the whole
	// 64 KB over the stack frame, including the return address.
	const unsigned int oversizedLength = 64 * 1024;
	char *oversized = static_cast<char*>(rakMalloc_Ex(oversizedLength, _FILE_AND_LINE_));
	memset(oversized, 'A', oversizedLength);

	StubTransport transport;
	transport.QueueUnterminated(oversized, oversizedLength);

	SLNet::ConsoleServer consoleServer;
	consoleServer.SetTransportProvider(&transport, 0);
	consoleServer.Update();

	rakFree_Ex(oversized, _FILE_AND_LINE_);
	SLN_CHECK_MSG(true, "reaching here without an ASan report is the assertion");
}

SLN_TEST(ConsoleServer_UnterminatedPacketIsNotWalkedPastItsEnd)
{
	// RakNetTransport2 allocates exactly p->length bytes and copies exactly p->length bytes, so
	// the payload has no NUL. Parsing it as a C string reads past the end of the allocation;
	// ASan catches that here only because the buffer is sized exactly to the contents.
	const char *unterminated = "somecommand";
	const unsigned int length = static_cast<unsigned int>(strlen(unterminated));

	StubTransport transport;
	transport.QueueUnterminated(unterminated, length);

	SLNet::ConsoleServer consoleServer;
	consoleServer.SetTransportProvider(&transport, 0);
	consoleServer.Update();

	SLN_CHECK_MSG(true, "reaching here without an ASan report is the assertion");
}
