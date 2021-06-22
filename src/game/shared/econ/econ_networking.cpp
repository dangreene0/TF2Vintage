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

	void OnClientConnected( SteamNetworkingIdentity const &identity, HSteamNetConnection hConnection ) OVERRIDE;
	void OnClientDisconnected( CSteamID const &steamID ) OVERRIDE;
	void ConnectToServer( SteamNetworkingIdentity const &identity ) OVERRIDE;

	CSteamSocket *OpenConnection( CSteamID const &steamID, HSteamNetConnection hConnection );
	void CloseConnection( CSteamSocket *pSocket );
	CSteamSocket *FindConnectionForID( CSteamID const &steamID );

	void ProcessDataFromConnection( HSteamNetConnection hConn );
	bool SendMessage( CSteamID const &targetID, MsgType_t eMsg, ::google::protobuf::Message const &msg ) OVERRIDE;

#ifdef CLIENT_DLL
	virtual void Update( float frametime );
#else
	virtual void PreClientUpdate( void );
#endif

	void SessionStatusChanged( SteamNetConnectionStatusChangedCallback_t *pStatus );

private:
	HSteamListenSocket						m_hListenSocket;
	HSteamNetConnection						m_hServerConnection;
	CUtlVector< CSteamSocket >				m_vecSockets;
	HSteamNetPollGroup						m_hPollGroup;
	CUtlMap< MsgType_t, IMessageHandler* >	m_MessageTypes;
};


DEFINE_FIXEDSIZE_ALLOCATOR_MT( CNetPacket, 128, UTLMEMORYPOOL_GROW_FAST );
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetPacket::Init( uint32 size, MsgType_t eMsg )
{
	m_Hdr = {eMsg, size, STEAM_CNX_PROTO_VERSION, k_EProtocolProtobuf};

	m_pMsg = SteamNetworkingUtils()->AllocateMessage( size + sizeof( MsgHdr_t ) );
	Q_memcpy( m_pMsg->m_pData, &m_Hdr, sizeof( MsgHdr_t ) );

	AddRef();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetPacket::InitFromMemory( void const *pMemory, uint32 size )
{
	Assert( pMemory );

	m_pMsg = SteamNetworkingUtils()->AllocateMessage( size );
	Q_memcpy( m_pMsg->m_pData, pMemory, size );

	Q_memcpy( &m_Hdr, m_pMsg->m_pData, sizeof( MsgHdr_t ) );

	Assert( ( m_Hdr.m_unMsgSize + sizeof( MsgHdr_t ) ) == size );

	AddRef();
}


static void NetworkingSessionStatusChanged(SteamNetConnectionStatusChangedCallback_t *);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconNetworking::CEconNetworking() : 
	CAutoGameSystemPerFrame( "EconNetworking" ),
	m_MessageTypes( DefLessFunc( MsgType_t ) ),
	m_hPollGroup( k_HSteamNetPollGroup_Invalid ),
	m_hListenSocket( k_HSteamListenSocket_Invalid ),
	m_hServerConnection( k_HSteamNetConnection_Invalid )
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
	FOR_EACH_VEC( m_vecSockets, i )
	{
		CloseConnection( &m_vecSockets[i] );
	}
	m_vecSockets.Purge();

	if( SteamNetworkingSockets() )
		SteamNetworkingSockets()->DestroyPollGroup( m_hPollGroup );

	s_vecMsgPools.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::OnClientConnected( SteamNetworkingIdentity const &identity, HSteamNetConnection hConnection )
{
#if defined( GAME_DLL )
	if ( !steamgameserverapicontext->SteamGameServer() )
		return;
#endif

	if ( !SteamNetworkingSockets() )
		return;

	CSteamID steamID = identity.GetSteamID();
	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( STEAM_CNX_COLOR, "Initiate %llx\n", steamID.ConvertToUint64() );
	}

	CSteamSocket *pSocket = OpenConnection( steamID, hConnection );
	if ( pSocket )
	{
	#if defined( GAME_DLL )
		CProtobufMsg<CServerHelloMsg> msg;
		CSteamID remoteID = steamgameserverapicontext->SteamGameServer()->GetSteamID();

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

		SendMessage( steamID, k_EServerHelloMsg, msg.Body() );
	#endif
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

	FOR_EACH_VEC_BACK( m_vecSockets, i )
	{
		if ( m_vecSockets[i].GetSteamID() == steamID )
		{
			CloseConnection( &m_vecSockets[i] );
			m_vecSockets.FastRemove( i );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSteamSocket *CEconNetworking::OpenConnection( CSteamID const &steamID, HSteamNetConnection hConnection )
{
	if ( !SteamNetworkingSockets() )
		return nullptr;

	FOR_EACH_VEC( m_vecSockets, i )
	{
		if ( m_vecSockets[i].GetSteamID() == steamID )
		{
			AssertMsg( false, "Tried to open multiple connections to the same remote!" );
			return nullptr;
		}
	}

	CSteamSocket sock( steamID, hConnection );
	m_vecSockets.AddToTail( sock );

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

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
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

		CSmartPtr<CNetPacket> pPacket( new CNetPacket );
		pPacket->InitFromMemory( pData, cubMsgSize );

		if ( pPacket->Hdr().m_ubProtoVer != STEAM_CNX_PROTO_VERSION )
		{
			SteamNetConnectionInfo_t info;
			if( SteamNetworkingSockets()->GetConnectionInfo( hConn, &info ) )
			{
				Warning( "Malformed packet received from %s, protocol mismatch!",
						 SteamNetworkingIdentityRender( info.m_identityRemote ).c_str() );
			}

			continue;
		}

		IMessageHandler *pHandler = NULL;
		unsigned nIndex = m_MessageTypes.Find( pPacket->Hdr().m_eMsgType );
		if ( nIndex != m_MessageTypes.InvalidIndex() )
			pHandler = m_MessageTypes[nIndex];

		QueueEconNetworkMessageWork( pHandler, pPacket );
	}

	for ( int i = 0; i < nNumMessages; ++i )
		messages[i]->Release();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconNetworking::SendMessage( CSteamID const &targetID, MsgType_t eMsg, google::protobuf::Message const &msg )
{
#if defined( GAME_DLL )
	CSteamSocket *pSock = FindConnectionForID( targetID );
	if ( pSock == nullptr )
		return false;

	HSteamNetConnection hConn = pSock->GetConnection();
	if ( hConn == k_HSteamNetConnection_Invalid )
		return false;
#endif

	CSmartPtr<CNetPacket> pPacket( new CNetPacket );
	if ( !pPacket )
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
		{
			ConColorMsg( STEAM_CNX_COLOR, "Connection forcibly closed with %s (%s)!\n", 
						 SteamNetworkingIdentityRender( pStatus->m_info.m_identityRemote ).c_str(),
						 SteamNetworkingIPAddrRender( pStatus->m_info.m_addrRemote ).c_str() );

			DevMsg( "Connection %s end reason: %s\n", pStatus->m_info.m_szConnectionDescription, pStatus->m_info.m_szEndDebug );

			SteamNetworkingSockets()->CloseConnection( pStatus->m_hConn, 0, NULL, false );
			break;
		}

		case k_ESteamNetworkingConnectionState_ClosedByPeer:
		{
			ConColorMsg( STEAM_CNX_COLOR, "Closing connection with %s.\n", 
						 SteamNetworkingIdentityRender( pStatus->m_info.m_identityRemote ).c_str() );

			SteamNetworkingSockets()->CloseConnection( pStatus->m_hConn, 0, NULL, true );
			break;
		}

		case k_ESteamNetworkingConnectionState_Connecting:
		{
			ConColorMsg( STEAM_CNX_COLOR, "Accepting connection from %s (%s).\n", 
						 SteamNetworkingIdentityRender( pStatus->m_info.m_identityRemote ).c_str(),
						 SteamNetworkingIPAddrRender( pStatus->m_info.m_addrRemote ).c_str() );

			SteamNetworkingSockets()->AcceptConnection( pStatus->m_hConn );
			break;
		}

		case k_ESteamNetworkingConnectionState_Connected:
		{
			ConColorMsg( STEAM_CNX_COLOR, "Connected to %s.\n", 
						 SteamNetworkingIdentityRender( pStatus->m_info.m_identityRemote ).c_str() );

		#if defined( GAME_DLL )
			SteamNetworkingIdentity remoteID = pStatus->m_info.m_identityRemote;
			HSteamNetConnection hConnection = pStatus->m_hConn;

			OnClientConnected( remoteID, hConnection );
		#endif
		}

		case k_ESteamNetworkingConnectionState_FindingRoute:
			break; // NAT traversal is being attempted, finding a route from peer to peer
		case k_ESteamNetworkingConnectionState_None:
			break; // Something happened, typically we closed the connection

		default:
			AssertMsg( false, "Unhandled connection state!" );
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
