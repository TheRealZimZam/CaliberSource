//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#ifndef WEAPON_CGUARD_H
#define WEAPON_CGUARD_H
#ifdef _WIN32
#pragma once
#endif

#include "basehlcombatweapon_shared.h"
#ifndef CLIENT_DLL
	#include "te_particlesystem.h"
#endif

#if 0
#ifdef CLIENT_DLL
#define CWeaponCGuard C_WeaponCGuard
#endif
class CWeaponCGuard : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( CWeaponCGuard, CBaseHLCombatWeapon );

	DECLARE_SERVERCLASS();

	CWeaponCGuard( void );
	
	void Precache( void );
	void PrimaryAttack( void );
	void AddViewKick( void );
	void DelayedFire( void );
	void ItemPostFrame( void );
	void AlertTargets( void );
	void UpdateLasers( void );

	int CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	bool CanBePickedUpByNPCs( void ) { return false; }

	DECLARE_ACTTABLE();

protected:
	float	m_flChargeTime;
	bool	m_bFired;

	int		m_beamIndex;
	int		m_haloIndex;
};
#endif

#endif // WEAPON_CGUARD_H