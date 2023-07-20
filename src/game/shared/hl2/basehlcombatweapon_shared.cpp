//========= Copyright © 1996-2007, Valve Corporation, All rights reserved. ============//
//
// Purpose: Global functions for (almost) all weapons
//
//=============================================================================//

#include "cbase.h"
#include "hl2_player_shared.h"
#include "basehlcombatweapon_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( basehlcombatweapon, CBaseHLCombatWeapon );

IMPLEMENT_NETWORKCLASS_ALIASED( BaseHLCombatWeapon , DT_BaseHLCombatWeapon )

BEGIN_NETWORK_TABLE( CBaseHLCombatWeapon , DT_BaseHLCombatWeapon )
#if !defined( CLIENT_DLL )
//	SendPropInt( SENDINFO( m_bReflectViewModelAnimations ), 1, SPROP_UNSIGNED ),
#else
//	RecvPropInt( RECVINFO( m_bReflectViewModelAnimations ) ),
#endif
END_NETWORK_TABLE()


#if !defined( CLIENT_DLL )

#include "globalstate.h"

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CBaseHLCombatWeapon )

	DEFINE_FIELD( m_bLowered,				FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flRaiseTime,			FIELD_TIME ),
	DEFINE_FIELD( m_flHolsterTime,			FIELD_TIME ),
	DEFINE_FIELD( m_iPrimaryAttacks,		FIELD_INTEGER ),
	DEFINE_FIELD( m_iSecondaryAttacks,		FIELD_INTEGER ),

END_DATADESC()

#endif

BEGIN_PREDICTION_DATA( CBaseHLCombatWeapon )
END_PREDICTION_DATA()

ConVar sk_auto_reload_time( "sk_auto_reload_time", "12", FCVAR_REPLICATED );
ConVar sk_realistic_spread( "sk_realistic_spread", "0", FCVAR_REPLICATED );
ConVar sk_spread_moving( "sk_spread_moving", "1.2", FCVAR_REPLICATED );	// Moving/walking
ConVar sk_spread_movingfast( "sk_spread_movingfast", "1.4", FCVAR_REPLICATED );	// Moving/jogging/sprinting
ConVar sk_spread_flying( "sk_spread_flying", "1.5", FCVAR_REPLICATED );	// Jumping/flying/swimming
ConVar sk_spread_crouch( "sk_spread_crouch", "0.8", FCVAR_REPLICATED );	// Crouching

//extern ConVar hl2_runspeed;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHLCombatWeapon::ItemHolsterFrame( void )
{
	BaseClass::ItemHolsterFrame();

	// Must be player held
	if ( GetOwner() && GetOwner()->IsPlayer() == false )
		return;

	// We can't be active
	if ( GetOwner()->GetActiveWeapon() == this )
		return;

	// If it's been longer than ten seconds, reload
	if ( ( gpGlobals->curtime - m_flHolsterTime ) > sk_auto_reload_time.GetFloat() && !m_bReloadsFullClip )
	{
		// Just load the clip with no animations
		FinishReload();
		m_flHolsterTime = gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CBaseHLCombatWeapon::CanLower()
{
	//Don't bother if we don't have the animation
	if ( SelectWeightedSequence( ACT_VM_IDLE_LOWERED ) == ACTIVITY_NOT_AVAILABLE )
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Drops the weapon into a lowered pose
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHLCombatWeapon::Lower( void )
{
	if ( !CanLower() )
		return false;

	m_bLowered = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Brings the weapon up to the ready position
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHLCombatWeapon::Ready( void )
{
	//Don't bother if we don't have the animation
	if ( SelectWeightedSequence( ACT_VM_LOWERED_TO_IDLE ) == ACTIVITY_NOT_AVAILABLE )
		return false;

	m_bLowered = false;	
	m_flRaiseTime = gpGlobals->curtime + 0.5f;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHLCombatWeapon::Deploy( void )
{
	// If we should be lowered, deploy in the lowered position
	// We have to ask the player if the last time it checked, the weapon was lowered
	if ( GetOwner() && GetOwner()->IsPlayer() )
	{
		WeaponSound( DEPLOY );
		CHL2_Player *pPlayer = assert_cast<CHL2_Player*>( GetOwner() );
		if ( pPlayer->IsWeaponLowered() )
		{
			if ( SelectWeightedSequence( ACT_VM_IDLE_LOWERED ) != ACTIVITY_NOT_AVAILABLE )
			{
				if ( DefaultDeploy( (char*)GetViewModel(), (char*)GetWorldModel(), ACT_VM_IDLE_LOWERED, (char*)GetAnimPrefix() ) )
				{
					m_bLowered = true;

					// Stomp the next attack time to fix the fact that the lower idles are long
					pPlayer->SetNextAttack( gpGlobals->curtime + 1.0 );
					m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
					m_flNextSecondaryAttack	= gpGlobals->curtime + 1.0;
					return true;
				}
			}
		}
	}

	m_bLowered = false;
	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHLCombatWeapon::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( BaseClass::Holster( pSwitchingTo ) )
	{
		m_flHolsterTime = gpGlobals->curtime;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHLCombatWeapon::WeaponShouldBeLowered( void )
{
	// Can't be in the middle of another animation
  	if ( GetIdealActivity() != ACT_VM_IDLE_LOWERED && GetIdealActivity() != ACT_VM_IDLE &&
		 GetIdealActivity() != ACT_VM_IDLE_TO_LOWERED && GetIdealActivity() != ACT_VM_LOWERED_TO_IDLE )
  		return false;

	if ( m_bLowered )
		return true;
	
#if !defined( CLIENT_DLL )

	if ( GlobalEntity_GetState( "friendly_encounter" ) == GLOBAL_ON )
		return true;

#endif

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Allows the weapon to choose proper weapon idle animation
//-----------------------------------------------------------------------------
void CBaseHLCombatWeapon::WeaponIdle( void )
{
	//See if we should idle high or low
	if ( WeaponShouldBeLowered() )
	{
#if !defined( CLIENT_DLL )
		CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(GetOwner());

		if( pPlayer )
		{
			pPlayer->Weapon_Lower();
		}
#endif

		// Move to lowered position if we're not there yet
		if ( GetActivity() != ACT_VM_IDLE_LOWERED && GetActivity() != ACT_VM_IDLE_TO_LOWERED 
			 && GetActivity() != ACT_TRANSITION )
		{
			SendWeaponAnim( ACT_VM_IDLE_LOWERED );
		}
		else if ( HasWeaponIdleTimeElapsed() )
		{
			// Keep idling low
			SendWeaponAnim( ACT_VM_IDLE_LOWERED );
		}
	}
	else
	{
		// See if we need to raise immediately
		if ( m_flRaiseTime < gpGlobals->curtime && GetActivity() == ACT_VM_IDLE_LOWERED ) 
		{
			SendWeaponAnim( ACT_VM_IDLE );
		}
		else if ( HasWeaponIdleTimeElapsed() ) 
		{
			SendWeaponAnim( ACT_VM_IDLE );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Dryfire animation when an attack is used without ammo
//-----------------------------------------------------------------------------
void CBaseHLCombatWeapon::DryFire( void )
{
	WeaponSound( EMPTY );
	SendWeaponAnim( ACT_VM_DRYFIRE );

	m_flNextPrimaryAttack		= gpGlobals->curtime + SequenceDuration();
	m_flNextSecondaryAttack		= gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: Factor in the player's anim speed to the sequence duration for weapons
//-----------------------------------------------------------------------------
float CBaseHLCombatWeapon::SequenceDuration( int iSequence )
{
	float flDuration = BaseClass::SequenceDuration( iSequence );
	CHL2_Player *pOwner = dynamic_cast<CHL2_Player*>(GetOwner());
	if ( pOwner )
	{
		flDuration /= pOwner->GetDefaultAnimSpeed();
	}

	return flDuration;
}

//-----------------------------------------------------------------------------
// Purpose: Calculate bullet spread
// NOTE; This is done for each shot, so try to keep it simple!
//-----------------------------------------------------------------------------
Vector CBaseHLCombatWeapon::CalculateBulletSpread( WeaponProficiency_t proficiency )
{
	// Apply aiming factors for player AND npcs
	float fSpreadModifier = 1.0f;
	const WeaponProficiencyInfo_t *pProficiencyValues = GetProficiencyValues();
	float flProficiencyModifier = (pProficiencyValues)[ proficiency ].spreadscale;

	if ( sk_realistic_spread.GetBool() )
	{
		// This also applies to npcs - shooting on the move is penalized
		if ( GetOwner()->GetAbsVelocity().Length() > 120 )	//walkspeed
			fSpreadModifier = sk_spread_moving.GetFloat();

		// Player only
		if ( GetOwner() && GetOwner()->IsPlayer() )
		{
			// Only for HL2 players
			CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(GetOwner());

			if ( pPlayer->GetAbsVelocity().Length2D() > 260 )	//sprintspeed
				fSpreadModifier = sk_spread_movingfast.GetFloat();

			if ( !(pPlayer->GetFlags() & FL_ONGROUND) || pPlayer->GetMoveType( ) == MOVETYPE_LADDER )
				fSpreadModifier = sk_spread_flying.GetFloat();

			if ( pPlayer->GetFlags() & FL_DUCKING )
				fSpreadModifier = sk_spread_crouch.GetFloat();
		}
	}

	// UNDONE: Now get the original spread, make it into a float, and apply proficiency
	// NOTE; Since this code only applies to weapons with default spread (weaponscript) for now, im gonna take the easy way out here
	float fHSpreadAmount;
	float fVSpreadAmount;
	fHSpreadAmount = GetHSpread() * fSpreadModifier * flProficiencyModifier;
	fVSpreadAmount = GetVSpread() * fSpreadModifier * flProficiencyModifier;

	// Finally, pack it back up into a vector
	Vector adjustedcone = Vector( fHSpreadAmount, ((fHSpreadAmount / 2) + (fVSpreadAmount / 2)), fVSpreadAmount );

	return adjustedcone;
}

const Vector &CBaseHLCombatWeapon::GetBulletSpread( void )
{
	return BaseClass::GetBulletSpread();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#if defined( CLIENT_DLL ) && ( !defined( HL2MP ) && !defined( PORTAL ) )

#define	HL2_BOB_CYCLE_MIN	1.0f
#define	HL2_BOB_CYCLE_MAX	0.45f
#define	HL2_BOB			0.002f
#define	HL2_BOB_UP		0.5f

// Defaults, can be changed ingame
static ConVar	cl_bob( "cl_bob","0.001" );
static ConVar	cl_bobcycle( "cl_bobcycle","0.8" );
static ConVar	cl_bobidle( "cl_bobidle","0.4" );
static ConVar	cl_bobup( "cl_bobup","0.5" );

// Register these cvars if needed for easy tweaking
static ConVar	v_iyaw_cycle( "v_iyaw_cycle", "2"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_iroll_cycle( "v_iroll_cycle", "0.5"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_ipitch_cycle( "v_ipitch_cycle", "1"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_iyaw_level( "v_iyaw_level", "0.3"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_iroll_level( "v_iroll_level", "0.1"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_ipitch_level( "v_ipitch_level", "0.3"/*, FCVAR_UNREGISTERED*/ );
#if 0
static ConVar	v_kickpitch( "v_kickpitch", "0.6"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_kickroll( "v_kickroll", "0.6"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_kicktime( "v_kicktime", "0.5"/*, FCVAR_UNREGISTERED*/ );
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
// TODO; Jumping/Flying kinda breaks this
//-----------------------------------------------------------------------------
float CBaseHLCombatWeapon::CalcViewmodelBob( void )
{
	static double	bobtime;
	static float	bob;
	float	cycle;

	CBasePlayer *player = ToBasePlayer( GetOwner() );
	if ( !player )
		return 0.0f;

	if ( ( player->GetGroundEntity() == NULL ) || !gpGlobals->frametime )
	{
		return bob;		// just use old value
	}

	bobtime += gpGlobals->frametime;
	
	cycle = bobtime - (int)(bobtime/cl_bobcycle.GetFloat())*cl_bobcycle.GetFloat();
	cycle /= cl_bobcycle.GetFloat();

	if (cycle < cl_bobup.GetFloat())
	{
		cycle = M_PI * cycle / cl_bobup.GetFloat();
	}
	else
	{
		cycle = M_PI + M_PI*(cycle-cl_bobup.GetFloat())/(1.0 - cl_bobup.GetFloat());
	}

	// bob is proportional to simulated velocity in the xy plane
	// (don't count Z, or jumping messes it up)
	bob = player->GetAbsVelocity().Length2D() * cl_bob.GetFloat();	//GetLocalVelocity()

	bob = bob*0.3 + bob*0.7*sin(cycle);

	bob = min( 4.0, bob );
	bob = max( -7.0, bob );
	return bob;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
void CBaseHLCombatWeapon::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{
	float fIdleScale = cl_bobidle.GetFloat();

	// TODO; Kinda works... causes a crash on death but otherwise has the intended effect
#if 0
	CBasePlayer *player = ToBasePlayer( GetOwner() );
	if ( player->GetFlags() & FL_DUCKING )
	{
		fIdleScale = cl_bobidle.GetFloat() * 0.5;
	}
#endif

	// Bias so view models aren't all synced to each other
	//float curtime = gpGlobals->curtime + ( viewmodelindex * 2 * M_PI / GetViewModelCount() );

	float curtime = gpGlobals->curtime + ( viewmodel->entindex() * 2 * M_PI );

	origin[ROLL]	-= fIdleScale * sin(curtime*v_iroll_cycle.GetFloat()) * v_iroll_level.GetFloat();
	origin[PITCH]	-= fIdleScale * sin(curtime*v_ipitch_cycle.GetFloat()) * (v_ipitch_level.GetFloat() * 0.5);
	origin[YAW]		-= fIdleScale * sin(curtime*v_iyaw_cycle.GetFloat()) * v_iyaw_level.GetFloat();

	Vector	forward;
	AngleVectors( angles, &forward, NULL, NULL );

	float	flBob = CalcViewmodelBob();

	// Apply bob, but scaled down to 40%
	VectorMA( origin, flBob * 0.4, forward, origin );

	// Z bob a bit more
	origin[2] += flBob;

	// throw in a little tilt.
	angles[ YAW ]	-= flBob * 0.6;
	angles[ ROLL ]	-= flBob * 0.5;
	angles[ PITCH ]	-= flBob * 0.4;
}

//-----------------------------------------------------------------------------
Vector CBaseHLCombatWeapon::GetBulletSpread( WeaponProficiency_t proficiency )
{
	return BaseClass::GetBulletSpread( proficiency );
}

//-----------------------------------------------------------------------------
float CBaseHLCombatWeapon::GetSpreadBias( WeaponProficiency_t proficiency )
{
	return BaseClass::GetSpreadBias( proficiency );
}
//-----------------------------------------------------------------------------

const WeaponProficiencyInfo_t *CBaseHLCombatWeapon::GetProficiencyValues()
{
	return NULL;
}

#else

// Server stubs
float CBaseHLCombatWeapon::CalcViewmodelBob( void )
{
	return 0.0f;
}

//-----------------------------------------------------------------------------
void CBaseHLCombatWeapon::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{
}

//-----------------------------------------------------------------------------
Vector CBaseHLCombatWeapon::GetBulletSpread( WeaponProficiency_t proficiency )
{
	if ( GetHSpread() != NULL )
	{
		Vector CalculatedSpread = CalculateBulletSpread( proficiency );
		return CalculatedSpread;
	}
	else
	{
		Vector BaseSpread = BaseClass::GetBulletSpread( proficiency );

		const WeaponProficiencyInfo_t *pProficiencyValues = GetProficiencyValues();
		float flModifier = (pProficiencyValues)[ proficiency ].spreadscale;
		return ( BaseSpread * flModifier );
	}
}

//-----------------------------------------------------------------------------
float CBaseHLCombatWeapon::GetSpreadBias( WeaponProficiency_t proficiency )
{
	const WeaponProficiencyInfo_t *pProficiencyValues = GetProficiencyValues();
	return (pProficiencyValues)[ proficiency ].bias;
}

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CBaseHLCombatWeapon::GetProficiencyValues()
{
	return GetDefaultProficiencyValues();
}

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CBaseHLCombatWeapon::GetDefaultProficiencyValues()
{
	// Weapon proficiency table. Keep this in sync with WeaponProficiency_t enum in the header!!
	static WeaponProficiencyInfo_t g_BaseWeaponProficiencyTable[] =
	{
		{ 2.80, 1.0	},
		{ 2.40, 1.0	},
		{ 2.00, 1.0	},
		{ 1.50, 1.0	},
		{ 1.25, 1.0 },
		{ 1.00, 1.0	},
	};

	COMPILE_TIME_ASSERT( ARRAYSIZE(g_BaseWeaponProficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);

	return g_BaseWeaponProficiencyTable;
}

#endif