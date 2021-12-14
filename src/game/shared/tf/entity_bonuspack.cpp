//========= Copyright © Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "entity_bonuspack.h"

#ifdef GAME_DLL
#include "particle_parse.h"
#include "tf_player.h"
#endif

ConVar tf_bonuspack_score( "tf_bonuspack_score", "1", FCVAR_REPLICATED|FCVAR_DEVELOPMENTONLY );

IMPLEMENT_NETWORKCLASS_ALIASED( BonusPack, DT_CBonusPack )

BEGIN_NETWORK_TABLE( CBonusPack, DT_CBonusPack )
END_NETWORK_TABLE()

BEGIN_DATADESC( CBonusPack )
END_DATADESC()

LINK_ENTITY_TO_CLASS( item_bonuspack, CBonusPack );

IMPLEMENT_AUTO_LIST( IBonusPackAutoList );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBonusPack::CBonusPack()
{
#ifdef GAME_DLL
	m_nBlinkState = 0;
#else
	SetCycle( RandomFloat() );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBonusPack::Spawn( void )
{
	Precache();
	BaseClass::Spawn();

#ifdef GAME_DLL
	const char *pszParticleName = GetTeamNumber() == TF_TEAM_RED ? "powercore_alert_blue" : "powercore_alert_red";
	DispatchParticleEffect( pszParticleName, PATTACH_POINT_FOLLOW, this, "particle_spawn" );

	SetModel( GetPowerupModel() );
	CollisionProp()->UseTriggerBounds( true, 64 );
	SetRenderMode( kRenderTransAlpha );

	m_flAllowPickupAt = gpGlobals->curtime + 0.5f;

	m_flRemoveAt = gpGlobals->curtime + 25.0f;
	SetContextThink( &CBonusPack::BlinkThink, gpGlobals->curtime + 20.0f, "blink_think" );
	SetContextThink( &CBonusPack::SUB_Remove, m_flRemoveAt, "RemoveThink" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBonusPack::Precache( void )
{
	BaseClass::Precache();

	bool bAllowPrecache = CBaseEntity::IsPrecacheAllowed();
	CBaseEntity::SetAllowPrecache( true );

	PrecacheParticleSystem( "powercore_embers_red" );
	PrecacheParticleSystem( "powercore_embers_blue" );

	CBaseEntity::SetAllowPrecache( bAllowPrecache );
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBonusPack::BlinkThink()
{
	float flTimeToKill = m_flRemoveAt - gpGlobals->curtime;
	float flNextBlink = RemapValClamped( flTimeToKill, 5.0f, 0.0f, 0.5f, 0.1f );
	SetContextThink( &CBonusPack::BlinkThink, gpGlobals->curtime + flNextBlink, "blink_think" );

	++m_nBlinkState;
	if ( m_nBlinkState % 2 == 0 )
	{
		SetRenderColorA( 25 );
	}
	else
	{
		SetRenderColorA( 255 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBonusPack::MyTouch( CBasePlayer *pPlayer )
{
	if ( ValidTouch( pPlayer ) && gpGlobals->curtime >= m_flAllowPickupAt )
	{
		CTFPlayer *pTFPlayer = ToTFPlayer( pPlayer );
		if ( !pTFPlayer )
			return true;

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBonusPack::ValidTouch( CBasePlayer *pPlayer )
{
	if ( pPlayer->GetTeamNumber() != GetTeamNumber() )
		return false;

	CTFPlayer *pTFPlayer = ToTFPlayer( pPlayer );
	if ( !pTFPlayer )
		return false;

	if ( pTFPlayer->m_Shared.InCond( TF_COND_STEALTHED ) || pTFPlayer->m_Shared.GetPercentInvisible() > 0.25f )
		return false;

	if ( pTFPlayer->m_Shared.InCond( TF_COND_DISGUISED ) || pTFPlayer->m_Shared.InCond( TF_COND_DISGUISING ) )
		return false;

	if ( pTFPlayer->m_Shared.InCond( TF_COND_PHASE ) || pTFPlayer->m_Shared.InCond( TF_COND_PASSTIME_INTERCEPTION ) )
		return false;

	if ( pTFPlayer->m_Shared.InCond( TF_COND_SELECTED_TO_TELEPORT ) )
		return false;

	if ( pTFPlayer->m_Shared.IsInvulnerable() )
		return false;

	return BaseClass::ValidTouch( pPlayer );
}
#endif