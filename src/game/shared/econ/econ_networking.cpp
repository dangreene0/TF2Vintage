#include "cbase.h"
#include "filesystem.h"
#include "tier0/threadtools.h"
#include "econ_networking.h"
#include "econ_networking_messages.h"
#include "tier1/smartptr.h"
#include "steam/isteamnetworkingutils.h"
#include "steam/isteamnetworkingsockets.h"
#include "tier0/icommandline.h"
#ifndef NO_STEAM
#include "steam/steamtypes.h"
#include "steam/steam_api_common.h"
#include "steam/isteamuser.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar net_steamcnx_debug( "net_steamcnx_debug", "0", FCVAR_NONE, "Show debug spew for steam based connections, 2 shows all network traffic for steam sockets." );
static ConVar net_steamcnx_allowrelay( "net_steamcnx_allowrelay", "1", FCVAR_ARCHIVE, "Allow steam connections to attempt to use relay servers as fallback (best if specified on command line:  +net_steamcnx_allowrelay 1)" );

#define STEAM_CNX_COLOR Color(255,255,100)
#define STEAM_CNX_PROTO_VERSION 2

static const char *RenderSteamId( CSteamID const &steamID )
{
	static char szId[64];

	SteamNetworkingIdentity identity;
	identity.SetSteamID( steamID );
	identity.ToString( szId, sizeof( szId ) );

	return szId;
}


class CSteamSocket
{
public:
	EXPLICIT CSteamSocket( CSteamID const &remoteID, HSteamNetConnection hConnection )
	{
		m_identity.SetSteamID( remoteID );
		m_hConn = hConnection;
	}

	CSteamID const &GetSteamID( void ) const				{ return m_identity.GetSteamID(); }
	HSteamNetConnection GetConnection( void ) const			{ return m_hConn; }
	SteamNetworkingIdentity const &GetIdentity( void ) const { return m_identity; }

private:
	HSteamNetConnection m_hConn;
	SteamNetworkingIdentity m_identity;
};


class CEconNetworking : public CAutoGameSystemPerFrame, public IEconNetworking
{
	friend void RegisterEconNetworkMessageHandler( MsgType_t eMsg, IMessageHandler *pHandler );
public:
	CEconNetworking();
	virtual ~CEconNetworking();

	virtual bool Init( void );
	virtual void Shutdown( void );

	void OnClientConnected( CSteamID const &steamID ) OVERRIDE;
	void OnClientDisconnected( CSteamID const &steamID ) OVERRIDE;

#if defined( GAME_DLL )
	CSteamSocket *OpenConnection( CSteamID const &steamID, HSteamNetConnection hConnection );
	void CloseConnection( CSteamSocket *pSocket );
	CSteamSocket *FindConnectionForID( CSteamID const &steamID );
#else
	void ConnectToServer( SteamNetworkingIdentity const &identity ) OVERRIDE;
#endif

	void ProcessDataFromConnection( HSteamNetConnection hConn );

	bool SendData( CSteamID const &targetID, void const *pubData, uint32 cubData ) OVERRIDE;
	bool RecvData( void *pubDest, uint32 cubDest ) OVERRIDE;

	bool BSendMessage( CSteamID const &targetID, MsgType_t eMsg, ::google::protobuf::Message const &msg ) OVERRIDE;

#ifdef CLIENT_DLL
	virtual void Update( float frametime );
#else
	virtual void PreClientUpdate( void );
#endif

	void SessionStatusChanged( SteamNetConnectionStatusChangedCallback_t *pStatus );

private:
#if defined( GAME_DLL )
	HSteamListenSocket						m_hListenSocket;
	CUtlVector< CSteamSocket >				m_vecSockets;
#else
	HSteamNetConnection						m_hServerConnection;
#endif

	HSteamNetPollGroup						m_hPollGroup;
	CUtlMap< MsgType_t, IMessageHandler* >	m_MessageTypes;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CNetPacket : public INetPacket
{
	friend class CRefCountAccessor;
	static CUtlMemoryPool *sm_MsgPool;
	static bool sm_bRegisteredPool;
#pragma push_macro("new")
#undef new
	DECLARE_FIXEDSIZE_ALLOCATOR_MT( CNetPacket );
#pragma pop_macro("new")
public:
	CNetPacket()
	{
		m_pMsg = NULL;
		m_Hdr.m_eMsgType = k_EInvalidMsg;
	}

	virtual MsgType_t MsgType( void ) const { return m_Hdr.m_eMsgType; }
	virtual byte const *Data( void ) const { return (byte*)m_pMsg->m_pData; }
	byte *MutableData( void ) { return (byte*)m_pMsg->m_pData; }
	virtual uint32 Size( void ) const { return m_pMsg->m_cbSize; }
	CProtobufMsgHdr const &Hdr( void ) const { return *m_Hdr.m_ProtoHdr; }

protected:
	virtual ~CNetPacket()
	{
		Assert( m_cRefCount == 0 );
		Assert( m_pMsg == NULL );

		if( m_Hdr.m_ProtoHdr )
			FreeProtoHdr( m_Hdr.m_ProtoHdr );
	}

	static CProtobufMsgHdr *AllocProtoHdr( void )
	{
		if ( !sm_bRegisteredPool )
		{
			Assert( sm_MsgPool == NULL );
			sm_MsgPool = new CUtlMemoryPool( sizeof( CProtobufMsgHdr ), 1 );
			sm_bRegisteredPool = true;
		}

		CProtobufMsgHdr *pMsg = (CProtobufMsgHdr *)sm_MsgPool->Alloc();
		Construct<CProtobufMsgHdr>( pMsg );
		return pMsg;
	}

	static void FreeProtoHdr( CProtobufMsgHdr *pObj )
	{
		Destruct<CProtobufMsgHdr>( pObj );
		sm_MsgPool->Free( (void *)pObj );
	}

	friend class CEconNetworking;
	void Init( uint32 size, MsgType_t eMsg )
	{
		m_Hdr = {eMsg, size};

		m_Hdr.m_ProtoHdr = AllocProtoHdr();
		m_Hdr.m_ProtoHdr->set_protocol_version( STEAM_CNX_PROTO_VERSION );
		m_Hdr.m_ProtoHdr->set_protocol_type( k_EProtocolTCP );

		m_pMsg = SteamNetworkingUtils()->AllocateMessage( size + sizeof( MsgHdr_t ) );
		Q_memcpy( m_pMsg->m_pData, &m_Hdr, sizeof( MsgHdr_t ) );

		AddRef();
	}
	void InitFromMemory( void const *pMemory, uint32 size )
	{
		Assert( pMemory );

		m_pMsg = SteamNetworkingUtils()->AllocateMessage( size );
		Q_memcpy( m_pMsg->m_pData, pMemory, size );

		Q_memcpy( &m_Hdr, m_pMsg->m_pData, sizeof( MsgHdr_t ) );

		Assert( ( m_Hdr.m_unMsgSize + sizeof( MsgHdr_t ) ) == size );

		AddRef();
	}

private:
	SteamNetworkingMessage_t *m_pMsg;
	MsgHdr_t m_Hdr;

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
				m_pMsg->Release();

			delete this;
		}

		return nRefCounts;
	}
	uint m_cRefCount;
};
DEFINE_FIXEDSIZE_ALLOCATOR_MT( CNetPacket, 0, UTLMEMORYPOOL_GROW_FAST );

static void NetworkingSessionStatusChanged(SteamNetConnectionStatusChangedCallback_t *);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconNetworking::CEconNetworking() : 
	CAutoGameSystemPerFrame( "EconNetworking" ),
	m_MessageTypes( DefLessFunc( MsgType_t ) ),
	m_hPollGroup( k_HSteamNetPollGroup_Invalid ),
#if defined( GAME_DLL )
	m_hListenSocket( k_HSteamListenSocket_Invalid )
#else
	m_hServerConnection( k_HSteamNetConnection_Invalid )
#endif
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
	if( net_steamcnx_allowrelay.GetBool() )
		SteamNetworkingUtils()->InitRelayNetworkAccess();

	SteamNetworkingUtils()->SetGlobalCallback_SteamNetConnectionStatusChanged( NetworkingSessionStatusChanged );

#if defined( GAME_DLL )
	if( engine->IsDedicatedServer() )
	{
		SteamNetworkingConfigValue_t config;
		config.SetInt32( k_ESteamNetworkingConfig_SymmetricConnect, 1 );

		SteamNetworkingIPAddr localAdr;
		localAdr.SetIPv6LocalHost( ECON_SERVER_PORT );
		m_hListenSocket = SteamNetworkingSockets()->CreateListenSocketIP( localAdr, 1, &config );
	}
#endif

	m_hPollGroup = SteamNetworkingSockets()->CreatePollGroup();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::Shutdown( void )
{
#if defined( GAME_DLL )
	FOR_EACH_VEC( m_vecSockets, i )
	{
		CloseConnection( &m_vecSockets[i] );
	}
	m_vecSockets.Purge();
#endif

	if( SteamNetworkingSockets() )
		SteamNetworkingSockets()->DestroyPollGroup( m_hPollGroup );
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

	if ( !SteamNetworkingSockets() )
		return;

	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( STEAM_CNX_COLOR, "Initiate %llx\n", steamID.ConvertToUint64() );
	}

	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::OnClientDisconnected( CSteamID const &steamID )
{
	if ( !SteamNetworkingSockets() )
		return;

	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( STEAM_CNX_COLOR, "Terminate %llx\n", steamID.ConvertToUint64() );
	}

#if defined( GAME_DLL )
	FOR_EACH_VEC_BACK( m_vecSockets, i )
	{
		if ( m_vecSockets[i].GetSteamID() == steamID )
		{
			CloseConnection( &m_vecSockets[i] );
			m_vecSockets.FastRemove( i );
		}
	}
#endif
}

#if defined( GAME_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSteamSocket *CEconNetworking::OpenConnection( CSteamID const &steamID, HSteamNetConnection hConnection )
{
	if ( !SteamNetworkingSockets() )
		return nullptr;

#if defined( CLIENT_DLL )
	m_hServerConnection = hConnection;
#endif

	FOR_EACH_VEC( m_vecSockets, i )
	{
		if ( m_vecSockets[i].GetSteamID() == steamID )
		{
			AssertMsg( false, "Tried to open multiple connections to the same remote!" );
			return nullptr;
		}
	}

	CSteamSocket pSock( steamID, hConnection );
	m_vecSockets.AddToTail( pSock );

	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( STEAM_CNX_COLOR, "Opened Steam Socket to %s\n", RenderSteamId( steamID ) );
	}

	SteamNetworkingSockets()->SetConnectionPollGroup( hConnection, m_hPollGroup );

	return &m_vecSockets.Tail();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::CloseConnection( CSteamSocket *pSocket )
{
	if ( !SteamNetworkingSockets() || pSocket == NULL )
		return;

	SteamNetworkingSockets()->CloseConnection( pSocket->GetConnection(), 0, "Game ended, conection closing", true );

	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( STEAM_CNX_COLOR, "Closed Steam Socket to %s\n", RenderSteamId( pSocket->GetSteamID() ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSteamSocket *CEconNetworking::FindConnectionForID( CSteamID const &steamID )
{
	CSteamSocket *pSock = nullptr;
	FOR_EACH_VEC( m_vecSockets, i )
	{
		if ( m_vecSockets[i].GetSteamID() == steamID )
		{
			pSock = &m_vecSockets[i];
			break;
		}
	}

	return pSock;
}

#else

//-----------------------------------------------------------------------------
// Purpose: Initialize connection to server
//-----------------------------------------------------------------------------
void CEconNetworking::ConnectToServer( SteamNetworkingIdentity const &identity )
{
	Assert( identity.m_eType == k_ESteamNetworkingIdentityType_IPAddress );
	m_hServerConnection = SteamNetworkingSockets()->ConnectByIPAddress( *identity.GetIPAddr(), 0, NULL );

	Assert( m_hPollGroup != k_HSteamNetPollGroup_Invalid );
	SteamNetworkingSockets()->SetConnectionPollGroup( m_hServerConnection, m_hPollGroup );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
// TODO: Convert
void CEconNetworking::ProcessDataFromConnection( HSteamNetConnection hConn )
{
	if ( !SteamNetworkingSockets() )
		return;

	SteamNetworkingMessage_t *messages[16];
	int nNumMessages = SteamNetworkingSockets()->ReceiveMessagesOnConnection( hConn, messages, ARRAYSIZE( messages ) );
	for ( int i = 0; i < nNumMessages; ++i )
	{
		uint32 cubMsgSize = messages[i]->GetSize(); // Includes header size
		void *pData = malloc( cubMsgSize );
		Q_memcpy( pData, messages[i]->GetData(), cubMsgSize );

		CNetPacket *pPacket = new CNetPacket;
		pPacket->InitFromMemory( pData, cubMsgSize );

		if ( !pPacket->Hdr().has_protocol_version() || pPacket->Hdr().protocol_version() != STEAM_CNX_PROTO_VERSION )
		{
			SteamNetConnectionInfo_t info;
			if( SteamNetworkingSockets()->GetConnectionInfo( hConn, &info ) )
			{
				Warning( "Malformed packet received from %s, protocol mismatch!",
						 SteamNetworkingIdentityRender( info.m_identityRemote ).c_str() );
			}

			continue;
		}

		CBaseMsgHandler *pHandler = NULL;
		unsigned nIndex = m_MessageTypes.Find( pPacket->MsgType() );
		if ( nIndex != m_MessageTypes.InvalidIndex() )
			pHandler = (CBaseMsgHandler *)m_MessageTypes[nIndex];

		if ( pHandler )
			pHandler->QueueWork( pPacket );

		pPacket->Release();
		messages[i]->Release();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconNetworking::SendData( CSteamID const &targetID, void const *pubData, uint32 cubData )
{
	if ( !SteamNetworkingSockets() )
		return false;

#if defined( GAME_DLL )
	CSteamSocket *pSock = FindConnectionForID( targetID );
	if ( pSock == nullptr )
		return false;

	HSteamNetConnection hConn = pSock->GetConnection();
	if ( hConn == k_HSteamNetConnection_Invalid )
		return false;

	return SteamNetworkingSockets()->SendMessageToConnection( hConn, pubData, cubData, k_nSteamNetworkingSend_Reliable, NULL ) == k_EResultOK;
#else
	return SteamNetworkingSockets()->SendMessageToConnection( m_hServerConnection, pubData, cubData, k_nSteamNetworkingSend_Reliable, NULL ) == k_EResultOK;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconNetworking::RecvData( void *pubDest, uint32 cubDest )
{
	if ( !SteamNetworkingSockets() )
		return false;

	if ( m_hPollGroup == k_HSteamNetPollGroup_Invalid )
		return false;

	SteamNetworkingMessage_t *message;
	int nNumMessages = SteamNetworkingSockets()->ReceiveMessagesOnPollGroup( m_hPollGroup, &message, 1 );
	if( nNumMessages != 0 )
	{
		Assert( message->GetSize() <= cubDest );
		V_memcpy( pubDest, message->GetData(), cubDest );

		message->Release();
	}

	return nNumMessages == 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconNetworking::BSendMessage( CSteamID const &targetID, MsgType_t eMsg, google::protobuf::Message const &msg )
{
#if defined( GAME_DLL )
	CSteamSocket *pSock = FindConnectionForID( targetID );
	if ( pSock == nullptr )
		return false;

	HSteamNetConnection hConn = pSock->GetConnection();
	if ( hConn == k_HSteamNetConnection_Invalid )
		return false;
#endif

	CNetPacket *pPacket = new CNetPacket;
	if ( pPacket == nullptr )
		return false;

	pPacket->Init( msg.ByteSize(), eMsg );
	msg.SerializeWithCachedSizesToArray( pPacket->MutableData() + sizeof(MsgHdr_t) );

#if defined( GAME_DLL )
	bool bSuccess = SteamNetworkingSockets()->SendMessageToConnection( hConn, pPacket->Data(), pPacket->Size(), 
																k_nSteamNetworkingSend_Reliable, NULL ) == k_EResultOK;
#else
	bool bSuccess = SteamNetworkingSockets()->SendMessageToConnection( m_hServerConnection, pPacket->Data(), pPacket->Size(), 
																k_nSteamNetworkingSend_Reliable, NULL ) == k_EResultOK;
#endif

	pPacket->Release();
	return bSuccess;
}

#if defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::Update( float frametime )
{
	if ( !SteamNetworkingSockets() )
		return;

	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( !pPlayer || !engine->IsConnected() )
		return;

	ProcessDataFromConnection( m_hServerConnection );
}

#else

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::PreClientUpdate()
{
	if ( !SteamNetworkingSockets() )
		return;

	FOR_EACH_VEC( m_vecSockets, i )
	{
		ProcessDataFromConnection( m_vecSockets[i].GetConnection() );
	}
}
#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::SessionStatusChanged( SteamNetConnectionStatusChangedCallback_t *pStatus )
{
	if ( !SteamNetworkingSockets() )
		return;

	switch ( pStatus->m_info.m_eState )
	{
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
		case k_ESteamNetworkingConnectionState_ClosedByPeer:
		{
			ConColorMsg( STEAM_CNX_COLOR, "Closing connection with %s (%s)", 
						 SteamNetworkingIdentityRender( pStatus->m_info.m_identityRemote ).c_str(),
						 SteamNetworkingIPAddrRender( pStatus->m_info.m_addrRemote ).c_str() );

			SteamNetworkingSockets()->CloseConnection( pStatus->m_hConn, 0, NULL, false );
			break;
		}

		case k_ESteamNetworkingConnectionState_Connecting:
		{
			ConColorMsg( STEAM_CNX_COLOR, "Accepting connection from %s (%s)", 
						 SteamNetworkingIdentityRender( pStatus->m_info.m_identityRemote ).c_str(),
						 SteamNetworkingIPAddrRender( pStatus->m_info.m_addrRemote ).c_str() );

			SteamNetworkingSockets()->AcceptConnection( pStatus->m_hConn );
			break;
		}

		case k_ESteamNetworkingConnectionState_Connected:
		{
			ConColorMsg( STEAM_CNX_COLOR, "Connected to %s (%s)", 
						 SteamNetworkingIdentityRender( pStatus->m_info.m_identityRemote ).c_str(),
						 SteamNetworkingIPAddrRender( pStatus->m_info.m_addrRemote ).c_str() );
		}

		case k_ESteamNetworkingConnectionState_FindingRoute:
			break; // NAT traversal is being attempted, finding a route from peer to peer
		case k_ESteamNetworkingConnectionState_None:
			break; // Something happened, typically we closed the connection

		default:
			AssertMsg( false, "Unhandled connection state in %s", __FUNCTION__ );
			break;
	}
}


//-----------------------------------------------------------------------------
static CEconNetworking g_Networking;
IEconNetworking *g_pEconNetwork = &g_Networking;

void RegisterEconNetworkMessageHandler( MsgType_t eMsg, IMessageHandler *pHandler )
{
	g_Networking.m_MessageTypes.Insert( eMsg, pHandler );
}


static void NetworkingSessionStatusChanged( SteamNetConnectionStatusChangedCallback_t *pStatus )
{
	g_Networking.SessionStatusChanged( pStatus );
}
//-----------------------------------------------------------------------------
