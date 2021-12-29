//========= Copyright � Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "nav_mesh/tf_nav_mesh.h"
#include "tf_team.h"
#include "tf_obj_teleporter.h"
#include "tf_obj_sentrygun.h"
#include "tf_populators.h"
#include "tf_population_manager.h"
#include "eventqueue.h"
#include "tier1/UtlSortVector.h"

extern ConVar tf_populator_debug;
extern ConVar tf_populator_active_buffer_range;

static CHandle<CBaseEntity> s_lastTeleporter = NULL;
static float s_flLastTeleportTime = -1;

LINK_ENTITY_TO_CLASS( populator_internal_spawn_point, CPopulatorInternalSpawnPoint );
CHandle<CPopulatorInternalSpawnPoint> g_internalSpawnPoint = NULL;

class CTFNavAreaIncursionLess
{
public:
	bool Less( const CTFNavArea *a, const CTFNavArea *b, void *pCtx )
	{
		return a->GetIncursionDistance( TF_TEAM_BLUE ) < b->GetIncursionDistance( TF_TEAM_BLUE );
	}
};

//-----------------------------------------------------------------------------
// Purpose: Fire off output events
//-----------------------------------------------------------------------------
void FireEvent( EventInfo *eventInfo, const char *eventName )
{
	if ( eventInfo )
	{
		CBaseEntity *targetEntity = gEntList.FindEntityByName( NULL, eventInfo->m_target );
		if ( !targetEntity )
		{
			Warning( "WaveSpawnPopulator: Can't find target entity '%s' for %s\n", eventInfo->m_target.Access(), eventName );
		}
		else
		{
			g_EventQueue.AddEvent( targetEntity, eventInfo->m_action, 0.0f, NULL, NULL );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create output event pairings
//-----------------------------------------------------------------------------
EventInfo *ParseEvent( KeyValues *data )
{
	EventInfo *eventInfo = new EventInfo();

	FOR_EACH_SUBKEY( data, pSubKey )
	{
		const char *pszKey = pSubKey->GetName();
		if ( !Q_stricmp( pszKey, "Target" ) )
		{
			eventInfo->m_target.sprintf( "%s", pSubKey->GetString() );
		}
		else if ( !Q_stricmp( pszKey, "Action" ) )
		{
			eventInfo->m_action.sprintf( "%s", pSubKey->GetString() );
		}
		else
		{
			Warning( "Unknown field '%s' in WaveSpawn event definition.\n", pSubKey->GetString() );
			delete eventInfo;
			return NULL;
		}
	}

	return eventInfo;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSpawnLocation::CSpawnLocation()
{
	m_nRandomSeed = RandomInt( 0, 9999 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSpawnLocation::Parse( KeyValues *data )
{
	const char *pszKey = data->GetName();
	const char *pszValue = data->GetString();

	if ( !V_stricmp( pszKey, "Where" ) || !V_stricmp( pszKey, "ClosestPoint" ) )
	{
		if ( !V_stricmp( pszValue, "Ahead" ) )
		{
			m_eRelative = AHEAD;
		}
		else if ( !V_stricmp( pszValue, "Behind" ) )
		{
			m_eRelative = BEHIND;
		}
		else if ( !V_stricmp( pszValue, "Anywhere" ) )
		{
			m_eRelative = ANYWHERE;
		}
		else
		{
			m_bClosestPointOnNav = V_stricmp( pszKey, "ClosestPoint" ) == 0;

			// collect entities with given name
			bool bFound = false;
			for ( int i=0; i < ITFTeamSpawnAutoList::AutoList().Count(); ++i )
			{
				CTFTeamSpawn *pTeamSpawn = static_cast<CTFTeamSpawn *>( ITFTeamSpawnAutoList::AutoList()[i] );
				if ( !V_stricmp( STRING( pTeamSpawn->GetEntityName() ), pszValue ) )
				{
					m_TeamSpawns.AddToTail( pTeamSpawn );
					bFound = true;
				}
			}

			if ( !bFound )
			{
				Warning( "Invalid Where argument '%s'\n", pszValue );
				return false;
			}
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
SpawnLocationResult CSpawnLocation::FindSpawnLocation( Vector *vSpawnPosition )
{
	return SPAWN_LOCATION_NOT_FOUND;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFNavArea *CSpawnLocation::SelectSpawnArea( void ) const
{
	VPROF_BUDGET( __FUNCTION__, "NextBot" );

	if ( m_eRelative == UNDEFINED )
		return nullptr;

	CUtlSortVector<CTFNavArea *, CTFNavAreaIncursionLess> theaterAreas;

	CUtlVector<INextBot *> bots;
	TheNextBots().CollectAllBots( &bots );

	CTFNavArea::MakeNewTFMarker();
	FOR_EACH_VEC( bots, i )
	{
		CTFBot *pBot = ToTFBot( bots[i]->GetEntity() );
		if ( pBot == nullptr )
			continue;

		if ( !pBot->IsAlive() )
			continue;

		if ( !pBot->GetLastKnownArea() )
			continue;

		CUtlVector<CTFNavArea *> nearbyAreas;
		CollectSurroundingAreas( &nearbyAreas, pBot->GetLastKnownArea(), tf_populator_active_buffer_range.GetFloat() );

		FOR_EACH_VEC( nearbyAreas, j )
		{
			CTFNavArea *pArea = nearbyAreas[i];
			if ( !pArea->IsTFMarked() )
			{
				pArea->TFMark();

				if ( pArea->IsPotentiallyVisibleToTeam( TF_TEAM_BLUE ) )
					continue;

				if ( !pArea->IsValidForWanderingPopulation() )
					continue;

				theaterAreas.Insert( pArea );

				if ( tf_populator_debug.GetBool() )
					TheNavMesh->AddToSelectedSet( pArea );
			}
		}
	}

	if ( theaterAreas.Count() == 0 )
	{
		if ( tf_populator_debug.GetBool() )
			DevMsg( "%3.2f: SelectSpawnArea: Empty theater!\n", gpGlobals->curtime );

		return nullptr;
	}

	for ( int i=0; i < 5; ++i )
	{
		int which = 0;
		switch ( m_eRelative )
		{
			case AHEAD:
				which = Max( RandomFloat( 0.0f, 1.0f ), RandomFloat( 0.0f, 1.0f ) ) * theaterAreas.Count();
				break;

			case BEHIND:
				which = ( 1.0f - Max( RandomFloat( 0.0f, 1.0f ), RandomFloat( 0.0f, 1.0f ) ) ) * theaterAreas.Count();
				break;

			case ANYWHERE:
				which = RandomFloat( 0.0f, 1.0f ) * theaterAreas.Count();
				break;
		}

		if ( which >= theaterAreas.Count() )
			which = theaterAreas.Count() - 1;

		return theaterAreas[which];
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMissionPopulator::CMissionPopulator( CPopulationManager *pManager )
	: IPopulator( pManager )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMissionPopulator::~CMissionPopulator()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMissionPopulator::Parse( KeyValues *data )
{
	int nWaveDuration = 99999;
	FOR_EACH_SUBKEY( data, pSubKey )
	{
		const char *pszKey = pSubKey->GetName();
		if ( !V_stricmp( pszKey, "Objective" ) )
		{
			if ( !V_stricmp( pSubKey->GetString(), "DestroySentries" ) )
			{
				m_eMission = CTFBot::MissionType::DESTROY_SENTRIES;
			}
			else if ( !V_stricmp( pSubKey->GetString(), "Sniper" ) )
			{
				m_eMission = CTFBot::MissionType::SNIPER;
			}
			else if ( !V_stricmp( pSubKey->GetString(), "Spy" ) )
			{
				m_eMission = CTFBot::MissionType::SPY;
			}
			else if ( !V_stricmp( pSubKey->GetString(), "Engineer" ) )
			{
				m_eMission = CTFBot::MissionType::ENGINEER;
			}
			else if ( !V_stricmp( pSubKey->GetString(), "SeekAndDestroy" ) )
			{
				m_eMission = CTFBot::MissionType::DESTROY_SENTRIES;
			}
			else
			{
				Warning( "Invalid mission '%s'\n", data->GetString() );
				return false;
			}
		}
		else if ( !V_stricmp( pszKey, "InitialCooldown" ) )
		{
			m_flInitialCooldown = data->GetFloat();
		}
		else if ( !V_stricmp( pszKey, "CooldownTime" ) )
		{
			m_flCooldownDuration = data->GetFloat();
		}
		else if ( !V_stricmp( pszKey, "BeginAtWave" ) )
		{
			m_nStartWave = data->GetInt() - 1;
		}
		else if ( !V_stricmp( pszKey, "RunForThisManyWaves" ) )
		{
			nWaveDuration = data->GetInt();
		}
		else if ( !V_stricmp( pszKey, "DesiredCount" ) )
		{
			m_nDesiredCount = data->GetInt();
		}
		else
		{
			m_spawner = IPopulationSpawner::ParseSpawner( this, pSubKey );
			if ( m_spawner == NULL )
			{
				Warning( "Unknown attribute '%s' in Mission definition.\n", pszKey );
			}
		}
	}

	m_nEndWave = m_nStartWave + nWaveDuration;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMissionPopulator::Update( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMissionPopulator::UnpauseSpawning( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMissionPopulator::UpdateMission( CTFBot::MissionType mission )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMissionPopulator::UpdateMissionDestroySentries( void )
{
	VPROF_BUDGET( __FUNCTION__, "NextBot" );

	if ( !m_cooldownTimer.IsElapsed() || !m_checkSentriesTimer.IsElapsed() )
		return false;


	m_checkSentriesTimer.Start( RandomFloat( 5.0f, 10.0f ) );

	int nDmgLimit = 0;
	int nKillLimit = 0;
	GetManager()->GetSentryBusterDamageAndKillThreshold( nDmgLimit, nKillLimit );

	CUtlVector<CObjectSentrygun *> dangerousSentries;
	FOR_EACH_VEC( IBaseObjectAutoList::AutoList(), i )
	{
		CBaseObject *pObj = static_cast<CBaseObject *>( IBaseObjectAutoList::AutoList()[i] );
		if ( pObj->ObjectType() != OBJ_SENTRYGUN )
			continue;
		
		if ( pObj->IsDisposableBuilding() )
			continue;

		if ( pObj->GetTeamNumber() != TF_TEAM_MVM_BOTS )
			continue;

		CTFPlayer *pOwner = pObj->GetOwner();
		if ( pOwner )
		{
		}
	}


	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWaveSpawnPopulator::CWaveSpawnPopulator( CPopulationManager *pManager )
	: IPopulator( pManager )
{
	m_maxActive = 999;
	m_spawnCount = 1;
	m_totalCurrency = -1;

	m_startWaveEvent = NULL;
	m_firstSpawnEvent = NULL;
	m_lastSpawnEvent = NULL;
	m_doneEvent = NULL;
	m_parentWave = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWaveSpawnPopulator::~CWaveSpawnPopulator()
{
	delete m_startWaveEvent;
	delete m_firstSpawnEvent;
	delete m_lastSpawnEvent;
	delete m_doneEvent;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWaveSpawnPopulator::Parse( KeyValues *data )
{
	KeyValues *pTemplate = data->FindKey( "Template" );
	if ( pTemplate )
	{
		KeyValues *pTemplateKV = GetManager()->GetTemplate( pTemplate->GetString() );
		if ( pTemplateKV )
		{
			if ( !Parse( pTemplateKV ) )
				return false;
		}
		else
		{
			Warning( "Unknown Template '%s' in WaveSpawn definition\n", pTemplate->GetString() );
		}
	}

	FOR_EACH_SUBKEY( data, pSubKey )
	{
		if ( m_where.Parse( pSubKey ) )
			continue;

		const char *pszKey = pSubKey->GetName();
		if ( !V_stricmp( pszKey, "TotalCount" ) )
		{
			m_totalCount = data->GetInt();
		}
		else if ( !V_stricmp( pszKey, "MaxActive" ) )
		{
			m_maxActive = data->GetInt();
		}
		else if ( !V_stricmp( pszKey, "SpawnCount" ) )
		{
			m_spawnCount = data->GetInt();
		}
		else if ( !V_stricmp( pszKey, "WaitBeforeStarting" ) )
		{
			m_waitBeforeStarting = data->GetFloat();
		}
		else if ( !V_stricmp( pszKey, "WaitBetweenSpawns" ) )
		{
			if ( m_waitBetweenSpawns != 0 && m_bWaitBetweenSpawnsAfterDeath )
			{
				Warning( "Already specified WaitBetweenSpawnsAfterDeath time, WaitBetweenSpawns won't be used\n" );
				continue;
			}

			m_waitBetweenSpawns = data->GetFloat();
		}
		else if ( !V_stricmp( pszKey, "WaitBetweenSpawnsAfterDeath" ) )
		{
			if ( m_waitBetweenSpawns != 0 )
			{
				Warning( "Already specified WaitBetweenSpawns time, WaitBetweenSpawnsAfterDeath won't be used\n" );
				continue;
			}

			m_bWaitBetweenSpawnsAfterDeath = true;
			m_waitBetweenSpawns = data->GetFloat();
		}
		else if ( !V_stricmp( pszKey, "StartWaveWarningSound" ) )
		{
			m_startWaveWarningSound.sprintf( "%s", data->GetString() );
		}
		else if ( !V_stricmp( pszKey, "StartWaveOutput" ) )
		{
			m_startWaveEvent = ParseEvent( data );
		}
		else if ( !V_stricmp( pszKey, "FirstSpawnWarningSound" ) )
		{
			m_firstSpawnWarningSound.sprintf( "%s", data->GetString() );
		}
		else if ( !V_stricmp( pszKey, "FirstSpawnOutput" ) )
		{
			m_firstSpawnEvent = ParseEvent( data );
		}
		else if ( !V_stricmp( pszKey, "LastSpawnWarningSound" ) )
		{
			m_lastSpawnWarningSound.sprintf( "%s", data->GetString() );
		}
		else if ( !V_stricmp( pszKey, "LastSpawnOutput" ) )
		{
			m_lastSpawnEvent = ParseEvent( data );
		}
		else if ( !V_stricmp( pszKey, "DoneWarningSound" ) )
		{
			m_doneWarningSound.sprintf( "%s", data->GetString() );
		}
		else if ( !V_stricmp( pszKey, "DoneOutput" ) )
		{
			m_doneEvent = ParseEvent( data );
		}
		else if ( !V_stricmp( pszKey, "TotalCurrency" ) )
		{
			m_totalCurrency = data->GetInt();
		}
		else if ( !V_stricmp( pszKey, "Name" ) )
		{
			m_name = data->GetString();
		}
		else if ( !V_stricmp( pszKey, "WaitForAllSpawned" ) )
		{
			m_waitForAllSpawned = data->GetString();
		}
		else if ( !V_stricmp( pszKey, "WaitForAllDead" ) )
		{
			m_waitForAllDead = data->GetString();
		}
		else if ( !V_stricmp( pszKey, "Support" ) )
		{
			m_bLimitedSupport = !V_stricmp( data->GetString(), "Limited" );
			m_bSupportWave = true;
		}
		else if ( !V_stricmp( pszKey, "RandomSpawn" ) )
		{
			m_bRandomSpawn = data->GetBool();
		}
		else
		{
			m_spawner = IPopulationSpawner::ParseSpawner( this, data );

			if ( m_spawner == NULL )
			{
				Warning( "Unknown attribute '%s' in WaveSpawn definition.\n", pszKey );
			}
		}

		// These allow us to avoid rounding errors later when divvying money to bots
		m_remainingCurrency = m_totalCurrency;
		m_remainingCount = m_totalCount;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaveSpawnPopulator::Update( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaveSpawnPopulator::OnPlayerKilled( CTFPlayer *pPlayer )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaveSpawnPopulator::ForceFinish( void )
{
	if ( m_eState < WAIT_FOR_ALL_DEAD )
	{
		SetState( WAIT_FOR_ALL_DEAD );
	}
	else if ( m_eState != WAIT_FOR_ALL_DEAD )
	{
		SetState( DONE );
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CWaveSpawnPopulator::GetCurrencyAmountPerDeath( void )
{
	int nCurrency = 0;
	return nCurrency;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWaveSpawnPopulator::IsFinishedSpawning( void )
{
	if ( m_bSupportWave && !m_bLimitedSupport )
		return false;

	return ( m_countSpawnedSoFar >= m_totalCount );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaveSpawnPopulator::OnNonSupportWavesDone( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaveSpawnPopulator::SetState( InternalStateType eState )
{
	m_eState = eState;

	if ( eState == WAIT_FOR_ALL_DEAD )
	{
		if ( m_lastSpawnWarningSound.Length() > 0 )
		{
			TFGameRules()->BroadcastSound( 255, m_lastSpawnWarningSound );
		}

		FireEvent( m_lastSpawnEvent, "LastSpawnOutput" );

		if ( tf_populator_debug.GetBool() )
		{
			DevMsg( "%3.2f: WaveSpawn(%s) started WAIT_FOR_ALL_DEAD\n", gpGlobals->curtime, m_name.IsEmpty() ? "" : m_name.Get() );
		}
	}
	else if ( eState == DONE )
	{
		if ( m_doneWarningSound.Length() > 0 )
		{
			TFGameRules()->BroadcastSound( 255, m_doneWarningSound );
		}

		FireEvent( m_doneEvent, "DoneOutput" );

		if ( tf_populator_debug.GetBool() )
		{
			DevMsg( "%3.2f: WaveSpawn(%s) DONE\n", gpGlobals->curtime, m_name.IsEmpty() ? "" : m_name.Get() );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPeriodicSpawnPopulator::CPeriodicSpawnPopulator( CPopulationManager *pManager )
	: IPopulator( pManager )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPeriodicSpawnPopulator::~CPeriodicSpawnPopulator()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPeriodicSpawnPopulator::Parse( KeyValues *data )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPeriodicSpawnPopulator::PostInitialize( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPeriodicSpawnPopulator::Update( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPeriodicSpawnPopulator::UnpauseSpawning( void )
{
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRandomPlacementPopulator::CRandomPlacementPopulator( CPopulationManager *pManager )
	: IPopulator( pManager )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRandomPlacementPopulator::~CRandomPlacementPopulator()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CRandomPlacementPopulator::Parse( KeyValues *data )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRandomPlacementPopulator::PostInitialize( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWave::CWave( CPopulationManager *pManager )
	: IPopulator( pManager )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWave::~CWave()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWave::Parse( KeyValues *data )
{
	FOR_EACH_SUBKEY( data, pSubKey )
	{
		const char *pszKey = pSubKey->GetName();
		if ( !V_stricmp( pszKey, "WaveSpawn" ) )
		{
			CWaveSpawnPopulator *pWavePopulator = new CWaveSpawnPopulator( GetManager() );
			if ( !pWavePopulator->Parse( pSubKey ) )
			{
				Warning( "Error reading WaveSpawn definition\n" );
				return false;
			}

			m_WaveSpawns.AddToTail( pWavePopulator );

			if ( pWavePopulator->m_spawner )
			{
				if ( pWavePopulator->m_spawner->IsVarious() )
				{
					for ( int i = 0; i < pWavePopulator->m_totalCount; ++i )
					{
						unsigned int iFlags = pWavePopulator->m_bSupportWave ? MVM_CLASS_FLAG_SUPPORT : MVM_CLASS_FLAG_NORMAL;
						if ( pWavePopulator->m_spawner->IsMiniBoss( i ) )
						{
							iFlags |= MVM_CLASS_FLAG_MINIBOSS;
						}
						if ( pWavePopulator->m_spawner->HasAttribute( CTFBot::AttributeType::ALWAYSCRIT, i ) )
						{
							iFlags |= MVM_CLASS_FLAG_ALWAYSCRIT;
						}
						if ( pWavePopulator->m_bLimitedSupport )
						{
							iFlags |= MVM_CLASS_FLAG_SUPPORT_LIMITED;
						}
						AddClassType( pWavePopulator->m_spawner->GetClassIcon( i ), 1, iFlags );
					}
				}
				else
				{
					unsigned int iFlags = pWavePopulator->m_bSupportWave ? MVM_CLASS_FLAG_SUPPORT : MVM_CLASS_FLAG_NORMAL;
					if ( pWavePopulator->m_spawner->IsMiniBoss() )
					{
						iFlags |= MVM_CLASS_FLAG_MINIBOSS;
					}
					if ( pWavePopulator->m_spawner->HasAttribute( CTFBot::AttributeType::ALWAYSCRIT ) )
					{
						iFlags |= MVM_CLASS_FLAG_ALWAYSCRIT;
					}
					if ( pWavePopulator->m_bLimitedSupport )
					{
						iFlags |= MVM_CLASS_FLAG_SUPPORT_LIMITED;
					}
					AddClassType( pWavePopulator->m_spawner->GetClassIcon(), pWavePopulator->m_totalCount, iFlags );
				}
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWave::Update( void )
{
	VPROF_BUDGET( __FUNCTION__, "NextBot" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWave::OnPlayerKilled( CTFPlayer *pPlayer )
{
	FOR_EACH_VEC( m_WaveSpawns, i )
	{
		m_WaveSpawns[i]->OnPlayerKilled( pPlayer );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWave::HasEventChangeAttributes( const char *pszEventName ) const
{
	bool bHasEventChangeAttributes = false;
	FOR_EACH_VEC( m_WaveSpawns, i )
	{
		bHasEventChangeAttributes |= m_WaveSpawns[i]->HasEventChangeAttributes( pszEventName );
	}

	return bHasEventChangeAttributes;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWave::AddClassType( string_t iszClassIconName, int nCount, unsigned int iFlags )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWaveSpawnPopulator *CWave::FindWaveSpawnPopulator( const char *name )
{
	FOR_EACH_VEC( m_WaveSpawns, i )
	{
		CWaveSpawnPopulator *pPopulator = m_WaveSpawns[i];
		if ( !V_stricmp( pPopulator->m_name, name ) )
			return pPopulator;
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWave::ForceFinish()
{
	FOR_EACH_VEC( m_WaveSpawns, i )
	{
		m_WaveSpawns[i]->ForceFinish();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWave::ForceReset()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWave::IsDoneWithNonSupportWaves( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWave::ActiveWaveUpdate( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWave::WaveCompleteUpdate( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWave::WaveIntermissionUpdate( void )
{
}
