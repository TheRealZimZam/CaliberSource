//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Launched Grenade
//
// TODO's: Fix smoke-trail orientation, airburst variant
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "grenade_ar2.h"
#include "weapon_ar2.h"
#include "soundent.h"
#include "decals.h"
#include "gamerules.h"
#include "ammodef.h"
#include "shake.h"
#ifdef OLD_SMOKE_TRAIL
#include "smoke_trail.h"
#else
#include "SpriteTrail.h"
#endif
#include "ar2_explosion.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "world.h"

#ifdef PORTAL
	#include "portal_util_shared.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define AR2_GRENADE_MAX_DANGER_RADIUS	300

extern short	g_sModelIndexFireball;			// (in combatweapon.cpp) holds the index for the smoke cloud

// Moved to HL2_SharedGameRules because these are referenced by shared AmmoDef functions
extern ConVar    sk_plr_dmg_smg1_grenade;
extern ConVar    sk_npc_dmg_smg1_grenade;
//extern ConVar    sk_max_smg1_grenade;

ConVar		sk_ar2_grenade_bounce_velocity_to_preserve	( "sk_ar2_grenade_bounce_velocity_to_preserve","0.1");
#define		AR2_GRENADE_BOUNCE_VELOCITY_TO_PRESERVE	sk_ar2_grenade_bounce_velocity_to_preserve.GetFloat()	//HL2 is 0.3, Quake is 0.8

ConVar		sk_ar2_grenade_activation_delay	( "sk_ar2_grenade_activation_delay","0.6");
#define		AR2_ACTIVATION_DELAY	sk_ar2_grenade_activation_delay.GetFloat()	//Time spent before activation - can bounce within this timeframe

ConVar	  sk_ar2_grenade_radius			( "sk_ar2_grenade_radius","0");
ConVar	  sk_ar2_airburst_radius		( "sk_ar2_airburst_radius","512");
ConVar	  sk_ar2_airburst_cone			( "sk_ar2_airburst_cone","0.95");		//Cone of bullets
ConVar	  sk_ar2_airburst_bullet_amt	( "sk_ar2_airburst_bullet_amt","30");	//Amount of bullets fired
ConVar	  sk_ar2_airburst_bullet_dmg	( "sk_ar2_airburst_bullet_dmg","4");	//Amount of bullets fired

ConVar g_CV_SmokeTrail("ar2_grenade_smoke_trail", "1", 0); // temporary dust explosion switch

BEGIN_DATADESC( CGrenadeAR2 )

	DEFINE_FIELD( m_hSmokeTrail, FIELD_EHANDLE ),
	DEFINE_FIELD( m_fSpawnTime, FIELD_TIME ),
	DEFINE_FIELD( m_fDangerRadius, FIELD_FLOAT ),

	// Function pointers
	DEFINE_ENTITYFUNC( GrenadeAR2Touch ),
	DEFINE_THINKFUNC( GrenadeAR2Think ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( grenade_ar2, CGrenadeAR2 );

void CGrenadeAR2::Precache( void )
{
	PrecacheModel("models/Weapons/ar2_grenade.mdl");
#ifndef OLD_SMOKE_TRAIL
	PrecacheModel( "sprites/smoke1.vmt" );
#endif

	PrecacheScriptSound( "BaseGrenade.BounceSound" );
}

CGrenadeAR2::CGrenadeAR2(void)
{
	m_hSmokeTrail  = NULL;
	m_bWantsToDetonate = false;
}

void CGrenadeAR2::Spawn( void )
{
	Precache( );

	SetModel( "models/Weapons/ar2_grenade.mdl");

#ifdef NEW_MOVEMENT
	SetMoveType( MOVETYPE_VPHYSICS );
	CreateVPhysics();
	AddSolidFlags( FSOLID_NOT_STANDABLE );
#else
	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
#endif

	// Hits everything but debris
	SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
	UTIL_SetSize(this, Vector(-3, -3, -3), Vector(3, 3, 3));

	SetUse( &CGrenadeAR2::DetonateUse );
	SetTouch( &CGrenadeAR2::GrenadeAR2Touch );
	SetThink( &CGrenadeAR2::GrenadeAR2Think );
	SetNextThink( gpGlobals->curtime + 0.1f );

	if( GetOwnerEntity() && GetOwnerEntity()->IsPlayer() )
	{
		m_flDamage = sk_plr_dmg_smg1_grenade.GetFloat();
	}
	else
	{
		m_flDamage = sk_npc_dmg_smg1_grenade.GetFloat();
	}

	m_DmgRadius		= sk_ar2_grenade_radius.GetFloat();
	m_takedamage	= DAMAGE_YES;
	m_bIsLive		= false;
	m_iHealth		= 1;		//Can be blown-up midair by other explosives n' such

	SetGravity( UTIL_ScaleForGravity( 450 ) );	// use a lower gravity for grenades to make them easier to shoot
	SetFriction( 0.8 );
	SetSequence( 0 );

	m_fDangerRadius = 100;

	m_fSpawnTime = gpGlobals->curtime;

	// -------------
	// Smoke trail.
	// -------------
#ifdef OLD_SMOKE_TRAIL
	if( g_CV_SmokeTrail.GetInt() && !IsXbox() )
	{
		m_hSmokeTrail = SmokeTrail::CreateSmokeTrail();
		
		if( m_hSmokeTrail )
		{
			m_hSmokeTrail->m_SpawnRate = 48;
			m_hSmokeTrail->m_ParticleLifetime = 1;
			m_hSmokeTrail->m_StartColor.Init(0.1f, 0.1f, 0.1f);
			m_hSmokeTrail->m_EndColor.Init(0,0,0);
			m_hSmokeTrail->m_StartSize = 8;	// 12
			m_hSmokeTrail->m_EndSize = m_hSmokeTrail->m_StartSize * 4;
			m_hSmokeTrail->m_SpawnRadius = 4;
			m_hSmokeTrail->m_MinSpeed = 4;
			m_hSmokeTrail->m_MaxSpeed = 24;
			m_hSmokeTrail->m_Opacity = 0.2f;

			m_hSmokeTrail->SetLifetime(10.0f);
			m_hSmokeTrail->FollowEntity(this);
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose:  The grenade has a slight delay before it goes live.  That way the
//			 person firing it can bounce it off a nearby wall.  However if it
//			 hits another character it blows up immediately
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CGrenadeAR2::GrenadeAR2Think( void )
{
	SetNextThink( gpGlobals->curtime + 0.05f );

	if (!m_bIsLive)
	{
		// Go live after a short delay
		if (m_fSpawnTime + AR2_ACTIVATION_DELAY < gpGlobals->curtime)
		{
			m_bIsLive  = true;
		}
#ifndef OLD_SMOKE_TRAIL
		if( g_CV_SmokeTrail.GetInt() && m_hSmokeTrail == NULL)
		{
			m_hSmokeTrail	= CSpriteTrail::SpriteTrailCreate( "sprites/smoke1.vmt", GetLocalOrigin(), true );

			m_hSmokeTrail->FollowEntity( this );
			m_hSmokeTrail->SetTransparency( kRenderTransAdd, 255, 255, 255, 128, kRenderFxNone );
			m_hSmokeTrail->SetStartWidth( 4.0f );
			m_hSmokeTrail->SetStartWidthVariance( 2.0f );
			m_hSmokeTrail->SetEndWidth( 1.0f );
			m_hSmokeTrail->SetTextureResolution( random->RandomFloat( 0.00075f, 0.001f ) );
			m_hSmokeTrail->SetLifeTime( 0.5f );
		}
#endif
	}
	
	// If I just went solid and my velocity is zero, it means I'm resting on
	// the floor already when I went solid so blow up
	if (m_bIsLive)
	{
		if (GetAbsVelocity().Length() == 0.0 ||
			GetGroundEntity() != NULL )
		{
			Detonate();
		}
	}

	// The old way of making danger sounds would scare the crap out of EVERYONE between you and where the grenade
	// was going to hit. The radius of the danger sound now 'blossoms' over the grenade's lifetime, making it seem
	// dangerous to a larger area downrange than it does from where it was fired.
	if( m_fDangerRadius <= AR2_GRENADE_MAX_DANGER_RADIUS )
	{
		m_fDangerRadius += ( AR2_GRENADE_MAX_DANGER_RADIUS * 0.05 );
	}

	CSoundEnt::InsertSound( SOUND_DANGER, GetAbsOrigin() + GetAbsVelocity() * 0.5, m_fDangerRadius, 0.2, this, SOUNDENT_CHANNEL_REPEATED_DANGER );
}

void CGrenadeAR2::Event_Killed( const CTakeDamageInfo &info )
{
	Detonate( );
}

void CGrenadeAR2::GrenadeAR2Touch( CBaseEntity *pOther )
{
	Assert( pOther );
	if ( !pOther->IsSolid() )
		return;

	// Dont slow down as much as other baseanimating stuff
	SetAbsVelocity( GetAbsVelocity() * 0.9 );

	// If I'm live go ahead and blow up
	if (m_bIsLive)
	{
		Detonate();
		return;
	}
	else
	{
		// If I'm not live, only blow up if I'm hitting an chacter that
		// is not the owner of the weapon
		CBaseCombatCharacter *pBCC = ToBaseCombatCharacter( pOther );
		if (pBCC && GetThrower() != pBCC)
		{
			m_bIsLive = true;
			Detonate();
			return;
		}
	}

//!	BaseClass::BounceTouch( pOther );

	BounceSound();
}

void CGrenadeAR2::BounceSound( void )
{
	if ( GetAbsVelocity().Length() > 99.0 )
	{
		EmitSound( "BaseGrenade.BounceSound" );
		CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 200, 1.0, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CGrenadeAR2::Detonate(void)
{
	if (!m_bIsLive)
	{
		return;
	}
	m_bIsLive		= false;
	m_takedamage	= DAMAGE_NO;	

#ifndef OLD_SMOKE_TRAIL
	if(m_hSmokeTrail)
	{
		UTIL_Remove(m_hSmokeTrail);
		m_hSmokeTrail = NULL;
	}
#endif

	CPASFilter filter( GetAbsOrigin() );
	te->Explosion( filter, 0.0,
		&GetAbsOrigin(), 
		g_sModelIndexFireball,
		m_DmgRadius * .02, 
		15,
		TE_EXPLFLAG_NOFIREBALLSMOKE,
		m_DmgRadius,
		m_flDamage );

	Vector vecForward = GetAbsVelocity();
	VectorNormalize(vecForward);
	trace_t		tr;
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + 60*vecForward, MASK_SHOT, 
		this, COLLISION_GROUP_NONE, &tr);

	if ((tr.m_pEnt != GetWorldEntity()) || (tr.hitbox != 0))
	{
		// non-world needs smaller decals
		if( tr.m_pEnt && !tr.m_pEnt->IsNPC() )
		{
			UTIL_DecalTrace( &tr, "SmallScorch" );
		}
	}
	else
	{
		UTIL_DecalTrace( &tr, "Scorch" );	//FIXME; Hook to unique/smaller decal 
	}

	UTIL_ScreenShake( GetAbsOrigin(), 25.0, 150.0, 1.0, 750, SHAKE_START );
//	CSoundEnt::InsertSound ( SOUND_DANGER, GetAbsOrigin(), BASEGRENADE_EXPLOSION_VOLUME, 3.0 );

	RadiusDamage( CTakeDamageInfo( this, GetThrower(), m_flDamage, DMG_BLAST ), GetAbsOrigin(), m_DmgRadius, CLASS_NONE, NULL );

	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose: New movement
//-----------------------------------------------------------------------------
void CGrenadeAR2::ResolveFlyCollisionCustom( trace_t &trace, Vector &vecVelocity )
{
	ResolveFlyCollisionBounce( trace, vecVelocity, AR2_GRENADE_BOUNCE_VELOCITY_TO_PRESERVE );
}

bool CGrenadeAR2::CreateVPhysics()
{
	// Create the object in the physics system
	VPhysicsInitNormal( SOLID_BBOX, 0, false );
	return true;
}

//=============================================================================
// Purpose:  Airburst variant - goes live when near a target/impact point
// OR when the command is given by a player
// NOTENOTE; This nade both creates an explosion at the origin AND
// sends a bunch of bullets at a huge spread towards the way it was point at
// detonation - like a flying, exploding shotgun
//=============================================================================
LINK_ENTITY_TO_CLASS( grenade_ar2_airburst, CGrenadeAR2Airburst );

void CGrenadeAR2Airburst::GrenadeAR2Touch( CBaseEntity *pOther )
{
	Assert( pOther );
	if ( !pOther->IsSolid() )
		return;

	// If I'm live go ahead and blow up
	if (m_bIsLive)
	{
		Detonate();
	}
}

// Explode when near an enemy/the impact destination OR if given a signal by the player
void CGrenadeAR2Airburst::GrenadeAR2Think( void )
{
	SetNextThink( gpGlobals->curtime + 0.05f );

	// If I'm just about to hit an enemy, or the world, blow up!
	if (m_bIsLive && m_bWantsToDetonate)
	{
		Detonate();
	}

	BaseClass::GrenadeAR2Think();
}

// Explode when near an enemy/the impact destination OR if given a signal by the player
void CGrenadeAR2Airburst::ManualDetonate( void )
{
	if (m_fSpawnTime + AR2_AIRBURST_NO_COLLIDE_TIME < gpGlobals->curtime)
	{
		// Dont let it detonate until a very small delay has elapsed, even if the shooter is smashing the right mouse button
		Detonate();
	}
	else
	{
		// Marked for detonation, will go boom as soon as possible
		m_bWantsToDetonate = true;
	}
}

void CGrenadeAR2Airburst::Detonate(void)
{
	m_bIsLive		= false;
	m_takedamage	= DAMAGE_NO;	

#ifndef OLD_SMOKE_TRAIL
	if(m_hSmokeTrail)
	{
		UTIL_Remove(m_hSmokeTrail);
		m_hSmokeTrail = NULL;
	}
#endif

	CPASFilter filter( GetAbsOrigin() );
	te->Explosion( filter, 0.0,
		&GetAbsOrigin(), 
		g_sModelIndexFireball,
		m_DmgRadius * .02, 
		15,
		TE_EXPLFLAG_NONE,
		m_DmgRadius,
		m_flDamage );

	// TODO; vecforward needs to be fixed here
	Vector vecForward = GetAbsVelocity();
	VectorNormalize(vecForward);
	trace_t		tr;
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + 60*vecForward, MASK_SHOT, 
		this, COLLISION_GROUP_NONE, &tr);
	UTIL_DecalTrace( &tr, "SmallScorch" );	//FIXME; Hook to unique/smaller decal 
	UTIL_ScreenShake( GetAbsOrigin(), 25.0, 150.0, 1.0, 750, SHAKE_START );

	RadiusDamage( CTakeDamageInfo( this, GetThrower(), m_flDamage, DMG_BLAST ), GetAbsOrigin(), m_DmgRadius, CLASS_NONE, NULL );

	// Fire out a whole bunch of bullets through the front
	FireBulletsInfo_t info;
	info.m_pAttacker = this;	//GetOwnerEntity()
	info.m_iShots = sk_ar2_airburst_bullet_amt.GetInt();
	info.m_vecSrc = GetAbsOrigin();
	info.m_vecDirShooting = vecForward;
	info.m_flDistance =  sk_ar2_airburst_radius.GetInt();
	info.m_iTracerFreq = 1;
	CAmmoDef *pAmmoDef = GetAmmoDef();
	info.m_iAmmoType = pAmmoDef->Index( "Pistol" );
	info.m_iDamage = sk_ar2_airburst_bullet_dmg.GetInt();
	info.m_vecSpread = sk_ar2_airburst_cone.GetFloat();
	info.m_nFlags = (FIRE_BULLETS_NO_IMPACTS|FIRE_BULLETS_TEMPORARY_DANGER_SOUND);
	FireBullets( info );

	UTIL_Remove( this );
}


//=============================================================================
// Purpose: Shrapnel - no explosion, just send out a cloud of buckshot
//=============================================================================
LINK_ENTITY_TO_CLASS( grenade_ar2_shrapnel, CGrenadeAR2Shrapnel );

#define VECTOR_CONE_SHRAPNEL		Vector( 0.10461, 0.10461, 0.10461 )
extern ConVar sk_plr_num_shotgun_pellets;

// Travel for a fraction of a second, then boom
void CGrenadeAR2Shrapnel::GrenadeAR2Touch( CBaseEntity *pOther )
{
	Assert( pOther );

	// If I'm live go ahead and blow up
	if (m_bIsLive)
	{
		Detonate();
	}

	m_bIsLive  = true;
}

void CGrenadeAR2Shrapnel::GrenadeAR2Think( void )
{
	if (m_bIsLive)
	{
		Detonate();
	}

	// Go live after a short delay
	if (m_fSpawnTime + AR2_AIRBURST_NO_COLLIDE_TIME < gpGlobals->curtime)
	{
		m_bIsLive  = true;
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CGrenadeAR2Shrapnel::Detonate(void)
{
	m_bIsLive		= false;
	m_takedamage	= DAMAGE_NO;	

#ifndef OLD_SMOKE_TRAIL
	if(m_hSmokeTrail)
	{
		UTIL_Remove(m_hSmokeTrail);
		m_hSmokeTrail = NULL;
	}
#endif

	// TODO; vecforward needs to be fixed here
	Vector vecForward = GetAbsVelocity();
	VectorNormalize(vecForward);
	trace_t		tr;
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + 60*vecForward, MASK_SHOT, 
		this, COLLISION_GROUP_NONE, &tr);
	UTIL_DecalTrace( &tr, "SmallScorch" );	//FIXME; Hook to unique/smaller decal 
	UTIL_ScreenShake( GetAbsOrigin(), 25.0, 150.0, 1.0, 750, SHAKE_START );

	// Fire out a whole bunch of bullets through the front
	FireBulletsInfo_t info;
	info.m_pAttacker = GetOwnerEntity();
	info.m_iShots = sk_plr_num_shotgun_pellets.GetInt();
	info.m_vecSrc = GetAbsOrigin();
	info.m_vecDirShooting = vecForward;
	info.m_flDistance =  1024;
	info.m_iTracerFreq = 1;
	CAmmoDef *pAmmoDef = GetAmmoDef();
	info.m_iAmmoType = pAmmoDef->Index( "Buckshot" );
	info.m_iDamage = sk_ar2_airburst_bullet_dmg.GetInt();
	info.m_vecSpread = VECTOR_CONE_SHRAPNEL;
	info.m_nFlags = (FIRE_BULLETS_TEMPORARY_DANGER_SOUND);
	FireBullets( info );

	UTIL_Remove( this );
}
