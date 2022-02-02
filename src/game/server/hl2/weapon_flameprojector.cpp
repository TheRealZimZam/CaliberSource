//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Burn things funly - currently only (kinda) works for npcs
//
// TODO's; Fix sound/collision bug. Better fx. Optimize.
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "NPCEvent.h"
#include "basehlcombatweapon.h"
#include "basecombatcharacter.h"
#include "decals.h"
#include "AI_BaseNPC.h"
#include "player.h"
#include "soundent.h"
#include "te_particlesystem.h"
#include "IEffects.h"
#include "ndebugoverlay.h"
#include "in_buttons.h"
#include "ai_memory.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar sk_flameprojector_radius( "sk_flameprojector_radius", "224.0" );
#define MAX_BURN_RADIUS		sk_flameprojector_radius.GetFloat()

ConVar sk_flameprojector_rate( "sk_flameprojector_rate", "56.0" );
#define RADIUS_GROW_RATE	sk_flameprojector_rate.GetFloat()	// units/sec 

#define IMMOLATOR_TARGET_INVALID Vector( FLT_MAX, FLT_MAX, FLT_MAX )

class CWeaponFlameprojector : public CBaseHLCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponFlameprojector, CBaseHLCombatWeapon );

	DECLARE_SERVERCLASS();

	CWeaponFlameprojector( void );
	
	void Precache( void );
	void PrimaryAttack( void );
	void ItemPostFrame( void );

	int CapabilitiesGet( void ) {	return bits_CAP_WEAPON_RANGE_ATTACK1;	}

	void ImmolationDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore );
	virtual bool WeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions );	
	virtual int	WeaponRangeAttack1Condition( float flDot, float flDist );
	void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	void Update();
	void UpdateThink();

	void StartImmolating();
	void StopImmolating();
	bool IsImmolating() { return m_flBurnRadius != 0.0; }

	DECLARE_ACTTABLE();
	DECLARE_DATADESC();

	int	m_beamIndex;

	float m_flBurnRadius;
	float m_flTimeLastUpdatedRadius;

	Vector  m_vecFlameprojectorTarget;
};

IMPLEMENT_SERVERCLASS_ST(CWeaponFlameprojector, DT_WeaponFlameprojector)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( info_target_flameprojector, CPointEntity );
LINK_ENTITY_TO_CLASS( weapon_flameprojector, CWeaponFlameprojector );
PRECACHE_WEAPON_REGISTER( weapon_flameprojector );

BEGIN_DATADESC( CWeaponFlameprojector )

	DEFINE_FIELD( m_beamIndex, FIELD_INTEGER ),
	DEFINE_FIELD( m_flBurnRadius, FIELD_FLOAT ),
	DEFINE_FIELD( m_flTimeLastUpdatedRadius, FIELD_TIME ),
	DEFINE_FIELD( m_vecFlameprojectorTarget, FIELD_VECTOR ),

	DEFINE_ENTITYFUNC( UpdateThink ),
END_DATADESC()


//-----------------------------------------------------------------------------
// Maps base activities to weapons-specific ones so our characters do the right things.
//-----------------------------------------------------------------------------
acttable_t CWeaponFlameprojector::m_acttable[] = 
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

IMPLEMENT_ACTTABLE( CWeaponFlameprojector );

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CWeaponFlameprojector::CWeaponFlameprojector( void )
{
	m_fMaxRange1 = 1024;
	StopImmolating();
}

void CWeaponFlameprojector::StartImmolating()
{
	// Start the radius really tiny because we use radius == 0.0 to 
	// determine whether the immolator is operating or not.
	m_flBurnRadius = 0.1;
	m_flTimeLastUpdatedRadius = gpGlobals->curtime;
	SetThink( &CWeaponFlameprojector::UpdateThink );
	SetNextThink( gpGlobals->curtime + 0.25 );	//Needs some time to ignite

	CSoundEnt::InsertSound( SOUND_DANGER, m_vecFlameprojectorTarget, 256, 5.0, GetOwner() );
}

void CWeaponFlameprojector::StopImmolating()
{
	// Stop flamesound
	WeaponSound( EMPTY );

	m_flBurnRadius = 0.0;
	SetThink( NULL );
	m_vecFlameprojectorTarget = IMMOLATOR_TARGET_INVALID;
	m_flNextPrimaryAttack = gpGlobals->curtime + 4.0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFlameprojector::Precache( void )
{
	m_beamIndex = PrecacheModel( "sprites/bluelaser1.vmt" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFlameprojector::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if (!pPlayer)
		return;

	pPlayer->DoMuzzleFlash();

	// Ignite sound
	WeaponSound( SPECIAL1 );

	if( !IsImmolating() )
	{
		StartImmolating();
	}

	m_flNextPrimaryAttack = gpGlobals->curtime + 4.0;
	SendWeaponAnim( GetPrimaryAttackActivity() );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	// Register a muzzleflash for the AI
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFlameprojector::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
	case EVENT_WEAPON_SMG1:
		{
			WeaponSound( SPECIAL1 );

			pOperator->DoMuzzleFlash();
			m_flNextPrimaryAttack = gpGlobals->curtime + 4.0;

			if( !IsImmolating() )
			{
				StartImmolating();
			}
		}
		break;

	default:
		BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
		break;
	}
}

//-----------------------------------------------------------------------------
// This weapon is said to have Line of Sight when it CAN'T see the target, but
// can see a place near the target that can.
//-----------------------------------------------------------------------------
bool CWeaponFlameprojector::WeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions )
{
	CAI_BaseNPC* npcOwner = GetOwner()->MyNPCPointer();

	if( !npcOwner )
	{
		return false;
	}

	if( IsImmolating() )
	{
		// Don't update while Immolating. This is a committed attack.
		return false;
	}

	// Assume we won't find a target.
	m_vecFlameprojectorTarget = targetPos;

	if( npcOwner->FInViewCone( targetPos ) && npcOwner->FVisible( targetPos ) )
	{
		// We can see the target (usually the enemy). Don't fire the immolator directly at them.
		return false;
	}

	// Find a nearby immolator target that CAN see targetPOS
	float flNearest = FLT_MAX;
	CBaseEntity *pNearest = NULL;

	CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, "info_target_immolator" );

	while( pEnt )
	{
		float flDist = ( pEnt->GetAbsOrigin() - targetPos ).Length();

		if( flDist < flNearest )
		{
			trace_t tr;

			UTIL_TraceLine( targetPos, pEnt->GetAbsOrigin(), MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr );

			if( tr.fraction == 1.0 )
			{
				pNearest = pEnt;
				flNearest = flDist;
			}
		}

		pEnt = gEntList.FindEntityByClassname( pEnt, "info_target_immolator" );
	}

	if( pNearest )
	{
		m_vecFlameprojectorTarget = pNearest->GetAbsOrigin();
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Weapon firing conditions
//-----------------------------------------------------------------------------
int CWeaponFlameprojector::WeaponRangeAttack1Condition( float flDot, float flDist )
{
	if( IsImmolating() )
	{
		// Once is enough!
		return COND_NONE;
	}

	if(	m_vecFlameprojectorTarget == IMMOLATOR_TARGET_INVALID )
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

void CWeaponFlameprojector::UpdateThink( void )
{
	CBaseCombatCharacter *pOwner = GetOwner();

	if( pOwner && !pOwner->IsAlive() )
	{
		StopImmolating();
		return;
	}

	Update();
	SetNextThink( gpGlobals->curtime + 0.05 );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CWeaponFlameprojector::Update()
{
	float flDuration = gpGlobals->curtime - m_flTimeLastUpdatedRadius;
	if( flDuration != 0.0 )
	{
		m_flBurnRadius += RADIUS_GROW_RATE * flDuration;
		// Play flamesound
		WeaponSound( SINGLE );
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
		vecAiming = m_vecFlameprojectorTarget - vecSrc;
		VectorNormalize( vecAiming );
	}

	trace_t	tr;
	UTIL_TraceLine( vecSrc, vecSrc + vecAiming * m_fMaxRange1, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr );	//MAX_TRACE_LENGTH

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
				75,	// green
				25,		// blue,

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
						75,	// green
						25,		// blue,

						128, // bright
						100  // speed
						);
		}

		UTIL_Smoke( tr.endpos, random->RandomInt( 10, 15 ), 10 );

		// Immolator starts to hurt a few seconds after the effect is seen
		if( m_flBurnRadius > 64.0 )
		{
			UTIL_DecalTrace( &tr, "RedGlowFade" );
			ImmolationDamage( CTakeDamageInfo( this, this, 1, DMG_BURN ), tr.endpos, m_flBurnRadius, CLASS_NONE );
		}
	}
	else
	{
		// The attack beam struck some kind of entity directly, the radius doesnt have to be as big to do damage.
		if( m_flBurnRadius > 8.0 )
		{
			g_pEffects->Sparks( tr.endpos );
			ImmolationDamage( CTakeDamageInfo( this, this, 1, DMG_BURN ), tr.endpos, 8.0, CLASS_NONE );
		}
	}

	m_flTimeLastUpdatedRadius = gpGlobals->curtime;

	if( m_flBurnRadius >= MAX_BURN_RADIUS )
	{
		StopImmolating();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFlameprojector::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();
}


void CWeaponFlameprojector::ImmolationDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore )
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

			pBCC->Ignite( random->RandomFloat( 5, 10 ) );
		}
	}
}

