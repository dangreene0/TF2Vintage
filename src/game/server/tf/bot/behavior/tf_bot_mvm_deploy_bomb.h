//========= Copyright © Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================
#ifndef TF_BOT_MISSION_DESTROY_SENTRIES_H
#define TF_BOT_MISSION_DESTROY_SENTRIES_H

#include "NextBotBehavior.h"

class CTFBotMvMDeployBomb : public Action<CTFBot>
{
public:
	CTFBotMvMDeployBomb();
	virtual ~CTFBotMvMDeployBomb();

	virtual const char *GetName() const override;

	virtual ActionResult<CTFBot> OnStart( CTFBot *me, Action<CTFBot> *action ) override;
	virtual ActionResult<CTFBot> Update( CTFBot *me, float dt ) override;
	virtual void OnEnd( CTFBot *me, Action<CTFBot> *action ) override;

	virtual EventDesiredResult<CTFBot> OnContact( CTFBot *me, CBaseEntity *who, CGameTrace *trace ) override;

	virtual QueryResultType ShouldAttack( const INextBot *me, const CKnownEntity *threat ) const override;

private:
	CountdownTimer m_ctDelay;
	Vector m_vecStand;
};

#endif