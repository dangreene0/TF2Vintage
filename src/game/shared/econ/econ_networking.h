#ifndef ECON_NETWORKING_H
#define ECON_NETWORKING_H
#ifdef _WIN32
#pragma once
#endif


#include <google/protobuf/message.h>
#include "econ_messages.pb.h"
#include "tier1/mempool.h"

enum EChannelType_t
{
	k_EChannelClient	= 0,
	k_EChannelServer	= 1
};
typedef uint32 MsgType_t;

//-----------------------------------------------------------------------------
// Purpose: Interface for sending economy data over the wire to clients
//-----------------------------------------------------------------------------
abstract_class IEconNetworking
{
public:
	virtual void OnClientConnected( CSteamID const &steamID ) = 0;
	virtual void OnClientDisconnected( CSteamID const &steamID ) = 0;
	virtual bool SendData( CSteamID const &targetID, void const *pubData, uint32 cubData, int nChannel = 0 ) = 0;
	virtual bool RecvData( void *pubDest, uint32 cubDest, uint32 *pcubMsgSize, CSteamID *pRemoteID, int nChannel = 0 ) = 0;
	virtual bool BSendMessage( CSteamID const &targetID, MsgType_t eMsg, google::protobuf::Message const &msg ) = 0;
};

extern IEconNetworking *g_pEconNetwork;


#endif
