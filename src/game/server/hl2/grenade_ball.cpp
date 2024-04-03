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
#include "engine/IEngineSound.h"
#include "particle_parse.h"
#include "particle_system.h"
#include "soundenvelope.h"
#include "ai_utils.h"
#include "te_effect_dispatch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar temp_demofixes;	//TEMPTEMP

#define USE_SPRITE 1				//Use an extra sprite that follows the main model, which is not un-rendered at long range
									//FIXME; Disabled until entity limit is increased

#define FIREBALL_IGNITE_PROPS 1		//Can fireballs ignite world entities?

ConVar    sk_fireball_dmg			( "sk_fireball_dmg", "2", FCVAR_NONE, "Total damage done by an individual fireball");
ConVar	  sk_fireball_radius		( "sk_fireball_radius","32", FCVAR_NONE, "fireball ignite radius");
ConVar	  sk_fireball_life			( "sk_fireball_life","3", FCVAR_NONE, "fireball lifetime");
ConVar	  sk_fireball_groundlife	( "sk_fireball_groundlife","0.6", FCVAR_NONE, "fireball lifetime after hitting something");
//ConVar	  sk_fireball_burninterval	( "sk_fireball_burninterval","0.4", FCVAR_NONE, "The next time the fireball can damage something");

ConVar    sk_acidball_dmg			( "sk_acidball_dmg", "5", FCVAR_NONE, "Total damage done by an individual acidball.");
ConVar	  sk_acidball_radius		( "sk_acidball_radius","40", FCVAR_NONE, "Acidball explosion radius.");
ConVar	  sk_acidball_life			( "sk_acidball_life","10", FCVAR_NONE, "Acidball lifetime.");
ConVar	  sk_acidball_groundlife	( "sk_acidball_groundlife","5", FCVAR_NONE, "acidball lifetime after hitting something");
//ConVar	  sk_acidball_burninterval	( "sk_acidball_burninterval","0.8", FCVAR_NONE, "The next time the acidball can damage something");

ConVar    sk_antlion_worker_spit_grenade_dmg		  ( "sk_antlion_worker_spit_grenade_dmg", "20", FCVAR_NONE, "Total damage done by an individual antlion worker loogie.");
ConVar	  sk_antlion_worker_spit_grenade_radius		  ( "sk_antlion_worker_spit_grenade_radius","40", FCVAR_NONE, "Radius of effect for an antlion worker spit grenade.");
ConVar	  sk_antlion_worker_spit_grenade_poison_ratio ( "sk_antlion_worker_spit_grenade_poison_ratio","0.3", FCVAR_NONE, "Percentage of an antlion worker's spit damage done as poison (which regenerates)"); 
ConVar	  sk_spitball_life		  ( "sk_spitball_life","15", FCVAR_NONE, "Spitball lifetime.");

LINK_ENTITY_TO_CLASS( grenade_ball, CGrenadeBall );

BEGIN_DATADESC( CGrenadeBall )

	DEFINE_FIELD( m_bPlaySound, FIELD_BOOLEAN ),
	DEFINE_SOUNDPATCH( m_pHissSound ),

	// Function pointers
	//DEFINE_THINKFUNC( SpitThink ),
	DEFINE_ENTITYFUNC( GrenadeSpitTouch ),
	
	DEFINE_FIELD( m_iSplatDecals, FIELD_INTEGER ),
	//DEFINE_FIELD( m_nSquidSpitSprite, FIELD_INTEGER ),
	DEFINE_FIELD( m_pSquidSpitSprite, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flSpitLifeTime, FIELD_TIME ),
	DEFINE_FIELD( m_flSpitGroundLifeTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flNextDamageTime, FIELD_TIME ),

END_DATADESC()

CGrenadeBall::CGrenadeBall( void ) : m_bPlaySound( false )
{
}

void CGrenadeBall::Precache( void )
{
	// Precache everything
	PrecacheModel("sprites/xfireball3.vmt");
	PrecacheModel("sprites/greenglow1.vmt");
	PrecacheModel("sprites/bigspit.vmt");

	PrecacheModel("models/spitball_large.mdl"); 
	PrecacheModel("models/spitball_medium.mdl"); 
	PrecacheModel("models/spitball_small.mdl"); 

	PrecacheScriptSound( "GrenadeFlame.Hiss" );
	PrecacheScriptSound( "GrenadeAcid.Hiss" );
	PrecacheScriptSound( "GrenadeSpit.Hiss" );
	
	PrecacheScriptSound( "GrenadeFlame.Hit" );
	PrecacheScriptSound( "GrenadeAcid.Hit" );
	PrecacheScriptSound( "GrenadeSpit.Hit" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeBall::Spawn( void )
{
	Precache( );
	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_FLYGRAVITY );
	SetSolidFlags( FSOLID_NOT_STANDABLE );
	SetCollisionGroup( HL2COLLISION_GROUP_SPIT );
//	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	SetModel( "models/spitball_small.mdl" );	//default to spit
	UTIL_SetSize(this, Vector(-3, -3, -3), Vector(3, 3, 3));

	m_nRenderMode = kRenderTransAdd;
	SetRenderColor( 255, 255, 255, 224 );
	m_nRenderFX	= kRenderFxGlowShell;

	SetUse( &CBaseGrenade::DetonateUse );
	SetTouch( &CGrenadeBall::GrenadeSpitTouch );
	SetThink( &CGrenadeBall::FlyThink );
	SetNextThink( gpGlobals->curtime + 0.1f );

	m_takedamage			= DAMAGE_NO;
	m_iHealth				= 1;
	m_flDamage				= 0;
	m_DmgRadius				= 0;
	m_flSpitLifeTime		= -1;
	m_flSpitGroundLifeTime	= 0;
	m_flNextDamageTime		= gpGlobals->curtime + 0.5f;
	m_iSplatDecals			= 0;
	m_bHitGround			= false;
	m_pHissSound			= NULL;

	SetGravity( UTIL_ScaleForGravity( SPIT_GRAVITY ) );
	SetFriction( 0.8f );
	SetSequence( 1 );

	// We're self-illuminating, so we don't take or give shadows
	AddEffects( EF_NOSHADOW|EF_NORECEIVESHADOW );
}

void CGrenadeBall::SetSpitType( int nType, int nSize )
{
	switch (nType)
	{
		case FIRE:
		{
			m_flDamage			= sk_fireball_dmg.GetFloat();
			m_DmgRadius			= sk_fireball_radius.GetFloat();
			m_flSpitLifeTime	= gpGlobals->curtime + sk_fireball_life.GetFloat();
			m_flSpitGroundLifeTime	= gpGlobals->curtime + sk_fireball_groundlife.GetFloat();
//			m_flNextDamageTime	= gpGlobals->curtime + sk_fireball_burninterval.GetFloat();

			SetGravity( UTIL_ScaleForGravity( FLAME_GRAVITY ) );
			SetFriction( 0.8f );
			SetDamageType(DMG_BURN);

			if ( !temp_demofixes.GetBool() )
				SetModel( "sprites/xfireball3.vmt" );

#if USE_SPRITE
			//!!!TODO; Flamelet1-5 for small fires
			m_pSquidSpitSprite = CSprite::SpriteCreate( "sprites/xfireball3.vmt", GetLocalOrigin(), true );// client side spittle.
#endif
			m_pHitSound = "GrenadeFlame.Hit";

			m_hSmokeTrail = SmokeTrail::CreateSmokeTrail();
			if( m_hSmokeTrail )
			{
				m_hSmokeTrail->m_SpawnRate = 8;
				m_hSmokeTrail->m_ParticleLifetime = 1;
				m_hSmokeTrail->m_StartColor.Init(0.1f, 0.1f, 0.1f);
				m_hSmokeTrail->m_EndColor.Init(0,0,0);
				m_hSmokeTrail->m_StartSize = nSize*11;
				m_hSmokeTrail->m_EndSize = m_hSmokeTrail->m_StartSize * 4;
				m_hSmokeTrail->m_SpawnRadius = 4;
				m_hSmokeTrail->m_MinSpeed = 0;
				m_hSmokeTrail->m_MaxSpeed = 64;
				m_hSmokeTrail->m_Opacity = 0.3f;

				m_hSmokeTrail->SetLifetime(10.0f);
				m_hSmokeTrail->FollowEntity(this);
			}
			break;
		}
		case ACID:
		{
			m_flDamage		= sk_acidball_dmg.GetFloat();
			m_DmgRadius		= sk_acidball_radius.GetFloat();
			m_flSpitLifeTime		= gpGlobals->curtime + sk_acidball_life.GetFloat();
			m_flSpitGroundLifeTime	= gpGlobals->curtime + sk_acidball_groundlife.GetFloat();
//			m_flNextDamageTime	= gpGlobals->curtime + sk_acidball_burninterval.GetFloat();

			SetGravity( UTIL_ScaleForGravity( ACID_GRAVITY ) );
			SetFriction( 0.9f );
			SetDamageType(DMG_ACID);

			if ( !temp_demofixes.GetBool() )
				SetModel( "sprites/greenglow1.vmt" );

#if USE_SPRITE
			m_pSquidSpitSprite = CSprite::SpriteCreate( "sprites/greenglow1.vmt", GetLocalOrigin(), true );// client side spittle.
#endif
			m_pHitSound = "GrenadeAcid.Hit";

			m_hSmokeTrail = SmokeTrail::CreateSmokeTrail();
			if( m_hSmokeTrail )
			{
				m_hSmokeTrail->m_SpawnRate = 16;
				m_hSmokeTrail->m_ParticleLifetime = 1;
				m_hSmokeTrail->m_StartColor.Init(0.4f, 0.6f, 0.4f);
				m_hSmokeTrail->m_EndColor.Init(0,0,0);
				m_hSmokeTrail->m_StartSize = nSize*9;
				m_hSmokeTrail->m_EndSize = m_hSmokeTrail->m_StartSize * 2;
				m_hSmokeTrail->m_SpawnRadius = 4;
				m_hSmokeTrail->m_MinSpeed = 4;
				m_hSmokeTrail->m_MaxSpeed = 12;
				m_hSmokeTrail->m_Opacity = 0.2f;

				m_hSmokeTrail->SetLifetime(5.0f);
				m_hSmokeTrail->FollowEntity(this);
			}
			break;
		}
		case SPIT:
		{
			m_flDamage		= sk_antlion_worker_spit_grenade_dmg.GetFloat();
			m_DmgRadius		= sk_antlion_worker_spit_grenade_radius.GetFloat();
			m_flSpitLifeTime		= gpGlobals->curtime + sk_spitball_life.GetFloat();
			m_flSpitGroundLifeTime	= gpGlobals->curtime + sk_spitball_life.GetFloat();
			SetDamageType(DMG_ACID);
			SetModel( "models/spitball_small.mdl" );

#if USE_SPRITE
			m_pSquidSpitSprite = CSprite::SpriteCreate( "sprites/bigspit.vmt", GetLocalOrigin(), true );// client side spittle.
#endif
			m_pHitSound = "GrenadeSpit.Hit";
			SetRenderColor( 255, 255, 255, 255 );
			break;
		}
	}
	switch (nSize)
	{
		case LARGE:
		{
			UTIL_SetSize(this, Vector(-3, -3, -3), Vector(3, 3, 3));
#if USE_SPRITE
			m_pSquidSpitSprite->SetSpriteScale( 1.0 );
			m_pSquidSpitSprite->SetFramerate( random->RandomInt( 10, 15 ) );
#endif
			m_bPlaySound = true;
			if ( nType == SPIT )
				SetModel( "models/spitball_large.mdl" );

			break;
		}
		case MEDIUM:
		{
			UTIL_SetSize(this, Vector(-2, -2, -2), Vector(2, 2, 2));
#if USE_SPRITE
			m_pSquidSpitSprite->SetSpriteScale( 0.6 );
			m_pSquidSpitSprite->SetFramerate( random->RandomInt( 5, 15 ) );
#endif
			m_bPlaySound = false;
			m_flDamage *= 0.75f;
			if ( nType == SPIT )
				SetModel( "models/spitball_medium.mdl" );

			break;
		}
		case SMALL:
		{
			UTIL_SetSize(this, Vector(-1, -1, -1), Vector(1, 1, 1));
#if USE_SPRITE
			m_pSquidSpitSprite->SetSpriteScale( 0.3 );
			m_pSquidSpitSprite->SetFramerate( random->RandomInt( 5, 10 ) );
#endif
			m_bPlaySound = false;
			m_flDamage *= 0.5f;
			break;
		}
	}

	nSpitType = nType;

	if ( m_pSquidSpitSprite != NULL )
	{
		m_pSquidSpitSprite->FollowEntity( this );
		m_pSquidSpitSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxGlowShell );
	}

	InitHissSound();
}

void CGrenadeBall::Event_Killed( const CTakeDamageInfo &info )
{
	Detonate();
}

//-----------------------------------------------------------------------------
// Purpose: Handle Collision
// Spitballs only collide with the world, everything else is phased through
//-----------------------------------------------------------------------------
void CGrenadeBall::GrenadeSpitTouch( CBaseEntity *pOther )
{
//	Assert( pOther );

	// Dont hit triggers
	if ( pOther->IsSolidFlagSet(FSOLID_TRIGGER|FSOLID_VOLUME_CONTENTS) )
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
		if ( nSpitType == FIRE )
		{
			// TODO; Boil the water
			Detonate();
		}
	}
	else if ( m_iSplatDecals < BALL_MAX_DECALS )
	{
		// Make a splat decal
		trace_t *pNewTrace = const_cast<trace_t*>( pTrace );
		if ( nSpitType == FIRE )
			UTIL_DecalTrace( pNewTrace, "SmallScorch" );
		else
			UTIL_DecalTrace( pNewTrace, "BeerSplash" );

		m_iSplatDecals++;
		m_bHitGround = true;	//Just hit the ground, start the groundlife timer
	}

	QAngle vecAngles;
	VectorAngles( tracePlaneNormal, vecAngles );

	// Part normal damage, part poison damage
	if ( nSpitType == SPIT )
	{
		float poisonratio = sk_antlion_worker_spit_grenade_poison_ratio.GetFloat();

		// Take direct damage if hit
		// NOTE: assume that pTrace is invalidated from this line forward!
		if ( pTraceEnt )
		{
			pTraceEnt->TakeDamage( CTakeDamageInfo( this, GetThrower(), m_flDamage * (1.0f-poisonratio), GetDamageType() ) );
			pTraceEnt->TakeDamage( CTakeDamageInfo( this, GetThrower(), m_flDamage * poisonratio, DMG_POISON ) );
		}
		Detonate();
	}
	else if ( m_flNextDamageTime <= gpGlobals->curtime )
	{
		if ( pTraceEnt )
		{
			pTraceEnt->TakeDamage( CTakeDamageInfo( this, GetThrower(), m_flDamage, GetDamageType() ) );
			m_flNextDamageTime = gpGlobals->curtime + 0.4f;
		}

		if ( m_bPlaySound )
			CSoundEnt::InsertSound( SOUND_DANGER, GetAbsOrigin(), m_DmgRadius * 2.0f, 0.5f, GetThrower() );
	}
}

void CGrenadeBall::Detonate(void)
{
	// All the spit damage is in the touch logic
	if ( nSpitType != SPIT )
	{
		if ( nSpitType == FIRE )
		{
			// Ignite entities in the vicinity.
			CBaseEntity *pEntity = NULL;
			for ( CEntitySphereQuery sphere( GetAbsOrigin(), m_DmgRadius ); (pEntity = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity() )
			{
				CBaseCombatCharacter *pBCC = pEntity->MyCombatCharacterPointer();
				if ( pBCC && !(pBCC == GetThrower()) )
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

		RadiusDamage( CTakeDamageInfo( this, GetThrower(), m_flDamage, GetDamageType() ), GetAbsOrigin(), m_DmgRadius, CLASS_NONE, NULL );
	}

	if ( m_bPlaySound )
		EmitSound( m_pHitSound );

	//
	// Shut everything down
	//

	// Stop our hissing sound
	if ( m_pHissSound != NULL )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pHissSound );
		m_pHissSound = NULL;
	}

	// Kill the sprite/trail
	if(m_pSquidSpitSprite)
	{
		m_pSquidSpitSprite->StopFollowingEntity();
		UTIL_Remove(m_pSquidSpitSprite);
	}

	if(m_hSmokeTrail)
		UTIL_Remove(m_hSmokeTrail);

	// Set to fade
	SetTouch( NULL );
#if 0
	SUB_StartFadeOut(1);
#else
	SUB_Remove();
#endif
}

void CGrenadeBall::InitHissSound( void )
{
	if ( !m_bPlaySound )
		return;

	if ( m_pHissSound == NULL )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		CPASAttenuationFilter filter( this );
		switch (nSpitType)
		{
		default:
			break;
		case FIRE:
			m_pHissSound = controller.SoundCreate( filter, entindex(), "GrenadeFlame.Hiss" );
		break;
		case ACID:
			m_pHissSound = controller.SoundCreate( filter, entindex(), "GrenadeAcid.Hiss" );
		break;
		case SPIT:
			m_pHissSound = controller.SoundCreate( filter, entindex(), "GrenadeSpit.Hiss" );
		break;
		}

		controller.Play( m_pHissSound, 1.0f, 100 );
	}
	else
	{
		// Delete the old sound
		CSoundEnvelopeController::GetController().SoundDestroy( m_pHissSound );
		m_pHissSound = NULL;
		// Call me again
		InitHissSound();
	}
}

void CGrenadeBall::FlyThink( void )
{
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
		// If im almost dead while in the air, start fading the sprite
		if (m_pSquidSpitSprite)
			m_pSquidSpitSprite->FadeAndDie( m_flSpitLifeTime );
	}

	// Set us up to think again shortly
	SetNextThink( gpGlobals->curtime + 0.1f );

	// Everything past here is sound related
	if ( m_pHissSound == NULL )
		return;

	// Add a doppler effect to the balls as they travel
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
			CSoundEnvelopeController::GetController().SoundChangePitch( m_pHissSound, iPitch, 0.1f );
		}
	}
}

//=============================================================================
// Purpose:	Spawn helpers - just create a certain type
//
//=============================================================================
LINK_ENTITY_TO_CLASS( grenade_fire, CGrenadeFireball );
LINK_ENTITY_TO_CLASS( grenade_fireball, CGrenadeFireball );

void CGrenadeFireball::Spawn( void )
{
	BaseClass::Spawn();
	SetSpitType( FIRE, MEDIUM );
}

//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( grenade_acid, CGrenadeAcid );

void CGrenadeAcid::Spawn( void )
{
	BaseClass::Spawn();
	SetSpitType( ACID, MEDIUM );
}

//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( grenade_spit, CGrenadeSpit );

void CGrenadeSpit::Spawn( void )
{
	BaseClass::Spawn();
	SetSpitType( SPIT, MEDIUM );
}

