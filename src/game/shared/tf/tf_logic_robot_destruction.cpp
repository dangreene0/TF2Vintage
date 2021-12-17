//========= Copyright � Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "tf_robot_destruction_robot.h"
#include "tf_logic_robot_destruction.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


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
		m_hRobot->SetModel( "TODO" );
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

