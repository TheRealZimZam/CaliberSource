//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 44 Deagle
//
// TODO; 
//=============================================================================//

#include "cbase.h"
#include "NPCEvent.h"
#include "basehlcombatweapon_shared.h"
#include "weapon_sniperrifle.h"
#include "baseplayer_shared.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "gamestats.h"
#ifndef CLIENT_DLL
	#include "basecombatcharacter.h"
	#include "AI_BaseNPC.h"
	#include "soundent.h"
	#include "game.h"
	#include "te_effect_dispatch.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	DEAGLE_REFIRE_TIME						sk_44_refire_time.GetFloat()
#define	DEAGLE_ACCURACY_SHOT_PENALTY_TIME		0.8f	// Applied amount of time each shot adds to the time we must recover from. Should be a bit over firerate
#define	DEAGLE_ACCURACY_MAXIMUM_PENALTY_TIME	2.0f	// Maximum penalty to deal out

ConVar	sk_44_refire_time( "sk_44_refire_time",	"0.4");
extern ConVar sv_funmode;

//-----------------------------------------------------------------------------
// CWeapon44
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
#define CWeapon44 C_Weapon44
#endif
class CWeapon44 : public CHLSniperRifle
{
	DECLARE_CLASS( CWeapon44, CHLSniperRifle );
public:
	CWeapon44( );

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	bool	Deploy( void );

	void	ItemPostFrame( void );
	void	ItemPreFrame( void );
	void	ItemBusyFrame( void );
	void	PrimaryAttack( void );
	void	DryFire( void );

	const char *GetTracerType( void ) { return "BigTracer"; }
	void	UpdatePenaltyTime( void );
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
										DEAGLE_ACCURACY_MAXIMUM_PENALTY_TIME, 
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

#ifndef CLIENT_DLL
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
#endif

	DECLARE_ACTTABLE();

private:
	CNetworkVar( float, m_flSoonestPrimaryAttack );
	CNetworkVar( float, m_flLastAttackTime );
	CNetworkVar( float, m_flAccuracyPenalty );
	CNetworkVar( int, m_nNumShotsFired );
};

IMPLEMENT_NETWORKCLASS_ALIASED( Weapon44, DT_Weapon44 )

BEGIN_NETWORK_TABLE( CWeapon44, DT_Weapon44 )
#ifdef CLIENT_DLL
	RecvPropFloat( RECVINFO( m_flSoonestPrimaryAttack ) ),
	RecvPropFloat( RECVINFO( m_flLastAttackTime ) ),
	RecvPropFloat( RECVINFO( m_flAccuracyPenalty ) ),
	RecvPropInt( RECVINFO( m_nNumShotsFired ) ),
#else
	SendPropFloat( SENDINFO( m_flSoonestPrimaryAttack ) ),
	SendPropFloat( SENDINFO( m_flLastAttackTime ) ),
	SendPropFloat( SENDINFO( m_flAccuracyPenalty ) ),
	SendPropInt( SENDINFO( m_nNumShotsFired ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon44 )
	DEFINE_PRED_FIELD( m_flSoonestPrimaryAttack, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flLastAttackTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flAccuracyPenalty, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nNumShotsFired, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_44, CWeapon44 );
PRECACHE_WEAPON_REGISTER( weapon_44 );

acttable_t	CWeapon44::m_acttable[] = 
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
IMPLEMENT_ACTTABLE( CWeapon44 );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon44::CWeapon44( )
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

bool CWeapon44::Deploy( void )
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
void CWeapon44::PrimaryAttack( void )
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
	m_flSoonestPrimaryAttack = gpGlobals->curtime + DEAGLE_REFIRE_TIME;

	BaseClass::PrimaryAttack();

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();

	// Add an accuracy penalty which can move past our maximum penalty time if we're really spastic
	m_flAccuracyPenalty += DEAGLE_ACCURACY_SHOT_PENALTY_TIME;

	m_iPrimaryAttacks++;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon44::DryFire( void )
{
	WeaponSound( EMPTY );
	SendWeaponAnim( ACT_VM_DRYFIRE );
	
	m_flSoonestPrimaryAttack	= gpGlobals->curtime + 0.5;
	m_flNextPrimaryAttack		= gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeapon44::UpdatePenaltyTime( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	// Check our penalty time decay
	if ( ( ( pOwner->m_nButtons & IN_ATTACK ) == false ) && ( m_flSoonestPrimaryAttack < gpGlobals->curtime ) )
	{
		m_flAccuracyPenalty -= gpGlobals->frametime;
		m_flAccuracyPenalty = clamp( m_flAccuracyPenalty, 0.0f, DEAGLE_ACCURACY_MAXIMUM_PENALTY_TIME );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeapon44::ItemPreFrame( void )
{
	UpdatePenaltyTime();

	BaseClass::ItemPreFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeapon44::ItemBusyFrame( void )
{
	UpdatePenaltyTime();

	BaseClass::ItemBusyFrame();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon44::ItemPostFrame( void )
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
bool CWeapon44::Reload( void )
{
	m_flAccuracyPenalty = 0.0f;

	return BaseClass::Reload();
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon44::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
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
#endif
