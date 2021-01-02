#ifndef ECON_NETWORKING_MESSAGES_H
#define ECON_NETWORKING_MESSAGES_H
#ifdef _WIN32
#pragma once
#endif


#include "tier1/mempool.h"

class CNetPacket;
class INetPacket;

//-----------------------------------------------------------------------------
// Purpose: Interface for processing network packets
//-----------------------------------------------------------------------------
abstract_class IMessageHandler
{
public:
	virtual ~IMessageHandler() {}
	virtual bool ProcessMessage( INetPacket *pPacket ) = 0;
	virtual bool BWaitForCompletion( void ) = 0;
};

class CBaseMsgHandler : public IMessageHandler
{
public:
	CBaseMsgHandler() : 
		m_pWorkPacket( NULL ), 
		m_hWorkThread( NULL )
	{
	}

	virtual bool ProcessMessage( INetPacket *pPacket )	{ return false; }
	bool BWaitForCompletion( void ) OVERRIDE;
	void QueueWork( INetPacket *pPacket );
private:
	INetPacket *m_pWorkPacket;
	ThreadHandle_t m_hWorkThread;
};


void RegisterEconNetworkMessageHandler( MsgType_t eMsg, IMessageHandler *pHandler );
#define REG_ECON_MSG_HANDLER( classType, msgType, msgName ) \
		static classType s_##msgName##Handler; \
		static class CReg##classType \
		{ \
		public: \
			CReg##classType() { RegisterEconNetworkMessageHandler( msgType, &s_##msgName##Handler ); } \
		} g_Reg##classType

#endif
