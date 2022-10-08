//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:	Knife - Stab, stab, stab
//
// TODO's; Better system for variants, optimize
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "basehlcombatweapon.h"
#include "player.h"
#include "gamerules.h"
#include "ammodef.h"
#include "mathlib/mathlib.h"
#include "in_buttons.h"
#include "soundent.h"
#include "basebludgeonweapon.h"
#include "vstdlib/random.h"
#include "npcevent.h"
#include "ai_basenpc.h"
#include "weapon_stab.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar    sk_plr_dmg_stab		( "sk_plr_dmg_stab","0");
ConVar    sk_npc_dmg_stab		( "sk_npc_dmg_stab","0");

//-----------------------------------------------------------------------------
// CWeaponStab
//-----------------------------------------------------------------------------

IMPLEMENT_SERVERCLASS_ST(CWeaponStab, DT_WeaponStab)
END_SEND_TABLE()

#ifndef HL2MP
LINK_ENTITY_TO_CLASS( weapon_stab, CWeaponStab );
PRECACHE_WEAPON_REGISTER( weapon_stab );
#endif

BEGIN_DATADESC( CWeaponStab )
END_DATADESC()

acttable_t CWeaponStab::m_acttable[] = 
{
	{ ACT_MELEE_ATTACK1,	ACT_MELEE_ATTACK_STAB,	true },
	{ ACT_IDLE,				ACT_IDLE_MELEE,			false },
	{ ACT_IDLE_ANGRY,		ACT_IDLE_ANGRY_MELEE,	false },

	{ ACT_GESTURE_MELEE_ATTACK1,	ACT_GESTURE_MELEE_ATTACK_STAB,	true },
};

IMPLEMENT_ACTTABLE(CWeaponStab);

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CWeaponStab::CWeaponStab( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Get the damage amount for the animation we're doing
// Input  : hitActivity - currently played activity
// Output : Damage amount
//-----------------------------------------------------------------------------
float CWeaponStab::GetDamageForActivity( Activity hitActivity )
{
	if ( ( GetOwner() != NULL ) && ( GetOwner()->IsPlayer() ) )
		return sk_plr_dmg_stab.GetFloat();

	return sk_npc_dmg_stab.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Add in a view kick for this weapon
//-----------------------------------------------------------------------------
void CWeaponStab::AddViewKick( void )
{
	CBasePlayer *pPlayer  = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	QAngle punchAng;

	punchAng.x = random->RandomFloat( 1.0f, 2.0f );
	punchAng.y = random->RandomFloat( -2.0f, -1.0f );
	punchAng.z = 0.0f;
	
	pPlayer->ViewPunch( punchAng ); 
}


//-----------------------------------------------------------------------------
// Attempt to lead the target (needed because citizens can't hit manhacks with the crowbar!)
//-----------------------------------------------------------------------------
ConVar sk_stab_lead_time( "sk_stab_lead_time", "0.75" );

int CWeaponStab::WeaponMeleeAttack1Condition( float flDot, float flDist )
{
	// Attempt to lead the target (needed because citizens can't hit manhacks with the crowbar!)
	CAI_BaseNPC *pNPC	= GetOwner()->MyNPCPointer();
	CBaseEntity *pEnemy = pNPC->GetEnemy();
	if (!pEnemy)
		return COND_NONE;

	Vector vecVelocity;
	vecVelocity = pEnemy->GetSmoothedVelocity( );

	// Project where the enemy will be in a little while
	float dt = sk_stab_lead_time.GetFloat();
	dt += random->RandomFloat( -0.3f, 0.2f );
	if ( dt < 0.0f )
		dt = 0.0f;

	Vector vecExtrapolatedPos;
	VectorMA( pEnemy->WorldSpaceCenter(), dt, vecVelocity, vecExtrapolatedPos );

	Vector vecDelta;
	VectorSubtract( vecExtrapolatedPos, pNPC->WorldSpaceCenter(), vecDelta );

	if ( fabs( vecDelta.z ) > 70 )
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	Vector vecForward = pNPC->BodyDirection2D( );
	vecDelta.z = 0.0f;
	float flExtrapolatedDist = Vector2DNormalize( vecDelta.AsVector2D() );
	if ((flDist > 64) && (flExtrapolatedDist > 64))
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
void CWeaponStab::HandleAnimEventMeleeHit( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
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
		Vector(-16,-16,-16), Vector(36,36,36), sk_npc_dmg_stab.GetFloat(), DMG_CLUB, 0.75 );
	
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
void CWeaponStab::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
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


//-----------------------------------------------------------------------------
// Bottle
//-----------------------------------------------------------------------------
//IMPLEMENT_SERVERCLASS_ST(CWeaponBottle, DT_WeaponBottle)
//END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_bottle, CWeaponBottle );
PRECACHE_WEAPON_REGISTER(weapon_bottle);

CWeaponBottle::CWeaponBottle( )
{
}

//-----------------------------------------------------------------------------
// Knife
//-----------------------------------------------------------------------------
//IMPLEMENT_SERVERCLASS_ST(CWeaponKnife, DT_WeaponKnife)
//END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_knife, CWeaponKnife );
PRECACHE_WEAPON_REGISTER(weapon_knife);

CWeaponKnife::CWeaponKnife( )
{
}

//-----------------------------------------------------------------------------
// Rebar
//-----------------------------------------------------------------------------
//IMPLEMENT_SERVERCLASS_ST(CWeaponRebar, DT_WeaponRebar)
//END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_rebar, CWeaponRebar );
PRECACHE_WEAPON_REGISTER(weapon_rebar);

CWeaponRebar::CWeaponRebar( )
{
}

//-----------------------------------------------------------------------------
// Shank
//-----------------------------------------------------------------------------
//IMPLEMENT_SERVERCLASS_ST(CWeaponShank, DT_WeaponShank)
//END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_shank, CWeaponShank );
PRECACHE_WEAPON_REGISTER(weapon_shank);

CWeaponShank::CWeaponShank( )
{
}

