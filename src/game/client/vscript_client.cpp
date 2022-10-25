//========== Copyright � 2008, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#include "cbase.h"
#include "vscript_client.h"
#include "icommandline.h"
#include "tier1/utlbuffer.h"
#include "tier1/fmtstr.h"
#include "filesystem.h"
#include "characterset.h"
#include "isaverestore.h"
#include "gamerules.h"
#include "vscript_client_nut.h"
#include "matchers.h"
#include "c_world.h"
#include "proxyentity.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "vscript_singletons.h"


extern ScriptClassDesc_t * GetScriptDesc( CBaseEntity * );

static ConVar cl_mapspawn_nut_exec( "cl_mapspawn_nut_exec", "0", FCVAR_NONE, "If set to 1, client will execute scripts/vscripts/mapspawn.nut file" );


//-----------------------------------------------------------------------------
// Purpose: A clientside variant of CScriptEntityIterator.
//-----------------------------------------------------------------------------
class CScriptClientEntityIterator
{
public:
	HSCRIPT GetLocalPlayer()
	{
		return ToHScript( C_BasePlayer::GetLocalPlayer() );
	}

	HSCRIPT First() { return Next( NULL ); }

	HSCRIPT Next( HSCRIPT hStartEntity )
	{
		return ToHScript( ClientEntityList().NextBaseEntity( ToEnt( hStartEntity ) ) );
	}

	HSCRIPT CreateByClassname( const char *className )
	{
		return ToHScript( CreateEntityByName( className ) );
	}

	HSCRIPT FindByClassname( HSCRIPT hStartEntity, const char *szName )
	{
		const CEntInfo *pInfo = hStartEntity ? ClientEntityList().GetEntInfoPtr( ToEnt( hStartEntity )->GetRefEHandle() )->m_pNext : ClientEntityList().FirstEntInfo();
		for ( ; pInfo; pInfo = pInfo->m_pNext )
		{
			C_BaseEntity *ent = (C_BaseEntity *)pInfo->m_pEntity;
			if ( !ent )
				continue;

			if ( Matcher_Match( szName, ent->GetClassname() ) )
				return ToHScript( ent );
		}

		return NULL;
	}

	HSCRIPT FindByName( HSCRIPT hStartEntity, const char *szName )
	{
		const CEntInfo *pInfo = hStartEntity ? ClientEntityList().GetEntInfoPtr( ToEnt( hStartEntity )->GetRefEHandle() )->m_pNext : ClientEntityList().FirstEntInfo();
		for ( ; pInfo; pInfo = pInfo->m_pNext )
		{
			C_BaseEntity *ent = (C_BaseEntity *)pInfo->m_pEntity;
			if ( !ent )
				continue;

			if ( Matcher_Match( szName, ent->GetEntityName() ) )
				return ToHScript( ent );
		}

		return NULL;
	}

private:
} g_ScriptEntityIterator;

BEGIN_SCRIPTDESC_ROOT_NAMED( CScriptClientEntityIterator, "CEntities", SCRIPT_SINGLETON "The global list of entities" )
	DEFINE_SCRIPTFUNC( GetLocalPlayer, "Get local player" )
	DEFINE_SCRIPTFUNC( First, "Begin an iteration over the list of entities" )
	DEFINE_SCRIPTFUNC( Next, "Continue an iteration over the list of entities, providing reference to a previously found entity" )
	DEFINE_SCRIPTFUNC( CreateByClassname, "Creates an entity by classname" )
	DEFINE_SCRIPTFUNC( FindByClassname, "Find entities by class name. Pass 'null' to start an iteration, or reference to a previously found entity to continue a search" )
	DEFINE_SCRIPTFUNC( FindByName, "Find entities by name. Pass 'null' to start an iteration, or reference to a previously found entity to continue a search" )
END_SCRIPTDESC();

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static float Time()
{
	return gpGlobals->curtime;
}

static const char *GetMapName()
{
	return engine->GetLevelName();
}

static const char *DoUniqueString( const char *pszBase )
{
	static char szBuf[512];
	g_pScriptVM->GenerateUniqueKey( pszBase, szBuf, ARRAYSIZE(szBuf) );
	return szBuf;
}

bool DoIncludeScript( const char *pszScript, HSCRIPT hScope )
{
	if ( !VScriptRunScript( pszScript, hScope, true ) )
	{
		g_pScriptVM->RaiseException( CFmtStr( "Failed to include script \"%s\"", ( pszScript ) ? pszScript : "unknown" ) );
		return false;
	}
	return true;
}

static float FrameTime()
{
	return gpGlobals->frametime;
}

static bool Con_IsVisible()
{
	return engine->Con_IsVisible();
}

static bool IsWindowedMode()
{
	return engine->IsWindowedMode();
}

int ScreenTransform( const Vector &point, Vector &screen );

//-----------------------------------------------------------------------------
// Input array [x,y], set normalised screen space pos. Return true if on screen
//-----------------------------------------------------------------------------
static bool ScriptScreenTransform( const Vector &pos, HSCRIPT hArray )
{
	if ( g_pScriptVM->GetNumTableEntries( hArray ) >= 2 )
	{
		Vector v;
		bool r = ScreenTransform( pos, v );
		float x = 0.5f * ( 1.0f + v[0] );
		float y = 0.5f * ( 1.0f - v[1] );

		g_pScriptVM->SetValue( hArray, "0", x);
		g_pScriptVM->SetValue( hArray, "1", y);
		return !r;
	}
	return false;
}

// Creates a client-side prop
HSCRIPT CreateProp( const char *pszEntityName, const Vector &vOrigin, const char *pszModelName, int iAnim )
{
	C_BaseAnimating *pBaseEntity = (C_BaseAnimating *)CreateEntityByName( pszEntityName );
	if ( !pBaseEntity )
		return NULL;

	pBaseEntity->SetAbsOrigin( vOrigin );
	pBaseEntity->SetModelName( pszModelName );
	if ( !pBaseEntity->InitializeAsClientEntity( pszModelName, RENDER_GROUP_OPAQUE_ENTITY ) )
	{
		Warning( "Can't initialize %s as client entity\n", pszEntityName );
		return NULL;
	}

	pBaseEntity->SetPlaybackRate( 1.0f );

	int iSequence = pBaseEntity->SelectWeightedSequence( (Activity)iAnim );

	if ( iSequence != -1 )
	{
		pBaseEntity->SetSequence( iSequence );
	}

	return ToHScript( pBaseEntity );
}

bool VScriptClientInit()
{
	VMPROF_START

	if( scriptmanager != NULL )
	{
		ScriptLanguage_t scriptLanguage = SL_DEFAULT;

		char const *pszScriptLanguage;
		if ( CommandLine()->CheckParm( "-scriptlang", &pszScriptLanguage ) )
		{
			if( !Q_stricmp(pszScriptLanguage, "squirrel") )
			{
				scriptLanguage = SL_SQUIRREL;
			}
			else if( !Q_stricmp(pszScriptLanguage, "lua") )
			{
				scriptLanguage = SL_LUA;
			}
			else
			{
				DevWarning("-scriptlang does not recognize a language named '%s'. virtual machine did NOT start.\n", pszScriptLanguage );
				scriptLanguage = SL_NONE;
			}
		}

		if( scriptLanguage != SL_NONE )
		{
			if ( g_pScriptVM == NULL )
				g_pScriptVM = scriptmanager->CreateVM( scriptLanguage );

			if( g_pScriptVM )
			{
				Log( "VSCRIPT CLIENT: Started VScript virtual machine using script language '%s'\n", g_pScriptVM->GetLanguageName() );

				ScriptRegisterFunction( g_pScriptVM, GetMapName, "Get the name of the map.");
				ScriptRegisterFunction( g_pScriptVM, Time, "Get the current server time" );
				ScriptRegisterFunction( g_pScriptVM, DoUniqueString, SCRIPT_ALIAS( "UniqueString", "Generate a string guaranteed to be unique across the life of the script VM, with an optional root string." ) );
				ScriptRegisterFunction( g_pScriptVM, DoIncludeScript, SCRIPT_ALIAS( "IncludeScript", "Execute a script (internal)" ) );
				ScriptRegisterFunction( g_pScriptVM, FrameTime, "Get the time spent on the client in the last frame" );
				ScriptRegisterFunction( g_pScriptVM, Con_IsVisible, "Returns true if the console is visible" );
				ScriptRegisterFunction( g_pScriptVM, ScreenWidth, "Width of the screen in pixels" );
				ScriptRegisterFunction( g_pScriptVM, ScreenHeight, "Height of the screen in pixels" );
				ScriptRegisterFunction( g_pScriptVM, IsWindowedMode, "" );
				ScriptRegisterFunctionNamed( g_pScriptVM, ScriptScreenTransform, "ScreenTransform", "Get the x & y positions of a world position in screen space. Returns true if it's onscreen" );
				ScriptRegisterFunction( g_pScriptVM, CreateProp, "Create an animating prop" );

				g_pScriptVM->RegisterClass( GetScriptDescForClass( CScriptKeyValues ) );
				
				if ( GameRules() )
				{
					GameRules()->RegisterScriptFunctions();
				}

				g_pScriptVM->RegisterInstance( &g_ScriptEntityIterator, "Entities" );

				RegisterSharedScriptConstants();
				RegisterSharedScriptFunctions();

				if ( scriptLanguage == SL_SQUIRREL )
				{
					g_pScriptVM->Run( g_Script_vscript_client );
				}

				if ( cl_mapspawn_nut_exec.GetBool() )
				{
					VScriptRunScript( "mapspawn", false );
				}

				VMPROF_SHOW( __FUNCTION__, "virtual machine startup" );

				return true;
			}
			else
			{
				DevWarning("VM Did not start!\n");
			}
		}
	}
	else
	{
		Msg( "\nVSCRIPT: Scripting is disabled.\n" );
	}

	g_pScriptVM = NULL;
	return false;
}

void VScriptClientTerm()
{
	if( scriptmanager != NULL )
	{
		if( g_pScriptVM )
		{
			scriptmanager->DestroyVM( g_pScriptVM );
			g_pScriptVM = NULL;
		}
	}
}


class CVScriptGameSystem : public CAutoGameSystemPerFrame
{
public:
	// Inherited from IAutoServerSystem
	virtual void LevelInitPreEntity( void )
	{
		m_bAllowEntityCreationInScripts = true;
		VScriptClientInit();
	}

	virtual void LevelInitPostEntity( void )
	{
		m_bAllowEntityCreationInScripts = false;
	}

	virtual void LevelShutdownPostEntity( void )
	{
		VScriptClientTerm();
	}

	virtual void FrameUpdatePostEntityThink() 
	{ 
		if ( g_pScriptVM )
			g_pScriptVM->Frame( gpGlobals->frametime );
	}

	bool m_bAllowEntityCreationInScripts;
};

CVScriptGameSystem g_VScriptGameSystem;

bool IsEntityCreationAllowedInScripts( void )
{
	return g_VScriptGameSystem.m_bAllowEntityCreationInScripts;
}


