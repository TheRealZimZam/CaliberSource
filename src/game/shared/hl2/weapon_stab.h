//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Bayonet Knife mounted on guns
//
//=============================================================================//

#ifndef WEAPON_STAB_H
#define WEAPON_STAB_H

#include "basebludgeonweapon_shared.h"

#if defined( _WIN32 )
#pragma once
#endif

#ifdef HL2MP
#error weapon_crowbar.h must not be included in hl2mp. The windows compiler will use the wrong class elsewhere if it is.
#endif

#define	STAB_RANGE	65.0f
#define	STAB_REFIRE	0.3f

//-----------------------------------------------------------------------------
// CWeaponStab
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
#define CWeaponStab C_WeaponStab
#endif
class CWeaponStab : public CBaseBludgeonWeapon
{
	DECLARE_CLASS( CWeaponStab, CBaseBludgeonWeapon );
public:
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();

	CWeaponStab();

	float		GetRange( void )		{	return STAB_RANGE;	}
	float		GetFireRate( void )		{	return BaseClass::GetCycleTime();	}	//STAB_REFIRE
	int			GetDamageType( void )	{	return DMG_SLASH;	}

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

// TODO; Better system needed here, this is just temp for demo
#if 0
class CWeaponBottle : public CWeaponStab
{
public:
	DECLARE_CLASS( CWeaponBottle, CWeaponStab );

//	DECLARE_SERVERCLASS();

	CWeaponBottle();

};

class CWeaponKnife : public CWeaponStab
{
public:
	DECLARE_CLASS( CWeaponKnife, CWeaponStab );

//	DECLARE_SERVERCLASS();

	CWeaponKnife();

};

class CWeaponRebar : public CWeaponStab
{
public:
	DECLARE_CLASS( CWeaponRebar, CWeaponStab );

//	DECLARE_SERVERCLASS();

	CWeaponRebar();

};

class CWeaponShank : public CWeaponStab
{
public:
	DECLARE_CLASS( CWeaponShank, CWeaponStab );

//	DECLARE_SERVERCLASS();

	CWeaponShank();

};
#endif

#endif // WEAPON_KNIFE_H
