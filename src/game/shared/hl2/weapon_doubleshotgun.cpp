//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Double barrel DOOMGun.
//
//			Primary attack: single barrel shot
//			Secondary attack: Two barrel shot
//
//=============================================================================//

#include "cbase.h"
#include "NPCEvent.h"
#include "basehlcombatweapon_shared.h"
#include "baseplayer_shared.h"
#include "gamerules.h"		// For g_pGameRules
#include "in_buttons.h"
#include "vstdlib/random.h"
#include "gamestats.h"
#ifndef CLIENT_DLL
	#include "basecombatcharacter.h"
	#include "AI_BaseNPC.h"
	#include "game.h"
	#include "soundent.h"
	#include "te_effect_dispatch.h"
#else
	#include "c_te_effect_dispatch.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define VECTOR_CONE_DOUBLESHOTGUN		Vector( 0.12206, 0.12206, 0.11334 )
#define DOUBLESHOTGUN_KICKBACK			2.5	// Base kickback

extern ConVar sk_plr_num_shotgun_pellets;
extern ConVar sk_npc_num_shotgun_pellets;
extern ConVar sv_funmode;

#ifdef CLIENT_DLL
#define CWeaponDoubleShotgun C_WeaponSuperShotgun
#endif
class CWeaponDoubleShotgun : public CBaseHLCombatWeapon
{
	DECLARE_CLASS( CWeaponDoubleShotgun, CBaseHLCombatWeapon );
public:

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	void	Precache( void );

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector AllyCone = VECTOR_CONE_4DEGREES;
		static Vector cone = VECTOR_CONE_DOUBLESHOTGUN;

		if ( GetHSpread() != NULL )
		{
			// We got a cone, use that instead
			cone = Vector( GetHSpread(), (GetHSpread() / 2) + (GetVSpread() / 2), GetVSpread() );
		}
#ifdef GAME_DLL
		if( GetOwner() && (GetOwner()->Classify() == CLASS_PLAYER_ALLY_VITAL) )
		{
			// Story allies fire shots more akin to slugs. Leave room-clearing for the player.
			return AllyCone;
		}
#endif
		return cone;
	}

	virtual int				GetMinBurst() { return 1; }
	virtual int				GetMaxBurst() { return 1; }

	virtual float			GetFireRate( void );

	Activity	GetPrimaryAttackActivity( void );

	bool	Reload( void );
//	void	WeaponIdle( void );
	void	ItemPostFrame( void );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );

	const char *GetTracerType( void ) { return "ShotgunTracer"; }
	void	DoImpactEffect( trace_t &tr, int nDamageType );

#ifndef CLIENT_DLL
	virtual float			GetMinRestTime();
	virtual float			GetMaxRestTime();
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	void	FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	void	Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary );
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
#endif
	float	WeaponAutoAimScale()	{ return 0.6f; }

	DECLARE_ACTTABLE();

	CWeaponDoubleShotgun( void );

};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponDoubleShotgun, DT_WeaponDoubleShotgun )

BEGIN_NETWORK_TABLE( CWeaponDoubleShotgun, DT_WeaponSuperShotgun )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponDoubleShotgun )
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_doubleshotgun, CWeaponDoubleShotgun );
PRECACHE_WEAPON_REGISTER( weapon_doubleshotgun );

acttable_t	CWeaponDoubleShotgun::m_acttable[] = 
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

	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,				true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,			true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,				true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,			true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_SHOTGUN,	true },	//ACT_GESTURE_RANGE_ATTACK_SUPERSHOTGUN
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SHOTGUN_LOW,		true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SHOTGUN_LOW,				false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SMG1,			false },
};

IMPLEMENT_ACTTABLE(CWeaponDoubleShotgun);

void CWeaponDoubleShotgun::Precache( void )
{
	CBaseCombatWeapon::Precache();

	PrecacheScriptSound( "Weapon_DOOMShotgun.Open" );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponDoubleShotgun::CWeaponDoubleShotgun( void )
{
	m_bReloadsSingly	= false;
	m_bUsingSecondaryAmmo = false;
	if ( sv_funmode.GetBool() )
		m_bCanJam			= true;

	m_fMinRange1		= 0.0;
	m_fMaxRange1		= 768;
}

//-----------------------------------------------------------------------------
// Purpose: Rocket shatter
// Input  : &tr - 
//			nDamageType - 
//-----------------------------------------------------------------------------
void CWeaponDoubleShotgun::DoImpactEffect( trace_t &tr, int nDamageType )
{
	// Make a DOOM style sprite
	CEffectData data;
	data.m_vOrigin = tr.endpos + ( tr.plane.normal * 1.0f );
	data.m_vNormal = tr.plane.normal;
	DispatchEffect( "AR2Impact", data );

	BaseClass::DoImpactEffect( tr, nDamageType );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CWeaponDoubleShotgun::GetFireRate()
{
	return 0.1f;
}

//-----------------------------------------------------------------------------
// Purpose: Reload one shell
//-----------------------------------------------------------------------------
bool CWeaponDoubleShotgun::Reload( void )
{
	int iActivity = ACT_VM_RELOAD;
	// If theres a shell in the gun, only put in one shell
	if ( m_iClip1 == 1 )
	{
		// SLIGHTLY Shorter reload
		iActivity = ACT_VM_RELOAD2;
	}

	return DefaultReload( GetMaxClip1(), GetMaxClip2(), iActivity );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponDoubleShotgun::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if ( !pPlayer )
		return;

	if ( m_iClip1 <= 0 )
	{
		DryFire();
		Reload();
		return;
	}

	m_nNumShotsFired++;
	m_iPrimaryAttacks++;

	WeaponSound(SINGLE);
	pPlayer->DoMuzzleFlash();

	// player """shoot""" animation
	SendWeaponAnim( GetPrimaryAttackActivity() );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	// Don't fire again until fire animation has completed
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	m_flLastAttackTime = gpGlobals->curtime + SequenceDuration();
	m_iClip1--;

	Vector	vecSrc		= pPlayer->Weapon_ShootPosition( );
	Vector	vecAiming	= pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );	

	// Fire the bullets, and force the first shot to be perfectly accuracy
	FireBulletsInfo_t info;
	info.m_iShots = sk_plr_num_shotgun_pellets.GetInt();
	info.m_vecSrc = vecSrc;
	info.m_vecDirShooting = vecAiming;
	info.m_vecSpread = GetBulletSpread();
	info.m_flDistance = MAX_TRACE_LENGTH;
	info.m_iAmmoType = GetActiveAmmoType();
	info.m_iTracerFreq = 2;
	pPlayer->FireBullets( info );

#ifndef CLIENT_DLL
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );
	pPlayer->SetAimTime( 2.0f );
	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), SOUNDENT_VOLUME_SHOTGUN, 0.2, GetOwner() );
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );

	//Disorient the player
	Vector	recoilForce = pPlayer->BodyDirection2D() * -( DOUBLESHOTGUN_KICKBACK + (sk_plr_num_shotgun_pellets.GetInt() * m_nNumShotsFired) );
	recoilForce[2] += 96.0f;
	pPlayer->ApplyAbsVelocityImpulse( recoilForce );
#endif

	//Disorient the player
	pPlayer->ViewPunch( QAngle( random->RandomFloat( -DOUBLESHOTGUN_KICKBACK - m_nNumShotsFired, -1 ), random->RandomFloat( -DOUBLESHOTGUN_KICKBACK - m_nNumShotsFired, DOUBLESHOTGUN_KICKBACK + m_nNumShotsFired ), 0 ) );

	if (!m_iClip1 && pPlayer->GetAmmoCount(GetActiveAmmoType()) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Activity CWeaponDoubleShotgun::GetPrimaryAttackActivity( void )
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
// Purpose: Double fire
//-----------------------------------------------------------------------------
void CWeaponDoubleShotgun::SecondaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
		return;

	if ( m_iClip1 <= 1 )
	{
		DryFire();
		return;
	}

	pPlayer->m_nButtons &= ~IN_ATTACK2;
	// MUST call sound before removing a round from the clip of a CMachineGun
	WeaponSound(WPN_DOUBLE);

	//TODO; Do two muzzle flashes OR put some new logic in the fire effect
	pPlayer->DoMuzzleFlash();

	// player "shoot" animation
	SendWeaponAnim( ACT_VM_SECONDARYATTACK );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	// Don't fire again until fire animation has completed
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	m_iClip1 -= 2;	// Shotgun uses same clip for primary and secondary attacks

	Vector vecSrc	 = pPlayer->Weapon_ShootPosition();
	Vector vecAiming = pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );	

	// Fire the bullets, and force the first shot to be perfectly accuracy
	int NumPellets = sk_plr_num_shotgun_pellets.GetInt() * 2;
	FireBulletsInfo_t info;
	info.m_iShots = NumPellets;
	info.m_vecSrc = vecSrc;
	info.m_vecDirShooting = vecAiming;
	info.m_vecSpread = GetBulletSpread();
	info.m_flDistance = MAX_TRACE_LENGTH;
	info.m_iAmmoType = GetActiveAmmoType();
	info.m_iTracerFreq = 2;
	pPlayer->FireBullets( info );

	pPlayer->ViewPunch( QAngle( -(NumPellets/2), random->RandomFloat( -(SHOTGUN_KICKBACK + 1), (SHOTGUN_KICKBACK + 1) ), 0 ) );
#ifndef CLIENT_DLL
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 1.0 );
	pPlayer->SetAimTime( 4.0f );

	//Disorient the player
	Vector	recoilForce = pPlayer->BodyDirection3D() * -( NumPellets * 5.0f );
	recoilForce[2] += 128.0f;
	pPlayer->ApplyAbsVelocityImpulse( recoilForce );

	if ( pPlayer->GetHealth() > 2 )
		pPlayer->TakeDamage( CTakeDamageInfo( this, this, 1, DMG_CLUB ) );
#endif

	if (!m_iClip1 && pPlayer->GetAmmoCount(GetPrimaryAmmoType()) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
	}

#ifndef CLIENT_DLL
	CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, GetAbsOrigin(), SOUNDENT_VOLUME_SHOTGUN, 0.2 );
	gamestats->Event_WeaponFired( pPlayer, false, GetClassname() );
#endif

	m_iSecondaryAttacks++;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponDoubleShotgun::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();
}


//==================================================
// Purpose: 
//==================================================
#if 0
void CWeaponDoubleShotgun::WeaponIdle( void )
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

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CWeaponDoubleShotgun::GetMinRestTime()
{
	return 0.2f;
}

//-----------------------------------------------------------------------------
float CWeaponDoubleShotgun::GetMaxRestTime()
{
	return 0.5f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOperator - 
//-----------------------------------------------------------------------------
void CWeaponDoubleShotgun::FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles )
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

	int	TracerFreq = random->RandomInt( 2, 3 );
	pOperator->FireBullets( sk_npc_num_shotgun_pellets.GetInt(), vecShootOrigin, vecShootDir, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, TracerFreq );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponDoubleShotgun::Operator_ForceNPCFire( CBaseCombatCharacter *pOperator, bool bSecondary )
{
	// Ensure we have enough rounds in the clip
	m_iClip1++;

	FireNPCPrimaryAttack( pOperator, true );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponDoubleShotgun::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_SMG1:
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
#endif
