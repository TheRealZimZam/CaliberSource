//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Projectile shot from the AR2 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef	WEAPONAR2_H
#define	WEAPONAR2_H

#include "basegrenade_shared.h"
#include "basehlcombatweapon_shared.h"

#ifdef CLIENT_DLL
#define CWeaponAR2 C_WeaponAR2
#endif
class CWeaponAR2 : public CHLMachineGun
{
public:
	DECLARE_CLASS( CWeaponAR2, CHLMachineGun );

	CWeaponAR2();

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	void	ItemPostFrame( void );
	void	Precache( void );
	virtual void Equip( CBaseCombatCharacter *pOwner );
	bool	IsWeaponZoomed() { return m_bZoomed; }

	void	PrimaryAttack( void );
	void	SecondaryAttack( void );

	const char *GetTracerType( void ) { return "AR2Tracer"; }
	void	DoImpactEffect( trace_t &tr, int nDamageType );

	void	AddViewKick( void );

	int		GetMinBurst( void )
	{
		if( IsWeaponZoomed() )
			return 2;

		return 4;
	}
	int		GetMaxBurst( void )
	{
		if( IsWeaponZoomed() )
			return 3;

		return 5;
	}

	float	GetFireRate( void )
	{
		if ( IsWeaponZoomed() )
			return BaseClass::GetCycleTime() * 2.0;

		return BaseClass::GetCycleTime();
	}
	float	GetDefaultAnimSpeed( void );

	bool	CanHolster( void );
	bool	Reload( void );

	Activity	GetPrimaryAttackActivity( void );

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;
		if( GetOwner() && IsWeaponZoomed() )
		{
			if ( GetOwner()->IsPlayer() )
				cone = VECTOR_CONE_1DEGREES;
			else
				cone = VECTOR_CONE_2DEGREES;

			return cone;
		}

		return BaseClass::GetBulletSpread();
	}

	const WeaponProficiencyInfo_t *GetProficiencyValues();

#ifndef CLIENT_DLL
	virtual float	GetMinRestTime();
	virtual float	GetMaxRestTime();
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	void	FireNPCAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
//	void	FireNPCSecondaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	void	Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary );
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
#endif

	DECLARE_ACTTABLE();

protected:
	CNetworkVar( bool, m_bShotDelayed );

	void			Zoom( void );
	CNetworkVar( bool, m_bZoomed );

	int				m_nVentPose;
};


#endif	//WEAPONAR2_H
