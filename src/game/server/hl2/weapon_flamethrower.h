//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:	Flamethrower test
//
//			Primary attack: Throw flame
//			Secondary attack: Spew ignitable gas onto surfaces
//
// $NoKeywords: $
//=============================================================================

#ifndef WEAPON_FLAMETHROWER_H
#define WEAPON_FLAMETHROWER_H
#ifdef _WIN32
#pragma once
#endif

#include "basehlcombatweapon_shared.h"
#include "basegrenade_shared.h"
#include "soundenvelope.h"
#include "sprite.h"

// TODO; Some of these might be good to offload
#define	FLAMETHROWER_VELOCITY	450.0f
#define	FLAMETHROWER_DRAINRATE	0.1f	// Drain a singular ammo every this amount of time while on
#define	FLAMETHROWER_FIRERATE	0.2f	// Actually eject a fireball entity this often.
#define	FLAMETHROWER_PREFIRE_WAIT		0.4f	// How long it takes to actually start firing
#define FLAMETHROWER_SPREAD_ANGLE		12.0	// How far the flame particles can spread from the center.
#define FLAMETHROWER_SPREAD_VERTICAL	2.0		// How far the flame particles can vertically spread

#define FLAMETHROWER_MUZZLEPOS_FORWARD		10.0f
#define FLAMETHROWER_MUZZLEPOS_RIGHT		9.0f
#define FLAMETHROWER_MUZZLEPOS_UP			-18.0f

enum FlameThrowerState_t
{
	// Firing states.
	FT_STATE_IDLE = 0,		//Idle, not in use
	FT_STATE_STARTFIRING,	//Preparing to fire	(pressed in_attack1)
	FT_STATE_FIRING,		//Firing (holding in_attack1)
	FT_STATE_STOPFIRING		//Stopping fire (in_attack1 debounce)
};

//=========================================================
// Medikit Weapon
//=========================================================
class CWeaponFlameThrower : public CBaseHLCombatWeapon
{
public:
	DECLARE_DATADESC();
	DECLARE_CLASS( CWeaponFlameThrower, CBaseHLCombatWeapon );

	CWeaponFlameThrower();
	~CWeaponFlameThrower();
	DECLARE_SERVERCLASS();

	float GetVelocity( void );
	float GetFireRate( void )		{ return BaseClass::GetCycleTime(); }	//FLAMETHROWER_FIRERATE

	int CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	void Precache( void );
	void PrimaryAttack( void );
	void SecondaryAttack( void );

	// Flame sound
	void	StartFlameSound();
	void	StopFlameSound();

	// Pilot light sound/fx
	bool 	StartPilotLight();
	bool 	StopPilotLight();

	bool	CanHolster( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	bool	Reload( void );

	void	ItemPostFrame( void );

	DECLARE_ACTTABLE();

private:
	float					m_fDrainRate;
//	float					m_fPrefireWait;

	// Start/stop the fire sound.
	bool					m_bSoundOn;	// Is the sound on?
	bool					m_bFiring;	// Are we firing?
	bool					m_bIgnited;	// Are we ignited?
	
	CHandle<CSprite>	m_hDripSprite;	//Dripings that come off the muzzle
	CHandle<CSprite>	m_hPilotSprite;	//Igniter

};

#endif // WEAPON_FLAME_THROWER_H
