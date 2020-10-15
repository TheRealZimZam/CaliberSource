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
public:
	DECLARE_CLASS( CWeaponSwing, CBaseHLBludgeonWeapon );

	DECLARE_SERVERCLASS();
	DECLARE_ACTTABLE();

	CWeaponSwing();

	float		GetRange( void )		{	return	SWING_RANGE;	}
	float		GetFireRate( void )		{	return BaseClass::GetCycleTime();	}	//SWING_REFIRE

	void		AddViewKick( void );
	float		GetDamageForActivity( Activity hitActivity );

	virtual int WeaponMeleeAttack1Condition( float flDot, float flDist );
	void		SecondaryAttack( void )	{	return;	}

	// Animation event
	virtual void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

private:
	// Animation event handlers
	void HandleAnimEventMeleeHit( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
};

#endif // WEAPON_BAT_H
