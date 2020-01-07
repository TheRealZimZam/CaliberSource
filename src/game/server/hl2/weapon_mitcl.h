//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_MITCL_H
#define WEAPON_MITCL_H

#ifdef _WIN32
#pragma once
#endif

#include "basehlcombatweapon.h"
#include "Sprite.h"
#include "npcevent.h"
#include "beam_shared.h"

//-----------------------------------------------------------------------------
// RPG
//-----------------------------------------------------------------------------
class CWeaponMITCL : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( CWeaponMITCL, CBaseHLCombatWeapon );

	CWeaponMITCL();
	~CWeaponMITCL();

	DECLARE_SERVERCLASS();

	void	Precache( void );

	void	PrimaryAttack( void );
	virtual float GetFireRate( void ) { return 0.875f; };
	void	ItemPostFrame( void );

	void	DecrementAmmo( CBaseCombatCharacter *pOwner );

	bool	Deploy( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	bool	Reload( void );
	bool	WeaponShouldBeLowered( void );
	bool	Lower( void );

	virtual void Drop( const Vector &vecVelocity );

	int		GetMinBurst() { return 4; }
	int		GetMaxBurst() { return 4; }
	float	GetMinRestTime() { return 4.0; }
	float	GetMaxRestTime() { return 4.0; }

	bool	WeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions );
	int		WeaponRangeAttack1Condition( float flDot, float flDist );

	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	void	NotifyRocketDied( void );

	bool	HasAnyAmmo( void );

	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone = VECTOR_CONE_3DEGREES;
		return cone;
	}
	
	CBaseEntity *GetMissile( void ) { return m_hMissile; }

	DECLARE_ACTTABLE();
	
protected:
	bool				m_bInitialStateUpdate;
//	CHandle<CMissile>	m_hMissile;
};

#endif // WEAPON_MITCL
