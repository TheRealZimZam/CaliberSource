//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Bat - breaking eggs
//
// TODO's; Better system for variants, optimize
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "weapon_swing.h"
#include "baseplayer_shared.h"
#include "gamerules.h"
#include "ammodef.h"
#include "mathlib/mathlib.h"
#include "in_buttons.h"
#include "vstdlib/random.h"
#include "npcevent.h"
#ifndef CLIENT_DLL
#include "soundent.h"
#include "ai_basenpc.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar    sk_swing_plr_dmg		( "sk_swing_plr_dmg","0", FCVAR_REPLICATED );
ConVar    sk_swing_npc_dmg		( "sk_swing_npc_dmg","0", FCVAR_REPLICATED );

//-----------------------------------------------------------------------------
// CWeaponSwing
//-----------------------------------------------------------------------------

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSwing, DT_WeaponSwing )

BEGIN_NETWORK_TABLE( CWeaponSwing, DT_WeaponSwing )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponSwing )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_swing, CWeaponSwing );
PRECACHE_WEAPON_REGISTER( weapon_swing );

acttable_t CWeaponSwing::m_acttable[] = 
{
	{ ACT_MELEE_ATTACK1,	ACT_MELEE_ATTACK_SWING, true },
	{ ACT_IDLE,				ACT_IDLE_MELEE,			false },
	{ ACT_IDLE_ANGRY,		ACT_IDLE_ANGRY_MELEE,	false },

	{ ACT_GESTURE_MELEE_ATTACK1,	ACT_GESTURE_MELEE_ATTACK_SWING,	true },
};

IMPLEMENT_ACTTABLE(CWeaponSwing);

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CWeaponSwing::CWeaponSwing( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Get the damage amount for the animation we're doing
// Input  : hitActivity - currently played activity
// Output : Damage amount
//-----------------------------------------------------------------------------
float CWeaponSwing::GetDamageForActivity( Activity hitActivity )
{
	if ( ( GetOwner() != NULL ) && ( GetOwner()->IsPlayer() ) )
		return sk_swing_plr_dmg.GetFloat();

	return sk_swing_npc_dmg.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Add in a view kick for this weapon
//-----------------------------------------------------------------------------
void CWeaponSwing::AddViewKick( void )
{
	CBasePlayer *pPlayer  = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	QAngle punchAng;

	punchAng.x = random->RandomFloat( 2.0f, 3.0f );
	punchAng.y = random->RandomFloat( -3.0f, -2.0f );
	punchAng.z = 0.0f;
	
	pPlayer->ViewPunch( punchAng ); 
}


//-----------------------------------------------------------------------------
// Attempt to lead the target (needed because citizens can't hit manhacks with the crowbar!)
//-----------------------------------------------------------------------------
#ifndef CLIENT_DLL
ConVar sk_swing_lead_time( "sk_swing_lead_time", "0.9" );

int CWeaponSwing::WeaponMeleeAttack1Condition( float flDot, float flDist )
{
	// Attempt to lead the target (needed because citizens can't hit manhacks with the crowbar!)
	CAI_BaseNPC *pNPC	= GetOwner()->MyNPCPointer();
	CBaseEntity *pEnemy = pNPC->GetEnemy();
	if (!pEnemy)
		return COND_NONE;

	Vector vecVelocity;
	vecVelocity = pEnemy->GetSmoothedVelocity( );

	// Project where the enemy will be in a little while
	float dt = sk_swing_lead_time.GetFloat();
	dt += random->RandomFloat( -0.3f, 0.2f );
	if ( dt < 0.0f )
		dt = 0.0f;

	Vector vecExtrapolatedPos;
	VectorMA( pEnemy->WorldSpaceCenter(), dt, vecVelocity, vecExtrapolatedPos );

	Vector vecDelta;
	VectorSubtract( vecExtrapolatedPos, pNPC->WorldSpaceCenter(), vecDelta );

	// Check if hes under or ontop of me
	if ( fabs( vecDelta.z ) > (GetRange()+10) )
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	Vector vecForward = pNPC->BodyDirection2D( );
	vecDelta.z = 0.0f;
	float flExtrapolatedDist = Vector2DNormalize( vecDelta.AsVector2D() );
	if ((flDist > GetRange()) && (flExtrapolatedDist > GetRange()))
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	float flExtrapolatedDot = DotProduct2D( vecDelta.AsVector2D(), vecForward.AsVector2D() );
	if ((flDot < 0.7) && (flExtrapolatedDot < 0.7))
	{
		return COND_NOT_FACING_ATTACK;
	}

	return COND_CAN_MELEE_ATTACK1;
}


//-----------------------------------------------------------------------------
// Animation event handlers
//-----------------------------------------------------------------------------
void CWeaponSwing::HandleAnimEventMeleeHit( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	// Trace up or down based on where the enemy is...
	// But only if we're basically facing that direction
	Vector vecDirection;
	AngleVectors( GetAbsAngles(), &vecDirection );

	CBaseEntity *pEnemy = pOperator->MyNPCPointer() ? pOperator->MyNPCPointer()->GetEnemy() : NULL;
	if ( pEnemy )
	{
		Vector vecDelta;
		VectorSubtract( pEnemy->WorldSpaceCenter(), pOperator->Weapon_ShootPosition(), vecDelta );
		VectorNormalize( vecDelta );
		
		Vector2D vecDelta2D = vecDelta.AsVector2D();
		Vector2DNormalize( vecDelta2D );
		if ( DotProduct2D( vecDelta2D, vecDirection.AsVector2D() ) > 0.8f )
		{
			vecDirection = vecDelta;
		}
	}

	Vector vecEnd;
	VectorMA( pOperator->Weapon_ShootPosition(), 50, vecDirection, vecEnd );
	CBaseEntity *pHurt = pOperator->CheckTraceHullAttack( pOperator->Weapon_ShootPosition(), vecEnd, 
		Vector(-16,-16,-16), Vector(36,36,36), sk_swing_npc_dmg.GetFloat(), DMG_CLUB, 0.75 );
	
	// did I hit someone?
	if ( pHurt )
	{
		// play sound
		WeaponSound( MELEE_HIT );

		// Fake a trace impact, so the effects work out like a player's crowbaw
		trace_t traceHit;
		UTIL_TraceLine( pOperator->Weapon_ShootPosition(), pHurt->GetAbsOrigin(), MASK_SHOT_HULL, pOperator, COLLISION_GROUP_NONE, &traceHit );
		ImpactEffect( traceHit );
	}
	else
	{
		WeaponSound( MELEE_MISS );
	}
}


//-----------------------------------------------------------------------------
// Animation event
//-----------------------------------------------------------------------------
void CWeaponSwing::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
	case EVENT_WEAPON_MELEE_HIT:
		HandleAnimEventMeleeHit( pEvent, pOperator );
		break;

	default:
		BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
		break;
	}
}
#endif

#if 0
//-----------------------------------------------------------------------------
// Bat
//-----------------------------------------------------------------------------
//IMPLEMENT_SERVERCLASS_ST(CWeaponBat, DT_WeaponBat)
//END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_bat, CWeaponBat );
PRECACHE_WEAPON_REGISTER(weapon_bat);

CWeaponBat::CWeaponBat( )
{
}

//-----------------------------------------------------------------------------
// Crowbar
//-----------------------------------------------------------------------------
//IMPLEMENT_SERVERCLASS_ST(CWeaponCrowbar, DT_WeaponCrowbar)
//END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_crowbar, CWeaponCrowbar );
PRECACHE_WEAPON_REGISTER(weapon_crowbar);

CWeaponCrowbar::CWeaponCrowbar( )
{
}

//-----------------------------------------------------------------------------
// Hammer
//-----------------------------------------------------------------------------
//IMPLEMENT_SERVERCLASS_ST(CWeaponHammer, DT_WeaponHammer)
//END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_hammer, CWeaponHammer );
PRECACHE_WEAPON_REGISTER(weapon_hammer);

CWeaponHammer::CWeaponHammer( )
{
}

//-----------------------------------------------------------------------------
// Pipe
//-----------------------------------------------------------------------------
//IMPLEMENT_SERVERCLASS_ST(CWeaponPipe, DT_WeaponPipe)
//END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_pipe, CWeaponPipe );
PRECACHE_WEAPON_REGISTER(weapon_pipe);

CWeaponPipe::CWeaponPipe( )
{
}
#endif