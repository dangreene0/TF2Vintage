//========= Copyright � Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================
#ifndef TF_LOGIC_ROBOT_DESTRUCTION_H
#define TF_LOGIC_ROBOT_DESTRUCTION_H

#include "tf_shareddefs.h"

#if defined(CLIENT_DLL)
#define CTFRobotDestruction_RobotSpawn	C_TFRobotDestruction_RobotSpawn
#define CTFRobotDestruction_RobotGroup	C_TFRobotDestruction_RobotGroup
#define CTFRobotDestructionLogic		C_TFRobotDestructionLogic
#endif

class CTFRobotDestruction_Robot;
class CTFRobotDestruction_RobotGroup;
class CTFRobotDestructionLogic;

enum ERobotType
{
	ROBOT_TYPE_SMALL,
	ROBOT_TYPE_MEDIUM,
	ROBOT_TYPE_LARGE,

	NUM_ROBOT_TYPES,
};
#if defined(GAME_DLL)
typedef struct RobotSpawnData_s
{
	RobotSpawnData_s &operator=( const RobotSpawnData_s &rhs )
	{
		m_eType = rhs.m_eType;
		m_nRobotHealth = rhs.m_nRobotHealth;
		m_nPoints = rhs.m_nPoints;
		m_nNumGibs = rhs.m_nNumGibs;
		m_pszPathName = rhs.m_pszPathName;
		m_pszGroupName = rhs.m_pszGroupName;

		return *this;
	}

	int m_eType;
	int m_nRobotHealth;
	int m_nPoints;
	int m_nNumGibs;
	const char *m_pszPathName;
	const char *m_pszGroupName;
} RobotSpawnData_t;
#endif

class CTFRobotDestruction_RobotSpawn : public CBaseEntity
{
	DECLARE_CLASS( CTFRobotDestruction_RobotSpawn, CBaseEntity );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();

	CTFRobotDestruction_RobotSpawn();

	virtual void	Activate( void );
	virtual void	Precache( void );
	virtual void	Spawn( void );
#if defined(GAME_DLL)
	virtual bool	ShouldCollide( int collisionGroup, int contentsMask ) const;

	void			ClearRobot( void );
	void			OnRobotKilled( void );
	void			SpawnRobot( void );

	void			InputSpawnRobot( inputdata_t &inputdata );

	CTFRobotDestruction_Robot *GetRobot( void ) const { return m_hRobot.Get(); }
	void			SetRobotGroup( CTFRobotDestruction_RobotGroup *pGroup ) { m_hRobotGroup.Set( pGroup ); }

private:
	CHandle<CTFRobotDestruction_Robot> m_hRobot;
	CHandle<CTFRobotDestruction_RobotGroup> m_hRobotGroup;
	RobotSpawnData_t m_spawnData;
	COutputEvent m_OnRobotKilled;
#endif
};

DECLARE_AUTO_LIST( IRobotDestructionGroupAutoList );
class CTFRobotDestruction_RobotGroup : public CBaseEntity, public IRobotDestructionGroupAutoList
{
#if defined(GAME_DLL)
	static float sm_flNextAllowedAttackAlertTime[TF_TEAM_COUNT];
#endif
	DECLARE_CLASS( CTFRobotDestruction_RobotGroup, CBaseEntity );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();

	CTFRobotDestruction_RobotGroup();
	virtual ~CTFRobotDestruction_RobotGroup();

#if defined(GAME_DLL)
	virtual void	Activate( void );
	virtual void	Spawn( void );
	virtual int		UpdateTransmitState( void );

	void			AddToGroup( CTFRobotDestruction_RobotSpawn *pSpawn );
	void			DisableUberForGroup( void );
	void			EnableUberForGroup( void );
	int				GetNumAliveBots( void );
	void			OnRobotAttacked( void );
	void			OnRobotKilled( void );
	void			OnRobotSpawned( void );
	void			RemoveFromGroup( CTFRobotDestruction_RobotSpawn *pSpawn );
	void			RespawnCountdownFinish( void );
	void			RespawnRobots( void );
	void			StartRespawnTimerIfNeeded( CTFRobotDestruction_RobotGroup *pGroup );
	void			StopRespawnTimer( void );
	void			UpdateState( void );
#else
	virtual int		GetTeamNumber( void ) const { return m_iTeamNum; }
	virtual void	PostDataUpdate( DataUpdateType_t updateType );
	virtual void	SetDormant( bool bDormant );
#endif
private:
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iTeamNum );
#ifdef GAME_DLL
	CUtlVector< CHandle<CTFRobotDestruction_RobotSpawn> > m_vecSpawns;

	int m_nTeamNumber;
	float m_flRespawnTime;
	string_t m_iszHudIcon;
	float m_flTeamRespawnReductionScale;

	COutputEvent m_OnRobotsRespawn;
	COutputEvent m_OnAllRobotsDead;
#endif

	CNetworkString( m_pszHudIcon, MAX_PATH );
	CNetworkVar( int, m_nGroupNumber );
	CNetworkVar( int, m_nState );
	CNetworkVar( float, m_flRespawnStartTime );
	CNetworkVar( float, m_flRespawnEndTime );
	CNetworkVar( float, m_flLastAttackedTime );
};

#endif