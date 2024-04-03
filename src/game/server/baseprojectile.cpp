//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 	Base class for simple projectiles
//
//=============================================================================

#include "cbase.h"
#include "baseprojectile.h"

BEGIN_DATADESC( CBaseProjectile )
	DEFINE_FIELD( m_iDamageType, FIELD_INTEGER ),
	DEFINE_FIELD( m_flDamageScale, FIELD_FLOAT ),

	DEFINE_ENTITYFUNC( ProjectileTouch ),
	DEFINE_THINKFUNC( FlyThink ),
END_DATADESC()

//LINK_ENTITY_TO_CLASS( proj_base, CBaseProjectile );

//-----------------------------------------------------------------------------
// Purpose: 
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
	SetGravity( 0.001f );
	m_takedamage = DAMAGE_NO;

	// Setup the touch and think functions.
	SetTouch( &CBaseProjectile::ProjectileTouch );
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
	return BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseProjectile::ProjectileTouch( CBaseEntity *pOther )
{
	// Verify a correct "other."
	Assert( pOther );
	if ( !pOther->IsSolid() || pOther->IsSolidFlagSet( FSOLID_VOLUME_CONTENTS ) )
		return;

	// Handle hitting skybox (disappear).
	const trace_t *pTrace = &CBaseEntity::GetTouchTrace();
	trace_t *pNewTrace = const_cast<trace_t*>( pTrace );

	if( pTrace->surface.flags & SURF_SKY )
	{
		UTIL_Remove( this );
		return;
	}

	CTakeDamageInfo info;
	info.SetAttacker( GetOwnerEntity() );
	info.SetInflictor( this );
	info.SetDamage( GetDamage() );
	info.SetDamageType( GetDamageType() );
	GuessDamageForce( &info, GetAbsVelocity(), GetAbsOrigin(), GetDamageScale() );

	Vector dir;
	AngleVectors( GetAbsAngles(), &dir );

	pOther->DispatchTraceAttack( info, dir, pNewTrace );
	ApplyMultiDamage();

	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose: Orient the projectile along its velocity
//-----------------------------------------------------------------------------
void CBaseProjectile::FlyThink( void )
{
	QAngle angles;
	VectorAngles( -(GetAbsVelocity()), angles );
	SetAbsAngles( angles );
	SetNextThink( gpGlobals->curtime + 0.1f );
}
