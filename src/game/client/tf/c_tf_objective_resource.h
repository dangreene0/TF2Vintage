//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: Entity that propagates general data needed by clients for every player.
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TF_OBJECTIVE_RESOURCE_H
#define C_TF_OBJECTIVE_RESOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_shareddefs.h"
#include "const.h"
#include "c_baseentity.h"
#include <igameresources.h>
#include "c_team_objectiveresource.h"

class C_TFObjectiveResource : public C_BaseTeamObjectiveResource
{
	DECLARE_CLASS( C_TFObjectiveResource, C_BaseTeamObjectiveResource );
public:
	DECLARE_CLIENTCLASS();

					C_TFObjectiveResource();
	virtual			~C_TFObjectiveResource();

	const char		*GetGameSpecificCPCappingSwipe( int index, int iCappingTeam );
	const char		*GetGameSpecificCPBarFG( int index, int iOwningTeam );
	const char		*GetGameSpecificCPBarBG( int index, int iCappingTeam );
	void			SetCappingTeam( int index, int team );

private:
	int m_nMannVsMachineMaxWaveCount;
	int m_nMannVsMachineWaveCount;
	int m_nMannVsMachineWaveEnemyCount;

	int m_nMvMWorldMoney;

	float m_flMannVsMachineNextWaveTime;
	bool m_bMannVsMachineBetweenWaves;

	int m_nMannVsMachineWaveClassCounts[ MVM_CLASS_TYPES_PER_WAVE_MAX ];
	int m_nMannVsMachineWaveClassCounts2[ MVM_CLASS_TYPES_PER_WAVE_MAX ];
	char m_iszMannVsMachineWaveClassNames[ MVM_CLASS_TYPES_PER_WAVE_MAX ][64];
	char m_iszMannVsMachineWaveClassNames2[ MVM_CLASS_TYPES_PER_WAVE_MAX ][64];
	unsigned int m_nMannVsMachineWaveClassFlags[ MVM_CLASS_TYPES_PER_WAVE_MAX ];
	unsigned int m_nMannVsMachineWaveClassFlags2[ MVM_CLASS_TYPES_PER_WAVE_MAX ];
	bool m_bMannVsMachineWaveClassActive[ MVM_CLASS_TYPES_PER_WAVE_MAX ];
	bool m_bMannVsMachineWaveClassActive2[ MVM_CLASS_TYPES_PER_WAVE_MAX ];

	int m_nFlagCarrierUpgradeLevel;
	float m_flMvMBaseBombUpgradeTime;
	float m_flMvMNextBombUpgradeTime;

	int m_iChallengeIndex;
	char m_iszMvMPopfileName[ MAX_PATH ];
	int m_nMvMEventPopfileType;
};

inline C_TFObjectiveResource *TFObjectiveResource()
{
	return static_cast<C_TFObjectiveResource*>(g_pObjectiveResource);
}

#endif // C_TF_OBJECTIVE_RESOURCE_H
