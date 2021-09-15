#include "cbase.h"
#include "filesystem.h"
#include "tier0/threadtools.h"
#include "econ_networking.h"
#include "econ_networking_messages.h"
#include "tier1/smartptr.h"
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

// Converted from https://github.com/ValveSoftware/GameNetworkingSockets/blob/master/src/common/steamid.cpp#L523-L594
static char const *RenderSteamId( CSteamID const &steamID )
{
	// longest length of returned string is k_cBufLen
	//	[A:%u:%u:%u]
	//	 %u == 10 * 3 + 6 == 36, plus terminator == 37
	const int k_cBufLen = 37;

	const int k_cBufs = 4;	// # of static bufs to use (so people can compose output with multiple calls to Render() )
	static char rgchBuf[k_cBufs][k_cBufLen];
	static int nBuf = 0;
	char *pchBuf = rgchBuf[nBuf];	// get pointer to current static buf
	nBuf++;	// use next buffer for next call to this method
	nBuf %= k_cBufs;

	if ( k_EAccountTypeAnonGameServer == steamID.GetEAccountType() )
	{
		V_snprintf( pchBuf, k_cBufLen, "[A:%u:%u:%u]", steamID.GetEUniverse(), steamID.GetAccountID(), steamID.GetUnAccountInstance() );
	}
	else if ( k_EAccountTypeGameServer == steamID.GetEAccountType() )
	{
		V_snprintf( pchBuf, k_cBufLen, "[G:%u:%u]", steamID.GetEUniverse(), steamID.GetAccountID() );
	}
	else if ( k_EAccountTypeMultiseat == steamID.GetEAccountType() )
	{
		V_snprintf( pchBuf, k_cBufLen, "[M:%u:%u:%u]", steamID.GetEUniverse(), steamID.GetAccountID(), steamID.GetUnAccountInstance() );
	}
	else if ( k_EAccountTypePending == steamID.GetEAccountType() )
	{
		V_snprintf( pchBuf, k_cBufLen, "[P:%u:%u]", steamID.GetEUniverse(), steamID.GetAccountID() );
	}
	else if ( k_EAccountTypeContentServer == steamID.GetEAccountType() )
	{
		V_snprintf( pchBuf, k_cBufLen, "[C:%u:%u]", steamID.GetEUniverse(), steamID.GetAccountID() );
	}
	else if ( k_EAccountTypeClan == steamID.GetEAccountType() )
	{
		// 'g' for "group"
		V_snprintf( pchBuf, k_cBufLen, "[g:%u:%u]", steamID.GetEUniverse(), steamID.GetAccountID() );
	}
	else if ( k_EAccountTypeChat == steamID.GetEAccountType() )
	{
		if ( steamID.GetUnAccountInstance() & k_EChatInstanceFlagClan )
		{
			V_snprintf( pchBuf, k_cBufLen, "[c:%u:%u]", steamID.GetEUniverse(), steamID.GetAccountID() );
		}
		else if ( steamID.GetUnAccountInstance() & k_EChatInstanceFlagLobby )
		{
			V_snprintf( pchBuf, k_cBufLen, "[L:%u:%u]", steamID.GetEUniverse(), steamID.GetAccountID() );
		}
		else // Anon chat
		{
			V_snprintf( pchBuf, k_cBufLen, "[T:%u:%u]", steamID.GetEUniverse(), steamID.GetAccountID() );
		}
	}
	else if ( k_EAccountTypeInvalid == steamID.GetEAccountType() )
	{
		V_snprintf( pchBuf, k_cBufLen, "[I:%u:%u]", steamID.GetEUniverse(), steamID.GetAccountID() );
	}
	else if ( k_EAccountTypeIndividual == steamID.GetEAccountType() )
	{
		V_snprintf( pchBuf, k_cBufLen, "[U:%u:%u]", steamID.GetEUniverse(), steamID.GetAccountID() );
	}
	else if ( k_EAccountTypeAnonUser == steamID.GetEAccountType() )
	{
		V_snprintf( pchBuf, k_cBufLen, "[a:%u:%u]", steamID.GetEUniverse(), steamID.GetAccountID() );
	}
	else
	{
		V_snprintf( pchBuf, k_cBufLen, "[i:%u:%u]", steamID.GetEUniverse(), steamID.GetAccountID() );
	}
	return pchBuf;
}

class CSteamSocket
{
public:
	EXPLICIT CSteamSocket( CSteamID const &remoteID, SNetSocket_t socket )
	{
		m_identity = remoteID;
		m_hConn = socket;
	}

	CSteamID GetSteamID( void ) const						{ return m_identity; }
	SNetSocket_t GetConnection( void ) const				{ return m_hConn; }

private:
	SNetSocket_t m_hConn;
	CSteamID m_identity;
};


class CEconNetworking : public CAutoGameSystemPerFrame, public IEconNetworking
{
public:
	CEconNetworking();
	virtual ~CEconNetworking();

	virtual bool Init( void );
	virtual void Shutdown( void );

	void OnClientConnected( CSteamID const &identity, SNetSocket_t socket ) OVERRIDE;
	void OnClientDisconnected( CSteamID const &steamID ) OVERRIDE;
	void ConnectToServer( long nIP, short nPort ) OVERRIDE;

	CSteamSocket *OpenConnection( CSteamID const &steamID, SNetSocket_t socket );
	void CloseConnection( CSteamSocket *pSocket );
	CSteamSocket *FindConnectionForID( CSteamID const &steamID );

	void ProcessDataFromConnection( SNetSocket_t socket );
	bool SendMessage( CSteamID const &targetID, MsgType_t eMsg, ::google::protobuf::Message const &msg ) OVERRIDE;

#ifdef CLIENT_DLL
	virtual void Update( float frametime );
#else
	virtual void PreClientUpdate( void );
#endif

	STEAM_CALLBACK( CEconNetworking, SessionStatusChanged, SocketStatusCallback_t, m_StatusCallback );
	bool AddMessageHandler( MsgType_t eMsg, IMessageHandler *pHandler );

private:
	inline ISteamNetworking *SteamNetworking( void ) const
	{
		ISteamNetworking *pNetworking = ::SteamNetworking();
	#ifdef GAME_DLL
		if ( pNetworking == NULL )
		{
			pNetworking = ::SteamGameServerNetworking();
		}
	#endif
		return pNetworking;
	}

	SNetListenSocket_t						m_hListenSocket;
	SNetSocket_t							m_hServerSocket;
	CUtlVector< CSteamSocket >				m_vecSockets;
	CUtlMap< MsgType_t, IMessageHandler* >	m_MessageTypes;
};


DEFINE_FIXEDSIZE_ALLOCATOR_MT( CNetPacket, 128, UTLMEMORYPOOL_GROW_FAST );
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNetPacket::Init( uint32 size, MsgType_t eMsg )
{
	m_Hdr = {eMsg, size, STEAM_CNX_PROTO_VERSION, k_EProtocolProtobuf};

	m_pMsg = malloc( size + sizeof( MsgHdr_t ) );
	Q_memcpy( m_pMsg, &m_Hdr, sizeof( MsgHdr_t ) );

	AddRef();
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

	AddRef();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconNetworking::CEconNetworking() : 
	CAutoGameSystemPerFrame( "EconNetworking" ),
	m_MessageTypes( DefLessFunc( MsgType_t ) ),
	m_StatusCallback( this, &CEconNetworking::SessionStatusChanged )
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
#if defined( GAME_DLL )
	if( engine->IsDedicatedServer() )
	{
		m_hListenSocket = SteamNetworking()->CreateListenSocket( 0, 0, ECON_SERVER_PORT, net_steamcnx_allowrelay.GetBool() );
	}
#endif

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::Shutdown( void )
{
	SteamNetworking()->DestroyListenSocket( m_hListenSocket, false );

	FOR_EACH_VEC( m_vecSockets, i )
	{
		CloseConnection( &m_vecSockets[i] );
	}
	m_vecSockets.Purge();

	s_vecMsgPools.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::OnClientConnected( CSteamID const &steamID, SNetSocket_t socket )
{
#if defined( GAME_DLL )
	if ( !steamgameserverapicontext->SteamGameServer() )
		return;
#endif

	if ( !SteamNetworking() )
		return;

	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( STEAM_CNX_COLOR, "Initiate %llx\n", steamID.ConvertToUint64() );
	}

	CSteamSocket *pSocket = OpenConnection( steamID, socket );
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

		msg->set_version( unVersion );
		msg->set_steamid( remoteID.ConvertToUint64() );

		SendMessage( steamID, k_EServerHelloMsg, msg.Body() );
	#endif
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
CSteamSocket *CEconNetworking::OpenConnection( CSteamID const &steamID, SNetSocket_t socket )
{
	if ( !SteamNetworking() )
		return nullptr;

	FOR_EACH_VEC( m_vecSockets, i )
	{
		if ( m_vecSockets[i].GetSteamID() == steamID )
		{
			AssertMsg( false, "Tried to open multiple connections to the same remote!" );
			return nullptr;
		}
	}

	CSteamSocket sock( steamID, socket );
	m_vecSockets.AddToTail( sock );

	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( STEAM_CNX_COLOR, "Opened Steam Socket to %s\n", RenderSteamId( steamID ) );
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

	SteamNetworking()->DestroySocket( pSocket->GetConnection(), true );

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
void CEconNetworking::ConnectToServer( long nIP, short nPort )
{
	m_hServerSocket = SteamNetworking()->CreateConnectionSocket( nIP, nPort, 20 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::ProcessDataFromConnection( SNetSocket_t socket )
{
	if ( !SteamNetworking() )
		return;

	uint32 cubMsgSize = 0;
	while ( SteamNetworking()->IsDataAvailableOnSocket( socket, &cubMsgSize ) )
	{
		uint32 cubReceived = 0;
		void *pData = malloc( cubMsgSize+1 );
		if ( SteamNetworking()->RetrieveDataFromSocket( socket, pData, cubMsgSize, &cubReceived ) )
		{
			AssertMsg( cubMsgSize == cubReceived, "We can't handle split packets at this time." );
			if ( cubReceived < cubMsgSize )
			{
				free( pData );
				return;
			}

			CNetPacket *pPacket = new CNetPacket;
			pPacket->InitFromMemory( pData, cubMsgSize );

			IMessageHandler *pHandler = NULL;
			unsigned nIndex = m_MessageTypes.Find( pPacket->Hdr().m_eMsgType );
			if ( nIndex != m_MessageTypes.InvalidIndex() )
				pHandler = m_MessageTypes[nIndex];

			QueueEconNetworkMessageWork( pHandler, pPacket );

			pPacket->Release();
		}

		free( pData );
	}
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

	SNetSocket_t socket = pSock->GetConnection();
	if ( socket == NULL )
		return false;
#endif

	CSmartPtr<CNetPacket> pPacket( new CNetPacket );
	if ( !pPacket )
		return false;

	pPacket->Init( msg.ByteSize(), eMsg );
	msg.SerializeWithCachedSizesToArray( pPacket->MutableData() + sizeof(MsgHdr_t) );

#if defined( GAME_DLL )
	bool bSuccess = SteamNetworking()->SendDataOnSocket( socket, (void *)pPacket->Data(), pPacket->Size(), false );
#else
	bool bSuccess = SteamNetworking()->SendDataOnSocket( m_hServerSocket, (void *)pPacket->Data(), pPacket->Size(), false );
#endif

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

	ProcessDataFromConnection( m_hServerSocket );
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
		ProcessDataFromConnection( m_vecSockets[i].GetConnection() );
	}
}
#endif // CLIENT_DLL

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
			break;

		case k_ESNetSocketStateChallengeHandshake:
			break;

		case k_ESNetSocketStateConnected:
			break;

		case k_ESNetSocketStateDisconnecting:
			break;

		case k_ESNetSocketStateLocalDisconnect:
			break;

		case k_ESNetSocketStateTimeoutDuringConnect:
			break;

		case k_ESNetSocketStateRemoteEndDisconnected:
		case k_ESNetSocketStateConnectionBroken:
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
IEconNetworking *g_pEconNetwork = &g_Networking;

void RegisterEconNetworkMessageHandler( MsgType_t eMsg, IMessageHandler *pHandler )
{
	g_Networking.AddMessageHandler( eMsg, pHandler );
}

//-----------------------------------------------------------------------------
