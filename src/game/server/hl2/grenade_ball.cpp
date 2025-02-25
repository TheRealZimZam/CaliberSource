//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 	Fire/Acid/Spitball
//
// TODO; This needs a whole lot moved to the client - this absolutely spikes
// the entity count as of now; currently every ball is actually 3 entities -
// the ball itself, the sprite, and the smoketrail. It absolutely needs to be 
// reduced to 1 (or less?). The best way I can see about doing it, is to
// re-implement sprite-models (mod_sprite) at the very lowest class (c_baseentity).
// Doing that will have to be done eventually anyway, if we really want to
// do those doom/duke bonus levels - M.M
//=============================================================================//

#include "cbase.h"
#include "grenade_ball.h"
#include "Sprite.h"
#include "soundent.h"
#include "decals.h"
#include "smoke_trail.h"
#include "hl2_shareddefs.h"
#include "vstdlib/random.h"
#include "mathlib/mathlib.h" 
#include "engine/IEngineSound.h"
#include "particle_parse.h"
#include "particle_system.h"
#include "soundenvelope.h"
#include "ai_utils.h"
#include "te_effect_dispatch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define FIREBALL_IGNITE_PROPS 1		//Can fireballs ignite world entities?

ConVar    sk_fireball_dmg			( "sk_fireball_dmg", "2", FCVAR_NONE, "Total damage done by an individual fireball");
ConVar	  sk_fireball_radius		( "sk_fireball_radius","32", FCVAR_NONE, "fireball ignite radius");
ConVar	  sk_fireball_life			( "sk_fireball_life","3", FCVAR_NONE, "fireball lifetime");
ConVar	  sk_fireball_groundlife	( "sk_fireball_groundlife","0.6", FCVAR_NONE, "fireball lifetime after hitting something");

ConVar    sk_acidball_dmg			( "sk_acidball_dmg", "5", FCVAR_NONE, "Total damage done by an individual acidball.");
ConVar	  sk_acidball_radius		( "sk_acidball_radius","40", FCVAR_NONE, "Acidball explosion radius.");
ConVar	  sk_acidball_life			( "sk_acidball_life","10", FCVAR_NONE, "Acidball lifetime.");
ConVar	  sk_acidball_groundlife	( "sk_acidball_groundlife","5", FCVAR_NONE, "acidball lifetime after hitting something");

ConVar    sk_spitball_dmg		  ( "sk_spitball_dmg", "20", FCVAR_NONE, "Total damage done by an individual antlion worker loogie.");
ConVar	  sk_spitball_radius		  ( "sk_spitball_radius","40", FCVAR_NONE, "Radius of effect for an antlion worker spit grenade.");
ConVar	  sk_spitball_poison_ratio ( "sk_spitball_ratio","0.3", FCVAR_NONE, "Percentage of an antlion worker's spit damage done as poison (which regenerates)"); 
ConVar	  sk_spitball_life		  ( "sk_spitball_life","15", FCVAR_NONE, "Spitball lifetime.");

LINK_ENTITY_TO_CLASS( grenade_ball, CGrenadeBall );

BEGIN_DATADESC( CGrenadeBall )

	DEFINE_FIELD( m_iszHissSound, FIELD_STRING ),
	DEFINE_FIELD( m_iszHitSound, FIELD_STRING ),

	DEFINE_FIELD( m_iSplatDecals, FIELD_INTEGER ),
	DEFINE_FIELD( m_flSpitLifeTime, FIELD_TIME ),
	DEFINE_FIELD( m_flSpitGroundLifeTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flNextDamageTime, FIELD_TIME ),

END_DATADESC()

CGrenadeBall::CGrenadeBall( void )
{
	m_flSpitLifeTime = -1;
	m_flSpitGroundLifeTime = -1;
}

void CGrenadeBall::Precache( void )
{
	BaseClass::Precache();

	if ( m_iszHissSound != NULL_STRING )
		PrecacheScriptSound( STRING(m_iszHissSound) );

	if ( m_iszHitSound != NULL_STRING )
		PrecacheScriptSound( STRING(m_iszHitSound) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeBall::Spawn( char *pszModel,
								int iSize,
								const Vector &vecVelocity,
								CBaseEntity *pOwner,
								MoveType_t	iMovetype,
								MoveCollide_t nMoveCollide,
								int	iGravity,
								CBaseEntity *pIntendedTarget,
								float fLifetime )
{
	BaseClass::Spawn( pszModel,
							iSize,
							vecVelocity,
							pOwner,
							iMovetype,
							nMoveCollide,
							iGravity,
							pIntendedTarget );

	SetCollisionGroup( HL2COLLISION_GROUP_SPIT );
	SetTouch( &CGrenadeBall::Touch );
	SetThink( &CGrenadeBall::FlyThink );

	m_nRenderMode = kRenderTransAdd;
	SetRenderColor( 255, 255, 255, 224 );
	m_nRenderFX	= kRenderFxGlowShell;

	m_iHealth				= 1;
	m_takedamage			= DAMAGE_EVENTS_ONLY;
	m_flSpitLifeTime		= fLifetime;
	m_flSpitGroundLifeTime	= 0;
	m_flNextDamageTime		= gpGlobals->curtime + 0.5f;
	m_iSplatDecals			= 0;
	m_bHitGround			= false;

	SetFriction( 0.8f );
	SetSequence( 1 );
	InitHissSound();

	// We're self-illuminating, so we don't take or give shadows
	AddEffects( EF_NOSHADOW|EF_NORECEIVESHADOW );
}

void CGrenadeBall::Event_Killed( const CTakeDamageInfo &info )
{
	Detonate();
}

//-----------------------------------------------------------------------------
// Purpose: Handle Collision
// Spitballs only collide with the world, everything else is phased through
//-----------------------------------------------------------------------------
void CGrenadeBall::Touch( CBaseEntity *pOther )
{
	Assert( pOther );

	// Dont hit triggers
	if ( !pOther->IsSolid() || pOther->IsSolidFlagSet(FSOLID_TRIGGER|FSOLID_VOLUME_CONTENTS) )
	{
		// Some NPCs are triggers that can take damage (like antlion grubs). We should hit them.
		if ( ( pOther->m_takedamage == DAMAGE_NO ) || ( pOther->m_takedamage == DAMAGE_EVENTS_ONLY ) )
			return;
	}

	// Don't hit other spit
	if ( pOther->GetCollisionGroup() == HL2COLLISION_GROUP_SPIT )
		return;

	// Dont collide with my owner
	if ( pOther == GetOwnerEntity() )
		return;

	// We want to collide with water
	const trace_t *pTrace = &CBaseEntity::GetTouchTrace();
	if( pTrace->surface.flags & SURF_SKY )
	{
		Detonate();
		return;
	}

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
		if ( GetDamageType() == DMG_BURN )
		{
			// TODO; Boil the water
			Detonate();
		}
	}
	else if ( m_iSplatDecals < BALL_MAX_DECALS )
	{
		// Make a splat decal
		trace_t *pNewTrace = const_cast<trace_t*>( pTrace );
		if ( GetDamageType() == DMG_BURN )
			UTIL_DecalTrace( pNewTrace, "SmallScorch" );
		else
			UTIL_DecalTrace( pNewTrace, "BeerSplash" );

		m_iSplatDecals++;
		m_bHitGround = true;	//Just hit the ground, start the groundlife timer
	}

	QAngle vecAngles;
	VectorAngles( tracePlaneNormal, vecAngles );

	// Part normal damage, part poison damage
	if ( GetDamageType() == DMG_ACID )
	{
		float poisonratio = sk_spitball_poison_ratio.GetFloat();

		// Take direct damage if hit
		// NOTE: assume that pTrace is invalidated from this line forward!
		if ( pTraceEnt )
		{
			pTraceEnt->TakeDamage( CTakeDamageInfo( this, GetOwnerEntity(), GetDamage() * (1.0f-poisonratio), GetDamageType() ) );
			pTraceEnt->TakeDamage( CTakeDamageInfo( this, GetOwnerEntity(), GetDamage() * poisonratio, DMG_POISON ) );
		}
		Detonate();
	}
	else if ( m_flNextDamageTime <= gpGlobals->curtime )
	{
		if ( pTraceEnt )
		{
			pTraceEnt->TakeDamage( CTakeDamageInfo( this, GetOwnerEntity(), GetDamage(), GetDamageType() ) );
			m_flNextDamageTime = gpGlobals->curtime + 0.4f;
		}

		if ( m_iszHissSound != NULL_STRING )
			CSoundEnt::InsertSound( SOUND_DANGER, GetAbsOrigin(), (GetDamageRadius() * 1.5f), 0.5f, GetOwnerEntity() );
	}
}

void CGrenadeBall::Detonate(void)
{
	// All the spit damage is in the touch logic
	if ( GetDamageType() != DMG_ACID )
	{
		if ( GetDamageType() == DMG_BURN )
		{
			// Ignite entities in the vicinity.
			CBaseEntity *pEntity = NULL;
			for ( CEntitySphereQuery sphere( GetAbsOrigin(), m_DmgRadius ); (pEntity = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity() )
			{
				CBaseCombatCharacter *pBCC = pEntity->MyCombatCharacterPointer();
				if ( pBCC && !(pBCC == GetOwnerEntity()) )
				{
					// Dont stack/reset it, only ignite if not already
					if (!pBCC->IsOnFire())
						pBCC->Ignite( random->RandomFloat( 5, 10 ) );
				}
#if FIREBALL_IGNITE_PROPS
				CBaseAnimating *pAnimating;
				pAnimating = dynamic_cast<CBaseAnimating*>(pEntity);
				if ( pAnimating && pAnimating->IsAlive() )
				{
					// Ignite props very rarely
					if ( random->RandomInt( 0, 30 ) == 30 && !pAnimating->IsOnFire())
						pAnimating->Ignite( random->RandomFloat( 5, 10 ) );
				}
#endif
			}
		}

		RadiusDamage( CTakeDamageInfo( this, GetOwnerEntity(), CBaseProjectile::GetDamage(), GetDamageType() ), GetAbsOrigin(), GetDamageRadius(), CLASS_NONE, NULL );
	}

	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), STRING(m_iszHitSound) );

	//
	// Shut everything down
	//

	// Stop our hissing sound
	StopSound(entindex(), CHAN_BODY, STRING(m_iszHissSound));

	if(m_hSmokeTrail)
		UTIL_Remove(m_hSmokeTrail);

	// Set to fade
	SetTouch( NULL );
#if 0
	SUB_StartFadeOut(1);
#else
	UTIL_Remove( this );
#endif
}

void CGrenadeBall::InitHissSound( void )
{
	if ( m_iszHissSound != NULL_STRING )
	{
		CPASAttenuationFilter filter( this );
		EmitSound_t ep;
		ep.m_nChannel = CHAN_BODY;
		ep.m_pSoundName = STRING(m_iszHissSound);
		ep.m_flVolume = 1.0f;
		ep.m_SoundLevel = SNDLVL_NORM;
		ep.m_nPitch = 100;

		EmitSound( filter, entindex(), ep );
	}
}

void CGrenadeBall::FlyThink( void )
{
	BaseClass::FlyThink();

	if (m_flSpitLifeTime != -1)
	{
		if ( m_flSpitLifeTime < gpGlobals->curtime )
		{
			Detonate();
			return;
		}
		else if ( m_bHitGround && m_flSpitGroundLifeTime < gpGlobals->curtime)
		{
			Detonate();
			return;
		}
	}

	// Set us up to think again shortly
	SetNextThink( gpGlobals->curtime + 0.1f );

	// Everything past here is sound related

	// Add a doppler effect to the balls as they travel
#if 0
	if ( !g_pGameRules->IsMultiplayer() )
	{
		CBaseEntity *pPlayer = AI_GetSinglePlayer();
		if ( pPlayer != NULL )
		{
			Vector dir;
			VectorSubtract( pPlayer->GetAbsOrigin(), GetAbsOrigin(), dir );
			VectorNormalize(dir);

			float velReceiver = DotProduct( pPlayer->GetAbsVelocity(), dir );
			float velTransmitter = -DotProduct( GetAbsVelocity(), dir );
			
			// speed of sound == 13049in/s
			int iPitch = 100 * ((1 - velReceiver / 13049) / (1 + velTransmitter / 13049));

			// clamp pitch shifts
			if ( iPitch > 200 )
			{
				iPitch = 200;
			}
			if ( iPitch < 50 )
			{
				iPitch = 50;
			}

			// Set the pitch we've calculated
			CSoundEnvelopeController::GetController().SoundChangePitch( m_iszHissSound, iPitch, 0.1f );
		}
	}
#endif
}

//=============================================================================
// Purpose:	Spawn helpers - just create a certain type
//
//=============================================================================
LINK_ENTITY_TO_CLASS( grenade_fire, CGrenadeBall );
LINK_ENTITY_TO_CLASS( grenade_fireball, CGrenadeBall );
/*
void CGrenadeFireball::Spawn( void )
{
	SetSpitType( FIRE, MEDIUM );
}
*/

//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( grenade_acid, CGrenadeBall );
/*
void CGrenadeAcid::Spawn( void )
{
	SetSpitType( ACID, MEDIUM );
}
*/

//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( grenade_spit, CGrenadeBall );
/*
void CGrenadeSpit::Spawn( void )
{
	SetSpitType( SPIT, MEDIUM );
}
*/
