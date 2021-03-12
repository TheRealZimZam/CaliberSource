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
#include "shake.h"
#include "smoke_trail.h"
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

ConVar	  sk_ar2_grenade_radius			( "sk_ar2_grenade_radius","0");
ConVar	  sk_ar2_airburst_radius		( "sk_ar2_airburst_radius","0");
ConVar	  sk_ar2_airburst_cone			( "sk_ar2_airburst_cone","180");	//Cone of explosion - 360 and above creates a normal explosion

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

void CGrenadeAR2::Spawn( void )
{
	Precache( );
	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );

	// Hits everything but debris
	SetCollisionGroup( COLLISION_GROUP_PROJECTILE );

	SetModel( "models/Weapons/ar2_grenade.mdl");
	UTIL_SetSize(this, Vector(-3, -3, -3), Vector(3, 3, 3));
//	UTIL_SetSize(this, Vector(0, 0, 0), Vector(0, 0, 0));

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

	SetGravity( UTIL_ScaleForGravity( 400 ) );	// use a lower gravity for grenades to make them easier to shoot
	SetFriction( 0.8 );
	SetSequence( 0 );

	m_fDangerRadius = 100;

	m_fSpawnTime = gpGlobals->curtime;

	// -------------
	// Smoke trail.
	// -------------
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
		if (m_fSpawnTime + MAX_AR2_NO_COLLIDE_TIME < gpGlobals->curtime)
		{
			m_bIsLive  = true;
		}
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

	// If I'm live go ahead and blow up
	if (m_bIsLive)
	{
		Detonate();
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
		}
	}
}

void CGrenadeAR2::Detonate(void)
{
	if (!m_bIsLive)
	{
		return;
	}
	m_bIsLive		= false;
	m_takedamage	= DAMAGE_NO;	

	if(m_hSmokeTrail)
	{
		UTIL_Remove(m_hSmokeTrail);
		m_hSmokeTrail = NULL;
	}

	CPASFilter filter( GetAbsOrigin() );

	te->Explosion( filter, 0.0,
		&GetAbsOrigin(), 
		g_sModelIndexFireball,
		m_DmgRadius * .02, 
		15,
		TE_EXPLFLAG_NONE,
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

void CGrenadeAR2::Precache( void )
{
	PrecacheModel("models/Weapons/ar2_grenade.mdl"); 
}


CGrenadeAR2::CGrenadeAR2(void)
{
	m_hSmokeTrail  = NULL;
}


//=============================================================================
// Purpose:  Airburst variant - goes live when near a target/impact point
// OR when the command is given by a player
// NOTENOTE; This nade creates two explosions - one in the center at half
// strength and one in-front of the grenade as dictacted by the airburst dist
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

	if (!m_bIsLive)
	{
		// Go live after a short delay
		if (m_fSpawnTime + MAX_AR2_NO_COLLIDE_TIME < gpGlobals->curtime)
		{
			m_bIsLive  = true;
		}
	}
	
	// If I'm just about to hit an enemy, or the world, blow up!
	if (m_bIsLive)
	{
		//TODO; If the player has given the command

		//FIXME; This is temp and doesnt work, just here to stop errors really
		//Do a trace infront of me, if theres ANYTHING there then detonate
		Vector vecForward = GetAbsVelocity();
		VectorNormalize(vecForward);
		trace_t		tr;
		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + AR2_AIRBURST_DIST*vecForward, MASK_SHOT, this, COLLISION_GROUP_PROJECTILE, &tr );
		if ( tr.m_pEnt || GetAbsVelocity().Length() == 0.0)	//Else if i've somehow glitched out and got on stuck on something nonsolid, dont be a dud!
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
