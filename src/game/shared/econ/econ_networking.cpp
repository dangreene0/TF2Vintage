#include "cbase.h"
#include "filesystem.h"
#include "vstdlib/coroutine.h"
#include "tier0/threadtools.h"
#include "econ_networking.h"
#include "tier1/smartptr.h"
#ifndef NO_STEAM
#include "steam/steamtypes.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef NO_STEAM

ConVar net_steamcnx_debug( "net_steamcnx_debug", "0", FCVAR_NONE, "Show debug spew for steam based connections, 2 shows all network traffic for steam sockets." );
static ConVar net_steamcnx_allowrelay( "net_steamcnx_allowrelay", "1", FCVAR_ARCHIVE, "Allow steam connections to attempt to use relay servers as fallback (best if specified on command line:  +net_steamcnx_allowrelay 1)" );

class CEconNetworking : public CAutoGameSystemPerFrame, public IEconNetworking
{
public:
	CEconNetworking();
	virtual ~CEconNetworking();

	virtual bool Init( void );
	virtual void Shutdown( void );

	void OnClientConnected( CSteamID const &steamID ) OVERRIDE;
	void OnClientDisconnected( CSteamID const &steamID ) OVERRIDE;
	bool SendData( CSteamID const &targetID, void const *pubData, uint32 cubData, int nChannel = 0 ) OVERRIDE;
	bool RecvData( void *pubDest, uint32 cubDest, uint32 *pcubMsgSize, CSteamID *pRemoteID, int nChannel = 0 ) OVERRIDE;

	bool BSendMessage( CSteamID const &targetID, MsgType_t eMsg, ::google::protobuf::Message const &msg ) OVERRIDE;

#ifdef CLIENT_DLL
	virtual void Update( float frametime );
#else
	virtual void PreClientUpdate( void );
#endif

	STEAM_CALLBACK( CEconNetworking, P2PSessionRequested, P2PSessionRequest_t, m_OnSessionRequested );
	STEAM_CALLBACK( CEconNetworking, P2PSessionFailed, P2PSessionConnectFail_t, m_OnSessionFailed );

private:
	ISteamNetworking *SteamNetworking( void ) const;
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
}

//-----------------------------------------------------------------------------
// Purpose: 
//
// TODO:    Get rid of the ifdefs
//-----------------------------------------------------------------------------
void CEconNetworking::OnClientConnected( CSteamID const &steamID )
{
	if ( !SteamNetworking() )
		return;

	if ( net_steamcnx_debug.GetBool() )
	{
		ConColorMsg( {255, 255, 100, 255}, "Initiate %llx\n", steamID.ConvertToUint64() );
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
}

#else

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconNetworking::PreClientUpdate()
{
	if ( !SteamNetworking() )
		return;
}
#endif // CLIENT_DLL

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

//-----------------------------------------------------------------------------
static CEconNetworking g_Networking;
IEconNetworking *g_pEconNetwork = &g_Networking;

#endif // NO_STEAM
//-----------------------------------------------------------------------------
