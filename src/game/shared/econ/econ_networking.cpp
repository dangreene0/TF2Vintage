#include "cbase.h"
#include "filesystem.h"
#include "tier0/threadtools.h"
#include "econ_networking.h"
#include "econ_networking_messages.h"
#include "tier1/smartptr.h"
#ifndef NO_STEAM
#include "steam/steamtypes.h"
#include "steam/steam_api.h"
#endif

#include <memory>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar net_steamcnx_debug( "net_steamcnx_debug", IsDebug() ? "1" : "0", FCVAR_CHEAT|FCVAR_REPLICATED, "Show debug spew for steam based connections, 2 shows all network traffic for steam sockets." );
static ConVar net_steamcnx_allowrelay( "net_steamcnx_allowrelay", "0", FCVAR_CHEAT|FCVAR_REPLICATED, "Allow steam connections to attempt to use relay servers as fallback (best if specified on command line:  +net_steamcnx_allowrelay 1)" );
static ConVar net_steamcnx_usep2p( "net_steamcnx_usep2p", "1", FCVAR_DEVELOPMENTONLY|FCVAR_REPLICATED );

#define STEAM_CNX_COLOR				Color(255,255,100,255)
#define STEAM_CNX_PROTO_VERSION		2


class CSteamSocket
{
public:
	enum ESocketType_t
	{
		CLIENT	= 0,
		SERVER	= 1
	};

	EXPLICIT CSteamSocket( CSteamID const &remoteID, SNetSocket_t hSocket, ESocketType_t eType )
	{
		m_identity = remoteID;
		m_hConn = hSocket;
		m_eSocketType = eType;
	}

	CSteamID GetSteamID( void ) const					{ return m_identity; }
	SNetSocket_t GetConnection( void ) const			{ return m_hConn; }
	ESocketType_t GetSocketType( void ) const			{ return m_eSocketType; }
	ESocketType_t GetRemoteSocketType() const			{ return ( m_eSocketType == CLIENT ) ? SERVER : CLIENT; }

	int GetRemoteChannel() const						{ return ( GetRemoteSocketType() == CLIENT ) ? k_nClientPort : k_nServerPort; }
	int GetLocalChannel() const							{ return ( GetSocketType() == CLIENT ) ? k_nClientPort : k_nServerPort; }

private:
	SNetSocket_t m_hConn;
	CSteamID m_identity;
	ESocketType_t m_eSocketType;
};


class CEconNetworking : public IEconNetworking
{
public:
	CEconNetworking();
	virtual ~CEconNetworking();

	virtual bool Init( void );
	virtual void Shutdown( void );

#if defined(GAME_DLL)
	void OnClientConnected( CSteamID const &identity, SNetSocket_t socket ) OVERRIDE;
	void OnClientDisconnected( CSteamID const &steamID ) OVERRIDE;
#else
	void ConnectToServer( long nIP, short nPort, CSteamID const &serverID ) OVERRIDE;
#endif

	CSteamSocket *OpenConnection( CSteamID const &steamID, SNetSocket_t socket );
	void CloseConnection( CSteamSocket *pSocket );
	CSteamSocket *FindConnectionForID( CSteamID const &steamID );

#if defined(GAME_DLL)
	void ProcessDataFromClients( void );
#else
	void ProcessDataFromServer( void );
#endif

	bool SendMessage( CSteamID const &targetID, MsgType_t eMsg, void *pubData, uint32 cubData ) OVERRIDE;

	virtual void Update( float frametime );

#ifndef NO_STEAM
	STEAM_CALLBACK( CEconNetworking, SessionStatusChanged, SocketStatusCallback_t, m_StatusCallback );

	STEAM_CALLBACK( CEconNetworking, P2PSessionRequested, P2PSessionRequest_t, m_OnSessionRequested );
	STEAM_CALLBACK( CEconNetworking, P2PSessionFailed, P2PSessionConnectFail_t, m_OnSessionFailed );

	STEAM_GAMESERVER_CALLBACK( CEconNetworking, OnSteamServersConnected, SteamServersConnected_t, m_SteamServersConnectedCallback );
	STEAM_GAMESERVER_CALLBACK( CEconNetworking, OnSteamServersConnectFailure, SteamServerConnectFailure_t, m_SteamServersConnectFailureCallback );
	STEAM_GAMESERVER_CALLBACK( CEconNetworking, OnSteamServersDisconnected, SteamServersDisconnected_t, m_SteamServersDisconnectedCallback );
#endif

	bool AddMessageHandler( MsgType_t eMsg, IMessageHandler *pHandler );

private:
	ISteamNetworking *SteamNetworking( void ) const
	{
	#ifdef NO_STEAM
		return nullptr;
	#else
		ISteamNetworking *pNetworking = steamapicontext->SteamNetworking();
	#ifdef GAME_DLL
		if ( pNetworking == NULL )
		{
			pNetworking = steamgameserverapicontext->SteamGameServerNetworking();
		}
	#endif
		return pNetworking;
	#endif
	}

	void HandleNetPacket( CSmartPtr<CNetPacket> const &pPacket )
	{
		IMessageHandler *pHandler = NULL;
		unsigned nIndex = m_MessageTypes.Find( pPacket->Hdr().m_eMsgType );
		if ( nIndex != m_MessageTypes.InvalidIndex() )
			pHandler = m_MessageTypes[nIndex];

		QueueEconNetworkMessageWork( pHandler, pPacket );
	}

	SNetListenSocket_t						m_hListenSocket;
	SNetSocket_t							m_hServerSocket;
	CUtlVector< CSteamSocket >				m_vecSockets;
	CUtlMap< MsgType_t, IMessageHandler* >	m_MessageTypes;
	bool									m_bSteamConnection;
	bool									m_bIsLoopback;
};


DEFINE_FIXEDSIZE_ALLOCATOR_MT( CNetPacket, 128, UTLMEMORYPOOL_GROW_FAST );
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetPacket::Init( uint32 size, MsgType_t eMsg )
{
	m_Hdr = {eMsg, size, STEAM_CNX_PROTO_VERSION};

	m_pMsg = malloc( size + sizeof( MsgHdr_t ) );
	Q_memcpy( m_pMsg, &m_Hdr, sizeof( MsgHdr_t ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetPacket::InitFromMemory( void const *pMemory, uint32 size )
{
	Assert( pMemory );

	m_pMsg = malloc( size );
	Q_memcpy( m_pMsg, pMemory, size );

	Q_memcpy( &m_Hdr, m_pMsg, sizeof( MsgHdr_t ) );

	Assert( ( m_Hdr.m_unMsgSize + sizeof( MsgHdr_t ) ) == size );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconNetworking::CEconNetworking() : 
	m_MessageTypes( DefLessFunc( MsgType_t ) ),
#ifndef NO_STEAM
	m_StatusCallback( this, &CEconNetworking::SessionStatusChanged ),
	m_SteamServersConnectedCallback( this, &CEconNetworking::OnSteamServersConnected ),
	m_SteamServersDisconnectedCallback( this, &CEconNetworking::OnSteamServersDisconnected ),
	m_SteamServersConnectFailureCallback( this, &CEconNetworking::OnSteamServersConnectFailure ),
	m_OnSessionRequested( this, &CEconNetworking::P2PSessionRequested ),
	m_OnSessionFailed( this, &CEconNetworking::P2PSessionFailed )
#endif
{
	m_bSteamConnection = false;
	m_bIsLoopback = false;
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
#if defined( GAME_DLL )
	if ( !SteamNetworking() )
		return true;

	if( engine->IsDedicatedServer() )
	{
		m_hListenSocket = SteamNetworking()->CreateListenSocket( k_nServerPort, SteamIPAddress_t::IPv4Any(), ECON_SERVER_PORT, net_steamcnx_allowrelay.GetBool() );
	}

	m_bIsLoopback = m_hListenSocket == NULL;
#endif

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::Shutdown( void )
{
	if ( SteamNetworking() )
	{
	#if defined( GAME_DLL )
		SteamNetworking()->DestroyListenSocket( m_hListenSocket, false );
	#else
		SteamNetworking()->DestroySocket( m_hServerSocket, false );
	#endif
	}

	FOR_EACH_VEC( m_vecSockets, i )
	{
		CloseConnection( &m_vecSockets[i] );
	}
	m_vecSockets.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::Update( float frametime )
{
#if defined( CLIENT_DLL )
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( !pPlayer || !engine->IsConnected() )
		return;

	ProcessDataFromServer();
#else
	ProcessDataFromClients();
#endif // CLIENT_DLL
}
#if defined( GAME_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::OnClientConnected( CSteamID const &steamID, SNetSocket_t socket )
{
	CSteamSocket *pSocket = OpenConnection( steamID, socket );
	if ( pSocket )
	{
		if ( !steamgameserverapicontext->SteamGameServer() )
		{
			CloseConnection( pSocket );
			return;
		}

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

		msg->set_version( unVersion );
		msg->set_remote_steamid( remoteID.ConvertToUint64() );

		const int nLength = msg->ByteSize();
		std::unique_ptr<byte[]> array = std::make_unique<byte[]>( nLength );
		msg->SerializeWithCachedSizesToArray( array.get() );

		SendMessage( steamID, k_EServerHelloMsg, array.get(), nLength );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::OnClientDisconnected( CSteamID const &steamID )
{
	FOR_EACH_VEC_BACK( m_vecSockets, i )
	{
		if ( m_vecSockets[i].GetSteamID() == steamID )
		{
			CloseConnection( &m_vecSockets[i] );
			m_vecSockets.FastRemove( i );
		}
	}
}
#endif
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSteamSocket *CEconNetworking::OpenConnection( CSteamID const &steamID, SNetSocket_t socket )
{
	FOR_EACH_VEC( m_vecSockets, i )
	{
		if ( m_vecSockets[i].GetSteamID() == steamID )
		{
			AssertMsg( false, "Tried to open multiple connections to the same remote!" );
			return &m_vecSockets[i];
		}
	}

	CSteamSocket::ESocketType_t sockType = steamID.BGameServerAccount() ? CSteamSocket::SERVER : CSteamSocket::CLIENT;
	CSteamSocket sock( steamID, socket, sockType );
	m_vecSockets.AddToTail( sock );

	if ( net_steamcnx_debug.GetBool() )
	{
		Msg( "Opened Steam Socket to %s.\n", steamID.Render() );
	}

	return &m_vecSockets.Tail();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::CloseConnection( CSteamSocket *pSocket )
{
	if ( !SteamNetworking() || pSocket == NULL )
		return;
	
	if( net_steamcnx_usep2p.GetBool() )
		SteamNetworking()->CloseP2PChannelWithUser( pSocket->GetSteamID(), pSocket->GetRemoteChannel() );
	else
		SteamNetworking()->DestroySocket( pSocket->GetConnection(), true );

	if ( net_steamcnx_debug.GetBool() )
	{
		Msg( "Closed Steam Socket to %s.\n", pSocket->GetSteamID().Render() );
	}

	m_hServerSocket = 0;
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

#if defined(CLIENT_DLL)
//-----------------------------------------------------------------------------
// Purpose: Initialize connection to server
//-----------------------------------------------------------------------------
void CEconNetworking::ConnectToServer( long nIP, short nPort, CSteamID const &serverID )
{
	if ( m_hServerSocket )
	{
		Warning( "Attempted to open multiple connections to the server!" );
		return;
	}

	if ( !SteamNetworking() )
		return;

	SteamIPAddress_t ipAddress;
	ipAddress.m_unIPv4 = nIP;
	ipAddress.m_eType = k_ESteamIPTypeIPv4;
	if ( !net_steamcnx_usep2p.GetBool() )
		m_hServerSocket = SteamNetworking()->CreateConnectionSocket( ipAddress, nPort, SERVER_TIMEOUT );
	else
		m_hServerSocket = 1;

	OpenConnection( serverID, m_hServerSocket );

	INetChannelInfo *pNetChan = engine->GetNetChannelInfo();
	m_bIsLoopback = pNetChan->IsLoopback();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::ProcessDataFromServer( void )
{
	if ( !m_hServerSocket && !net_steamcnx_usep2p.GetBool() )
		return;

	if ( !SteamNetworking() )
		return;

	if ( net_steamcnx_usep2p.GetBool() )
	{
		Assert( m_vecSockets.Count() == 1 );

		uint32 cubMsgSize = 0; // Message size includes header
		int nChannel = m_vecSockets[0].GetLocalChannel();
		while ( SteamNetworking()->IsP2PPacketAvailable( &cubMsgSize, nChannel ) )
		{
			void *pData = malloc( cubMsgSize );
			uint32 cubTotalMsgSize{}; CSteamID remoteID{};
			if ( SteamNetworking()->ReadP2PPacket( pData, cubMsgSize, &cubTotalMsgSize, &remoteID, nChannel ) )
			{
				Assert( cubMsgSize >= sizeof( MsgHdr_t ) );
				if ( cubMsgSize < sizeof( MsgHdr_t ) )
				{
					Warning( "%s: Malformed packet recieved; Dropping.\n", __FUNCTION__ );
					free( pData );
					continue;
				}

				CNetPacket *pPacket = new CNetPacket;
				pPacket->InitFromMemory( pData, cubMsgSize );
				HandleNetPacket( pPacket );
				pPacket->Release();
			}
			free( pData );
		}
	}
	else
	{
		uint32 cubMsgSize = 0;
		while ( SteamNetworking()->IsDataAvailableOnSocket( m_hServerSocket, &cubMsgSize ) )
		{
			void *pData = malloc( cubMsgSize+1 );
			if ( SteamNetworking()->RetrieveDataFromSocket( m_hServerSocket, pData, cubMsgSize, &cubMsgSize ) )
			{
				Assert( cubMsgSize >= sizeof( MsgHdr_t ) );
				if ( cubMsgSize < sizeof( MsgHdr_t ) )
				{
					Warning( "%s: Malformed packet recieved; Dropping.\n", __FUNCTION__ );
					free( pData );
					continue;
				}

				CNetPacket *pPacket = new CNetPacket;
				pPacket->InitFromMemory( pData, cubMsgSize );
				HandleNetPacket( pPacket );
				pPacket->Release();
			}

			free( pData );
		}
	}
	
}
#else
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::ProcessDataFromClients( void )
{
	if ( !m_hListenSocket && !net_steamcnx_usep2p.GetBool() )
		return;

	if ( !SteamNetworking() )
		return;

	if ( net_steamcnx_usep2p.GetBool() )
	{
		FOR_EACH_VEC( m_vecSockets, i )
		{
			uint32 cubMsgSize = 0; // Message size includes header
			int nChannel = m_vecSockets[i].GetLocalChannel();
			while ( SteamNetworking()->IsP2PPacketAvailable( &cubMsgSize, nChannel ) )
			{
				void *pData = malloc( cubMsgSize );
				uint32 cubTotalMsgSize{}; CSteamID remoteID{};
				if ( SteamNetworking()->ReadP2PPacket( pData, cubMsgSize, &cubTotalMsgSize, &remoteID, nChannel ) )
				{
					Assert( cubMsgSize >= sizeof( MsgHdr_t ) );
					if ( cubMsgSize < sizeof( MsgHdr_t ) )
					{
						Warning( "%s: Malformed packet recieved; Dropping.\n", __FUNCTION__ );
						free( pData );
						continue;
					}

					CSmartPtr<CNetPacket> pPacket( new CNetPacket );
					pPacket->InitFromMemory( pData, cubMsgSize );
					HandleNetPacket( pPacket );
				}
				free( pData );
			}
		}
	}
	else
	{ 
		uint32 cubMsgSize = 0; SNetSocket_t hSock{};
		while ( SteamNetworking()->IsDataAvailable( m_hListenSocket, &cubMsgSize, &hSock ) )
		{
			void *pData = malloc( cubMsgSize+1 );
			if ( SteamNetworking()->RetrieveData( m_hListenSocket, pData, cubMsgSize, &cubMsgSize, &hSock ) )
			{
				Assert( cubMsgSize >= sizeof( MsgHdr_t ) );
				if ( cubMsgSize < sizeof( MsgHdr_t ) )
				{
					Warning( "%s: Malformed packet recieved; Dropping.\n", __FUNCTION__ );
					free( pData );
					continue;
				}

				CSmartPtr<CNetPacket> pPacket( new CNetPacket );
				pPacket->InitFromMemory( pData, cubMsgSize );
				HandleNetPacket( pPacket );
			}

			free( pData );
		}
	}
	
}
#endif
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconNetworking::SendMessage( CSteamID const &targetID, MsgType_t eMsg, void *pubData, uint32 cubData )
{
	CSmartPtr<CNetPacket> pPacket( new CNetPacket );
	if ( !pPacket )
		return false;

	pPacket->Init( cubData, eMsg );
	// Very simple malicious intent awareness
	if ( pPacket->Hdr().m_eMsgType != eMsg || pPacket->Hdr().m_ubProtoVer != STEAM_CNX_PROTO_VERSION )
		return false;

	Q_memcpy( pPacket->MutableData(), pubData, cubData );

	if ( m_bIsLoopback || !m_bSteamConnection )
	{
		return true;
	}
	else
	{
		CSteamSocket *pSock = FindConnectionForID( targetID );
		if ( pSock == nullptr )
			return false;

		bool bSuccess = false;
		if ( net_steamcnx_usep2p.GetBool() )
		{
			bSuccess = SteamNetworking()->SendP2PPacket( pSock->GetSteamID(), pPacket->Data(), pPacket->Size(), k_EP2PSendReliable, pSock->GetRemoteChannel() );
		}
		else
		{
			SNetSocket_t socket = pSock->GetConnection();
			if ( socket == NULL )
				return false;

			bSuccess = SteamNetworking()->SendDataOnSocket( socket, pPacket->Data(), pPacket->Size(), false );
		}
		
		return bSuccess;
	}
}

#ifndef NO_STEAM
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::SessionStatusChanged( SocketStatusCallback_t *pStatus )
{
	if ( !SteamNetworking() )
		return;

	switch ( pStatus->m_eSNetSocketState )
	{
		case k_ESNetSocketStateInitiated:
			if ( net_steamcnx_debug.GetBool() )
				Msg( "Initializing Steam connection.\n" );
			break;

		case k_ESNetSocketStateChallengeHandshake:
		#if defined(CLIENT_DLL)
			if ( net_steamcnx_debug.GetBool() )
			{
				SteamIPAddress_t nIP ={}; uint16 nPort = 0;
				SteamNetworking()->GetListenSocketInfo( pStatus->m_hListenSocket, &nIP, &nPort );

				Msg( "Initiating connection to %d.%d.%d.%d:%d.\n",
							 uint8( nIP.m_unIPv4 >> 24 ), uint8( nIP.m_unIPv4 >> 16 ), uint8( nIP.m_unIPv4 >> 8 ), uint8( nIP.m_unIPv4 >> 24 ), nPort );
			}
		#endif
			break;

		case k_ESNetSocketStateConnected:
		#if defined(GAME_DLL)
			OnClientConnected( pStatus->m_steamIDRemote, pStatus->m_hSocket );
		#endif
			if ( net_steamcnx_debug.GetBool() )
				Msg( "Connected to %s.\n", pStatus->m_steamIDRemote.Render() );
			break;

		case k_ESNetSocketStateDisconnecting:
			if ( net_steamcnx_debug.GetBool() )
				Msg( "Closing connection with %s.\n", pStatus->m_steamIDRemote.Render() );
			break;

		case k_ESNetSocketStateLocalDisconnect:
		case k_ESNetSocketStateRemoteEndDisconnected:
		case k_ESNetSocketStateConnectionBroken:
		case k_ESNetSocketStateTimeoutDuringConnect:
			CloseConnection( FindConnectionForID( pStatus->m_steamIDRemote ) );
		#if defined(CLIENT_DLL)
			m_hServerSocket = NULL;
		#endif
			if ( net_steamcnx_debug.GetBool() )
				Msg( "Connection closed by %s. Reason %d.\n", pStatus->m_steamIDRemote.Render(), pStatus->m_eSNetSocketState );
			break;

		case k_ESNetSocketStateLocalCandidatesFound:
		case k_ESNetSocketStateReceivedRemoteCandidates:
			if ( net_steamcnx_debug.GetBool() )
				Msg( "Connection info for %s received. State %d.\n", pStatus->m_steamIDRemote.Render(), pStatus->m_eSNetSocketState );
			break;

		case k_ESNetSocketStateInvalid:
			break;

		default:
			AssertMsg1( false, "Unhandled connection state %d", pStatus->m_eSNetSocketState );
			break;
	}
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
	ConnectToServer( 0, 0, remoteID );
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

	CloseConnection( FindConnectionForID( pFailure->m_steamIDRemote ) );

	const int nFailureMessage = pFailure->m_eP2PSessionError - 1;
	ConColorMsg( {255, 255, 100, 255}, "P2P session failed: Reason %d (%s)\n", pFailure->m_eP2PSessionError, s_szFailureReason[nFailureMessage] );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::OnSteamServersConnected( SteamServersConnected_t *pLogonSuccess )
{
	m_bSteamConnection = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::OnSteamServersDisconnected( SteamServersDisconnected_t *pLoggedOff )
{
	m_bSteamConnection = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::OnSteamServersConnectFailure( SteamServerConnectFailure_t *pConnectFailure )
{
	m_bSteamConnection = false;
}

#endif // NO_STEAM

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconNetworking::AddMessageHandler( MsgType_t eMsg, IMessageHandler *pHandler )
{
	int nFound = m_MessageTypes.Find( eMsg );
	if ( nFound != m_MessageTypes.InvalidIndex() )
	{
		Assert( nFound == m_MessageTypes.InvalidIndex() );
		return false;
	}

	m_MessageTypes.Insert( eMsg, pHandler );
	return true;
}


//-----------------------------------------------------------------------------
static CEconNetworking g_Networking;
IEconNetworking *g_pNetworking = &g_Networking;

void RegisterEconNetworkMessageHandler( MsgType_t eMsg, IMessageHandler *pHandler )
{
	g_Networking.AddMessageHandler( eMsg, pHandler );
}

//-----------------------------------------------------------------------------
