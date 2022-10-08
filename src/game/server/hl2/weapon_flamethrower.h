//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef WEAPON_FLAMETHROWER_H
#define WEAPON_FLAMETHROWER_H
#ifdef _WIN32
#pragma once
#endif

#include "basehlcombatweapon.h"
#include "basegrenade_shared.h"
#include "soundenvelope.h"

#define	FLAMETHROWER_VELOCITY	400.0f
#define	FLAMETHROWER_DRAINRATE	0.05f	// Drain a singular ammo every this amount of time while on
#define	FLAMETHROWER_FIRERATE	0.1f	// Actually eject a fire blob entity this often.
#define FLAMETHROWER_SPREAD_ANGLE		15.0	// How far the flame particles will spread from the center.

//=========================================================
// Medikit Weapon
//=========================================================
class CWeaponFlameThrower : public CBaseHLCombatWeapon
{
public:
	DECLARE_DATADESC();
	DECLARE_CLASS( CWeaponFlameThrower, CBaseHLCombatWeapon );

	CWeaponFlameThrower();
	DECLARE_SERVERCLASS();

	float GetVelocity( void )		{ return FLAMETHROWER_VELOCITY; }
	float GetFireRate( void )		{ return FLAMETHROWER_FIRERATE; }

	int CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	void Precache( void );
	void PrimaryAttack( void );

	void	StartSound();
	void	StopFlameSound();
	
	bool	CanHolster( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	bool	Reload( void );

	void	ItemPostFrame( void );
	
	DECLARE_ACTTABLE();

private:
	float					m_fDrainRate;

	// Start/stop the fire sound.
	bool					m_bSoundOn;	// Is the sound on?
	bool					m_bFiring;	// Are we firing?
};

#endif // WEAPON_FLAME_THROWER_H
