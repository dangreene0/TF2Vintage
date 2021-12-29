//========= Copyright © Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================
#ifndef TF_POPULATOR_SPAWNERS_H
#define TF_POPULATOR_SPAWNERS_H

#include "tf_bot.h"

class IPopulator;
struct EventInfo;

class IPopulationSpawner
{
public:
	IPopulationSpawner( IPopulator *populator ) : m_populator( populator ) { }
	virtual ~IPopulationSpawner() { }

	static IPopulationSpawner *ParseSpawner( IPopulator *populator, KeyValues *data );

	virtual bool	Parse( KeyValues *data ) = 0;
	virtual bool	Spawn( const Vector &vecPos, CUtlVector< CHandle<CBaseEntity> > *pOutVec = NULL ) = 0;
	virtual bool	IsWhereRequired( void ) const { return true; }

	virtual bool	IsVarious( void ) { return false; }
	virtual int		GetClass( int nSpawnNum = -1 ) { return TF_CLASS_UNDEFINED; }
	virtual string_t GetClassIcon( int nSpawnNum = -1 ) { return NULL_STRING; }
	virtual int		GetHealth( int nSpawnNum = -1 ) { return 0; }
	virtual bool	IsMiniBoss( int nSpawnNum = -1 ) { return false; }
	virtual bool	HasAttribute( CTFBot::AttributeType type, int nSpawnNum = -1 ) { return false; }
	virtual bool	HasEventChangeAttributes( const char *pszEventName ) const = 0;

	IPopulator *m_populator;
};

class CTFBotSpawner : public IPopulationSpawner
{
public:
	CTFBotSpawner( IPopulator *populator );
	virtual ~CTFBotSpawner() { }

	virtual bool	Parse( KeyValues *data );
	virtual bool	Spawn( const Vector &vecPos, CUtlVector< CHandle<CBaseEntity> > *pOutVec = NULL );

	virtual int		GetClass( int nSpawnNum = -1 );
	virtual string_t GetClassIcon( int nSpawnNum = -1 );
	virtual int		GetHealth( int nSpawnNum = -1 );
	virtual bool	IsMiniBoss( int nSpawnNum = -1 );
	virtual bool	HasAttribute( CTFBot::AttributeType type, int nSpawnNum = -1 );
	virtual bool	HasEventChangeAttributes( const char *pszEventName ) const;

	bool			ParseEventChangeAttributes( KeyValues *data );
};

class CTankSpawner : public IPopulationSpawner
{
public:
	CTankSpawner( IPopulator *populator );

	virtual bool	Parse( KeyValues *data );
	virtual bool	Spawn( const Vector &vecPos, CUtlVector< CHandle<CBaseEntity> > *pOutVec = NULL );
	virtual bool	IsWhereRequired( void ) const OVERRIDE { return false; }

	virtual string_t GetClassIcon( int nSpawnNum = -1 ) { return MAKE_STRING( "tank" ); }
	virtual int		GetHealth( int nSpawnNum = -1 ) { return m_iHealth; }
	virtual bool	IsMiniBoss( int nSpawnNum = -1 ) OVERRIDE { return true; }
	virtual bool	HasEventChangeAttributes( const char *pszEventName ) const OVERRIDE { return false; }

	int m_iHealth;
	float m_flSpeed;
	CUtlString m_szName;
	CUtlString m_szStartingPathTrackNodeName;
	int m_nSkin;
	EventInfo *m_onKilledEvent;
	EventInfo *m_onBombDroppedEvent;
};

class CSentryGunSpawner : public IPopulationSpawner
{
public:
	CSentryGunSpawner( IPopulator *populator );

	virtual bool	Parse( KeyValues *data );
	virtual bool	Spawn( const Vector &vecPos, CUtlVector< CHandle<CBaseEntity> > *pOutVec = NULL );
	virtual bool	HasEventChangeAttributes( const char *pszEventName ) const OVERRIDE { return false; }

	int m_nLevel;
};


//-----------------------------------------------------------------------
class CSquadSpawner : public IPopulationSpawner
{
public:
	CSquadSpawner( IPopulator *populator );
	virtual ~CSquadSpawner();

	virtual bool	Parse( KeyValues *data );
	virtual bool	Spawn( const Vector &vecPos, CUtlVector< CHandle<CBaseEntity> > *pOutVec = NULL );

	virtual bool	IsVarious( void ) { return true; }
	virtual int		GetClass( int nSpawnNum = -1 );
	virtual string_t GetClassIcon( int nSpawnNum = -1 );
	virtual int		GetHealth( int nSpawnNum = -1 );
	virtual bool	IsMiniBoss( int nSpawnNum = -1 );
	virtual bool	HasAttribute( CTFBot::AttributeType type, int nSpawnNum = -1 );
	virtual bool	HasEventChangeAttributes( const char *pszEventName ) const;

	CUtlVector<IPopulationSpawner *> m_SquadSpawners;
	float m_flFormationSize;
	bool m_bShouldPreserveSquad;
};

class CMobSpawner : public IPopulationSpawner
{
public:
	CMobSpawner( IPopulator *populator );
	virtual ~CMobSpawner();

	virtual bool	Parse( KeyValues *data );
	virtual bool	Spawn( const Vector &vecPos, CUtlVector< CHandle<CBaseEntity> > *pOutVec = NULL );
	virtual bool	HasEventChangeAttributes( const char *pszEventName ) const;

	int m_nSpawnCount;
	IPopulationSpawner *m_pSpawner;
};

class CRandomChoiceSpawner : public IPopulationSpawner
{
public:
	CRandomChoiceSpawner( IPopulator *populator );
	virtual ~CRandomChoiceSpawner();

	virtual bool	Parse( KeyValues *data );
	virtual bool	Spawn( const Vector &vecPos, CUtlVector< CHandle< CBaseEntity > > *pOutVec = NULL );

	virtual bool	IsVarious( void ) OVERRIDE { return true; }
	virtual int		GetClass( int nSpawnNum = -1 );
	virtual string_t GetClassIcon( int nSpawnNum = -1 );
	virtual int		GetHealth( int nSpawnNum = -1 );
	virtual bool	IsMiniBoss( int nSpawnNum = -1 );
	virtual bool	HasAttribute( CTFBot::AttributeType type, int nSpawnNum = -1 );
	virtual bool	HasEventChangeAttributes( const char *pszEventName ) const;

	CUtlVector<IPopulationSpawner *> m_Spawners;
	CUtlVector<int> m_RandomPicks;
	int m_nNumSpawned;
};

#endif