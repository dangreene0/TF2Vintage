//========= Copyright © Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "tf_mann_vs_machine_logic.h"
#include "tf_gamerules.h"
#include "entity_capture_flag.h"
#include "func_capture_zone.h"
#include "func_flagdetectionzone.h"


BEGIN_DATADESC( CMannVsMachineLogic )
	DEFINE_THINKFUNC( Update ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( tf_logic_mann_vs_machine, CMannVsMachineLogic );

CHandle<CMannVsMachineLogic> g_hMannVsMachineLogic;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMannVsMachineLogic::CMannVsMachineLogic()
{
	InitPopulationManager();

	m_flNextAlarmCheck = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMannVsMachineLogic::~CMannVsMachineLogic()
{
	g_hMannVsMachineLogic = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMannVsMachineLogic::Spawn( void )
{
	BaseClass::Spawn();

	SetThink( &CMannVsMachineLogic::Update );
	SetNextThink( gpGlobals->curtime );

	g_hMannVsMachineLogic = this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMannVsMachineLogic::SetupOnRoundStart( void )
{
	if ( !TFGameRules() || !TFGameRules()->IsMannVsMachineMode() )
		return;

	if ( m_populationManager )
	{
		//m_populationManager->SetupOnRoundStart();
	}
}

void CMannVsMachineLogic::Update( void )
{
	VPROF_BUDGET( "CMannVsMachineLogic::Update", VPROF_BUDGETGROUP_GAME );

	SetNextThink( gpGlobals->curtime + 0.05f );

	if ( !TFGameRules() || !TFGameRules()->IsMannVsMachineMode() )
		return;

	if ( m_populationManager )
	{
		//m_populationManager->Update();
	}

	if ( m_flNextAlarmCheck < gpGlobals->curtime )
	{
		m_flNextAlarmCheck = gpGlobals->curtime + 0.1;
		for ( int i=0; i<ICaptureFlagAutoList::AutoList().Count(); ++i )
		{
			CCaptureFlag *pFlag = static_cast<CCaptureFlag *>( ICaptureFlagAutoList::AutoList()[i] );
			if ( pFlag->IsStolen() )
			{
				for ( int j=0; j<IFlagDetectionZoneAutoList::AutoList().Count(); ++j )
				{
					CFlagDetectionZone *pZone = static_cast<CFlagDetectionZone *>( IFlagDetectionZoneAutoList::AutoList()[j] );
					if ( !pZone->IsDisabled() && pZone->IsAlarmZone() && pZone->PointIsWithin( pFlag->GetAbsOrigin() ) )
					{
						// Is the alarm currently off?
						if ( TFGameRules()->GetMannVsMachineAlarmStatus() == false )
						{
							IGameEvent *event = gameeventmanager->CreateEvent( "mvm_bomb_alarm_triggered" );
							if ( event )
							{
								gameeventmanager->FireEvent( event );
							}
						}

						TFGameRules()->SetMannVsMachineAlarmStatus( true );
						return;
					}
				}
			}
		}

		TFGameRules()->SetMannVsMachineAlarmStatus( false );
	}
}