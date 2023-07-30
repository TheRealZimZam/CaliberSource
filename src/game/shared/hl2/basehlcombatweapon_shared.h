//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef BASEHLCOMBATWEAPON_SHARED_H
#define BASEHLCOMBATWEAPON_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "baseplayer_shared.h"
#include "hl2_player_shared.h"
#include "basecombatweapon_shared.h"

#if defined( CLIENT_DLL )
	#define CBaseHLCombatWeapon C_BaseHLCombatWeapon
	void UTIL_ClipPunchAngleOffset( QAngle &in, const QAngle &punch, const QAngle &clip );
#endif
class CBaseHLCombatWeapon : public CBaseCombatWeapon
{
#if !defined( CLIENT_DLL )
#ifndef _XBOX
	DECLARE_DATADESC();
#else
protected:
	DECLARE_DATADESC();
private:
#endif
#endif

	DECLARE_CLASS( CBaseHLCombatWeapon, CBaseCombatWeapon );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	// All predicted weapons need to implement and return true
	virtual bool	IsPredicted() const;
#ifdef CLIENT_DLL
	bool ShouldPredict();
	void OnDataChanged( DataUpdateType_t type );
#endif

	virtual bool	WeaponShouldBeLowered( void );

	virtual bool	CanLower();
	virtual bool	Ready( void );
	virtual bool	Lower( void );
	virtual bool	Deploy( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void	WeaponIdle( void );
	virtual void	DryFire( void );

	// FIXME: why are these virtual?
	virtual float	SequenceDuration( int iSequence );
	virtual float	SequenceDuration( void ) { return SequenceDuration( GetSequence() ); }

	virtual void	AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles );
	virtual	float	CalcViewmodelBob( void );

	virtual Vector	CalculateBulletSpread( WeaponProficiency_t proficiency );
	virtual Vector	GetBulletSpread( WeaponProficiency_t proficiency );		// Calculated spread
	virtual const Vector &GetBulletSpread( void );							// Base weapon spread

	virtual float	GetSpreadBias( WeaponProficiency_t proficiency );

	virtual const	WeaponProficiencyInfo_t *GetProficiencyValues();
	static const	WeaponProficiencyInfo_t *GetDefaultProficiencyValues();

	virtual void	ItemHolsterFrame( void );

	int				m_iPrimaryAttacks;		// # of primary attacks performed with this weapon
	int				m_iSecondaryAttacks;	// # of secondary attacks performed with this weapon

protected:
	bool			m_bLowered;				// Whether the viewmodel is raised or lowered
	float			m_flRaiseTime;			// If lowered, the time we should raise the viewmodel
	float			m_flHolsterTime;		// When the weapon was holstered
	float		m_flPrevAnimTime;	//DIRTY HACK
};

//=========================================================
// Machine gun base class
//=========================================================
#if defined( CLIENT_DLL )
#define CHLMachineGun C_HLMachineGun
#endif
class CHLMachineGun : public CBaseHLCombatWeapon
{
public:
	DECLARE_CLASS( CHLMachineGun, CBaseHLCombatWeapon );

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CHLMachineGun();

	void	PrimaryAttack( void );

	// Default calls through to m_hOwner, but plasma weapons can override and shoot projectiles here.
	virtual void	ItemPostFrame( void );
	virtual void	FireBullets( const FireBulletsInfo_t &info );
	virtual float	GetFireRate( void ) { return BaseClass::GetCycleTime(); }
	virtual bool	Deploy( void );

	virtual const Vector &GetBulletSpread( void );

	int				WeaponSoundRealtime( WeaponSound_t shoot_type );
	Activity		GetPrimaryAttackActivity( void );

	// utility function
	static void DoMachineGunKick( CBasePlayer *pPlayer, float dampEasy, float maxVerticleKickAngle, float fireDurationTime, float slideLimitTime );

#ifndef CLIENT_DLL
	virtual int		WeaponRangeAttack1Condition( float flDot, float flDist );
	DECLARE_DATADESC();
#endif

protected:
	int	m_nShotsFired;	// Number of consecutive shots fired
	float	m_flNextSoundTime;	// real-time clock of when to make next sound
};

//=========================================================
// Machine guns capable of switching between full auto and 
// burst fire modes.
//=========================================================
// Mode settings for select fire weapons
enum
{
	FIREMODE_FULLAUTO = 1,
	FIREMODE_SEMI,
	FIREMODE_3RNDBURST,
};

//=========================================================
//	>> CHLSelectFireMachineGun
//=========================================================
#if defined( CLIENT_DLL )
#define CHLSelectFireMachineGun C_HLSelectFireMachineGun
#endif
class CHLSelectFireMachineGun : public CHLMachineGun
{
	DECLARE_CLASS( CHLSelectFireMachineGun, CHLMachineGun );
public:

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CHLSelectFireMachineGun();

	virtual float	GetBurstCycleRate( void );
	virtual float	GetFireRate( void );

	virtual bool	Deploy( void );
	virtual void	WeaponSound( WeaponSound_t shoot_type, float soundtime = 0.0f );

	virtual int		GetBurstSize( void ) { return 3; };

	void			ChangeFireMode( void );
	void			BurstThink( void );

	virtual void	PrimaryAttack( void );
	virtual void	SecondaryAttack( void );
	virtual void	TertiaryAttack( void );

#ifndef CLIENT_DLL
	virtual int		WeaponRangeAttack1Condition( float flDot, float flDist );
	virtual int		WeaponRangeAttack2Condition( float flDot, float flDist );
	DECLARE_DATADESC();
#endif
protected:
	int m_iBurstSize;
	int	m_iFireMode;
};

#endif // BASEHLCOMBATWEAPON_SHARED_H
