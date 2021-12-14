//========= Copyright © Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================

#ifdef GAME_DLL
#include "tf_powerup.h"
#endif

#ifdef CLIENT_DLL
#define CBonusPack C_BonusPack
#endif

DECLARE_AUTO_LIST( IBonusPackAutoList );

class CBonusPack :
#ifdef GAME_DLL
	/*public CCurrencyPack*/public CTFPowerup, public IBonusPackAutoList
#else
	public C_BaseAnimating, public IBonusPackAutoList
#endif
{
#ifdef GAME_DLL
	DECLARE_CLASS( CBonusPack, CTFPowerup );
#else
	DECLARE_CLASS( CBonusPack, C_BaseAnimating );
#endif
public:
	DECLARE_DATADESC();
	DECLARE_NETWORKCLASS();

	CBonusPack();

	virtual void	Spawn( void );
	virtual void	Precache( void );
#ifdef GAME_DLL
	//virtual bool	AffectedByRadiusCollection() const { return false; }
	virtual bool	MyTouch( CBasePlayer *pPlayer );
	virtual bool	ValidTouch( CBasePlayer *pPlayer );

	virtual const char *GetDefaultPowerupModel( void )
	{
		return "models/bots/bot_worker/bot_worker_powercore.mdl";
	}

	void			BlinkThink( void );

private:
	float m_flRemoveAt;
	float m_flAllowPickupAt;
	int m_nBlinkState;
#endif
};