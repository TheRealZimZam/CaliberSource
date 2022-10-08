//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ITEMS_H
#define ITEMS_H

#ifdef _WIN32
#pragma once
#endif

#include "entityoutput.h"
#include "player_pickup.h"
#include "vphysics/constraints.h"


// Armor given by a battery
#define MAX_NORMAL_BATTERY	100

//! Putting these in the baseclass is gross, but its needed for item_dynamic_resupply
#ifdef HL2_DLL
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
#define SIZE_AMMO_SMG2				35
#define SIZE_AMMO_SMG2_LARGE		140
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
#define SIZE_AMMO_BUCKSHOT_LARGE	16	//Bigger box based on smaller box, for sweeper use clips
#define SIZE_AMMO_SLUG_MAG			7
#define SIZE_AMMO_CROSSBOW			1
#define SIZE_AMMO_FLAMER			100	// One tank
#define SIZE_AMMO_PROJECTOR			200	// A full burst
#define SIZE_AMMO_REVOLVER			6
#endif

#define SF_ITEM_START_CONSTRAINED	0x00000001


class CItem : public CBaseAnimating, public CDefaultPlayerPickupVPhysics
{
public:
	DECLARE_CLASS( CItem, CBaseAnimating );

	CItem();

	virtual void Spawn( void );
	virtual void Precache();

	virtual CBaseEntity* Respawn( void );
	void ItemTouch( CBaseEntity *pOther );
	virtual void Materialize( void );
	virtual bool MyTouch( CBasePlayer *pPlayer ) { return false; };

	// Become touchable when we are at rest
	virtual void OnEntityEvent( EntityEvent_t event, void *pEventData );

	// Activate when at rest, but don't allow pickup until then
	void ActivateWhenAtRest();

	// IPlayerPickupVPhysics
	virtual void OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason = PICKED_UP_BY_CANNON );
	virtual void OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason );

	virtual int	ObjectCaps() { return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE | FCAP_WCEDIT_POSITION; };
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	Vector	GetOriginalSpawnOrigin( void ) { return m_vOriginalSpawnOrigin;	}
	QAngle	GetOriginalSpawnAngles( void ) { return m_vOriginalSpawnAngles;	}
	void	SetOriginalSpawnOrigin( const Vector& origin ) { m_vOriginalSpawnOrigin = origin; }
	void	SetOriginalSpawnAngles( const QAngle& angles ) { m_vOriginalSpawnAngles = angles; }
	bool	CreateItemVPhysicsObject( void );
	bool	ItemCanBeTouchedByPlayer( CBasePlayer *pPlayer );

#if defined( HL2MP )
	void	FallThink( void );
	float  m_flNextResetCheckTime;
#endif

	DECLARE_DATADESC();
private:
	void ComeToRest( void );

private:
	bool		m_bActivateWhenAtRest;
	COutputEvent m_OnPlayerTouch;
	COutputEvent m_OnCacheInteraction;
	
	Vector		m_vOriginalSpawnOrigin;
	QAngle		m_vOriginalSpawnAngles;

	IPhysicsConstraint		*m_pConstraint;
};

#endif // ITEMS_H
