//========= Copyright © Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "tf_population_manager.h"
#include "tf_populators.h"
#include "tf_populator_spawners.h"
#include "tf_objective_resource.h"
#include "tf_gamestats.h"
#include "tf_mann_vs_machine_stats.h"

extern ConVar tf_mvm_respec_limit;
extern ConVar tf_mvm_respec_credit_goal;

ConVar tf_mvm_default_sentry_buster_damage_dealt_threshold( "tf_mvm_default_sentry_buster_damage_dealt_threshold", "3000", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar tf_mvm_default_sentry_buster_kill_threshold( "tf_mvm_default_sentry_buster_kill_threshold", "15", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

ConVar tf_mvm_endless_force_on( "tf_mvm_endless_force_on", "0", FCVAR_DONTRECORD | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Force MvM Endless mode on" );
ConVar tf_mvm_endless_wait_time( "tf_mvm_endless_wait_time", "5.0f", FCVAR_DONTRECORD | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );
ConVar tf_mvm_endless_bomb_reset( "tf_mvm_endless_bomb_reset", "5", FCVAR_DONTRECORD | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Number of Waves to Complete before bomb reset" );
ConVar tf_mvm_endless_bot_cash( "tf_mvm_endless_bot_cash", "120", FCVAR_DONTRECORD | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "In Endless, number of credits bots get per wave" );
ConVar tf_mvm_endless_tank_boost( "tf_mvm_endless_tank_boost", "0.2", FCVAR_DONTRECORD | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "In Endless, amount of extra health for the tank per wave" );

ConVar tf_populator_debug( "tf_populator_debug", "0", FCVAR_CHEAT );
ConVar tf_populator_active_buffer_range( "tf_populator_active_buffer_range", "3000", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Populate the world this far ahead of lead raider, and this far behind last raider" );
ConVar tf_populator_health_multiplier( "tf_populator_health_multiplier", "1.0", FCVAR_DONTRECORD | FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar tf_populator_damage_multiplier( "tf_populator_damage_multiplier", "1.0", FCVAR_DONTRECORD | FCVAR_REPLICATED | FCVAR_CHEAT );

ConVar tf_mvm_disconnect_on_victory( "tf_mvm_disconnect_on_victory", "0", FCVAR_REPLICATED, "Enable to Disconnect Players after completing MvM" );
ConVar tf_mvm_victory_reset_time( "tf_mvm_victory_reset_time", "60.0", FCVAR_REPLICATED, "Seconds to wait after MvM victory before cycling to the next mission.  (Only used if tf_mvm_disconnect_on_victory is false.)" );
ConVar tf_mvm_victory_disconnect_time( "tf_mvm_victory_disconnect_time", "180.0", FCVAR_REPLICATED, "Seconds to wait after MvM victory before kicking players.  (Only used if tf_mvm_disconnect_on_victory is true.)" );

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


//-----------------------------------------------------------------------------
static bool HaveMap( char const *pszMap )
{
	char szMap[64];
	V_strcpy_safe( szMap, pszMap );

	IVEngineServer::eFindMapResult result = engine->FindMap( szMap, sizeof( szMap ) );
	if ( result == IVEngineServer::eFindMap_Found || result == IVEngineServer::eFindMap_PossiblyAvailable )
		return true;

	return false;
}

CPopulationManager *g_pPopulationManager = NULL;


BEGIN_DATADESC( CPopulationManager )
	DEFINE_THINKFUNC( Update ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( info_populator, CPopulationManager );

PRECACHE_REGISTER( info_populator );


CUtlVector<CPopulationManager::CheckpointSnapshotInfo *> CPopulationManager::sm_checkpointSnapshots;
int CPopulationManager::m_nCheckpointWaveIndex;
int CPopulationManager::m_nNumConsecutiveWipes;


static int s_iLastKnownMissionCategory = 1;
static int s_iLastKnownMission = 1;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPopulationManager::CPopulationManager()
{
	Assert( g_pPopulationManager == NULL );
	g_pPopulationManager = this;

	m_pMVMStats = MannVsMachineStats_GetInstance();

	m_iSentryBusterDamageDealtThreshold = tf_mvm_default_sentry_buster_damage_dealt_threshold.GetInt();
	m_iSentryBusterKillThreshold = tf_mvm_default_sentry_buster_kill_threshold.GetInt();

	m_nCurrentWaveIndex = 0;
	m_nStartingCurrency = 0;
	m_nNumConsecutiveWipes = 0;
	m_nRespawnWaveTime = 10;
	m_szPopfileFull[0] = '\0';
	m_szPopfileShort[0] = '\0';
	m_bIsInitialized = false;
	m_bIsAdvanced = false;
	m_bAllocatedBots = false;
	m_bFixedRespawnWaveTime = false;

	m_bCanBotsAttackWhileInSpawnRoom = true;

	SetThink( &CPopulationManager::Update );
	SetNextThink( gpGlobals->curtime );

	ListenForGameEvent( "pve_win_panel" );

	m_random.SetSeed( 0 );
	for ( int i = 0; i < 27; ++i )
	{
		int nRandom = m_random.RandomInt( 0, INT_MAX );
		m_RandomSeeds.AddToTail( nRandom );
	}

	EndlessParseBotUpgrades();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPopulationManager::~CPopulationManager()
{
	Assert( g_pPopulationManager == this );
	g_pPopulationManager = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::FireGameEvent( IGameEvent *event )
{
	const char *pszName = event->GetName();

	if ( FStrEq( pszName, "pve_win_panel" ) )
	{
		MarkAllCurrentPlayersSafeToLeave();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
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
		m_nNumConsecutiveWipes = 0;
		m_pMVMStats->SetCurrentWave( m_nCurrentWaveIndex );
	}
	else
	{
		RestoreCheckpoint();

		if ( m_bIsInitialized && ( m_nCurrentWaveIndex > 0 || m_nNumConsecutiveWipes > 1 ) )
			m_pMVMStats->RoundEvent_WaveEnd( false );

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

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::Precache( void )
{
	BaseClass::Precache();

	PrecacheScriptSound( "music.mvm_end_wave" );
	PrecacheScriptSound( "music.mvm_end_tank_wave" );
	PrecacheScriptSound( "music.mvm_end_mid_wave" );
	PrecacheScriptSound( "music.mvm_end_last_wave" );
	PrecacheScriptSound( "MVM.PlayerUpgraded" );
	PrecacheScriptSound( "MVM.PlayerBoughtIn" );
	PrecacheScriptSound( "MVM.PlayerUsedPowerup" );
	PrecacheScriptSound( "MVM.PlayerDied" );
	PrecacheScriptSound( "MVM.PlayerDiedScout" );
	PrecacheScriptSound( "MVM.PlayerDiedSniper" );
	PrecacheScriptSound( "MVM.PlayerDiedSoldier" );
	PrecacheScriptSound( "MVM.PlayerDiedDemoman" );
	PrecacheScriptSound( "MVM.PlayerDiedMedic" );
	PrecacheScriptSound( "MVM.PlayerDiedHeavy" );
	PrecacheScriptSound( "MVM.PlayerDiedPyro" );
	PrecacheScriptSound( "MVM.PlayerDiedSpy" );
	PrecacheScriptSound( "MVM.PlayerDiedEngineer" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::Reset( void )
{
	m_iSentryBusterDamageDealtThreshold = tf_mvm_default_sentry_buster_damage_dealt_threshold.GetInt();
	m_iSentryBusterKillThreshold = tf_mvm_default_sentry_buster_kill_threshold.GetInt();

	m_nStartingCurrency = 0;
	m_nRespawnWaveTime = 10;

	m_bSpawningPaused = false;
	m_bIsAdvanced = false;
	m_bCanBotsAttackWhileInSpawnRoom = true;

	m_szDefaultEventChangeAttributesName = "Default";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::Spawn( void )
{
	BaseClass::Spawn();

	Initialize();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::AddPlayerCurrencySpent( CTFPlayer *pPlayer, int nSpent )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::AddRespecToPlayer( CTFPlayer *pPlayer )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::AdjustMinPlayerSpawnTime( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::AllocateBots( void )
{
	if ( m_bAllocatedBots )
		return;

	CUtlVector<CTFPlayer *> bots;
	int nNumBots = CollectMvMBots( &bots );

	if ( nNumBots > 0 )
	{
		Warning( "%d bots were already allocated some how before "
				 "CPopulationManager::AllocateBots was called\n", nNumBots );
	}

	for ( int i = nNumBots; i < k_nMvMBotTeamSize; ++i )
	{
		CTFBot *pBot = NextBotCreatePlayerBot<CTFBot>( "TFBot", false );
		if ( pBot ) pBot->ChangeTeam( TEAM_SPECTATOR, false, true );
	}

	m_bAllocatedBots = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::ClearCheckpoint( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CPopulationManager::CollectMvMBots( CUtlVector<CTFPlayer *> *pBotsOut )
{
	pBotsOut->RemoveAll();
	for ( int i = 0; i < gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer == NULL || FNullEnt( pPlayer->edict() ) || !pPlayer->IsPlayer() )
			continue;

		if ( !pPlayer->IsConnected() || !pPlayer->IsBot() )
			continue;

		if ( pPlayer->GetTeamNumber() != TF_TEAM_MVM_BOTS )
			continue;

		pBotsOut->AddToTail( (CTFPlayer *)pPlayer );
	}

	return pBotsOut->Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::CycleMission( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::DebugWaveStats( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::EndlessFlagHasReset( void )
{
	m_bShouldResetFlag = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::EndlessParseBotUpgrades( void )
{
	m_BotUpgrades.RemoveAll();

	KeyValuesAD pKV( "Upgrades" );
	if ( !pKV->LoadFromFile( filesystem, "scripts/items/mvm_botupgrades.txt", "MOD" ) )
	{
		Warning( "Can't open scripts/items/mvm_botupgrades.txt\n" );
		return;
	}

	FOR_EACH_SUBKEY( pKV, pSubKey )
	{
		const char *pszAttrib = pSubKey->GetString( "attribute" );
		bool bIsBotAttr = pSubKey->GetBool( "IsBotAttr" );
		bool bIsSkillAttr = pSubKey->GetBool( "IsSkillAttr" );
		float flValue = pSubKey->GetFloat( "value" );
		float flMax = pSubKey->GetFloat( "max" );
		int nCost = pSubKey->GetInt( "cost", 100 );
		int nWeight = pSubKey->GetInt( "weight", 1 );

		attrib_def_index_t iAttrIndex = 0;
		if ( !bIsBotAttr && !bIsSkillAttr )
		{
			CEconAttributeDefinition const *pAttr = GetItemSchema()->GetAttributeDefinitionByName( pszAttrib );
			if ( pAttr == NULL )
			{
				Warning( "Unable to Find Attribute %s when parsing EndlessParseBotUpgrades.\n", pszAttrib );
				return;
			}

			iAttrIndex = pAttr->index;
		}

		int nIndex = m_BotUpgrades.AddToTail();

		// ???
		for ( int i =0; i < nWeight; ++i )
		{
			CMvMBotUpgrade *pUpgrade = &m_BotUpgrades[ nIndex ];
			V_strncpy( pUpgrade->m_szAttrib, pszAttrib, sizeof( pUpgrade->m_szAttrib ) );
			pUpgrade->m_flValue = flValue;
			pUpgrade->m_flMax = flMax;
			pUpgrade->m_nCost = nCost;
			pUpgrade->m_bIsBotAttr = bIsBotAttr;
			pUpgrade->m_bIsSkillAttr = bIsSkillAttr;
			pUpgrade->m_iAttribIndex = iAttrIndex;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::EndlessRollEscalation( void )
{
	m_ActiveBotUpgrades.Purge();

	int nCurrency = tf_mvm_endless_bot_cash.GetFloat() * m_nCheckpointWaveIndex;

	CUtlVector<CMvMBotUpgrade> availableUpgrades;
	FOR_EACH_VEC( m_BotUpgrades, i )
	{
		if ( m_BotUpgrades[i].m_nCost <= nCurrency )
			availableUpgrades.AddToTail( m_BotUpgrades[i] );
	}

	CUniformRandomStream random;
	random.SetSeed( m_RandomSeeds[ m_nCurrentWaveIndex % m_RandomSeeds.Count() ] );
	while ( nCurrency >= 100 && !availableUpgrades.IsEmpty() )
	{
		int nRandIndex = random.RandomInt( 0, availableUpgrades.Count() - 1 );
		CMvMBotUpgrade upgrade = availableUpgrades[ nRandIndex ];

		if ( m_ActiveBotUpgrades.IsEmpty() )
		{
			m_ActiveBotUpgrades.AddToTail( upgrade );

			nCurrency -= upgrade.m_nCost;

			if ( ( upgrade.m_flMax > 0 && upgrade.m_flMax <= upgrade.m_flValue ) ||
				 ( upgrade.m_flMax < 0 && upgrade.m_flMax >= upgrade.m_flValue ) )
			{
				availableUpgrades.FastRemove( nRandIndex );
			}
		}
		else
		{
			int nFoundIndex = -1;
			FOR_EACH_VEC( m_ActiveBotUpgrades, i )
			{
				if ( !V_strcmp( upgrade.m_szAttrib, m_ActiveBotUpgrades[i].m_szAttrib ) )
				{
					m_ActiveBotUpgrades[i].m_flValue += upgrade.m_flValue;
					float flValue = m_ActiveBotUpgrades[i].m_flValue;
					
					nCurrency -= upgrade.m_nCost;

					if ( ( upgrade.m_flMax > 0 && upgrade.m_flMax <= flValue ) ||
						 ( upgrade.m_flMax < 0 && upgrade.m_flMax >= flValue ) )
					{
						availableUpgrades.FastRemove( nRandIndex );
					}

					nFoundIndex = i;
					break;
				}
			}

			if ( !m_ActiveBotUpgrades.IsValidIndex( nFoundIndex ) )
			{
				m_ActiveBotUpgrades.AddToTail( upgrade );

				nCurrency -= upgrade.m_nCost;

				if ( ( upgrade.m_flMax > 0 && upgrade.m_flMax <= upgrade.m_flValue ) ||
					 ( upgrade.m_flMax < 0 && upgrade.m_flMax >= upgrade.m_flValue ) )
				{
					availableUpgrades.FastRemove( nRandIndex );
				}
			}
		}

		if ( nCurrency > 0 )
		{
			FOR_EACH_VEC_BACK( availableUpgrades, i )
			{
				if ( availableUpgrades[i].m_nCost > nCurrency )
					availableUpgrades.FastRemove( i );
			}
		}
	}

	char szMsg[255];
	V_strcpy_safe( szMsg, "*** Bot Upgrades\n" );
	FOR_EACH_VEC( m_ActiveBotUpgrades, i )
	{
		char szAttrib[sizeof( m_ActiveBotUpgrades[i].m_szAttrib ) + 20];
		V_sprintf_safe( szAttrib, "-%s %.1f", m_ActiveBotUpgrades[i].m_szAttrib, m_ActiveBotUpgrades[i].m_flValue );
		V_strcat_safe( szMsg, szAttrib );
	}

	UTIL_ClientPrintAll( HUD_PRINTCENTER, szMsg );
	UTIL_ClientPrintAll( HUD_PRINTCONSOLE, szMsg );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::EndlessSetAttributesForBot( CTFBot *pBot )
{
	FOR_EACH_VEC( m_ActiveBotUpgrades, i )
	{
		CMvMBotUpgrade upgrade = m_ActiveBotUpgrades[i];
		if ( upgrade.m_bIsBotAttr )
		{
			pBot->SetAttribute( (CTFBot::AttributeType)(int)upgrade.m_flValue );
		}
		else if ( upgrade.m_bIsSkillAttr )
		{
			pBot->SetDifficulty( (CTFBot::DifficultyType)(int)upgrade.m_flValue );
		}
		else
		{
			CEconAttributeDefinition const *pAttr = GetItemSchema()->GetAttributeDefinition( upgrade.m_iAttribIndex );
			if ( pAttr )
			{
				float flValue = upgrade.m_flValue;
				if ( pAttr->description_format == ATTRIB_FORMAT_PERCENTAGE ||
						 pAttr->description_format == ATTRIB_FORMAT_INVERTED_PERCENTAGE )
				{
						flValue += 1.0f;
				}

				pBot->GetAttributeList()->SetRuntimeAttributeValue( pAttr, flValue );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPopulationManager::EndlessShouldResetFlag( void )
{
	return m_bShouldResetFlag;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPopulationManager::CheckpointSnapshotInfo *CPopulationManager::FindCheckpointSnapshot( CSteamID steamID ) const
{
	FOR_EACH_VEC( sm_checkpointSnapshots, i )
	{

	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPopulationManager::CheckpointSnapshotInfo *CPopulationManager::FindCheckpointSnapshot( CTFPlayer *pPlayer ) const
{
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::FindDefaultPopulationFileShortNames( CUtlVector<CUtlString> &outNames )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPopulationManager::PlayerUpgradeHistory *CPopulationManager::FindOrAddPlayerUpgradeHistory( CSteamID steamID ) const
{
	FOR_EACH_VEC( m_PlayerUpgrades, i )
	{
		if ( m_PlayerUpgrades[i]->m_steamID == steamID )
			return m_PlayerUpgrades[i];
	}

	PlayerUpgradeHistory *pHistory = new PlayerUpgradeHistory;
	pHistory->m_steamID = steamID;
	pHistory->m_nCurrencySpent = 0;
	m_PlayerUpgrades.AddToTail( pHistory );

	return pHistory;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPopulationManager::PlayerUpgradeHistory *CPopulationManager::FindOrAddPlayerUpgradeHistory( CTFPlayer *pPlayer ) const
{
	CSteamID steamID;
	if ( !pPlayer->GetSteamID( &steamID ) )
	{
		Log( "MvM : Unable to Find SteamID for player %s, unable to locate their upgrade history!", pPlayer->GetPlayerName() );
		return nullptr;
	}

	return FindOrAddPlayerUpgradeHistory( steamID );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPopulationManager::FindPopulationFileByShortName( char const *pszName, CUtlString *outFullName )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::ForgetOtherBottleUpgrades( CTFPlayer *pPlayer, CEconItemView *pItem, int nUpgradeKept )
{
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void CPopulationManager::GameRulesThink( void )
{
	// Some MM logic goes here
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWave *CPopulationManager::GetCurrentWave( void )
{
	if ( !m_bIsInitialized || m_Waves.Count() == 0 )
		return nullptr;

	if ( IsInEndlessWaves() )
	{
		return m_Waves[ m_nCurrentWaveIndex % m_Waves.Count() ];
	}
	else if ( m_nCurrentWaveIndex < m_Waves.Count() )
	{
		return m_Waves[ m_nCurrentWaveIndex ];
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CPopulationManager::GetDamageMultiplier( void ) const
{
	return tf_populator_damage_multiplier.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CPopulationManager::GetHealthMultiplier( bool bTankMultiplier ) const
{
	if ( bTankMultiplier && IsInEndlessWaves() )
		return tf_populator_health_multiplier.GetFloat() + m_nCurrentWaveIndex * tf_mvm_endless_tank_boost.GetFloat();

	return tf_populator_health_multiplier.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CPopulationManager::GetNumBuybackCreditsForPlayer( CTFPlayer *pPlayer )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CPopulationManager::GetNumRespecsAvailableForPlayer( CTFPlayer *pPlayer )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CPopulationManager::GetPlayerCurrencySpent( CTFPlayer *pPlayer )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CUtlVector<CUpgradeInfo> *CPopulationManager::GetPlayerUpgradeHistory( CTFPlayer *pPlayer )
{
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
char const *CPopulationManager::GetPopulationFilename( void ) const
{
	return m_szPopfileFull;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
char const *CPopulationManager::GetPopulationFilenameShort( void ) const
{
	return m_szPopfileShort;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::GetSentryBusterDamageAndKillThreshold( int &nNumDamage, int &nNumKills )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CPopulationManager::GetTotalPopFileCurrency( void )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPopulationManager::HasEventChangeAttributes( char const *pszEventName )
{
	FOR_EACH_VEC( m_Waves, i )
	{
		if ( m_Waves[i]->HasEventChangeAttributes( pszEventName ) )
			return true;
	}

	FOR_EACH_VEC( m_Populators, i )
	{
		if ( m_Populators[i]->HasEventChangeAttributes( pszEventName ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPopulationManager::IsInEndlessWaves( void ) const
{
	return ( m_bIsEndless || tf_mvm_endless_force_on.GetBool() ) && !m_Waves.IsEmpty();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPopulationManager::IsPlayerBeingTrackedForBuybacks( CTFPlayer *pPlayer )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPopulationManager::IsValidMvMMap( char const *pszMapName )
{
	if ( m_pMvMMapCycle == NULL )
		LoadMissionCycleFile();

	if ( !pszMapName || !pszMapName[0] )
		return false;

	int nCatagories = m_pMvMMapCycle->GetInt( "catagories" );
	for ( int i = 1; i <= nCatagories; ++i )
	{
		KeyValues *pCatagory = m_pMvMMapCycle->FindKey( UTIL_VarArgs( "%d", i ) );
		if ( pCatagory )
		{
			int nCount = pCatagory->GetInt( "count" );
			for ( int j = 1; j <= nCount; ++j )
			{
				KeyValues *pMission = pCatagory->FindKey( UTIL_VarArgs( "%d", j ) );
				if ( pMission )
				{
					if ( !V_strcmp( pszMapName, pMission->GetString( "map" ) ) )
						return HaveMap( pszMapName );
				}
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::JumpToWave( int nWave, float f1 )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::LoadLastKnownMission( void )
{
	if ( !m_pMvMMapCycle && !LoadMissionCycleFile() )
	{
		ResetMap();
		return;
	}

	KeyValues *pCatagory = m_pMvMMapCycle->FindKey( UTIL_VarArgs( "%d", s_iLastKnownMissionCategory ) );
	if ( pCatagory )
	{
		KeyValues *pMission = pCatagory->FindKey( UTIL_VarArgs( "%d", s_iLastKnownMission ) );
		if ( pMission )
		{
			if ( LoadMvMMission( pMission ) )
				return;
		}
	}

	pCatagory = m_pMvMMapCycle->FindKey( UTIL_VarArgs( "%d", 1 ) );
	if ( pCatagory )
	{
		KeyValues *pMission = pCatagory->FindKey( UTIL_VarArgs( "%d" ), 1 );
		if ( pMission )
		{
			if ( !LoadMvMMission( pMission ) )
			{
				ResetMap();
				return;
			}
		}
	}

	ResetMap();
	
	s_iLastKnownMission = 1;
	s_iLastKnownMissionCategory = 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPopulationManager::LoadMissionCycleFile( void )
{
	if ( m_pMvMMapCycle )
		m_pMvMMapCycle->deleteThis();

	char const *pszMissionCycle = tf_mvm_missioncyclefile.GetString();
	m_pMvMMapCycle = new KeyValues( pszMissionCycle );
	return m_pMvMMapCycle->LoadFromFile( g_pFullFileSystem, pszMissionCycle, "MOD" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPopulationManager::LoadMvMMission( KeyValues *pMissionKV )
{
	if ( pMissionKV == NULL )
		return false;

	char const *pszMap = pMissionKV->GetString( "map" );
	char const *pszPopFile = pMissionKV->GetString( "popfile" );
	if ( !pszMap || !pszPopFile )
		return false;

	char szPopFile[MAX_PATH];
	V_sprintf_safe( szPopFile, "scripts/population/%s.pop", pszPopFile );
	if ( !g_pFullFileSystem->FileExists( szPopFile, "MOD" ) )
		return false;

	if ( !HaveMap( pszMap ) )
		return false;

	engine->ChangeLevel( pszMap, NULL );
	TFGameRules()->SetNextMvMPopfile( pszPopFile );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::MarkAllCurrentPlayersSafeToLeave( void )
{
	// MM stuff
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::MvMVictory( void )
{
	for ( int i = 0; i <= gpGlobals->maxClients; ++i )
	{
		CTFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsAlive() )
			continue;

		pPlayer->m_Shared.AddCond( TF_COND_CRITBOOSTED_BONUS_TIME, 10.0f );
	}

	TFGameRules()->BroadcastSound( 255, "Game.YourTeamWon" );

	m_pMVMStats->RoundOver( true );

	bool bDisconnect = tf_mvm_disconnect_on_victory.GetBool();
	float flActionTime = bDisconnect ? tf_mvm_victory_disconnect_time.GetFloat() : tf_mvm_victory_reset_time.GetFloat();

	CReliableBroadcastRecipientFilter filter;
	UserMessageBegin( filter, "MVMVictory" );
		WRITE_BYTE( bDisconnect );
		WRITE_BYTE( RoundFloatToByte( flActionTime ) );
	MessageEnd();

	// MM stuff
	// GTFGCClientSystem()->SendMvMVictoryResult();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::OnCurrencyCollected( int nCurrency, bool bWasDropped, bool bBonus )
{
	bool bBetweenWaves = TFObjectiveResource()->GetMannVsMachineIsBetweenWaves();
	int nCurrentWave = bBetweenWaves ? m_nCurrentWaveIndex - 1 : m_nCurrentWaveIndex;

	if ( bWasDropped )
		m_pMVMStats->RoundEvent_CreditsDropped( nCurrentWave, nCurrency );

	m_pMVMStats->RoundEvent_AcquiredCredits( nCurrentWave, nCurrency, bBonus );

	int const nRespecLimit = tf_mvm_respec_limit.GetInt();
	if ( nRespecLimit != 0 )
	{
		if ( m_nRespecsAwarded < nRespecLimit )
		{
			m_nCurrencyForRespec += nCurrency;

			int nRespecCreditGoal = tf_mvm_respec_credit_goal.GetFloat();
			while ( m_nCurrencyForRespec > nRespecCreditGoal )
			{
				m_nRespecsAwardedInWave++;
				m_nRespecsAwarded++;

				CUtlVector<CTFPlayer *> humans;
				CollectPlayers( &humans, TF_TEAM_MVM_PLAYERS );
				FOR_EACH_VEC( humans, i )
				{
					AddRespecToPlayer( humans[i] );
				}

				if ( m_nRespecsAwarded < nRespecLimit )
					m_nCurrencyForRespec -= nRespecCreditGoal;
				else
					m_nCurrencyForRespec = nRespecCreditGoal;
			}

			if ( m_pMVMStats )
			{
				m_pMVMStats->SetNumRespecsEarnedInWave( m_nRespecsAwardedInWave );
				m_pMVMStats->SetAcquiredCreditsForRespec( m_nCurrencyForRespec );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Unused
//-----------------------------------------------------------------------------
void CPopulationManager::OnCurrencyPackFade( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::OnPlayerKilled( CTFPlayer *pPlayer )
{
	FOR_EACH_VEC( m_Populators, i )
	{
		m_Populators[i]->OnPlayerKilled( pPlayer );
	}

	CWave *pWave = GetCurrentWave();
	if ( pWave ) pWave->OnPlayerKilled( pPlayer );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPopulationManager::Parse( void )
{
	if ( m_szPopfileFull[0] == '\0' )
	{
		Warning( "No population file specified.\n" );
		return false;
	}

	KeyValuesAD pKV( "Population" );
	if ( !pKV->LoadFromFile( filesystem, m_szPopfileFull, "GAME" ) )
	{
		Warning( "Can't open %s.\n", m_szPopfileFull );
		pKV->deleteThis();
		return false;
	}

	m_Populators.PurgeAndDeleteElements();
	m_Waves.RemoveAll();

	if ( m_pTemplates )
	{
		m_pTemplates->deleteThis();
		m_pTemplates = NULL;
	}

	KeyValues *pKVTemplates = pKV->FindKey( "Templates" );
	if ( pKVTemplates )
	{
		m_pTemplates = pKVTemplates->MakeCopy();
	}

	FOR_EACH_SUBKEY( pKV, pSubKey )
	{
		const char *pszKey = pSubKey->GetName();
		if ( !V_stricmp( pszKey, "StartingCurrency" ) )
		{
			m_nStartingCurrency = pSubKey->GetInt();
		}
		else if ( !V_stricmp( pszKey, "RespawnBaveTime" ) )
		{
			m_nRespawnWaveTime = pSubKey->GetInt();
		}
		else if ( !V_stricmp( pszKey, "EventPopFile" ) )
		{
			if ( !V_stricmp( pSubKey->GetString(), "Halloween" ) )
			{
				m_nMvMEventPopfileType = MVM_EVENT_POPFILE_HALLOWEEN;
			}
			else
			{
				m_nMvMEventPopfileType = MVM_EVENT_POPFILE_NONE;
			}
		}
		else if ( !V_stricmp( pszKey, "FixedRespawnWaveTime" ) )
		{
			m_bFixedRespawnWaveTime = true;
		}
		else if ( !V_stricmp( pszKey, "AddSentryBusterWhenDamageDealtExceeds" ) )
		{
			m_iSentryBusterDamageDealtThreshold = pSubKey->GetInt();
		}
		else if ( !V_stricmp( pszKey, "AddSentryBusterWhenKillCountExceeds" ) )
		{
			m_iSentryBusterKillThreshold = pSubKey->GetInt();
		}
		else if ( !V_stricmp( pszKey, "CanBotsAttackInSpawnRoom" ) )
		{
			// Why is "no" and "false" taken instead of pSubKey->GetBool ??
			if ( !V_stricmp( pSubKey->GetString(), "no" ) || !V_stricmp( pSubKey->GetString(), "false" ) )
			{
				m_bCanBotsAttackWhileInSpawnRoom = false;
			}
			else
			{
				m_bCanBotsAttackWhileInSpawnRoom = true;
			}
		}
		else if ( !V_stricmp( pszKey, "RandomPlacement" ) )
		{
			CRandomPlacementPopulator *pPopulator = new CRandomPlacementPopulator( this );
			if ( !pPopulator->Parse( pSubKey ) )
			{
				Warning( "Error reading RandomPlacement definition\n" );
				return false;
			}

			m_Populators.AddToTail( pPopulator );
		}
		else if ( !V_stricmp( pszKey, "PeriodicSpawn" ) )
		{
			CPeriodicSpawnPopulator *pPopulator = new CPeriodicSpawnPopulator( this );
			if ( !pPopulator->Parse( pSubKey ) )
			{
				Warning( "Error reading PeriodicSpawn definition\n" );
				return false;
			}

			m_Populators.AddToTail( pPopulator );
		}
		else if ( !V_stricmp( pszKey, "Wave" ) )
		{
			CWave *pWave = new CWave( this );
			if ( !pWave->Parse( pSubKey ) )
			{
				Warning( "Error reading Wave definition\n" );
				return false;
			}

			m_Waves.AddToTail( pWave );
		}
		else if ( !V_stricmp( pszKey, "Mission" ) )
		{
			CMissionPopulator *pPopulator = new CMissionPopulator( this );
			if ( !pPopulator->Parse( pSubKey ) )
			{
				Warning( "Error reading RandomPlacement definition\n" );
				return false;
			}

			m_Populators.AddToTail( pPopulator );
		}
		else if ( !V_stricmp( pszKey, "Templates" ) )
		{

		}
		else if ( !V_stricmp( pszKey, "Advanced" ) )
		{
			m_bIsAdvanced = true;
		}
		else if ( !V_stricmp( pszKey, "IsEndless" ) )
		{
			m_bIsEndless = true;
		}
		else
		{
			Warning( "Invalid populator '%s'\n", pszKey );
			return false;
		}
	}

	FOR_EACH_VEC( m_Populators, i )
	{
		CMissionPopulator *pMission = dynamic_cast<CMissionPopulator *>( m_Populators[i] );
		if ( pMission )
		{
			if ( pMission->GetSpawner() && !pMission->GetSpawner()->IsVarious() )
			{
				for ( int i = pMission->m_nStartWave; i < pMission->m_nEndWave; ++i )
				{
					if ( m_Waves.IsValidIndex( i ) )
					{
						CWave *pWave = m_Waves[i];

						unsigned int iFlags = MVM_CLASS_FLAG_MISSION;
						if ( pMission->GetSpawner()->IsMiniBoss() )
						{
							iFlags |= MVM_CLASS_FLAG_MINIBOSS;
						}
						if ( pMission->GetSpawner()->HasAttribute( CTFBot::AttributeType::ALWAYSCRIT ) )
						{
							iFlags |= MVM_CLASS_FLAG_ALWAYSCRIT;
						}
						pWave->AddClassType( pMission->GetSpawner()->GetClassIcon(), 0, iFlags );
					}
				}
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::PauseSpawning( void )
{
	DevMsg( "Wave paused\n" );
	m_bSpawningPaused = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::PlayerDoneViewingLoot( CTFPlayer const *pPlayer )
{
	if ( cutlvector6BC.Find( pPlayer ) != cutlvector6BC.InvalidIndex() )
		return;

	CUtlVector<CTFPlayer const *> humans;
	CollectPlayers( &humans, TF_TEAM_MVM_PLAYERS );
	if ( humans.Find( pPlayer ) == humans.InvalidIndex() )
		return;

	cutlvector6BC.AddToTail( pPlayer );

	float flTimeRemaining = m_flTimeToRestart - gpGlobals->curtime;
	if ( flTimeRemaining > 15.0f )
	{
		float flTimeDeduction = ( /*tf_mm_trusted.GetBool() ||*/ tf_mvm_disconnect_on_victory.GetBool() ) ?
								tf_mvm_victory_disconnect_time.GetFloat() 
								:
								tf_mvm_victory_reset_time.GetFloat();

		flTimeDeduction = flTimeDeduction * 0.8 / humans.Count();
		flTimeRemaining = fmaxf( 15.0, flTimeRemaining - flTimeDeduction );

		m_flTimeToRestart = gpGlobals->curtime + flTimeRemaining;

		CReliableBroadcastRecipientFilter filter;
		UserMessageBegin( filter, "MVMServerKickTimeUpdate" );
			WRITE_BYTE( RoundFloatToByte( flTimeRemaining ) );
		MessageEnd();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::PostInitialize( void )
{
	if ( TheNavMesh->GetNavAreaCount() <= 0 )
	{
		Warning( "Cannot populate - no Navigation Mesh exists.\n" );
		return;
	}

	FOR_EACH_VEC( m_Populators, i )
	{
		m_Populators[i]->PostInitialize();
	}

	FOR_EACH_VEC( m_Waves, i )
	{
		m_Waves[i]->PostInitialize();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::RemoveBuybackCreditFromPlayer( CTFPlayer *pPlayer )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::RemovePlayerAndItemUpgradesFromHistory( CTFPlayer *pPlayer )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::ResetMap( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::ResetRespecPoints( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::RestoreCheckpoint( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::RestoreItemToCheckpoint( CTFPlayer *pPlayer, CEconItemView *pItem )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::RestorePlayerCurrency( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::SendUpgradesToPlayer( CTFPlayer *pPlayer )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::SetBuybackCreditsForPlayer( CTFPlayer *pPlayer, int nCredits )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::SetCheckpoint( int nWave )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::SetNumRespecsForPlayer( CTFPlayer *pplayer, int nRespecs )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::SetPopulationFilename( char const *pszFileName )
{
	m_bIsInitialized = false;

	V_strcpy_safe( m_szPopfileFull, pszFileName );
	V_FileBase( m_szPopfileFull, m_szPopfileShort, sizeof( m_szPopfileShort ) );

	MannVsMachineStats_SetPopulationFile( m_szPopfileFull );
	ResetMap();

	if ( TFObjectiveResource() )
	{
		//TFObjectiveResource()->SetMannVsMachineChallengeIndex( GetItemSchema()->FindMvmMissionByName( m_szPopfileFull ) );
		TFObjectiveResource()->SetMvMPopfileName( MAKE_STRING( m_szPopfileFull ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::SetupOnRoundStart( void )
{
	Initialize();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::ShowNextWaveDescription( void )
{
	UpdateObjectiveResource();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::StartCurrentWave( void )
{
	if ( TFObjectiveResource() )
	{
		TFObjectiveResource()->SetMannVsMachineNextWaveTime( 0 );
		TFObjectiveResource()->SetMannVsMachineBetweenWaves( false );
	}

	UpdateObjectiveResource();
	m_pMVMStats->RoundEvent_WaveStart();

	TFGameRules()->State_Transition( GR_STATE_RND_RUNNING );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::UnpauseSpawning( void )
{
	DevMsg( "Wave unpaused\n" );
	m_bSpawningPaused = false;

	FOR_EACH_VEC( m_Populators, i )
	{
		m_Populators[i]->UnpauseSpawning();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::Update( void )
{
	VPROF_BUDGET( __FUNCTION__, "NextBot" );
	SetNextThink( gpGlobals->curtime );

	FOR_EACH_VEC( m_Populators, i )
	{
		m_Populators[i]->Update();
	}

	CWave *pWave = GetCurrentWave();
	if ( pWave ) pWave->Update();

	if ( TFGameRules()->State_Get() == GR_STATE_GAME_OVER )
	{
		if ( m_flTimeToRestart < gpGlobals->curtime )
		{
			if ( tf_mvm_disconnect_on_victory.GetBool() )
			{
				// TODO: Alternative to GC
				/*if ( !TFGameRules()->IsManagedMatchEnded() )
				{
					TFGameRules()->EndManagedMvMMatch( true );
				}*/
			}
			else
			{
				CycleMission();
			}
		}

		if ( tf_mvm_disconnect_on_victory.GetBool() && ( m_flTimeToRestart + 5.0f ) < gpGlobals->curtime )
		{
			Log( "Kicking all players\n" );
			engine->ServerCommand( "kickall #TF_PVE_Disconnect" );
			CycleMission();
		}

		// Logic for achievements
		if ( byte5D8 && MannVsMachineStats_GetDroppedCredits() && !MannVsMachineStats_GetMissedCredits() )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( m_bIsAdvanced ? "mvm_creditbonus_all_advanced" : "mvm_creditbonus_all" );
			if ( event )
			{
				gameeventmanager->FireEvent( event );
			}

			byte5D8 = false;
		}
	}
	else if ( TFGameRules()->State_Get() == GR_STATE_STARTGAME )
	{
		AllocateBots();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::UpdateObjectiveResource( void )
{
	if ( m_Waves.IsEmpty() || !TFObjectiveResource() )
		return;

	TFObjectiveResource()->SetMannVsMachineEventPopfileType( m_nMvMEventPopfileType );

	if ( IsInEndlessWaves() )
		TFObjectiveResource()->SetMannVsMachineMaxWaveCount( 0 );
	else
		TFObjectiveResource()->SetMannVsMachineMaxWaveCount( m_Waves.Count() );

	TFObjectiveResource()->SetMannVsMachineWaveCount( m_nCurrentWaveIndex + 1 );

	CWave *pWave = GetCurrentWave();
	if ( pWave )
	{
		TFObjectiveResource()->SetMannVsMachineWaveEnemyCount( pWave->m_nTotalEnemyCount );

		TFObjectiveResource()->ClearMannVsMachineWaveClassFlags();

		int nTypes = 0; bool bHasEngineer = false;
		while ( nTypes < MVM_CLASS_TYPES_PER_WAVE_MAX && nTypes < pWave->GetNumClassTypes() )
		{
			bHasEngineer |= FStrEq( pWave->GetClassIconName( nTypes ).ToCStr(), "engineer" );

			TFObjectiveResource()->SetMannVsMachineWaveClassName( nTypes, pWave->GetClassIconName( nTypes ) );
			TFObjectiveResource()->SetMannVsMachineWaveClassCount( nTypes, pWave->GetClassCount( nTypes ) );
			TFObjectiveResource()->AddMannVsMachineWaveClassFlags( nTypes, pWave->GetClassFlags( nTypes ) );

			nTypes++;
		}

		if ( bHasEngineer )
		{
			if ( nTypes < MVM_CLASS_TYPES_PER_WAVE_MAX )
			{
				TFObjectiveResource()->SetMannVsMachineWaveClassName( nTypes, AllocPooledString( "teleporter" ) );
				TFObjectiveResource()->SetMannVsMachineWaveClassCount( nTypes, 0 );
				TFObjectiveResource()->AddMannVsMachineWaveClassFlags( nTypes, MVM_CLASS_FLAG_MISSION );

				nTypes++;
			}
		}

		while ( nTypes++ <= MVM_CLASS_TYPES_PER_WAVE_MAX )
		{
			TFObjectiveResource()->SetMannVsMachineWaveClassCount( nTypes, 0 );
			TFObjectiveResource()->SetMannVsMachineWaveClassName( nTypes, NULL_STRING );
			TFObjectiveResource()->AddMannVsMachineWaveClassFlags( nTypes, MVM_CLASS_FLAG_NONE );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPopulationManager::WaveEnd( bool bComplete )
{
	m_pMVMStats->RoundEvent_WaveEnd( bComplete );

	IGameEvent *event = gameeventmanager->CreateEvent( "scorestats_accumulated_update" );
	if ( event )
	{
		gameeventmanager->FireEvent( event );
	}

	CTF_GameStats.ResetRoundStats();
	MarkAllCurrentPlayersSafeToLeave();

	if ( bComplete )
	{
		if ( byte58A )
		{
			if ( ehandle58C )
			{
				UTIL_Remove( ehandle58C );
				ehandle58C = NULL;
			}

			byte58A = false;
		}
		else
		{
			m_nCurrentWaveIndex++;
		}

		m_pMVMStats->SetCurrentWave( m_nCurrentWaveIndex );

		CWave *pWave = GetCurrentWave();
		if ( pWave )
		{
			SetCheckpoint( -1 );
			UpdateObjectiveResource();

			pWave->m_upgradeAlertTimer.Start( 3.0f );

			if ( IsInEndlessWaves() )
			{
				EndlessRollEscalation();

				pWave->ForceReset();

				float flTime = gpGlobals->curtime + tf_mvm_endless_wait_time.GetFloat();
				if ( m_nCurrentWaveIndex % tf_mvm_endless_bomb_reset.GetInt() == 0 )
				{
					flTime += tf_mvm_endless_wait_time.GetFloat();
					m_bShouldResetFlag = true;
				}

				pWave->m_flStartTime = flTime;
			}
		}

		if ( m_nCurrentWaveIndex >= m_Waves.Count() && !IsInEndlessWaves() )
		{
			if ( /*tf_mm_trusted.GetBool() ||*/ tf_mvm_disconnect_on_victory.GetBool() )
				m_flTimeToRestart = gpGlobals->curtime + tf_mvm_victory_disconnect_time.GetFloat();
			else
				m_flTimeToRestart = gpGlobals->curtime + tf_mvm_victory_reset_time.GetFloat();

			TFObjectiveResource()->SetMannVsMachineBetweenWaves( true );
			TFGameRules()->State_Transition( GR_STATE_GAME_OVER );

			return;
		}
	}

	if ( !IsInEndlessWaves() )
	{
		TFGameRules()->State_Transition( GR_STATE_BETWEEN_RNDS );
		TFObjectiveResource()->SetMannVsMachineBetweenWaves( true );
	}
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
		g_pPopulationManager->JumpToWave( g_pPopulationManager->m_Waves.Count() - 1 );
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
