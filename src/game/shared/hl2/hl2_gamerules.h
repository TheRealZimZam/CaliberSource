//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Game rules for Caliber.
//
// Instead of having to (YUCK!) launch a completely seperate client just to play MP,
// its all gonna be rolled up in one package like any other normal game. To do this
// in source, we need some ((very)) powerful gamerule code that can dynamically change
// from SP to MP on the fly, and support multiple gamemodes
// To do this, we inherit from multiplayer, and patch the singleplayer ONTOP of that.
//
// NOTENOTE; Usually you would inherit from roundbased_gamerules.h, but we
// arent doing that here. For caliber, after a match ends, the server restarts completely.
// This is the old way of doing it, which shouldnt break compatibility with non-mp, or older maps.
// It also has the plus of keeping performance up, and not having to create a bunch of entities
// constantly. 
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
#define MAX_GAME_MESSAGE_LENGTH	64	// Max length of a game message (start, end, lts, etc.), smaller the better

enum
{
	//! - Already defined in shareddef
//!	TEAM_UNASSIGNED,
//!	TEAM_SPECTATOR,
	TEAM_COMBINE = 2,	//Attack
	TEAM_REBELS,		//Defence
	TEAM_PMC,			//For 3rd parties, ie. arena
};

enum GameModifiers_t
{
	//Modifiers (big head, always gib, etc.)
	// TODO; These need to be rethought - right now, only one modifier can be active at a time
	NONE = 0,
	BIGHEADS,	//Walkin talkin bobbleheads
	BLOODYMESS,	//Always gib!
	NOAMMO,	//Ammo pickups replaced with health
	NOHEALTH,	//Health pickups replaced with ammo
	NOGUNS,	//This is a gun-free area!
	SHOTGUNSONLY,
	RAILGUNSONLY,
	PISTOLSONLY,
	TRIPLEDAMAGE,	//Pair this with skill4 and its insta-death
};

enum Holiday_t
{
	//Holidays to celebrate
	NORMAL = 0,
	CHRISTMAS,
	HALLOWEEN,
	INDEPENDENCE,
	VETERANS,
	FATHERS,
	MOTHERS,
	EASTER,
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

//	virtual bool InIntermission(){ return m_flIntermissionEndTime > gpGlobals->curtime; }

	// Get the view vectors for this mod.
	virtual const CViewVectors *GetViewVectors() const;

	// Damage Query Overrides.
	virtual bool			Damage_IsTimeBased( int iDmgType );
	virtual int				Damage_GetTimeBased( void );

	virtual bool			ShouldCollide( int collisionGroup0, int collisionGroup1 );
	virtual bool			ShouldUseRobustRadiusDamage(CBaseEntity *pEntity);
#ifdef GAME_DLL
	// Allow servers to set custom msgs
	//virtual void SetStartMessage( const char *sMsg ){ Q_strncpy(m_pszStartMessage.GetForModify(), sMsg, sizeof(m_pszStartMessage)); }
	//virtual void SetCountdownMessage( const char *sMsg ){ Q_strncpy(m_pszCountdownMessage.GetForModify(), sMsg, sizeof(m_pszCountdownMessage)); }
	//virtual void SetEndMessage( const char *sMsg ){ Q_strncpy(m_pszEndMessage.GetForModify(), sMsg, sizeof(m_pszEndMessage)); }

	virtual int GetGameModifier( void ){ return m_iGameModifier; }
	virtual void SetGameModifier( int GameModifier ){ m_iGameModifier = GameModifier; }

	virtual int GetHoliday( void ){ return m_iHoliday; }

	virtual bool			AllowAutoTargetCrosshair( void );
	virtual int				GetAutoAimMode(){ return sk_autoaim_mode.GetInt(); }
	virtual bool			ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target );
	virtual float			GetAutoAimScale( CBasePlayer *pPlayer );
	virtual float			GetAmmoQuantityScale( int iAmmoIndex );
#endif

	// Declaring these on the client for posterity
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

//	CNetworkString( m_pszStartMessage, MAX_GAME_MESSAGE_LENGTH );		// Message to display on game start
//	CNetworkString( m_pszCountdownMessage, MAX_GAME_MESSAGE_LENGTH );	// "Hurry up" Message for obj
//	CNetworkString( m_pszEndMessage, MAX_GAME_MESSAGE_LENGTH );			// Message that displays on game end

	int DefaultFOV( void ) { return 80; }

public:
#ifdef CLIENT_DLL
	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.

	const char *GetVideoFileForMap( bool bWithExtension = true );
#else
	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.
#endif

#ifdef GAME_DLL
	virtual void			Think( void );
	virtual void			LevelInitPreEntity();

	virtual bool			ClientCommand( CBaseEntity *pEdict, const CCommand &args );
	virtual void			PlayerThink( CBasePlayer *pPlayer );
	virtual void			PlayerSpawn( CBasePlayer *pPlayer );
	virtual void			PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );

	// Respawn everyone regardless of state - for debugging/gamemodes
	inline void RespawnAllPlayers( void ) { RespawnPlayers( true ); }
	void RespawnPlayers( bool bForceRespawn, bool bTeam = false, int iTeam = TEAM_UNASSIGNED );

//	virtual void			SendHudNotification( IRecipientFilter &filter, HudNotification_t iType );
	virtual void			SendHudNotification( IRecipientFilter &filter, const char *pszText, const char *pszIcon, int iTeam = TEAM_UNASSIGNED );

	virtual void			InitDefaultAIRelationships( void );
	virtual const char*		AIClassText(int classType);

	// Ammo
//	virtual bool			CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );
	virtual float			GetAmmoDamage( CBaseEntity *pAttacker, CBaseEntity *pVictim, int nAmmoType );

public:
	bool	AllowDamage( CBaseEntity *pVictim, const CTakeDamageInfo &info );
	bool	NPC_ShouldDropGrenade( CBasePlayer *pRecipient );
	bool	NPC_ShouldDropHealth( CBasePlayer *pRecipient );
	void	NPC_DroppedHealth( void );
	void	NPC_DroppedGrenade( void );
	bool	MegaPhyscannonActive( void ) { return m_bMegaPhysgun;	}

	virtual bool	FPlayerCanRespawn( CBasePlayer *pPlayer );
	virtual float	FlPlayerSpawnTime( CBasePlayer *pPlayer );
	virtual bool	PlayerHasPowerplay( CBasePlayer *pPlayer );

	virtual bool	IsAlyxInDarknessMode();
	virtual bool	ShouldBurningPropsEmitLight();
	
	// Killed in a funny way (flaregun, pushing, etc.)
//	virtual void	PlayerHumiliated( CBasePlayer *pPlayer );
	// Dominating this guy
//	virtual void	PlayerDominated( CBasePlayer *pVictim, CBasePlayer *pKiller );

private:
	float	m_flLastHealthDropTime;
	float	m_flLastGrenadeDropTime;

	int		m_iGameModifier;
	int		m_iHoliday;	//What holiday is it today? Internally set, use GetHoliday().

	bool	m_bHumiliation;	//Somebody just got humiliated, point and laugh

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
