#include "cbase.h"
#include "filesystem.h"
#include "tier0/threadtools.h"
#include "econ_networking.h"
#include "econ_networking_messages.h"
#include "tier1/smartptr.h"
#include "steam/isteamnetworkingsockets.h"
#ifndef NO_STEAM
#include "steam/steamtypes.h"
#include "steam/steam_api_common.h"
#include "steam/isteamuser.h"
#endif

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
		m_eMsg = k_EInvalidMsg;
	}

	virtual MsgType_t MsgType( void ) const { return m_eMsg; }
	virtual byte const *Data( void ) const { return m_pData; }
	byte *MutableData( void ) { return m_pData; }
	virtual uint32 Size( void ) const { return m_unSize; }
	MsgHdr_t const &Hdr( void ) const { return m_Hdr; }

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
	MsgHdr_t m_Hdr;

	virtual int AddRef( void );
	virtual int Release( void );
	volatile uint m_cRefCount;
};
DEFINE_FIXEDSIZE_ALLOCATOR_MT( CNetPacket, 0, UTLMEMORYPOOL_GROW_FAST );


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef NO_STEAM

ConVar net_steamcnx_debug( "net_steamcnx_debug", "0", FCVAR_NONE, "Show debug spew for steam based connections, 2 shows all network traffic for steam sockets." );
static ConVar net_steamcnx_allowrelay( "net_steamcnx_allowrelay", "1", FCVAR_ARCHIVE, "Allow steam connections to attempt to use relay servers as fallback (best if specified on command line:  +net_steamcnx_allowrelay 1)" );


class CSocket
{
public:
	enum ESocketType_t
	{
		CLIENT	= 0,
		SERVER	= 1
	};

	EXPLICIT CSocket( CSteamID const &remoteID, ESocketType_t socketType )
	{
		m_steamID = remoteID;
		m_socketType = socketType;
	}

	static char const *NameOfSocket( ESocketType_t sock )
	{
		static char const *szNames[] ={
			"CLIENT",
			"SERVER"
		};
		return szNames[ sock ];
	}

	CSteamID const &GetSteamID( void ) const			{ return m_steamID; }
	ESocketType_t GetSocketType( void ) const			{ return m_socketType; }
	ESocketType_t GetRemoteSocketType() const			{ return ( m_socketType == CLIENT ) ? SERVER : CLIENT; }

	int GetRemoteChannel() const						{ return ( GetRemoteSocketType() == CLIENT ) ? k_EChannelClient : k_EChannelServer; }
	int GetLocalChannel() const							{ return ( GetSocketType() == CLIENT ) ? k_EChannelClient : k_EChannelServer; }
private:
	CSteamID m_steamID;
	ESocketType_t m_socketType;
};


class CEconNetworking : public CAutoGameSystemPerFrame, public IEconNetworking
{
public:
	CEconNetworking();
	virtual ~CEconNetworking();

	virtual bool Init( void );
	virtual void Shutdown( void );

	void OnClientConnected( CSteamID const &steamID ) OVERRIDE;
	void OnClientDisconnected( CSteamID const &steamID ) OVERRIDE;

	CSocket *OpenConnection( CSteamID const &steamID );
	void CloseConnection( CSocket *pSocket );
	CSocket *FindConnectionForID( CSteamID const &steamID );
	void ProcessDataFromConnection( CSocket *pSocket );

	bool SendData( CSteamID const &targetID, void const *pubData, uint32 cubData, int nChannel = 0 ) OVERRIDE;
	bool RecvData( void *pubDest, uint32 cubDest, uint32 *pcubMsgSize, CSteamID *pRemoteID, int nChannel = 0 ) OVERRIDE;

	bool BSendMessage( CSteamID const &targetID, MsgType_t eMsg, ::google::protobuf::Message const &msg ) OVERRIDE;

#ifdef CLIENT_DLL
	virtual void Update( float frametime );
#else
	virtual void PreClientUpdate( void );
#endif

	void ReceivedServerHello( CServerHelloMsg const &msg );
	void ReceivedClientHello( CClientHelloMsg const &msg );

	STEAM_CALLBACK( CEconNetworking, P2PSessionRequested, P2PSessionRequest_t, m_OnSessionRequested );
	STEAM_CALLBACK( CEconNetworking, P2PSessionFailed, P2PSessionConnectFail_t, m_OnSessionFailed );

private:
	ISteamNetworking *SteamNetworking( void ) const;

	CUtlVector< CPlainAutoPtr<CSocket> >	m_vecSockets;
	CUtlMap< MsgType_t, IMessageHandler* >	m_MessageTypes;

	friend void RegisterEconNetworkMessageHandler( MsgType_t eMsg, IMessageHandler *pHandler );
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconNetworking::CEconNetworking() : 
	CAutoGameSystemPerFrame( "EconNetworking" ),
	m_MessageTypes( DefLessFunc( MsgType_t ) ),
	m_OnSessionRequested(this, &CEconNetworking::P2PSessionRequested),
	m_OnSessionFailed(this, &CEconNetworking::P2PSessionFailed)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconNetworking::~CEconNetworking()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconNetworking::Init( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::Shutdown( void )
{
	FOR_EACH_VEC( m_vecSockets, i )
	{
		CloseConnection( m_vecSockets[i].Get() );
	}
	m_vecSockets.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: 
//
// TODO:    Get rid of the ifdefs
//-----------------------------------------------------------------------------
void CEconNetworking::OnClientConnected( CSteamID const &steamID )
{
#if defined( GAME_DLL )
	if ( !steamgameserverapicontext->SteamGameServer() )
		return;
#else
	if ( !steamapicontext->SteamUser() )
		return;
#endif

	if ( !SteamNetworking() )
		return;

	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( {255, 255, 100, 255}, "Initiate %llx\n", steamID.ConvertToUint64() );
	}

	CSocket *pSocket = OpenConnection( steamID );
	if ( pSocket )
	{
	#if defined( GAME_DLL )
		CProtobufMsg<CServerHelloMsg> msg;
		CSteamID remoteID = steamgameserverapicontext->SteamGameServer()->GetSteamID();
		MsgType_t eMsg = k_EServerHelloMsg;
	#else
		CProtobufMsg<CClientHelloMsg> msg;
		CSteamID remoteID = steamapicontext->SteamUser()->GetSteamID();
		MsgType_t eMsg = k_EClientHelloMsg;
	#endif

		uint unVersion = 0;
		FileHandle_t fh = filesystem->Open( "version.txt", "r", "MOD" );
		if ( fh && filesystem->Tell( fh ) > 0 )
		{
			char version[48];
			filesystem->ReadLine( version, sizeof( version ), fh );
			unVersion = CRC32_ProcessSingleBuffer( version, Q_strlen( version ) + 1 );
		}
		filesystem->Close( fh );

		msg.Body().set_version( unVersion );
		msg.Body().set_steamid( remoteID.ConvertToUint64() );

		BSendMessage( steamID, eMsg, msg.Body() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::OnClientDisconnected( CSteamID const &steamID )
{
	if ( !SteamNetworking() )
		return;

	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( {255, 255, 100, 255}, "Terminate %llx\n", steamID.ConvertToUint64() );
	}

	FOR_EACH_VEC_BACK( m_vecSockets, i )
	{
		if ( m_vecSockets[i]->GetSteamID() == steamID )
		{
			CloseConnection( m_vecSockets[i].Get() );
			m_vecSockets.FastRemove( i );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSocket *CEconNetworking::OpenConnection( CSteamID const &steamID )
{
	if ( !SteamNetworking() )
		return nullptr;

	FOR_EACH_VEC( m_vecSockets, i )
	{
		if ( m_vecSockets[i]->GetSteamID() == steamID )
		{
			AssertMsg( false, "Tried to open multiple connections to the same remote!" );
			return nullptr;
		}
	}

	CSocket::ESocketType_t sock = steamID.BGameServerAccount() ? CSocket::SERVER : CSocket::CLIENT;
	CSocket *pSock = new CSocket( steamID, sock );
	m_vecSockets[m_vecSockets.AddToTail()].Attach( pSock );

	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( {255, 255, 100, 255}, "Opened Steam Socket %s ( socket %d )\n", CSocket::NameOfSocket( sock ), sock );
	}

	return pSock;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::CloseConnection( CSocket *pSocket )
{
	if ( !SteamNetworking() || pSocket == NULL )
		return;

	SteamNetworking()->CloseP2PChannelWithUser( pSocket->GetSteamID(), pSocket->GetLocalChannel() );
	SteamNetworking()->CloseP2PChannelWithUser( pSocket->GetSteamID(), pSocket->GetRemoteChannel() );

	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( {255, 255, 100, 255}, "Closed Steam Socket %s\n", CSocket::NameOfSocket( pSocket->GetSocketType() ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSocket *CEconNetworking::FindConnectionForID( CSteamID const &steamID )
{
	CSocket *pSock = nullptr;
	FOR_EACH_VEC( m_vecSockets, i )
	{
		if ( m_vecSockets[i]->GetSteamID() == steamID )
		{
			pSock = m_vecSockets[i].Get();
			break;
		}
	}

	return pSock;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::ProcessDataFromConnection( CSocket *pSocket )
{
	uint32 cubMsgSize = 0; // Message size includes header
	int nChannel = pSocket->GetLocalChannel();
	while ( SteamNetworking()->IsP2PPacketAvailable( &cubMsgSize, nChannel ) )
	{
		void *pData = malloc( cubMsgSize );
		uint32 cubTotalMsgSize{}; CSteamID remoteID{};
		if ( SteamNetworking()->ReadP2PPacket( pData, cubMsgSize, &cubTotalMsgSize, &remoteID, nChannel ) )
		{
			AssertMsg( cubMsgSize == cubTotalMsgSize, "We can't handle split packets at this time." );

			CNetPacket *pPacket = new CNetPacket;
			pPacket->InitFromMemory( pData, cubMsgSize );

			CBaseMsgHandler *pHandler = NULL;
			unsigned nIndex = m_MessageTypes.Find( pPacket->MsgType() );
			if ( nIndex != m_MessageTypes.InvalidIndex() )
				pHandler = (CBaseMsgHandler*)m_MessageTypes[ nIndex ];

			if ( pHandler )
				pHandler->QueueWork( pPacket );

			pPacket->Release();
		}
		free( pData );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconNetworking::SendData( CSteamID const &targetID, void const *pubData, uint32 cubData, int nChannel )
{
	if ( !SteamNetworking() )
		return false;

	return SteamNetworking()->SendP2PPacket( targetID, pubData, cubData, k_EP2PSendReliable, nChannel );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconNetworking::RecvData( void *pubDest, uint32 cubDest, uint32 *pcubMsgSize, CSteamID *pRemoteID, int nChannel )
{
	if ( !SteamNetworking() )
		return false;

	if ( !SteamNetworking()->IsP2PPacketAvailable( pcubMsgSize, nChannel ) )
		return false;

	return SteamNetworking()->ReadP2PPacket( pubDest, cubDest, pcubMsgSize, pRemoteID, nChannel );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconNetworking::BSendMessage( CSteamID const &targetID, MsgType_t eMsg, google::protobuf::Message const &msg )
{
	if ( !SteamNetworking() )
		return false;

	CSocket *pSock = FindConnectionForID( targetID );
	if ( pSock == nullptr )
		return false;

	CNetPacket *pPacket = new CNetPacket;
	if ( pPacket == nullptr )
		return false;

	pPacket->Init( msg.ByteSize(), eMsg );
	msg.SerializeWithCachedSizesToArray( pPacket->MutableData() + sizeof(MsgHdr_t) );

	bool bSuccess = SteamNetworking()->SendP2PPacket( targetID, pPacket->Data(), pPacket->Size(), k_EP2PSendReliable, pSock->GetRemoteChannel() );

	pPacket->Release();
	return bSuccess;
}

#if defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::Update( float frametime )
{
	if ( !SteamNetworking() )
		return;

	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( !pPlayer || !engine->IsConnected() )
		return;

	FOR_EACH_VEC( m_vecSockets, i )
	{
		CSocket *pSocket = m_vecSockets[i].Get();
		ProcessDataFromConnection( pSocket );
	}
}

#else

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::PreClientUpdate()
{
	if ( !SteamNetworking() )
		return;

	FOR_EACH_VEC( m_vecSockets, i )
	{
		CSocket *pSocket = m_vecSockets[i].Get();
		ProcessDataFromConnection( pSocket );
	}
}
#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::ReceivedServerHello( CServerHelloMsg const &msg )
{
#if defined( CLIENT_DLL )
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( !pPlayer || !engine->IsConnected() )
		return;

	FileHandle_t fh = filesystem->Open( "version.txt", "r", "MOD" );
	if ( fh && filesystem->Tell( fh ) > 0 )
	{
		char version[48];
		filesystem->ReadLine( version, sizeof( version ), fh );
		uint32 unVersion = CRC32_ProcessSingleBuffer( version, Q_strlen(version)+1 );

		CProtobufMsg<CClientHelloMsg> reply;
		reply.Body().set_version( unVersion );

		CSteamID playerID;
		pPlayer->GetSteamID( &playerID );
		reply.Body().set_steamid( playerID.ConvertToUint64() );

		CSteamID serverID( msg.steamid() );
		Assert( serverID.BGameServerAccount() );

		BSendMessage( serverID, k_EClientHelloMsg, reply.Body() );
	}

	filesystem->Close( fh );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::ReceivedClientHello( CClientHelloMsg const &msg )
{
#if defined( GAME_DLL )
	FileHandle_t fh = filesystem->Open( "version.txt", "r", "MOD" );
	if ( fh && filesystem->Tell( fh ) > 0 )
	{
		char version[48];
		filesystem->ReadLine( version, sizeof( version ), fh );

		uint32 unVersion = CRC32_ProcessSingleBuffer( version, Q_strlen(version)+1 );
		if ( unVersion > 0 && msg.has_version() )
		{
			if ( msg.version() != unVersion )
			{
				engine->ServerCommand( UTIL_VarArgs( 
					"kickid %d \"The server you are trying to connect to is running a\\n different version of the game.\"\n",
					UTIL_PlayerBySteamID( msg.steamid() )->GetUserID() ) 
				);
			}
		}
	}

	filesystem->Close( fh );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Local SteamNetworking definition that
//-----------------------------------------------------------------------------
FORCEINLINE ISteamNetworking *CEconNetworking::SteamNetworking( void ) const
{
	ISteamNetworking *pNetwork = steamapicontext->SteamNetworking();
	if ( !pNetwork )
	{
	#if defined( GAME_DLL )
		pNetwork = steamgameserverapicontext->SteamGameServerNetworking();
		if ( pNetwork )
			return pNetwork;
	#endif
		return nullptr;
	}

	return pNetwork;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::P2PSessionRequested( P2PSessionRequest_t *pRequest )
{
	CSteamID remoteID = pRequest->m_steamIDRemote;
	if ( !SteamNetworking() )
		return;

#if defined( GAME_DLL )
	if ( !engine->IsDedicatedServer() )
	{
		CBasePlayer *pLocalPlayer = UTIL_GetLocalPlayerOrListenServerHost();
		if ( pLocalPlayer == NULL )
			return;

		CSteamID localID;
		pLocalPlayer->GetSteamID( &localID );

		if ( remoteID != localID )
			return;
	}
#else
	if ( !OpenConnection( remoteID ) )
		return;
#endif

	SteamNetworking()->AcceptP2PSessionWithUser( remoteID );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::P2PSessionFailed( P2PSessionConnectFail_t *pFailure )
{
	static const char *const s_szFailureReason[] ={
		"Target not running app",
		"Target doesn't own app",
		"Target isn't connected to steam",
		"Target didn't respond"
	};

	const int nFailureMessage = pFailure->m_eP2PSessionError - 1;
	ConColorMsg( {255, 255, 100, 255}, "P2P session failed: Reason %d (%s)\n", pFailure->m_eP2PSessionError, s_szFailureReason[nFailureMessage] );
}


void CNetPacket::Init( uint32 size, MsgType_t eMsg )
{
	m_Hdr = {eMsg, size};
	m_unSize = size + sizeof( MsgHdr_t );
	m_eMsg = eMsg;

	m_pData = (byte*)malloc( m_unSize );
	Q_memcpy( m_pData, &m_Hdr, sizeof( MsgHdr_t ) );

	AddRef();
}

void CNetPacket::InitFromMemory( void const *pMemory, uint32 size )
{
	Assert( pMemory );

	m_pData = (byte *)malloc( size );
	Q_memcpy( m_pData, pMemory, size );

	Q_memcpy( &m_Hdr, m_pData, sizeof( MsgHdr_t ) );

	Assert( ( m_Hdr.m_unMsgSize + sizeof( MsgHdr_t ) ) == size );
	m_eMsg = m_Hdr.m_eMsgType;
	m_unSize = size + sizeof( MsgHdr_t );

	AddRef();
}

int CNetPacket::AddRef( void )
{
	return ThreadInterlockedIncrement( &m_cRefCount );
}

int CNetPacket::Release( void )
{
	Assert( m_cRefCount > 0 );
	int nRefCounts = ThreadInterlockedDecrement( &m_cRefCount );
	if ( nRefCounts == 0 )
	{
		if( m_pData )
			free( m_pData );

		delete this;
	}

	return nRefCounts;
}

//-----------------------------------------------------------------------------
static CEconNetworking g_Networking;
IEconNetworking *g_pEconNetwork = &g_Networking;

void RegisterEconNetworkMessageHandler( MsgType_t eMsg, IMessageHandler *pHandler )
{
	g_Networking.m_MessageTypes.Insert( eMsg, pHandler );
}

class CServerHelloHandler : public CBaseMsgHandler
{
public:
	CServerHelloHandler() {}

	virtual bool ProcessPacket( INetPacket *pPacket )
	{
		CProtobufMsg<CServerHelloMsg> msg( pPacket );
		g_Networking.ReceivedServerHello( msg.Body() );

		return true;
	}
};
REG_ECON_MSG_HANDLER( CServerHelloHandler, k_EServerHelloMsg, CServerHelloMsg );

class CClientHelloHandler : public CBaseMsgHandler
{
public:
	CClientHelloHandler() {}

	virtual bool ProcessPacket( INetPacket *pPacket )
	{
		CProtobufMsg<CClientHelloMsg> msg( pPacket );
		g_Networking.ReceivedClientHello( msg.Body() );

		return true;
	}
};
REG_ECON_MSG_HANDLER( CClientHelloHandler, k_EClientHelloMsg, CClientHelloMsg );

#endif // NO_STEAM
//-----------------------------------------------------------------------------
