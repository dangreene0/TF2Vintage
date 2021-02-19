#ifndef ECON_NETWORKING_H
#define ECON_NETWORKING_H
#ifdef _WIN32
#pragma once
#endif


#ifdef COMPILER_GCC
#undef min
#undef max
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

struct MsgHdr_t
{
	MsgType_t m_eMsgType;	// Message type
	uint32 m_unMsgSize;		// Size of message without header
	EChannelType_t m_eChannel;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
abstract_class INetPacket : public IRefCounted
{
public:
	virtual MsgType_t MsgType( void ) const = 0;
	virtual byte const *Data( void ) const = 0;
	virtual uint32 Size( void ) const = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Interface for sending economy data over the wire to clients
//-----------------------------------------------------------------------------
abstract_class IEconNetworking
{
public:
	virtual void OnClientConnected( CSteamID const &steamID ) = 0;
	virtual void OnClientDisconnected( CSteamID const &steamID ) = 0;
	virtual bool SendData( CSteamID const &targetID, void const *pubData, uint32 cubData, int nChannel = 0 ) = 0;
	virtual bool RecvData( void *pubDest, uint32 cubDest, int nChannel = 0 ) = 0;
	virtual bool BSendMessage( CSteamID const &targetID, MsgType_t eMsg, google::protobuf::Message const &msg ) = 0;
};

extern IEconNetworking *g_pEconNetwork;


#endif
