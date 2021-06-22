#ifndef ECON_NETWORKING_MESSAGES_H
#define ECON_NETWORKING_MESSAGES_H
#ifdef _WIN32
#pragma once
#endif


#include "tier1/mempool.h"
#include "vstdlib/coroutine.h"
#include "tier1/smartptr.h"

class CNetPacket;

//-----------------------------------------------------------------------------
// Purpose: Interface for processing network packets
//-----------------------------------------------------------------------------
abstract_class IMessageHandler
{
public:
	virtual ~IMessageHandler() {}
	virtual bool ProcessMessage( CNetPacket *pPacket ) = 0;
};


void RegisterEconNetworkMessageHandler( MsgType_t eMsg, IMessageHandler *pHandler );
#define REG_ECON_MSG_HANDLER( classType, msgType, msgName ) \
		static classType s_##msgName##Handler; \
		static class CReg##classType \
		{ \
		public: \
			CReg##classType() { RegisterEconNetworkMessageHandler( msgType, &s_##msgName##Handler ); } \
		} g_Reg##classType

bool QueueEconNetworkMessageWork( IMessageHandler *pHandler, CSmartPtr<CNetPacket> const &pPacket );


template< class TProtoMsg >
class CProtobufMsg
{
	static CUtlMemoryPool *sm_MsgPool;
	static bool sm_bRegisteredPool;
public:
	CProtobufMsg()
		: m_pPacket( NULL )
	{
		m_pBody = AllocMsg();
	}
	CProtobufMsg( CNetPacket *pPacket )
		: m_pPacket( pPacket )
	{
		m_pBody = AllocMsg();
		Assert( m_pBody );

		m_pBody->ParseFromArray( m_pPacket->Data() + sizeof( MsgHdr_t ), 
								 m_pPacket->Size() - sizeof( MsgHdr_t ) );
	}
	virtual ~CProtobufMsg()
	{
		if ( m_pBody )
		{
			FreeMsg( m_pBody );
		}
	}

	TProtoMsg &Body( void ) { return *m_pBody; }
	TProtoMsg const &Body( void ) const { return *m_pBody; }
	TProtoMsg *operator->() { return m_pBody; }

protected:
	TProtoMsg *AllocMsg( void )
	{
		if ( !sm_bRegisteredPool )
		{
			Assert( sm_MsgPool == NULL );
			sm_MsgPool = new CUtlMemoryPool( sizeof( TProtoMsg ), 1 );
			sm_bRegisteredPool = true;
		}

		TProtoMsg *pMsg = (TProtoMsg *)sm_MsgPool->Alloc();
		Construct<TProtoMsg>( pMsg );
		return pMsg;
	}

	void FreeMsg( TProtoMsg *pObj )
	{
		Destruct<TProtoMsg>( pObj );
		sm_MsgPool->Free( (void *)pObj );
	}

private:
	CSmartPtr<CNetPacket> m_pPacket;
	TProtoMsg *m_pBody;

	// Copying is illegal
	CProtobufMsg( const CProtobufMsg& );
	CProtobufMsg& operator=( const CProtobufMsg& );
};

template< typename TProtoMsg >
CUtlMemoryPool *CProtobufMsg<TProtoMsg>::sm_MsgPool;
template< typename TProtoMsg >
bool CProtobufMsg<TProtoMsg>::sm_bRegisteredPool;

#endif
