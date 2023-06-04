//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose:	Shared Throwable functions
//
// $Workfile:     $
// $Date:         $
//=============================================================================

#include "basehlcombatweapon.h"

#ifndef	BASEHLTHROWABLE_H
#define	BASEHLTHROWABLE_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Fragmentation grenades
//-----------------------------------------------------------------------------
class CHLThrowable : public CBaseHLCombatWeapon
{
	DECLARE_CLASS( CHLThrowable, CBaseHLCombatWeapon );
public:
	CHLThrowable();

	DECLARE_SERVERCLASS();

	DECLARE_DATADESC();

	virtual void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	virtual void	PrimaryAttack( void );
	virtual void	SecondaryAttack( void );
	virtual void	DecrementAmmo( CBaseCombatCharacter *pOwner );
	virtual void	ItemPostFrame( void );

	virtual bool	Deploy( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );

	virtual	int			WeaponRangeAttack1Condition( float flDot, float flDist );
	virtual	int			WeaponRangeAttack2Condition( float flDot, float flDist ) { return WeaponRangeAttack1Condition(flDot,flDist); }
	virtual	bool		WeaponLOSCondition(const Vector &ownerPos, const Vector &targetPos, bool bSetConditions);
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	
	virtual bool	Reload( void );

	bool	ShouldDisplayHUDHint() { return true; }
	virtual bool IsArmed( void ) { return m_bDrawbackFinished == true; }

	// Overwriten by baseclasses
	virtual float	GetFuseTime( void ) { return 0; }	//Base fuse time
	virtual float	GetCookedFuseTime( void ) { return 0.1 + m_flCookedFuseTime; }	//Fuse time including cooking
	virtual float	GetMinAttackDelay( void ) { return 6; }
	virtual float	GetMaxAttackDelay( void ) { return 8; }

	virtual void	ThrowGrenade( CBaseCombatCharacter *pOwner );
	virtual void	RollGrenade( CBaseCombatCharacter *pOwner );
	virtual void	LobGrenade( CBaseCombatCharacter *pOwner );

protected:
	// check a throw from vecSrc.  If not valid, move the position back along the line to vecEye
	virtual void	CheckThrowPosition( CBasePlayer *pPlayer, const Vector &vecEye, Vector &vecSrc );

	bool	m_bRedraw;				//Draw the weapon again after throwing a grenade
	int		m_AttackPaused;
	bool	m_bDrawbackFinished;
	float	m_flCookedFuseTime;
	float	m_flNextThrowCheck;		// When to check throw ability next
	
	Vector				m_vecTossVelocity;

};

#endif	//BASEHLTHROWABLE_H
