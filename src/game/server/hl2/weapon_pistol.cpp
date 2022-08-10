//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:	Pistol - hand gun - 10mm of fun!
//
//			Primary attack: single accurate shot(s).
//			Secondary attack: innaccurate burst.
// TODO's:  Burst for npcs - gotta set secondary attack conds
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "NPCEvent.h"
#include "basehlcombatweapon.h"
#include "basecombatcharacter.h"
#include "AI_BaseNPC.h"
#include "player.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "soundent.h"
#include "game.h"
#include "vstdlib/random.h"
#include "gamestats.h"
#include "te_effect_dispatch.h"	//TEMPTEMP

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	PISTOL_FASTEST_REFIRE_TIME		sk_pistol_refire_time.GetFloat()	//0.2f

ConVar	sk_pistol_refire_time( "sk_pistol_refire_time",	"0.2");
ConVar	sk_pistol_burst_accuracy_scale( "sk_pistol_burst_accuracy_scale",	"2.5");	//7 DEGREES
extern ConVar sv_funmode;

//-----------------------------------------------------------------------------
// CWeaponPistol
//-----------------------------------------------------------------------------

class CWeaponPistol : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();

public:
	DECLARE_CLASS( CWeaponPistol, CBaseHLCombatWeapon );

	CWeaponPistol(void);

	DECLARE_SERVERCLASS();

	void	Precache( void );
	void	ItemPostFrame( void );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	void	AddViewKick( void );
	void	DryFire( void );
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	Activity	GetPrimaryAttackActivity( void );

	virtual bool Reload( void );

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;
		cone = VECTOR_CONE_4DEGREES;

		if ( GetOwner() && GetOwner()->IsNPC() )
			cone = VECTOR_CONE_7DEGREES;

		if ( GetHSpread() != NULL )
		{
			// We got a weaponscript cone, use that instead
			cone = Vector( GetHSpread(), ((GetHSpread() / 2) + (GetVSpread() / 2)), GetVSpread() );
		}

		return cone;
	}

	// Size of burst
	virtual int		GetMinBurst()
	{
		if ( GetOwner() && GetOwner()->IsNPC() )
			return 1;
		else
			return 4;
	}
	virtual int		GetMaxBurst() { return GetMinBurst(); }

	// time between shots
	virtual float	GetMinRestTime() { return PISTOL_FASTEST_REFIRE_TIME; }
	virtual float	GetMaxRestTime()
	{
		Class_T OwnerClass = GetOwner()->Classify();

		if ( OwnerClass == CLASS_CITIZEN_PASSIVE	||
			 OwnerClass == CLASS_CITIZEN_REBEL	)
			 return (BaseClass::GetFireRate() * 3);

		return (BaseClass::GetFireRate() * 2.5);
	}

	virtual float GetFireRate( void )
	{
		return BaseClass::GetCycleTime();
	}
	
	DECLARE_ACTTABLE();

private:
	void	PistolFire( Vector vSpread, float flCycleTime, bool bBurstFire );
	float	m_flSoonestPrimaryAttack;
	int		m_nNumShotsFired;
};


IMPLEMENT_SERVERCLASS_ST(CWeaponPistol, DT_WeaponPistol)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_pistol, CWeaponPistol );
PRECACHE_WEAPON_REGISTER( weapon_pistol );

BEGIN_DATADESC( CWeaponPistol )

	DEFINE_FIELD( m_flSoonestPrimaryAttack, FIELD_TIME ),
	DEFINE_FIELD( m_flNextSecondaryAttack, FIELD_TIME ),
	DEFINE_FIELD( m_nNumShotsFired,			FIELD_INTEGER ),

END_DATADESC()

acttable_t	CWeaponPistol::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_PISTOL,		true },
	{ ACT_RELOAD,					ACT_RELOAD_PISTOL,				true },
	{ ACT_IDLE,						ACT_IDLE_PISTOL,				true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_PISTOL,			true },

	{ ACT_WALK,						ACT_WALK_PISTOL,				true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_PISTOL,			true },
	{ ACT_RUN,						ACT_RUN_PISTOL,					true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_PISTOL,				true },

	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_PISTOL,	false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_PISTOL_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_PISTOL_LOW,		false },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_PISTOL_LOW,				false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_PISTOL,			false },
};


IMPLEMENT_ACTTABLE( CWeaponPistol );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponPistol::CWeaponPistol( void )
{
//	m_flSoonestPrimaryAttack = gpGlobals->curtime;

	m_fMinRange1		= 24;
	m_fMaxRange1		= 1024;

	//Burst range for ai
//	m_fMinRange2		= 24;
//	m_fMaxRange2		= 200;

	if ( !sv_funmode.GetBool() )
	{
		m_bReloadsFullClip	= true;
		m_bCanJam			= true;
	}
	m_bReloadsSingly	= false;
	m_bFiresUnderwater	= true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPistol::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponPistol::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_PISTOL_FIRE:
		{
			Vector vecShootOrigin, vecShootDir;
			vecShootOrigin = pOperator->Weapon_ShootPosition();

			CAI_BaseNPC *npc = pOperator->MyNPCPointer();
			ASSERT( npc != NULL );

			vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );

			CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_PISTOL, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy() );

			WeaponSound( SINGLE_NPC );
			pOperator->FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2 );
			pOperator->DoMuzzleFlash();

			//Temp effect until model is done with CL_EVENT_EJECTBRASS1, like the smg
			CEffectData data;
			data.m_vOrigin = pOperator->WorldSpaceCenter() + RandomVector( -4, 4 );
			data.m_vAngles = QAngle( 90, random->RandomInt( 0, 360 ), 0 );
			data.m_nEntIndex = entindex();
			DispatchEffect( "ShellEject", data );

			m_iClip1 = m_iClip1 - 1;
		}
		break;
		default:
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponPistol::DryFire( void )
{
	WeaponSound( EMPTY );
	SendWeaponAnim( ACT_VM_DRYFIRE );

	m_flSoonestPrimaryAttack	= gpGlobals->curtime + PISTOL_FASTEST_REFIRE_TIME;
	m_flNextPrimaryAttack		= gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: Accurate single shot
//-----------------------------------------------------------------------------
void CWeaponPistol::PrimaryAttack( void )
{
	PistolFire( GetBulletSpread(), GetFireRate(), false );
}

//-----------------------------------------------------------------------------
// Purpose: Inaccurate but very quick-burst fire
//-----------------------------------------------------------------------------
void CWeaponPistol::SecondaryAttack( void )
{
	PistolFire( (GetBulletSpread() * sk_pistol_burst_accuracy_scale.GetFloat()), 0.06f, true );
}

//-----------------------------------------------------------------------------
// Purpose: Shoot de gun, mon!
//-----------------------------------------------------------------------------
void CWeaponPistol::PistolFire( Vector vSpread, float flCycleTime, bool bBurstFire )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	// If my clip is empty (and I use clips) start reload
	if ( UsesClipsForAmmo1() && !m_iClip1 ) 
	{
		Reload();
		return;
	}

	WeaponSound( SINGLE );
	pPlayer->DoMuzzleFlash();
	m_iClip1--;

	// player "shoot" animation
	SendWeaponAnim( GetPrimaryAttackActivity() );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );
	pPlayer->SetAimTime( 1.0f );

	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), SOUNDENT_VOLUME_PISTOL, 0.2, GetOwner() );

	//TEMP/TODO; This needs improvement
	if ( bBurstFire )
	{
		m_nNumShotsFired++;
		//Lock/delay the primary fire
		m_flSoonestPrimaryAttack = gpGlobals->curtime + (flCycleTime * 4);
		m_flNextPrimaryAttack	= gpGlobals->curtime + (flCycleTime * 4);
		m_flNextSecondaryAttack = gpGlobals->curtime + flCycleTime;
		//If four boolets have been shooted, reset
		if ( m_nNumShotsFired > (GetMinBurst() - 1) )
		{
			m_nNumShotsFired = 0;
			m_flSoonestPrimaryAttack = gpGlobals->curtime;
			m_flNextPrimaryAttack	= gpGlobals->curtime;
			m_flNextSecondaryAttack = gpGlobals->curtime + (flCycleTime * 4);
		}
	}
	else
	{
		m_nNumShotsFired++;
		m_flSoonestPrimaryAttack = gpGlobals->curtime + PISTOL_FASTEST_REFIRE_TIME;
		m_flNextPrimaryAttack	= gpGlobals->curtime + flCycleTime;
		m_flNextSecondaryAttack	= gpGlobals->curtime + flCycleTime;
	}

	Vector	vecSrc	 = pPlayer->Weapon_ShootPosition( );
	Vector	vecAiming	= pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );	

//	pPlayer->FireBullets( 1, vecSrc, vecAiming, Vector( flSpread, flSpread, flSpread ), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2 );
	pPlayer->FireBullets( 1, vecSrc, vecAiming, vSpread, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2 );

	if( pPlayer )
	{
		// Each time the player fires the pistol, reset the view punch. This prevents
		// the aim from 'drifting off' when the player fires very quickly. This may
		// not be the ideal way to achieve this, but it's cheap and it works, which is
		// great for a feature we're evaluating. (sjb)
		pPlayer->ViewPunchReset();
	}

	//Add our view kick in
	AddViewKick();

//	BaseClass::PrimaryAttack();

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
}

//-----------------------------------------------------------------------------
// Purpose: Allows firing as fast as button is pressed
//-----------------------------------------------------------------------------
void CWeaponPistol::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	if ( m_bInReload )
		return;
	
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	//Allow a refire as fast as the player can click
	if ( ( ( pOwner->m_nButtons & IN_ATTACK ) == false ) && ( m_flSoonestPrimaryAttack < gpGlobals->curtime ) )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime - 0.1f;
	}
	else if ( ( pOwner->m_nButtons & IN_ATTACK ) && ( m_flNextPrimaryAttack < gpGlobals->curtime ) && ( m_iClip1 <= 0 ) )
	{
		DryFire();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
Activity CWeaponPistol::GetPrimaryAttackActivity( void )
{
	if ( m_nNumShotsFired < 1 )
		return ACT_VM_PRIMARYATTACK;

	if ( m_nNumShotsFired < 2 )
		return ACT_VM_RECOIL1;

	if ( m_nNumShotsFired < 3 )
		return ACT_VM_RECOIL2;

	return ACT_VM_RECOIL3;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CWeaponPistol::Reload( void )
{
	bool fRet = DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
	if ( fRet )
	{
		WeaponSound( RELOAD );
	}
	return fRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPistol::AddViewKick( void )
{
	CBasePlayer *pPlayer  = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	QAngle	viewPunch;

	viewPunch.x = random->RandomFloat( 0.25f, 0.5f );
	viewPunch.y = random->RandomFloat( -.6f, .6f );
	viewPunch.z = 0.0f;

	//Add it to the view punch
	pPlayer->ViewPunch( viewPunch );
}



//=============================================================================//
//
// Purpose:	GPistol - GOOOOOOLLLD FINNNN-GEERRRRRR
//
//			Primary attack: single accurate shot(s).
//
//=============================================================================//

#define	GPISTOL_FASTEST_REFIRE_TIME		0.125f

//-----------------------------------------------------------------------------
// CWeaponGPistol
//-----------------------------------------------------------------------------

class CWeaponGPistol : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();

public:
	DECLARE_CLASS( CWeaponGPistol, CBaseHLCombatWeapon );

	CWeaponGPistol(void);

	DECLARE_SERVERCLASS();

	void	Precache( void );
	void	ItemPostFrame( void );
	void	PrimaryAttack( void );
	void	AddViewKick( void );
	void	DryFire( void );
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	void	UpdatePenaltyTime( void );

	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	Activity	GetPrimaryAttackActivity( void );

	virtual bool Reload( void );

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone = VECTOR_CONE_2DEGREES;

		return cone;
	}
	
	virtual int		GetMinBurst() { return 1; }
	virtual int		GetMaxBurst() { return 1; }

	virtual float GetFireRate( void ) 
	{
		if ( GetOwner() && GetOwner()->IsNPC() )
		{
			// NPC value
			return 0.55f;
		}
		else
		{
			// Player(s) value
			return 0.4f;
		}
	}
	
	DECLARE_ACTTABLE();

private:
	float	m_flSoonestPrimaryAttack;
	float	m_flLastAttackTime;
	int		m_nNumShotsFired;
};


IMPLEMENT_SERVERCLASS_ST(CWeaponGPistol, DT_WeaponGPistol)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_gpistol, CWeaponGPistol );
PRECACHE_WEAPON_REGISTER( weapon_gpistol );

BEGIN_DATADESC( CWeaponGPistol )

	DEFINE_FIELD( m_flSoonestPrimaryAttack, FIELD_TIME ),
	DEFINE_FIELD( m_flLastAttackTime,		FIELD_TIME ),
	DEFINE_FIELD( m_nNumShotsFired,			FIELD_INTEGER ),

END_DATADESC()

acttable_t	CWeaponGPistol::m_acttable[] = 
{
	{ ACT_IDLE,						ACT_IDLE_PISTOL,				true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_PISTOL,			true },
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_PISTOL,		true },
	{ ACT_RELOAD,					ACT_RELOAD_PISTOL,				true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_PISTOL,			true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_PISTOL,				true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_PISTOL,true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_PISTOL_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_PISTOL_LOW,	false },
	{ ACT_COVER_LOW,				ACT_COVER_PISTOL_LOW,			false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_PISTOL_LOW,		false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_WALK,						ACT_WALK_PISTOL,				false },
	{ ACT_RUN,						ACT_RUN_PISTOL,					false },
};


IMPLEMENT_ACTTABLE( CWeaponGPistol );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponGPistol::CWeaponGPistol( void )
{
	m_fMinRange1		= 24;
	m_fMaxRange1		= 1500;

	m_bReloadsSingly	= false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGPistol::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponGPistol::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_PISTOL_FIRE:
		{
			Vector vecShootOrigin, vecShootDir;
			vecShootOrigin = pOperator->Weapon_ShootPosition();

			CAI_BaseNPC *npc = pOperator->MyNPCPointer();
			ASSERT( npc != NULL );

			vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );

			CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_PISTOL, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy() );

			WeaponSound( SINGLE_NPC );
			pOperator->FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2 );
			pOperator->DoMuzzleFlash();
			m_iClip1 = m_iClip1 - 1;
		}
		break;
		default:
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponGPistol::DryFire( void )
{
	WeaponSound( EMPTY );
	SendWeaponAnim( ACT_VM_DRYFIRE );
	
	m_flSoonestPrimaryAttack	= gpGlobals->curtime + GPISTOL_FASTEST_REFIRE_TIME;
	m_flNextPrimaryAttack		= gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: Single shot
//-----------------------------------------------------------------------------
void CWeaponGPistol::PrimaryAttack( void )
{
	if ( ( gpGlobals->curtime - m_flLastAttackTime ) > 0.5f )
	{
		m_nNumShotsFired = 0;
	}
	else
	{
		m_nNumShotsFired++;
	}

	m_flLastAttackTime = gpGlobals->curtime;
	m_flSoonestPrimaryAttack = gpGlobals->curtime + GPISTOL_FASTEST_REFIRE_TIME;
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	BaseClass::PrimaryAttack();
	
	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pOwner, true, GetClassname() );
}

//-----------------------------------------------------------------------------
// Purpose: Allows firing as fast as button is pressed
//-----------------------------------------------------------------------------
void CWeaponGPistol::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	if ( m_bInReload )
		return;
	
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	//Allow a refire as fast as the player can click
	if ( ( ( pOwner->m_nButtons & IN_ATTACK ) == false ) && ( m_flSoonestPrimaryAttack < gpGlobals->curtime ) )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime - 0.1f;
	}
	else if ( ( pOwner->m_nButtons & IN_ATTACK ) && ( m_flNextPrimaryAttack < gpGlobals->curtime ) && ( m_iClip1 <= 0 ) )
	{
		DryFire();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
Activity CWeaponGPistol::GetPrimaryAttackActivity( void )
{
	if ( m_nNumShotsFired < 1 )
		return ACT_VM_PRIMARYATTACK;

	if ( m_nNumShotsFired < 2 )
		return ACT_VM_RECOIL1;

	if ( m_nNumShotsFired < 3 )
		return ACT_VM_RECOIL2;

	return ACT_VM_RECOIL3;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CWeaponGPistol::Reload( void )
{
	bool fRet = DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
	if ( fRet )
	{
		WeaponSound( RELOAD );
	}
	return fRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGPistol::AddViewKick( void )
{
	CBasePlayer *pPlayer  = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	QAngle	viewPunch;

	viewPunch.x = random->RandomFloat( -2.0f, -1.0f );
	viewPunch.y = random->RandomFloat( 0.5f, 1.0f );
	viewPunch.z = 0.0f;

	//Add it to the view punch
	pPlayer->ViewPunch( viewPunch );
}
