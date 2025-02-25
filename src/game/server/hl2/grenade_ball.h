//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Fire/Acid/Spitball
//
// $Workfile:     $
// $Date:         $
//=============================================================================//

#ifndef	GRENADE_BALL_H
#define	GRENADE_BALL_H
#ifdef _WIN32
#pragma once
#endif

#include "basespriteprojectile.h"

#define FLAME_GRAVITY 250	//Fire has the lowest gravity, try to go in a straightish line
#define ACID_GRAVITY 400	// Acid drops fast
#define SPIT_GRAVITY 650	// Spits drops fastest, but is fired in a arc

#define BALL_MAX_DECALS 2	// Maximum amount of decals a single ball can create

class SmokeTrail;

class CGrenadeBall : public CBaseSpriteProjectile
{
	DECLARE_CLASS( CGrenadeBall, CBaseSpriteProjectile );
	DECLARE_DATADESC();

public:
	CGrenadeBall( void );

	virtual void		Precache( void );
	void				Spawn(	char *pszModel,
									int iSize,
									const Vector &vecVelocity,
									CBaseEntity *pOwner,
									MoveType_t	iMovetype,
									MoveCollide_t nMoveCollide,
									int	iGravity,
									CBaseEntity *pIntendedTarget = NULL,
									float fLifetime = -1 );
	virtual void		Event_Killed( const CTakeDamageInfo &info );

//	virtual	unsigned int	PhysicsSolidMaskForEntity( void ) const { return ( BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_WATER ); }

	void 				Touch( CBaseEntity *pOther );
	void				FlyThink( void );

	virtual void		SetLife( float fLifetime ) { m_flSpitLifeTime = fLifetime; }
	virtual void		SetGroundLife( float fGLifetime ) { m_flSpitGroundLifeTime = fGLifetime; }
	virtual void		SetDamage( int iDamage ) { m_flDamage = iDamage; }
	virtual void		SetDamageRadius( int iDamageRadius ) { m_DmgRadius = iDamageRadius; }

	void				SetHissSound( const char *pszSound ) { m_iszHissSound = MAKE_STRING(pszSound); }
	void				SetHitSound( const char *pszSound ) { m_iszHitSound = MAKE_STRING(pszSound); }

	void				Detonate( void );
	void				InitHissSound( void );

	CHandle<SmokeTrail>	m_hSmokeTrail;

private:
	float			m_flSpitLifeTime;		// If non-zero won't despawn
	float			m_flSpitGroundLifeTime;	// How long to sizzle on the ground
	float			m_flNextDamageTime;

	int 			nSpitType;
	int 			m_iSplatDecals;	//stop decaling surfaces after this many touches

	string_t		m_iszHissSound;
	string_t		m_iszHitSound;
	bool			m_bHitGround;
};

#endif	//GRENADE_BALL_H
