//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// TODO; Missile needs to be given its own class! Alot of other stuff
// needs it!
//=============================================================================//

#ifndef GRENADE_RPG_H
#define GRENADE_RPG_H

#ifdef _WIN32
#pragma once
#endif

#include "basegrenade_shared.h"
#include "Sprite.h"
#include "smoke_trail.h"

class CWeaponRPG;

#define SF_ROCKET_FLARE			(1 << 16)

//###########################################################################
//	>> CMissile		(missile launcher class is below this one!)
//###########################################################################
class CMissile : public CBaseGrenade
{
	DECLARE_CLASS( CMissile, CBaseGrenade );

public:
	static const int EXPLOSION_RADIUS = 216;

	CMissile();
	~CMissile();

#ifdef HL1_DLL
	Class_T Classify( void ) { return CLASS_NONE; }
#else
	Class_T Classify( void ) { return CLASS_MISSILE; }
#endif
	
	void	Spawn( void );
	void	Precache( void );
	void	MissileTouch( CBaseEntity *pOther );
	void	Explode( void );
	void	ShotDown( void );
	void	AccelerateThink( void );
	void	AugerThink( void );
	void	IgniteThink( void );
	void	SeekThink( void );
	void	KillEngine( void );
	void	SetGracePeriod( float flGracePeriod );

	int		OnTakeDamage_Alive( const CTakeDamageInfo &info );
	void	Event_Killed( const CTakeDamageInfo &info );
	
	virtual float	GetDamage() { return m_flDamage; }
	virtual void	SetDamage(float flDamage) { m_flDamage = flDamage; }
	virtual void	SetEngineLifetime(float flLifetime) { m_flEngineLifetime = gpGlobals->curtime + flLifetime; }
	virtual void	SetMaxLifetime(float flLifetime) { m_flMaxLifetime = gpGlobals->curtime + flLifetime; }

	unsigned int PhysicsSolidMaskForEntity( void ) const;

	CHandle<CWeaponRPG>		m_hOwner;

	static CMissile *Create( const Vector &vecOrigin, const QAngle &vecAngles, edict_t *pentOwner );

	void CreateDangerSounds( bool bState ){ m_bCreateDangerSounds = bState; }
	void GuidingDisabled( bool bState ){ m_bGuidingDisabled = bState; }

	static void AddCustomDetonator( CBaseEntity *pEntity, float radius, float height = -1 );
	static void RemoveCustomDetonator( CBaseEntity *pEntity );

protected:
	virtual void DoExplosion();	
	virtual void ComputeActualDotPosition( CLaserDot *pLaserDot, Vector *pActualDotPosition, float *pHomingSpeed );
	virtual int AugerHealth() { return m_iMaxHealth - 15; }

	// Creates the smoke trail
	void CreateSmokeTrail( void );
	void CreateFlare( void );

	// Gets the shooting position 
	void GetShootPosition( CLaserDot *pLaserDot, Vector *pShootPosition );

	CHandle<RocketTrail>	m_hRocketTrail;		// Smoke trail
	CHandle<CSprite>		m_hRocketFlare;		// Flare at the tip
//	float					m_flAugerTime;		// Amount of time to auger before blowing up anyway
	float					m_flMarkDeadTime;
	float					m_flDamage;

	struct CustomDetonator_t
	{
		EHANDLE hEntity;
		float radiusSq;
		float halfHeight;
	};

	static CUtlVector<CustomDetonator_t> gm_CustomDetonators;

private:
	float					m_flGracePeriodEndsAt;
	float					m_flEngineLifetime;
	float					m_flMaxLifetime;
	bool					m_bCreateDangerSounds;
	bool					m_bGuidingDisabled;

	DECLARE_DATADESC();
};

//-----------------------------------------------------------------------------
// Specialized mizzizzile
//-----------------------------------------------------------------------------
class CAPCMissile : public CMissile
{
	DECLARE_CLASS( CMissile, CMissile );
	DECLARE_DATADESC();

public:
	static CAPCMissile *Create( const Vector &vecOrigin, const QAngle &vecAngles, const Vector &vecVelocity, CBaseEntity *pOwner );

	CAPCMissile();
	~CAPCMissile();
	void	IgniteDelay( void );
	void	AugerDelay( float flDelayTime );
	void	ExplodeDelay( float flDelayTime );
	void	DisableGuiding();
#if defined( HL2_DLL )
	virtual Class_T Classify ( void ) { return CLASS_COMBINE; }
#endif

	void	AimAtSpecificTarget( CBaseEntity *pTarget );
	void	SetGuidanceHint( const char *pHintName );

	void	APCSeekThink( void );

	CAPCMissile			*m_pNext;

protected:
	virtual void DoExplosion();	
	virtual void ComputeActualDotPosition( CLaserDot *pLaserDot, Vector *pActualDotPosition, float *pHomingSpeed );
	virtual int AugerHealth();

private:
	void Init();
	void ComputeLeadingPosition( const Vector &vecShootPosition, CBaseEntity *pTarget, Vector *pLeadPosition );
	void BeginSeekThink();
	void AugerStartThink();
	void ExplodeThink();
	void APCMissileTouch( CBaseEntity *pOther );

	float	m_flReachedTargetTime;
	float	m_flIgnitionTime;
	float   m_flLastHomingSpeed;
	EHANDLE m_hSpecificTarget;
	string_t m_strHint;
};


//-----------------------------------------------------------------------------
// Finds apc missiles in cone
//-----------------------------------------------------------------------------
CAPCMissile *FindAPCMissileInCone( const Vector &vecOrigin, const Vector &vecDirection, float flAngle );


#endif // GRENADE_RPG_H
