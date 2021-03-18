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

struct SteamNetworkingIdentity;
typedef uint32 MsgType_t;

#define ECON_SERVER_PORT 27200 //constexpr uint16 g_nServerPort =

// Use this to determine how a message was sent over the
// wire, if required. Don't rely on this value being present,
// or what the value is!
enum EProtocolType
{	// Currently only a TCP type is implemented
	k_EProtocolTCP		= 1,
	k_EProtocolUDP		= 2
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
	virtual void OnClientConnected( CSteamID const &steamID ) = 0;
	virtual void OnClientDisconnected( CSteamID const &steamID ) = 0;
#if defined( CLIENT_DLL )
	virtual void ConnectToServer( SteamNetworkingIdentity const &identity ) = 0;
#endif
	virtual bool SendData( CSteamID const &targetID, void const *pubData, uint32 cubData ) = 0;
	virtual bool RecvData( void *pubDest, uint32 cubDest ) = 0;
	virtual bool BSendMessage( CSteamID const &targetID, MsgType_t eMsg, google::protobuf::Message const &msg ) = 0;
};

extern IEconNetworking *g_pEconNetwork;


#endif
