//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GRENADE_CONCUSSIVE_H
#define GRENADE_CONCUSSIVE_H
#ifdef _WIN32
#pragma once
#endif

#include "basegrenade_shared.h"
#include "te_particlesystem.h"

//Concussive explosion entity
class CTEConcussiveExplosion : public CTEParticleSystem
{
public:
	DECLARE_CLASS( CTEConcussiveExplosion, CTEParticleSystem );
	DECLARE_SERVERCLASS();

	CTEConcussiveExplosion( const char *name );
	virtual	~CTEConcussiveExplosion( void );

	CNetworkVector( m_vecNormal );
	CNetworkVar( float, m_flScale );
	CNetworkVar( int, m_nRadius );
	CNetworkVar( int, m_nMagnitude );
};

//Temp ent for the blast
class CConcussiveBlast : public CBaseEntity
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( CConcussiveBlast, CBaseEntity );

	int		m_spriteTexture;

	CConcussiveBlast( void ) {}

	void Precache( void )
	{
		m_spriteTexture = PrecacheModel( "sprites/lgtning.vmt" );
		PrecacheModel( "sprites/blueflare1.vmt" );

		BaseClass::Precache();
	}

	//-----------------------------------------------------------------------------
//	void CreateConcussiveBlast( const Vector &origin, const Vector &surfaceNormal, CBaseEntity *pOwner, float magnitude );
	void Explode( float magnitude );
};


//-----------------------------------------------------------------------------
// Purpose: CGrenadeConcussive
//-----------------------------------------------------------------------------
class CGrenadeConcussive : public CBaseGrenade
{
	DECLARE_CLASS( CGrenadeConcussive, CBaseGrenade );

public:
	void	Precache( void );
	void	Spawn( void );

//	bool	CreateVPhysics( void );
	void		Event_Killed( const CTakeDamageInfo &info );

private:
	bool	m_inSolid;	//If im stuck in a solid dont do any damage

public:
	void EXPORT				Detonate(void);
	CGrenadeConcussive(void);

	DECLARE_DATADESC();
};

#endif // GRENADE_CONCUSSIVE_H
