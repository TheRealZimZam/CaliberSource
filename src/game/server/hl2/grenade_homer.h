//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Projectile shot by city scanner 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef	GRENADEHOMER_H
#define	GRENADEHOMER_H

#ifdef _WIN32
#pragma once
#endif

#include "basegrenade_shared.h"
#include "weapon_rpg.h"

enum HomerRocketTrail_t
{
	HOMER_SMOKE_TRAIL_OFF,			// No smoke trail
	HOMER_SMOKE_TRAIL_ON,			// Smoke trail always on
	HOMER_SMOKE_TRAIL_ON_HOMING,	// Smoke trail on when homing turned on
	HOMER_SMOKE_TRAIL_ALIEN,		// Alien colors on smoke trail
};

enum MissileType_t
{
	EXPLOSIVE,				// Default
	EXPLOSIVEINCENDIARY,	// Explode + Incendiary
	INCENDIARY,				// Incendiary Only
	ARMORPIERCING,			// Does full damage to hit entity, half to everything else
};

class CGrenadeHomer : public CBaseGrenade
{
public:
	DECLARE_CLASS( CGrenadeHomer, CBaseGrenade );

	static CGrenadeHomer* CreateGrenadeHomer(  string_t nModelName, string_t sFlySound, const Vector &vecOrigin, const QAngle &vecAngles, edict_t *pentOwner );

	virtual void Precache( void );
	void		Spawn( void );
	void		Launch( CBaseEntity *pOwner, CBaseEntity *pTarget, const Vector &vInitVelocity, float m_flHomingSpeed, float fFallSpeed, int nRocketTrailType);
	void		SetSpin(float flSpinMagnitude, float flSpinSpeed);
	void		SetHoming(float flStrength, float flDelay, float flRampUp, float flDuration, float flRampDown);

	void		SetRocketType(int iRocketType) { m_nRocketType = iRocketType; }

	CHandle<RocketTrail>	m_hRocketTrail[3];

private:
	string_t	m_sFlySound;
//	float		m_flNextFlySoundTime;
	float		m_flNextWarnTime;

	// Input Parameters
	float		m_flHomingStrength;
	float		m_flHomingDelay;				// How long before homing starts
	float		m_flHomingRampUp;				// How long it take to reach full strength
	float		m_flHomingDuration;				// How long does homing last
	float		m_flHomingRampDown;				// How long to reach no homing again
	float		m_flHomingSpeed;
	float		m_flSpinMagnitude;
	float		m_flSpinSpeed;
	int			m_nRocketTrailType;
	int			m_nRocketType;
	int			m_spriteTexture;

	// In flight data
	float		m_flHomingLaunchTime;
	float		m_flHomingStartTime;
	float		m_flHomingEndTime;
	float		m_flSpinOffset;				// For randomization

	EHANDLE		m_hTarget;

	void		AimThink( void );
	void		StartRocketTrail(bool bTrail1 = true, bool bTrail2 = true, bool bTrail3 = true);
	void		UpdateRocketTrail(float fScale);
	void		StopRocketTrail(void);
	void		StopCosmeticTrails(bool bTrail1 = true, bool bTrail2 = true);
	void		PlayFlySound( void );
	void 		GrenadeHomerTouch( CBaseEntity *pOther );
	void		Event_Killed( const CTakeDamageInfo &info );
	int			OnTakeDamage( const CTakeDamageInfo &info );

public:
	void EXPORT				Detonate( bool bToSpace = false );
	CGrenadeHomer(void);

	DECLARE_DATADESC();
};

#endif	//GRENADEHOMER_H
