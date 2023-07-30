//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: GREN-AYYYY-DUH - Shared Throwable functions
//
// TODO; Cooking (ACT_VM_PULLPIN)
//=============================================================================//

#include "cbase.h"
#include "basehlcombatweapon_shared.h"
#include "basehlthrowable.h"
#include "NPCevent.h"
#include "basecombatcharacter.h"
#include "AI_BaseNPC.h"
#include "player.h"
#include "game.h"
#include "gamerules.h"
#include "engine/IEngineSound.h"
#include "items.h"
#include "in_buttons.h"
#include "soundent.h"
#include "gamestats.h"
#include "IEffects.h"
#include "vstdlib/random.h"
#include "movevars_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define GRENADE_PAUSED_NO			0
#define GRENADE_PAUSED_PRIMARY		1
#define GRENADE_PAUSED_SECONDARY	2

#define GRENADE_RADIUS	4.0f // inches

IMPLEMENT_SERVERCLASS_ST( CHLThrowable, DT_HLThrowable )
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CHLThrowable )

	DEFINE_FIELD( m_bRedraw, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_AttackPaused, FIELD_INTEGER ),
	DEFINE_FIELD( m_bDrawbackFinished, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flCookedFuseTime, FIELD_TIME ),
	DEFINE_FIELD( m_flNextThrowCheck, FIELD_TIME ),
	DEFINE_FIELD( m_vecTossVelocity, FIELD_VECTOR ),

END_DATADESC()

CHLThrowable::CHLThrowable( )
{
	m_vecTossVelocity.Init();

	m_fMinRange1	= 100;
	m_fMaxRange1	= 600;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHLThrowable::Deploy( void )
{
	m_bRedraw = false;
	m_bDrawbackFinished = false;
	m_flCookedFuseTime = 0;
	
	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHLThrowable::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	m_bRedraw = false;
	m_bDrawbackFinished = false;
	m_flCookedFuseTime = 0;

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CHLThrowable::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	bool fThrewGrenade = false;
	int iNextThrowDelay = g_pGameRules->IsSkillLevel( SKILL_HARD ) ? random->RandomInt( (GetMinAttackDelay() * 0.8), (GetMaxAttackDelay() * 0.8) ) : random->RandomInt( GetMinAttackDelay(), GetMaxAttackDelay() );
	switch( pEvent->event )
	{
		case EVENT_WEAPON_SEQUENCE_FINISHED:
			m_bDrawbackFinished = true;
			m_flCookedFuseTime = GetFuseTime();	//Start counting
			break;

		case EVENT_WEAPON_THROW:
			ThrowGrenade( pOperator );
			if ( pOperator->IsPlayer() )
				DecrementAmmo( pPlayer );

			m_flNextThrowCheck = gpGlobals->curtime + iNextThrowDelay;
			fThrewGrenade = true;
			break;

		case EVENT_WEAPON_THROW2:
			RollGrenade( pOperator );
			if ( pOperator->IsPlayer() )
				DecrementAmmo( pPlayer );

			m_flNextThrowCheck = gpGlobals->curtime + iNextThrowDelay;
			fThrewGrenade = true;
			break;

		case EVENT_WEAPON_THROW3:
			LobGrenade( pOperator );
			if ( pOperator->IsPlayer() )
				DecrementAmmo( pPlayer );
			
			m_flNextThrowCheck = gpGlobals->curtime + iNextThrowDelay;
			fThrewGrenade = true;
			break;

		default:
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}

#define RETHROW_DELAY	0.5
	if( fThrewGrenade )
	{
		m_flNextPrimaryAttack	= gpGlobals->curtime + RETHROW_DELAY;
		m_flNextSecondaryAttack	= gpGlobals->curtime + RETHROW_DELAY;
		m_flTimeWeaponIdle = FLT_MAX; //NOTE: This is set once the animation has finished up!
		m_flCookedFuseTime = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override to check throw
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CHLThrowable::WeaponRangeAttack1Condition( float flDot, float flDist )
{
	// If things haven't changed too much since last time
	// just return that previously calculated value
	if (gpGlobals->curtime < m_flNextThrowCheck )
		return COND_NONE;

	if ( flDist < m_fMinRange1)
	{
		m_flNextThrowCheck = gpGlobals->curtime + 0.5; // half second.
		return COND_TOO_CLOSE_TO_ATTACK;
	}
	else if (flDist > m_fMaxRange1)
	{
		m_flNextThrowCheck = gpGlobals->curtime + 1; // one full second.
		return COND_TOO_FAR_TO_ATTACK;
	}
	else if (flDot < 0.5)
	{
		return COND_NOT_FACING_ATTACK;
	}

	// If moving, can't throw.
	if ( m_flGroundSpeed > 1 )
	{
		m_flNextThrowCheck = gpGlobals->curtime + 0.5; // half second.
		return COND_NONE;
	}
	else 
	{
		// Ok we should check again as some time has passed
		// This function is only used by NPC's so we can cast to a Base Monster
		CAI_BaseNPC *pNPC	= GetOwner()->MyNPCPointer();
		CBaseEntity *pEnemy = pNPC->GetEnemy();

		if (!pEnemy)
			return COND_NONE;

		// Get Enemy Position 
		Vector vecEnemyLKP = pNPC->GetEnemyLKP();
		Vector vecTarget = vecEnemyLKP + pEnemy->GetAbsVelocity();

		// Are any friendlies near the intended grenade impact area?
		CBaseEntity *pTarget = NULL;
		while ( ( pTarget = gEntList.FindEntityInSphere( pTarget, vecTarget, m_fMinRange1 ) ) != NULL )
		{
			if ( pNPC->IRelationType( pTarget ) == D_LI )
			{
				// crap, I might blow my own guy up. Don't throw a grenade and don't check again for a while.
				m_flNextThrowCheck = gpGlobals->curtime + 1; // one full second.
				return COND_WEAPON_BLOCKED_BY_FRIEND;
			}
		}

		// Get Toss Vector
		Vector			throwStart  = pNPC->Weapon_ShootPosition();

		Vector			vecMins = -Vector(2,2,2);
		Vector			vecMaxs = Vector(2,2,4);
//		float			fGravity = sv_gravity.GetFloat(); 
		Vector			vecToss = VecCheckToss( this, throwStart, vecTarget, -1, 1.0, true, &vecMins, &vecMaxs );
#if 0
		float			throwDist	= (throwStart - vecTarget).Length();
		float			throwLimit	= pNPC->ThrowLimit(throwStart, vecTarget, fGravity, 35, WorldAlignMins(), WorldAlignMaxs(), pEnemy, &vecToss, &pTarget);
#endif

		// If I can make the throw (or most of the throw)
		if (vecToss != vec3_origin)	//!throwLimit || (throwLimit != throwDist && throwLimit > 0.8*throwDist)
		{
			m_vecTossVelocity = vecToss;
			m_flNextThrowCheck = gpGlobals->curtime + 0.33; // 1/3 second.
			return COND_CAN_RANGE_ATTACK1;
		}

		m_flNextThrowCheck = gpGlobals->curtime + 0.5; // 1/2 second.
		return COND_NONE;
	}

	// don't check again for a while.
	m_flNextThrowCheck = gpGlobals->curtime + 0.5; // 1/2 second.

	return COND_NONE;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CHLThrowable::WeaponLOSCondition(const Vector &ownerPos, const Vector &targetPos, bool bSetConditions)
{
	// <<TODO>> should test if can throw from present location here...
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHLThrowable::Reload( void )
{
	if ( !HasPrimaryAmmo() )
		return false;

	if ( ( m_bRedraw ) && ( m_flNextPrimaryAttack <= gpGlobals->curtime ) && ( m_flNextSecondaryAttack <= gpGlobals->curtime ) )
	{
		//Redraw the weapon
		SendWeaponAnim( ACT_VM_DRAW );

		//Update our times
		m_flNextPrimaryAttack	= gpGlobals->curtime + SequenceDuration();
		m_flNextSecondaryAttack	= gpGlobals->curtime + SequenceDuration();
		m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();
		m_flCookedFuseTime = 0;

		//Mark this as done
		m_bRedraw = false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLThrowable::SecondaryAttack( void )
{
	if ( m_bRedraw )
		return;

	if ( !HasPrimaryAmmo() )
		return;

	CBaseCombatCharacter *pOwner  = GetOwner();

	if ( pOwner == NULL )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( pOwner );
	
	if ( pPlayer == NULL )
		return;

	// Note that this is a secondary attack and prepare the grenade attack to pause.
	m_AttackPaused = GRENADE_PAUSED_SECONDARY;
	SendWeaponAnim( ACT_VM_PULLBACK_LOW );

	// Don't let weapon idle interfere in the middle of a throw!
	m_flTimeWeaponIdle = FLT_MAX;
	m_flNextSecondaryAttack	= FLT_MAX;

	// If I'm now out of ammo, switch away
	if ( !HasPrimaryAmmo() )
	{
		pPlayer->SwitchToNextBestWeapon( this );
	}

	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLThrowable::PrimaryAttack( void )
{
	if ( m_bRedraw )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );;

	if ( !pPlayer )
		return;

	// Note that this is a primary attack and prepare the grenade attack to pause.
	m_AttackPaused = GRENADE_PAUSED_PRIMARY;
	SendWeaponAnim( ACT_VM_PULLBACK_HIGH );
	
	// Put both of these off indefinitely. We do not know how long
	// the player will hold the grenade.
	m_flTimeWeaponIdle = FLT_MAX;
	m_flNextPrimaryAttack = FLT_MAX;

	// If I'm now out of ammo, switch away
	if ( !HasPrimaryAmmo() )
	{
		pPlayer->SwitchToNextBestWeapon( this );
	}

	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOwner - 
//-----------------------------------------------------------------------------
void CHLThrowable::DecrementAmmo( CBaseCombatCharacter *pOwner )
{
	pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLThrowable::ItemPostFrame( void )
{
	if( m_bDrawbackFinished )
	{
		CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
		if (pOwner)
		{
			switch( m_AttackPaused )
			{
			case GRENADE_PAUSED_PRIMARY:
				if( !(pOwner->m_nButtons & IN_ATTACK) )
				{
					SendWeaponAnim( ACT_VM_THROW );
					m_bDrawbackFinished = false;
				}
				break;

			case GRENADE_PAUSED_SECONDARY:
				if( !(pOwner->m_nButtons & IN_ATTACK2) )
				{
					//See if we're ducking
					if ( pOwner->m_nButtons & IN_DUCK )
					{
						//Send the weapon animation
						SendWeaponAnim( ACT_VM_SECONDARYATTACK );
					}
					else
					{
						//Send the weapon animation
						SendWeaponAnim( ACT_VM_HAULBACK );
					}

					m_bDrawbackFinished = false;
				}
				break;

			default:
				break;
			}
		}
	}

	BaseClass::ItemPostFrame();

	if ( m_bRedraw )
	{
		if ( IsViewModelSequenceFinished() )
		{
			Reload();
		}
	}
}

// check a throw from vecSrc.  If not valid, move the position back along the line to vecEye
void CHLThrowable::CheckThrowPosition( CBasePlayer *pPlayer, const Vector &vecEye, Vector &vecSrc )
{
	trace_t tr;

	UTIL_TraceHull( vecEye, vecSrc, -Vector(GRENADE_RADIUS+2,GRENADE_RADIUS+2,GRENADE_RADIUS+2), Vector(GRENADE_RADIUS+2,GRENADE_RADIUS+2,GRENADE_RADIUS+2), 
		pPlayer->PhysicsSolidMaskForEntity(), pPlayer, pPlayer->GetCollisionGroup(), &tr );
	
	if ( tr.DidHit() )
	{
		vecSrc = tr.endpos;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Throw a grenade
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CHLThrowable::ThrowGrenade( CBaseCombatCharacter *pOwner )
{
	m_bRedraw = true;

	WeaponSound( SINGLE );

	m_iPrimaryAttacks++;
}

//-----------------------------------------------------------------------------
// Purpose: Underhand lob a grenade
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CHLThrowable::LobGrenade( CBaseCombatCharacter *pOwner )
{
	WeaponSound( WPN_DOUBLE );

	m_bRedraw = true;

	m_iPrimaryAttacks++;
}

//-----------------------------------------------------------------------------
// Purpose: Roll a grenade
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CHLThrowable::RollGrenade( CBaseCombatCharacter *pOwner )
{
	WeaponSound( SPECIAL1 );

	m_bRedraw = true;

	m_iPrimaryAttacks++;
}

