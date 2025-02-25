//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Pump-action shotgun/Starting shotgun.
//
//			Primary attack: single barrel shot.
//			Secondary attack: double shell shot, requires a single pump before use.
// TODO's: New impact effects (normal holes are too big), push player back for secondary attack
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
#include "AI_BaseNPC.h"
#include "basecombatcharacter.h"
#include "soundent.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define VECTOR_CONE_SHOTGUN		Vector( 0.10461, 0.10461, 0.09589 )
#define SHOTGUN_KICKBACK		4	// Range for punchangle when firing.

extern ConVar sk_auto_reload_time;
extern ConVar sk_plr_num_shotgun_pellets;
extern ConVar sk_npc_num_shotgun_pellets;
extern ConVar sv_funmode;

#ifdef CLIENT_DLL
#define CWeaponShotgun C_WeaponShotgun
#endif
class CWeaponShotgun : public CBaseHLCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponShotgun, CBaseHLCombatWeapon );

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

private:
	CNetworkVar( bool,	m_bNeedPump );		// When emptied completely
	CNetworkVar( bool,	m_bDelayedFire1 );	// Fire primary when finished reloading
	CNetworkVar( bool,	m_bDelayedFire2 );	// Fire secondary when finished reloading
//	CNetworkVar( bool,	m_bDelayedReload );	// Reload when finished pump

public:
	void	Precache( void );

	virtual const Vector& GetBulletSpread( void )
	{
#ifdef GAME_DLL
		static Vector AllyCone = VECTOR_CONE_5DEGREES;
		if( GetOwner() && (GetOwner()->Classify() == CLASS_PLAYER_ALLY_VITAL) )
		{
			// Story allies fire shots more akin to slugs. Leave room-clearing for the player.
			return AllyCone;
		}
#endif
		if ( GetHSpread() != NULL )
			return BaseClass::GetBulletSpread();

		static Vector cone = VECTOR_CONE_SHOTGUN;
		return cone;
	}

	virtual float			GetFireRate( void );

	bool StartReload( void );
	bool Reload( void );
	void FillClip( void );
	void FinishReload( void );
	void CheckHolsterReload( void );
	void Pump( void );
	void HalfPump( void );	//For secondary fire
//	void WeaponIdle( void );
	void ItemDormantFrame( void );
	void ItemPostFrame( void );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	bool CheckFireConditions( void );
//	void DoImpactEffect( trace_t &tr, int nDamageType );

	const char *GetTracerType( void ) { return "ShotgunTracer"; }
#ifndef CLIENT_DLL
	int CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	virtual float			GetMinRestTime();
	virtual float			GetMaxRestTime();

	void FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	void FireNPCSecondaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	void Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary );
	void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
#endif
	DECLARE_ACTTABLE();

	CWeaponShotgun(void);
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponShotgun, DT_WeaponShotgun )

BEGIN_NETWORK_TABLE( CWeaponShotgun, DT_WeaponShotgun )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bNeedPump ) ),
	RecvPropBool( RECVINFO( m_bDelayedFire1 ) ),
	RecvPropBool( RECVINFO( m_bDelayedFire2 ) ),
//	RecvPropBool( RECVINFO( m_bDelayedReload ) ),
#else
	SendPropBool( SENDINFO( m_bNeedPump ) ),
	SendPropBool( SENDINFO( m_bDelayedFire1 ) ),
	SendPropBool( SENDINFO( m_bDelayedFire2 ) ),
//	SendPropBool( SENDINFO( m_bDelayedReload ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponShotgun )
	DEFINE_PRED_FIELD( m_bNeedPump, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDelayedFire1, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDelayedFire2, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
//	DEFINE_PRED_FIELD( m_bDelayedReload, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_shotgun, CWeaponShotgun );
PRECACHE_WEAPON_REGISTER(weapon_shotgun);

acttable_t	CWeaponShotgun::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_SHOTGUN,			true },
	{ ACT_RELOAD,					ACT_RELOAD_SHOTGUN,					false },
	{ ACT_IDLE,						ACT_IDLE_RIFLE,						true },	// FIXME: hook to shotgun unique
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
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_SHOTGUN,	true },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SHOTGUN_LOW,		true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SHOTGUN_LOW,				false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SHOTGUN,			false },
};

IMPLEMENT_ACTTABLE(CWeaponShotgun);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponShotgun::CWeaponShotgun( void )
{
	m_bReloadsSingly = true;

	m_bNeedPump		= false;
	m_bDelayedFire1 = false;
	m_bDelayedFire2 = false;
//	m_bDelayedReload = false;

	if ( sv_funmode.GetBool() )
	{
		m_bCanJam			= true;
	}
	m_fMinRange1		= 0.0;
	m_fMaxRange1		= 768;
	m_fMinRange2		= 0.0;
	m_fMaxRange2		= 256;	//This is secondary attack range for ai, if its ever implemented (gods forbid)
}

void CWeaponShotgun::Precache( void )
{
	CBaseCombatWeapon::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:	When we shipped HL2, the shotgun weapon did not override the
//			BaseCombatWeapon default rest time of 0.3 to 0.6 seconds. When
//			NPC's fight from a stationary position, their animation events
//			govern when they fire so the rate of fire is specified by the
//			animation. When NPC's move-and-shoot, the rate of fire is 
//			specifically controlled by the shot regulator, so it's imporant
//			that GetMinRestTime and GetMaxRestTime are implemented and provide
//			reasonable defaults for the weapon. To address difficulty concerns,
//			we are going to fix the combine's rate of shotgun fire in episodic.
//			This change will not affect Alyx using a shotgun in EP1. (sjb)
//-----------------------------------------------------------------------------
#ifndef CLIENT_DLL
float CWeaponShotgun::GetMinRestTime()
{
	Class_T OwnerClass = GetOwner()->Classify();

	if ( OwnerClass == CLASS_CITIZEN_PASSIVE	||
		 OwnerClass == CLASS_CITIZEN_REBEL	||
		 OwnerClass == CLASS_METROPOLICE	)
		 return 1.1f;

	return GetFireRate();
}

//-----------------------------------------------------------------------------
float CWeaponShotgun::GetMaxRestTime()
{
	Class_T OwnerClass = GetOwner()->Classify();

	if ( OwnerClass == CLASS_CITIZEN_PASSIVE	||
		 OwnerClass == CLASS_CITIZEN_REBEL	||
		 OwnerClass == CLASS_METROPOLICE	)
		 return 1.6f;

	return GetFireRate() + 0.4f;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Time between successive shots in a burst.
//-----------------------------------------------------------------------------
float CWeaponShotgun::GetFireRate()
{
	return BaseClass::GetFireRate();
}

//-----------------------------------------------------------------------------
// Purpose: Override so only reload one shell at a time
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CWeaponShotgun::StartReload( void )
{
	CBaseCombatCharacter *pOwner  = GetOwner();
	
	if ( pOwner == NULL )
		return false;

	if (pOwner->GetAmmoCount(GetPrimaryAmmoType()) <= 0)
		return false;

	if (m_iClip1 >= GetMaxClip1())
		return false;

	// If shotgun totally emptied then a pump animation is needed
	
	//NOTENOTE: This is kinda coolio because the player doesn't get strong feedback on when the reload has finished,
	//			without the pump.  Technically, it's incorrect, but it's good for feedback...

	if (m_iClip1 <= 0)
	{
		m_bNeedPump = true;
	}

	int j = min(1, pOwner->GetAmmoCount(GetPrimaryAmmoType()));

	if (j <= 0)
		return false;

	SendWeaponAnim( ACT_SHOTGUN_RELOAD_START );

	// Make shotgun shell visible
	SetBodygroup(1,0);

	pOwner->m_flNextAttack = gpGlobals->curtime;
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();

	m_bInReload = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Override so only reload one shell at a time
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CWeaponShotgun::Reload( void )
{
	// Check that StartReload was called first
	if (!m_bInReload)
	{
		Warning("ERROR: Shotgun Reload called incorrectly!\n");
		m_bInReload = true;
	}

	CBaseCombatCharacter *pOwner  = GetOwner();
	
	if ( pOwner == NULL )
		return false;

	if (pOwner->GetAmmoCount(GetPrimaryAmmoType()) <= 0)
		return false;

	if (m_iClip1 >= GetMaxClip1())
		return false;

	int j = min(1, pOwner->GetAmmoCount(GetPrimaryAmmoType()));

	if (j <= 0)
		return false;

	FillClip();
	// Play reload on different channel as otherwise steals channel away from fire sound
	WeaponSound(RELOAD);
	SendWeaponAnim( ACT_VM_RELOAD );
	// Play the player's reload animation
	if ( pOwner->IsPlayer() )
	{
		( ( CBasePlayer * )pOwner)->SetAnimation( PLAYER_RELOAD );
	}

	pOwner->m_flNextAttack = gpGlobals->curtime;
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Play finish reload anim and fill clip
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponShotgun::FinishReload( void )
{
	// Make shotgun shell invisible
	SetBodygroup(1,1);

	CBaseCombatCharacter *pOwner  = GetOwner();
	
	if ( pOwner == NULL )
		return;

	m_bInReload = false;

	// Finish reload animation
	SendWeaponAnim( ACT_SHOTGUN_RELOAD_FINISH );

	pOwner->m_flNextAttack = gpGlobals->curtime;
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: Play finish reload anim and fill clip
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponShotgun::FillClip( void )
{
	CBaseCombatCharacter *pOwner  = GetOwner();
	
	if ( pOwner == NULL )
		return;

	// Add them to the clip
	if ( pOwner->GetAmmoCount( GetPrimaryAmmoType() ) > 0 )
	{
		if ( Clip1() < GetMaxClip1() )
		{
			m_iClip1++;
			pOwner->RemoveAmmo( 1, GetPrimaryAmmoType() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play weapon pump anim
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponShotgun::Pump( void )
{
	CBaseCombatCharacter *pOwner  = GetOwner();

	if ( pOwner == NULL )
		return;

	m_bNeedPump = false;

	WeaponSound( SPECIAL1 );
#ifndef CLIENT_DLL
	CSoundEnt::InsertSound( SOUND_WEAPON, GetAbsOrigin(), 384, 0.2, GetOwner() );
#endif
	// Finish reload animation
	SendWeaponAnim( ACT_SHOTGUN_PUMP );

	pOwner->m_flNextAttack	= gpGlobals->curtime + GetFireRate();	//SequenceDuration
	m_flNextPrimaryAttack	= gpGlobals->curtime + GetFireRate();	//SequenceDuration
}

void CWeaponShotgun::HalfPump( void )
{
	CBaseCombatCharacter *pOwner  = GetOwner();

	if ( pOwner == NULL )
		return;

	m_bNeedPump = false;

	WeaponSound( SPECIAL1 );

	// Half pump animation - load a second shell without ejecting the first
	SendWeaponAnim( ACT_VM_PULLBACK );

	SecondaryAttack();
}

//-----------------------------------------------------------------------------
// Purpose: 
// NOTENOTE; This should probably be compressed and baseclass'd
//-----------------------------------------------------------------------------
void CWeaponShotgun::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
	{
		return;
	}

	// MUST call sound before removing a round from the clip of a CMachineGun
	WeaponSound(SINGLE);

	// player "shoot" animation
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );
	pPlayer->DoMuzzleFlash();

	// Don't fire again until fire animation has completed
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	m_iClip1 -= 1;

	Vector	vecSrc		= pPlayer->Weapon_ShootPosition( );
	Vector	vecAiming	= pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );

	// Fire the bullets, and force the first shot to be perfectly accuracy
	//pPlayer->FireBullets( sk_plr_num_shotgun_pellets.GetInt(), vecSrc, vecAiming, GetBulletSpread(), MAX_TRACE_LENGTH, GetPrimaryAmmoType(), 2, -1, -1, 0, NULL, true, true );
	FireBulletsInfo_t info;
	info.m_iShots = sk_plr_num_shotgun_pellets.GetInt();
	info.m_vecSrc = vecSrc;
	info.m_vecDirShooting = vecAiming;
	info.m_vecSpread = GetBulletSpread();
	info.m_flDistance = MAX_TRACE_LENGTH;
	info.m_iAmmoType = GetActiveAmmoType();
	info.m_iTracerFreq = 2;
	pPlayer->FireBullets( info );

	pPlayer->ViewPunch( QAngle( -SHOTGUN_KICKBACK, random->RandomFloat( -SHOTGUN_KICKBACK, SHOTGUN_KICKBACK ), 0 ) );

#ifndef CLIENT_DLL
	pPlayer->SetAimTime( 3.0f );
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 1.0 );
	Vector recoilForce = pPlayer->BodyDirection2D() * -( sk_plr_num_shotgun_pellets.GetInt() * 3.0f );
	pPlayer->ApplyAbsVelocityImpulse( recoilForce );
#endif

	if (!m_iClip1 && pPlayer->GetAmmoCount(GetPrimaryAmmoType()) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
	}
	if( m_iClip1 )
	{
		// pump so long as some rounds are left.
		m_bNeedPump = true;
	}
#ifndef CLIENT_DLL
	CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, GetAbsOrigin(), SOUNDENT_VOLUME_SHOTGUN, 0.2, GetOwner() );
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
#endif
	m_iPrimaryAttacks++;
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CWeaponShotgun::SecondaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
		return;

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

	// Fire the bullets
	int NumPellets = sk_plr_num_shotgun_pellets.GetInt() * 2;
//	pPlayer->FireBullets( NumPellets, vecSrc, vecAiming, (VECTOR_CONE_SHOTGUN * 1.25), MAX_TRACE_LENGTH, GetPrimaryAmmoType(), 2, -1, -1, 0, NULL, false, false );
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

	Vector	recoilForce = pPlayer->BodyDirection3D() * -( NumPellets * 5.0f );
	recoilForce[2] += 128.0f;

	//TODO; Clamp force?
	// Naaahhhh this is fun
	pPlayer->ApplyAbsVelocityImpulse( recoilForce );

	if ( pPlayer->GetHealth() > 2 )
		pPlayer->TakeDamage( CTakeDamageInfo( this, this, 1, DMG_CLUB ) );
#endif

	if (!m_iClip1 && pPlayer->GetAmmoCount(GetPrimaryAmmoType()) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
	}
	if( m_iClip1 )
	{
		// pump so long as some rounds are left.
		m_bNeedPump = true;
	}

#ifndef CLIENT_DLL
	CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, GetAbsOrigin(), SOUNDENT_VOLUME_SHOTGUN, 0.2 );
	gamestats->Event_WeaponFired( pPlayer, false, GetClassname() );
#endif

	m_iSecondaryAttacks++;
}
	
//-----------------------------------------------------------------------------
// Purpose: Override so shotgun can do mulitple reloads in a row
// TODO; Theres way more if/elses than i'd like here - MM
//-----------------------------------------------------------------------------
void CWeaponShotgun::ItemPostFrame( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if (!pOwner)
	{
		return;
	}

	if (m_bInReload)
	{
		// TODO; switch?
#if 0
		switch( pOwner->m_nButtons )
		{
			case IN_ATTACK:
				// If I'm primary firing and have one round stop reloading and fire
				if (m_iClip1 >=1)
				{
					FinishReload();
				//!	m_bNeedPump		= false;
					m_bDelayedFire1 = true;
					return;
				}
			break;

			case IN_ATTACK2:
				// If I'm secondary firing and have two rounds stop reloading and fire
				if (m_iClip1 >=2)
				{
					FinishReload();
				//!	m_bNeedPump		= false;
					m_bDelayedFire2 = true;
					return;
				}
			break;

			case IN_RELOAD:
				// Reload cancelled by owner
				m_bDelayedStopReload = true;
				return;
			break;

			default:
			if (m_flNextPrimaryAttack <= gpGlobals->curtime)
			{
				// Completely out of ammo
				if ( pOwner->GetAmmoCount(GetPrimaryAmmoType()) <=0)
				{
					FinishReload();
					return;
				}
				// If clip not full reload again
				if (m_iClip1 < GetMaxClip1())
				{
					Reload();
					return;
				}
				// Clip full, stop reloading
				else
				{
					FinishReload();
					return;
				}
			}
			break;
		}
#else
		// If I'm primary firing and have one round stop reloading and fire
		if ((pOwner->m_nButtons & IN_ATTACK) && (m_iClip1 >=1))
		{
			FinishReload();
		//!	m_bNeedPump		= false;
		//	m_bDelayedFire1 = true;
			return;
		}
		// If I'm secondary firing and have two rounds stop reloading and fire
		else if ((pOwner->m_nButtons & IN_ATTACK2) && (m_iClip1 >=2))
		{
			FinishReload();
		//!	m_bNeedPump		= false;
		//	m_bDelayedFire2 = true;
			return;
		}
		else if (m_flNextPrimaryAttack <= gpGlobals->curtime)
		{
			// Completely out of ammo
			if ( pOwner->GetAmmoCount(GetPrimaryAmmoType()) <=0)
			{
				FinishReload();
				return;
			}

			// If clip not full reload again
			if (m_iClip1 < GetMaxClip1())
			{
				Reload();
				return;
			}
			// Clip full, stop reloading
			else
			{
				FinishReload();
				return;
			}
		}
#endif
	}
	else
	{			
		// Make shotgun shell invisible
		SetBodygroup(1,1);
	}

	if ((m_bNeedPump) && (m_flNextPrimaryAttack <= gpGlobals->curtime))	
	{
		Pump();
		return;
	}
	
	// Shotgun uses same timing and ammo for secondary attack
	if (m_flNextPrimaryAttack <= gpGlobals->curtime)
	{
		if ( m_bDelayedFire2 || pOwner->m_nButtons & IN_ATTACK2 )
		{
			m_bDelayedFire2 = false;
			// Check if im allowed to fire normally
			if ( CheckFireConditions() )
			{
				// If the firing button was just pressed, reset the firing time
				if ( pOwner->m_afButtonPressed & IN_ATTACK )
					 m_flNextPrimaryAttack = gpGlobals->curtime;

				SecondaryAttack();
			}
		}
		else if ( m_bDelayedFire1 || pOwner->m_nButtons & IN_ATTACK )
		{
			m_bDelayedFire1 = false;
			// Check if im allowed to fire normally
			if ( CheckFireConditions() )
			{
				// If the firing button was just pressed, reset the firing time
				if ( pOwner->m_afButtonPressed & IN_ATTACK )
					 m_flNextPrimaryAttack = gpGlobals->curtime;

				PrimaryAttack();
			}
		}
	}

	if ( (pOwner->m_nButtons & IN_RELOAD) && UsesClipsForAmmo1() ) 
	{
		if ( !m_bInReload )
			StartReload();
	}
	else
	{
		// no fire buttons down
		m_bFireOnEmpty = false;

		if ( !HasAnyAmmo() && m_flNextPrimaryAttack < gpGlobals->curtime ) 
		{
			// weapon isn't useable, switch.
			if ( !(GetWeaponFlags() & ITEM_FLAG_NOAUTOSWITCHEMPTY) && pOwner->SwitchToNextBestWeapon( this ) )
			{
				m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
				return;
			}
		}
		else
		{
			// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
			if ( m_iClip1 <= 0 && !(GetWeaponFlags() & ITEM_FLAG_NOAUTORELOAD) && m_flNextPrimaryAttack < gpGlobals->curtime )
			{
				if (StartReload())
				{
					// if we've successfully started to reload, we're done
					return;
				}
			}
		}

		WeaponIdle( );
		return;
	}

}

bool CWeaponShotgun::CheckFireConditions( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if (!pOwner)
		return false;

	// Fire underwater?
	if (pOwner->GetWaterLevel() == 3 && m_bFiresUnderwater == false)
	{
		DryFire();
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
		return false;
	}

	if ( (m_iClip1 <= 1 && UsesClipsForAmmo1()))
	{
		// If only one shell is left, always do a single shot	
		if ( m_iClip1 == 1 )
			PrimaryAttack();
		else if (!pOwner->GetAmmoCount(m_iPrimaryAmmoType))
			DryFire();
		else
			StartReload();

		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Smaller Impact FX
//-----------------------------------------------------------------------------
/*
void CWeaponShotgun::DoImpactEffect( trace_t &tr, int nDamageType )
{
	CEffectData data;

	data.m_vOrigin = tr.endpos + ( tr.plane.normal * 1.0f );
	data.m_vNormal = tr.plane.normal;

	DispatchEffect( "ShotgunImpact", data );

	BaseClass::DoImpactEffect( tr, nDamageType );
}
*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponShotgun::ItemDormantFrame( void )
{
	// Must be player held
	if ( GetOwner() && GetOwner()->IsPlayer() == false )
		return;

	// We can't be active
	if ( GetOwner()->GetActiveWeapon() == this )
		return;

	// If it's been longer than three seconds, reload
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
void CWeaponShotgun::WeaponIdle( void )
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
	
	BaseClass::WeaponIdle();
}
#endif

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOperator - 
//-----------------------------------------------------------------------------
void CWeaponShotgun::FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles )
{
	Vector vecShootOrigin, vecShootDir;
	CAI_BaseNPC *npc = pOperator->MyNPCPointer();
	ASSERT( npc != NULL );
	WeaponSound( SINGLE_NPC );
	CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_SHOTGUN, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy() );
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
	pOperator->FireBullets( sk_npc_num_shotgun_pellets.GetInt(), vecShootOrigin, vecShootDir, GetBulletSpread(), MAX_TRACE_LENGTH, GetPrimaryAmmoType(), TracerFreq );
}

//-----------------------------------------------------------------------------
void CWeaponShotgun::FireNPCSecondaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles )
{
	Vector vecShootOrigin, vecShootDir;
	CAI_BaseNPC *npc = pOperator->MyNPCPointer();
	ASSERT( npc != NULL );
	WeaponSound( DOUBLE_NPC );
	CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_SHOTGUN, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy() );
	pOperator->DoMuzzleFlash();
	m_iClip1 = m_iClip1 - 2;

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

	int NumPellets = sk_npc_num_shotgun_pellets.GetInt() * 2;
	pOperator->FireBullets( NumPellets, vecShootOrigin, vecShootDir, (VECTOR_CONE_SHOTGUN * 1.25), MAX_TRACE_LENGTH, GetPrimaryAmmoType(), 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponShotgun::Operator_ForceNPCFire( CBaseCombatCharacter *pOperator, bool bSecondary )
{
	// Ensure we have enough rounds in the clip
	m_iClip1++;

	FireNPCPrimaryAttack( pOperator, true );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponShotgun::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
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
