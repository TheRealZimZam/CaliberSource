//========= Copyright © 1996-2007, Valve Corporation, All rights reserved. ============//
//
// Purpose: Global functions for (almost) all weapons
//
//=============================================================================//

#include "cbase.h"
#include "hl2_player_shared.h"
#include "basehlcombatweapon_shared.h"
#include "in_buttons.h"
#include "ammodef.h"
#include "gamestats.h"
#if defined( CLIENT_DLL )
	#include "c_te_effect_dispatch.h"
	void UTIL_ClipPunchAngleOffset( QAngle &in, const QAngle &punch, const QAngle &clip )
	{
		QAngle	final = in + punch;

		//Clip each component
		for ( int i = 0; i < 3; i++ )
		{
			if ( final[i] > clip[i] )
			{
				final[i] = clip[i];
			}
			else if ( final[i] < -clip[i] )
			{
				final[i] = -clip[i];
			}

			//Return the result
			in[i] = final[i] - punch[i];
		}
	}
#else
	#include "basecombatcharacter.h"
	#include "AI_BaseNPC.h"
	#include "soundent.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( basehlcombatweapon, CBaseHLCombatWeapon );

IMPLEMENT_NETWORKCLASS_ALIASED( BaseHLCombatWeapon, DT_BaseHLCombatWeapon )

BEGIN_NETWORK_TABLE( CBaseHLCombatWeapon, DT_BaseHLCombatWeapon )
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
bool CBaseHLCombatWeapon::IsPredicted() const
{ 
	return true;
}

#ifdef CLIENT_DLL
bool CBaseHLCombatWeapon::ShouldPredict()
{
	if ( GetOwner() && GetOwner() == C_BasePlayer::GetLocalPlayer() )
		return true;

	return BaseClass::ShouldPredict();
}

void CBaseHLCombatWeapon::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( GetPredictable() && !ShouldPredict() )
		ShutdownPredictable();
}
#endif

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

IMPLEMENT_NETWORKCLASS_ALIASED( HLMachineGun, DT_HLMachineGun )

BEGIN_NETWORK_TABLE( CHLMachineGun, DT_HLMachineGun )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CHLMachineGun )
END_PREDICTION_DATA()

//=========================================================
//	>> CHLSelectFireMachineGun
//=========================================================
#ifndef CLIENT_DLL
BEGIN_DATADESC( CHLMachineGun )

	DEFINE_FIELD( m_nShotsFired,	FIELD_INTEGER ),
	DEFINE_FIELD( m_flNextSoundTime, FIELD_TIME ),

END_DATADESC()
#endif

CHLMachineGun::CHLMachineGun( void )
{
}

const Vector &CHLMachineGun::GetBulletSpread( void )
{
	static Vector cone = VECTOR_CONE_3DEGREES;
	if ( GetHSpread() != NULL )
		cone = Vector( GetHSpread(), ((GetHSpread() / 2) + (GetVSpread() / 2)), GetVSpread() );

	return cone;
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CHLMachineGun::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if (!pPlayer)
		return;
	
	// Abort here to handle burst and auto fire modes
	if ( (UsesClipsForAmmo1() && m_iClip1 == 0) || ( !UsesClipsForAmmo1() && !pPlayer->GetAmmoCount(m_iPrimaryAmmoType) ) )
	{
		DryFire();
		return;
	}

	m_nShotsFired++;

	pPlayer->DoMuzzleFlash();

	// To make the firing framerate independent, we may have to fire more than one bullet here on low-framerate systems, 
	// especially if the weapon we're firing has a really fast rate of fire.
	int iBulletsToFire = 0;
	float fireRate = GetFireRate();

	// MUST call sound before removing a round from the clip of a CHLMachineGun
	while ( m_flNextPrimaryAttack <= gpGlobals->curtime )
	{
		WeaponSound(SINGLE, m_flNextPrimaryAttack);
		m_flNextPrimaryAttack = m_flNextPrimaryAttack + fireRate;
		iBulletsToFire++;
	}

	// Make sure we don't fire more than the amount in the clip, if this weapon uses clips
	if ( UsesClipsForAmmo1() )
	{
		if ( iBulletsToFire > m_iClip1 )
			iBulletsToFire = m_iClip1;
		m_iClip1 -= iBulletsToFire;
	}

	m_iPrimaryAttacks++;

	// Fire the bullets
	FireBulletsInfo_t info;
	info.m_iShots = iBulletsToFire;
	info.m_vecSrc = pPlayer->Weapon_ShootPosition( );
	info.m_vecDirShooting = pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );
#ifndef CLIENT_DLL
	info.m_vecSpread = pPlayer->GetAttackSpread( this );
#else
	//!!!HACKHACK - what does the client want this function for? 
	info.m_vecSpread = GetActiveWeapon()->GetBulletSpread( WEAPON_PROFICIENCY_PERFECT );
#endif // CLIENT_DLL

	info.m_flDistance = MAX_TRACE_LENGTH;
	info.m_iAmmoType = m_iPrimaryAmmoType;
	info.m_iTracerFreq = 2;
	FireBullets( info );

	//Factor in the view kick
	AddViewKick();

	SendWeaponAnim( GetPrimaryAttackActivity() );
	//player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

#ifndef CLIENT_DLL
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
	CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, GetAbsOrigin(), SOUNDENT_VOLUME_MACHINEGUN, 0.2, pPlayer );
	pPlayer->SetAimTime( 2.0f );
	// Register a muzzleflash for the AI
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );
#endif
	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
//-----------------------------------------------------------------------------
void CHLMachineGun::FireBullets( const FireBulletsInfo_t &info )
{
	if(CBasePlayer *pPlayer = ToBasePlayer ( GetOwner() ) )
	{
		pPlayer->FireBullets(info);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add recoil and kick effects
// TODO; Accuracy should be separate to the kick angle, and crouching should
// reduce the recoil/accuracy hit (sort of like RTCW)
//-----------------------------------------------------------------------------
void CHLMachineGun::DoMachineGunKick( CBasePlayer *pPlayer, float dampEasy, float maxVerticleKickAngle, float fireDurationTime, float slideLimitTime )
{
	#define	KICK_MIN_X			0.2f	//Degrees
	#define	KICK_MIN_Y			0.2f	//Degrees
	#define	KICK_MIN_Z			0.1f	//Degrees

	QAngle vecScratch;
	
	//Find how far into our accuracy degradation we are
	float duration	= ( fireDurationTime > slideLimitTime ) ? slideLimitTime : fireDurationTime;
	float kickPerc = duration / slideLimitTime;

	// do this to get a hard discontinuity, clear out anything under 10 degrees punch
	pPlayer->ViewPunchReset( 10 );

	//Apply this to the view angles as well
	vecScratch.x = -( KICK_MIN_X + ( maxVerticleKickAngle * kickPerc ) );
	vecScratch.y = -( KICK_MIN_Y + ( maxVerticleKickAngle * kickPerc ) ) / 3;
	vecScratch.z = KICK_MIN_Z + ( maxVerticleKickAngle * kickPerc ) / 8;

	//Wibble left and right
	if ( random->RandomInt( -1, 1 ) >= 0 )
		vecScratch.y *= -1;

	//Wobble up and down
	if ( random->RandomInt( -1, 1 ) >= 0 )
		vecScratch.z *= -1;

	//If we're in easy, dampen the effect a bit
#ifdef GAME_DLL
	if ( g_pGameRules->IsSkillLevel( SKILL_EASY ) )
	{
		for ( int i = 0; i < 3; i++ )
		{
			vecScratch[i] *= dampEasy;
		}
	}
#endif

	//Clip this to our desired min/max
	UTIL_ClipPunchAngleOffset( vecScratch, pPlayer->m_Local.m_vecPunchAngle, QAngle( 24.0f, 3.0f, 1.0f ) );

	//Add it to the view punch
	// NOTE: 0.5 is just tuned to match the old effect before the punch became simulated
	pPlayer->ViewPunch( vecScratch * 0.5 );
}

//-----------------------------------------------------------------------------
// Purpose: Reset our shots fired
//-----------------------------------------------------------------------------
bool CHLMachineGun::Deploy( void )
{
	m_nShotsFired = 0;

	return BaseClass::Deploy();
}


//NOTENOTE; Shouldnt this just do a looped sound??
//-----------------------------------------------------------------------------
// Purpose: Make enough sound events to fill the estimated think interval
// returns: number of shots needed
//-----------------------------------------------------------------------------
int CHLMachineGun::WeaponSoundRealtime( WeaponSound_t shoot_type )
{
	int numBullets = 0;

	// ran out of time, clamp to current
	if (m_flNextSoundTime < gpGlobals->curtime)
	{
		m_flNextSoundTime = gpGlobals->curtime;
	}

	// make enough sound events to fill up the next estimated think interval
	float dt = clamp( m_flAnimTime - m_flPrevAnimTime, 0, 0.2 );
	if (m_flNextSoundTime < gpGlobals->curtime + dt)
	{
		WeaponSound( SINGLE_NPC, m_flNextSoundTime );
		m_flNextSoundTime += GetFireRate();
		numBullets++;
	}
	if (m_flNextSoundTime < gpGlobals->curtime + dt)
	{
		WeaponSound( SINGLE_NPC, m_flNextSoundTime );
		m_flNextSoundTime += GetFireRate();
		numBullets++;
	}

	return numBullets;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CHLMachineGun::GetPrimaryAttackActivity( void )
{
	if ( m_nShotsFired > 2 )
		return ACT_VM_RECOIL1;

	if ( m_nShotsFired > 4 )
		return ACT_VM_RECOIL2;

	if ( m_nShotsFired > 6 )
		return ACT_VM_RECOIL3;

	return ACT_VM_PRIMARYATTACK;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLMachineGun::ItemPostFrame( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	// Debounce the recoiling counter
	if ( ( pOwner->m_nButtons & IN_ATTACK ) == false )
	{
		m_nShotsFired = 0;
	}

	BaseClass::ItemPostFrame();
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: Weapon firing conditions
//-----------------------------------------------------------------------------
int CHLMachineGun::WeaponRangeAttack1Condition( float flDot, float flDist )
{
	if ( m_iClip1 <=0 )
	{
		return COND_NO_PRIMARY_AMMO;
	}
	else if ( flDist < m_fMinRange1 ) 
	{
		return COND_TOO_CLOSE_TO_ATTACK;
	}
	else if ( flDist > m_fMaxRange1 )
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	else if ( flDot < 0.5f )	// UNDONE: Why check this here? Isn't the AI checking this already?
	{
		return COND_NOT_FACING_ATTACK;
	}

	return COND_CAN_RANGE_ATTACK1;
}
#endif


IMPLEMENT_NETWORKCLASS_ALIASED( HLSelectFireMachineGun, DT_HLSelectFireMachineGun )

BEGIN_NETWORK_TABLE( CHLSelectFireMachineGun, DT_HLSelectFireMachineGun )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CHLSelectFireMachineGun )
END_PREDICTION_DATA()

//=========================================================
//	>> CHLSelectFireMachineGun
//=========================================================
#ifndef CLIENT_DLL
BEGIN_DATADESC( CHLSelectFireMachineGun )

	DEFINE_FIELD( m_iBurstSize,		FIELD_INTEGER ),
	DEFINE_FIELD( m_iFireMode,		FIELD_INTEGER ),
	
	// Function pinters
	DEFINE_FUNCTION( BurstThink ),

END_DATADESC()
#endif

CHLSelectFireMachineGun::CHLSelectFireMachineGun( void )
{
}

float CHLSelectFireMachineGun::GetBurstCycleRate( void )
{
	// this is the time it takes to fire an entire 
	// burst, plus whatever amount of delay we want
	// to have between bursts.
	return 0.5f;
}

float CHLSelectFireMachineGun::GetFireRate( void )
{
	switch( m_iFireMode )
	{
	case FIREMODE_FULLAUTO:
		// the time between rounds fired on full auto
		return 0.1f;	// 600 rounds per minute = 0.1 seconds per bullet
		break;

	case FIREMODE_3RNDBURST:	// FIXME
		// the time between rounds fired within a single burst
		return 0.1f;	// 600 rounds per minute = 0.1 seconds per bullet
		break;

	default:
		return BaseClass::GetFireRate();
		break;
	}
}

bool CHLSelectFireMachineGun::Deploy( void )
{
	// Forget about any bursts this weapon was firing when holstered
	m_iBurstSize = 0;
	return BaseClass::Deploy();
}


//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CHLSelectFireMachineGun::PrimaryAttack( void )
{
	if (m_bFireOnEmpty)
	{
		return;
	}
	switch( m_iFireMode )
	{
	case FIREMODE_FULLAUTO:
		BaseClass::PrimaryAttack();
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + GetFireRate();
		SetWeaponIdleTime( gpGlobals->curtime + 3.0f );
		break;

	case FIREMODE_3RNDBURST:
		m_iBurstSize = GetBurstSize();
		
		// Call the think function directly so that the first round gets fired immediately.
		BurstThink();
		SetThink( &CHLSelectFireMachineGun::BurstThink );
		m_flNextPrimaryAttack = gpGlobals->curtime + GetBurstCycleRate();
		m_flNextSecondaryAttack = gpGlobals->curtime + GetBurstCycleRate();

		// Pick up the rest of the burst through the think function.
		SetNextThink( gpGlobals->curtime + GetFireRate() );
		break;
	}

#ifndef CLIENT_DLL
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner )
	{
		m_iPrimaryAttacks++;
		gamestats->Event_WeaponFired( pOwner, true, GetClassname() );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CHLSelectFireMachineGun::SecondaryAttack( void )
{
	ChangeFireMode();
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.3;
}

void CHLSelectFireMachineGun::TertiaryAttack( void )
{
	ChangeFireMode();
	m_flNextTertiaryAttack = gpGlobals->curtime + 0.3;
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CHLSelectFireMachineGun::ChangeFireMode( void )
{
	// change fire modes
	switch( m_iFireMode )
	{
	case FIREMODE_FULLAUTO:
		//Msg( "Burst\n" );
		m_iFireMode = FIREMODE_3RNDBURST;
		WeaponSound(SPECIAL2);
		break;

	case FIREMODE_3RNDBURST:
		//Msg( "Auto\n" );
		m_iFireMode = FIREMODE_FULLAUTO;
		WeaponSound(SPECIAL1);
		break;
	}
	
	SendWeaponAnim( GetSecondaryAttackActivity() );

#ifndef CLIENT_DLL
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner )
		gamestats->Event_WeaponFired( pOwner, false, GetClassname() );
#endif
}

void CHLSelectFireMachineGun::BurstThink( void )
{
	CHLMachineGun::PrimaryAttack();

	m_iBurstSize--;

	if( m_iBurstSize == 0 )
	{
		// The burst is over!
		SetThink(NULL);

		// idle immediately to stop the firing animation
		SetWeaponIdleTime( gpGlobals->curtime );
		return;
	}

	SetNextThink( gpGlobals->curtime + GetFireRate() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CHLSelectFireMachineGun::WeaponSound( WeaponSound_t shoot_type, float soundtime /*= 0.0f*/ )
{
	if (shoot_type == SINGLE)
	{
		switch( m_iFireMode )
		{
		case FIREMODE_FULLAUTO:
			BaseClass::WeaponSound( SINGLE, soundtime );
			break;

		case FIREMODE_3RNDBURST:
			if( m_iBurstSize == GetBurstSize() && m_iClip1 >= m_iBurstSize )
			{
				// First round of a burst, and enough bullets remaining in the clip to fire the whole burst
				BaseClass::WeaponSound( BURST, soundtime );
			}
			else if( m_iClip1 < m_iBurstSize )
			{
				// Not enough rounds remaining in the magazine to fire a burst, so play the gunshot
				// sounds individually as each round is fired.
				BaseClass::WeaponSound( SINGLE, soundtime );
			}

			break;
		}
		return;
	}

	BaseClass::WeaponSound( shoot_type, soundtime );
}

#ifndef CLIENT_DLL
// BUGBUG: These need to be rethought
//-----------------------------------------------------------------------------
int CHLSelectFireMachineGun::WeaponRangeAttack1Condition( float flDot, float flDist )
{
	if (m_iClip1 <=0)
	{
		return COND_NO_PRIMARY_AMMO;
	}
	else if ( flDist < m_fMinRange1) 
	{
		return COND_TOO_CLOSE_TO_ATTACK;
	}
	else if ( flDist > m_fMaxRange1) 
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	else if ( flDot < 0.5)	// UNDONE: Why check this here? Isn't the AI checking this already?
	{
		return COND_NOT_FACING_ATTACK;
	}

	return COND_CAN_RANGE_ATTACK1;
}

//-----------------------------------------------------------------------------
int CHLSelectFireMachineGun::WeaponRangeAttack2Condition( float flDot, float flDist )
{
	// NOTENOTE; This needs to be defined on a per-weapon basis
	return COND_NONE;

	// Default conds
#if 0
	// m_iClip2 == -1 when no secondary clip is used
	if ( m_iClip2 == 0 && UsesSecondaryAmmo() )
	{
		return COND_NO_SECONDARY_AMMO;
	}
	else if ( flDist < m_fMinRange2 )
	{
		// Don't return	COND_TOO_CLOSE_TO_ATTACK only for primary attack
		return COND_NONE;
	}
	else if ( flDist > m_fMaxRange2 )
	{
		// Don't return COND_TOO_FAR_TO_ATTACK only for primary attack
		return COND_NONE;
	}
	else if ( flDot < 0.5 ) // UNDONE: Why check this here? Isn't the AI checking this already?
	{
		return COND_NOT_FACING_ATTACK;
	}

	return COND_CAN_RANGE_ATTACK2;
#endif
}
#endif

