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

private:
	CHandle<CTFRobotDestruction_Robot> m_hRobot;
	CHandle<CTFRobotDestruction_RobotGroup> m_hRobotGroup;
	RobotSpawnData_t m_spawnData;
	COutputEvent m_OnRobotKilled;
#endif
};

#endif