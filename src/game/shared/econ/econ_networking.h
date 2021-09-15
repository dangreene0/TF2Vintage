#ifndef ECON_NETWORKING_H
#define ECON_NETWORKING_H
#ifdef _WIN32
#pragma once
#endif


#ifdef COMPILER_GCC
	#undef min
	#undef max
#endif

#include "steam/isteamnetworking.h"
#include "econ_messages.pb.h"
#include "tier1/mempool.h"

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
	uint8 m_ubProtoVer;
	EProtocolType m_eProtoType;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CNetPacket : public IRefCounted
{
	DECLARE_FIXEDSIZE_ALLOCATOR_MT( CNetPacket );
public:
	CNetPacket()
	{
		m_pMsg = NULL;
		m_Hdr.m_eMsgType = k_EInvalidMsg;
	}

	byte const *Data( void ) const { return (byte*)m_pMsg; }
	byte *MutableData( void ) { return (byte*)m_pMsg; }
	uint32 Size( void ) const { return m_Hdr.m_unMsgSize; }
	MsgHdr_t const &Hdr( void ) const { return m_Hdr; }

protected:
	virtual ~CNetPacket()
	{
		Assert( m_cRefCount == 0 );
		Assert( m_pMsg == NULL );
	}

	friend class CEconNetworking;
	void Init( uint32 size, MsgType_t eMsg );
	void InitFromMemory( void const *pMemory, uint32 size );

private:
	void *m_pMsg;
	MsgHdr_t m_Hdr;

	friend class CRefCountAccessor;
	virtual int AddRef( void )
	{
		return ThreadInterlockedIncrement( &m_cRefCount );
	}
	virtual int Release( void )
	{
		Assert( m_cRefCount > 0 );
		int nRefCounts = ThreadInterlockedDecrement( &m_cRefCount );
		if ( nRefCounts == 0 )
		{
			if( m_pMsg )
				free( m_pMsg );

			delete this;
		}

		return nRefCounts;
	}
	uint m_cRefCount;
};

//-----------------------------------------------------------------------------
// Purpose: Interface for sending economy data over the wire to clients
//-----------------------------------------------------------------------------
abstract_class IEconNetworking
{
public:
	virtual void OnClientConnected( CSteamID const &id, SNetSocket_t socket ) = 0;
	virtual void OnClientDisconnected( CSteamID const &steamID ) = 0;
	virtual void ConnectToServer( long nIP, short nPort ) = 0;
	virtual bool SendMessage( CSteamID const &targetID, MsgType_t eMsg, google::protobuf::Message const &msg ) = 0;
};

extern IEconNetworking *g_pEconNetwork;


#endif
