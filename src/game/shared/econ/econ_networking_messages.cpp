#include "cbase.h"
#include "econ_networking.h"
#include "econ_networking_messages.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct WorkItem_t
{
	IMessageHandler *m_pHandler;
	INetPacket *m_pPacket;
};

static void WorkThread( void *pvParam )
{
	WorkItem_t *pWork = (WorkItem_t*)pvParam;
	IMessageHandler *pHandler = pWork->m_pHandler;
	INetPacket *pPacket = pWork->m_pPacket;

	pHandler->ProcessMessage( pPacket );

	pPacket->Release();
}


CBaseMsgHandler::CBaseMsgHandler()
	: m_pWorkPacket( NULL ), m_hCoroutine( NULL )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseMsgHandler::BWaitForCompletion( void )
{
	while ( Coroutine_GetCurrentlyActive() == m_hCoroutine )
		ThreadSleep( 25 );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseMsgHandler::QueueWork( INetPacket *pPacket )
{
	WorkItem_t work{this, pPacket};
	pPacket->AddRef();

	m_hCoroutine = Coroutine_Create( WorkThread, &work );
	Coroutine_Continue( m_hCoroutine, "process packet" );
}
