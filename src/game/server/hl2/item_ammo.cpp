//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: The various ammo types for HL2	
//
//=============================================================================//

#include "cbase.h"
#include "player.h"
#include "gamerules.h"
#include "items.h"
#include "ammodef.h"
#include "eventlist.h"
#include "npcevent.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//---------------------------------------------------------
// Applies ammo quantity scale.
//---------------------------------------------------------
int ITEM_GiveAmmo( CBasePlayer *pPlayer, float flCount, const char *pszAmmoName, bool bSuppressSound = true )
{
	int iAmmoType = GetAmmoDef()->Index(pszAmmoName);
	if (iAmmoType == -1)
	{
		Msg("ERROR: Attempting to give unknown ammo type (%s)\n",pszAmmoName);
		return 0;
	}

	flCount *= g_pGameRules->GetAmmoQuantityScale(iAmmoType);

	// Don't give out less than 1 of anything.
	flCount = max( 1.0f, flCount );

	return pPlayer->GiveAmmo( flCount, iAmmoType, bSuppressSound );
}

// Already defined in baseclass because of item_dynamic_resupply
#if 0
// Ammo counts given by ammo items - Large variations are NEAR full-resupply
// NOTENOTE: This should (almost) always be a multiplier of the magazine size for the weapon;
// Also, this is very closely linked to balance. Because of this, it might be prudent to link this to the convars, 
// but im not sureabout the perf/function impact of that. For now, It'll stay hardcoded, cause'
// im pretty sure nobody is going to be making mods for a mod anytime soon.
#define SIZE_AMMO_PISTOL			26	// Default Pistol clips come in a bundle
#define SIZE_AMMO_PISTOL_SMALL		13	// Here for posterity
#define SIZE_AMMO_PISTOL_LARGE		104
#define SIZE_AMMO_SMG1				50
#define SIZE_AMMO_SMG1_LARGE		200
#define SIZE_AMMO_SMG2				60
#define SIZE_AMMO_SMG2_LARGE		180
#define SIZE_AMMO_AR1				30
#define SIZE_AMMO_AR1_LARGE			120
#define SIZE_AMMO_AR2				25
#define SIZE_AMMO_AR2_LARGE			100
#define SIZE_AMMO_357				8
#define SIZE_AMMO_357_LARGE			32

#define SIZE_AMMO_RPG_ROUND			1
#define	SIZE_AMMO_AR2_ALTFIRE		1	// Direct grenade
#define SIZE_AMMO_SMG1_GRENADE		1	// Arc grenade

#define SIZE_AMMO_SNIPER			4	// Comes in a strip like a bolt rifle, but is loaded seperately
// Both shotguns use the same ammo, so average out their magazine size (better solution L8R)
#define SIZE_AMMO_BUCKSHOT			5
#define SIZE_AMMO_BUCKSHOT_MAG		7
#define SIZE_AMMO_BUCKSHOT_LARGE	15	//Bigger box based on smaller box, for sweeper use clips
#define SIZE_AMMO_SLUG_MAG			7
#define SIZE_AMMO_CROSSBOW			1
#define SIZE_AMMO_FLAMER			100	// One tank
#define SIZE_AMMO_PROJECTOR			200	// A full burst
#define SIZE_AMMO_REVOLVER			6
#endif

// Obselete ammo types
#define SIZE_BOX_FLARE_ROUNDS 5
#define SIZE_BOX_SNIPER_ROUNDS 8

// ========================================================================
//	>> BoxSRounds
// Pistol clip
// ========================================================================
class CItem_BoxPistolRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_BoxPistolRounds, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/boxsrounds.mdl");

		PrecacheScriptSound( "Item.Magazine_Touch" );
	}

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/boxsrounds.mdl" );
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_PISTOL, "Pistol"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.Magazine_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.Magazine_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}

			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_box_srounds, CItem_BoxPistolRounds);	//OBSELETE - DO NOT USE
LINK_ENTITY_TO_CLASS(item_ammo_pistol, CItem_BoxPistolRounds);

// ========================================================================
//	>> BoxMRounds
// SMG1 clip
// ========================================================================
class CItem_BoxSMGRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_BoxSMGRounds, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/boxmrounds.mdl");

		PrecacheScriptSound( "Item.Magazine_Touch" );
	}

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/boxmrounds.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_SMG1, "SMG1"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.Magazine_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.Magazine_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_box_mrounds, CItem_BoxSMGRounds);	//OBSELETE - DO NOT USE
LINK_ENTITY_TO_CLASS(item_ammo_smg1, CItem_BoxSMGRounds);


// ========================================================================
//	>> LargeBoxSRounds
// General pistol ammo (pistol, smg1)
// ========================================================================
class CItem_LargeBoxSRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_LargeBoxSRounds, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/boxsrounds.mdl");

		PrecacheScriptSound( "Item.SRounds_Touch" );
	}

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/boxsrounds.mdl" );
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		//This seems super-hacky, find a better solution not-so-pronto
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_PISTOL_LARGE, "Pistol") && ITEM_GiveAmmo( pPlayer, SIZE_AMMO_SMG1_LARGE, "SMG1"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.SRounds_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.SRounds_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_large_box_srounds, CItem_LargeBoxSRounds);
LINK_ENTITY_TO_CLASS(item_ammo_pistol_large, CItem_LargeBoxSRounds);	//OBSELETE - DO NOT USE


// ========================================================================
//	>> HMGBox
// HMG Clip - inbetween medium and large
// ========================================================================
class CItem_HMGBox : public CItem
{
public:
	DECLARE_CLASS( CItem_HMGBox, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/boxmrounds.mdl");

		PrecacheScriptSound( "Item.HMGRounds_Touch" );
	}

	void Spawn( void )
	{
		Precache( );
		SetModel( "models/items/boxmrounds.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_SMG2, "HMG"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.HMGRounds_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.HMGRounds_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_ammo_hmg, CItem_HMGBox);

// ========================================================================
//	>> LargeBoxMRounds
// Large box of general rifle rounds (smg1, ar1)
// ========================================================================
class CItem_LargeBoxMRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_LargeBoxMRounds, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/boxmrounds.mdl");

		PrecacheScriptSound( "Item.MRounds_Touch" );
	}

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/boxmrounds.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_SMG1_LARGE, "SMG1") && ITEM_GiveAmmo( pPlayer, SIZE_AMMO_AR2_LARGE, "HMG"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.MRounds_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.MRounds_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_large_box_mrounds, CItem_LargeBoxMRounds);
LINK_ENTITY_TO_CLASS(item_ammo_smg1_large, CItem_LargeBoxMRounds);	//OBSELETE - DO NOT USE

// ========================================================================
//	>> CItem_BoxAR1Rounds
// AR1 Clip
// ========================================================================
#if 0
class CItem_BoxAR1Rounds : public CItem
{
public:
	DECLARE_CLASS( CItem_BoxAR1Rounds, CItem );

	void Spawn( void )
	{
		Precache( );
		SetModel( "models/items/combine_rifle_cartridge01.mdl");
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/items/combine_rifle_cartridge01.mdl");
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_AR1, "AR1"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}	
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_ammo_ar1, CItem_BoxAR1Rounds);
#endif

// ========================================================================
//	>> BoxLRounds
// AR2 Clip
// ========================================================================
class CItem_BoxAR2Rounds : public CItem
{
public:
	DECLARE_CLASS( CItem_BoxAR2Rounds, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/combine_rifle_cartridge01.mdl");

		PrecacheScriptSound( "Item.Magazine_Touch" );
	}

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/combine_rifle_cartridge01.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_AR2, "AR2"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.Magazine_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.Magazine_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}	
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_box_lrounds, CItem_BoxAR2Rounds);	//OBSELETE - DO NOT USE
LINK_ENTITY_TO_CLASS(item_ammo_ar2, CItem_BoxAR2Rounds);

// ========================================================================
//	>> CItem_Box357Rounds
// 357 Clip
// ========================================================================
class CItem_Box357Rounds : public CItem
{
public:
	DECLARE_CLASS( CItem_Box357Rounds, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/357ammo.mdl");

		PrecacheScriptSound( "Item.Magazine_Touch" );
	}

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/357ammo.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_357, "357"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.Magazine_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.Magazine_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}	
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_ammo_357, CItem_Box357Rounds);

// ========================================================================
//	>> CItem_LargeBox357Rounds
// OBSELETE USE item_large_box_lrounds KEPT FOR COMPATIBILITY
// ========================================================================
class CItem_LargeBox357Rounds : public CItem
{
public:
	DECLARE_CLASS( CItem_LargeBox357Rounds, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/357ammobox.mdl");
	}

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/357ammobox.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_357_LARGE, "357"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_ammo_357_large, CItem_LargeBox357Rounds);	//OBSELETE - DO NOT USE

// ========================================================================
//	>> BoxSniperRounds
// Scrapgun clip
// ========================================================================
class CItem_BoxSniperRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_BoxSniperRounds, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/boxsniperrounds.mdl");

		PrecacheScriptSound( "Item.LRounds_Touch" );
	}

	void Spawn( void )
	{
		Precache( );
		SetModel( "models/items/boxsniperrounds.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_BOX_SNIPER_ROUNDS, "SniperRound"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.LRounds_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.LRounds_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}	
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_ammo_sniper, CItem_BoxSniperRounds);


// ========================================================================
//	>> LargeBoxLRounds
// Box of general large/caseless rounds (ar2, magnum, scrapgun)
// ========================================================================
class CItem_LargeBoxLRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_LargeBoxLRounds, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/357ammobox.mdl");

		PrecacheScriptSound( "Item.LRounds_Touch" );
	}

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/357ammobox.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		//This seems super-hacky, find a better solution not-so-pronto
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_AR2_LARGE, "AR2") && ITEM_GiveAmmo( pPlayer, SIZE_AMMO_357_LARGE, "357") && ITEM_GiveAmmo( pPlayer, SIZE_AMMO_SNIPER, "SniperRound"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.LRounds_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.LRounds_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}	
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_large_box_lrounds, CItem_LargeBoxLRounds);
LINK_ENTITY_TO_CLASS(item_ammo_ar2_large, CItem_LargeBoxLRounds);	//OBSELETE - DO NOT USE


// ========================================================================
//	>> CItem_BoxXBowRounds
// ========================================================================
class CItem_BoxXBowRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_BoxXBowRounds, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/crossbowrounds.mdl");
	}

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/crossbowrounds.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_CROSSBOW, "XBowBolt" ))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}	
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_ammo_crossbow, CItem_BoxXBowRounds);

// ========================================================================
//	>> FlareRound
// Single flare
// ========================================================================
class CItem_FlareRound : public CItem
{
public:
	DECLARE_CLASS( CItem_FlareRound, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/flare.mdl");

		PrecacheScriptSound( "Item.FlareRound_Touch" );
	}

	void Spawn( void )
	{
		Precache( );
		SetModel( "models/items/flare.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, 1, "FlareRound"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.FlareRound_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.FlareRound_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_flare_round, CItem_FlareRound);

// ========================================================================
//	>> BoxFlareRounds
// Flaregun clip
// ========================================================================
class CItem_BoxFlareRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_BoxFlareRounds, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/boxflares.mdl");

		PrecacheScriptSound( "Item.FlareRound_Touch" );
	}

	void Spawn( void )
	{
		Precache( );
		SetModel( "models/items/boxflares.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_BOX_FLARE_ROUNDS, "FlareRound"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.FlareRound_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.FlareRound_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_box_flare_rounds, CItem_BoxFlareRounds);


// ========================================================================
// RPG Round
// ========================================================================
class CItem_RPG_Round : public CItem
{
public:
	DECLARE_CLASS( CItem_RPG_Round, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/weapons/w_missile_closed.mdl");

		PrecacheScriptSound( "Item.RPGRound_Touch" );
	}

	void Spawn( void )
	{
		Precache( );
		SetModel( "models/weapons/w_missile_closed.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_RPG_ROUND, "RPGRound"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.RPGRound_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.RPGRound_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}	
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_ml_grenade, CItem_RPG_Round);	//OBSELETE - DO NOT USE
LINK_ENTITY_TO_CLASS(item_rpg_round, CItem_RPG_Round);


// ========================================================================
//	>> AR2_Grenade/SMG1_Grenade
// Grenade for grenade launcher
// ========================================================================
class CItem_AR2_Grenade : public CItem
{
public:
	DECLARE_CLASS( CItem_AR2_Grenade, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/ar2_grenade.mdl");

		PrecacheScriptSound( "Item.AR2Round_Touch" );
	}

	void Spawn( void )
	{
		Precache( );
		SetModel( "models/items/ar2_grenade.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_SMG1_GRENADE, "AR2Grenade"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.AR2Round_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.AR2Round_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}	
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_ar2_grenade, CItem_AR2_Grenade);
LINK_ENTITY_TO_CLASS(item_ammo_smg1_grenade, CItem_AR2_Grenade);

// ========================================================================
//	>> BoxBuckshot
// ========================================================================
class CItem_BoxBuckshot : public CItem
{
public:
	DECLARE_CLASS( CItem_BoxBuckshot, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/boxbuckshot.mdl");

		PrecacheScriptSound( "Item.BoxBuckshot_Touch" );
	}

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/boxbuckshot.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_BUCKSHOT, "Buckshot"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.BoxBuckshot_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.BoxBuckshot_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_box_buckshot, CItem_BoxBuckshot);


// ========================================================================
//	>> LargeBoxBuckshot
// ========================================================================
class CItem_LargeBoxBuckshot : public CItem
{
public:
	DECLARE_CLASS( CItem_LargeBoxBuckshot, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/boxbuckshot.mdl");

		PrecacheScriptSound( "Item.BoxBuckshot_Touch" );
	}

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/boxbuckshot.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_BUCKSHOT_LARGE, "Buckshot"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.BoxBuckshot_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.BoxBuckshot_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_large_box_buckshot, CItem_LargeBoxBuckshot);


// ========================================================================
//	>> ClipBuckshot
// ========================================================================
class CItem_ClipBuckshot : public CItem
{
public:
	DECLARE_CLASS( CItem_ClipBuckshot, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/combine_rifle_cartridge01.mdl");	//TODO;Real model

		PrecacheScriptSound( "Item.Magazine_Touch" );
	}

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/combine_rifle_cartridge01.mdl");	//TODO;Real model
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_BUCKSHOT_MAG, "Buckshot"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.Magazine_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.Magazine_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_ammo_buckshot, CItem_ClipBuckshot);


// ========================================================================
//	>> ClipFragshot
// ========================================================================
class CItem_ClipFragshot : public CItem
{
public:
	DECLARE_CLASS( CItem_ClipFragshot, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/combine_rifle_cartridge01.mdl");	//TODO;Real model

		PrecacheScriptSound( "Item.Magazine_Touch" );
	}

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/combine_rifle_cartridge01.mdl");	//TODO;Real model
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_BUCKSHOT_MAG, "Fragshot"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.Magazine_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.Magazine_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_ammo_fragshot, CItem_ClipFragshot);


// ========================================================================
//	>> CItem_AR2AltFireRound
// Old AR2 Altfire
// ========================================================================
class CItem_AR2AltFireRound : public CItem
{
public:
	DECLARE_CLASS( CItem_AR2AltFireRound, CItem );

	void Precache( void )
	{
		PrecacheParticleSystem( "combineball" );
		PrecacheModel ("models/items/combine_rifle_ammo01.mdl");
	}

	void Spawn( void )
	{
		Precache( );
		SetModel( "models/items/combine_rifle_ammo01.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_AR2_ALTFIRE, "AR2AltFire" ) )
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS( item_ammo_ar2_altfire, CItem_AR2AltFireRound );


// ========================================================================
//	>> CItem_FlamerTank
// Tank o' napalm
// ========================================================================
class CItem_FlamerTank : public CItem
{
public:
	DECLARE_CLASS( CItem_FlamerTank, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/flametank.mdl");

		PrecacheScriptSound( "Item.FlameTank_Touch" );
	}

	void Spawn( void )
	{
		Precache( );
		SetModel( "models/items/flametank.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_FLAMER, "Flamer"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.FlameTank_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.FlameTank_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}	
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_ammo_flametank, CItem_FlamerTank);
LINK_ENTITY_TO_CLASS(item_ammo_flame, CItem_FlamerTank);

// ========================================================================
//	>> CItem_LargeFlamerTank
// Bundle o' tanks
// ========================================================================
#if 0
class CItem_LargeFlamerTank : public CItem
{
public:
	DECLARE_CLASS( CItem_LargeFlamerTank, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/flametank_large.mdl");

		PrecacheScriptSound( "Item.FlameTank_Touch" );
	}

	void Spawn( void )
	{
		Precache( );
		SetModel( "models/items/flametank_large.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_PROJECTOR, "Flamer"))
		{
			CPASAttenuationFilter filter( pPlayer, "Item.FlameTank_Touch" );
			EmitSound( filter, pPlayer->entindex(), "Item.FlameTank_Touch" );
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}	
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_ammo_flamer_large, CItem_LargeFlamerTank);
#endif

// ==================================================================
// Ammo crate which will supply infinite ammo of the specified type
// ==================================================================

// Ammo types
enum
{
	AMMOCRATE_SMALL_ROUNDS,
	AMMOCRATE_MEDIUM_ROUNDS,
	AMMOCRATE_LARGE_ROUNDS,
	AMMOCRATE_RPG_ROUNDS,
	AMMOCRATE_BUCKSHOT,
	AMMOCRATE_GRENADES,
	AMMOCRATE_357,
	AMMOCRATE_CROSSBOW,
	AMMOCRATE_AR2_ALTFIRE,
	AMMOCRATE_SMG_ALTFIRE,
	NUM_AMMO_CRATE_TYPES,
};

// Ammo crate

class CItem_AmmoCrate : public CBaseAnimating
{
public:
	DECLARE_CLASS( CItem_AmmoCrate, CBaseAnimating );

	void	Spawn( void );
	void	Precache( void );
	bool	CreateVPhysics( void );

	virtual void HandleAnimEvent( animevent_t *pEvent );

	void	SetupCrate( void );
	void	OnRestore( void );

	//FIXME: May not want to have this used in a radius
	int		ObjectCaps( void ) { return (BaseClass::ObjectCaps() | (FCAP_IMPULSE_USE|FCAP_USE_IN_RADIUS)); };
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void	InputKill( inputdata_t &data );
	void	CrateThink( void );
	
	virtual int OnTakeDamage( const CTakeDamageInfo &info );

protected:

	int		m_nAmmoType;
	int		m_nAmmoIndex;

	static const char *m_lpzModelNames[NUM_AMMO_CRATE_TYPES];
	static const char *m_lpzAmmoNames[NUM_AMMO_CRATE_TYPES];
	static int m_nAmmoAmounts[NUM_AMMO_CRATE_TYPES];
	static const char *m_pGiveWeapon[NUM_AMMO_CRATE_TYPES];

	float	m_flCloseTime;
	COutputEvent	m_OnUsed;
	CHandle< CBasePlayer > m_hActivator;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( item_ammo_crate, CItem_AmmoCrate );

BEGIN_DATADESC( CItem_AmmoCrate )

	DEFINE_KEYFIELD( m_nAmmoType,	FIELD_INTEGER, "AmmoType" ),	

	DEFINE_FIELD( m_flCloseTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_hActivator, FIELD_EHANDLE ),

	// These can be recreated
	//DEFINE_FIELD( m_nAmmoIndex,		FIELD_INTEGER ),
	//DEFINE_FIELD( m_lpzModelNames,	FIELD_ ),
	//DEFINE_FIELD( m_lpzAmmoNames,	FIELD_ ),
	//DEFINE_FIELD( m_nAmmoAmounts,	FIELD_INTEGER ),

	DEFINE_OUTPUT( m_OnUsed, "OnUsed" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Kill", InputKill ),

	DEFINE_THINKFUNC( CrateThink ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Animation events.
//-----------------------------------------------------------------------------

// Models names
const char *CItem_AmmoCrate::m_lpzModelNames[NUM_AMMO_CRATE_TYPES] =
{
	"models/items/ammocrate_pistol.mdl",	// Small rounds
	"models/items/ammocrate_smg1.mdl",		// Medium rounds
	"models/items/ammocrate_ar2.mdl",		// Large rounds
	"models/items/ammocrate_rockets.mdl",	// RPG rounds
	"models/items/ammocrate_buckshot.mdl",	// Buckshot
	"models/items/ammocrate_grenade.mdl",	// Grenades
	"models/items/ammocrate_smg1.mdl",		// 357
	"models/items/ammocrate_smg1.mdl",	// Crossbow
	
	//FIXME: This model is incorrect!
	"models/items/ammocrate_ar2.mdl",		// Combine Ball 
	"models/items/ammocrate_smg2.mdl",	    // smg grenade
};

// Ammo type names
const char *CItem_AmmoCrate::m_lpzAmmoNames[NUM_AMMO_CRATE_TYPES] =
{
	"Pistol",		
	"SMG1",			
	"AR2",			
	"RPGRound",	
	"Buckshot",		
	"Grenade",
	"357",
	"XBowBolt",
	"AR2AltFire",
	"AR2Grenade",
};

// Ammo amount given per +use
int CItem_AmmoCrate::m_nAmmoAmounts[NUM_AMMO_CRATE_TYPES] =
{
	300,	// Pistol
	300,	// SMG1
	300,	// AR2
	3,		// RPG rounds
	120,	// Buckshot
	4,		// Grenades
	50,		// 357
	50,		// Crossbow
	3,		// AR2 alt-fire
	5,
};

const char *CItem_AmmoCrate::m_pGiveWeapon[NUM_AMMO_CRATE_TYPES] =
{
	NULL,	// Pistol
	NULL,	// SMG1
	NULL,	// AR2
	NULL,		// RPG rounds
	NULL,	// Buckshot
	"weapon_frag",		// Grenades
	NULL,		// 357
	NULL,		// Crossbow
	NULL,		// AR2 alt-fire
	NULL,		// SMG alt-fire
};

#define	AMMO_CRATE_CLOSE_DELAY	1.5f

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItem_AmmoCrate::Spawn( void )
{
	Precache();

	BaseClass::Spawn();

	SetModel( STRING( GetModelName() ) );
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_VPHYSICS );
	CreateVPhysics();

	ResetSequence( LookupSequence( "Idle" ) );
	SetBodygroup( 1, true );

	m_flCloseTime = gpGlobals->curtime;
	m_flAnimTime = gpGlobals->curtime;
	m_flPlaybackRate = 0.0;
	SetCycle( 0 );

	m_takedamage = DAMAGE_EVENTS_ONLY;

}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
bool CItem_AmmoCrate::CreateVPhysics( void )
{
	return ( VPhysicsInitStatic() != NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItem_AmmoCrate::Precache( void )
{
	SetupCrate();
	PrecacheModel( STRING( GetModelName() ) );

	PrecacheScriptSound( "AmmoCrate.Open" );
	PrecacheScriptSound( "AmmoCrate.Close" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItem_AmmoCrate::SetupCrate( void )
{
	SetModelName( AllocPooledString( m_lpzModelNames[m_nAmmoType] ) );
	
	m_nAmmoIndex = GetAmmoDef()->Index( m_lpzAmmoNames[m_nAmmoType] );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItem_AmmoCrate::OnRestore( void )
{
	BaseClass::OnRestore();

	// Restore our internal state
	SetupCrate();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pActivator - 
//			*pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void CItem_AmmoCrate::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );

	if ( pPlayer == NULL )
		return;

	m_OnUsed.FireOutput( pActivator, this );

	int iSequence = LookupSequence( "Open" );

	// See if we're not opening already
	if ( GetSequence() != iSequence )
	{
		Vector mins, maxs;
		trace_t tr;

		CollisionProp()->WorldSpaceAABB( &mins, &maxs );

		Vector vOrigin = GetAbsOrigin();
		vOrigin.z += ( maxs.z - mins.z );
		mins = (mins - GetAbsOrigin()) * 0.2f;
		maxs = (maxs - GetAbsOrigin()) * 0.2f;
		mins.z = ( GetAbsOrigin().z - vOrigin.z );  
		
		UTIL_TraceHull( vOrigin, vOrigin, mins, maxs, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

		if ( tr.startsolid || tr.allsolid )
			 return;
			
		m_hActivator = pPlayer;

		// Animate!
		ResetSequence( iSequence );

		// Make sound
		CPASAttenuationFilter sndFilter( this, "AmmoCrate.Open" );
		EmitSound( sndFilter, entindex(), "AmmoCrate.Open" );

		// Start thinking to make it return
		SetThink( &CItem_AmmoCrate::CrateThink );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}

	// Don't close again for two seconds
	m_flCloseTime = gpGlobals->curtime + AMMO_CRATE_CLOSE_DELAY;
}

//-----------------------------------------------------------------------------
// Purpose: allows the crate to open up when hit by a crowbar
//-----------------------------------------------------------------------------
int CItem_AmmoCrate::OnTakeDamage( const CTakeDamageInfo &info )
{
	// if it's the player hitting us with a crowbar, open up
	CBasePlayer *player = ToBasePlayer(info.GetAttacker());
	if (player)
	{
		CBaseCombatWeapon *weapon = player->GetActiveWeapon();
		
		// Shouldnt this just grab the damage type?
		if (weapon && ( !stricmp(weapon->GetName(), "weapon_crowbar") || !stricmp(weapon->GetName(), "weapon_swing") || !stricmp(weapon->GetName(), "weapon_stab") ) )
		{
			// play the normal use sound
			player->EmitSound( "HL2Player.Use" );
			// open the crate
			Use(info.GetAttacker(), info.GetAttacker(), USE_TOGGLE, 0.0f);
		}
	}

	// don't actually take any damage
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Catches the monster-specific messages that occur when tagged
//			animation frames are played.
// Input  : *pEvent - 
//-----------------------------------------------------------------------------
void CItem_AmmoCrate::HandleAnimEvent( animevent_t *pEvent )
{
	if ( pEvent->event == AE_AMMOCRATE_PICKUP_AMMO )
	{
		if ( m_hActivator )
		{
			if ( m_pGiveWeapon[m_nAmmoType] && !m_hActivator->Weapon_OwnsThisType( m_pGiveWeapon[m_nAmmoType] ) )
			{
				CBaseEntity *pEntity = CreateEntityByName( m_pGiveWeapon[m_nAmmoType] );
				CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon*>(pEntity);
				if ( pWeapon )
				{
					pWeapon->SetAbsOrigin( m_hActivator->GetAbsOrigin() );
					pWeapon->m_iPrimaryAmmoType = 0;
					pWeapon->m_iSecondaryAmmoType = 0;
					pWeapon->Spawn();
					if ( !m_hActivator->BumpWeapon( pWeapon ) )
					{
						UTIL_Remove( pEntity );
					}
					else
					{
						SetBodygroup( 1, false );
					}
				}
			}

			if ( m_hActivator->GiveAmmo( m_nAmmoAmounts[m_nAmmoType], m_nAmmoIndex ) != 0 )
			{
				SetBodygroup( 1, false );
			}
			m_hActivator = NULL;
		}
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

	
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItem_AmmoCrate::CrateThink( void )
{
	StudioFrameAdvance();
	DispatchAnimEvents( this );

	SetNextThink( gpGlobals->curtime + 0.1f );

	// Start closing if we're not already
	if ( GetSequence() != LookupSequence( "Close" ) )
	{
		// Not ready to close?
		if ( m_flCloseTime <= gpGlobals->curtime )
		{
			m_hActivator = NULL;

			ResetSequence( LookupSequence( "Close" ) );
		}
	}
	else
	{
		// See if we're fully closed
		if ( IsSequenceFinished() )
		{
			// Stop thinking
			SetThink( NULL );
			CPASAttenuationFilter sndFilter( this, "AmmoCrate.Close" );
			EmitSound( sndFilter, entindex(), "AmmoCrate.Close" );

			// FIXME: We're resetting the sequence here
			// but setting Think to NULL will cause this to never have
			// StudioFrameAdvance called. What are the consequences of that?
			ResetSequence( LookupSequence( "Idle" ) );
			SetBodygroup( 1, true );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void CItem_AmmoCrate::InputKill( inputdata_t &data )
{
	UTIL_Remove( this );
}

