//========= Copyright � Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================
#ifndef TF_POPULATORS_H
#define TF_POPULATORS_H

#include "tf_populator_spawners.h"
#include "tf_bot.h"

class CPopulationManager;
class CWave;


enum RelativePositionType
{
	UNDEFINED = 0,
	AHEAD,
	BEHIND,
	ANYWHERE
};

struct EventInfo
{
	CFmtStr m_target;
	CFmtStr m_action;
};

enum SpawnLocationResult
{
	SPAWN_LOCATION_NOT_FOUND = 0,
	SPAWN_LOCATION_NAV,
	SPAWN_LOCATION_TELEPORTER
};

//-----------------------------------------------------------------------------
// Interface for all populators
class IPopulator
{
public:
	IPopulator( CPopulationManager *pManager )
	{
		m_manager = pManager;
		m_spawner = NULL;
	}
	virtual ~IPopulator()
	{
		if ( m_spawner )
		{
			delete m_spawner;
		}
		m_spawner = NULL;
	}

	virtual bool Parse( KeyValues *data ) = 0;

	virtual void PostInitialize( void ) { }
	virtual void Update( void ) { }
	virtual void UnpauseSpawning() {}

	virtual void OnPlayerKilled( CTFPlayer *pPlayer ) { }

	CPopulationManager *GetManager( void ) const { return m_manager; }

	virtual bool HasEventChangeAttributes( const char *pszEventName ) const
	{
		if ( m_spawner )
		{
			return m_spawner->HasEventChangeAttributes( pszEventName );
		}

		return false;
	}

	IPopulationSpawner *m_spawner;

private:
	CPopulationManager *m_manager;
};


class CSpawnLocation
{
public:
	CSpawnLocation();

	bool Parse( KeyValues *data );

	SpawnLocationResult FindSpawnLocation( Vector *vSpawnPosition );
	CTFNavArea *SelectSpawnArea( void ) const;

private:
	RelativePositionType m_eRelative;
	CUtlVector< CHandle<CTFTeamSpawn> > m_TeamSpawns;

	int m_nSpawnCount;
	int m_nRandomSeed;
	bool m_bClosestPointOnNav;
};

class CPopulatorInternalSpawnPoint : public CPointEntity
{
	DECLARE_CLASS( CPopulatorInternalSpawnPoint, CPointEntity );
};
extern CHandle< CPopulatorInternalSpawnPoint > g_internalSpawnPoint;


class CMissionPopulator : public IPopulator
{
public:
	CMissionPopulator( CPopulationManager *manager );
	virtual ~CMissionPopulator();

	virtual bool	Parse( KeyValues *data );

	virtual void	Update( void );
	virtual void	UnpauseSpawning( void );

	bool			UpdateMission( CTFBot::MissionType mission );
	bool			UpdateMissionDestroySentries( void );

	CTFBot::MissionType m_eMission;
	CSpawnLocation m_where;
	float m_flInitialCooldown;
	float m_flCooldownDuration;
	CountdownTimer m_cooldownTimer;
	CountdownTimer m_checkSentriesTimer;
	int m_nDesiredCount;

	int m_nStartWave;
	int m_nEndWave;
};


class CWaveSpawnPopulator : public IPopulator
{
public:
	CWaveSpawnPopulator( CPopulationManager *pManager );
	virtual ~CWaveSpawnPopulator();

	virtual bool	Parse( KeyValues *data );

	virtual void	Update( void );

	virtual void	OnPlayerKilled( CTFPlayer *pPlayer );

	void			ForceFinish( void );
	int				GetCurrencyAmountPerDeath( void );
	bool			IsFinishedSpawning( void );

	void			OnNonSupportWavesDone( void );

	enum InternalStateType
	{
		PENDING,
		PRE_SPAWN_DELAY,
		SPAWNING,
		WAIT_FOR_ALL_DEAD,
		DONE
	};
	void			SetState( InternalStateType eState );

	CSpawnLocation m_where;
	int m_totalCount;
	int m_remainingCount;
	int m_classCounts;
	int m_maxActive;
	int m_spawnCount;
	float m_waitBeforeStarting;
	float m_waitBetweenSpawns;
	bool m_bWaitBetweenSpawnsAfterDeath;
	CFmtStr m_startWaveWarningSound;
	EventInfo *m_startWaveEvent;
	CFmtStr m_firstSpawnWarningSound;
	EventInfo *m_firstSpawnEvent;
	CFmtStr m_lastSpawnWarningSound;
	EventInfo *m_lastSpawnEvent;
	CFmtStr m_doneWarningSound;
	EventInfo *m_doneEvent;
	int m_totalCurrency;
	int m_remainingCurrency;
	CUtlString m_name;
	CUtlString m_waitForAllSpawned;
	CUtlString m_waitForAllDead;
	CountdownTimer m_timer;
	CUtlVector<EHANDLE> m_activeSpawns;
	int m_countSpawnedSoFar;
	char gap4C8[4];
	bool m_bSupportWave;
	bool m_bLimitedSupport;
	CWave *m_parentWave;
	bool m_bRandomSpawn;
	InternalStateType m_eState;
};


class CPeriodicSpawnPopulator : public IPopulator
{
public:
	CPeriodicSpawnPopulator( CPopulationManager *manager );
	virtual ~CPeriodicSpawnPopulator();

	virtual bool Parse( KeyValues *data );

	virtual void PostInitialize( void );		// create initial population at start of scenario
	virtual void Update( void );			// continuously invoked to modify population over time
	virtual void UnpauseSpawning( void );

	CSpawnLocation m_where;
	float m_flMinInterval;
	float m_flMaxInterval;
	CountdownTimer m_timer;
};


class CRandomPlacementPopulator : public IPopulator
{
public:
	CRandomPlacementPopulator( CPopulationManager *manager );
	virtual ~CRandomPlacementPopulator();

	virtual bool Parse( KeyValues *data );

	virtual void PostInitialize( void );		// create initial population at start of scenario
};


class CWave : public IPopulator
{
public:
	CWave( CPopulationManager *pManager );
	virtual ~CWave();

	virtual bool Parse( KeyValues *data );
	virtual void Update( void );

	virtual void OnPlayerKilled( CTFPlayer *pPlayer );

	virtual bool HasEventChangeAttributes( const char *pszEventName ) const;
	
	void AddClassType( string_t iszClassIconName, int nCount, unsigned int iFlags );
	CWaveSpawnPopulator *FindWaveSpawnPopulator( const char *name );
	void ForceFinish();
	void ForceReset();
	bool IsDoneWithNonSupportWaves( void );

	void ActiveWaveUpdate( void );
	void WaveCompleteUpdate( void );
	void WaveIntermissionUpdate( void );
};

#endif