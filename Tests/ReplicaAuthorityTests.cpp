/*
*  Copyright (c) 2026, SLikeNet contributors
*
*  This source code is  licensed under the MIT-style license found in the license.txt
*  file in the root directory of this source tree.
*/

// ReplicaManager3 reads creatingSystemGUID out of a construction message and then uses it as an
// ownership token, and destroys whatever NetworkID a destruction message names, neither checked
// against the sender. Both travel in ID_REPLICA_MANAGER_CONSTRUCTION, so SetAuthorityMode() gates
// them together.

#include "TestHarness.h"

#include "slikenet/BitStream.h"
#include "slikenet/ReplicaManager3.h"
#include "slikenet/MessageIdentifiers.h"
#include "slikenet/types.h"

#include <cstring>

namespace
{
	/// Records whether a construction message got past the authority gate.
	class CountingReplicaManager3 : public SLNet::ReplicaManager3
	{
	public:
		CountingReplicaManager3() : allocConnectionCalls(0) {}

		SLNet::Connection_RM3* AllocConnection(const SLNet::SystemAddress &, SLNet::RakNetGUID) const override
		{
			++allocConnectionCalls;
			return nullptr;
		}
		void DeallocConnection(SLNet::Connection_RM3 *) const override {}

		mutable int allocConnectionCalls;
	};

	SLNet::RakNetGUID MakeGuid(uint64_t value)
	{
		SLNet::RakNetGUID guid;
		guid.g = value;
		return guid;
	}

	// Taken as the base type: ReplicaManager3 re-declares OnReceive as protected.
	SLNet::PluginReceiveResult DeliverConstruction(SLNet::PluginInterface2 &manager, SLNet::RakNetGUID sender)
	{
		// worldId 0 is created by the constructor, so this reaches the dispatch.
		unsigned char message[2] = { ID_REPLICA_MANAGER_CONSTRUCTION, 0 };

		SLNet::Packet packet;
		memset(&packet, 0, sizeof(packet));
		packet.systemAddress = SLNet::UNASSIGNED_SYSTEM_ADDRESS;
		packet.guid = sender;
		packet.length = sizeof(message);
		packet.bitSize = BYTES_TO_BITS(sizeof(message));
		packet.data = message;

		return manager.OnReceive(&packet);
	}
}

SLN_TEST(ReplicaAuthority_DefaultsToNoEnforcement)
{
	// The default must preserve existing behaviour, since changing it silently would break
	// peer-to-peer and relaying users of the library.
	CountingReplicaManager3 manager;
	SLN_CHECK(manager.GetAuthorityMode() == SLNet::RM3AM_NONE);
}

SLN_TEST(ReplicaAuthority_LocalAuthorityRejectsRemoteConstruction)
{
	// An authoritative server creates and destroys everything, so a client sending construction
	// or destruction must get nowhere.
	CountingReplicaManager3 manager;
	manager.SetAuthorityMode(SLNet::RM3AM_LOCAL_AUTHORITY);

	const SLNet::PluginReceiveResult result = DeliverConstruction(manager, MakeGuid(1234));

	SLN_CHECK_MSG(result == SLNet::RR_STOP_PROCESSING_AND_DEALLOCATE,
		"a local authority must refuse remote construction outright");
	SLN_CHECK(manager.allocConnectionCalls == 0);
}

SLN_TEST(ReplicaAuthority_RemoteAuthorityRejectsNonAuthoritySender)
{
	// On a client, only the server may construct. Another client must not.
	const SLNet::RakNetGUID serverGuid = MakeGuid(1);
	const SLNet::RakNetGUID otherClientGuid = MakeGuid(2);

	CountingReplicaManager3 manager;
	manager.SetAuthorityMode(SLNet::RM3AM_REMOTE_AUTHORITY, serverGuid);

	const SLNet::PluginReceiveResult result = DeliverConstruction(manager, otherClientGuid);

	SLN_CHECK_MSG(result == SLNet::RR_STOP_PROCESSING_AND_DEALLOCATE,
		"construction from a system that is not the declared authority must be refused");
	SLN_CHECK(manager.allocConnectionCalls == 0);
}

SLN_TEST(ReplicaAuthority_RemoteAuthorityAcceptsTheAuthority)
{
	// The authority itself must still get through, otherwise replication stops entirely. It is
	// refused later for having no connection, which is past the authority gate.
	const SLNet::RakNetGUID serverGuid = MakeGuid(1);

	CountingReplicaManager3 manager;
	manager.SetAuthorityMode(SLNet::RM3AM_REMOTE_AUTHORITY, serverGuid);

	const SLNet::PluginReceiveResult result = DeliverConstruction(manager, serverGuid);

	SLN_CHECK_MSG(result != SLNet::RR_STOP_PROCESSING_AND_DEALLOCATE,
		"the declared authority must not be gated out");
}

SLN_TEST(ReplicaAuthority_ModeIsReportedBack)
{
	CountingReplicaManager3 manager;
	manager.SetAuthorityMode(SLNet::RM3AM_LOCAL_AUTHORITY);
	SLN_CHECK(manager.GetAuthorityMode() == SLNet::RM3AM_LOCAL_AUTHORITY);

	manager.SetAuthorityMode(SLNet::RM3AM_REMOTE_AUTHORITY, MakeGuid(9));
	SLN_CHECK(manager.GetAuthorityMode() == SLNet::RM3AM_REMOTE_AUTHORITY);
}
