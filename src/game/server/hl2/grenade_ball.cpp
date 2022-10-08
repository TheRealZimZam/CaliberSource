//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 	Fire/Acid/Spitball
//
// TODO; Clamp the amount of running hiss sounds ASAP, more than one flamethrower
// firing at once breaks things
//=============================================================================//

#include "cbase.h"
#include "grenade_ball.h"
#include "Sprite.h"
#include "soundent.h"
#include "decals.h"
//#include "cbasespriteprojectile.h"
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

ConVar    sk_fireball_dmg			( "sk_fireball_dmg", "2", FCVAR_NONE, "Total damage done by an individual fireball");
ConVar	  sk_fireball_radius		( "sk_fireball_radius","32", FCVAR_NONE, "fireball explosion radius");
ConVar	  sk_fireball_life			( "sk_fireball_life","4", FCVAR_NONE, "fireball lifetime");
ConVar	  sk_fireball_groundlife	( "sk_fireball_groundlife","0.4", FCVAR_NONE, "fireball lifetime after hitting something");
ConVar	  sk_fireball_burninterval	( "sk_fireball_burninterval","0.4", FCVAR_NONE, "The next time the fireball can damage something");

ConVar    sk_acidball_dmg			( "sk_acidball_dmg", "5", FCVAR_NONE, "Total damage done by an individual acidball.");
ConVar	  sk_acidball_radius		( "sk_acidball_radius","40", FCVAR_NONE, "Acidball explosion radius.");
ConVar	  sk_acidball_life			( "sk_acidball_life","10", FCVAR_NONE, "Acidball lifetime.");
ConVar	  sk_acidball_groundlife	( "sk_acidball_groundlife","5", FCVAR_NONE, "acidball lifetime after hitting something");
ConVar	  sk_acidball_burninterval	( "sk_acidball_burninterval","0.8", FCVAR_NONE, "The next time the acidball can damage something");

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
	
	DEFINE_FIELD( m_iMaxSplatDecals, FIELD_INTEGER ),
	//DEFINE_FIELD( m_nSquidSpitSprite, FIELD_INTEGER ),
	DEFINE_FIELD( m_pSquidSpitSprite,	FIELD_EHANDLE ),
	DEFINE_FIELD( m_flSpitLifeTime, FIELD_TIME ),
	DEFINE_FIELD( m_flSpitGroundLifeTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flNextDamageTime, FIELD_TIME ),

END_DATADESC()

CGrenadeBall::CGrenadeBall( void ) : m_bPlaySound( true )
{
}

void CGrenadeBall::Precache( void )
{
	PrecacheModel("effects/fire_cloud2.vmt");// client side spittle.
	PrecacheModel("sprites/greenglow1.vmt");// client side spittle.
	PrecacheModel("sprites/bigspit.vmt");// client side spittle.

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

	SetModel( "models/spitball_small.mdl" );
	UTIL_SetSize(this, Vector(-3, -3, -3), Vector(3, 3, 3));

	m_nRenderMode	= kRenderTransAdd;
	SetRenderColor( 0, 0, 0, 0 );
	m_nRenderFX	= kRenderFxNone;

	SetUse( &CBaseGrenade::DetonateUse );
	SetTouch( &CGrenadeBall::GrenadeSpitTouch );
	SetNextThink( gpGlobals->curtime + 0.05f );

	m_takedamage	= DAMAGE_NO;
	m_iHealth		= 1;
	m_flDamage			= 0;
	m_DmgRadius			= 0;
	m_flSpitLifeTime	= -1;
	m_flSpitGroundLifeTime	= 0;
	m_flNextDamageTime = 0;
	m_iMaxSplatDecals = 0;
	m_bHitGround	= false;

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
			m_flSpitLifeTime	= sk_fireball_life.GetFloat();
			m_flSpitGroundLifeTime	= sk_fireball_groundlife.GetFloat();
			m_flNextDamageTime	= sk_fireball_burninterval.GetFloat();

			SetGravity( UTIL_ScaleForGravity( FLAME_GRAVITY ) );
			SetFriction( 0.8f );

			//!!!TODO; Flamelet1-5 for small fires
			m_pSquidSpitSprite = CSprite::SpriteCreate( "effects/fire_cloud2.vmt", GetLocalOrigin(), true );// client side spittle.
			m_pHitSound = "GrenadeFlame.Hit";

			m_hSmokeTrail = SmokeTrail::CreateSmokeTrail();
			if( m_hSmokeTrail )
			{
				m_hSmokeTrail->m_SpawnRate = 48;
				m_hSmokeTrail->m_ParticleLifetime = 1;
				m_hSmokeTrail->m_StartColor.Init(0.1f, 0.1f, 0.1f);
				m_hSmokeTrail->m_EndColor.Init(0,0,0);
				m_hSmokeTrail->m_StartSize = nSize*11;
				m_hSmokeTrail->m_EndSize = m_hSmokeTrail->m_StartSize * 4;
				m_hSmokeTrail->m_SpawnRadius = 4;
				m_hSmokeTrail->m_MinSpeed = 4;
				m_hSmokeTrail->m_MaxSpeed = 24;
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
			m_flSpitLifeTime		= sk_acidball_life.GetFloat();
			m_flSpitGroundLifeTime	= sk_acidball_groundlife.GetFloat();
			m_flNextDamageTime	= sk_acidball_burninterval.GetFloat();

			SetGravity( UTIL_ScaleForGravity( ACID_GRAVITY ) );
			SetFriction( 0.8f );

			m_pSquidSpitSprite = CSprite::SpriteCreate( "sprites/greenglow1.vmt", GetLocalOrigin(), true );// client side spittle.
			m_pHitSound = "GrenadeAcid.Hit";

			m_hSmokeTrail = SmokeTrail::CreateSmokeTrail();
			if( m_hSmokeTrail )
			{
				m_hSmokeTrail->m_SpawnRate = 32;
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
			m_flSpitLifeTime		= sk_spitball_life.GetFloat();
			m_flSpitGroundLifeTime	= sk_spitball_life.GetFloat();

			m_pSquidSpitSprite = CSprite::SpriteCreate( "sprites/bigspit.vmt", GetLocalOrigin(), true );// client side spittle.
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
			m_pSquidSpitSprite->SetSpriteScale( 1.5 );
			m_bPlaySound = true;
			if ( nType == SPIT )
			{
				SetModel( "models/spitball_large.mdl" );
			}
			break;
		}
		case MEDIUM:
		{
			UTIL_SetSize(this, Vector(-2, -2, -2), Vector(2, 2, 2));
			m_pSquidSpitSprite->SetSpriteScale( 1 );
			m_bPlaySound = false;
			m_flDamage *= 0.75f;
			if ( nType == SPIT )
			{
				SetModel( "models/spitball_medium.mdl" );
			}
			break;
		}
		case SMALL:
		{
			UTIL_SetSize(this, Vector(-1, -1, -1), Vector(1, 1, 1));
			m_pSquidSpitSprite->SetSpriteScale( 0.5 );
			m_bPlaySound = false;
			m_flDamage *= 0.5f;
			break;
		}
	}

	nSpitType = nType;

	if ( m_pSquidSpitSprite != NULL )
	{
		m_pSquidSpitSprite->FollowEntity( this );
		m_pSquidSpitSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone );
		m_pSquidSpitSprite->SetScale( nSize * 0.5f );
	}
}

void CGrenadeBall::Event_Killed( const CTakeDamageInfo &info )
{
	Detonate();
}

//-----------------------------------------------------------------------------
// Purpose: Handle spitting
//-----------------------------------------------------------------------------
void CGrenadeBall::GrenadeSpitTouch( CBaseEntity *pOther )
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
		if ( nSpitType == FIRE )
		{
			// TODO; A water-flavored embercloud might be cool here
			Detonate();
		}
	}
	else if ( m_iMaxSplatDecals < 4 )	// No more than 3 times
	{
		// Make a splat decal
		trace_t *pNewTrace = const_cast<trace_t*>( pTrace );
		if ( nSpitType == FIRE )
		{
			UTIL_DecalTrace( pNewTrace, "SmallScorch" );
		}
		else
		{
			UTIL_DecalTrace( pNewTrace, "BeerSplash" );
		}
		m_iMaxSplatDecals++;
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
			pTraceEnt->TakeDamage( CTakeDamageInfo( this, GetThrower(), m_flDamage * (1.0f-poisonratio), DMG_ACID ) );
			pTraceEnt->TakeDamage( CTakeDamageInfo( this, GetThrower(), m_flDamage * poisonratio, DMG_POISON ) );
		}
		Detonate();
	}
	else if ( m_flNextDamageTime <= gpGlobals->curtime )
	{
		if ( pTraceEnt )
		{
			pTraceEnt->TakeDamage( CTakeDamageInfo( this, GetThrower(), m_flDamage, DMG_BURN ) );
		}
	}

	CSoundEnt::InsertSound( SOUND_DANGER, GetAbsOrigin(), m_DmgRadius * 2.0f, 0.5f, GetThrower() );
}

void CGrenadeBall::Detonate(void)
{
	m_takedamage = DAMAGE_NO;

	if ( nSpitType != SPIT )
	{
		// All the spit damage is in the touch logic
		int DmgType = DMG_BURN;
		if ( nSpitType == ACID )
			DmgType = DMG_ACID;

		RadiusDamage( CTakeDamageInfo( this, GetThrower(), m_flDamage, DmgType ), GetAbsOrigin(), m_DmgRadius, CLASS_NONE, NULL );
	}
	EmitSound( m_pHitSound );

	// Stop our hissing sound
	if ( m_pHissSound != NULL )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pHissSound );
		m_pHissSound = NULL;
	}

	UTIL_Remove( m_pSquidSpitSprite );
	if(m_hSmokeTrail)
	{
		UTIL_Remove(m_hSmokeTrail);
	}

	UTIL_Remove( this );
}

void CGrenadeBall::InitHissSound( void )
{
	if ( m_bPlaySound == false )
		return;

	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	if ( m_pHissSound == NULL )
	{
		CPASAttenuationFilter filter( this );
		if ( nSpitType == FIRE )
		{
			m_pHissSound = controller.SoundCreate( filter, entindex(), "GrenadeFlame.Hiss" );
		}
		else
		{
			m_pHissSound = controller.SoundCreate( filter, entindex(), "GrenadeSpit.Hiss" );
		}
		controller.Play( m_pHissSound, 1.0f, 100 );
	}
}

void CGrenadeBall::Think( void )
{
	if (m_flSpitLifeTime != -1)
	{
		if ( m_flSpitLifeTime <= gpGlobals->curtime )
		{
			UTIL_Remove( this );
			return;
		}
		else if (m_bHitGround && m_flSpitGroundLifeTime <= gpGlobals->curtime)
		{
			Detonate();
		}
	}

	// Set us up to think again shortly
	SetNextThink( gpGlobals->curtime + 0.1f );

	InitHissSound();
	if ( m_pHissSound == NULL )
		return;

	// Add a doppler effect to the balls as they travel
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
		if ( iPitch > 250 )
		{
			iPitch = 250;
		}
		if ( iPitch < 50 )
		{
			iPitch = 50;
		}

		// Set the pitch we've calculated
		CSoundEnvelopeController::GetController().SoundChangePitch( m_pHissSound, iPitch, 0.1f );
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

