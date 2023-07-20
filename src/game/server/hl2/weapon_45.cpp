//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		45-70
//
// TODO; 
//=============================================================================//

#include "cbase.h"
#include "NPCEvent.h"
#include "basehlcombatweapon.h"
#include "weapon_sniperrifle.h"
#include "basecombatcharacter.h"
#include "AI_BaseNPC.h"
#include "player.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "soundent.h"
#include "game.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "te_effect_dispatch.h"
#include "gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	CONTENDER_REFIRE_TIME		sk_45_refire_time.GetFloat()
#define	CONTENDER_ACCURACY_SHOT_PENALTY_TIME		0.8f	// Applied amount of time each shot adds to the time we must recover from. Should be a bit over firerate
#define	CONTENDER_ACCURACY_MAXIMUM_PENALTY_TIME	2.0f	// Maximum penalty to deal out

ConVar	sk_45_refire_time( "sk_45_refire_time",	"0.4");
extern ConVar sv_funmode;

//-----------------------------------------------------------------------------
// CWeapon45
//-----------------------------------------------------------------------------

class CWeapon45 : public CHLSniperRifle
{
	DECLARE_CLASS( CWeapon45, CHLSniperRifle );
public:
	CWeapon45( );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	bool	Deploy( void );

	void	ItemPostFrame( void );
	void	ItemPreFrame( void );
	void	ItemBusyFrame( void );
	void	PrimaryAttack( void );
	void	DryFire( void );
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	const char *GetTracerType( void ) { return "BigTracer"; }
	void	UpdatePenaltyTime( void );
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	float	WeaponAutoAimScale()	{ return 0.6f; }

	virtual bool Reload( void );
	
	virtual const Vector& GetBulletSpread( void )
	{
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_3DEGREES;
		if ( GetOwner() && GetOwner()->IsNPC() )
			return npcCone;

		static Vector cone;

		// In MP first shot is always %100 accurate
		if ( g_pGameRules->IsMultiplayer() )
			cone = VECTOR_CONE_PRECALCULATED;
		else
			cone = VECTOR_CONE_2DEGREES;

		float ramp = RemapValClamped(	m_flAccuracyPenalty, 
										0.0f, 
										CONTENDER_ACCURACY_MAXIMUM_PENALTY_TIME, 
										0.0f, 
										1.0f ); 

		// If the player wants to break his wrist, give him the accuracy of one
		VectorLerp( VECTOR_CONE_1DEGREES, VECTOR_CONE_9DEGREES, ramp, cone );

		return cone;
	}

	virtual float GetFireRate( void ) 
	{
		// This is default fire-rate (primary attack) holding it down, should be about 1 shot a second; TF2 is 0.15
		if ( GetOwner() && GetOwner()->IsNPC() )
			return BaseClass::GetFireRate() + 0.25f;	// NPC value
		else
			return BaseClass::GetFireRate();	// Player(s) value
	}

	DECLARE_ACTTABLE();

private:
	float	m_flSoonestPrimaryAttack;
	float	m_flLastAttackTime;
	float	m_flAccuracyPenalty;
	int		m_nNumShotsFired;
};

IMPLEMENT_SERVERCLASS_ST( CWeapon45, DT_Weapon45 )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_45, CWeapon45 );
PRECACHE_WEAPON_REGISTER( weapon_45 );

BEGIN_DATADESC( CWeapon45 )

	DEFINE_FIELD( m_flSoonestPrimaryAttack, FIELD_TIME ),
	DEFINE_FIELD( m_flLastAttackTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flAccuracyPenalty,		FIELD_FLOAT ), //NOTENOTE: This is NOT tracking game time
	DEFINE_FIELD( m_nNumShotsFired,			FIELD_INTEGER ),

END_DATADESC()

acttable_t	CWeapon45::m_acttable[] = 
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
IMPLEMENT_ACTTABLE( CWeapon45 );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon45::CWeapon45( )
{
//	m_flSoonestPrimaryAttack = gpGlobals->curtime;
	m_flAccuracyPenalty = 0.0f;

	m_fMinRange1		= 16;
	m_fMaxRange1		= 1500;

	if ( sv_funmode.GetBool() )
	{
		m_bCanJam			= true;
	}
	m_bReloadsSingly	= false;
	m_bReloadsFullClip	= true;
	m_bFiresUnderwater	= false;
	m_bUseProjectile = false;
}

bool CWeapon45::Deploy( void )
{
	// Multiplayer, we dont have that tiny base spread, rep it
	if ( g_pGameRules->IsMultiplayer() )
	{
		// enable laser sight geometry.
		SetBodygroup( 4, 1 );
	}
	else
	{
		SetBodygroup( 4, 0 );
	}

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon45::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_RELOAD:
			{
				CEffectData data;

				// Emit six spent shells
				for ( int i = 0; i < GetDefaultClip1(); i++ )
				{
					data.m_vOrigin = pOperator->WorldSpaceCenter() + RandomVector( -4, 4 );
					data.m_vAngles = QAngle( 90, random->RandomInt( 0, 360 ), 0 );
					data.m_nEntIndex = entindex();

					DispatchEffect( "ShellEject", data );
				}

				break;
			}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon45::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
	{
		return;
	}

	if ( ( gpGlobals->curtime - m_flLastAttackTime ) > 0.5f )
		m_nNumShotsFired = 0;
	else
		m_nNumShotsFired++;

	m_flLastAttackTime = gpGlobals->curtime;
	m_flSoonestPrimaryAttack = gpGlobals->curtime + CONTENDER_REFIRE_TIME;

	BaseClass::PrimaryAttack();

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();

	// Add an accuracy penalty which can move past our maximum penalty time if we're really spastic
	m_flAccuracyPenalty += CONTENDER_ACCURACY_SHOT_PENALTY_TIME;

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon45::DryFire( void )
{
	WeaponSound( EMPTY );
	SendWeaponAnim( ACT_VM_DRYFIRE );
	
	m_flSoonestPrimaryAttack	= gpGlobals->curtime + 0.5;
	m_flNextPrimaryAttack		= gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeapon45::UpdatePenaltyTime( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	// Check our penalty time decay
	if ( ( ( pOwner->m_nButtons & IN_ATTACK ) == false ) && ( m_flSoonestPrimaryAttack < gpGlobals->curtime ) )
	{
		m_flAccuracyPenalty -= gpGlobals->frametime;
		m_flAccuracyPenalty = clamp( m_flAccuracyPenalty, 0.0f, CONTENDER_ACCURACY_MAXIMUM_PENALTY_TIME );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeapon45::ItemPreFrame( void )
{
	UpdatePenaltyTime();

	BaseClass::ItemPreFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeapon45::ItemBusyFrame( void )
{
	UpdatePenaltyTime();

	BaseClass::ItemBusyFrame();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon45::ItemPostFrame( void )
{
	UpdatePenaltyTime();

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
//-----------------------------------------------------------------------------
bool CWeapon45::Reload( void )
{
	m_flAccuracyPenalty = 0.0f;

	return BaseClass::Reload();
}
