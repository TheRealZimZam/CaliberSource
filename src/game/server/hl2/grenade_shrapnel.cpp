//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Launched Grenade
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "grenade_shrapnel.h"
#include "soundent.h"
#include "decals.h"
#include "gamerules.h"
#include "ammodef.h"
#include "shake.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "world.h"

#ifdef PORTAL
	#include "portal_util_shared.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern short	g_sModelIndexFireball;			// (in combatweapon.cpp) holds the index for the smoke cloud

// Moved to HL2_SharedGameRules because these are referenced by shared AmmoDef functions
extern ConVar    sk_plr_dmg_buckshot;
extern ConVar    sk_npc_dmg_buckshot;

ConVar	sk_shrapnel_cone			("sk_shrapnel_cone","0.105");
#define SHRAPNEL_VECTOR_CONE		sk_shrapnel_cone.GetFloat()	//Vector( 0.10461, 0.10461, 0.10461 )

ConVar	  sk_shrapnel_bullet_amt	( "sk_shrapnel_bullet_amt","8");	//Amount of bullets fired

BEGIN_DATADESC( CGrenadeShrapnel )

	DEFINE_FIELD( m_fSpawnTime, FIELD_TIME ),

	// Function pointers
	DEFINE_ENTITYFUNC( GrenadeShrapnelTouch ),
	DEFINE_THINKFUNC( GrenadeShrapnelThink ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( grenade_shrapnel, CGrenadeShrapnel );

void CGrenadeShrapnel::Precache( void )
{
	PrecacheModel("models/Weapons/ar2_grenade.mdl");	//!!!TEMP

	PrecacheScriptSound( "GrenadeShrapnel.Detonate" );
}

CGrenadeShrapnel::CGrenadeShrapnel(void)
{

}

void CGrenadeShrapnel::Spawn( void )
{
	Precache( );

	SetModel( "models/Weapons/ar2_grenade.mdl");	//!!!TEMP

	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );

	// Hits everything but debris
	SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
	UTIL_SetSize(this, Vector(-3, -3, -3), Vector(3, 3, 3));

	SetUse( &CGrenadeShrapnel::DetonateUse );
	SetTouch( &CGrenadeShrapnel::GrenadeShrapnelTouch );
	SetThink( &CGrenadeShrapnel::GrenadeShrapnelThink );
	SetNextThink( gpGlobals->curtime + 0.1f );

	if( GetOwnerEntity() && GetOwnerEntity()->IsPlayer() )
	{
		m_flDamage = sk_plr_dmg_buckshot.GetFloat();
	}
	else
	{
		m_flDamage = sk_npc_dmg_buckshot.GetFloat();
	}

	m_DmgRadius		= 0;
	m_takedamage	= DAMAGE_YES;
	m_bIsLive		= false;
	m_iHealth		= 1;		//Can be blown-up midair by other explosives n' such

	SetGravity( UTIL_ScaleForGravity( 500 ) );	// use a lower gravity for grenades to make them easier to shoot
	SetFriction( 0.8 );
	SetSequence( 0 );

	m_fSpawnTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose:  The grenade has a slight delay before it goes live.  That way the
//			 person firing it can bounce it off a nearby wall.  However if it
//			 hits another character it blows up immediately
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CGrenadeShrapnel::GrenadeShrapnelThink( void )
{
	SetNextThink( gpGlobals->curtime + 0.05f );

	if (!m_bIsLive)
	{
		// Go live after a short delay
		if (m_fSpawnTime + SHRAPNEL_NO_COLLIDE_TIME < gpGlobals->curtime)
		{
			m_bIsLive  = true;
		}
	}

	if (m_bIsLive)
	{
		Detonate();
	}
}

void CGrenadeShrapnel::Event_Killed( const CTakeDamageInfo &info )
{
	Detonate();
}

void CGrenadeShrapnel::GrenadeShrapnelTouch( CBaseEntity *pOther )
{
	Assert( pOther );

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

	BaseClass::BounceTouch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CGrenadeShrapnel::Detonate(void)
{
	if (!m_bIsLive)
	{
		return;
	}
	m_bIsLive		= false;
	m_takedamage	= DAMAGE_NO;	

	Vector vecForward = GetAbsVelocity();
	VectorNormalize(vecForward);
	trace_t		tr;
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + 60*vecForward, MASK_SHOT, 
		this, COLLISION_GROUP_NONE, &tr);
	UTIL_DecalTrace( &tr, "SmallScorch" );	//FIXME; Hook to unique/smaller decal 
	UTIL_ScreenShake( GetAbsOrigin(), 25.0, 150.0, 1.0, 300, SHAKE_START );

	EmitSound( "GrenadeShrapnel.Detonate" );
	CSoundEnt::InsertSound( SOUND_DANGER, GetAbsOrigin(), 100, 1.0 );	//BASEGRENADE_EXPLOSION_VOLUME

	// Fire out a whole bunch of bullets through the front
	FireBulletsInfo_t info;
	info.m_pAttacker = GetOwnerEntity();
	info.m_iShots = sk_shrapnel_bullet_amt.GetInt();
	info.m_vecSrc = GetAbsOrigin();
	info.m_vecDirShooting = vecForward;
	info.m_flDistance = 1024;
	info.m_iTracerFreq = 1;
	CAmmoDef *pAmmoDef = GetAmmoDef();
	info.m_iAmmoType = pAmmoDef->Index( "Buckshot" );
	info.m_iDamage = m_flDamage;
	info.m_vecSpread = SHRAPNEL_VECTOR_CONE;
	info.m_nFlags = (FIRE_BULLETS_TEMPORARY_DANGER_SOUND);
	FireBullets( info );

	// Explosion effect just for the fire trails
	CPASFilter filter( GetAbsOrigin() );
	te->Explosion( filter, 0.0,
		&GetAbsOrigin(), 
		g_sModelIndexFireball,
		3, 
		40,
		TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NOFIREBALL | TE_EXPLFLAG_NOFIREBALLSMOKE,
		0,
		0 );
	// Dont use explosion smoke, use the smaller smoke instead
	UTIL_Smoke( GetAbsOrigin(), random->RandomInt( 15, 20 ), 10 );

	UTIL_Remove( this );
}
