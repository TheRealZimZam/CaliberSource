//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: GREN-AYYYY-DUH
//
//			Primary attack: throw
//			Secondary attack: toss/roll
// TODO; Cooking
//=============================================================================//

#include "cbase.h"
#include "basehlthrowable.h"
#include "player.h"
#include "gamerules.h"
#include "grenade_frag.h"
#include "npcevent.h"
#include "engine/IEngineSound.h"
#include "items.h"
#include "in_buttons.h"
#include "soundent.h"
#include "gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	FRAGGRENADE_TIMER	sk_fraggrenade_timer.GetFloat()
ConVar sk_fraggrenade_timer( "sk_fraggrenade_timer","3.0");
#define	FRAGGRENADE_TIMER_MIN	sk_fraggrenade_timer_min.GetFloat()
ConVar sk_fraggrenade_timer_min( "sk_fraggrenade_timer_min","0.6");

#define FRAGGRENADE_RADIUS	4.0f // inches

//-----------------------------------------------------------------------------
// Fragmentation grenades
//-----------------------------------------------------------------------------
class CWeaponFrag: public CHLThrowable
{
public:
	DECLARE_CLASS( CWeaponFrag, CHLThrowable );

	DECLARE_SERVERCLASS();

	CWeaponFrag();

	void	Precache( void );
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	float	GetFuseTime( void ) { return FRAGGRENADE_TIMER; }
	
	void	ThrowGrenade( CBaseCombatCharacter *pOwner );
	void	RollGrenade( CBaseCombatCharacter *pOwner );
	void	LobGrenade( CBaseCombatCharacter *pOwner );

	DECLARE_ACTTABLE();
private:
	DECLARE_DATADESC();
};

acttable_t	CWeaponFrag::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_THROW, true },
};

IMPLEMENT_ACTTABLE(CWeaponFrag);

IMPLEMENT_SERVERCLASS_ST(CWeaponFrag, DT_WeaponFrag)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_frag, CWeaponFrag );
PRECACHE_WEAPON_REGISTER(weapon_frag);

BEGIN_DATADESC( CWeaponFrag )
END_DATADESC()

CWeaponFrag::CWeaponFrag( )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFrag::Precache( void )
{
	BaseClass::Precache();

	UTIL_PrecacheOther( "npc_grenade_frag" );

	PrecacheScriptSound( "WeaponFrag.Throw" );
	PrecacheScriptSound( "WeaponFrag.Roll" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOwner - 
//-----------------------------------------------------------------------------
void CWeaponFrag::ThrowGrenade( CBaseCombatCharacter *pOwner )
{
	Vector	vecEye = pOwner->EyePosition();
	Vector	vForward, vRight;

//	pOwner->EyeVectors( &vForward, &vRight, NULL );
	AngleVectors( EyeAngles(), &vForward, &vRight, NULL );
	Vector vecSrc = vecEye + vForward * 18.0f + vRight * 8.0f;
	if ( pOwner->IsPlayer() )
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		CheckThrowPosition( pPlayer, vecEye, vecSrc );
	}

	Vector vecThrow, up;
	GetVectors( &vForward, NULL, &up );
	vecThrow = vForward * 1000 + up * 20;

	m_flFuseTime = FRAGGRENADE_TIMER_MIN + (m_flCookTime - FRAGGRENADE_TIMER_MIN);
	Fraggrenade_Create( vecSrc, vec3_angle, vecThrow, AngularImpulse(600,random->RandomInt(-1200,1200),0), pOwner, m_flFuseTime, false );

	m_bRedraw = true;

	WeaponSound( SINGLE );

	m_iPrimaryAttacks++;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOwner - 
//-----------------------------------------------------------------------------
void CWeaponFrag::LobGrenade( CBaseCombatCharacter *pOwner )
{
	Vector	vecEye = pOwner->EyePosition();
	Vector	vForward, vRight;

//	pOwner->EyeVectors( &vForward, &vRight, NULL );
	AngleVectors( EyeAngles(), &vForward, &vRight, NULL );
	Vector vecSrc = vecEye + vForward * 18.0f + vRight * 8.0f + Vector( 0, 0, -8 );

	if ( pOwner->IsPlayer() )
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		CheckThrowPosition( pPlayer, vecEye, vecSrc );
	}

	Vector vecThrow, up;

	GetVectors( &vForward, NULL, &up );
	vecThrow = vForward * 400 + up * 50;

	m_flFuseTime = FRAGGRENADE_TIMER_MIN + (m_flCookTime - FRAGGRENADE_TIMER_MIN);
	Fraggrenade_Create( vecSrc, vec3_angle, vecThrow, AngularImpulse(200,random->RandomInt(-600,600),0), pOwner, m_flFuseTime, false );

	WeaponSound( WPN_DOUBLE );

	m_bRedraw = true;

	m_iPrimaryAttacks++;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOwner - 
//-----------------------------------------------------------------------------
void CWeaponFrag::RollGrenade( CBaseCombatCharacter *pOwner )
{
	// BUGBUG: Hardcoded grenade width of 4 - better not change the model :)
	Vector vecSrc;
	pOwner->CollisionProp()->NormalizedToWorldSpace( Vector( 0.5f, 0.5f, 0.0f ), &vecSrc );
	vecSrc.z += FRAGGRENADE_RADIUS;

	Vector vecFacing = pOwner->BodyDirection2D( );
	// no up/down direction
	vecFacing.z = 0;
	VectorNormalize( vecFacing );
	trace_t tr;
	UTIL_TraceLine( vecSrc, vecSrc - Vector(0,0,16), MASK_PLAYERSOLID, pOwner, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction != 1.0 )
	{
		// compute forward vec parallel to floor plane and roll grenade along that
		Vector tangent;
		CrossProduct( vecFacing, tr.plane.normal, tangent );
		CrossProduct( tr.plane.normal, tangent, vecFacing );
	}
	vecSrc += (vecFacing * 18.0);
	if ( pOwner->IsPlayer() )
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		CheckThrowPosition( pPlayer, pPlayer->WorldSpaceCenter(), vecSrc );
	}

	Vector vecThrow;
	pOwner->GetVelocity( &vecThrow, NULL );
	vecThrow += vecFacing * 600;

	// put it on its side
	QAngle orientation(0,pOwner->GetLocalAngles().y,-90);
	// roll it
	AngularImpulse rotSpeed(0,0,720);
	Fraggrenade_Create( vecSrc, orientation, vecThrow, rotSpeed, pOwner, GetFuseTime(), false );

	WeaponSound( SPECIAL1 );

	m_bRedraw = true;

	m_iPrimaryAttacks++;
}

