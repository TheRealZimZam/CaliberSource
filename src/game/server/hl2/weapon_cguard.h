//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#ifndef WEAPON_CGUARD_H
#define WEAPON_CGUARD_H
#ifdef _WIN32
#pragma once
#endif

#include "basehlcombatweapon.h"
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

// Singleton to fire TEExplosion objects
static CTEConcussiveExplosion g_TEConcussiveExplosion( "ConcussiveExplosion" );

void TE_ConcussiveExplosion( IRecipientFilter& filter, float delay,
	const Vector* pos, float scale, int radius, int magnitude, const Vector* normal )
{
	g_TEConcussiveExplosion.m_vecOrigin		= *pos;
	g_TEConcussiveExplosion.m_flScale			= scale;
	g_TEConcussiveExplosion.m_nRadius			= radius;
	g_TEConcussiveExplosion.m_nMagnitude		= magnitude;

	if ( normal )
		g_TEConcussiveExplosion.m_vecNormal	= *normal;
	else 
		g_TEConcussiveExplosion.m_vecNormal	= Vector(0,0,1);

	// Send it over the wire
	g_TEConcussiveExplosion.Create( filter, delay );
}

//Temp ent for the blast
class CConcussiveBlast : public CBaseEntity
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( CConcussiveBlast, CBaseEntity );

	int		m_spriteTexture;

	CConcussiveBlast( void ) {}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Output :
	//-----------------------------------------------------------------------------
	void Precache( void )
	{
		m_spriteTexture = PrecacheModel( "sprites/lgtning.vmt" );

		BaseClass::Precache();
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Output :
	//-----------------------------------------------------------------------------

	void Explode( float magnitude )
	{
		//Create a concussive explosion
		CPASFilter filter( GetAbsOrigin() );

		Vector vecForward;
		AngleVectors( GetAbsAngles(), &vecForward );
		TE_ConcussiveExplosion( filter, 0.0,
			&GetAbsOrigin(),//position
			1.0f,	//scale
			256*magnitude,	//radius
			175*magnitude,	//magnitude
			&vecForward );	//normal
		
		int	colorRamp = random->RandomInt( 128, 255 );

		//Shockring
		CBroadcastRecipientFilter filter2;
		te->BeamRingPoint( filter2, 0, 
			GetAbsOrigin(),	//origin
			16,			//start radius
			300*magnitude,		//end radius
			m_spriteTexture, //texture
			0,			//halo index
			0,			//start frame
			2,			//framerate
			0.3f,		//life
			128,		//width
			16,			//spread
			0,			//amplitude
			colorRamp,	//r
			colorRamp,	//g
			255,		//g
			24,			//a
			128			//speed
			);

		//Do the radius damage
		//RadiusDamage( CTakeDamageInfo( this, GetOwnerEntity(), 200, DMG_BLAST|DMG_DISSOLVE ), GetAbsOrigin(), 256, CLASS_NONE, NULL );

		UTIL_Remove( this );
	}
};

//-----------------------------------------------------------------------------
// Purpose: Create a concussive blast entity and detonate it
//-----------------------------------------------------------------------------
void CreateConcussiveBlast( const Vector &origin, const Vector &surfaceNormal, CBaseEntity *pOwner, float magnitude )
{
	QAngle angles;
	VectorAngles( surfaceNormal, angles );
	CConcussiveBlast *pBlast = (CConcussiveBlast *) CBaseEntity::Create( "concussiveblast", origin, angles, pOwner );

	if ( pBlast )
	{
		pBlast->Explode( magnitude );
	}
}


class CWeaponCGuard : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( CWeaponCGuard, CBaseHLCombatWeapon );

	DECLARE_SERVERCLASS();

	CWeaponCGuard( void );
	
	void Precache( void );
	void PrimaryAttack( void );
	void AddViewKick( void );
	void DelayedFire( void );
	void ItemPostFrame( void );
	void AlertTargets( void );
	void UpdateLasers( void );

	int CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	DECLARE_ACTTABLE();

protected:
	float	m_flChargeTime;
	bool	m_bFired;

	int		m_beamIndex;
	int		m_haloIndex;
};

#endif // WEAPON_CGUARD_H