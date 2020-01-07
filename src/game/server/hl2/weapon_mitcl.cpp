//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Non-guidable light rocket launcher
//
//=============================================================================//

#include "cbase.h"
#include "basehlcombatweapon.h"
#include "basecombatcharacter.h"
#include "movie_explosion.h"
#include "soundent.h"
#include "player.h"
#include "rope.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "explode.h"
#include "util.h"
#include "in_buttons.h"
#include "weapon_mitcl.h"
#include "shake.h"
#include "AI_BaseNPC.h"
#include "AI_Squad.h"
#include "te_effect_dispatch.h"
#include "triggers.h"
#include "smoke_trail.h"
#include "collisionutils.h"
#include "hl2_shareddefs.h"
#include "rumble_shared.h"
#include "gamestats.h"

#ifdef HL2_DLL
	extern int g_interactionPlayerLaunchedRPG;
#endif
	extern CHandle<CMissile>	m_hMissile;
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//=============================================================================
// LIGHT-RPG
//=============================================================================

IMPLEMENT_SERVERCLASS_ST(CWeaponMITCL, DT_WeaponMITCL)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_mitcl, CWeaponMITCL );
PRECACHE_WEAPON_REGISTER(weapon_mitcl);

BEGIN_DATADESC( CWeaponMITCL )

	DEFINE_FIELD( m_bInitialStateUpdate,FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hMissile,			FIELD_EHANDLE ),

END_DATADESC()

acttable_t	CWeaponMITCL::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_RPG, true },

	{ ACT_IDLE_RELAXED,				ACT_IDLE_RPG_RELAXED,			true },
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_ANGRY_RPG,				true },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_RPG,				true },

	{ ACT_IDLE,						ACT_IDLE_RPG,					true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_RPG,				true },
	{ ACT_WALK,						ACT_WALK_RPG,					true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RPG,			true },
	{ ACT_RUN,						ACT_RUN_RPG,					true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RPG,				true },
	{ ACT_COVER_LOW,				ACT_COVER_LOW_RPG,				true },
};

IMPLEMENT_ACTTABLE(CWeaponMITCL);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponMITCL::CWeaponMITCL()
{
	m_bReloadsSingly = true;
	m_bInitialStateUpdate= false;

	m_fMinRange1 = m_fMinRange2 = 40*12;
	m_fMaxRange1 = m_fMaxRange2 = 500*12;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponMITCL::~CWeaponMITCL()
{

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMITCL::Precache( void )
{
	BaseClass::Precache();

	PrecacheScriptSound( "Missile.Ignite" );
	PrecacheScriptSound( "Missile.Accelerate" );

	UTIL_PrecacheOther( "rpg_missile" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CWeaponMITCL::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_SMG1:
		{
			if ( m_hMissile != NULL )
				return;

			Vector	muzzlePoint;
			QAngle	vecAngles;

			muzzlePoint = GetOwner()->Weapon_ShootPosition();

			CAI_BaseNPC *npc = pOperator->MyNPCPointer();
			ASSERT( npc != NULL );

			Vector vecShootDir = npc->GetActualShootTrajectory( muzzlePoint );

			// look for a better launch location
			Vector altLaunchPoint;
			if (GetAttachment( "missile", altLaunchPoint ))
			{
				// check to see if it's relativly free
				trace_t tr;
				AI_TraceHull( altLaunchPoint, altLaunchPoint + vecShootDir * (10.0f*12.0f), Vector( -24, -24, -24 ), Vector( 24, 24, 24 ), MASK_NPCSOLID, NULL, &tr );

				if( tr.fraction == 1.0)
				{
					muzzlePoint = altLaunchPoint;
				}
			}

			VectorAngles( vecShootDir, vecAngles );

			m_hMissile = CMissile::Create( muzzlePoint, vecAngles, GetOwner()->edict() );		
			m_hMissile->m_hOwner = this;

			// NPCs always get a grace period
			m_hMissile->SetGracePeriod( 0.5 );

			pOperator->DoMuzzleFlash();

			WeaponSound( SINGLE_NPC );
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
bool CWeaponMITCL::HasAnyAmmo( void )
{
	if ( m_hMissile != NULL )
		return true;

	return BaseClass::HasAnyAmmo();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponMITCL::WeaponShouldBeLowered( void )
{
	// Lower us if we're out of ammo
	if ( !HasAnyAmmo() )
		return true;
	
	return BaseClass::WeaponShouldBeLowered();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMITCL::PrimaryAttack( void )
{
	// Can't have an active missile out
//	if ( m_hMissile != NULL )
//		return;

	// Can't be reloading
	if ( GetActivity() == ACT_VM_RELOAD )
		return;

	Vector vecOrigin;
	Vector vecForward;

	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	Vector	vForward, vRight, vUp;

	pOwner->EyeVectors( &vForward, &vRight, &vUp );

	Vector	muzzlePoint = pOwner->Weapon_ShootPosition() + vForward * 12.0f + vRight * 6.0f + vUp * -3.0f;

	QAngle vecAngles;
	VectorAngles( vForward, vecAngles );
	m_hMissile = CMissile::Create( muzzlePoint, vecAngles, GetOwner()->edict() );

	m_hMissile->m_hOwner = this;

	// If the shot is clear to the player, give the missile a grace period
	trace_t	tr;
	Vector vecEye = pOwner->EyePosition();
	UTIL_TraceLine( vecEye, vecEye + vForward * 128, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction == 1.0 )
	{
		m_hMissile->SetGracePeriod( 0.3 );
	}

	DecrementAmmo( GetOwner() );

	// Register a muzzleflash for the AI
	pOwner->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	WeaponSound( SINGLE );

	pOwner->RumbleEffect( RUMBLE_SHOTGUN_SINGLE, 0, RUMBLE_FLAG_RESTART );

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pOwner, true, GetClassname() );

	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 1000, 0.2, GetOwner(), SOUNDENT_CHANNEL_WEAPON );

	// Check to see if we should trigger any RPG firing triggers
	int iCount = g_hWeaponFireTriggers.Count();
	for ( int i = 0; i < iCount; i++ )
	{
		if ( g_hWeaponFireTriggers[i]->IsTouching( pOwner ) )
		{
			if ( FClassnameIs( g_hWeaponFireTriggers[i], "trigger_rpgfire" ) )
			{
				g_hWeaponFireTriggers[i]->ActivateMultiTrigger( pOwner );
			}
		}
	}

	if( hl2_episodic.GetBool() )
	{
		CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
		int nAIs = g_AI_Manager.NumAIs();

		string_t iszStriderClassname = AllocPooledString( "npc_strider" );

		for ( int i = 0; i < nAIs; i++ )
		{
			if( ppAIs[ i ]->m_iClassname == iszStriderClassname )
			{
				ppAIs[ i ]->DispatchInteraction( g_interactionPlayerLaunchedRPG, NULL, m_hMissile );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOwner - 
//-----------------------------------------------------------------------------
void CWeaponMITCL::DecrementAmmo( CBaseCombatCharacter *pOwner )
{
	// Take away our primary ammo type
	pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );
}

//-----------------------------------------------------------------------------
// Purpose: Override this if we're guiding a missile currently
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponMITCL::Lower( void )
{
	if ( m_hMissile != NULL )
		return false;

	return BaseClass::Lower();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMITCL::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	//If we're pulling the weapon out for the first time, wait to draw the laser
	if ( ( m_bInitialStateUpdate ) && ( GetActivity() != ACT_VM_DRAW ) )
	{
		m_bInitialStateUpdate = false;
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponMITCL::Deploy( void )
{
	m_bInitialStateUpdate = true;

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponMITCL::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	//Can't have an active missile out
	if ( m_hMissile != NULL )
		return false;

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMITCL::Drop( const Vector &vecVelocity )
{

	BaseClass::Drop( vecVelocity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMITCL::NotifyRocketDied( void )
{
	m_hMissile = NULL;

	Reload();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponMITCL::Reload( void )
{
	CBaseCombatCharacter *pOwner = GetOwner();
	
	if ( pOwner == NULL )
		return false;

	if ( pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
		return false;

	bool fRet;
	fRet = DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
	if ( fRet )
	{
		WeaponSound( RELOAD );
	}

	return fRet;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CWeaponMITCL::WeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions )
{
	bool bResult = BaseClass::WeaponLOSCondition( ownerPos, targetPos, bSetConditions );

	if( bResult )
	{
		CAI_BaseNPC* npcOwner = GetOwner()->MyNPCPointer();

		if( npcOwner )
		{
			trace_t tr;

			Vector vecRelativeShootPosition;
			VectorSubtract( npcOwner->Weapon_ShootPosition(), npcOwner->GetAbsOrigin(), vecRelativeShootPosition );
			Vector vecMuzzle = ownerPos + vecRelativeShootPosition;
			Vector vecShootDir = npcOwner->GetActualShootTrajectory( vecMuzzle );

			// Make sure I have a good 10 feet of wide clearance in front, or I'll blow my teeth out.
			AI_TraceHull( vecMuzzle, vecMuzzle + vecShootDir * (10.0f*12.0f), Vector( -24, -24, -24 ), Vector( 24, 24, 24 ), MASK_NPCSOLID, NULL, &tr );

			if( tr.fraction != 1.0f )
				bResult = false;
		}
	}

	return bResult;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flDot - 
//			flDist - 
// Output : int
//-----------------------------------------------------------------------------
int CWeaponMITCL::WeaponRangeAttack1Condition( float flDot, float flDist )
{
	if ( m_hMissile != NULL )
		return 0;

	// Ignore vertical distance when doing our RPG distance calculations
	CAI_BaseNPC *pNPC = GetOwner()->MyNPCPointer();
	if ( pNPC )
	{
		CBaseEntity *pEnemy = pNPC->GetEnemy();
		Vector vecToTarget = (pEnemy->GetAbsOrigin() - pNPC->GetAbsOrigin());
		vecToTarget.z = 0;
		flDist = vecToTarget.Length();
	}

	if ( flDist < min( m_fMinRange1, m_fMinRange2 ) )
		return COND_TOO_CLOSE_TO_ATTACK;

	if ( m_flNextPrimaryAttack > gpGlobals->curtime )
		return 0;

	// See if there's anyone in the way!
	CAI_BaseNPC *pOwner = GetOwner()->MyNPCPointer();
	ASSERT( pOwner != NULL );

	if( pOwner )
	{
		// Make sure I don't shoot the world!
		trace_t tr;

		Vector vecMuzzle = pOwner->Weapon_ShootPosition();
		Vector vecShootDir = pOwner->GetActualShootTrajectory( vecMuzzle );

		// Make sure I have a good 10 feet of wide clearance in front, or I'll blow my teeth out.
		AI_TraceHull( vecMuzzle, vecMuzzle + vecShootDir * (10.0f*12.0f), Vector( -24, -24, -24 ), Vector( 24, 24, 24 ), MASK_NPCSOLID, NULL, &tr );

		if( tr.fraction != 1.0 )
		{
			return COND_WEAPON_SIGHT_OCCLUDED;
		}
	}

	return COND_CAN_RANGE_ATTACK1;
}

