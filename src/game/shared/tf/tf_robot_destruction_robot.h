//========= Copyright © Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================
#ifndef ROBOT_DESTRUCTION_ROBOT_H
#define ROBOT_DESTRUCTION_ROBOT_H

#ifdef GAME_DLL
	#include "tf_shareddefs.h"
	#include "pathtrack.h"
	#include "NextBotGroundLocomotion.h"
	#include "NextBotBehavior.h"
	#include "NextBot.h"
	#include "tf_obj_dispenser.h"
#else
	#include "c_obj_dispenser.h"
	#include "NextBot/c_NextBot.h"
#endif

#include "props_shared.h"

#ifdef CLIENT_DLL
#define NextBotCombatCharacter C_NextBotCombatCharacter
#define CTFRobotDestruction_Robot C_TFRobotDestruction_Robot

#define CObjectDispenser C_ObjectDispenser
#define CRobotDispenser C_RobotDispenser
#endif

class CTFRobotDestruction_Robot;

#ifdef GAME_DLL
class CRobotLocomotion : public NextBotGroundLocomotion
{
public:
	CRobotLocomotion( INextBot *bot );
	virtual ~CRobotLocomotion() { }

	virtual float GetGroundSpeed( void ) const OVERRIDE;
	virtual float GetRunSpeed( void ) const OVERRIDE;								// get maximum running speed
	virtual float GetStepHeight( void ) const OVERRIDE { return 24.f; }				// if delta Z is greater than this, we have to jump to get up
	virtual float GetMaxJumpHeight( void ) const OVERRIDE { return 40.f; }			// return maximum height of a jump
	virtual float GetMaxYawRate( void ) const OVERRIDE { return 200.f; }				// return max rate of yaw rotation

	virtual bool ShouldCollideWith( const CBaseEntity *object ) const OVERRIDE;
};

class CRobotBody : public IBody
{
public:
	CRobotBody( INextBot *actor );
	virtual ~CRobotBody() { };

	virtual void Update( void ) OVERRIDE;

	virtual unsigned int GetSolidMask( void ) const OVERRIDE { return MASK_NPCSOLID|CONTENTS_PLAYERCLIP; }

	virtual Activity GetActivity( void ) const OVERRIDE { return m_Activity; }
	virtual bool StartActivity( Activity act, unsigned int flags = 0 ) OVERRIDE;
	virtual bool IsActivity( Activity act ) const OVERRIDE { return m_Activity == act; }

	void Impulse( Vector const& vecImpulse );

private:
	void Approach( Vector& vecIn, Vector const& vecTarget, float flRate );

	Activity m_Activity;

	int m_iMoveX;
	int m_iMoveY;

	Vector m_vecPrevOrigin;
	Vector m_vecLean;
	Vector m_vecImpulse;
};
#endif


class CRobotDispenser : public CObjectDispenser
{
	DECLARE_CLASS( CRobotDispenser, CObjectDispenser )
public:
	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();

#ifdef GAME_DLL
	CRobotDispenser();

	virtual void Spawn( void );
	virtual void OnGoActive( void );
	virtual void GetControlPanelInfo( int nPanelIndex, const char *&pPanelName ) OVERRIDE { }
	virtual void SetModel( const char *pModel );
	virtual float GetDispenserRadius( void ) { return 128; }
	virtual float GetHealRate() { return 5.f; }

	virtual int DispenseMetal( CTFPlayer* ) { return 0; }
	virtual bool DispenseAmmo( CTFPlayer* ) { return false; }

	virtual void PlayActiveSound() OVERRIDE { }
#endif
};


class CTFRobotDestruction_Robot : public NextBotCombatCharacter
#ifdef CLIENT_DLL
	, public CGameEventListener
#endif
{
	DECLARE_CLASS( CTFRobotDestruction_Robot, NextBotCombatCharacter )
public:
	DECLARE_DATADESC();
	DECLARE_NETWORKCLASS();

	CTFRobotDestruction_Robot( void );
	virtual ~CTFRobotDestruction_Robot( void );

	virtual void Precache( void );

	virtual void Spawn( void );
	virtual bool ShouldCollide( int collisionGroup, int contentsMask ) const;

#ifdef GAME_DLL
	DECLARE_INTENTION_INTERFACE( CTFRobotDestruction_Robot );
	virtual CRobotLocomotion	*GetLocomotionInterface( void ) const OVERRIDE	{ return m_loco; }
	virtual CRobotBody			*GetBodyInterface( void ) const	OVERRIDE		{ return m_body; }

	virtual void	HandleAnimEvent( animevent_t *pEvent );

	virtual bool	IsRemovedOnReset( void ) const { return false; }
	virtual void	UpdateOnRemove( void );

	virtual void	Event_Killed( const CTakeDamageInfo &info );
	virtual int		OnTakeDamage( const CTakeDamageInfo &info );
	virtual void	TraceAttack( const CTakeDamageInfo &inputInfo, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );

	void			InputStopAndUseComputer( inputdata_t &inputdata );

	void			RepairSelfThink( void );

	void			SetPanicking( bool bSet ) { m_bPanicking = bSet; }
	bool			IsPanicking( void ) const { return m_bPanicking; }

	CPathTrack*		GetGoalPath( void ) const { return m_hGoalPath; }
	void			ArriveAtPath( void );
#else
	virtual int		GetHealth( void ) const OVERRIDE { return m_iHealth; }
	virtual int		GetMaxHealth( void ) const OVERRIDE { return m_iMaxHealth; }

	virtual void	FireGameEvent( IGameEvent *event );

	virtual void	OnDataChanged( DataUpdateType_t type );
	virtual CStudioHdr* OnNewModel();

	virtual void	FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options );
	virtual void	UpdateClientSideAnimation( void );
#endif

private:
	int			m_iHealth;
	int			m_iMaxHealth;
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iHealth );

	CNetworkVar( bool,	m_bShielded );
	CNetworkVar( int,	m_eType );

	CUtlVector<breakmodel_t> m_aGibs;
	CUtlVector<breakmodel_t> m_aSpawnProps;

#ifdef GAME_DLL

	CRobotLocomotion *m_loco;
	CRobotBody		*m_body;

	CHandle<CRobotDispenser> m_hDispenser;
	CHandle<CPathTrack>		m_hGoalPath;

	bool m_bPanicking;
#endif
};

#ifdef GAME_DLL
//--------------------------------------------------------------------------------------------------------------
class CRobotPathCost : public IPathCost
{
public:
	CRobotPathCost( CTFRobotDestruction_Robot *me )
	{
		m_Actor = me;
	}

	virtual float operator()( CNavArea *area, CNavArea *fromArea, const CNavLadder *ladder, const CFuncElevator *elevator, float length ) const OVERRIDE;

private:
	CTFRobotDestruction_Robot *m_Actor;
};
#endif

#endif // ROBOT_DESTRUCTION_ROBOT_H
