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
#ifdef GAME_DLL
	#include "tf_gamestats.h"
	#include "entity_capture_flag.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar tf_rd_robot_attack_notification_cooldown( "tf_rd_robot_attack_notification_cooldown", "10", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );
ConVar tf_rd_points_approach_interval( "tf_rd_points_approach_interval", "0.1f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );
ConVar tf_rd_points_per_approach( "tf_rd_points_per_approach", "5", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );
ConVar tf_rd_steal_rate( "tf_rd_steal_rate", "0.5", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );
ConVar tf_rd_points_per_steal( "tf_rd_points_per_steal", "5", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );
ConVar tf_rd_min_points_to_steal( "tf_rd_min_points_to_steal", "25", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );

#ifdef CLIENT_DLL
ConVar tf_rd_finale_beep_time( "tf_rd_finale_beep_time", "10", FCVAR_ARCHIVE );
#endif


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

	if ( CTFRobotDestructionLogic::GetRobotDestructionLogic() )
		CTFRobotDestructionLogic::GetRobotDestructionLogic()->AddRobotGroup( this );
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

	if ( CTFRobotDestructionLogic::GetRobotDestructionLogic() )
		CTFRobotDestructionLogic::GetRobotDestructionLogic()->ManageGameState();

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

	if ( CTFRobotDestructionLogic::GetRobotDestructionLogic() )
		CTFRobotDestructionLogic::GetRobotDestructionLogic()->ManageGameState();

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

LINK_ENTITY_TO_CLASS( tf_logic_robot_destruction, CTFRobotDestructionLogic );
IMPLEMENT_NETWORKCLASS_ALIASED( TFRobotDestructionLogic, DT_TFRobotDestructionLogic )

BEGIN_NETWORK_TABLE_NOBASE( CTFRobotDestructionLogic, DT_TFRobotDestructionLogic )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_nMaxPoints ) ),
	RecvPropInt( RECVINFO( m_nBlueScore ) ),
	RecvPropInt( RECVINFO( m_nRedScore ) ),
	RecvPropInt( RECVINFO( m_nBlueTargetPoints ) ),
	RecvPropInt( RECVINFO( m_nRedTargetPoints ) ),
	RecvPropFloat( RECVINFO( m_flBlueTeamRespawnScale ) ),
	RecvPropFloat( RECVINFO( m_flRedTeamRespawnScale ) ),
	RecvPropFloat( RECVINFO( m_flBlueFinaleEndTime ) ),
	RecvPropFloat( RECVINFO( m_flRedFinaleEndTime ) ),
	RecvPropFloat( RECVINFO( m_flFinaleLength ) ),
	RecvPropString( RECVINFO( m_szResFile ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_eWinningMethod ), RecvPropInt( RECVINFO( m_eWinningMethod[0] ) ) ),
	RecvPropFloat( RECVINFO( m_flCountdownEndTime ) ),
#else
	SendPropInt( SENDINFO( m_nMaxPoints ), -1, SPROP_VARINT | SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nBlueScore ), -1, SPROP_VARINT | SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nRedScore ), -1, SPROP_VARINT | SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nBlueTargetPoints ), -1, SPROP_VARINT | SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nRedTargetPoints ), -1, SPROP_VARINT | SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flBlueTeamRespawnScale ), -1, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flRedTeamRespawnScale ), -1, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flBlueFinaleEndTime ), -1, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flRedFinaleEndTime ), -1, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flFinaleLength ), -1, SPROP_NOSCALE ),
	SendPropString( SENDINFO( m_szResFile ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_eWinningMethod ), SendPropInt( SENDINFO_ARRAY( m_eWinningMethod ), -1, SPROP_UNSIGNED | SPROP_VARINT ) ),
	SendPropFloat( SENDINFO( m_flCountdownEndTime ), -1, SPROP_NOSCALE ),
#endif
END_NETWORK_TABLE()

BEGIN_DATADESC( CTFRobotDestructionLogic )
#if defined(GAME_DLL)
	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "RoundActivate", InputRoundActivate ),

	// Outputs
	DEFINE_OUTPUT( m_OnRedHitZeroPoints, "OnRedHitZeroPoints" ),
	DEFINE_OUTPUT( m_OnRedHasPoints, "OnRedHasPoints" ),
	DEFINE_OUTPUT( m_OnRedFinalePeriodEnd, "OnRedFinalePeriodEnd" ),

	DEFINE_OUTPUT( m_OnBlueHitZeroPoints, "OnBlueHitZeroPoints" ),
	DEFINE_OUTPUT( m_OnBlueHasPoints, "OnBlueHasPoints" ),
	DEFINE_OUTPUT( m_OnBlueFinalePeriodEnd, "OnBlueFinalePeriodEnd" ),

	DEFINE_OUTPUT( m_OnRedFirstFlagStolen, "OnRedFirstFlagStolen" ),
	DEFINE_OUTPUT( m_OnRedFlagStolen, "OnRedFlagStolen" ),
	DEFINE_OUTPUT( m_OnRedLastFlagReturned, "OnRedLastFlagReturned" ),
	DEFINE_OUTPUT( m_OnBlueFirstFlagStolen, "OnBlueFirstFlagStolen" ),
	DEFINE_OUTPUT( m_OnBlueFlagStolen, "OnBlueFlagStolen" ),
	DEFINE_OUTPUT( m_OnBlueLastFlagReturned, "OnBlueLastFlagReturned" ),
	DEFINE_OUTPUT( m_OnBlueLeaveMaxPoints, "OnBlueLeaveMaxPoints" ),
	DEFINE_OUTPUT( m_OnRedLeaveMaxPoints, "OnRedLeaveMaxPoints" ),
	DEFINE_OUTPUT( m_OnBlueHitMaxPoints, "OnBlueHitMaxPoints" ),
	DEFINE_OUTPUT( m_OnRedHitMaxPoints, "OnRedHitMaxPoints" ),

	// Keyfields
	DEFINE_KEYFIELD( m_flRobotScoreInterval, FIELD_FLOAT, "score_interval" ),
	DEFINE_KEYFIELD( m_flLoserRespawnBonusPerBot, FIELD_FLOAT, "loser_respawn_bonus_per_bot" ),
	DEFINE_KEYFIELD( m_nMaxPoints, FIELD_INTEGER, "max_points" ),
	DEFINE_KEYFIELD( m_flFinaleLength, FIELD_FLOAT, "finale_length" ),
	DEFINE_KEYFIELD( m_iszResFile, FIELD_STRING, "res_file" ),
#endif
END_DATADESC()


CTFRobotDestructionLogic *CTFRobotDestructionLogic::sm_CTFRobotDestructionLogic = NULL;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFRobotDestructionLogic::CTFRobotDestructionLogic()
{
	sm_CTFRobotDestructionLogic = this;
#ifdef GAME_DLL
	Q_memset( m_nNumFlags, 0, sizeof( m_nNumFlags ) );
	m_iszResFile = MAKE_STRING( "resource/UI/HudObjectiveRobotDestruction.res" );

	ListenForGameEvent( "teamplay_pre_round_time_left" );
	ListenForGameEvent( "player_spawn" );

	for ( int i = 0; i < TF_TEAM_COUNT; i++ )
	{
		m_eWinningMethod.Set( i, SCORE_UNDEFINED );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFRobotDestructionLogic::~CTFRobotDestructionLogic()
{
	if ( sm_CTFRobotDestructionLogic == this )
		sm_CTFRobotDestructionLogic = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFRobotDestructionLogic *CTFRobotDestructionLogic::GetRobotDestructionLogic()
{
	return sm_CTFRobotDestructionLogic;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "Announcer.HowToPlayRD" );
	PrecacheScriptSound( "RD.TeamScoreCore" );
	PrecacheScriptSound( "RD.EnemyScoreCore" );
	PrecacheScriptSound( "RD.EnemyStealingPoints" );
	PrecacheScriptSound( "RD.FlagReturn" );
	PrecacheScriptSound( "RD.FinaleMusic" );

#ifdef GAME_DLL
	PrecacheScriptSound( "Announcer.EnemyTeamCloseToWinning" );
	PrecacheScriptSound( "Announcer.OurTeamCloseToWinning" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::Spawn()
{
	Precache();
	BaseClass::Spawn();

#ifdef GAME_DLL
	V_strncpy( m_szResFile.GetForModify(), STRING( m_iszResFile ), MAX_PATH );
#endif
}

float CTFRobotDestructionLogic::GetFinaleWinTime( int nTeam ) const
{
	return nTeam == TF_TEAM_RED ? m_flRedFinaleEndTime.Get() : m_flBlueFinaleEndTime.Get();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFRobotDestructionLogic::GetScore( int nTeam ) const
{
	return nTeam == TF_TEAM_RED ? m_nRedScore.Get() : m_nBlueScore.Get();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CTFRobotDestructionLogic::GetTargetScore( int nTeam ) const
{
	return nTeam == TF_TEAM_RED ? m_nRedTargetPoints.Get() : m_nBlueTargetPoints.Get();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFRobotDestructionLogic::GetRespawnScaleForTeam( int nTeam ) const
{
	if ( nTeam == TF_TEAM_RED )
	{
		return m_flRedTeamRespawnScale;
	}
	else
	{
		return m_flBlueTeamRespawnScale;
	}
}

#if defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::Activate()
{
	BaseClass::Activate();

	IGameEvent *event = gameeventmanager->CreateEvent( "rd_rules_state_changed" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// TODO Play Announcer.HowToPlayRD to certain people
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::FireGameEvent( IGameEvent *event )
{
	const char *pszName = event->GetName();
	if ( FStrEq( pszName, "teamplay_pre_round_time_left" ) )
	{
		const int nTimeLeft = event->GetInt( "time" );
		if ( nTimeLeft == 0 )
		{
			
		}
		else if ( nTimeLeft == 20 )
		{
			CUtlVector<CTFPlayer *> players;
			CollectHumanPlayers( &players );

			FOR_EACH_VEC( players, i )
			{
				CTFPlayer *pPlayer = players[i];
				if ( !pPlayer->IsAlive() )
					continue;
			}
		}
	}
	else if ( FStrEq( pszName, "player_spawn" ) )
	{
		const int nUserID = event->GetInt( "userid" );
		CTFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByUserId( nUserID ) );

		if ( pPlayer && pPlayer->IsAlive() )
		{
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFRobotDestructionLogic::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::AddRobotGroup( CTFRobotDestruction_RobotGroup *pGroup )
{
	m_vecRobotGroups.AddToTail( pGroup );

	IGameEvent *event = gameeventmanager->CreateEvent( "rd_rules_state_changed" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::ApproachTargetScoresThink()
{
	if ( !TFGameRules()->FlagsMayBeCapped() )
		return;

	// Approach
	int nOldRedScore = m_nRedScore;
	m_nRedScore = ApproachTeamTargetScore( TF_TEAM_RED, m_nRedTargetPoints, m_nRedScore.Get() );
	if ( nOldRedScore != m_nRedScore )
		OnRedScoreChanged();

	int m_nOldBlueScore = m_nBlueScore;
	m_nBlueScore = ApproachTeamTargetScore( TF_TEAM_BLUE, m_nBlueTargetPoints, m_nBlueScore.Get() );
	if ( m_nOldBlueScore != m_nBlueScore )
		OnBlueScoreChanged();

	if ( m_nBlueTargetPoints != m_nBlueScore.Get() || m_nRedTargetPoints != m_nRedScore.Get() )
	{
		SetContextThink( &CTFRobotDestructionLogic::ApproachTargetScoresThink, 
						 gpGlobals->curtime + tf_rd_points_approach_interval.GetFloat(), 
						 "approach_points_think" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFRobotDestructionLogic::ApproachTeamTargetScore( int nTeam, int nTargetScore, int nScore )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::BlueTeamWin()
{
	TeamWin( TF_TEAM_BLUE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::FlagCreated( int nTeam )
{
	if ( nTeam == TF_TEAM_RED )
	{
		m_OnRedFlagStolen.FireOutput( this, this );
		if ( m_nNumFlags[nTeam] == 0 )
		{
			m_OnRedFirstFlagStolen.FireOutput( this, this );
		}
	}
	else
	{
		m_OnBlueFlagStolen.FireOutput( this, this );
		if ( m_nNumFlags[nTeam] == 0 )
		{
			m_OnBlueFirstFlagStolen.FireOutput( this, this );
		}
	}

	++m_nNumFlags[nTeam];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::FlagDestroyed( int nTeam )
{
	if ( m_nNumFlags[nTeam] == 1 )
	{
		if ( nTeam == TF_TEAM_RED )
		{
			m_OnRedLastFlagReturned.FireOutput( this, this );
		}
		else
		{
			m_OnBlueLastFlagReturned.FireOutput( this, this );
		}
	}

	--m_nNumFlags[nTeam];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFRobotDestruction_Robot *CTFRobotDestructionLogic::IterateRobots( CTFRobotDestruction_Robot *pRobot )
{
	int nIndex = m_vecRobots.Find( pRobot );
	if ( nIndex == m_vecRobots.InvalidIndex() && !m_vecRobots.IsEmpty() )
		return m_vecRobots.Head();
	
	if ( ( nIndex + 1 ) >= m_vecRobots.Count() )
		return NULL;
	
	return m_vecRobots[nIndex + 1];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::ManageGameState( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::PlaySoundInPlayersEars( CTFPlayer *pSpeaker, EmitSound_t const &params )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::PlaySoundInfoForScoreEvent( CTFPlayer *pSpeaker, bool b1, int nScore, int nTeam, ERDScoreMethod eEvent )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::RedTeamWin()
{
	TeamWin( TF_TEAM_RED );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::RobotAttacked( CTFRobotDestruction_Robot *pRobot )
{
	// Duplicate code to CTFRobotDestruction_RobotGroup::OnRobotAttacked here
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::RobotCreated( CTFRobotDestruction_Robot *pRobot )
{
	m_vecRobots.AddToTail( pRobot );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::RobotRemoved( CTFRobotDestruction_Robot *pRobot )
{
	m_vecRobots.FindAndRemove( pRobot );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::ScorePoints( int nTeam, int nPoints, ERDScoreMethod eMethod, CTFPlayer *pScorer )
{
	if ( !TFGameRules()->FlagsMayBeCapped() )
		return;

	if ( nPoints == 0 )
		return;

	int nTargetScore = 0;
	if ( nTeam == TF_TEAM_RED )
	{
		nTargetScore = m_nRedTargetPoints = clamp( m_nRedTargetPoints + nPoints, 0, m_nMaxPoints.Get() );
	}
	else
	{
		nTargetScore = m_nBlueTargetPoints = clamp( m_nBlueTargetPoints + nPoints, 0, m_nMaxPoints.Get() );
	}

	if ( GetNextThink( "approach_points_think" ) == TICK_NEVER_THINK )
	{
		SetContextThink( &CTFRobotDestructionLogic::ApproachTargetScoresThink, 
						 gpGlobals->curtime + tf_rd_points_approach_interval.GetFloat(), 
						 "approach_points_think" );
	}

	// TODO

	if ( pScorer && nPoints > 0 )
	{
		CTF_GameStats.Event_PlayerAwardBonusPoints( pScorer, NULL, nPoints );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::TeamWin( int nTeam )
{
	ERDScoreMethod eMethod = (ERDScoreMethod)m_eWinningMethod.Get( nTeam );

	int nWinReason = WINREASON_NONE;
	switch ( eMethod )
	{
		case SCORE_REACTOR_CAPTURED:
			nWinReason = WINREASON_RD_REACTOR_CAPTURED;
			break;
		case SCORE_REACTOR_RETURNED:
			nWinReason = WINREASON_RD_REACTOR_RETURNED;
			break;
		case SCORE_CORES_COLLECTED:
			nWinReason = WINREASON_RD_CORES_COLLECTED;
			break;
	}

	if ( TFGameRules() )
	{
		TFGameRules()->SetWinningTeam( nTeam, nWinReason );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::InputRoundActivate( inputdata_t& )
{
	FOR_EACH_VEC( m_vecRobotGroups, i )
	{
		m_vecRobotGroups[i]->RespawnRobots();
	}
}
#else
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	float flSoonestFinale = Min( m_flBlueFinaleEndTime.Get(), m_flRedFinaleEndTime.Get() ) - gpGlobals->curtime;
	if ( flSoonestFinale <= m_flFinaleLength )
	{
		float flStartBeepAt = flSoonestFinale - tf_rd_finale_beep_time.GetFloat();
		SetNextClientThink( gpGlobals->curtime + flStartBeepAt );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play beeps when near end of game
//-----------------------------------------------------------------------------
void CTFRobotDestructionLogic::ClientThink()
{
	float flSoonestFinale = Min( m_flBlueFinaleEndTime.Get(), m_flRedFinaleEndTime.Get() ) - gpGlobals->curtime;
	if ( flSoonestFinale <= tf_rd_finale_beep_time.GetFloat() && flSoonestFinale > 0 )
	{
		SetNextClientThink( gpGlobals->curtime + 1 );

		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pPlayer )
		{
			bool bLastBeep = flSoonestFinale <= 1.0f;
			float flScale = RemapValClamped( Bias( 1.0f - ( flSoonestFinale / tf_rd_finale_beep_time.GetFloat() ), 0.2f ), 0.0f, 1.0f, 0.3f, 1.0f );
			EmitSound_t params;
			params.m_flSoundTime = 0;
			params.m_pSoundName = bLastBeep ? "Weapon_Grenade_Det_Pack.Timer" : "RD.FinaleBeep";
			float dummy = 0;
			params.m_pflSoundDuration = &dummy;
			params.m_flVolume = flScale;
			params.m_nPitch = bLastBeep ? PITCH_NORM : PITCH_NORM * ( 1.0f + flScale );
			params.m_nFlags |= SND_CHANGE_VOL|SND_CHANGE_PITCH;
			CBroadcastRecipientFilter filter;
			pPlayer->EmitSound( filter, pPlayer->entindex(), params );
		}
	}
}
#endif

#if defined(GAME_DLL)

LINK_ENTITY_TO_CLASS( trigger_rd_vault_trigger, CRobotDestructionVaultTrigger );

BEGIN_DATADESC( CRobotDestructionVaultTrigger )
	DEFINE_OUTPUT( m_OnPointsStolen, "OnPointsStolen" ),
	DEFINE_OUTPUT( m_OnPointsStartStealing, "OnPointsStartStealing" ),
	DEFINE_OUTPUT( m_OnPointsEndStealing, "OnPointsEndStealing" ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRobotDestructionVaultTrigger::CRobotDestructionVaultTrigger()
	: m_bStealing( false )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRobotDestructionVaultTrigger::Spawn()
{
	BaseClass::Spawn();

	InitTrigger();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRobotDestructionVaultTrigger::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "Cart.WarningSingle" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CRobotDestructionVaultTrigger::PassesTriggerFilters( CBaseEntity *pOther )
{
	if ( pOther->GetTeamNumber() == GetTeamNumber() )
		return false;

	if ( !pOther->ClassMatches( "player" ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRobotDestructionVaultTrigger::StartTouch( CBaseEntity *pOther )
{
	if ( !PassesTriggerFilters( pOther ) )
		return;

	BaseClass::StartTouch( pOther );

	if ( m_hTouchingEntities.Count() == 1 )
	{
		SetContextThink( &CRobotDestructionVaultTrigger::StealPointsThink, gpGlobals->curtime, "add_points_context" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRobotDestructionVaultTrigger::EndTouch( CBaseEntity *pOther )
{
	BaseClass::EndTouch( pOther );

	if ( m_hTouchingEntities.Count() == 0 )
	{
		SetContextThink( NULL, 0, "add_points_context" );
	}

	CTFPlayer *pPlayer = dynamic_cast<CTFPlayer *>( pOther );
	if ( pPlayer )
	{
		CCaptureFlag *pFlag = dynamic_cast<CCaptureFlag *>( pPlayer->GetItem() );
		if ( pFlag )
		{
			if ( pFlag->GetPointValue() < tf_rd_min_points_to_steal.GetInt() )
			{
				pFlag->Drop( pPlayer, true, true );
				pFlag->ResetMessage();
				pFlag->Reset();
			}

			if ( m_bStealing )
			{
				m_bStealing = false;
				m_OnPointsEndStealing.FireOutput( this, this );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRobotDestructionVaultTrigger::StealPointsThink()
{
	SetContextThink( &CRobotDestructionVaultTrigger::StealPointsThink, 
					 gpGlobals->curtime + tf_rd_steal_rate.GetFloat(), 
					 "add_points_context" );

	int nNumStolen = 0;
	FOR_EACH_VEC( m_hTouchingEntities, i )
	{
		CTFPlayer *pPlayer = static_cast<CTFPlayer *>( m_hTouchingEntities[i].Get() );
		if ( pPlayer )
		{
			CCaptureFlag *pFlag = dynamic_cast<CCaptureFlag *>( pPlayer->GetItem() );
			if ( pFlag )
			{
				nNumStolen = StealPoints( pPlayer );
			}
		}
	}

	if ( nNumStolen > 0 )
	{
		m_OnPointsStolen.FireOutput( this, this );
	}
	if ( nNumStolen && !m_bStealing )
	{
		m_OnPointsStartStealing.FireOutput( this, this );
	}
	else if ( !nNumStolen && m_bStealing )
	{
		m_OnPointsEndStealing.FireOutput( this, this );
	}

	m_bStealing = nNumStolen != 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CRobotDestructionVaultTrigger::StealPoints( CTFPlayer *pPlayer )
{
	CCaptureFlag *pFlag = dynamic_cast<CCaptureFlag *>( pPlayer->GetItem() );
	if ( pFlag && CTFRobotDestructionLogic::GetRobotDestructionLogic() )
	{
		int nEnemyTeam = GetEnemyTeam( pPlayer->GetTeamNumber() );
		int nEnemyPoints = CTFRobotDestructionLogic::GetRobotDestructionLogic()->GetTargetScore( nEnemyTeam );
		if ( nEnemyPoints )
		{
			int nPointsToSteal = Min( nEnemyPoints, tf_rd_points_per_steal.GetInt() );
			pFlag->AddPointValue( nPointsToSteal );
			CTFRobotDestructionLogic::GetRobotDestructionLogic()->ScorePoints( nEnemyTeam, -nPointsToSteal, SCORE_REACTOR_STEAL, pPlayer );

			SetContextThink( &CRobotDestructionVaultTrigger::StealPointsThink, 
							 gpGlobals->curtime + tf_rd_steal_rate.GetFloat(), 
							 "add_points_context" );

			return nPointsToSteal;
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void cc_tf_rd_max_points_override( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( CTFRobotDestructionLogic::GetRobotDestructionLogic() )
		CTFRobotDestructionLogic::GetRobotDestructionLogic()->SetMaxPoints( var.GetInt() );
}
ConVar tf_rd_max_points_override( "tf_rd_max_points_override", "0", FCVAR_GAMEDLL, "When changed, overrides the current max points", cc_tf_rd_max_points_override );
#endif