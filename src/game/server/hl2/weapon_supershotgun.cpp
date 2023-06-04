//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Magazine-fed auto shotgun used by soldiers.
//
//			Primary attack: single barrel shot
//			Secondary attack:
//
// TODO's: Use machinegun recoil, do 1 damage to the player per shot after constant firing after 3 rounds
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

#define VECTOR_CONE_SUPERSHOTGUN		Vector( 0.12206, 0.12206, 0.11334 )
#define SUPERSHOTGUN_KICKBACK			2	// Base kickback

ConVar sk_weapon_supershotgun_lerp( "sk_weapon_supershotgun_lerp", "5.0" );

extern ConVar sk_auto_reload_time;
extern ConVar sk_plr_num_shotgun_pellets;
extern ConVar sk_npc_num_shotgun_pellets;
extern ConVar sv_funmode;

class CWeaponSuperShotgun : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( CWeaponSuperShotgun, CBaseHLCombatWeapon );

	DECLARE_SERVERCLASS();

	void	Precache( void );

	int CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector AllyCone = VECTOR_CONE_4DEGREES;
		static Vector cone = VECTOR_CONE_SUPERSHOTGUN;

		if ( GetHSpread() != NULL )
		{
			// We got a cone, use that instead
			cone = Vector( GetHSpread(), (GetHSpread() / 2) + (GetVSpread() / 2), GetVSpread() );
		}
		if( GetOwner() && (GetOwner()->Classify() == CLASS_PLAYER_ALLY_VITAL) )
		{
			// Story allies fire shots more akin to slugs. Leave room-clearing for the player.
			return AllyCone;
		}

		return cone;
	}

	virtual int				GetMinBurst() { return 2; }
	virtual int				GetMaxBurst() { return 2; }

	virtual float			GetMinRestTime();
	virtual float			GetMaxRestTime();

	virtual float			GetFireRate( void );

	Activity	GetPrimaryAttackActivity( void );

	bool	Reload( void );
//	void	WeaponIdle( void );
	void	ItemPostFrame( void );
	void	ItemHolsterFrame( void );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );

	const char *GetTracerType( void ) { return "ShotgunTracer"; }
	void	FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	void	Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary );
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	
	float	WeaponAutoAimScale()	{ return 0.6f; }

	DECLARE_ACTTABLE();

	CWeaponSuperShotgun( void );

private:
	float	m_flLastAttackTime;
	int		m_nNumShotsFired;
	bool	m_bLastfire;
};

IMPLEMENT_SERVERCLASS_ST( CWeaponSuperShotgun, DT_WeaponSuperShotgun )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_supershotgun, CWeaponSuperShotgun );
PRECACHE_WEAPON_REGISTER( weapon_supershotgun );

BEGIN_DATADESC( CWeaponSuperShotgun )

	DEFINE_FIELD( m_flLastAttackTime, FIELD_TIME ),
	DEFINE_FIELD( m_nNumShotsFired, FIELD_INTEGER ),

END_DATADESC()

acttable_t	CWeaponSuperShotgun::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_SHOTGUN,			true },		//ACT_RANGE_ATTACK_SUPERSHOTGUN
	{ ACT_RELOAD,					ACT_RELOAD_SMG1,					false },	// FIXME: hook to shotgun unique
	{ ACT_IDLE,						ACT_IDLE_RIFLE,						true },		// FIXME: hook to shotgun unique
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SHOTGUN,				true },

	{ ACT_WALK,						ACT_WALK_RIFLE,						true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_SHOTGUN,				true },
	{ ACT_RUN,						ACT_RUN_RIFLE,						true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_SHOTGUN,				true },
	{ ACT_SPRINT,					ACT_RUN_RIFLE_STIMULATED,			true },

// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_SHOTGUN_RELAXED,		false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_SHOTGUN_STIMULATED,	false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_SHOTGUN_AGITATED,		false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_RIFLE_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_SHOTGUN,			false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_RIFLE_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_SHOTGUN,			false },//always aims

// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SHOTGUN_RELAXED,		false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SHOTGUN,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_RIFLE_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_SHOTGUN,			false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_RIFLE_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_SHOTGUN,			false },//always aims
//End readiness activities

	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,				true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,			true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,				true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,			true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_SHOTGUN,	true },	//ACT_GESTURE_RANGE_ATTACK_SUPERSHOTGUN
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SHOTGUN_LOW,		true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SHOTGUN_LOW,				false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SMG1,			false },
};

IMPLEMENT_ACTTABLE(CWeaponSuperShotgun);

void CWeaponSuperShotgun::Precache( void )
{
	CBaseCombatWeapon::Precache();

	PrecacheScriptSound( "Weapon_SuperShotgun.Lastfire" );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponSuperShotgun::CWeaponSuperShotgun( void )
{
	m_bReloadsSingly	= false;
	m_bUsingSecondaryAmmo = false;
	if ( sv_funmode.GetBool() )
	{
		m_bReloadsFullClip	= true;
		m_bCanJam			= true;
	}

	m_fMinRange1		= 0.0;
	m_fMaxRange1		= 768;
	
	m_nNumShotsFired = 0;
	m_bLastfire = false;
	m_flLastAttackTime = gpGlobals->curtime;
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

	// Last round in the clip, clang after next shot
	if ( m_iClip1 <= 1 )
	{
		m_bLastfire = true;
	}

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

	int	TracerFreq = random->RandomInt( 2, 3 );
	pOperator->FireBullets( sk_npc_num_shotgun_pellets.GetInt(), vecShootOrigin, vecShootDir, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, TracerFreq );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponSuperShotgun::Operator_ForceNPCFire( CBaseCombatCharacter *pOperator, bool bSecondary )
{
	// Ensure we have enough rounds in the clip
	m_iClip1++;

	FireNPCPrimaryAttack( pOperator, true );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponSuperShotgun::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
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
// Purpose: Cops and gangsters use shot regulator, not conscripts. Conscripts lose accuracy instead.
// Input  :
// Output :
//-----------------------------------------------------------------------------
float CWeaponSuperShotgun::GetMinRestTime()
{
	Class_T OwnerClass = GetOwner()->Classify();

	if ( OwnerClass == CLASS_CITIZEN_PASSIVE	||
		 OwnerClass == CLASS_METROPOLICE	)
		 return 0.9f;

	return 0.6f;
}

//-----------------------------------------------------------------------------
float CWeaponSuperShotgun::GetMaxRestTime()
{
	Class_T OwnerClass = GetOwner()->Classify();

	if ( OwnerClass == CLASS_CITIZEN_PASSIVE	||
		 OwnerClass == CLASS_METROPOLICE	)
		 return 1.2f;

	return 0.9f;
}

//-----------------------------------------------------------------------------
// Purpose: Time between successive shots in a burst. Also returned for EP2
//			with an eye to not messing up Alyx in EP1.
//-----------------------------------------------------------------------------
float CWeaponSuperShotgun::GetFireRate()
{
	if ( GetOwner() && GetOwner()->IsNPC() )
	{
		// NPC value
		return BaseClass::GetCycleTime() * 1.25;
	}

	return BaseClass::GetCycleTime();
}

//-----------------------------------------------------------------------------
// Purpose: Reload mercy
//-----------------------------------------------------------------------------
bool CWeaponSuperShotgun::Reload( void )
{
	int iActivity = ACT_VM_RELOAD;
	if ( m_iClip1 > 1 )
	{
		iActivity = ACT_VM_RELOAD2;
	}

	return DefaultReload( GetMaxClip1(), GetMaxClip2(), iActivity );
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
		DryFire();
		Reload();
		return;
	}

	m_nNumShotsFired++;
	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );

	WeaponSound(SINGLE);
	pPlayer->DoMuzzleFlash();

	// player """shoot""" animation
	SendWeaponAnim( GetPrimaryAttackActivity() );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );
	pPlayer->SetAimTime( 2.0f );

	// Don't fire again until fire animation has completed
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	m_flLastAttackTime = gpGlobals->curtime + SequenceDuration();
	m_iClip1--;

	// Last round in the clip, clang after next shot
	if ( m_iClip1 == 1 )
	{
		m_bLastfire = true;
	}

	Vector	vecSrc		= pPlayer->Weapon_ShootPosition( );
	Vector	vecAiming	= pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );	

	// Fire the bullets, and force the first shot to be perfectly accuracy
	pPlayer->FireBullets( sk_plr_num_shotgun_pellets.GetInt(), vecSrc, vecAiming, GetBulletSpread(), MAX_TRACE_LENGTH, GetActiveAmmoType(), 2, -1, -1, 0, NULL, true, true );
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );

	//Disorient the player
	pPlayer->ViewPunch( QAngle( random->RandomFloat( -SUPERSHOTGUN_KICKBACK - m_nNumShotsFired, -1 ), random->RandomFloat( -SUPERSHOTGUN_KICKBACK - m_nNumShotsFired, SUPERSHOTGUN_KICKBACK + m_nNumShotsFired ), 0 ) );

	Vector	recoilForce = pPlayer->BodyDirection2D() * -( SUPERSHOTGUN_KICKBACK + (sk_plr_num_shotgun_pellets.GetInt() * m_nNumShotsFired) );
	recoilForce[2] += 96.0f;
	pPlayer->ApplyAbsVelocityImpulse( recoilForce );

	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), SOUNDENT_VOLUME_SHOTGUN, 0.2, GetOwner() );

	if (!m_iClip1 && pPlayer->GetAmmoCount(GetActiveAmmoType()) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Activity CWeaponSuperShotgun::GetPrimaryAttackActivity( void )
{
	if ( m_nNumShotsFired <= 1 )
		return ACT_VM_PRIMARYATTACK;

	//TEMP; Shotgun doesnt have recoil anims yet
/*
	if ( m_nNumShotsFired < 3 )
		return ACT_VM_RECOIL1;

	if ( m_nNumShotsFired < 5 )
		return ACT_VM_RECOIL2;
*/
	return ACT_VM_PRIMARYATTACK;	//ACT_VM_RECOIL3
}

//-----------------------------------------------------------------------------
// Purpose: Stab
//-----------------------------------------------------------------------------
void CWeaponSuperShotgun::SecondaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;

	pOwner->m_flNextAttack = gpGlobals->curtime + 1;
	SwitchAmmoType();

	m_iSecondaryAttacks++;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponSuperShotgun::ItemPostFrame( void )
{
	// Clang
	if ( m_bLastfire )
	{
		CPASAttenuationFilter filter( GetOwner() );
		EmitSound( filter, GetOwner()->entindex(), "Weapon_SuperShotgun.Lastfire" ); 

		m_bLastfire = false;
	}

	// Reset to default kick
	if ( ( gpGlobals->curtime - m_flLastAttackTime ) > 0.5f )
	{
		m_nNumShotsFired = 0;
	}

	BaseClass::ItemPostFrame();
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

	if ( ( gpGlobals->curtime - m_flHolsterTime ) > sk_auto_reload_time.GetFloat() )
	{
		// Reset the timer
		m_flHolsterTime = gpGlobals->curtime;
	
		if ( GetOwner() == NULL )
			return;

		if ( m_iClip1 == GetMaxClip1() )
			return;

		// Just load the clip with no animations
		int ammoFill = min( (GetMaxClip1() - m_iClip1), GetOwner()->GetAmmoCount( GetPrimaryAmmoType() ) );
		
		GetOwner()->RemoveAmmo( ammoFill, GetPrimaryAmmoType() );
		m_iClip1 += ammoFill;
	}
}

//==================================================
// Purpose: 
//==================================================
#if 0
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
	
	m_nNumShotsFired = 0;
	
	BaseClass::WeaponIdle();
}
#endif
