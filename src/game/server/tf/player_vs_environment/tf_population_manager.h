//========= Copyright © Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================
#ifndef TF_POPULATION_MANAGER_H
#define TF_POPULATION_MANAGER_H

#include "tf_bot.h"

class CWave;
class CMannVsMachineStats;
class IPopulator;

class CPopulationManager : public CPointEntity, public CGameEventListener
{
	DECLARE_CLASS( CPopulationManager, CPointEntity );

	struct CheckpointSnapshotInfo
	{
		CSteamID m_steamId;
		int m_currencySpent;
		CUtlVector< CUpgradeInfo > m_upgradeVector;
	};
	static CUtlVector<CheckpointSnapshotInfo *> sm_checkpointSnapshots;

	struct PlayerUpgradeHistory
	{
		CSteamID m_steamId;							// which player this snapshot is for
		CUtlVector< CUpgradeInfo > m_upgradeVector;
		int m_currencySpent;
	};
	mutable CUtlVector<PlayerUpgradeHistory *> m_PlayerUpgrades;

	static int m_nCheckpointWaveIndex;
	static int m_nNumConsecutiveWipes;
public:
	DECLARE_DATADESC();

	CPopulationManager( void );
	virtual ~CPopulationManager();

	virtual void FireGameEvent( IGameEvent *gameEvent );
	virtual bool Initialize( void );
	virtual void Precache( void );
	virtual void Reset( void );
	virtual void Spawn( void );

	void AddPlayerCurrencySpent( CTFPlayer *pPlayer, int nSpent );
	void AddRespecToPlayer( CTFPlayer *pPlayer );
	void AdjustMinPlayerSpawnTime( void );
	void AllocateBots( void );
	bool CanBotsAttackWhileInSpawnRoom( void ) const { return m_bCanBotsAttackWhileInSpawnRoom; }
	void ClearCheckpoint( void );
	static int CollectMvMBots( CUtlVector<CTFPlayer *> *vecBotsOut );
	void CycleMission( void );
	void DebugWaveStats( void );
	void EndlessFlagHasReset( void );
	void EndlessParseBotUpgrades( void );
	void EndlessRollEscalation( void );
	void EndlessSetAttributesForBot( CTFBot *pBot );
	bool EndlessShouldResetFlag( void );
	CheckpointSnapshotInfo *FindCheckpointSnapshot( CSteamID steamID ) const;
	CheckpointSnapshotInfo *FindCheckpointSnapshot( CTFPlayer *pPlayer ) const;
	static void FindDefaultPopulationFileShortNames( CUtlVector<CUtlString> &outNames );
	PlayerUpgradeHistory *FindOrAddPlayerUpgradeHistory( CSteamID steamID ) const;
	PlayerUpgradeHistory *FindOrAddPlayerUpgradeHistory( CTFPlayer *pPlayer ) const;
	bool FindPopulationFileByShortName( char const *pszName, CUtlString *outFullName );
	void ForgetOtherBottleUpgrades( CTFPlayer *pPlayer, CEconItemView *pItem, int nUpgradeKept );
	void GameRulesThink( void );
	CWave *GetCurrentWave( void );
	float GetDamageMultiplier( void ) const;
	float GetHealthMultiplier( bool bTankMultiplier ) const;
	int GetNumBuybackCreditsForPlayer( CTFPlayer *pPlayer );
	int GetNumRespecsAvailableForPlayer( CTFPlayer *pPlayer );
	int GetPlayerCurrencySpent( CTFPlayer *pPlayer );
	CUtlVector<CUpgradeInfo> *GetPlayerUpgradeHistory( CTFPlayer *pPlayer );
	char const *GetPopulationFilename( void ) const;
	char const *GetPopulationFilenameShort( void ) const;
	int GetRespawnWaveTime( void ) const { return m_nRespawnWaveTime; }
	void GetSentryBusterDamageAndKillThreshold( int &nNumDamage, int &nNumKills );
	int GetStartingCurrency( void ) const { return m_nStartingCurrency; }
	KeyValues *GetTemplate( const char *pszName ) const;
	int GetTotalPopFileCurrency( void );
	bool HasEventChangeAttributes( char const *pszEventName );
	bool IsAdvanced( void ) const { return m_bIsAdvanced; }
	bool IsInEndlessWaves( void ) const;
	bool IsPlayerBeingTrackedForBuybacks( CTFPlayer *pPlayer );
	bool IsValidMvMMap( char const *pszMapName );
	void JumpToWave( uint nWave, float f1 = -1.0f );
	void LoadLastKnownMission( void );
	void LoadMissionCycleFile( void );
	void LoadMvMMission( KeyValues *pMissionKV );
	void MarkAllCurrentPlayersSafeToLeave( void );
	void MvMVictory( void );
	void OnCurrencyCollected( int nCurrency, bool b1, bool b2 );
	void OnCurrencyPackFade( void );
	void OnPlayerKilled( CTFPlayer *pPlayer );
	bool Parse( void );
	void PauseSpawning( void );
	void PlayerDoneViewingLoot( CTFPlayer const *pPlayer );
	void PostInitialize( void );
	void RemoveBuybackCreditFromPlayer( CTFPlayer *pPlayer );
	void RemovePlayerAndItemUpgradesFromHistory( CTFPlayer *pPlayer );
	void ResetMap( void );
	void ResetRespecPoints( void );
	void RestoreCheckpoint( void );
	void RestoreItemToCheckpoint( CTFPlayer *pPlayer, CEconItemView *pItem );
	void RestorePlayerCurrency( void );
	void SendUpgradesToPlayer( CTFPlayer *pPlayer );
	void SetBuybackCreditsForPlayer( CTFPlayer *pPlayer, int nCredits );
	void SetCheckpoint( int nWave );
	void SetNumRespecsForPlayer( CTFPlayer *pplayer, int nRespecs );
	void SetPopulationFilename( char const *pszFileName );
	void SetupOnRoundStart( void );
	void ShowNextWaveDescription( void );
	void StartCurrentWave( void );
	void UnpauseSpawning( void );
	void Update( void );
	void UpdateObjectiveResource( void );
	void WaveEnd( bool b1 );

	int m_nCurrentWaveIndex;
	CUtlVector<CWave *> m_Waves;

private:
	CUtlVector<IPopulator *> m_Populators;
	char m_szPopfileFull[MAX_PATH];
	char m_szPopfileShort[MAX_PATH];
	KeyValues *m_pTemplates;
	bool m_bIsInitialized;
	int m_nStartingCurrency;
	int m_nLobbyBonusCurrency;
	int m_nMvMEventPopfileType;
	int m_nRespawnWaveTime;
	bool m_bFixedRespawnWaveTime;
	bool m_bCanBotsAttackWhileInSpawnRoom;
	int m_iSentryBusterDamageDealtThreshold;
	int m_iSentryBusterKillThreshold;
	bool m_bIsAdvanced;
	bool m_bIsEndless;
};

inline KeyValues *CPopulationManager::GetTemplate( const char *pszName ) const
{
	return m_pTemplates->FindKey( pszName );
}

extern CPopulationManager *g_pPopulationManager;
#endif