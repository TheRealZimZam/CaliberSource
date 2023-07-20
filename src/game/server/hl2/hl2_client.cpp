//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: HL2 client/server game specific stuff
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "player.h"
#include "game.h"
#include "hl2_player.h"
#include "hl2_gamerules.h"
#include "gamerules.h"
#include "EntityList.h"
#include "physics.h"
#include "player_resource.h"
#include "engine/IEngineSound.h"
#include "filesystem.h"

#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void Host_Say( edict_t *pEdict, bool teamonly );

extern CBaseEntity*	FindPickerEntityClass( CBasePlayer *pPlayer, char *classname );
extern bool			g_fGameOver;

/*
===========
ClientPutInServer

called each time a player is spawned into the game
============
*/
void ClientPutInServer( edict_t *pEdict, const char *playername )
{
	// Allocate a CBasePlayer for pev, and call spawn
	CHL2_Player *pPlayer = CHL2_Player::CreatePlayer( "player", pEdict );
	pPlayer->SetPlayerName( playername );
}


void ClientActive( edict_t *pEdict, bool bLoadGame )
{
	CHL2_Player *pPlayer = dynamic_cast< CHL2_Player* >( CBaseEntity::Instance( pEdict ) );
	Assert( pPlayer );

	if ( !pPlayer )
	{
		return;
	}

	pPlayer->InitialSpawn();

	if ( !bLoadGame )
	{
		pPlayer->Spawn();
	}
}


/*
===============
const char *GetGameDescription()

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
const char *GetGameDescription()
{
	if ( g_pGameRules ) // this function may be called before the world has spawned, and the game rules initialized
		return g_pGameRules->GetGameDescription();
	else
		return "Half-Life 2";
}

//-----------------------------------------------------------------------------
// Purpose: Given a player and optional name returns the entity of that 
//			classname that the player is nearest facing
//			
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseEntity* FindEntity( edict_t *pEdict, char *classname)
{
	// If no name was given set bits based on the picked
	if (FStrEq(classname,"")) 
	{
		return (FindPickerEntityClass( static_cast<CBasePlayer*>(GetContainingEntity(pEdict)), classname ));
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Precache game-specific models & sounds
//-----------------------------------------------------------------------------
void ClientGamePrecache( void )
{
	const char *pFilename = "scripts/client_precache.txt";
	KeyValues *pValues = new KeyValues( "ClientPrecache" );

	if ( !pValues->LoadFromFile( filesystem, pFilename, "GAME" ) )
	{
		Error( "Can't open %s for client precache info.", pFilename );

		// Do the default list
		CBaseEntity::PrecacheModel("sprites/hud1.vmt");
		CBaseEntity::PrecacheModel("models/player.mdl");
		CBaseEntity::PrecacheModel("models/gibs/agibs.mdl");
		CBaseEntity::PrecacheModel("models/weapons/v_hands.mdl");

		CBaseEntity::PrecacheScriptSound( "HUDQuickInfo.LowAmmo" );
		CBaseEntity::PrecacheScriptSound( "HUDQuickInfo.LowHealth" );

		CBaseEntity::PrecacheScriptSound( "FX_AntlionImpact.ShellImpact" );	//Why??
		CBaseEntity::PrecacheScriptSound( "Missile.ShotDown" );
		CBaseEntity::PrecacheScriptSound( "Bullets.SmallNearmiss" );
		CBaseEntity::PrecacheScriptSound( "Bullets.MediumNearmiss" );
		CBaseEntity::PrecacheScriptSound( "Bullets.LargeNearmiss" );
		
		CBaseEntity::PrecacheScriptSound( "Geiger.BeepHigh" );
		CBaseEntity::PrecacheScriptSound( "Geiger.BeepLow" );
		
		pValues->deleteThis();
		return;
	}

	for ( KeyValues *pData = pValues->GetFirstSubKey(); pData != NULL; pData = pData->GetNextKey() )
	{
		const char *pszType = pData->GetName();
		const char *pszFile = pData->GetString();

		if ( Q_strlen( pszType ) > 0 &&
			 Q_strlen( pszFile ) > 0 )
		{
			if ( !Q_stricmp( pData->GetName(), "model" ) )
			{
				CBaseEntity::PrecacheModel( pszFile );
			}
			else if ( !Q_stricmp( pData->GetName(), "scriptsound" ) )
			{
				CBaseEntity::PrecacheScriptSound( pszFile );
			}
		}
	}

	pValues->deleteThis();
}


// called by ClientKill and DeadThink
void respawn( CBaseEntity *pEdict, bool fCopyCorpse )
{
	if (gpGlobals->coop || gpGlobals->deathmatch || teamplay.GetInt() > 0)
	{
		if ( fCopyCorpse )
		{
			// make a copy of the dead body for appearances sake
			((CHL2_Player *)pEdict)->CreateCorpse();
		}

		// respawn player
		pEdict->Spawn();
	}
	else
	{
		// restart the entire server
		engine->ServerCommand("reload\n");
	}
}

void GameStartFrame( void )
{
	VPROF("GameStartFrame()");

	if ( g_pGameRules )
		g_pGameRules->Think();

	if ( g_fGameOver )
		return;

	gpGlobals->teamplay = (teamplay.GetInt() != 0);

#ifdef DEBUG
	extern void Bot_RunAll();
	Bot_RunAll();
#endif
}

#ifdef HL2_DLL
extern ConVar gamerules_survival;
#endif

//=========================================================
// instantiate the proper game rules object
//=========================================================
void InstallGameRules()
{
#ifdef HL2_DLL
	if ( gamerules_survival.GetBool() )
		// Survival mode
		CreateGameRulesObject( "CHalfLife2Survival" );
	else
		// Caliber All-in-one
		CreateGameRulesObject( "CHalfLife2" );
#else
	{
		if ( teamplay.GetInt() > 0 )
		{
			// teamplay
			CreateGameRulesObject( "CTeamplayRules" );
		}
		else if (gpGlobals->deathmatch == 1)
		{
			// vanilla deathmatch
			teamplay.SetValue( 0 );
			CreateGameRulesObject( "CMultiplayRules" );
		}
		else
		{
			// vanilla deathmatch??
			teamplay.SetValue( 0 );
			CreateGameRulesObject( "CMultiplayRules" );
		}
	}
#endif
}

