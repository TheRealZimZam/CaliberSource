//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Burn things funly
//
// TODO's; Code the bloody thing
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
#include "te_particlesystem.h"
#include "ndebugoverlay.h"
#include "ai_memory.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define FLAMETHROWER_MAX_BURN_RADIUS		256
#define FLAMETHROWER_RADIUS_GROW_RATE	50.0	// units/sec 
#define FLAMETHROWER_VELOCITY	2200.0	// units/sec 
#define FLAMETHROWER_MAXRANGE	376.0
#define FLAMETHROWER_PUSHFORCE	30.0	// For phys objects
#define FLAMETHROWER_DAMAGE_PER_SEC	100

#define FLAMETHROWER_TARGET_INVALID Vector( FLT_MAX, FLT_MAX, FLT_MAX )

enum FlameThrowerState_t
{
	// Firing states.
	FT_STATE_IDLE = 0,
	FT_STATE_STARTFIRING,
	FT_STATE_FIRING
};

//=========================================================
// Flamethrower Weapon
//=========================================================
class CWeaponFlamethrower : public CBaseHLCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponFlamethrower, CBaseHLCombatWeapon );

	DECLARE_SERVERCLASS();

	CWeaponFlamethrower( void );

	virtual void Spawn( void );

	void Precache( void );
	void PrimaryAttack( void );
	void ItemPostFrame( void );

	virtual void WeaponReset( void );
	virtual void DestroySounds( void );

	int CapabilitiesGet( void ) {	return bits_CAP_WEAPON_RANGE_ATTACK1;	}

	void FlameDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore );
	virtual bool WeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions );	
	virtual int	WeaponRangeAttack1Condition( float flDot, float flDist );

	void Update();
	void UpdateThink();

	void StartFlaming();
	void StopFlaming();
	bool IsFlaming() { return m_flBurnRadius != 0.0; }

	DECLARE_ACTTABLE();
	DECLARE_DATADESC();

	int	m_beamIndex;

	float m_flBurnRadius;
	float m_flTimeLastUpdatedRadius;

	CSoundPatch	*m_pFiringStartSound;
	CSoundPatch	*m_pFiringLoop;
	bool m_bFiringLoopCritical;
	bool m_bFlameEffects;

	Vector  m_vecFlamethrowerTarget;

private:
	CNetworkVar( int, m_iWeaponState );
	float m_flStartFiringTime;
	float m_flAmmoUseRemainder;

	CSoundPatch	*m_pFiringStartSound;
	CSoundPatch	*m_pFiringLoop;
	CSoundPatch *m_pPilotLightSound;

	bool		m_bFlameEffects;

};

IMPLEMENT_SERVERCLASS_ST(CWeaponFlamethrower, DT_WeaponFlamethrower)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( info_target_flamethrower, CPointEntity );
LINK_ENTITY_TO_CLASS( weapon_flamethrower, CWeaponFlamethrower );
PRECACHE_WEAPON_REGISTER( weapon_flamethrower );

BEGIN_DATADESC( CWeaponFlamethrower )

	DEFINE_FIELD( m_beamIndex, FIELD_INTEGER ),
	DEFINE_FIELD( m_flBurnRadius, FIELD_FLOAT ),
	DEFINE_FIELD( m_flTimeLastUpdatedRadius, FIELD_TIME ),
	DEFINE_FIELD( m_vecFlamethrowerTarget, FIELD_VECTOR ),

	DEFINE_ENTITYFUNC( UpdateThink ),
END_DATADESC()


//-----------------------------------------------------------------------------
// Maps base activities to weapons-specific ones so our characters do the right things.
//-----------------------------------------------------------------------------
acttable_t CWeaponFlamethrower::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_FLAMETHROWER,				true },
	{ ACT_WALK,						ACT_WALK_FLAMETHROWER,						true },
	{ ACT_RUN,						ACT_RUN_FLAMETHROWER,						true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_FLAMETHROWER,				true },
	// Readiness activities (aiming)
//	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG1_RELAXED,			false },//never aims	
//	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
//	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG1,			false },//always aims
	//
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_FLAMETHROWER,		true },
};

IMPLEMENT_ACTTABLE( CWeaponFlamethrower );

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CWeaponFlamethrower::CWeaponFlamethrower( void )
{
	// Max range of a non-napalm flamethrower
	m_fMaxRange1 = 512;
	StopFlaming();
}

void CWeaponFlamethrower::DestroySounds( void )
{
//#if defined( CLIENT_DLL )
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	if ( m_pFiringStartSound )
	{
		controller.SoundDestroy( m_pFiringStartSound );
		m_pFiringStartSound = NULL;
	}
	if ( m_pFiringLoop )
	{
		controller.SoundDestroy( m_pFiringLoop );
		m_pFiringLoop = NULL;
	}
	if ( m_pPilotLightSound )
	{
		controller.SoundDestroy( m_pPilotLightSound );
		m_pPilotLightSound = NULL;
	}
//#endif
}

void CWeaponFlamethrower::StartFlaming()
{
	// Start the radius really tiny because we use radius == 0.0 to 
	// determine whether the immolator is operating or not.
	m_flBurnRadius = 0.1;
	m_flTimeLastUpdatedRadius = gpGlobals->curtime;
	SetThink( &CWeaponFlamethrower::UpdateThink );
	SetNextThink( gpGlobals->curtime );

	CSoundEnt::InsertSound( SOUND_DANGER, m_vecFlamethrowerTarget, 256, 5.0, GetOwner() );
}

void CWeaponFlamethrower::StopFlaming()
{
	m_flBurnRadius = 0.0;
	SetThink( NULL );
	m_vecFlamethrowerTarget= FLAMETHROWER_TARGET_INVALID;
	m_flNextPrimaryAttack = gpGlobals->curtime + 5.0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFlamethrower::Precache( void )
{
	m_beamIndex = PrecacheModel( "sprites/bluelaser1.vmt" );

	BaseClass::Precache();
}

void CWeaponFlamethrower::WeaponReset( void )
{
	BaseClass::WeaponReset();

	m_iWeaponState = FT_STATE_IDLE;
	m_flStartFiringTime = 0;
	m_flAmmoUseRemainder = 0;

	DestroySounds();
}

void CWeaponFlamethrower::Spawn( void )
{
//	m_iAltFireHint = HINT_ALTFIRE_FLAMETHROWER;
	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFlamethrower::PrimaryAttack( void )
{
	// Are we capable of firing again?
	if ( m_flNextPrimaryAttack > gpGlobals->curtime )
		return;

	// Get the player owning the weapon.
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;

	if ( !CanAttack() )
	{
		StopFlaming();
	}

//	WeaponSound( SINGLE );

	if( !IsFlaming() )
	{
		StartFlaming();
	} 


	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;
	m_flTimeWeaponIdle = gpGlobals->curtime + 0.5f;

}

//-----------------------------------------------------------------------------
// This weapon is said to have Line of Sight when it CAN'T see the target, but
// can see a place near the target than can.
//-----------------------------------------------------------------------------
bool CWeaponFlamethrower::WeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions )
{
	CAI_BaseNPC* npcOwner = GetOwner()->MyNPCPointer();

	if( !npcOwner )
	{
		return false;
	}

	if( IsFlaming() )
	{
		// Don't update while Flaming. This is a committed attack.
		return false;
	}

	// Assume we won't find a target.
	m_vecFlamethrowerTarget = targetPos;

	if( npcOwner->FInViewCone( targetPos ) && npcOwner->FVisible( targetPos ) )
	{
		// We can see the target (usually the enemy). Don't fire the immolator directly at them.
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Weapon firing conditions
//-----------------------------------------------------------------------------
int CWeaponFlamethrower::WeaponRangeAttack1Condition( float flDot, float flDist )
{
	if( m_flNextPrimaryAttack > gpGlobals->curtime )
	{
		// Too soon to attack!
		return COND_NONE;
	}

	if( IsFlaming() )
	{
		// Once is enough!
		return COND_NONE;
	}

	if(	m_vecFlamethrowerTarget == FLAMETHROWER_TARGET_INVALID )
	{
		// No target!
		return COND_NONE;
	}

	if ( flDist > m_fMaxRange1 )
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	else if ( flDot < 0.5f )	// UNDONE: Why check this here? Isn't the AI checking this already?
	{
		return COND_NOT_FACING_ATTACK;
	}

	return COND_CAN_RANGE_ATTACK1;
	// !!!HACKHACK this makes the strider work! 
	// It's the Immolator's RangeAttack1, but it's the Strider's Range Attack 2.
//	return COND_CAN_RANGE_ATTACK2;
}

void CWeaponFlamethrower::UpdateThink( void )
{
	CBaseCombatCharacter *pOwner = GetOwner();

	if( pOwner && !pOwner->IsAlive() )
	{
		StopFlaming();
		return;
	}

	Update();
	SetNextThink( gpGlobals->curtime + 0.05 );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CWeaponFlamethrower::Update()
{
	float flDuration = gpGlobals->curtime - m_flTimeLastUpdatedRadius;
	if( flDuration != 0.0 )
	{
		m_flBurnRadius += RADIUS_GROW_RATE * flDuration;
	}

	// Clamp
	m_flBurnRadius = min( m_flBurnRadius, MAX_BURN_RADIUS );

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	Vector vecSrc;
	Vector vecAiming;

	if( pOwner )
	{
		vecSrc	 = pOwner->Weapon_ShootPosition( );
		vecAiming = pOwner->GetAutoaimVector(AUTOAIM_2DEGREES);
	}
	else
	{
		CBaseCombatCharacter *pOwner = GetOwner();

		vecSrc = pOwner->Weapon_ShootPosition( );
		vecAiming = m_vecFlamethrowerTarget - vecSrc;
		VectorNormalize( vecAiming );
	}

	trace_t	tr;
	UTIL_TraceLine( vecSrc, vecSrc + vecAiming * MAX_TRACE_LENGTH, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr );

	int brightness;
	brightness = 255 * (m_flBurnRadius/MAX_BURN_RADIUS);
	UTIL_Beam(  vecSrc,
				tr.endpos,
				m_beamIndex,
				0,		//halo index
				0,		//frame start
				2.0f,	//framerate
				0.1f,	//life
				20,		// width
				1,		// endwidth
				0,		// fadelength,
				1,		// noise

				255,	// red
				125,	// green
				0,		// blue,

				brightness, // bright
				100  // speed
				);


	if( tr.DidHitWorld() )
	{
		int beams;

		for( beams = 0 ; beams < 5 ; beams++ )
		{
			Vector vecDest;

			// Random unit vector
			vecDest.x = random->RandomFloat( -1, 1 );
			vecDest.y = random->RandomFloat( -1, 1 );
			vecDest.z = random->RandomFloat( 0, 1 );

			// Push out to radius dist.
			vecDest = tr.endpos + vecDest * m_flBurnRadius;

			UTIL_Beam(  tr.endpos,
						vecDest,
						m_beamIndex,
						0,		//halo index
						0,		//frame start
						2.0f,	//framerate
						0.15f,	//life
						20,		// width
						1.75,	// endwidth
						0.75,	// fadelength,
						15,		// noise

						255,	// red
						125,	// green
						0,		// blue,

						128, // bright
						100  // speed
						);
		}

		// Immolator starts to hurt a few seconds after the effect is seen
		if( m_flBurnRadius > 64.0 )
		{
			FlameDamage( CTakeDamageInfo( this, this, 1, DMG_BURN ), tr.endpos, m_flBurnRadius, CLASS_NONE );
		}
	}
	else
	{
		// The attack beam struck some kind of entity directly.
	}

	m_flTimeLastUpdatedRadius = gpGlobals->curtime;

	if( m_flBurnRadius >= MAX_BURN_RADIUS )
	{
		StopFlaming();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFlamethrower::ItemPostFrame( void )
{
//	if ( m_bLowered )
//		return;

	// Get the player owning the weapon.
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;

	int iAmmo = pOwner->GetAmmoCount( m_iPrimaryAmmoType );

	if ( pOwner->IsAlive() && ( pOwner->m_nButtons & IN_ATTACK ) && iAmmo > 0 )
	{
		PrimaryAttack();
	}

	BaseClass::ItemPostFrame();
}



void CWeaponFlamethrower::FlameDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore )
{
	CBaseEntity *pEntity = NULL;
	trace_t		tr;
	Vector		vecSpot;

	Vector vecSrc = vecSrcIn;

	// iterate on all entities in the vicinity.
	for ( CEntitySphereQuery sphere( vecSrc, flRadius ); pEntity = sphere.GetCurrentEntity(); sphere.NextEntity() )
	{
		CBaseCombatCharacter *pBCC;

		pBCC = pEntity->MyCombatCharacterPointer();

		if ( pEntity->m_takedamage != DAMAGE_NO )
		{
			// UNDONE: this should check a damage mask, not an ignore
			if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
			{
				continue;
			}

			pEntity->TakeDamage( info );

		}
		// Ignite the target too
		if ( pBCC && !pBCC->IsOnFire() )
		{
			// UNDONE: this should check a damage mask, not an ignore
			if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
			{
				continue;
			}

			if( pEntity == GetOwner() )
			{
				continue;
			}

			pBCC->Ignite( random->RandomFloat( 15, 20 ) );
		}
	}
}
