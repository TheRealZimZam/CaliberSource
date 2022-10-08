//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:	Flamethrower test
//
//			Primary attack: Throw flame
//			Secondary attack: Eject ignitable gas onto surfaces
// TODO's: Actually properly code this thing
//=============================================================================
#include "cbase.h"
#include "basecombatweapon.h"
#include "NPCevent.h"
#include "basecombatcharacter.h"
#include "AI_BaseNPC.h"
#include "player.h"
#include "weapon_flamethrower.h"
#include "grenade_ball.h"
#include "gamerules.h"
#include "game.h"
#include "in_buttons.h"
#include "AI_Memory.h"
#include "soundent.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "hl2_player.h"
#include "EntityFlame.h"
#include "te_effect_dispatch.h"
#include "rumble_shared.h"
#include "gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar sv_funmode;

static const char *g_pFlameThrowerSound = "Weapon_Flamethrower.Flame";

//###########################################################################
//	>> CWeaponFlameThrower
//###########################################################################

BEGIN_DATADESC( CWeaponFlameThrower )

	DEFINE_FIELD( m_fDrainRate,	FIELD_FLOAT ),

	DEFINE_FIELD( m_bSoundOn,	FIELD_BOOLEAN ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CWeaponFlameThrower, DT_WeaponFlameThrower)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_flamethrower, CWeaponFlameThrower );
PRECACHE_WEAPON_REGISTER(weapon_flamethrower);

acttable_t	CWeaponFlameThrower::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_FLAMETHROWER,	true },
	{ ACT_RELOAD,					ACT_RELOAD_AR2,					true },	
	{ ACT_IDLE,						ACT_IDLE_ANGRY_FLAMETHROWER,	true },	
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_FLAMETHROWER,	true },	

	{ ACT_WALK,						ACT_WALK_FLAMETHROWER,			true },
	{ ACT_WALK_AIM,					ACT_WALK_FLAMETHROWER,			true },
	{ ACT_RUN,						ACT_RUN_FLAMETHROWER,			true },
	{ ACT_RUN_AIM,					ACT_RUN_FLAMETHROWER,			true },

	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_FLAMETHROWER,	false },
};

IMPLEMENT_ACTTABLE(CWeaponFlameThrower);

CWeaponFlameThrower::CWeaponFlameThrower( )
{
	m_fMinRange1		= 0.0;
	m_fMaxRange1		= 512;
	m_fMinRange2		= 512;
	m_fMaxRange2		= 768;
	m_fDrainRate		= FLAMETHROWER_DRAINRATE;
	m_flNextPrimaryAttack = 0.2;

	if ( !sv_funmode.GetBool() )
	{
		m_bCanJam			= true;
	}

	// Cannot fire underwater
	m_bFiresUnderwater	= false;
	m_bAltFiresUnderwater = false;
}

void CWeaponFlameThrower::Precache( void )
{
	UTIL_PrecacheOther("grenade_fireball");
	
	PrecacheScriptSound( g_pFlameThrowerSound );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponFlameThrower::ItemPostFrame()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;

	if ( pOwner->IsAlive() && 
		(pOwner->m_nButtons & IN_ATTACK) &&
		 pOwner->GetAmmoCount(m_iPrimaryAmmoType) > 0 )
	{
		StartSound();
		PrimaryAttack();
		m_bFiring = true;
	}
	else
	{
		StopFlameSound();
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f;
		m_bFiring = false;
	}
	
	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponFlameThrower::PrimaryAttack()
{
	// Only the player fires this way so we can cast
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;

	Vector vOrigin = pOwner->Weapon_ShootPosition( );
	GetAttachment( "muzzle", vOrigin );	//TEMP - using rpg model so yeah

	// Fire a new ball.
	if ( gpGlobals->curtime >= m_flNextPrimaryAttack )
	{
		for ( int i = 0; i < 3; i++ )
		{
			CGrenadeFireball *pGrenade = (CGrenadeFireball*)Create( "grenade_fireball", vOrigin, vec3_angle, pOwner );
			DispatchSpawn( pGrenade );
			
			if ( i == 0 )
			{
				pGrenade->SetSpitType( FIRE, LARGE );
				pGrenade->SetAbsVelocity( GetVelocity() );
			}
			else
			{
				pGrenade->SetSpitType( FIRE, random->RandomInt( SMALL, MEDIUM ) );
				pGrenade->SetAbsVelocity( GetVelocity() + RandomFloat( -18.0f, 9.0f ) );
			}

			pGrenade->SetThrower( pOwner );
			pGrenade->SetOwnerEntity( GetOwner() );

			// Tumble through the air
			pGrenade->SetLocalAngularVelocity(
				QAngle( random->RandomFloat( -250, -500 ),
						random->RandomFloat( -250, -500 ),
						random->RandomFloat( -250, -500 ) ) );
		}

		pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );
		SendWeaponAnim( GetPrimaryAttackActivity() );
		pOwner->SetAnimation( PLAYER_ATTACK1 );
		pOwner->SetAimTime( 3.0f );

		m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFlameThrower::StartSound()
{
	if ( !m_bSoundOn )
	{
		WeaponSound( SPECIAL1 );

		CPASAttenuationFilter filter( GetOwner() );
		EmitSound( filter, GetOwner()->entindex(), g_pFlameThrowerSound ); 

		m_bSoundOn = true;
	}
}

void CWeaponFlameThrower::StopFlameSound()
{
	if ( m_bSoundOn )
	{
		WeaponSound( EMPTY );
		StopSound( entindex(), g_pFlameThrowerSound );
		m_bSoundOn = false;
	}
}

//-----------------------------------------------------------------------------
bool CWeaponFlameThrower::CanHolster( void )
{
	if ( m_bFiring )
		return false;
	
	return BaseClass::CanHolster();
}

bool CWeaponFlameThrower::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	m_bFiring = false;

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: Override if we're waiting to release a shot
//-----------------------------------------------------------------------------
bool CWeaponFlameThrower::Reload( void )
{
	// No reloading if we're currently spewing, gotta unignite first
	if ( m_bFiring )
		return false;

	return DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
}
