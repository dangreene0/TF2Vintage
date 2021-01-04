#include "cbase.h"
#include "econ_networking.h"
#include "econ_networking_messages.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DEFINE_FIXEDSIZE_ALLOCATOR_MT( CNetPacket, 0, UTLMEMORYPOOL_GROW_FAST );

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


void CNetPacket::Init( uint32 size, MsgType_t eMsg )
{
	m_pData = (byte*)malloc( size );
	m_unSize = size;
	m_eMsg = eMsg;

	AddRef();
}


void CNetPacket::InitFromMemory( void const *pMemory, uint32 size )
{
	Assert( pMemory );

	m_eMsg = *(MsgType_t *)pMemory;
	m_pData = (byte *)malloc( size );
	m_unSize = size;

	Assert( m_pData );
	Q_memcpy( m_pData, (void **)((intp)pMemory + sizeof( MsgType_t )), size );

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
