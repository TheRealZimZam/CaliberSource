//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Magazine-fed auto shotgun used by soldiers.
//
//			Primary attack: single barrel shot.
//			5-6 second reload
// TODO's: Allow firing right after a shot, but double the spread/recoil
//
//=============================================================================//

#include "cbase.h"
#include "NPCEvent.h"
#include "basehlcombatweapon_shared.h"
#include "basecombatcharacter.h"
#include "AI_BaseNPC.h"
#include "player.h"
#include "gamerules.h"		// For g_pGameRules
#include "in_buttons.h"
#include "soundent.h"
#include "game.h"
#include "vstdlib/random.h"
#include "gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar sk_auto_reload_time;
extern ConVar sk_plr_num_shotgun_pellets;

class CWeaponSuperShotgun : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( CWeaponSuperShotgun, CBaseHLCombatWeapon );

	DECLARE_SERVERCLASS();

private:
	bool	m_bDelayedFire1;	// Clang when finished reloading

public:
	void	Precache( void );

	int CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector AllyCone = VECTOR_CONE_3DEGREES;
		// 12 in SP, 15 in MP
	//	static Vector MPcone = VECTOR_CONE_15DEGREES;
		static Vector cone = VECTOR_CONE_12DEGREES;

		if( GetOwner() && (GetOwner()->Classify() == CLASS_PLAYER_ALLY_VITAL) )
		{
			// Allies fire shots more akin to slugs. Leave room-clearing for the player.
			return AllyCone;
		}

		return cone;
	}

	virtual int				GetMinBurst() { return 2; }
	virtual int				GetMaxBurst() { return 4; }

	virtual float			GetMinRestTime();
	virtual float			GetMaxRestTime();

	virtual float			GetFireRate( void );

	bool	Reload( void );
	void	Pump( void );
//	void	WeaponIdle( void );
	void	ItemHolsterFrame( void );
//	void	ItemPostFrame( void );
	void	PrimaryAttack( void );
	void	DryFire( void );
	
	void	FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	void	Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary );
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	
	float	WeaponAutoAimScale()	{ return 0.6f; }

	DECLARE_ACTTABLE();

	CWeaponSuperShotgun( void );
};

IMPLEMENT_SERVERCLASS_ST( CWeaponSuperShotgun, DT_WeaponSuperShotgun )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_supershotgun, CWeaponSuperShotgun );
PRECACHE_WEAPON_REGISTER( weapon_supershotgun );

BEGIN_DATADESC( CWeaponSuperShotgun )

	DEFINE_FIELD( m_bDelayedFire1, FIELD_BOOLEAN ),

END_DATADESC()

acttable_t	CWeaponSuperShotgun::m_acttable[] = 
{
	{ ACT_IDLE,						ACT_IDLE_SMG1,					true },	// FIXME: hook to shotgun unique

	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_SHOTGUN,			true },
	{ ACT_RELOAD,					ACT_RELOAD_SMG1,					false },	// FIXME: hook to shotgun unique
	{ ACT_WALK,						ACT_WALK_RIFLE,						true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SHOTGUN,				true },

// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_SHOTGUN_RELAXED,		false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_SHOTGUN_STIMULATED,	false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_SHOTGUN_AGITATED,		false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_RIFLE_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_RIFLE_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_RIFLE,				false },//always aims

// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG1_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_RIFLE_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_RIFLE_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_RIFLE,				false },//always aims
//End readiness activities

	{ ACT_WALK_AIM,					ACT_WALK_AIM_SHOTGUN,				true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,				true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,			true },
	{ ACT_RUN,						ACT_RUN_RIFLE,						true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_SHOTGUN,				true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,				true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,			true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_SHOTGUN,	true },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SHOTGUN_LOW,		true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SHOTGUN_LOW,				false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SHOTGUN,			false },
};

IMPLEMENT_ACTTABLE(CWeaponSuperShotgun);

void CWeaponSuperShotgun::Precache( void )
{
	CBaseCombatWeapon::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOperator - 
//-----------------------------------------------------------------------------
void CWeaponSuperShotgun::FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles )
{
	Vector vecShootOrigin, vecShootDir;
	CAI_BaseNPC *npc = pOperator->MyNPCPointer();
	ASSERT( npc != NULL );
	WeaponSound( SINGLE_NPC );
	pOperator->DoMuzzleFlash();
	m_iClip1 = m_iClip1 - 1;

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

	pOperator->FireBullets( 8, vecShootOrigin, vecShootDir, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponSuperShotgun::Operator_ForceNPCFire( CBaseCombatCharacter *pOperator, bool bSecondary )
{
	// Ensure we have enough rounds in the clip
//	m_iClip1++;

	FireNPCPrimaryAttack( pOperator, true );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponSuperShotgun::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
//	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	switch( pEvent->event )
	{
		case EVENT_WEAPON_SHOTGUN_FIRE:
		{
			FireNPCPrimaryAttack( pOperator, false );
		}
		break;

		default:
			CBaseCombatWeapon::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}
}


//-----------------------------------------------------------------------------
// Purpose:	When we shipped Caliber, the shotgun was op as fu**,
//			enemies could blow your brains out in less than a second,
//			cause' the rof was around 1 shot a second, so I nerfed it. (wm)
//-----------------------------------------------------------------------------
// Purpose: Cops and gangsters use shot regulator, not conscripts. Conscripts lose accuracy instead.
// Input  :
// Output :
//-----------------------------------------------------------------------------
float CWeaponSuperShotgun::GetMinRestTime()
{
	if( hl2_episodic.GetBool() && GetOwner() && GetOwner()->Classify() == CLASS_COMBINE )
	{
		return 1.2f;
	}
	if( hl2_episodic.GetBool() && GetOwner() && GetOwner()->Classify() == CLASS_METROPOLICE )
	{
		return 1.2f;
	}
	// Gangsters need to regulate too, cause' they also use the shotgun alot (ONLY APPLIES DURING PRECRIMINAL)
	if( GetOwner() && GetOwner()->Classify() == CLASS_CITIZEN_PASSIVE )
	{
		return 1.2f;
	}
	
	return BaseClass::GetMinRestTime();
}

//-----------------------------------------------------------------------------
float CWeaponSuperShotgun::GetMaxRestTime()
{
	if( hl2_episodic.GetBool() && GetOwner() && GetOwner()->Classify() == CLASS_COMBINE )
	{
		return 1.5f;
	}
	// Cops might pickup the gun, so regulate them too even if they are never "supposed" to get one
	if( hl2_episodic.GetBool() && GetOwner() && GetOwner()->Classify() == CLASS_METROPOLICE )
	{
		return 1.5f;
	}
	// Mobsters need to regulate too, cause' they also use the shotgun alot (ONLY APPLIES DURING PRECRIMINAL)
	if( GetOwner() && GetOwner()->Classify() == CLASS_CITIZEN_PASSIVE )
	{
		return 1.5f;
	}

	return BaseClass::GetMaxRestTime();
}

//-----------------------------------------------------------------------------
// Purpose: Time between successive shots in a burst. Also returned for EP2
//			with an eye to not messing up Alyx in EP1.
//-----------------------------------------------------------------------------
float CWeaponSuperShotgun::GetFireRate()
{
	if( GetOwner() && GetOwner()->Classify() == CLASS_METROPOLICE )
	{
		return 0.75f;
	}

	// Conscript troops do (need) to have a bit of a pause when firing, even without the clang
	if( GetOwner() && GetOwner()->Classify() == CLASS_COMBINE )
	{
		return 0.675f;
	}

	return 0.5f;
}

//-----------------------------------------------------------------------------
// Purpose: Play weapon clang anim
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponSuperShotgun::Pump( void )
{
	CBaseCombatCharacter *pOwner  = GetOwner();

	if ( pOwner == NULL )
		return;

	WeaponSound( SPECIAL1 );

	// Finish reload animation
	SendWeaponAnim( ACT_SHOTGUN_PUMP );

	pOwner->m_flNextAttack	= gpGlobals->curtime + SequenceDuration();
	m_flNextPrimaryAttack	= gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CWeaponSuperShotgun::DryFire( void )
{
	WeaponSound(EMPTY);
	SendWeaponAnim( ACT_VM_DRYFIRE );
	
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponSuperShotgun::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if ( !pPlayer )
	{
		return;
	}

	if ( m_iClip1 <= 0 )
	{
		if ( !m_bFireOnEmpty )
		{
			Reload();
		}
		else
		{
			WeaponSound( EMPTY );
			m_flNextPrimaryAttack = 0.15;
		}

		return;
	}
	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );

	WeaponSound(SINGLE);
	pPlayer->DoMuzzleFlash();

	// player """shoot""" animation
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	// Don't fire again until fire animation has completed
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
//	m_iClip1 -= 1;
	m_iClip1--;

	Vector	vecSrc		= pPlayer->Weapon_ShootPosition( );
	Vector	vecAiming	= pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );	

	// Fire the bullets, and force the first shot to be perfectly accuracy
	pPlayer->FireBullets( sk_plr_num_shotgun_pellets.GetInt(), vecSrc, vecAiming, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0, -1, -1, 0, NULL, true, true );
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );

	//Disorient the player
	pPlayer->ViewPunch( QAngle( random->RandomFloat( -2, -1 ), random->RandomFloat( -2, 2 ), 0 ) );

	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), SOUNDENT_VOLUME_SHOTGUN, 0.2, GetOwner() );

	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
	}

}

//-----------------------------------------------------------------------------
// Purpose: Override so shotgun can do mulitple reloads in a row
//-----------------------------------------------------------------------------
/*
void CWeaponSuperShotgun::ItemPostFrame( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if (!pOwner)
	{
		return;
	}

//	if ((m_bNeedPump) && (m_flNextPrimaryAttack <= gpGlobals->curtime))
//	{
//		Pump();
//		return;
//	}

}
*/
//-----------------------------------------------------------------------------
// Purpose: Override if we're waiting to release a shot
//-----------------------------------------------------------------------------

bool CWeaponSuperShotgun::Reload( void )
{

//	// Clang
//	if ( m_bNeedPump )
//	{
//		Pump();
//	}
//

	bool fRet = DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
	if ( fRet )
	{
		WeaponSound( RELOAD );
	}
	return fRet;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponSuperShotgun::CWeaponSuperShotgun( void )
{
	m_bReloadsSingly	= false;

	m_fMinRange1		= 0.0;
	m_fMaxRange1		= 768;
	m_fMinRange2		= 0.0;
	m_fMaxRange2		= 256;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void CWeaponSuperShotgun::ItemHolsterFrame( void )
{
	// Must be player held
	if ( GetOwner() && GetOwner()->IsPlayer() == false )
		return;

	// We can't be active
	if ( GetOwner()->GetActiveWeapon() == this )
		return;

}

//==================================================
// Purpose: 
//==================================================
/*
void CWeaponSuperShotgun::WeaponIdle( void )
{
	//Only the player fires this way so we can cast
	CBasePlayer *pPlayer = GetOwner()

	if ( pPlayer == NULL )
		return;

	//If we're on a target, play the new anim
	if ( pPlayer->IsOnTarget() )
	{
		SendWeaponAnim( ACT_VM_IDLE_ACTIVE );
	}
}
*/
