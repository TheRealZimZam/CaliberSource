//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "grenade_shrapnel.h"
#include "soundent.h"
#include "decals.h"
#include "smoke_trail.h"
#include "hl2_shareddefs.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "soundenvelope.h"
#include "ai_utils.h"
#include "te_effect_dispatch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar    sk_shrapnel_dmg		( "sk_shrapnel_dmg","0");

LINK_ENTITY_TO_CLASS( grenade_shrapnel, CGrenadeShrapnel );

BEGIN_DATADESC( CGrenadeShrapnel )

	DEFINE_FIELD( m_bPlaySound, FIELD_BOOLEAN ),

	// Function pointers
	DEFINE_ENTITYFUNC( GrenadeShrapnelTouch ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  :
// Output :
//-----------------------------------------------------------------------------
CGrenadeShrapnel::CGrenadeShrapnel(void)
{

}

//-----------------------------------------------------------------------------
void CGrenadeShrapnel::Precache( void )
{
	// m_nSquidSpitSprite = PrecacheModel("sprites/greenglow1.vmt");// client side spittle.

	PrecacheModel("models/Weapons/w_shrapnel.mdl");
	PrecacheModel("models/Weapons/shrapnel.mdl");

	PrecacheScriptSound( "GrenadeShrapnel.Fuse" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeShrapnel::Spawn( void )
{
	Precache( );
	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetSolidFlags( FSOLID_NOT_STANDABLE );
	SetCollisionGroup( COLLISION_GROUP_WEAPON );

	SetModel( "models/Weapons/w_shrapnel.mdl" );
	UTIL_SetSize( this, vec3_origin, vec3_origin );

	SetUse( &CBaseGrenade::DetonateUse );
	SetTouch( &CGrenadeShrapnel::GrenadeShrapnelTouch );
	SetNextThink( gpGlobals->curtime + 0.1f );

	m_flDamage		= sk_shrapnel_dmg.GetFloat();
	m_DmgRadius		= 1;
	m_takedamage	= DAMAGE_NO;
	m_iHealth		= 1;

	SetGravity( UTIL_ScaleForGravity( SPIT_GRAVITY ) );
	SetFriction( 0.8f );

	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	// We're self-illuminating, so we don't take or give shadows
	AddEffects( EF_NOSHADOW|EF_NORECEIVESHADOW );
}

void CGrenadeShrapnel::Event_Killed( const CTakeDamageInfo &info )
{
	Detonate( );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CGrenadeShrapnel::GrenadeShrapnelTouch( CBaseEntity *pOther )
{
	if ( pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS | FSOLID_TRIGGER) )
	{
		// Some NPCs are triggers that can take damage (like antlion grubs). We should hit them.
		if ( ( pOther->m_takedamage == DAMAGE_NO ) || ( pOther->m_takedamage == DAMAGE_EVENTS_ONLY ) )
			return;
	}

	// Don't hit other spit
	if ( pOther->GetCollisionGroup() == HL2COLLISION_GROUP_SPIT )
		return;

	// We want to collide with water
	const trace_t *pTrace = &CBaseEntity::GetTouchTrace();

	// copy out some important things about this trace, because the first TakeDamage
	// call below may cause another trace that overwrites the one global pTrace points
	// at.
	bool bHitWater = ( ( pTrace->contents & CONTENTS_WATER ) != 0 );
	CBaseEntity *const pTraceEnt = pTrace->m_pEnt;
	const Vector tracePlaneNormal = pTrace->plane.normal;

	if ( bHitWater )
	{
		// Splash!
		CEffectData data;
		data.m_fFlags = 0;
		data.m_vOrigin = pTrace->endpos;
		data.m_vNormal = Vector( 0, 0, 1 );
		data.m_flScale = 8.0f;

		DispatchEffect( "watersplash", data );
	}
	else
	{
		// Make a splat decal
		trace_t *pNewTrace = const_cast<trace_t*>( pTrace );
		UTIL_DecalTrace( pNewTrace, "BeerSplash" );
	}

	// Part normal damage, part poison damage
	float poisonratio = sk_antlion_worker_spit_grenade_poison_ratio.GetFloat();

	// Take direct damage if hit
	// NOTE: assume that pTrace is invalidated from this line forward!
	if ( pTraceEnt )
	{
		pTraceEnt->TakeDamage( CTakeDamageInfo( this, GetThrower(), m_flDamage * (1.0f-poisonratio), DMG_ACID ) );
		pTraceEnt->TakeDamage( CTakeDamageInfo( this, GetThrower(), m_flDamage * poisonratio, DMG_POISON ) );
	}

	CSoundEnt::InsertSound( SOUND_DANGER, GetAbsOrigin(), m_DmgRadius * 2.0f, 0.5f, GetThrower() );

	QAngle vecAngles;
	VectorAngles( tracePlaneNormal, vecAngles );
	
	if ( pOther->IsPlayer() || bHitWater )
	{
		// Do a lighter-weight effect if we just hit a player
		DispatchParticleEffect( "antlion_spit_player", GetAbsOrigin(), vecAngles );
	}
	else
	{
		DispatchParticleEffect( "antlion_spit", GetAbsOrigin(), vecAngles );
	}

	Detonate();
}

void CGrenadeShrapnel::Detonate(void)
{
	m_takedamage = DAMAGE_NO;

	EmitSound( "GrenadeSpit.Hit" );	

	// Throw out the shrapnel
	//TODO;

	UTIL_Remove( this );
}

void CGrenadeShrapnel::Think( void )
{

	// Set us up to think again shortly
	SetNextThink( gpGlobals->curtime + 0.05f );
}

