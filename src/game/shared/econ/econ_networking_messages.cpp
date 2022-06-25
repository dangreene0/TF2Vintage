#include "cbase.h"
#include "filesystem.h"
#include "vstdlib/coroutine.h"
#include "econ_networking.h"
#include "econ_networking_messages.h"
#ifndef NO_STEAM
#include "steam/steamclientpublic.h"
#endif

#include <memory>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CUtlVector<CUtlMemoryPool*> s_vecMsgPools;
struct WorkItem_t
{
	IMessageHandler *m_pHandler;
	CSmartPtr<CNetPacket> m_pPacket;
};

// Queue is the wrong word
// TODO: Threaded job manager
bool QueueEconNetworkMessageWork( IMessageHandler *pHandler, CSmartPtr<CNetPacket> const &pPacket )
{
	if ( pHandler == nullptr || pPacket == nullptr )
		return false;

	const auto WorkThread = [ ] ( void *pvParam ) {
		WorkItem_t *pWork = (WorkItem_t *)pvParam;
		CNetPacket *pPacket = pWork->m_pPacket.GetObject();

		while ( !pWork->m_pHandler->ProcessMessage( pPacket ) )
			ThreadSleep(25);
	};

	WorkItem_t work{pHandler, pPacket};
	HCoroutine hCoroutine = Coroutine_Create( WorkThread, &work );
	Coroutine_Continue( hCoroutine, "process packet" );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CServerHelloHandler : public IMessageHandler
{
public:
	CServerHelloHandler() {}

	virtual bool ProcessMessage( CNetPacket *pPacket )
	{
		CProtobufMsg<CServerHelloMsg> msg( pPacket );

		CSteamID serverID( msg->remote_steamid() );
		DbgVerify( serverID.BGameServerAccount() );
		
	#if defined( CLIENT_DLL )
		CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
		if ( !pPlayer || !engine->IsConnected() )
			return false;

		uint unVersion = 0;
		FileHandle_t fh = g_pFullFileSystem->Open( "version.txt", "r", "MOD" );
		if ( fh && g_pFullFileSystem->Size( fh ) > 0 )
		{
			char version[48];
			g_pFullFileSystem->ReadLine( version, sizeof( version ), fh );
			unVersion = CRC32_ProcessSingleBuffer( version, Q_strlen(version)+1 );

			if ( msg->version() != unVersion )
				Warning( "The server you are trying to connect to is running a different version of the game.\n" );
		}

		g_pFullFileSystem->Close( fh );

		CProtobufMsg<CClientHelloMsg> reply;
		reply->set_version( unVersion );

		CSteamID playerID;
		pPlayer->GetSteamID( &playerID );
		reply->set_remote_steamid( playerID.ConvertToUint64() );

		const int nLength = reply->ByteSize();
		CUtlVectorConservative<byte> array( 0, nLength );
		reply->SerializeWithCachedSizesToArray( array.Base() );

		g_pNetworking->SendMessage( serverID, k_EClientHelloMsg, array.Base(), nLength );
	#endif

		return true;
	}
};
REG_ECON_MSG_HANDLER( CServerHelloHandler, k_EServerHelloMsg, CServerHelloMsg );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CClientHelloHandler : public IMessageHandler
{
public:
	CClientHelloHandler() {}

	virtual bool ProcessMessage( CNetPacket *pPacket )
	{
	#if defined( GAME_DLL )
		FileHandle_t fh = filesystem->Open("version.txt", "r", "MOD");
		if ( fh && filesystem->Size( fh ) > 0 )
		{
			CProtobufMsg<CClientHelloMsg> msg( pPacket );

			char version[48];
			filesystem->ReadLine( version, sizeof( version ), fh );

			uint32 unVersion = CRC32_ProcessSingleBuffer( version, Q_strlen(version)+1 );
			if ( unVersion > 0 && msg->has_version() )
			{
				if ( msg->version() != unVersion )
				{
					engine->ServerCommand( UTIL_VarArgs( 
						"kickid %d \"The server you are trying to connect to is running a\\n different version of the game.\"\n",
						UTIL_PlayerBySteamID( msg->remote_steamid() )->GetUserID() ) 
					);
				}
			}
		}

		filesystem->Close( fh );
	#endif

		return true;
	}
};
REG_ECON_MSG_HANDLER( CClientHelloHandler, k_EClientHelloMsg, CClientHelloMsg );