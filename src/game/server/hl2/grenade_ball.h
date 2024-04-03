//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Fire/Acid/Spitball
//
// $Workfile:     $
// $Date:         $
//=============================================================================//

#ifndef	GRENADEBALL_H
#define	GRENADEBALL_H
#ifdef _WIN32
#pragma once
#endif

//#include "cbasespriteprojectile.h"
#include "baseprojectile.h"

enum SpitType_e
{
	FIRE,
	ACID,
	SPIT,
};

enum SpitSize_e
{
	SMALL,	//Imp fireballs, small flamelets
	MEDIUM,	//Fireclouds, flamethrower, explosions
	LARGE,	//Big explosions,
};

#define FLAME_GRAVITY 250	//Fire has the lowest gravity, try to go in a straightish line
#define ACID_GRAVITY 400	// Acid drops fast
#define SPIT_GRAVITY 650	// Spits drops fastest, but is fired in a arc

#define BALL_MAX_DECALS 2	// Maximum amount of decals a single ball can create

class SmokeTrail;
class CSprite;

abstract_class CGrenadeBall : public CBaseProjectile
{
	DECLARE_CLASS( CGrenadeBall, CBaseProjectile );
	DECLARE_DATADESC();

public:
	CGrenadeBall( void );

	virtual void		Spawn( void );
	virtual void		Precache( void );
	virtual void		Event_Killed( const CTakeDamageInfo &info );

//	virtual	unsigned int	PhysicsSolidMaskForEntity( void ) const { return ( BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_WATER ); }

	void 				GrenadeSpitTouch( CBaseEntity *pOther );
	void				SetSpitType( int nType, int nSize );
	void				Detonate( void );
	void				FlyThink( void );
	void				InitHissSound( void );

	CHandle<SmokeTrail>	m_hSmokeTrail;
	CHandle<CSprite>	m_pSquidSpitSprite;

private:
	float			m_flSpitLifeTime;		// If non-zero won't despawn
	float			m_flSpitGroundLifeTime;	// How long to sizzle on the ground
	float			m_flNextDamageTime;

	int 			nSpitType;
	int 			m_iSplatDecals;	//stop decaling surfaces after this many touches

	CSoundPatch		*m_pHissSound;
	const char 		*m_pHitSound;
	bool			m_bPlaySound;
	bool			m_bHitGround;
};

//=============================================================================
// Purpose:	Spawn helpers - just create a certain type
// Code-created balls should still use the baseclass and manually assign a type
//=============================================================================
class CGrenadeFireball : public CGrenadeBall
{
	DECLARE_CLASS( CGrenadeFireball, CGrenadeBall );
public:
	void Spawn( void );
};

//-----------------------------------------------------------------------------
class CGrenadeAcid : public CGrenadeBall
{
	DECLARE_CLASS( CGrenadeAcid, CGrenadeBall );
public:
	void Spawn( void );
};

//-----------------------------------------------------------------------------
class CGrenadeSpit : public CGrenadeBall
{
	DECLARE_CLASS( CGrenadeSpit, CGrenadeBall );
public:
	void Spawn( void );
};

#endif	//GRENADEBALL_H
