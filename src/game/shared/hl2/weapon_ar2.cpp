//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:	AR2 - Battle Rifle
//
//			Primary attack: Normal full-auto with decent accuracy.
//			Secondary attack: Sniper mode - Single shots with perfect accuracy.
// TODO's: Optimize
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "NPCevent.h"
#include "weapon_ar2.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "rumble_shared.h"
#include "shake.h"
#include "gamestats.h"
#include "baseplayer_shared.h"
#include "hl2_player_shared.h"

#ifdef CLIENT_DLL
	#include "c_te_effect_dispatch.h"
#else
	#include "basecombatweapon.h"
	#include "game.h"
	#include "basecombatcharacter.h"
	#include "AI_BaseNPC.h"
	#include "AI_Memory.h"
	#include "soundent.h"
	#include "te_effect_dispatch.h"
	//#include "grenade_ar2.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//###########################################################################
//	>> CWeaponAR2
//###########################################################################

extern ConVar sv_funmode;

ConVar sk_weapon_ar2_lerp( "sk_weapon_ar2_lerp", "5.0", FCVAR_REPLICATED );
ConVar sk_weapon_ar2_num_flechettes( "sk_weapon_ar2_num_flechettes", "6", FCVAR_REPLICATED );
ConVar sk_weapon_ar2_ducking_bonus( "sk_weapon_ar2_ducking_bonus", "0.25", FCVAR_REPLICATED | FCVAR_CHEAT );

#define AR2_ZOOM_RATE	0.3f	// Interval between zoom levels in seconds.

//=========================================================
//=========================================================

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponAR2, DT_WeaponAR2 )

BEGIN_NETWORK_TABLE( CWeaponAR2, DT_WeaponAR2 )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bShotDelayed ) ),
	RecvPropBool( RECVINFO( m_bZoomed ) ),
#else
	SendPropBool( SENDINFO( m_bShotDelayed ) ),
	SendPropBool( SENDINFO( m_bZoomed ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponAR2 )
	DEFINE_PRED_FIELD( m_bShotDelayed, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bZoomed, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_ar2, CWeaponAR2 );
PRECACHE_WEAPON_REGISTER(weapon_ar2);

acttable_t	CWeaponAR2::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_AR2,			true },
	{ ACT_RELOAD,					ACT_RELOAD_AR2,					true },	
	{ ACT_IDLE,						ACT_IDLE_AR2,					true },	
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_AR2,				true },	

	{ ACT_WALK,						ACT_WALK_RIFLE,					true },	// FIXME: hook to AR2 unique
	{ ACT_WALK_AIM,					ACT_WALK_AIM_RIFLE,				true },	// FIXME: hook to AR2 unique
	{ ACT_RUN,						ACT_RUN_RIFLE,					true },	// FIXME: hook to AR2 unique
	{ ACT_RUN_AIM,					ACT_RUN_AIM_AR2,				true },	// FIXME: hook to AR2 unique
	{ ACT_SPRINT,					ACT_RUN_RIFLE_STIMULATED,		true },

// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_AR2_RELAXED,			false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_AR2_STIMULATED,		false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_AR2,				false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_RIFLE_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_AR2,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_RIFLE_STIMULATED,			false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_AR2,				false },//always aims

// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_AR2_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_AR2,				false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_AR2,				false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_AR2,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_AR2_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_AR2_STIMULATED,		false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_AR2,				false },//always aims
//End readiness activities

	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,			true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,		true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,			true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,		true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_AR2,	false },
//	{ ACT_COVER_LOW,				ACT_COVER_SMG1_LOW,				false },		// FIXME: hook to AR2 unique
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_AR2_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_AR2_LOW,		true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_AR2_LOW,				false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_AR2,			true },
	{ ACT_RANGE_ATTACK2,			ACT_RANGE_ATTACK_AR2_GRENADE,	true },
};

IMPLEMENT_ACTTABLE(CWeaponAR2);

CWeaponAR2::CWeaponAR2( )
{
	// Non-Zoom range for AI
	m_fMinRange1	= 48;
	m_fMaxRange1	= 2048;

	// Zoom range
	m_fMinRange2	= 1024;
	m_fMaxRange2	= 2048;

	if ( sv_funmode.GetBool() )
	{
		// Lose the entire clip when reloading
		m_bReloadsFullClip	= true;
	}

	m_nShotsFired	= 0;
	m_nVentPose		= -1;

	// Cannot zoom in underwater
	m_bAltFiresUnderwater = false;
}

void CWeaponAR2::Precache( void )
{
	BaseClass::Precache();

#ifndef CLIENT_DLL
//	UTIL_PrecacheOther( "prop_combine_ball" );
//	UTIL_PrecacheOther( "env_entity_dissolver" );
//	UTIL_PrecacheOther("grenade_ar2");
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Give this weapon longer range when wielded by an ally NPC.
//-----------------------------------------------------------------------------
void CWeaponAR2::Equip( CBaseCombatCharacter *pOwner )
{
#ifdef GAME_DLL
	if( pOwner->Classify() == CLASS_PLAYER_ALLY )
	{
		m_fMaxRange1 = 2560;
		m_fMaxRange2 = 2560;
	}
#endif

	BaseClass::Equip( pOwner );
}

//-----------------------------------------------------------------------------
// Purpose: Offset the autoreload
//-----------------------------------------------------------------------------
/*
bool CWeaponAR2::Deploy( void )
{
	m_nShotsFired = 0;

	return BaseClass::Deploy();
}
*/

//-----------------------------------------------------------------------------
// Purpose: Handle grenade detonate in-air (even when no ammo is left)
//-----------------------------------------------------------------------------
void CWeaponAR2::ItemPostFrame( void )
{
	// Update our pose parameter for the vents
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner )
	{
		CBaseViewModel *pVM = pOwner->GetViewModel();

		if ( pVM )
		{
			if ( m_nVentPose == -1 )
			{
				m_nVentPose = pVM->LookupPoseParameter( "VentPoses" );
			}
			
			float flVentPose = RemapValClamped( m_nShotsFired, 0, 5, 0.0f, 1.0f );
			pVM->SetPoseParameter( m_nVentPose, flVentPose );
		}
	}

	//Zoom in
	if ( pOwner->m_afButtonPressed & IN_ATTACK2 )
		Zoom();

	//Don't kick the same when we're zoomed in
	if ( IsWeaponZoomed() )
	{
		m_fFireDuration = 0.05f;
	}

	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CWeaponAR2::GetPrimaryAttackActivity( void )
{
	if ( m_nShotsFired < 2 )
		return ACT_VM_PRIMARYATTACK;

	if ( m_nShotsFired < 3 )
		return ACT_VM_RECOIL1;
	
	if ( m_nShotsFired < 4 )
		return ACT_VM_RECOIL2;

	return ACT_VM_RECOIL3;
}

//-----------------------------------------------------------------------------
// Purpose: flechette shatter
// Input  : &tr - 
//			nDamageType - 
//-----------------------------------------------------------------------------
void CWeaponAR2::DoImpactEffect( trace_t &tr, int nDamageType )
{
	BaseClass::DoImpactEffect( tr, nDamageType );

	UTIL_ImpactTrace( &tr, nDamageType, "AR2Impact" );
}

//-----------------------------------------------------------------------------
// Purpose: Tracer frequency
//-----------------------------------------------------------------------------
void CWeaponAR2::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if (!pPlayer)
		return;

	// Abort here to handle burst and auto fire modes
	if ( (UsesClipsForAmmo1() && m_iClip1 == 0) || ( !UsesClipsForAmmo1() && !pPlayer->GetAmmoCount(m_iPrimaryAmmoType) ) )
		return;

	m_nShotsFired++;

	// To make the firing framerate independent, we may have to fire more than one bullet here on low-framerate systems, 
	// especially if the weapon we're firing has a really fast rate of fire.
	int iBulletsToFire = sk_weapon_ar2_num_flechettes.GetInt();

	// MUST call sound before removing a round from the clip of a CHLMachineGun
	WeaponSound( SINGLE );
	m_flNextPrimaryAttack = m_flNextPrimaryAttack + GetFireRate();

	// Fire the bullets
	FireBulletsInfo_t info;
	info.m_iShots = iBulletsToFire;
	info.m_vecSrc = pPlayer->Weapon_ShootPosition( );
	info.m_vecDirShooting = pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );
	info.m_vecSpread = GetBulletSpread();
	info.m_flDistance = MAX_TRACE_LENGTH;
	info.m_iAmmoType = m_iPrimaryAmmoType;
	info.m_iTracerFreq = 1;
	FireBullets( info );

	// Remove a round
	if ( UsesClipsForAmmo1() )
		m_iClip1 -= 1;
	else
		pPlayer->RemoveAmmo( 1, GetActiveAmmoType() );

	//Factor in the view kick
	AddViewKick();

	SendWeaponAnim( GetPrimaryAttackActivity() );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );
	pPlayer->DoMuzzleFlash();

#ifndef CLIENT_DLL
	pPlayer->SetAimTime( 3.0f );

	// Register a muzzleflash for the AI
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );

	CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, GetAbsOrigin(), SOUNDENT_VOLUME_MACHINEGUN, 0.2, pPlayer );
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
#endif

	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
	}

	m_iPrimaryAttacks++;
}

//-----------------------------------------------------------------------------
// Purpose: Energy Ball
//-----------------------------------------------------------------------------
void CWeaponAR2::SecondaryAttack( void )
{
	//NOTENOTE: The zooming is handled by the post/busy frames
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponAR2::Zoom( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if ( pPlayer == NULL )
		return;

#ifdef GAME_DLL
	CHL2_Player *pHL2Player = dynamic_cast<CHL2_Player*>(pPlayer);
	color32 ScopeGreen = { 30, 105, 20, 32 };	//100, 225, 150, 32

	if ( m_bZoomed )
	{
		// Zoom out to the default zoom level
		if ( pHL2Player )
			pHL2Player->StopZooming(this);
		else if ( pPlayer->SetFOV( this, 0, AR2_ZOOM_RATE ) )
		{
			pPlayer->ShowViewModel(true);
		}

		pPlayer->SetAnimation( PLAYER_LEAVE_AIMING );
		WeaponSound(SPECIAL2);
		m_bZoomed = false;
		UTIL_ScreenFade( pPlayer, ScopeGreen, 0.2f, 0, (FFADE_IN|FFADE_PURGE) );
	}
	else
	{
		// If underwater, do not zoom
		if (GetOwner()->GetWaterLevel() == 3 && m_bAltFiresUnderwater == false)
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
			return;
		}
		else
		{
			// Darken corners of the screen
			if ( pHL2Player && pHL2Player->CanZoom(this) )
			{
				pHL2Player->StartZooming(this, 35);
			}
			else if ( pPlayer->SetFOV( this, 35, AR2_ZOOM_RATE ) )
			{
				pPlayer->ShowViewModel(false);
			}
			else
				return;

			pPlayer->SetAnimation( PLAYER_START_AIMING );
			WeaponSound(SPECIAL1);
			m_bZoomed = true;
			UTIL_ScreenFade( pPlayer, ScopeGreen, 0.2f, 0, (FFADE_OUT|FFADE_PURGE|FFADE_STAYOUT) );
		}
	}
	// Client logic
#else
	if ( m_bZoomed )
	{
		// Zoom out to the default zoom level
		WeaponSound(SPECIAL2);
		m_bZoomed = false;
	}
	else
	{
		// If underwater, do not zoom
		if (GetOwner()->GetWaterLevel() == 3 && m_bAltFiresUnderwater == false)
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
			return;
		}
		else
		{
			SendWeaponAnim( ACT_VM_FIDGET );
			WeaponSound(SPECIAL1);
			m_bZoomed = true;
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Faster reload when crouched/stationary
//-----------------------------------------------------------------------------
float CWeaponAR2::GetDefaultAnimSpeed( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( GetOwner() && GetOwner()->IsPlayer() && m_bInReload )
	{
		if ( pPlayer->GetAbsVelocity().Length2D() > 180 )
			return 0.75;
		else if ( GetOwner()->GetFlags() & FL_DUCKING )
			return 1.0 + sk_weapon_ar2_ducking_bonus.GetFloat();
	}

	return BaseClass::GetDefaultAnimSpeed();
}


//-----------------------------------------------------------------------------
// Purpose: Override if we're waiting to release a shot
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponAR2::CanHolster( void )
{
	if ( m_bShotDelayed )
		return false;

	// Zoomout
	if ( IsWeaponZoomed() )
	{
		Zoom();
	}

	return BaseClass::CanHolster();
}

//-----------------------------------------------------------------------------
// Purpose: Reload mercy
//-----------------------------------------------------------------------------
bool CWeaponAR2::Reload( void )
{
	if ( m_bShotDelayed )
		return false;

	// Zoomout
	if ( IsWeaponZoomed() )
	{
		Zoom();
	}

	int iActivity = ACT_VM_RELOAD;
	if ( m_iClip1 > 1 )
	{
		iActivity = ACT_VM_RELOAD2;
	}

#ifndef CLIENT_DLL
	CSoundEnt::InsertSound( SOUND_WEAPON, GetAbsOrigin(), 384, 0.4, GetOwner() );
#endif

	return DefaultReload( GetMaxClip1(), GetMaxClip2(), iActivity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponAR2::AddViewKick( void )
{
	#define	EASY_DAMPEN			0.5f
	#define	MAX_VERTICAL_KICK	12.0f	//Degrees
	#define	SLIDE_LIMIT			5.0f	//Seconds
	
	//Get the view kick
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
		return;

	DoMachineGunKick( pPlayer, EASY_DAMPEN, MAX_VERTICAL_KICK, m_fFireDuration, SLIDE_LIMIT );
}

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CWeaponAR2::GetProficiencyValues()
{
	static WeaponProficiencyInfo_t proficiencyTable[] =
	{
		{ 7.0,		0.75	},
		{ 7.0,		0.75	},
		{ 5.00,		0.75	},
		{ 3.0,		0.85	},
		{ 5.0/3.0,	0.75	},
		{ 1.00,		1.0		},
	};

	COMPILE_TIME_ASSERT( ARRAYSIZE(proficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);

	return proficiencyTable;
}

#if !defined(CLIENT_DLL)
//-----------------------------------------------------------------------------
// Purpose: Adjust time between bursts for different npc types.
//-----------------------------------------------------------------------------
float CWeaponAR2::GetMinRestTime()
{
	//Default is 0.3
	return 0.6f;
}

//-----------------------------------------------------------------------------
float CWeaponAR2::GetMaxRestTime()
{
	// Default is 0.6
	Class_T OwnerClass = GetOwner()->Classify();

	if ( OwnerClass == CLASS_COMBINE )
		 return 1.0f;

	return 1.1f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOperator - 
//-----------------------------------------------------------------------------
void CWeaponAR2::FireNPCAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles )
{
	Vector vecShootOrigin, vecShootDir;

	CAI_BaseNPC *npc = pOperator->MyNPCPointer();
	ASSERT( npc != NULL );

	if ( bUseWeaponAngles )
	{
		QAngle	angShootDir;
		GetAttachment( LookupAttachment( "muzzle" ), vecShootOrigin, angShootDir );
		AngleVectors( angShootDir, &vecShootDir );
	}
	else 
	{
		vecShootOrigin = pOperator->Weapon_ShootPosition();
		vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );
	}

	WeaponSound( SINGLE_NPC );

	// NOTENOTE: This is overriden on the client-side
	pOperator->DoMuzzleFlash();
	m_iClip1 = m_iClip1 - 1;

	CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_MACHINEGUN, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy() );
	int	TracerFreq = 1;
	pOperator->FireBullets( sk_weapon_ar2_num_flechettes.GetInt(), vecShootOrigin, vecShootDir, pOperator->GetAttackSpread(this), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, TracerFreq );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponAR2::Operator_ForceNPCFire( CBaseCombatCharacter *pOperator, bool bSecondary )
{
	// Ensure we have enough rounds in the clip
	m_iClip1++;

	FireNPCAttack( pOperator, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CWeaponAR2::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_AR2:
		case EVENT_WEAPON_SMG1:
			{
				FireNPCAttack( pOperator, false );
			}
			break;

#if 0
		case EVENT_WEAPON_AR2_ALTFIRE:
			{
				Operator_ForceNPCFire( pOperator, true );
			}
#endif

		default:
			CBaseCombatWeapon::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}
}
#endif
