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
#include "steam/steamnetworkingtypes.h"

#define ECON_SERVER_PORT 27200


typedef uint32 MsgType_t;
// Currently only using protobuf
enum EProtocolType
{
	k_EProtocolStruct		= 1,
	k_EProtocolProtobuf		= 2
};

struct MsgHdr_t
{
	MsgType_t m_eMsgType;	// Message type
	uint32 m_unMsgSize;		// Size of message without header
	CProtobufMsgHdr *m_ProtoHdr;
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
	virtual CProtobufMsgHdr const &Hdr( void ) const = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Interface for sending economy data over the wire to clients
//-----------------------------------------------------------------------------
abstract_class IEconNetworking
{
public:
	virtual void OnClientConnected( SteamNetworkingIdentity const &identity, HSteamNetConnection hConnection ) = 0;
	virtual void OnClientDisconnected( CSteamID const &steamID ) = 0;
#if defined( CLIENT_DLL )
	virtual void ConnectToServer( SteamNetworkingIdentity const &identity ) = 0;
#endif
	virtual bool SendMessage( CSteamID const &targetID, MsgType_t eMsg, google::protobuf::Message const &msg ) = 0;
};

extern IEconNetworking *g_pEconNetwork;


#endif
