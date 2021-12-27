//========= Copyright © Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "tf_population_manager.h"
#include "tf_objective_resource.h"

extern ConVar tf_mvm_respec_limit;
extern ConVar tf_mvm_respec_credit_goal;

ConVar tf_mvm_default_sentry_buster_damage_dealt_threshold( "tf_mvm_default_sentry_buster_damage_dealt_threshold", "3000", FCVAR_DEVELOPMENTONLY );
ConVar tf_mvm_default_sentry_buster_kill_threshold( "tf_mvm_default_sentry_buster_kill_threshold", "15", FCVAR_DEVELOPMENTONLY );

ConVar tf_mvm_endless_force_on( "tf_mvm_endless_force_on", "0", FCVAR_DONTRECORD | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Force MvM Endless mode on" );
ConVar tf_mvm_endless_wait_time( "tf_mvm_endless_wait_time", "5.0f", FCVAR_DONTRECORD | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );
ConVar tf_mvm_endless_bomb_reset( "tf_mvm_endless_bomb_reset", "5", FCVAR_DONTRECORD | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Number of Waves to Complete before bomb reset" );
ConVar tf_mvm_endless_bot_cash( "tf_mvm_endless_bot_cash", "120", FCVAR_DONTRECORD | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "In Endless, number of credits bots get per wave" );
ConVar tf_mvm_endless_tank_boost( "tf_mvm_endless_tank_boost", "0.2", FCVAR_DONTRECORD | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "In Endless, amount of extra health for the tank per wave" );

ConVar tf_populator_debug( "tf_populator_debug", "0", FCVAR_DEVELOPMENTONLY );
ConVar tf_populator_health_multiplier( "tf_populator_health_multiplier", "1.0", FCVAR_DONTRECORD | FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar tf_populator_damage_multiplier( "tf_populator_damage_multiplier", "1.0", FCVAR_DONTRECORD | FCVAR_REPLICATED | FCVAR_CHEAT );

void MvMMinibossScaleChangedCallBack( IConVar *pVar, const char *pOldString, float flOldValue );
ConVar tf_mvm_miniboss_scale( "tf_mvm_miniboss_scale", "1.75", FCVAR_REPLICATED | FCVAR_CHEAT, 
							  "Full body scale for minibosses.", MvMMinibossScaleChangedCallBack );

void MvMMissionCycleFileChangedCallback( IConVar *var, const char *pOldString, float flOldValue );
ConVar tf_mvm_missioncyclefile( "tf_mvm_missioncyclefile", "tf_mvm_missioncycle.res", FCVAR_NONE, 
								"Name of the .res file used to cycle mvm misisons", MvMMissionCycleFileChangedCallback );

void MvMSkillChangedCallback( IConVar *pVar, const char *pOldString, float flOldValue );
ConVar tf_mvm_skill( "tf_mvm_skill", "3", FCVAR_DONTRECORD | FCVAR_REPLICATED | FCVAR_CHEAT, 
					 "Sets the challenge level of the invading bot army. 1 = easiest, 3 = normal, 5 = hardest", 
					 true, 1, true, 5, MvMSkillChangedCallback );

void MvMMinibossScaleChangedCallBack( IConVar *pVar, const char *pOldString, float flOldValue )
{
	ConVarRef cvar( pVar );
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		CTFBot *pBot = ToTFBot( UTIL_PlayerByIndex( i ) );
		if ( pBot /* && pPlayer->IsMiniBoss()*/ )
		{
			pBot->SetModelScale( cvar.GetFloat(), 1.0f );
		}
	}
}

void MvMMissionCycleFileChangedCallback( IConVar *var, const char *pOldString, float flOldValue )
{
	if ( g_pPopulationManager )
	{
		g_pPopulationManager->LoadMissionCycleFile();
	}
}

void MvMSkillChangedCallback( IConVar *pVar, const char *pOldString, float flOldValue )
{
	ConVarRef cvar( pVar );
	float flHealth = 1.0f;	// Health modifier for bots
	float flDamage = 1.0f;	// Damage modifier in bot vs player

	switch ( cvar.GetInt() )
	{
		case 1:
		{
			flHealth = 0.75f;
			flDamage = 0.75f;
			break;
		}
		case 2:
		{
			flHealth = 0.9f;
			flDamage = 0.9f;
			break;
		}
		// "Normal"
		case 3:
		{
			flHealth = 1.0f;
			flDamage = 1.0f;
			break;
		}
		case 4:
		{
			flHealth = 1.10f;
			flDamage = 1.10f;
			break;
		}
		case 5:
		{
			flHealth = 1.25f;
			flDamage = 1.25f;
			break;
		}
	}

	tf_populator_health_multiplier.SetValue( flHealth );
	tf_populator_damage_multiplier.SetValue( flDamage );
}


CPopulationManager *g_pPopulationManager = NULL;


BEGIN_DATADESC( CPopulationManager )
	DEFINE_THINKFUNC( Update ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( info_populator, CPopulationManager );

PRECACHE_REGISTER( info_populator );


CUtlVector<CPopulationManager::CheckpointSnapshotInfo *> CPopulationManager::sm_checkpointSnapshots;


static int s_iLastKnownMissionCategory = 1;
static int s_iLastKnownMission = 1;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPopulationManager::CPopulationManager()
{
	Assert( g_pPopulationManager == NULL );
	g_pPopulationManager = this;

	m_iSentryBusterDamageDealtThreshold = tf_mvm_default_sentry_buster_damage_dealt_threshold.GetInt();
	m_iSentryBusterKillThreshold = tf_mvm_default_sentry_buster_kill_threshold.GetInt();

	SetThink( &CPopulationManager::Update );
	SetNextThink( gpGlobals->curtime );

	ListenForGameEvent( "pve_win_panel" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPopulationManager::~CPopulationManager()
{
	Assert( g_pPopulationManager == this );
	g_pPopulationManager = NULL;
}


void CPopulationManager::FireGameEvent( IGameEvent *event )
{
	const char *pszName = event->GetName();

	if ( FStrEq( pszName, "pve_win_panel" ) )
	{
		MarkAllCurrentPlayersSafeToLeave();
	}
}

bool CPopulationManager::Initialize( void )
{
	if ( ( TheNavMesh == NULL ) || ( TheNavMesh->GetNavAreaCount() <= 0 ) )
	{
		Warning( "No Nav Mesh CPopulationManager::Initialize for %s", m_szPopfileFull );
		return false;
	}

	Reset();

	if ( !Parse() )
	{
		Warning( "Parse Failed in CPopulationManager::Initialize for %s", m_szPopfileFull );
		return false;
	}

	if ( TFGameRules()->State_Get() == GR_STATE_PREGAME )
	{
		ClearCheckpoint();
		m_nCurrentWaveIndex = 0;
		//m_nNumConsecutiveWipes = 0;
		//m_pMVMStats->SetCurrentWave( m_iCurrentWaveIndex );
	}
	else
	{
		RestoreCheckpoint();

		//if ( m_bIsInitialized && ( m_iCurrentWaveIndex > 0 || m_nNumConsecutiveWipes > 1 ) )
			//m_pMVMStats->RoundEvent_WaveEnd( false );

		IGameEvent *event = gameeventmanager->CreateEvent( "mvm_wave_failed" );
		if ( event )
		{
			gameeventmanager->FireEvent( event );
		}
	}

	if ( IsInEndlessWaves() )
		EndlessRollEscalation();

	UpdateObjectiveResource();

	DebugWaveStats();

	if ( tf_mvm_respec_limit.GetBool() )
	{
		const int nAmount = tf_mvm_respec_limit.GetInt() + 1;
		tf_mvm_respec_credit_goal.SetValue( GetTotalPopFileCurrency() / nAmount );
	}

	PostInitialize();
	m_bIsInitialized = true;

	return true;
}

void CPopulationManager::Precache( void )
{
}

void CPopulationManager::Reset( void )
{
}

void CPopulationManager::Spawn( void )
{
	BaseClass::Spawn();

	Initialize();
}

void CPopulationManager::AddPlayerCurrencySpent( CTFPlayer *pPlayer, int nSpent )
{
}

void CPopulationManager::AddRespecToPlayer( CTFPlayer *pPlayer )
{
}

void CPopulationManager::AdjustMinPlayerSpawnTime( void )
{
}

void CPopulationManager::AllocateBots( void )
{
}

void CPopulationManager::ClearCheckpoint( void )
{
}

void CPopulationManager::CollectMvMBots( CUtlVector<CTFPlayer *> *vecBotsOut )
{
}

void CPopulationManager::CycleMission( void )
{
}

void CPopulationManager::DebugWaveStats( void )
{
}

void CPopulationManager::EndlessFlagHasReset( void )
{
}

void CPopulationManager::EndlessParseBotUpgrades( void )
{
}

void CPopulationManager::EndlessRollEscalation( void )
{
}

void CPopulationManager::EndlessSetAttributesForBot( CTFBot *pBot )
{
}

bool CPopulationManager::EndlessShouldResetFlag( void )
{
	return false;
}

CPopulationManager::CheckpointSnapshotInfo *CPopulationManager::FindCheckpointSnapshot( CSteamID steamID ) const
{
	FOR_EACH_VEC( sm_checkpointSnapshots, i )
	{

	}

	return nullptr;
}

CPopulationManager::CheckpointSnapshotInfo *CPopulationManager::FindCheckpointSnapshot( CTFPlayer *pPlayer ) const
{
	return nullptr;
}

void CPopulationManager::FindDefaultPopulationFileShortNames( CUtlVector<CUtlString> &vecNames )
{
}

CPopulationManager::PlayerUpgradeHistory *CPopulationManager::FindOrAddPlayerUpgradeHistory( CSteamID steamID ) const
{
	return nullptr;
}

CPopulationManager::PlayerUpgradeHistory *CPopulationManager::FindOrAddPlayerUpgradeHistory( CTFPlayer *pPlayer ) const
{
	return nullptr;
}

bool CPopulationManager::FindPopulationFileByShortName( char const *pszName, CUtlString *outFullName )
{
	return false;
}

void CPopulationManager::ForgetOtherBottleUpgrades( CTFPlayer *pPlayer, CEconItemView *pItem, int nUpgradeKept )
{
}

void CPopulationManager::GameRulesThink( void )
{
	// Some MM logic goes here
}

CWave *CPopulationManager::GetCurrentWave( void )
{
	if ( !m_bIsInitialized || m_vecWaves.Count() == 0 )
		return nullptr;

	if ( IsInEndlessWaves() )
	{
		return m_vecWaves[ m_nCurrentWaveIndex % m_vecWaves.Count() ];
	}
	else if ( m_nCurrentWaveIndex < m_vecWaves.Count() )
	{
		return m_vecWaves[ m_nCurrentWaveIndex ];
	}

	return nullptr;
}

float CPopulationManager::GetDamageMultiplier( void ) const
{
	return tf_populator_damage_multiplier.GetFloat();
}

float CPopulationManager::GetHealthMultiplier( bool bTankMultiplier ) const
{
	if ( bTankMultiplier && IsInEndlessWaves() )
		return tf_populator_health_multiplier.GetFloat() + m_nCurrentWaveIndex * tf_mvm_endless_tank_boost.GetFloat();

	return tf_populator_health_multiplier.GetFloat();
}

int CPopulationManager::GetNumBuybackCreditsForPlayer( CTFPlayer *pPlayer )
{
	return 0;
}

int CPopulationManager::GetNumRespecsAvailableForPlayer( CTFPlayer *pPlayer )
{
	return 0;
}

int CPopulationManager::GetPlayerCurrencySpent( CTFPlayer *pPlayer )
{
	return 0;
}

CUtlVector<CUpgradeInfo> *CPopulationManager::GetPlayerUpgradeHistory( CTFPlayer *pPlayer )
{
	return nullptr;
}

char const *CPopulationManager::GetPopulationFilename( void ) const
{
	return nullptr;
}

char const *CPopulationManager::GetPopulationFilenameShort( void ) const
{
	return nullptr;
}

void CPopulationManager::GetSentryBusterDamageAndKillThreshold( int &nNumDamage, int &nNumKills )
{
}

int CPopulationManager::GetTotalPopFileCurrency( void )
{
	return 0;
}

bool CPopulationManager::HasEventChangeAttributes( char const *psz )
{
	return false;
}

bool CPopulationManager::IsInEndlessWaves( void ) const
{
	return ( m_bEndlessWavesOn || tf_mvm_endless_force_on.GetBool() ) && !m_vecWaves.IsEmpty();
}

bool CPopulationManager::IsPlayerBeingTrackedForBuybacks( CTFPlayer *pPlayer )
{
	return false;
}

bool CPopulationManager::IsValidMvMMap( char const *pszMapName )
{
	return false;
}

void CPopulationManager::JumpToWave( uint nWave, float f1 )
{
}

void CPopulationManager::LoadLastKnownMission( void )
{
}

void CPopulationManager::LoadMissionCycleFile( void )
{
}

void CPopulationManager::LoadMvMMission( KeyValues *pMissionKV )
{
}

void CPopulationManager::MarkAllCurrentPlayersSafeToLeave( void )
{
}

void CPopulationManager::MvMVictory( void )
{
}

void CPopulationManager::OnCurrencyCollected( int nCurrency, bool b1, bool b2 )
{
}

void CPopulationManager::OnCurrencyPackFade( void )
{
}

void CPopulationManager::OnPlayerKilled( CTFPlayer *pPlayer )
{
}

bool CPopulationManager::Parse( void )
{
	return false;
}

void CPopulationManager::PauseSpawning( void )
{
}

bool CPopulationManager::PlayerDoneViewingLoot( CTFPlayer const *pPlayer )
{
	return false;
}

void CPopulationManager::PostInitialize( void )
{
	if ( TheNavMesh->GetNavAreaCount() <= 0 )
	{
		Warning( "Cannot populate - no Navigation Mesh exists.\n" );
		return;
	}

	FOR_EACH_VEC( m_vecPopulators, i )
	{
		//m_vecPopulators[i]->PostInitialize();
	}

	FOR_EACH_VEC( m_vecWaves, i )
	{
		//m_vecWaves[i]->PostInitialize();
	}
}

void CPopulationManager::RemoveBuybackCreditFromPlayer( CTFPlayer *pPlayer )
{
}

void CPopulationManager::RemovePlayerAndItemUpgradesFromHistory( CTFPlayer *pPlayer )
{
}

void CPopulationManager::ResetMap( void )
{
}

void CPopulationManager::ResetRespecPoints( void )
{
}

void CPopulationManager::RestoreCheckpoint( void )
{
}

void CPopulationManager::RestoreItemToCheckpoint( CTFPlayer *pPlayer, CEconItemView *pItem )
{
}

void CPopulationManager::RestorePlayerCurrency( void )
{
}

void CPopulationManager::SendUpgradesToPlayer( CTFPlayer *pPlayer )
{
}

void CPopulationManager::SetBuybackCreditsForPlayer( CTFPlayer *pPlayer, int nCredits )
{
}

void CPopulationManager::SetCheckpoint( int nWave )
{
}

void CPopulationManager::SetNumRespecsForPlayer( CTFPlayer *pplayer, int nRespecs )
{
}

void CPopulationManager::SetPopulationFilename( char const *pszFileName )
{
	m_bIsInitialized = false;

	V_strcpy_safe( m_szPopfileFull, pszFileName );
	V_FileBase( m_szPopfileFull, m_szPopfileShort, sizeof( m_szPopfileShort ) );

	//MannVsMachineStats_SetPopulationFile( m_szPopfileFull );
	ResetMap();

	if ( TFObjectiveResource() )
	{
		//TFObjectiveResource()->SetMannVsMachineChallengeIndex( GetItemSchema()->FindMvmMissionByName( m_szPopfileFull ) );
		TFObjectiveResource()->SetMvMPopfileName( MAKE_STRING( m_szPopfileFull ) );
	}
}

void CPopulationManager::SetupOnRoundStart( void )
{
}

void CPopulationManager::ShowNextWaveDescription( void )
{
	UpdateObjectiveResource();
}

void CPopulationManager::StartCurrentWave( void )
{
}

void CPopulationManager::UnpauseSpawning( void )
{
}

void CPopulationManager::Update( void )
{
}

void CPopulationManager::UpdateObjectiveResource( void )
{
}

void CPopulationManager::WaveEnd( bool b1 )
{
}

CON_COMMAND_F( tf_mvm_nextmission, "Load the next mission", FCVAR_CHEAT )
{
	if ( g_pPopulationManager )
	{
		g_pPopulationManager->CycleMission();
	}
}

CON_COMMAND_F( tf_mvm_force_victory, "Force immediate victory.", FCVAR_CHEAT )
{
	if ( g_pPopulationManager )
	{
		g_pPopulationManager->JumpToWave( g_pPopulationManager->m_vecWaves.Count() - 1 );
		g_pPopulationManager->WaveEnd( true );
		g_pPopulationManager->MvMVictory();
	}
}

CON_COMMAND_F( tf_mvm_checkpoint, "Save a checkpoint snapshot", FCVAR_CHEAT )
{
	if ( g_pPopulationManager )
	{
		g_pPopulationManager->SetCheckpoint( -1 );
	}
}

CON_COMMAND_F( tf_mvm_checkpoint_clear, "Clear the saved checkpoint", FCVAR_CHEAT )
{
	if ( g_pPopulationManager )
	{
		g_pPopulationManager->ClearCheckpoint();
	}
}

CON_COMMAND_F( tf_mvm_jump_to_wave, "Jumps directly to the given Mann Vs Machine wave number", FCVAR_CHEAT )
{
	if ( args.ArgC() <= 1 )
	{
		Msg( "Missing wave number\n" );
		return;
	}

	float f1 = -1.0f;
	if ( args.ArgC() >= 3 )
	{
		f1 = atof( args.Arg( 2 ) );
	}

	// find the population manager
	CPopulationManager *manager = (CPopulationManager *)gEntList.FindEntityByClassname( NULL, "info_populator" );
	if ( !manager )
	{
		Msg( "No Population Manager found in the map\n" );
		return;
	}

	uint32 desiredWave = (uint32)Max( atoi( args.Arg( 1 ) ) - 1, 0 );
	manager->JumpToWave( desiredWave, f1 );
}

CON_COMMAND_F( tf_mvm_debugstats, "Dumpout MvM Data", FCVAR_CHEAT )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( g_pPopulationManager )
	{
		g_pPopulationManager->DebugWaveStats();
	}
}
