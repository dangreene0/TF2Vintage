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
// Purpose: 
//-----------------------------------------------------------------------------
class CNetPacket : public INetPacket
{
	friend class CRefCountAccessor;
	DECLARE_FIXEDSIZE_ALLOCATOR_MT( CNetPacket );
public:
	CNetPacket()
	{
		m_pData = NULL;
		m_unSize = 0;
		m_eMsg = k_EInvalidMsg;
	}

	virtual MsgType_t MsgType( void ) const { return m_eMsg; }
	virtual byte const *Data( void ) const { return m_pData; }
	byte *MutableData( void ) { return m_pData; }
	virtual uint32 Size( void ) const { return m_unSize; }

protected:
	virtual ~CNetPacket()
	{
		Assert( m_cRefCount == 0 );
		Assert( m_pData == NULL );
	}

	friend class CEconNetworking;
	void Init( uint32 size, MsgType_t eMsg );
	void InitFromMemory( void const *pMemory, uint32 size );

private:
	MsgType_t m_eMsg;
	byte *m_pData;
	size_t m_unSize;

	virtual int AddRef( void );
	virtual int Release( void );
	volatile uint m_cRefCount;
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
	virtual bool RecvData( void *pubDest, uint32 cubDest, uint32 *pcubMsgSize, CSteamID *pRemoteID, int nChannel = 0 ) = 0;
	virtual bool BSendMessage( CSteamID const &targetID, MsgType_t eMsg, google::protobuf::Message const &msg ) = 0;
};

extern IEconNetworking *g_pEconNetwork;


#endif
