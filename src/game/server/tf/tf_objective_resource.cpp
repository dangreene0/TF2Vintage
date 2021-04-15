//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: Entity that propagates general data needed by clients for every player.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_objective_resource.h"
#include "shareddefs.h"
#include <coordsize.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern void SendProxy_String_tToString( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );

// Datatable
IMPLEMENT_SERVERCLASS_ST( CTFObjectiveResource, DT_TFObjectiveResource )
	SendPropInt( SENDINFO( m_nMannVsMachineMaxWaveCount ), 9, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nMannVsMachineWaveCount ), 9, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nMannVsMachineWaveEnemyCount ), 16, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nMvMWorldMoney ), 16, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flMannVsMachineNextWaveTime ) ),
	SendPropBool( SENDINFO( m_bMannVsMachineBetweenWaves ) ),

	SendPropArray3( SENDINFO_ARRAY3( m_nMannVsMachineWaveClassCounts ), SendPropInt( SENDINFO_ARRAY( m_nMannVsMachineWaveClassCounts ), 16 ) ),
	SendPropArray( SendPropString( SENDINFO_ARRAY( m_iszMannVsMachineWaveClassNames ), 0, SendProxy_String_tToString ), m_iszMannVsMachineWaveClassNames ),
	SendPropArray3( SENDINFO_ARRAY3( m_nMannVsMachineWaveClassFlags ), SendPropInt( SENDINFO_ARRAY( m_nMannVsMachineWaveClassFlags ), 10, SPROP_UNSIGNED ) ),

	SendPropArray3( SENDINFO_ARRAY3( m_nMannVsMachineWaveClassCounts2 ), SendPropInt( SENDINFO_ARRAY( m_nMannVsMachineWaveClassCounts2 ), 16 ) ),
	SendPropArray( SendPropString( SENDINFO_ARRAY( m_iszMannVsMachineWaveClassNames2 ), 0, SendProxy_String_tToString ), m_iszMannVsMachineWaveClassNames2 ),
	SendPropArray3( SENDINFO_ARRAY3( m_nMannVsMachineWaveClassFlags2 ), SendPropInt( SENDINFO_ARRAY( m_nMannVsMachineWaveClassFlags2 ), 10, SPROP_UNSIGNED ) ),

	SendPropArray3( SENDINFO_ARRAY3( m_bMannVsMachineWaveClassActive ), SendPropBool( SENDINFO_ARRAY( m_bMannVsMachineWaveClassActive ) ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_bMannVsMachineWaveClassActive2 ), SendPropBool( SENDINFO_ARRAY( m_bMannVsMachineWaveClassActive2 ) ) ),

	SendPropInt( SENDINFO( m_nFlagCarrierUpgradeLevel ), 4, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flMvMBaseBombUpgradeTime ) ),
	SendPropFloat( SENDINFO( m_flMvMNextBombUpgradeTime ) ),
	SendPropStringT( SENDINFO( m_iszMvMPopfileName ) ),
	SendPropInt( SENDINFO( m_iChallengeIndex ), 16 ),
	SendPropInt( SENDINFO( m_nMvMEventPopfileType ), 4, SPROP_UNSIGNED ),
END_SEND_TABLE()


BEGIN_DATADESC( CTFObjectiveResource )
DEFINE_FIELD( m_nMannVsMachineMaxWaveCount, FIELD_INTEGER ),
DEFINE_FIELD( m_nMannVsMachineWaveCount, FIELD_INTEGER ),
DEFINE_FIELD( m_nMannVsMachineWaveEnemyCount, FIELD_INTEGER ),
DEFINE_FIELD( m_nMvMWorldMoney, FIELD_INTEGER ),
DEFINE_FIELD( m_flMannVsMachineNextWaveTime, FIELD_TIME ),
DEFINE_FIELD( m_bMannVsMachineBetweenWaves, FIELD_BOOLEAN ),

DEFINE_AUTO_ARRAY( m_nMannVsMachineWaveClassCounts, FIELD_INTEGER ),
DEFINE_AUTO_ARRAY( m_iszMannVsMachineWaveClassNames, FIELD_STRING ),
DEFINE_AUTO_ARRAY( m_nMannVsMachineWaveClassFlags, FIELD_INTEGER ),

DEFINE_AUTO_ARRAY( m_nMannVsMachineWaveClassCounts2, FIELD_INTEGER ),
DEFINE_AUTO_ARRAY( m_iszMannVsMachineWaveClassNames2, FIELD_STRING ),
DEFINE_AUTO_ARRAY( m_nMannVsMachineWaveClassFlags2, FIELD_INTEGER ),

DEFINE_AUTO_ARRAY( m_bMannVsMachineWaveClassActive, FIELD_BOOLEAN ),
DEFINE_AUTO_ARRAY( m_bMannVsMachineWaveClassActive2, FIELD_BOOLEAN ),
END_DATADESC()


LINK_ENTITY_TO_CLASS( tf_objective_resource, CTFObjectiveResource );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFObjectiveResource::CTFObjectiveResource()
{
	m_nMannVsMachineMaxWaveCount = 0;
	m_nMannVsMachineWaveCount = 0;
	m_nMannVsMachineWaveEnemyCount = 0;
	m_nMvMWorldMoney = 0;
	m_flMannVsMachineNextWaveTime = 0;
	m_bMannVsMachineBetweenWaves = false;
	m_nFlagCarrierUpgradeLevel = 0;
	m_flMvMBaseBombUpgradeTime = 0;
	m_flMvMNextBombUpgradeTime = 0;
	m_iChallengeIndex = -1;
	m_iszMvMPopfileName = MAKE_STRING( "" );
	m_nMvMEventPopfileType.Set( MVM_EVENT_POPFILE_NONE );

	for ( int i = 0 ; i < m_nMannVsMachineWaveClassCounts.Count() ; ++i )
	{
		m_nMannVsMachineWaveClassCounts.Set( i, 0 );
	}

	for ( int i = 0 ; i < m_nMannVsMachineWaveClassCounts2.Count() ; ++i )
	{
		m_nMannVsMachineWaveClassCounts2.Set( i, 0 );
	}

	for ( int i = 0 ; i < m_nMannVsMachineWaveClassFlags.Count() ; ++i )
	{
		m_nMannVsMachineWaveClassFlags.Set( i, MVM_CLASS_FLAG_NONE );
	}

	for ( int i = 0 ; i < m_nMannVsMachineWaveClassFlags2.Count() ; ++i )
	{
		m_nMannVsMachineWaveClassFlags2.Set( i, MVM_CLASS_FLAG_NONE );
	}

	for ( int i = 0 ; i < m_iszMannVsMachineWaveClassNames.Count() ; ++i )
	{
		m_iszMannVsMachineWaveClassNames.Set( i, NULL_STRING );
	}

	for ( int i = 0 ; i < m_iszMannVsMachineWaveClassNames2.Count() ; ++i )
	{
		m_iszMannVsMachineWaveClassNames2.Set( i, NULL_STRING );
	}

	for ( int i = 0 ; i < m_bMannVsMachineWaveClassActive.Count() ; ++i )
	{
		m_bMannVsMachineWaveClassActive.Set( i, false );
	}

	for ( int i = 0 ; i < m_bMannVsMachineWaveClassActive2.Count() ; ++i )
	{
		m_bMannVsMachineWaveClassActive2.Set( i, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFObjectiveResource::Spawn( void )
{
	BaseClass::Spawn();
}
