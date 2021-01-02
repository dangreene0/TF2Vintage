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

static unsigned WorkThread( void *pvParam )
{
	Assert( pvParam );
	WorkItem_t *pWork = (WorkItem_t*)pvParam;
	IMessageHandler *pHandler = pWork->m_pHandler;
	INetPacket *pPacket = pWork->m_pPacket;

	pHandler->ProcessMessage( pPacket );
	while ( !pHandler->BWaitForCompletion() )
		ThreadSleep( 25 );

	pPacket->Release();

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseMsgHandler::BWaitForCompletion( void )
{
	while ( ThreadGetCurrentHandle() == m_hWorkThread )
		ThreadSleep( 25 );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseMsgHandler::QueueWork( INetPacket *pPacket )
{
	WorkItem_t work{this, pPacket};
	m_hWorkThread = CreateSimpleThread( &WorkThread, &work, 1024 );
}
