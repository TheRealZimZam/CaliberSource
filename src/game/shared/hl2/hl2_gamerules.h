//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Game rules for Caliber.
//
// Instead of having to (YUCK!) launch a completely seperate client just to play MP,
// its all gonna be rolled up in one package like any other normal game. To do this
// in source, we need some ((very)) powerful gamerule code that can dynamically change
// from SP to MP on the fly, and support multiple gamemodes
// To do this, we inherit from multiplayer, and patch the singleplayer ONTOP of that.
//=============================================================================//

#ifndef HL2_GAMERULES_H
#define HL2_GAMERULES_H
#ifdef _WIN32
#pragma once
#endif

#include "gamerules.h"
#include "singleplay_gamerules.h"
#include "teamplay_gamerules.h"
#include "hl2_shareddefs.h"

#ifdef CLIENT_DLL
	#define CHalfLife2 C_HalfLife2
	#define CHalfLife2Proxy C_HalfLife2Proxy
#endif

extern ConVar default_fov;

enum
{
	//! - Already defined in shareddef
//!	TEAM_UNASSIGNED,
//!	TEAM_SPECTATOR,
	TEAM_COMBINE = 2,
	TEAM_REBELS,
	TEAM_PMC,
};

enum GameMode_t
{
	//Gamemodes, deathmatch/ffa is default
	SINGLEPLAYER,
	COOP,
	DEATHMATCH,
	TDM,
	OBJECTIVE,
	FLAG,
	ARENA,
	LASTMAN,
	RACE,
};

enum GameModifiers_t
{
	//Modifiers (big head, always gib, etc.)
	BIGHEADS,
	BLOODYMESS,
	NOAMMO,
	NOGUNS,
	SHOTGUNSONLY,
	RAILGUNSONLY,
	TRIPLEDAMAGE,
};

// When restarting a round, dont touch these entities
static const char *s_PreserveEnts[] =
{
	"ai_network",
	"ai_hint",
	"hl2_gamerules",
	"team_manager",
	"player_manager",
	"env_soundscape",
	"env_soundscape_proxy",
	"env_soundscape_triggerable",
	"env_sun",
	"env_wind",
	"env_fog_controller",
	"func_brush",
	"func_wall",
	"func_buyzone",
	"func_illusionary",
	"infodecal",
	"info_projecteddecal",
	"info_node",
	"info_target",
	"info_node_hint",
	"info_player_start",
	"info_player_deathmatch",
	"info_player_combine",
	"info_player_rebel",
	"info_map_parameters",
	"keyframe_rope",
	"move_rope",
	"info_ladder",
	"player",
	"point_viewcontrol",
	"scene_manager",
	"shadow_control",
	"sky_camera",
	"soundent",
	"trigger_soundscape",
	"viewmodel",
	"predicted_viewmodel",
	"worldspawn",
	"point_devshot_camera",
	"", // END Marker
};

class CHalfLife2Proxy : public CGameRulesProxy
{
public:
	DECLARE_CLASS( CHalfLife2Proxy, CGameRulesProxy );
	DECLARE_NETWORKCLASS();

#ifdef GAME_DLL
	DECLARE_DATADESC();
	virtual void Activate();
#endif
};

class CHalfLife2 : public CTeamplayRules
{
public:
	DECLARE_CLASS( CHalfLife2, CTeamplayRules );

	CHalfLife2();
	virtual ~CHalfLife2();

	virtual void	Activate();

	virtual const char*	GetGameDescription( void );
	virtual const char*	GetGameTypeName( void ){ return GetGameDescription(); }
	virtual int GetGameType( void ){ return m_iGameMode; }
	virtual void SetGameType( int GameMode ){ m_iGameMode = GameMode; }

	// Bonus levels can be considered CoOp, enable info_player_coop if the level requests it
	virtual bool			IsMultiplayer( void );
	virtual bool			IsTeamplay( void );
	virtual bool			IsDeathmatch( void );	//Also known as FFA
	virtual bool 			IsCoOp( void );

	// Damage Query Overrides.
	virtual bool			Damage_IsTimeBased( int iDmgType );
	virtual int				Damage_GetTimeBased( void );

	virtual bool			ShouldCollide( int collisionGroup0, int collisionGroup1 );
	virtual bool			ShouldUseRobustRadiusDamage(CBaseEntity *pEntity);
#ifndef CLIENT_DLL
	virtual bool			AllowAutoTargetCrosshair( void );
	virtual int				GetAutoAimMode(){ return sk_autoaim_mode.GetInt(); }
	virtual bool			ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target );
	virtual float			GetAutoAimScale( CBasePlayer *pPlayer );
	virtual float			GetAmmoQuantityScale( int iAmmoIndex );
	virtual void			LevelInitPreEntity();
#endif

	// MP overrides
	bool FAllowNPCs( void );
	int WeaponShouldRespawn( CBaseCombatWeapon *pWeapon );
	int ItemShouldRespawn( CItem *pItem );
	float FlWeaponRespawnTime( CBaseCombatWeapon *pWeapon );
	float FlItemRespawnTime( CItem *pItem );

private:
	CNetworkVar( int, m_iGameMode );
	CNetworkVar( bool, m_bTeamPlayEnabled );
	// Rules change for the mega physgun
	CNetworkVar( bool, m_bMegaPhysgun );

	int DefaultFOV( void ) { return 85; }

#ifdef CLIENT_DLL
	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.

	const char *GetVideoFileForMap( bool bWithExtension = true );
#else
	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.
#endif

#ifndef CLIENT_DLL
	virtual void			Think( void );

	virtual bool			ClientCommand( CBaseEntity *pEdict, const CCommand &args );
	virtual void			PlayerSpawn( CBasePlayer *pPlayer );
	virtual void			PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual bool			FPlayerCanRespawn( CBasePlayer *pPlayer );
	virtual float			FlPlayerSpawnTime( CBasePlayer *pPlayer );

	//!virtual void	SendHudNotification( IRecipientFilter &filter, HudNotification_t iType );
	virtual void	SendHudNotification( IRecipientFilter &filter, const char *pszText, const char *pszIcon, int iTeam = TEAM_UNASSIGNED );

	virtual void			InitDefaultAIRelationships( void );
	virtual const char*		AIClassText(int classType);

	// Ammo
	virtual void			PlayerThink( CBasePlayer *pPlayer );
//	virtual bool			CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );
	virtual float			GetAmmoDamage( CBaseEntity *pAttacker, CBaseEntity *pVictim, int nAmmoType );

	virtual bool			ShouldBurningPropsEmitLight();
public:
	bool AllowDamage( CBaseEntity *pVictim, const CTakeDamageInfo &info );
	bool	NPC_ShouldDropGrenade( CBasePlayer *pRecipient );
	bool	NPC_ShouldDropHealth( CBasePlayer *pRecipient );
	void	NPC_DroppedHealth( void );
	void	NPC_DroppedGrenade( void );
	bool	MegaPhyscannonActive( void ) { return m_bMegaPhysgun;	}

	virtual bool IsAlyxInDarknessMode();

private:

	float	m_flLastHealthDropTime;
	float	m_flLastGrenadeDropTime;

	void AdjustPlayerDamageTaken( CTakeDamageInfo *pInfo );
	float AdjustPlayerDamageInflicted( float damage );
#endif

};


//-----------------------------------------------------------------------------
// Gets us at the Half-Life 2 game rules
//-----------------------------------------------------------------------------
inline CHalfLife2* HL2GameRules()
{
	return static_cast<CHalfLife2*>(g_pGameRules);
}



#endif // HL2_GAMERULES_H
