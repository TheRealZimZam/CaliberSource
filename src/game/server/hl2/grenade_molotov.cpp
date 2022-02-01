//=========== (C) Copyright 2000 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Flaming bottle thrown from the hand
//
// TODO; Attach a flamesprite to the top of the bottle
//=============================================================================

#include "cbase.h"
#include "player.h"
#include "ammodef.h"
#include "gamerules.h"
#include "grenade_molotov.h"
#include "weapon_brickbat.h"
#include "soundent.h"
#include "decals.h"
#include "fire.h"
#include "ndebugoverlay.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"

extern short	g_sModelIndexFireball;

extern ConVar    sk_plr_dmg_molotov;
extern ConVar    sk_npc_dmg_molotov;
//This is the radius of the fire, not the explosion
ConVar    sk_molotov_radius			( "sk_molotov_radius","128");
ConVar    sk_molotov_fire_radius	( "sk_molotov_fire_radius","200");
ConVar    sk_molotov_fire_time		( "sk_molotov_fire_time","30");

#define MOLOTOV_EXPLOSION_VOLUME	1024

BEGIN_DATADESC( CGrenadeMolotov )

	DEFINE_FIELD( m_pFireTrail, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_fSpawnTime, FIELD_TIME ),

	// Function Pointers
	DEFINE_ENTITYFUNC( MolotovTouch ),
	DEFINE_THINKFUNC( MolotovThink ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( grenade_molotov, CGrenadeMolotov );

void CGrenadeMolotov::Spawn( void )
{
	Precache( );

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetSolid( SOLID_BBOX );
	SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
//!	AddEffects( EF_NOINTERP );

	SetModel( "models/weapons/w_molotov.mdl");

	UTIL_SetSize(this, Vector( -6, -6, -2), Vector(6, 6, 2));

	SetTouch( &CGrenadeMolotov::MolotovTouch );
	SetThink( &CGrenadeMolotov::MolotovThink );
	SetNextThink( gpGlobals->curtime + 0.1f );

	m_flDamage		= sk_plr_dmg_molotov.GetFloat();
	m_DmgRadius		= sk_molotov_radius.GetFloat();

	m_takedamage	= DAMAGE_YES;
	m_iHealth		= 1;
	m_fSpawnTime	= gpGlobals->curtime;

	SetGravity( 1.0 );
	SetFriction( 0.8 );  // Give a little bounce so can flatten
	SetSequence( 1 );

	m_pFireTrail = SmokeTrail::CreateSmokeTrail();
		
	if( m_pFireTrail )
	{
		m_pFireTrail->m_SpawnRate = 48;
		m_pFireTrail->m_ParticleLifetime = 1;
		m_pFireTrail->m_StartColor.Init(0.2f, 0.2f, 0.2f);
		m_pFireTrail->m_EndColor.Init(0,0,0);
		m_pFireTrail->m_StartSize = 8;
		m_pFireTrail->m_EndSize = m_pFireTrail->m_StartSize * 4;
		m_pFireTrail->m_SpawnRadius = 4;
		m_pFireTrail->m_MinSpeed = 4;
		m_pFireTrail->m_MaxSpeed = 24;
		m_pFireTrail->m_Opacity = 0.25f;

		m_pFireTrail->SetLifetime(20.0f);
		m_pFireTrail->FollowEntity(this);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CGrenadeMolotov::MolotovTouch( CBaseEntity *pOther )
{
	// If I'm live go ahead and blow up
	if (m_bIsLive)
	{
		Detonate();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeMolotov::Detonate( void ) 
{
	SetModelName( NULL_STRING );		//invisible
	AddSolidFlags( FSOLID_NOT_SOLID );	// intangible

	m_takedamage = DAMAGE_NO;

	trace_t trace;
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + Vector ( 0, 0, -128 ),  MASK_SOLID_BRUSHONLY, 
		this, COLLISION_GROUP_NONE, &trace);

	// Pull out of the wall a bit
	if ( trace.fraction != 1.0 )
	{
		SetLocalOrigin( trace.endpos + (trace.plane.normal * (m_flDamage - 24) * 0.6) );
	}

	if ( GetWaterLevel() > 1 )
	{
		UTIL_Bubbles( GetAbsOrigin(), GetAbsOrigin(), 3 );
		UTIL_Remove( this );
		return;
	}

	EmitSound( "GrenadeMolotov.Detonate");

// Start some fires
	float lifetime = sk_molotov_fire_time.GetFloat();
	int i;
	QAngle vecTraceAngles;
	Vector vecTraceDir;
	trace_t firetrace;

	for( i = 0 ; i < 16 ; i++ )
	{
		// build a little ray - a teeny tiny little ray
		vecTraceAngles[PITCH]	= random->RandomFloat(45, 135);
		vecTraceAngles[YAW]		= random->RandomFloat(0, 360);
		vecTraceAngles[ROLL]	= 0.0f;

		AngleVectors( vecTraceAngles, &vecTraceDir );

		Vector vecStart, vecEnd;

		vecStart = GetAbsOrigin() + ( trace.plane.normal * 128 );
		vecEnd = vecStart + vecTraceDir * 512;

		UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &firetrace );

		Vector	ofsDir = ( firetrace.endpos - GetAbsOrigin() );
		float	offset = VectorNormalize( ofsDir );

		if ( offset > 128 )
			offset = 128;

		//Get our scale based on distance
		float scale	 = 32.0f + ( 0.75f * ( 1.0f - ( offset / 128.0f ) ) );
		float growth = 0.1f + ( 0.75f * ( offset / 128.0f ) );

		if( firetrace.fraction != 1.0 )
		{
			FireSystem_StartFire( firetrace.endpos, scale, growth, lifetime, (SF_FIRE_START_ON|SF_FIRE_SMOKELESS|SF_FIRE_NO_GLOW), (CBaseEntity*) this, FIRE_NATURAL );
		}
	}
	// End Start some fires

	// Stock explosion effect works fine without smoke
	CPASFilter filter2( trace.endpos );

	te->Explosion( filter2, 0.0,
		&trace.endpos, 
		g_sModelIndexFireball,
		4.0, 
		15,
		TE_EXPLFLAG_NOPARTICLES| TE_EXPLFLAG_NOSOUND,
		m_DmgRadius,
		m_flDamage );

	CBaseEntity *pOwner;
	pOwner = GetOwnerEntity();
	SetOwnerEntity( NULL ); // can't traceline attack owner if this is set

	UTIL_DecalTrace( &trace, "Scorch" );

	CSoundEnt::InsertSound( SOUND_DANGER, GetAbsOrigin(), BASEGRENADE_EXPLOSION_VOLUME, 3.0 );

	//Ignite entities
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = gEntList.FindEntityInSphere( pEntity, GetAbsOrigin(), m_DmgRadius ) ) != NULL )
	{
		CBaseAnimating *pAnim;
		pAnim = dynamic_cast<CBaseAnimating*>(pEntity);
		if( pAnim )
		{
			pAnim->Ignite( lifetime * 0.5f );
		}
	}
	RadiusDamage( CTakeDamageInfo( this, pOwner, m_flDamage, DMG_BURN ), GetAbsOrigin(), m_DmgRadius, CLASS_NONE, NULL );

	RemoveEffects( EF_NODRAW );
	SetAbsVelocity( vec3_origin );
	SetNextThink( gpGlobals->curtime + 0.2 );

	if ( m_pFireTrail )
	{
		UTIL_Remove( m_pFireTrail );
	}

	UTIL_Remove(this);
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CGrenadeMolotov::MolotovThink( void )
{
	if (!m_bIsLive)
	{
		// Go live after a short delay
		if (m_fSpawnTime + MOLOTOV_NO_COLLIDE_TIME < gpGlobals->curtime)
		{
			m_bIsLive  = true;
		}
	}

	// See if I can lose my owner (has dropper moved out of way?)
	// Want do this so owner can throw the brickbat
	if (GetOwnerEntity())
	{
		trace_t tr;
		Vector	vUpABit = GetAbsOrigin();
		vUpABit.z += 5.0;

		CBaseEntity* saveOwner	= GetOwnerEntity();
		SetOwnerEntity( NULL );
		UTIL_TraceEntity( this, GetAbsOrigin(), vUpABit, MASK_SOLID, &tr );
		if ( tr.startsolid || tr.fraction != 1.0 )
		{
			SetOwnerEntity( saveOwner );
		}
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CGrenadeMolotov::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel("models/weapons/w_molotov.mdl");

	UTIL_PrecacheOther("_firesmoke");
	
	PrecacheScriptSound( "GrenadeMolotov.Detonate" );
}

