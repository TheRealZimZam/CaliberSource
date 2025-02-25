//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 	Base class for projectiles
// Does some basic lifting and provides helper functions for launched projectiles
// (rockets, fireballs, launched grenades, etc.)
// Thrown grenades/slow moving physical projectiles should still use CBaseGrenade
//
//=============================================================================

#include "cbase.h"
#include "baseprojectile.h"

BEGIN_DATADESC( CBaseProjectile )
	DEFINE_FIELD( m_iDamageType, FIELD_INTEGER ),
	DEFINE_FIELD( m_hIntendedTarget, FIELD_EHANDLE ),

	DEFINE_THINKFUNC( FlyThink ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( baseprojectile, CBaseProjectile );

//-----------------------------------------------------------------------------
// Purpose: This is usually overridden by subclass
//-----------------------------------------------------------------------------
void CBaseProjectile::Spawn( void )
{
	Precache();

	SetModel( STRING( GetModelName() ) );

	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
	AddFlag( FL_OBJECT );

	UTIL_SetSize( this, -Vector( 1.0f, 1.0f, 1.0f ), Vector( 1.0f, 1.0f, 1.0f ) );

	// Setup attributes.
	SetGravity( 0.01f );
	m_takedamage = DAMAGE_NO;

	// Setup the touch and think functions.
	SetTouch( &CBaseProjectile::Touch );
	SetThink( &CBaseProjectile::FlyThink );
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseProjectile::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( STRING( GetModelName() ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
unsigned int CBaseProjectile::PhysicsSolidMaskForEntity( void ) const
{ 
	return (BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseProjectile::Touch( CBaseEntity *pOther )
{
	// Verify a correct "other."
	Assert( pOther );
	if ( !pOther->IsSolid() || pOther->IsSolidFlagSet( FSOLID_VOLUME_CONTENTS ) )
		return;

	// Handle hitting skybox (disappear).
	const trace_t *pTrace = &CBaseEntity::GetTouchTrace();
	if( pTrace->surface.flags & SURF_SKY )
	{
		UTIL_Remove( this );
		return;
	}

	// Did i hit my intended target?
	if ( pOther == m_hIntendedTarget )
	{
		Detonate();
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Orient the projectile along its velocity
//-----------------------------------------------------------------------------
void CBaseProjectile::FlyThink( void )
{
	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	QAngle angles;
	VectorAngles( -(GetAbsVelocity()), angles );
	SetAbsAngles( angles );
	SetNextThink( gpGlobals->curtime + 0.1f );
}
