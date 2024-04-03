//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Swing type weapons
//
//=============================================================================//

#ifndef WEAPON_SWING_H
#define WEAPON_SWING_H

#include "basebludgeonweapon_shared.h"

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
#ifdef CLIENT_DLL
#define CWeaponSwing C_WeaponSwing
#endif
class CWeaponSwing : public CBaseBludgeonWeapon
{
	DECLARE_CLASS( CWeaponSwing, CBaseBludgeonWeapon );
public:

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();

	CWeaponSwing();

	float		GetRange( void )		{	return SWING_RANGE;	}
	float		GetFireRate( void )		{	return BaseClass::GetCycleTime();	}	//SWING_REFIRE
	int			GetDamageType( void )	{	return DMG_CLUB;	}

	void		AddViewKick( void );
	float		GetDamageForActivity( Activity hitActivity );

	void		SecondaryAttack( void )	{ return; }	//No secondary (for now)

#ifndef CLIENT_DLL
	// Animation event
	virtual int WeaponMeleeAttack1Condition( float flDot, float flDist );
	virtual void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
#endif

private:
#ifndef CLIENT_DLL
	// Animation event handlers
	void HandleAnimEventMeleeHit( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
#endif
	int m_nModel;
};

#if 0
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
#endif

#endif // WEAPON_BAT_H
