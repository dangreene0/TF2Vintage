//========= Copyright � Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "tf_gamerules.h"
#include "tf_robot_destruction_robot.h"
#include "tf_logic_robot_destruction.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar tf_rd_robot_attack_notification_cooldown( "tf_rd_robot_attack_notification_cooldown", "10", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );


RobotData_t *g_RobotData[NUM_ROBOT_TYPES] =
{
	new RobotData_t( "models/bots/bot_worker/bot_worker_A.mdl",	"models/bots/bot_worker/bot_worker_A.mdl", "Robot.Pain", "Robot.Death", "Robot.Collide", "Robot.Greeting", -35.f ),
	new RobotData_t( "models/bots/bot_worker/bot_worker2.mdl", "models/bots/bot_worker/bot_worker2.mdl", "Robot.Pain", "Robot.Death", "Robot.Collide", "Robot.Greeting", -30.f ),
	new RobotData_t( "models/bots/bot_worker/bot_worker3.mdl", "models/bots/bot_worker/bot_worker3_nohead.mdl", "Robot.Pain", "Robot.Death", "Robot.Collide", "Robot.Greeting", -10.f ),
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RobotData_t::Precache( void )
{
	CBaseEntity::PrecacheModel( m_pszModelName );
	PrecacheGibsForModel( modelinfo->GetModelIndex( m_pszModelName ) );
	PrecachePropsForModel( modelinfo->GetModelIndex( m_pszModelName ), "spawn" );
	CBaseEntity::PrecacheModel( m_pszDamagedModelName );

	CBaseEntity::PrecacheScriptSound( m_pszHurtSound );
	CBaseEntity::PrecacheScriptSound( m_pszDeathSound );
	CBaseEntity::PrecacheScriptSound( m_pszCollideSound );
	CBaseEntity::PrecacheScriptSound( m_pszIdleSound );
}


IMPLEMENT_NETWORKCLASS_ALIASED( TFRobotDestruction_RobotSpawn, DT_TFRobotDestructionRobotSpawn )

BEGIN_NETWORK_TABLE_NOBASE( CTFRobotDestruction_RobotSpawn, DT_TFRobotDestructionRobotSpawn )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( tf_robot_destruction_robot_spawn, CTFRobotDestruction_RobotSpawn );

BEGIN_DATADESC( CTFRobotDestruction_RobotSpawn )
#ifdef GAME_DLL
	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "SpawnRobot", InputSpawnRobot ),

	// Keyfields
	DEFINE_KEYFIELD( m_spawnData.m_eType, FIELD_INTEGER, "type" ),
	DEFINE_KEYFIELD( m_spawnData.m_nRobotHealth, FIELD_INTEGER, "health" ),
	DEFINE_KEYFIELD( m_spawnData.m_nPoints, FIELD_INTEGER, "points" ),
	DEFINE_KEYFIELD( m_spawnData.m_pszGroupName, FIELD_STRING, "spawngroup" ),
	DEFINE_KEYFIELD( m_spawnData.m_nNumGibs, FIELD_INTEGER, "gibs" ),
	DEFINE_KEYFIELD( m_spawnData.m_pszPathName, FIELD_STRING, "startpath" ),

	// Outputs
	DEFINE_OUTPUT( m_OnRobotKilled, "OnRobotKilled" ),
#endif
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFRobotDestruction_RobotSpawn::CTFRobotDestruction_RobotSpawn()
{
#if defined(GAME_DLL)
	m_hRobot = NULL;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotSpawn::Activate()
{
	BaseClass::Activate();
#if defined(GAME_DLL)
	if ( !m_spawnData.m_pszGroupName || !m_spawnData.m_pszGroupName[0] )
	{
		Warning( "%s has no spawn group defined!", STRING( GetEntityName() ) );
		return;
	}

	// TODO
	/*CTFRobotDestruction_RobotGroup *pGroup  = dynamic_cast<CTFRobotDestruction_RobotGroup *>( pEntity );
	if ( )
	{
		Warning( "%s", CFmtStr( "%s specified '%s' as its group, but %s is a %s", 
								STRING( GetEntityName() ), 
								m_spawnData.m_pszGroupName, 
								m_spawnData.m_pszGroupName, 
								pEnt->GetClassname() );
	}
	
	if ( pGroup )
	{
		pGroup->AddToGroup( this );
	}
	else
	{
		Warning( "Couldn't find robot destruction spawn group named '%s'!\n", m_spawnData.m_pszGroupName );
	}

	CPathTrack *pPath = dynamic_cast<CPathTrack *>( pEntity );
	if ( )
	{
		Warning( "%s", CFmtStr( "%s specified '%s' as its first path, but %s is a %s", 
								STRING( GetEntityName() ), 
								m_spawnData.m_pszPathName, 
								m_spawnData.m_pszPathName, 
								pEnt->GetClassname() ) );
	}
	else if( pEntity == NULL )
	{
		Warning( "%s", CFmtStr( "%s specified '%s' as its first path, but %s doesn't exist", 
								STRING( GetEntityName() ), 
								m_spawnData.m_pszPathName, 
								m_spawnData.m_pszPathName ) );
	}
	*/
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotSpawn::Precache( void )
{
	BaseClass::Precache();

	PrecacheParticleSystem( "rd_robot_explosion" );
	PrecacheParticleSystem( "bot_radio_waves" );
	PrecacheParticleSystem( "sentrydamage_4" );

	PrecacheScriptSound( "RD.BotDeathExplosion" );

	for ( int i=0; i < ARRAYSIZE( g_RobotData ); ++i )
	{
		g_RobotData[i]->Precache();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotSpawn::Spawn()
{
	BaseClass::Spawn();

#ifdef GAME_DLL
	SetSolid( SOLID_NONE );

	Precache();
#endif
}

#if defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotSpawn::ClearRobot()
{
	m_hRobot = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotSpawn::OnRobotKilled()
{
	ClearRobot();
	m_OnRobotKilled.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotSpawn::SpawnRobot()
{
	if ( m_hRobotGroup == NULL )
	{
		Warning( "Spawnpoint '%s' tried to spawn a robot, but group name '%s' didnt find any groups!\n", STRING( GetEntityName() ), m_spawnData.m_pszGroupName );
		return;
	}

	if ( m_hRobot == NULL )
	{
		m_hRobot = (CTFRobotDestruction_Robot *)CreateEntityByName( "tf_robot_destruction_robot" );
		m_hRobot->SetModel( g_RobotData[ m_spawnData.m_eType ]->m_pszModelName );
		m_hRobot->ChangeTeam( m_hRobotGroup->GetTeamNumber() );
		m_hRobot->SetHealth( m_spawnData.m_nRobotHealth );
		m_hRobot->SetMaxHealth( m_spawnData.m_nRobotHealth );
		m_hRobot->SetRobotGroup( m_hRobotGroup.Get() );
		m_hRobot->SetRobotSpawn( this );
		m_hRobot->SetSpawnData( m_spawnData );
		m_hRobot->SetName( AllocPooledString( CFmtStr( "%s_robot", STRING( GetEntityName() ) ) ) );
		DispatchSpawn( m_hRobot );

		m_hRobot->SetAbsOrigin( GetAbsOrigin() );
		m_hRobot->SetAbsAngles( GetAbsAngles() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotSpawn::InputSpawnRobot( inputdata_t &inputdata )
{
	SpawnRobot();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFRobotDestruction_RobotSpawn::ShouldCollide( int collisionGroup, int contentsMask ) const
{
	if ( collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT )
	{
		return false;
	}

	return BaseClass::ShouldCollide( collisionGroup, contentsMask );
}
#endif

IMPLEMENT_AUTO_LIST( IRobotDestructionGroupAutoList );

LINK_ENTITY_TO_CLASS( tf_robot_destruction_spawn_group, CTFRobotDestruction_RobotGroup );
IMPLEMENT_NETWORKCLASS_ALIASED( TFRobotDestruction_RobotGroup, DT_TFRobotDestruction_RobotGroup )

BEGIN_NETWORK_TABLE_NOBASE( CTFRobotDestruction_RobotGroup, DT_TFRobotDestruction_RobotGroup )
#ifdef CLIENT_DLL
	RecvPropString( RECVINFO( m_pszHudIcon ) ),
	RecvPropInt( RECVINFO( m_iTeamNum ) ),
	RecvPropInt( RECVINFO( m_nGroupNumber ) ),
	RecvPropInt( RECVINFO( m_nState ) ),
	RecvPropFloat( RECVINFO( m_flRespawnStartTime ) ),
	RecvPropFloat( RECVINFO( m_flRespawnEndTime ) ),
	RecvPropFloat( RECVINFO( m_flLastAttackedTime ) ),
#else
	SendPropString( SENDINFO( m_pszHudIcon ) ),
	SendPropInt( SENDINFO( m_iTeamNum ), -1, SPROP_VARINT | SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nGroupNumber ), -1, SPROP_VARINT | SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nState ), -1, SPROP_VARINT | SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flRespawnStartTime ), -1, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flRespawnEndTime ), -1, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flLastAttackedTime ), -1, SPROP_NOSCALE ),
#endif
END_NETWORK_TABLE()


BEGIN_DATADESC( CTFRobotDestruction_RobotGroup )
#ifdef GAME_DLL
	// Keyfields
	DEFINE_KEYFIELD( m_iszHudIcon, FIELD_STRING, "hud_icon" ),
	DEFINE_KEYFIELD( m_flRespawnTime, FIELD_FLOAT, "respawn_time" ),
	DEFINE_KEYFIELD( m_nGroupNumber, FIELD_INTEGER, "group_number" ),
	DEFINE_KEYFIELD( m_nTeamNumber, FIELD_INTEGER, "team_number" ),
	DEFINE_KEYFIELD( m_flTeamRespawnReductionScale, FIELD_FLOAT, "respawn_reduction_scale" ),

	// Outputs
	DEFINE_OUTPUT( m_OnRobotsRespawn, "OnRobotsRespawn" ),
	DEFINE_OUTPUT( m_OnAllRobotsDead, "OnAllRobotsDead" ),
#endif
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFRobotDestruction_RobotGroup::CTFRobotDestruction_RobotGroup()
{
#ifdef GAME_DLL
	m_flRespawnTime = 0;
	m_nTeamNumber = 0;
	m_nState.Set( ROBOT_STATE_DEAD );
	m_nGroupNumber.Set( 0 );
	m_flRespawnStartTime.Set( 0 );
	m_flRespawnEndTime.Set( 1.0f );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFRobotDestruction_RobotGroup::~CTFRobotDestruction_RobotGroup()
{
#ifdef CLIENT_DLL
	IGameEvent *event = gameeventmanager->CreateEvent( "rd_rules_state_changed" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}
#endif
}

#if defined(GAME_DLL)

float CTFRobotDestruction_RobotGroup::sm_flNextAllowedAttackAlertTime[TF_TEAM_COUNT] ={};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotGroup::Spawn()
{
	V_strncpy( m_pszHudIcon.GetForModify(), STRING( m_iszHudIcon ), MAX_PATH );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotGroup::Activate()
{
	BaseClass::Activate();
	ChangeTeam( m_nTeamNumber );

	Q_memset( sm_flNextAllowedAttackAlertTime, 0.f, sizeof( sm_flNextAllowedAttackAlertTime ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFRobotDestruction_RobotGroup::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotGroup::AddToGroup( CTFRobotDestruction_RobotSpawn *pSpawn )
{
	pSpawn->SetRobotGroup( this );
	m_vecSpawns.AddToTail( pSpawn );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotGroup::EnableUberForGroup()
{
	FOR_EACH_VEC( m_vecSpawns, i )
	{
		CTFRobotDestruction_Robot *pRobot = m_vecSpawns[i]->GetRobot();
		if ( pRobot )
		{
			pRobot->EnableUber();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotGroup::DisableUberForGroup()
{
	FOR_EACH_VEC( m_vecSpawns, i )
	{
		CTFRobotDestruction_Robot *pRobot = m_vecSpawns[i]->GetRobot();
		if ( pRobot )
		{
			pRobot->DisableUber();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFRobotDestruction_RobotGroup::GetNumAliveBots()
{
	int nNumAlive = 0;
	FOR_EACH_VEC( m_vecSpawns, i )
	{
		CTFRobotDestruction_Robot *pRobot = m_vecSpawns[i]->GetRobot();
		if ( pRobot && pRobot->m_lifeState != LIFE_DEAD )
		{
			++nNumAlive;
		}
	}

	return nNumAlive;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotGroup::OnRobotAttacked()
{
	float &flNextAlertTime = sm_flNextAllowedAttackAlertTime[ GetTeamNumber() ];

	if ( gpGlobals->curtime >= flNextAlertTime )
	{
		flNextAlertTime = gpGlobals->curtime + tf_rd_robot_attack_notification_cooldown.GetFloat();

		CTeamRecipientFilter filter( GetTeamNumber(), true );
		TFGameRules()->SendHudNotification( filter, HUD_NOTIFY_RD_ROBOT_ATTACKED );
	}

	m_flLastAttackedTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotGroup::OnRobotKilled()
{
	UpdateState();

	// If all our robots are dead, fire the corresponding output
	if ( GetNumAliveBots() == 0 )
		m_OnAllRobotsDead.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotGroup::OnRobotSpawned()
{
	UpdateState();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotGroup::RemoveFromGroup( CTFRobotDestruction_RobotSpawn *pSpawn )
{
	pSpawn->SetRobotGroup( NULL );
	m_vecSpawns.FindAndRemove( pSpawn );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotGroup::RespawnCountdownFinish()
{
	RespawnRobots();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotGroup::RespawnRobots()
{
	StopRespawnTimer();

	FOR_EACH_VEC( m_vecSpawns, i )
	{
		m_vecSpawns[i]->SpawnRobot();
	}

	m_OnRobotsRespawn.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotGroup::StartRespawnTimerIfNeeded( CTFRobotDestruction_RobotGroup *pGroup )
{
	bool bBool = pGroup == this || pGroup == NULL;

	if ( GetNextThink( "group_respawn_context" ) != TICK_NEVER_THINK && bBool )
		return;

	if ( GetNumAliveBots() != 0 )
		return;

	float flRespawnTime = bBool ? gpGlobals->curtime + m_flRespawnTime : pGroup->GetNextThink( "group_respawn_context" );

	// If this respawn time is different, then mark this time as the respawn start time.  This can
	// get multiple times with the same value, and we dont want to update every time if we dont have to.
	if ( !AlmostEqual( flRespawnTime, GetNextThink( "group_respawn_context" ) ) )
	{
		// Mark this time
		m_flRespawnStartTime = gpGlobals->curtime;
	}

	m_flRespawnEndTime = flRespawnTime;
	SetContextThink( &CTFRobotDestruction_RobotGroup::RespawnCountdownFinish, m_flRespawnEndTime, "group_respawn_context" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotGroup::StopRespawnTimer()
{
	SetContextThink( NULL, TICK_NEVER_THINK, "group_respawn_context" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotGroup::UpdateState()
{
	bool bShielded = false;

	int nAlive = 0;
	FOR_EACH_VEC( m_vecSpawns, i )
	{
		CTFRobotDestruction_Robot *pRobot = m_vecSpawns[i]->GetRobot();
		if ( pRobot == NULL )
			continue;

		if ( pRobot->m_lifeState != LIFE_DEAD )
		{
			++nAlive;
			bShielded |= m_vecSpawns[i]->GetRobot()->IsShielded();
		}
	}

	ERobotState eState = ROBOT_STATE_INACIVE;
	if ( bShielded )
	{
		eState = ROBOT_STATE_SHIELDED;
	}
	else if ( nAlive > 0 )
	{
		eState = ROBOT_STATE_ACTIVE;
	}
	else
	{
		eState = ROBOT_STATE_DEAD;
	}

	m_nState = eState;
	m_flRespawnEndTime = GetNextThink( "group_respawn_context" );
}

#else //GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotGroup::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "rd_rules_state_changed" );
		if ( event )
		{
			gameeventmanager->FireEventClientSide( event );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_RobotGroup::SetDormant( bool bDormant )
{
	BaseClass::SetDormant( bDormant );

	IGameEvent *event = gameeventmanager->CreateEvent( "rd_rules_state_changed" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}
}

#endif
