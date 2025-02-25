//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:	Flamethrower test
//
//			Primary attack: Throw flame
//			Secondary attack: Spew ignitable gas onto surfaces
// TODO's: Half this stuff needs to be moved to the client, grenade_ball is
// unsustainable as of now (source engine cant handle projectiles), 
// probably gonna have to do this with tracing like tf2... UGH!
//=============================================================================
#include "cbase.h"
#include "NPCevent.h"
#include "basecombatcharacter.h"
#include "player.h"
#include "weapon_flamethrower.h"
#include "grenade_ball.h"
#include "gamerules.h"
#include "game.h"
#include "in_buttons.h"
#include "AI_BaseNPC.h"
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


static const char *g_pFlameThrowerSound = "Weapon_Flamethrower.Flame";

extern ConVar sk_fireball_dmg;
extern ConVar sk_fireball_radius;
extern ConVar sk_fireball_life;

extern ConVar sv_funmode;
ConVar sk_flamethrower_velocity( "sk_flamethrower_velocity", "450.0", FCVAR_CHEAT | FCVAR_GAMEDLL, "Velocity of flameballs." );

//###########################################################################
//	>> CWeaponFlameThrower
//###########################################################################

BEGIN_DATADESC( CWeaponFlameThrower )

	DEFINE_FIELD( m_fDrainRate,	FIELD_TIME ),

	DEFINE_FIELD( m_bSoundOn,	FIELD_BOOLEAN ),
	
	DEFINE_FIELD( m_hDripSprite,	FIELD_EHANDLE ),
	DEFINE_FIELD( m_hPilotSprite,	FIELD_EHANDLE ),

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
	m_fDrainRate		= gpGlobals->curtime;
//	m_fPrefireWait		= FLAMETHROWER_PREFIRE_WAIT;
	m_flNextPrimaryAttack = 0.2;

	if ( sv_funmode.GetBool() )
	{
		m_bCanJam			= true;
	}

	// Always toss the whole tank
	m_bReloadsFullClip	= true;

	// Cannot fire underwater
	m_bFiresUnderwater	= false;
	m_bAltFiresUnderwater = false;

	m_hDripSprite = NULL;
	m_hPilotSprite = NULL;
}

CWeaponFlameThrower::~CWeaponFlameThrower()
{
	StopFlameSound();
	StopPilotLight();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFlameThrower::Precache( void )
{
	UTIL_PrecacheOther("grenade_ball");
	PrecacheModel("sprites/xfireball3.vmt");

	PrecacheScriptSound( g_pFlameThrowerSound );

	BaseClass::Precache();
}

float CWeaponFlameThrower::GetVelocity( void )
{
	return sk_flamethrower_velocity.GetFloat();	//FLAMETHROWER_VELOCITY
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponFlameThrower::ItemPostFrame()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;

	if ( pOwner->IsAlive() && (pOwner->m_nButtons & IN_ATTACK) )
	{
		PrimaryAttack();

		// Drain ammo - seperate from the actual attack function
		if ( m_fDrainRate <= gpGlobals->curtime )
		{
			if (UsesClipsForAmmo1())
				m_iClip1--;
			else
				pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );
			
			m_fDrainRate = gpGlobals->curtime + FLAMETHROWER_DRAINRATE;
		}
	}
	else
	{
		StopPilotLight();
		StopFlameSound();
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

	if ( (UsesClipsForAmmo1() && m_iClip1 == 0) )
	{
		Reload();
		return;
	}

	// Ignite!
	if ( !m_bIgnited )
	{
		StartPilotLight();
		return;
	}

	StartFlameSound();
	m_bFiring = true;

	// Fire a new ball.
	if ( gpGlobals->curtime >= m_flNextPrimaryAttack )
	{
		Vector vForward, vRight, vUp;
		pOwner->EyeVectors( &vForward, &vRight, &vUp );
		Vector vOrigin = pOwner->Weapon_ShootPosition();
		Vector vMuzzle;

		if ( !GetAttachment( "muzzle", vMuzzle ) )
		{
			// No muzzlepoint? Gotta manually specifiy the smaller details Old style.
			Vector vMuzzle = vOrigin + vForward * FLAMETHROWER_MUZZLEPOS_FORWARD 
				+ vRight * FLAMETHROWER_MUZZLEPOS_RIGHT + vUp * FLAMETHROWER_MUZZLEPOS_UP;
		}

		QAngle vecAngles;
		VectorAngles( vForward, vecAngles );

		Vector vVelocity = vForward * GetVelocity() + 
		(vRight * RandomFloat(-GetHSpread(),GetHSpread())) +	//FLAMETHROWER_SPREAD_ANGLE
		(vUp * RandomFloat(-GetVSpread(),GetVSpread()));	//FLAMETHROWER_SPREAD_VERTICAL

		// If we have multiple flamethrowers shooting at once, the entity queue is going
		// to get filled up, real quick. Cut the small/medium balls, just make the
		// muzzleflash/client-fire-spew effect bigger.
		int iFireballs = 3;
		if ( g_pGameRules->IsMultiplayer() )
			iFireballs = 1;

		for ( int i = 0; i < iFireballs; i++ )
		{
			CGrenadeBall *pGrenade = (CGrenadeBall*)CreateNoSpawn( "grenade_fireball", vMuzzle, vecAngles, pOwner );
			pGrenade->SetHissSound( "GrenadeFlame.Hiss" );
			pGrenade->SetHitSound( "GrenadeFlame.Hit" );
			pGrenade->Spawn( "sprites/xfireball3.vmt",
				3,
				vVelocity,
				pOwner,
				MOVETYPE_FLYGRAVITY,
				MOVECOLLIDE_FLY_SLIDE,
				FLAME_GRAVITY,
				NULL,
				sk_fireball_life.GetFloat());
			pGrenade->SetDamageType( DMG_BURN );

			// 0 is the main fireball
			if ( i == 0 )
			{
				pGrenade->SetDamage( sk_fireball_dmg.GetFloat() );
				pGrenade->SetDamageRadius( sk_fireball_radius.GetFloat() );
				pGrenade->SetHitSound( "GrenadeFlame.Hit" );
			}
			else
			{
				UTIL_SetSize( pGrenade, -Vector( 1.0f, 1.0f, 1.0f ), Vector( 1.0f, 1.0f, 1.0f ) );
				pGrenade->SetAbsVelocity( vVelocity + RandomFloat( -48.0f, 12.0f ) );
				pGrenade->SetDamage( 0 );
				pGrenade->SetDamageRadius( 0 );
			}

			// Tumble through the air
			pGrenade->SetLocalAngularVelocity(
				QAngle( random->RandomFloat( -250, -500 ),
						random->RandomFloat( -250, -500 ),
						random->RandomFloat( -250, -500 ) ) );
		}

		SendWeaponAnim( GetPrimaryAttackActivity() );
		pOwner->SetAnimation( PLAYER_ATTACK1 );
		pOwner->SetAimTime( 3.0f );

		pOwner->RumbleEffect( RUMBLE_SMG1, 0, RUMBLE_FLAG_RESTART );

		m_iPrimaryAttacks++;
		gamestats->Event_WeaponFired( pOwner, true, GetClassname() );

		m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponFlameThrower::SecondaryAttack()
{
	// Only the player fires this way so we can cast
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;

	if ( (UsesClipsForAmmo1() && m_iClip1 == 0) )
	{
		Reload();
		return;
	}

	// Unignite!
	if ( m_bIgnited )
	{
		StopPilotLight();
		return;
	}

	StopFlameSound();
	m_bFiring = true;

	//TODO; Spew gas decals and create dormant env_fires
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFlameThrower::StartFlameSound()
{
	if ( !m_bSoundOn )
	{
		CPASAttenuationFilter filter( GetOwner() );
		EmitSound( filter, GetOwner()->entindex(), g_pFlameThrowerSound ); 
		m_bSoundOn = true;
	}
}

void CWeaponFlameThrower::StopFlameSound()
{
	if ( m_bSoundOn )
	{
		if ( m_fFireDuration > 0.5f )	//gpGlobals->curtime + 0.5f
			WeaponSound( EMPTY );

		StopSound( entindex(), g_pFlameThrowerSound );
		m_bSoundOn = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponFlameThrower::StartPilotLight()
{
	if ( !m_bIgnited )
	{
		SendWeaponAnim( ACT_VM_FIDGET );
		m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
		WeaponSound( SPECIAL1 );
		m_bIgnited = true;
		return true;
	}

	return false;
}

bool CWeaponFlameThrower::StopPilotLight()
{
	if ( m_bIgnited )
	{
		SendWeaponAnim( ACT_VM_FIDGET );
		m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
		WeaponSound( SPECIAL2 );
		m_bIgnited = false;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
bool CWeaponFlameThrower::CanHolster( void )
{
	// Unignite first
	if (StopPilotLight())
		return false;

	return BaseClass::CanHolster();
}

bool CWeaponFlameThrower::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	// Stop all my effects
	StopFlameSound();
	StopPilotLight();
	m_bFiring = false;

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: Override if we're waiting to release a shot
//-----------------------------------------------------------------------------
bool CWeaponFlameThrower::Reload( void )
{
	// Unignite first
	if (StopPilotLight())
		return false;

	StopFlameSound();

	return DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
}
