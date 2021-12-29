//========= Copyright © Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "tf_population_manager.h"
#include "tf_populator_spawners.h"
#include "tf_obj_sentrygun.h"
#include "tf_tank_boss.h"
#include "tf_bot_squad.h"

extern ConVar tf_populator_debug;

ConVar tf_debug_placement_failure( "tf_debug_placement_failure", "0", FCVAR_CHEAT );

extern EventInfo *ParseEvent( KeyValues *data );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
IPopulationSpawner *IPopulationSpawner::ParseSpawner( IPopulator *pPopulator, KeyValues *data )
{
	const char *pszKey = data->GetName();
	if ( !V_stricmp( pszKey, "TFBot" ) )
	{
		CTFBotSpawner *botSpawner = new CTFBotSpawner( pPopulator );
		if ( !botSpawner->Parse( data ) )
		{
			Warning( "Warning reading TFBot spawner definition\n" );
			delete botSpawner;
			return NULL;
		}

		return botSpawner;
	}
	else if ( !V_stricmp( pszKey, "Tank" ) )
	{
		CTankSpawner *tankSpawner = new CTankSpawner( pPopulator );
		if ( !tankSpawner->Parse( data ) )
		{
			Warning( "Warning reading Tank spawner definition\n" );
			delete tankSpawner;
			return NULL;
		}

		return tankSpawner;
	}
	else if ( !V_stricmp( pszKey, "SentryGun" ) )
	{
		CSentryGunSpawner *sentrySpawner = new CSentryGunSpawner( pPopulator );
		if ( !sentrySpawner->Parse( data ) )
		{
			Warning( "Warning reading SentryGun spawner definition\n" );
			delete sentrySpawner;
			return NULL;
		}

		return sentrySpawner;
	}
	else if ( !V_stricmp( pszKey, "Squad" ) )
	{
		CSquadSpawner *squadSpawner = new CSquadSpawner( pPopulator );
		if ( !squadSpawner->Parse( data ) )
		{
			Warning( "Warning reading Squad spawner definition\n" );
			delete squadSpawner;
			return NULL;
		}

		return squadSpawner;
	}
	else if ( !V_stricmp( pszKey, "Mob" ) )
	{
		CMobSpawner *mobSpawner = new CMobSpawner( pPopulator );
		if ( !mobSpawner->Parse( data ) )
		{
			Warning( "Warning reading Mob spawner definition\n" );
			delete mobSpawner;
			return NULL;
		}

		return mobSpawner;
	}
	else if ( !V_stricmp( pszKey, "RandomChoice" ) )
	{
		CRandomChoiceSpawner *randomSpawner = new CRandomChoiceSpawner( pPopulator );
		if ( !randomSpawner->Parse( data ) )
		{
			Warning( "Warning reading RandomChoice spawner definition\n" );
			delete randomSpawner;
			return NULL;
		}

		return randomSpawner;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFBotSpawner::CTFBotSpawner( IPopulator *pPopulator )
	: IPopulationSpawner( pPopulator )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFBotSpawner::Parse( KeyValues *data )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFBotSpawner::Spawn( const Vector &vecPos, CUtlVector<CHandle<CBaseEntity>> *pOutVec )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFBotSpawner::GetClass( int nSpawnNum )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
string_t CTFBotSpawner::GetClassIcon( int nSpawnNum )
{
	return string_t();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFBotSpawner::GetHealth( int nSpawnNum )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFBotSpawner::IsMiniBoss( int nSpawnNum )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFBotSpawner::HasAttribute( CTFBot::AttributeType type, int nSpawnNum )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFBotSpawner::HasEventChangeAttributes( const char *pszEventName ) const
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFBotSpawner::ParseEventChangeAttributes( KeyValues *data )
{
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTankSpawner::CTankSpawner( IPopulator *pPopulator )
	: IPopulationSpawner( pPopulator )
{
	m_iHealth = 50000;
	m_flSpeed = 75.0f;
	m_szName = "Tank";
	m_onKilledEvent = NULL;
	m_onBombDroppedEvent = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTankSpawner::Parse( KeyValues *data )
{
	FOR_EACH_SUBKEY( data, pSubKey )
	{
		const char *pszKey = pSubKey->GetName();
		if ( !V_stricmp( pszKey, "Health" ) )
			m_iHealth = pSubKey->GetInt();
		else if ( !V_stricmp( pszKey, "Speed" ) )
			m_flSpeed = pSubKey->GetFloat();
		else if ( !V_stricmp( pszKey, "Name" ) )
			m_szName = pSubKey->GetString();
		else if ( !V_stricmp( pszKey, "Skin" ) )
			m_nSkin = pSubKey->GetInt();
		else if ( !V_stricmp( pszKey, "StartingPathTrackNode" ) )
			m_szStartingPathTrackNodeName = pSubKey->GetName();
		else if ( !V_stricmp( pszKey, "OnKilledOutput" ) )
			m_onKilledEvent = ParseEvent( pSubKey );
		else if ( !V_stricmp( pszKey, "OnBombDroppedOutput" ) )
			m_onBombDroppedEvent = ParseEvent( pSubKey );
		else
		{
			Warning( "Invalid attribute '%s' in Tank definition\n", pszKey );
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTankSpawner::Spawn( const Vector &vecPos, CUtlVector<CHandle<CBaseEntity>> *pOutVec )
{
	CTFTankBoss *pTank = (CTFTankBoss *)CreateEntityByName( "tank_boss" );
	if ( pTank == NULL )
		return false;

	pTank->SetAbsOrigin( vecPos );
	pTank->SetAbsAngles( vec3_angle );
	pTank->SetInitialHealth( m_iHealth * g_pPopulationManager->GetHealthMultiplier( true ) );
	pTank->SetMaxSpeed( m_flSpeed );
	pTank->SetName( MAKE_STRING( m_szName ) );
	pTank->SetSkin( m_nSkin );
	pTank->SetStartingPathTrackNode( m_szStartingPathTrackNodeName.GetForModify() );
	pTank->Spawn();
	pTank->SetOnKilledEvent( m_onKilledEvent );
	pTank->SetOnBombDroppedEvent( m_onBombDroppedEvent );

	if ( pOutVec )
		pOutVec->AddToTail( pTank );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSentryGunSpawner::CSentryGunSpawner( IPopulator *pPopulator )
	: IPopulationSpawner( pPopulator )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSentryGunSpawner::Parse( KeyValues *data )
{
	FOR_EACH_SUBKEY( data, pSubKey )
	{
		const char *pszKey = pSubKey->GetName();
		if ( !V_stricmp( pszKey, "Level" ) )
		{
			m_nLevel = pSubKey->GetInt();
		}
		else
		{
			Warning( "Invalid attribute '%s' in SentryGun definition\n", pszKey );
			return false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSentryGunSpawner::Spawn( const Vector &vecPos, CUtlVector<CHandle<CBaseEntity>> *pOutVec )
{
	CObjectSentrygun *pSentry = (CObjectSentrygun *)CreateEntityByName( "obj_sentrygun" );
	if ( pSentry == NULL )
	{
		if ( tf_populator_debug.GetBool() )
			DevMsg( "CSentryGunSpawner: %3.2f: Failed to create obj_sentrygun\n", gpGlobals->curtime );

		return false;
	}

	pSentry->SetAbsOrigin( vecPos );
	pSentry->SetAbsAngles( vec3_angle );
	pSentry->Spawn();
	pSentry->ChangeTeam( TF_TEAM_RED );
	pSentry->SetDefaultUpgradeLevel( m_nLevel + 1 );
	pSentry->InitializeMapPlacedObject();

	if( pOutVec )
		pOutVec->AddToTail( pSentry );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSquadSpawner::CSquadSpawner( IPopulator *pPopulator )
	: IPopulationSpawner( pPopulator )
{
	m_flFormationSize = -1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSquadSpawner::~CSquadSpawner()
{
	m_SquadSpawners.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSquadSpawner::Parse( KeyValues *data )
{
	FOR_EACH_SUBKEY( data, pSubKey )
	{
		char const *pszKey = pSubKey->GetName();
		if ( !V_stricmp( pszKey, "FormationSize" ) )
		{
			m_flFormationSize = pSubKey->GetFloat();
		}
		else if ( !V_stricmp( pszKey, "ShouldPreserveSquad" ) )
		{
			m_bShouldPreserveSquad = pSubKey->GetBool();
		}
		else
		{
			IPopulationSpawner *pSpawner = IPopulationSpawner::ParseSpawner( m_populator, pSubKey );
			if ( pSpawner )
			{
				m_SquadSpawners.AddToTail( pSpawner );
			}
			else
			{
				Warning( "Unknown attribute '%s' in Mob definition.\n", pszKey );
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSquadSpawner::Spawn( const Vector &vecPos, CUtlVector<CHandle<CBaseEntity>> *pOutVec )
{
	VPROF_BUDGET( __FUNCTION__, "NextBot" );

	if ( tf_populator_debug.GetBool() )
	{
		DevMsg( "CSquadSpawner: %3.2f: <<<< Spawning Squad >>>>\n", gpGlobals->curtime );
	}

	bool bFullySpawned = false;
	CUtlVector<CHandle<CBaseEntity>> spawnedMembers;
	FOR_EACH_VEC( m_SquadSpawners, i )
	{
		if ( !m_SquadSpawners[i]->Spawn( vecPos, &spawnedMembers ) )
		{
			bFullySpawned = false;
			break;
		}
	}

	if ( !bFullySpawned )
	{
		if ( tf_populator_debug.GetBool() )
		{
			DevMsg( "%3.2f: CSquadSpawner: Unable to spawn entire squad\n", gpGlobals->curtime );
		}

		FOR_EACH_VEC( spawnedMembers, i )
		{
			UTIL_Remove( spawnedMembers[i] );
		}

		return false;
	}

	CTFBotSquad *pSquad = new CTFBotSquad();
	if ( pSquad )
	{
		pSquad->SetFormationSize( m_flFormationSize );
		pSquad->SetShouldPreserveSquad( m_bShouldPreserveSquad );

		FOR_EACH_VEC( spawnedMembers, i )
		{
			CTFBot *pBot = ToTFBot( spawnedMembers[i] );
			if ( pBot )
			{
				pBot->JoinSquad( pSquad );
			}
		}
	}

	if ( pOutVec )
		pOutVec->AddVectorToTail( spawnedMembers );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CSquadSpawner::GetClass( int nSpawnNum )
{
	if ( nSpawnNum < 0 || m_SquadSpawners.IsEmpty() )
		return TF_CLASS_UNDEFINED;

	int nWhich = nSpawnNum % m_SquadSpawners.Count();
	if ( m_SquadSpawners[ nWhich ]->IsVarious() )
	{
		DevWarning( "Nested complex spawner types... need a method for counting these." );
		return TF_CLASS_UNDEFINED;
	}

	return m_SquadSpawners[ nWhich ]->GetClass();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
string_t CSquadSpawner::GetClassIcon( int nSpawnNum )
{
	if ( nSpawnNum < 0 || m_SquadSpawners.IsEmpty() )
		return NULL_STRING;

	int nWhich = nSpawnNum % m_SquadSpawners.Count();
	if ( m_SquadSpawners[nWhich]->IsVarious() )
	{
		DevWarning( "Nested complex spawner types... need a method for counting these." );
		return NULL_STRING;
	}

	return m_SquadSpawners[nWhich]->GetClassIcon();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CSquadSpawner::GetHealth( int nSpawnNum )
{
	if ( nSpawnNum < 0 || m_SquadSpawners.IsEmpty() )
		return 0;

	int nWhich = nSpawnNum % m_SquadSpawners.Count();
	if ( m_SquadSpawners[nWhich]->IsVarious() )
	{
		DevWarning( "Nested complex spawner types... need a method for counting these." );
		return 0;
	}

	return m_SquadSpawners[nWhich]->GetHealth();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSquadSpawner::IsMiniBoss( int nSpawnNum )
{
	if ( nSpawnNum < 0 || m_SquadSpawners.IsEmpty() )
		return false;

	int nWhich = nSpawnNum % m_SquadSpawners.Count();
	if ( m_SquadSpawners[nWhich]->IsVarious() )
	{
		DevWarning( "Nested complex spawner types... need a method for counting these." );
		return false;
	}

	return m_SquadSpawners[nWhich]->IsMiniBoss();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSquadSpawner::HasAttribute( CTFBot::AttributeType type, int nSpawnNum )
{
	if ( nSpawnNum < 0 || m_SquadSpawners.IsEmpty() )
		return false;

	int nWhich = nSpawnNum % m_SquadSpawners.Count();
	if ( m_SquadSpawners[nWhich]->IsVarious() )
	{
		DevWarning( "Nested complex spawner types... need a method for counting these." );
		return false;
	}

	return m_SquadSpawners[nWhich]->HasAttribute( type );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSquadSpawner::HasEventChangeAttributes( const char *pszEventName ) const
{
	bool bHasEventChangeAttributes = false;
	FOR_EACH_VEC( m_SquadSpawners, i )
	{
		bHasEventChangeAttributes |= m_SquadSpawners[i]->HasEventChangeAttributes( pszEventName );
	}

	return bHasEventChangeAttributes;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMobSpawner::CMobSpawner( IPopulator *pPopulator )
	: IPopulationSpawner( pPopulator )
{
	m_pSpawner = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMobSpawner::~CMobSpawner()
{
	if ( m_pSpawner )
	{
		delete m_pSpawner;
		m_pSpawner = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMobSpawner::Parse( KeyValues *data )
{
	FOR_EACH_SUBKEY( data, pSubKey )
	{
		char const *pszKey = pSubKey->GetName();
		if ( !V_stricmp( pszKey, "Count" ) )
		{
			m_nSpawnCount = pSubKey->GetInt();
		}
		else
		{
			IPopulationSpawner *pSpawner = IPopulationSpawner::ParseSpawner( m_populator, pSubKey );
			if ( pSpawner )
			{
				if ( m_pSpawner == NULL )
					m_pSpawner = pSpawner;
				else
					Warning( "CMobSpawner: Duplicate spawner encountered - discarding!\n" );
			}
			else
			{
				Warning( "Unknown attribute '%s' in Mob definition.\n", pszKey );
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMobSpawner::Spawn( const Vector &vecPos, CUtlVector<CHandle<CBaseEntity>> *pOutVec )
{
	if ( !m_pSpawner )
		return false;

	int nNumSpawned = 0;
	if ( m_nSpawnCount > 0 )
	{
		while ( m_pSpawner->Spawn( vecPos, pOutVec ) )
		{
			nNumSpawned++;

			if ( m_nSpawnCount <= nNumSpawned )
				return true;
		}

		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMobSpawner::HasEventChangeAttributes( const char *pszEventName ) const
{
	if ( m_pSpawner )
		return m_pSpawner->HasEventChangeAttributes( pszEventName );

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRandomChoiceSpawner::CRandomChoiceSpawner( IPopulator *pPopulator )
	: IPopulationSpawner( pPopulator )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRandomChoiceSpawner::~CRandomChoiceSpawner()
{
	m_Spawners.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CRandomChoiceSpawner::Parse( KeyValues *data )
{
	FOR_EACH_SUBKEY( data, pSubKey )
	{
		const char *pszKey = pSubKey->GetName();
		IPopulationSpawner *pSawner = IPopulationSpawner::ParseSpawner( m_populator, pSubKey );
		if ( pSawner )
			m_Spawners.AddToTail( pSawner );
		else
			Warning( "Unknown attribute '%s' in RandomChoice definition.\n", pszKey );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CRandomChoiceSpawner::Spawn( const Vector &vecPos, CUtlVector<CHandle<CBaseEntity>> *pOutVec )
{
	if ( m_Spawners.IsEmpty() )
		return false;

	int nCurrentCount = m_RandomPicks.Count();
	int nNumToAdd = ( m_nNumSpawned + 1 ) - nCurrentCount;

	if ( nNumToAdd > 0 )
	{
		m_RandomPicks.AddMultipleToTail( nNumToAdd );

		for ( int i = nCurrentCount; i < m_nNumSpawned + 1; ++i )
		{
			m_RandomPicks[i] = RandomInt( 0, m_Spawners.Count() - 1 );
		}
	}

	return m_Spawners[ m_RandomPicks[ m_nNumSpawned++ ] ]->Spawn( vecPos, pOutVec );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CRandomChoiceSpawner::GetClass( int nSpawnNum )
{
	if ( m_Spawners.IsEmpty() )
		return TF_CLASS_UNDEFINED;

	int nCurrentCount = m_RandomPicks.Count();
	int nNumToAdd = ( nSpawnNum + 1 ) - nCurrentCount;

	if ( nNumToAdd > 0 )
	{
		m_RandomPicks.AddMultipleToTail( nNumToAdd );

		for ( int i = nCurrentCount; i < nSpawnNum + 1; ++i )
		{
			m_RandomPicks[i] = RandomInt( 0, m_Spawners.Count() - 1 );
		}
	}

	if ( m_Spawners[ m_RandomPicks[ nSpawnNum ] ]->IsVarious() )
	{
		DevWarning( "Nested complex spawner types... need a method for counting these." );
		return TF_CLASS_UNDEFINED;
	}

	return m_Spawners[ m_RandomPicks[ nSpawnNum ] ]->GetClass( nSpawnNum );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
string_t CRandomChoiceSpawner::GetClassIcon( int nSpawnNum )
{
	if ( m_Spawners.IsEmpty() )
		return NULL_STRING;

	int nCurrentCount = m_RandomPicks.Count();
	int nNumToAdd = ( nSpawnNum + 1 ) - nCurrentCount;

	if ( nNumToAdd > 0 )
	{
		m_RandomPicks.AddMultipleToTail( nNumToAdd );

		for ( int i = nCurrentCount; i < nSpawnNum + 1; ++i )
		{
			m_RandomPicks[i] = RandomInt( 0, m_Spawners.Count() - 1 );
		}
	}

	if ( m_Spawners[m_RandomPicks[nSpawnNum]]->IsVarious() )
	{
		DevWarning( "Nested complex spawner types... need a method for counting these." );
		return NULL_STRING;
	}

	return m_Spawners[ m_RandomPicks[ nSpawnNum ] ]->GetClassIcon( nSpawnNum );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CRandomChoiceSpawner::GetHealth( int nSpawnNum )
{
	if ( m_Spawners.IsEmpty() )
		return 0;

	int nCurrentCount = m_RandomPicks.Count();
	int nNumToAdd = ( nSpawnNum + 1 ) - nCurrentCount;

	if ( nNumToAdd > 0 )
	{
		m_RandomPicks.AddMultipleToTail( nNumToAdd );

		for ( int i = nCurrentCount; i < nSpawnNum + 1; ++i )
		{
			m_RandomPicks[i] = RandomInt( 0, m_Spawners.Count() - 1 );
		}
	}

	if ( m_Spawners[ m_RandomPicks[ nSpawnNum ] ]->IsVarious() )
	{
		DevWarning( "Nested complex spawner types... need a method for counting these." );
		return 0;
	}

	return m_Spawners[ m_RandomPicks[ nSpawnNum ] ]->GetHealth( nSpawnNum );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CRandomChoiceSpawner::IsMiniBoss( int nSpawnNum )
{
	if ( m_Spawners.IsEmpty() )
		return false;

	int nCurrentCount = m_RandomPicks.Count();
	int nNumToAdd = ( nSpawnNum + 1 ) - nCurrentCount;

	if ( nNumToAdd > 0 )
	{
		m_RandomPicks.AddMultipleToTail( nNumToAdd );

		for ( int i = nCurrentCount; i < nSpawnNum + 1; ++i )
		{
			m_RandomPicks[i] = RandomInt( 0, m_Spawners.Count() - 1 );
		}
	}

	if ( m_Spawners[m_RandomPicks[nSpawnNum]]->IsVarious() )
	{
		DevWarning( "Nested complex spawner types... need a method for counting these." );
		return false;
	}

	return m_Spawners[ m_RandomPicks[ nSpawnNum ] ] ->IsMiniBoss( nSpawnNum );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CRandomChoiceSpawner::HasAttribute( CTFBot::AttributeType type, int nSpawnNum )
{
	if ( m_Spawners.IsEmpty() )
		return false;

	int nCurrentCount = m_RandomPicks.Count();
	int nNumToAdd = ( nSpawnNum + 1 ) - nCurrentCount;

	if ( nNumToAdd > 0 )
	{
		m_RandomPicks.AddMultipleToTail( nNumToAdd );

		for ( int i = nCurrentCount; i < nSpawnNum + 1; ++i )
		{
			m_RandomPicks[i] = RandomInt( 0, m_Spawners.Count() - 1 );
		}
	}

	if ( m_Spawners[ m_RandomPicks[ nSpawnNum ] ]->IsVarious() )
	{
		DevWarning( "Nested complex spawner types... need a method for counting these." );
		return false;
	}

	return m_Spawners[ m_RandomPicks[ nSpawnNum ] ]->HasAttribute( type, nSpawnNum );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CRandomChoiceSpawner::HasEventChangeAttributes( const char *pszEventName ) const
{
	bool bHasEventChangeAttributes = false;
	FOR_EACH_VEC( m_Spawners, i )
	{
		bHasEventChangeAttributes |= m_Spawners[i]->HasEventChangeAttributes( pszEventName );
	}

	return bHasEventChangeAttributes;
}
