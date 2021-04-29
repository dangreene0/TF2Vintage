#include "cbase.h"
#include "tf_robot_destruction_robot.h"
#include "particle_parse.h"

#ifdef GAME_DLL
	#include "player_vs_environment/tf_robot_destruction_robot_behaviors.h"
#else
	#include "eventlist.h"
#endif

#ifdef GAME_DLL
extern ConVar tf_obj_gib_velocity_min;
extern ConVar tf_obj_gib_velocity_max;
extern ConVar tf_obj_gib_maxspeed;
#endif

ConVar tf_rd_robot_repair_rate( "tf_rd_robot_repair_rate", "60", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRobotLocomotion::CRobotLocomotion( INextBot *bot )
	: NextBotGroundLocomotion( bot )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CRobotLocomotion::GetRunSpeed( void ) const
{
	CTFRobotDestruction_Robot *me = static_cast<CTFRobotDestruction_Robot *>( GetBot()->GetEntity() );
	return me->IsPanicking() ? 150.f : 80.f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CRobotLocomotion::GetGroundSpeed() const
{
	CTFRobotDestruction_Robot *me = static_cast<CTFRobotDestruction_Robot *>( GetBot()->GetEntity() );
	return me->IsPanicking() ? 150.f : 80.f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CRobotLocomotion::ShouldCollideWith( const CBaseEntity *object ) const
{
	return false;
}


CRobotBody::CRobotBody( INextBot *actor )
	: IBody( actor )
{
	m_iMoveX = -1;
	m_iMoveY = -1;

	m_vecLean = vec3_origin;
	m_vecPrevOrigin = vec3_origin;
	m_vecImpulse = vec3_origin;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CRobotBody::Update( void )
{
	CBaseCombatCharacter *actor = GetBot()->GetEntity();
	if ( actor == nullptr )
		return;

	if ( m_iMoveX < 0 )
		m_iMoveX = actor->LookupPoseParameter( "move_x" );
	if ( m_iMoveY < 0 )
		m_iMoveY = actor->LookupPoseParameter( "move_y" );

	Vector vecVelocity = m_vecPrevOrigin - actor->GetAbsOrigin();
	m_vecPrevOrigin = actor->GetAbsOrigin();

	Approach( m_vecLean, vecVelocity + m_vecImpulse, 2.0f );
	Approach( m_vecImpulse, vec3_origin, 200.0f );

	Vector vecFwd, vecRight;
	actor->GetVectors( &vecFwd, &vecRight, nullptr );

	if ( m_iMoveX >= 0 )
		actor->SetPoseParameter( m_iMoveX, vecFwd.Dot( m_vecLean ) );
	if ( m_iMoveY >= 0 )
		actor->SetPoseParameter( m_iMoveY, vecRight.Dot( m_vecLean ) );

	const float flSpeed = GetBot()->GetLocomotionInterface()->GetGroundSpeed();
	if ( actor->m_flGroundSpeed != 0.0f )
		actor->SetPlaybackRate( Clamp( flSpeed / actor->m_flGroundSpeed, -4.0f, 12.0f ) );

	actor->StudioFrameAdvance();
	actor->DispatchAnimEvents( actor );
}

//-----------------------------------------------------------------------------
// Purpose: Begin a specific sequence
//-----------------------------------------------------------------------------
bool CRobotBody::StartActivity( Activity act, unsigned int flags )
{
	CBaseCombatCharacter *actor = GetBot()->GetEntity();

	int iSequence = actor->SelectWeightedSequence( act );
	if ( iSequence )
	{
		m_Activity = act;

		actor->SetSequence( iSequence );
		actor->SetPlaybackRate( 1.0f );
		actor->SetCycle( 0.0f );
		actor->ResetSequenceInfo();

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CRobotBody::Impulse( Vector const& vecImpulse )
{ 
	m_vecImpulse += vecImpulse * 5;
}

//-----------------------------------------------------------------------------
// Purpose: Shift a vector towards the target at the specified rate over time
//-----------------------------------------------------------------------------
void CRobotBody::Approach( Vector& vecIn, Vector const& vecTarget, float flRate )
{
	Vector vecApproach = ( vecTarget - vecIn ) * flRate * gpGlobals->frametime;
	if ( vecApproach.LengthSqr() > ( vecIn - vecTarget ).LengthSqr() )
		vecIn = vecTarget;
	else
		vecIn += vecApproach;		
}
#endif


BEGIN_DATADESC( CRobotDispenser )
END_DATADESC()

IMPLEMENT_NETWORKCLASS_ALIASED( RobotDispenser, DT_RobotDispenser )
BEGIN_NETWORK_TABLE( CRobotDispenser, DT_RobotDispenser  )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( rd_robot_dispenser, CRobotDispenser );

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRobotDispenser::CRobotDispenser()
{
	m_bPlayRefillSound = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CRobotDispenser::Spawn( void )
{
	m_fObjectFlags |= OF_DOESNT_HAVE_A_MODEL;
	m_takedamage = DAMAGE_NO;
	m_iUpgradeLevel = 1;
}

//-----------------------------------------------------------------------------
// Purpose: Finished building
//-----------------------------------------------------------------------------
void CRobotDispenser::OnGoActive( void )
{
	BaseClass::OnGoActive();

	if ( m_hTouchTrigger )
	{
		m_hTouchTrigger->SetParent( GetParent() );
	}

	SetModel( "" ); 
}

//-----------------------------------------------------------------------------
// Purpose: Skip base class function call
//-----------------------------------------------------------------------------
void CRobotDispenser::SetModel( const char *pModel )
{
	CBaseObject::SetModel( pModel );
}

#endif

IMPLEMENT_NETWORKCLASS_ALIASED( TFRobotDestruction_Robot, DT_TFRobotDestruction_Robot )

BEGIN_NETWORK_TABLE( CTFRobotDestruction_Robot, DT_TFRobotDestruction_Robot  )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO(m_iHealth) ),
	RecvPropInt( RECVINFO(m_iMaxHealth) ),
	RecvPropInt( RECVINFO(m_eType) ),
#else
	SendPropInt( SENDINFO(m_iHealth), -1, SPROP_VARINT ),
	SendPropInt( SENDINFO(m_iMaxHealth), -1, SPROP_VARINT|SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_eType), -1, SPROP_VARINT|SPROP_UNSIGNED ),
#endif
END_NETWORK_TABLE()

BEGIN_DATADESC( CTFRobotDestruction_Robot )
#ifdef GAME_DLL
	DEFINE_INPUTFUNC( FIELD_VOID, "StopAndUseComputer", InputStopAndUseComputer ),
#endif
END_DATADESC()

LINK_ENTITY_TO_CLASS( tf_robot_destruction_robot, CTFRobotDestruction_Robot );

CTFRobotDestruction_Robot::CTFRobotDestruction_Robot( void )
{
#ifdef GAME_DLL
	ALLOCATE_INTENTION_INTERFACE( CTFRobotDestruction_Robot );
	m_loco = new CRobotLocomotion( this );
	m_body = new CRobotBody( this );
#else
	ListenForGameEvent( "rd_robot_impact" );
#endif
}

CTFRobotDestruction_Robot::~CTFRobotDestruction_Robot( void )
{
#ifdef GAME_DLL
	if ( m_intention )
		delete m_intention;
	if ( m_loco )
		delete m_loco;
	if ( m_body )
		delete m_body;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_Robot::Precache()
{
	BaseClass::Precache();
	
	PrecacheParticleSystem( "rd_robot_explosion" );
	PrecacheParticleSystem( "bot_radio_waves" );
	PrecacheParticleSystem( "sentrydamage_4" );

	PrecacheScriptSound( "RD.BotDeathExplosion" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_Robot::Spawn()
{
	BuildGibList( m_aGibs, GetModelIndex(), 1.0f, COLLISION_GROUP_NONE );
	BuildPropList( "spawn", m_aSpawnProps, GetModelIndex(), 1.0f, COLLISION_GROUP_NONE );

	BaseClass::Spawn();

	SetSolid( SOLID_BBOX );
	m_takedamage = DAMAGE_YES;
	m_nSkin = ( GetTeamNumber() == TF_TEAM_RED ) ? 0 : 1;

#ifdef GAME_DLL
	//m_hGoalPath = dynamic_cast<CPathTrack *>( gEntList.FindEntityByName( NULL, "" ) );
	if ( !m_hGoalPath )
	{
		UTIL_Remove( this );
	}

	/*if ( CTFRobotDestructionLogic::GetRobotDestructionLogic() )
		CTFRobotDestructionLogic::GetRobotDestructionLogic()->RobotCreated( this );*/

	m_hDispenser = dynamic_cast<CRobotDispenser *>( CreateEntityByName( "rd_robot_dispenser" ) );
	m_hDispenser->SetParent( this );
	m_hDispenser->Spawn();
	m_hDispenser->ChangeTeam( GetTeamNumber() );
	m_hDispenser->OnGoActive();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Dont collide with players
//-----------------------------------------------------------------------------
bool CTFRobotDestruction_Robot::ShouldCollide( int collisionGroup, int contentsMask ) const
{
	if ( collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT )
	{
		return false;
	}

	return BaseClass::ShouldCollide( collisionGroup, contentsMask );
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_Robot::HandleAnimEvent( animevent_t *pEvent )
{
}

//-----------------------------------------------------------------------------
// Purpose: Tell the game logic we're gone
//-----------------------------------------------------------------------------
void CTFRobotDestruction_Robot::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();

	/*if ( CTFRobotDestructionLogic::GetRobotDestructionLogic() )
		CTFRobotDestructionLogic::GetRobotDestructionLogic()->RobotRemoved( this );*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_Robot::Event_Killed( const CTakeDamageInfo &info )
{
	// Let the game logic know that we died
	/*if ( CTFRobotDestructionLogic::GetRobotDestructionLogic() )
		CTFRobotDestructionLogic::GetRobotDestructionLogic()->RobotRemoved( this );*/

	CTFPlayer *pScorer = ToTFPlayer( TFGameRules()->GetDeathScorer( info.GetAttacker(), info.GetInflictor(), this ) );
	if ( pScorer )
	{
		CTFPlayer *pAssister = NULL;

		CTFPlayer *pHealer = ToTFPlayer( pScorer->m_Shared.GetFirstHealer() );
		if ( pHealer && pHealer->Weapon_OwnsThisID( TF_WEAPON_MEDIGUN ) )
		{
			pAssister = pHealer;
		}

		int iWeaponID;
		const char *killer_weapon_name = TFGameRules()->GetKillingWeaponName( info, NULL, iWeaponID );
		const char *killer_weapon_log_name = killer_weapon_name;

		CTFWeaponBase *pWeapon = pScorer->Weapon_OwnsThisID( iWeaponID );
		if ( pWeapon )
		{
			CEconItemDefinition *pItemDef = pWeapon->GetItem()->GetStaticData();
			if ( pItemDef )
			{
				if ( pItemDef->GetIconName() )
					killer_weapon_name = pItemDef->GetIconName();

				if ( pItemDef->GetLogName() )
					killer_weapon_log_name = pItemDef->GetLogName();
			}
		}

		IGameEvent *event = gameeventmanager->CreateEvent( "rd_robot_killed" );
		if ( event )
		{
			if ( pAssister && ( pAssister != pScorer ) )
			{
				event->SetInt( "assister", pAssister->GetUserID() );
			}

			event->SetInt( "attacker", pScorer->GetUserID() );
			event->SetString( "weapon", killer_weapon_name );
			event->SetString( "weapon_logclassname", killer_weapon_log_name );
			event->SetInt( "weaponid", iWeaponID );
			event->SetInt( "priority", 6 );		// HLTV priority

			gameeventmanager->FireEvent( event );
		}
	}

	CBaseAnimating::Event_Killed( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFRobotDestruction_Robot::OnTakeDamage( const CTakeDamageInfo &info )
{
	// Check teams
	if ( info.GetAttacker() )
	{
		if ( InSameTeam( info.GetAttacker() ) )
			return 0;

		CBasePlayer *pAttacker = ToBasePlayer( info.GetAttacker() );
		if ( pAttacker )
		{
			pAttacker->SetLastObjectiveTime( gpGlobals->curtime );
		}
	}

	SetContextThink( &CTFRobotDestruction_Robot::RepairSelfThink, gpGlobals->curtime + 5.0f, "RepairSelfThink" );

	if ( m_bShielded )
		return 0;

	CTakeDamageInfo newInfo{info};

	Vector vecDamagePos = newInfo.GetDamagePosition();
	QAngle vecDamageAngles;
	VectorAngles( -newInfo.GetDamageForce(), vecDamageAngles );
	if ( vecDamagePos == vec3_origin )
	{
		vecDamagePos = WorldSpaceCenter();
	}

	// Play a spark effect
	DispatchParticleEffect( "rd_bot_impact_sparks", vecDamagePos, vecDamageAngles );

	Vector vecImpulse( newInfo.GetDamageForce() );
	GetBodyInterface()->Impulse( vecImpulse.Normalized() * 20.f );

	IGameEvent *event = gameeventmanager->CreateEvent( "rd_robot_impact" );
	if ( event )
	{
		event->SetInt( "entindex", entindex() );
		event->SetInt( "impulse_x", vecImpulse.x );
		event->SetInt( "impulse_y", vecImpulse.y );
		event->SetInt( "impulse_z", vecImpulse.z );

		gameeventmanager->FireEvent( event );
	}

	int nResult = BaseClass::OnTakeDamage( newInfo );

	/*if ( CTFRobotDestructionLogic::GetRobotDestructionLogic() )
		CTFRobotDestructionLogic::GetRobotDestructionLogic()->RobotAttacked( this );*/

	return nResult;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_Robot::TraceAttack( const CTakeDamageInfo &inputInfo, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	// Prevent team damage here so blood doesn't appear
	if ( inputInfo.GetAttacker() && InSameTeam( inputInfo.GetAttacker() ) )
		return;

	AddMultiDamage( inputInfo, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_Robot::InputStopAndUseComputer( inputdata_t &inputdata )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_Robot::ArriveAtPath( void )
{
	variant_t null;
	m_hGoalPath->AcceptInput( "InPass", this, this, null, 0 );
	m_hGoalPath = m_hGoalPath->GetNext();
}

//-----------------------------------------------------------------------------
// Purpose: Repair ourselves!
//-----------------------------------------------------------------------------
void CTFRobotDestruction_Robot::RepairSelfThink()
{
	int nHealth = GetHealth();
	if ( tf_rd_robot_repair_rate.GetFloat() != 0.0f )
	{
		nHealth += GetMaxHealth() / tf_rd_robot_repair_rate.GetFloat();
	}

	nHealth = Min( nHealth, GetMaxHealth() );
	SetHealth( nHealth );

	// Continue to heal if we're still hurt
	if ( GetHealth() != GetMaxHealth() )
	{
		SetContextThink( &CTFRobotDestruction_Robot::RepairSelfThink, gpGlobals->curtime + 1.0f, "RepairSelfThink" );
	}
}
#else

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_Robot::FireGameEvent( IGameEvent *event )
{
	const char *pszName = event->GetName();
	if ( FStrEq( pszName, "rd_robot_impact" ) )
	{
		const int index_ = event->GetInt( "entindex" );
		if ( index_ == entindex() )
		{
			Vector vecImpulse( event->GetFloat( "impulse_x" ), event->GetFloat( "impulse_y" ), event->GetFloat( "impulse_z" ) );
			// TODO: Animation control
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_Robot::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	//UpdateDamagedEffects();
}

CStudioHdr *CTFRobotDestruction_Robot::OnNewModel()
{
	CStudioHdr *hdr = BaseClass::OnNewModel();
	BuildPropList( "spawn", m_aSpawnProps, GetModelIndex(), 1.0f, COLLISION_GROUP_NONE );

	return hdr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_Robot::FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options )
{
	if ( event == AE_RD_ROBOT_POP_PANELS_OFF )
	{
		CUtlVector<breakmodel_t> vecProp;
		FOR_EACH_VEC( m_aSpawnProps, i )
		{
			char szLowerName[MAX_PATH]{0};
			Q_snprintf( szLowerName, sizeof( szLowerName ), "%s", options );
			Q_strlower( szLowerName );
			if ( Q_strstr( m_aSpawnProps[i].modelName, szLowerName ) )
			{
				vecProp.AddToTail( m_aSpawnProps[i] );
			}
		}

		if ( vecProp.Count() )
		{
			Vector vForward, vRight, vUp;
			AngleVectors( GetAbsAngles(), &vForward, &vRight, &vUp );

			Vector vecBreakVelocity = Vector(0, 0, 200);
			AngularImpulse angularImpulse( RandomFloat( 0.0f, 120.0f ), RandomFloat( 0.0f, 120.0f ), 0.0 );
			Vector vecOrigin = GetAbsOrigin() + vForward*70 + vUp*10;
			QAngle vecAngles = GetAbsAngles();
			breakablepropparams_t breakParams( vecOrigin, vecAngles, vecBreakVelocity, angularImpulse );
			breakParams.impactEnergyScale = 1.0f;
			breakParams.defBurstScale = 3.0f;
			int nModelIndex = GetModelIndex();

			CreateGibsFromList( vecProp, nModelIndex, NULL, breakParams, this, -1 , false, true );
		}
	}

	BaseClass::FireEvent( origin, angles, event, options );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRobotDestruction_Robot::UpdateClientSideAnimation( void )
{
	BaseClass::UpdateClientSideAnimation();
}

#endif

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CRobotPathCost::operator()( CNavArea *area, CNavArea *fromArea, const CNavLadder *ladder, const CFuncElevator *elevator, float length ) const
{
	if ( fromArea == nullptr )
	{
		// first area in path; zero cost
		return 0.0f;
	}

	if ( !m_Actor->GetLocomotionInterface()->IsAreaTraversable( area ) )
	{
		// dead end
		return -1.0f;
	}

	if ( ladder != nullptr )
		length = ladder->m_length;
	else if ( length <= 0.0f )
		length = ( area->GetCenter() - fromArea->GetCenter() ).Length();

	const float dz = fromArea->ComputeAdjacentConnectionHeightChange( area );
	if ( dz >= m_Actor->GetLocomotionInterface()->GetStepHeight() )
	{
		if ( dz >= m_Actor->GetLocomotionInterface()->GetMaxJumpHeight() )
			return -1.0f;

		// we won't actually get here according to the locomotor
		length *= 5;
	}
	else
	{
		if ( dz < -m_Actor->GetLocomotionInterface()->GetDeathDropHeight() )
			return -1.0f;
	}

	return length + fromArea->GetCostSoFar();
}

IMPLEMENT_INTENTION_INTERFACE( CTFRobotDestruction_Robot, CRobotBehavior );

#endif