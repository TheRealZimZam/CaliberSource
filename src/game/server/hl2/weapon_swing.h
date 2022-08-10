//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Swing type weapons
//
//=============================================================================//

#ifndef WEAPON_SWING_H
#define WEAPON_SWING_H

#include "basebludgeonweapon.h"

#if defined( _WIN32 )
#pragma once
#endif

#ifdef HL2MP
#error weapon_crowbar.h must not be included in hl2mp. The windows compiler will use the wrong class elsewhere if it is.
#endif

#define	SWING_RANGE	85.0f
#define	SWING_REFIRE	0.5f

//-----------------------------------------------------------------------------
// CWeaponSwing
//-----------------------------------------------------------------------------

class CWeaponSwing : public CBaseHLBludgeonWeapon
{
	DECLARE_CLASS( CWeaponSwing, CBaseHLBludgeonWeapon );
public:

	DECLARE_SERVERCLASS();
	DECLARE_ACTTABLE();

	DECLARE_DATADESC();

	CWeaponSwing();

	float		GetRange( void )		{	return	SWING_RANGE;	}
	float		GetFireRate( void )		{	return BaseClass::GetCycleTime();	}	//SWING_REFIRE
	int			GetDamageType( void )	{	return DMG_CLUB;	}

	void		AddViewKick( void );
	float		GetDamageForActivity( Activity hitActivity );

	virtual int WeaponMeleeAttack1Condition( float flDot, float flDist );
	void		SecondaryAttack( void )	{ return; }	//No secondary (for now)

	// Animation event
	virtual void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

private:
	// Animation event handlers
	void HandleAnimEventMeleeHit( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
};

// TODO; Better system needed here, this is just temp for demo
class CWeaponBat : public CWeaponSwing
{
public:
	DECLARE_CLASS( CWeaponBat, CWeaponSwing );

//	DECLARE_SERVERCLASS();

	CWeaponBat();
};

class CWeaponCrowbar : public CWeaponSwing
{
public:
	DECLARE_CLASS( CWeaponCrowbar, CWeaponSwing );

//	DECLARE_SERVERCLASS();

	CWeaponCrowbar();
};

class CWeaponHammer : public CWeaponSwing
{
public:
	DECLARE_CLASS( CWeaponHammer, CWeaponSwing );

//	DECLARE_SERVERCLASS();

	CWeaponHammer();
};

class CWeaponPipe : public CWeaponSwing
{
public:
	DECLARE_CLASS( CWeaponPipe, CWeaponSwing );

//	DECLARE_SERVERCLASS();

	CWeaponPipe();
};

#endif // WEAPON_BAT_H
