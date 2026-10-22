/*
*  Copyright (c) 2026, SLikeNet contributors
*
*  This source code is  licensed under the MIT-style license found in the license.txt
*  file in the root directory of this source tree.
*/

// ReplicaManager3 and TeamManager index a worldsArray with a WorldId taken off the wire. WorldId
// is a uint8_t but the arrays held only 255 entries, so worldId 255 read one pointer past the
// end - landing on the adjacent worldsList member, whose leading listArray pointer is non-null
// and therefore passed the "is this world in use" test that guards the call sites.
//
// Run under AddressSanitizer (SLIKENET_ENABLE_ASAN=ON) for these to be meaningful.

#include "TestHarness.h"

#include "slikenet/ReplicaManager3.h"
#include "slikenet/TeamManager.h"
#include "slikenet/MessageIdentifiers.h"
#include "slikenet/types.h"

#include <cstring>

namespace
{
	/// ReplicaManager3 is abstract; the worldId paths under test never reach these.
	class TestReplicaManager3 : public SLNet::ReplicaManager3
	{
	public:
		SLNet::Connection_RM3* AllocConnection(const SLNet::SystemAddress &, SLNet::RakNetGUID) const override
		{
			return nullptr;
		}
		void DeallocConnection(SLNet::Connection_RM3 *) const override {}
	};

	void DeliverRaw(SLNet::PluginInterface2 &plugin, const unsigned char *bytes, unsigned int length)
	{
		SLNet::Packet packet;
		memset(&packet, 0, sizeof(packet));
		packet.systemAddress = SLNet::UNASSIGNED_SYSTEM_ADDRESS;
		packet.guid = SLNet::UNASSIGNED_RAKNET_GUID;
		packet.length = length;
		packet.bitSize = BYTES_TO_BITS(length);
		packet.data = const_cast<unsigned char*>(bytes);
		packet.deleteData = false;
		packet.wasGeneratedLocally = false;

		plugin.OnReceive(&packet);
	}
}

SLN_TEST(ReplicaManager3_WorldId255DoesNotReadPastWorldsArray)
{
	// Two bytes is the whole exploit: the message id plus worldId 0xFF.
	const unsigned char message[] = { ID_REPLICA_MANAGER_SERIALIZE, 0xFF };

	TestReplicaManager3 replicaManager;
	DeliverRaw(replicaManager, message, sizeof(message));

	SLN_CHECK_MSG(true, "reaching here without an ASan report is the assertion");
}

SLN_TEST(ReplicaManager3_EveryWorldIdValueIsAddressable)
{
	// The array must have an entry for every value a WorldId can hold, including 255.
	const unsigned char construction[] = { ID_REPLICA_MANAGER_CONSTRUCTION, 0xFF };
	const unsigned char download[]     = { ID_REPLICA_MANAGER_DOWNLOAD_STARTED, 0xFF };

	TestReplicaManager3 replicaManager;
	DeliverRaw(replicaManager, construction, sizeof(construction));
	DeliverRaw(replicaManager, download, sizeof(download));

	SLN_CHECK_MSG(true, "reaching here without an ASan report is the assertion");
}

SLN_TEST(ReplicaManager3_TruncatedTimestampMessageIsRejected)
{
	// The worldId sits at offset 2+sizeof(Time) on the ID_TIMESTAMP path, so a message that stops
	// one byte short of it must be refused rather than reading past the packet.
	unsigned char message[2 + sizeof(SLNet::Time)];
	memset(message, 0, sizeof(message));
	message[0] = ID_TIMESTAMP;

	TestReplicaManager3 replicaManager;
	DeliverRaw(replicaManager, message, sizeof(message));

	SLN_CHECK_MSG(true, "reaching here without an ASan report is the assertion");
}

SLN_TEST(TeamManager_WorldId255DoesNotReadPastWorldsArray)
{
	SLNet::TeamManager teamManager;
	SLN_CHECK(teamManager.GetWorldWithId(255) == 0);
	SLN_CHECK(teamManager.GetWorldWithId(254) == 0);
}

SLN_TEST(TeamManager_TruncatedTeamAssignedDoesNotUseUninitializedWorldId)
{
	// Only the message id - the worldId and member id are both absent.
	const unsigned char message[] = { ID_TEAM_BALANCER_TEAM_ASSIGNED };

	SLNet::Packet packet;
	memset(&packet, 0, sizeof(packet));
	packet.systemAddress = SLNet::UNASSIGNED_SYSTEM_ADDRESS;
	packet.guid = SLNet::UNASSIGNED_RAKNET_GUID;
	packet.length = sizeof(message);
	packet.bitSize = BYTES_TO_BITS(sizeof(message));
	packet.data = const_cast<unsigned char*>(message);

	SLNet::TeamManager teamManager;
	SLNet::TM_World *world = reinterpret_cast<SLNet::TM_World*>(~static_cast<uintptr_t>(0));
	SLNet::TM_TeamMember *member = reinterpret_cast<SLNet::TM_TeamMember*>(~static_cast<uintptr_t>(0));

	teamManager.DecodeTeamAssigned(&packet, &world, &member);

	SLN_CHECK_MSG(world == 0, "a truncated message must not select a world");
	SLN_CHECK(member == 0);
}
