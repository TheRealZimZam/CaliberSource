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

#include "basegrenade_shared.h"

enum SpitType_e
{
	FIRE,
	ACID,
	SPIT,
};

enum SpitSize_e
{
	SMALL,
	MEDIUM,
	LARGE,
};

#define FLAME_GRAVITY 400
#define ACID_GRAVITY 550
#define SPIT_GRAVITY 650

class SmokeTrail;
class CSprite;

abstract_class CGrenadeBall : public CBaseGrenade
{
	DECLARE_CLASS( CGrenadeBall, CBaseGrenade );
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
	void				Think( void );
	void				InitHissSound( void );

	CHandle<SmokeTrail>	m_hSmokeTrail;
	CHandle<CSprite>	m_pSquidSpitSprite;

private:
	float			m_flSpitLifeTime;		// If non-zero won't despawn
	float			m_flSpitGroundLifeTime;	// How long to sizzle on the ground
	float			m_flNextDamageTime;

	int 			nSpitType;
	int 			m_iMaxSplatDecals;	//stop decaling surfaces after this many touches

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
