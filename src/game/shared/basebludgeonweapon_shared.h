//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		The class from which all bludgeon melee
//				weapons are derived. 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "basecombatweapon_shared.h"

#ifndef BASEBLUDGEONWEAPON_H
#define BASEBLUDGEONWEAPON_H

#ifdef _WIN32
#pragma once
#endif

#if defined( CLIENT_DLL )
#define CBaseBludgeonWeapon C_BaseBludgeonWeapon
#endif

//=========================================================
// CBaseBludgeonWeapon 
//=========================================================
class CBaseBludgeonWeapon : public CBaseCombatWeapon
{
	DECLARE_CLASS( CBaseBludgeonWeapon, CBaseCombatWeapon );
public:
	CBaseBludgeonWeapon();

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	virtual bool	IsPredicted() const { return true; }

	virtual	void	Spawn( void );
	virtual	void	Precache( void );

	//Attack functions
	virtual	void	PrimaryAttack( void );
	virtual	void	SecondaryAttack( void );
	
	virtual void	ItemPostFrame( void );

	//Functions to select animation sequences 
	virtual Activity	GetPrimaryAttackActivity( void )	{	return	ACT_VM_HITCENTER;	}
	virtual Activity	GetSecondaryAttackActivity( void )	{	return	ACT_VM_HITCENTER2;	}

	virtual	float	GetFireRate( void )								{	return	0.2f;	}
	virtual float	GetRange( void )								{	return	60.0f;	}
	virtual	int		GetDamageType( void )							{	return	DMG_CLUB;	}
	virtual	float	GetDamageForActivity( Activity hitActivity )	{	return	1.0f;	}

#ifndef CLIENT_DLL
	virtual int		CapabilitiesGet( void );
	virtual	int		WeaponMeleeAttack1Condition( float flDot, float flDist );
#endif

protected:
	virtual	void	ImpactEffect( trace_t &trace );
	virtual void	ImpactSound( CBaseEntity *pHitEntity );

private:
	bool			ImpactWater( const Vector &start, const Vector &end );
	void			Swing( int bIsSecondary );
	void			Hit( trace_t &traceHit, Activity nHitActivity, bool bIsSecondary );
	Activity		ChooseIntersectionPointAndActivity( trace_t &hitTrace, const Vector &mins, const Vector &maxs, CBasePlayer *pOwner );
};

#endif