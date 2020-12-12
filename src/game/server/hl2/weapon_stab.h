//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Bayonet Knife mounted on guns
//
//=============================================================================//

#ifndef WEAPON_STAB_H
#define WEAPON_STAB_H

#include "basebludgeonweapon.h"

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

class CWeaponStab : public CBaseHLBludgeonWeapon
{
public:
	DECLARE_CLASS( CWeaponStab, CBaseHLBludgeonWeapon );

	DECLARE_SERVERCLASS();
	DECLARE_ACTTABLE();

	CWeaponStab();

	float		GetRange( void )		{	return STAB_RANGE;	}
	float		GetFireRate( void )		{	return BaseClass::GetCycleTime();	}	//STAB_REFIRE

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

#endif // WEAPON_KNIFE_H
