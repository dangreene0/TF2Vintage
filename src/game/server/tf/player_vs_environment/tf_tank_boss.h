//========= Copyright © Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================
#ifndef TF_TANK_BOSS_H
#define TF_TANK_BOSS_H


#include "NextBotBodyInterface.h"
#include "tf_base_boss.h"
#include "pathtrack.h"

class INextBot;


class CTFTankBossBody : public IBody
{
public:
	CTFTankBossBody( INextBot *bot );
	virtual ~CTFTankBossBody() { }

	virtual void Update( void );
	virtual unsigned int GetSolidMask( void ) const;

	bool StartSequence( const char *name );
	void SetSkin( int nSkin );
};

class CTFTankBoss : public CTFBaseBoss
{
	static float sm_flLastTankAlert;
public:
	DECLARE_CLASS( CTFTankBoss, CTFBaseBoss );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CTFTankBoss();
	virtual ~CTFTankBoss();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual void SetSkin( int nSkin );
	virtual void UpdateOnRemove( void );
	virtual void UpdateCollisionBounds( void );

	virtual CTFTankBossBody *GetBodyInterface( void ) const OVERRIDE { return m_body; }

	virtual int OnTakeDamage_Alive( const CTakeDamageInfo &rawInfo );
	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual int GetCurrencyValue( void );

	void TankBossThink( void );

	void InputDestroyIfAtCapturePoint( inputdata_t &inputdata );
	void InputAddCaptureDestroyPostfix( inputdata_t &inputdata );

	void SetStartingPathTrackNode( char *pszName );

protected:
	virtual void ModifyDamage( CTakeDamageInfo *info ) OVERRIDE;

private:
	void Explode( void );

	void UpdatePingSound( void );
	float m_flLastPingTime;

	CTFTankBossBody *m_body;

	int m_nLastHealth;
	int m_iDamageModelIndex;
	int m_nDeathAnimPick;
	char m_szDeathPostfix[8];
	bool m_bKilledByPlayers;

	CHandle<CPathTrack> m_hStartNode;
	CHandle<CPathTrack> m_hEndNode;
	CHandle<CPathTrack> m_hGoalNode;

	Vector m_vecCollisionMins;
	Vector m_vecCollisionMaxs;

	CHandle<CBaseAnimating> m_hBomb;
	CHandle<CBaseAnimating> m_hLeftTrack;
	CHandle<CBaseAnimating> m_hRightTrack;

	Vector m_vecRightTrackPrevPos;
	Vector m_vecLeftTrackPrevPos;
};

inline void CTFTankBoss::SetSkin( int nSkin )
{
	if ( m_body )
		m_body->SetSkin( nSkin );
}

#endif
