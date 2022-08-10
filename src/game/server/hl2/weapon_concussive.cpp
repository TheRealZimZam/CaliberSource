//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Concussive Grenade
//
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

#define CONCGRENADE_RADIUS	4.0f // inches

//-----------------------------------------------------------------------------
// Fragmentation grenades
//-----------------------------------------------------------------------------
class CWeaponConcussive: public CHLThrowable
{
public:
	DECLARE_CLASS( CWeaponConcussive, CHLThrowable );

	DECLARE_SERVERCLASS();

	CWeaponConcussive();

	void	Precache( void );
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	void	ThrowGrenade( CBasePlayer *pPlayer );
	void	RollGrenade( CBasePlayer *pPlayer );
	void	LobGrenade( CBasePlayer *pPlayer );

private:
	DECLARE_ACTTABLE();

	DECLARE_DATADESC();
};

acttable_t	CWeaponConcussive::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_THROW, true },
};

IMPLEMENT_ACTTABLE(CWeaponConcussive);

IMPLEMENT_SERVERCLASS_ST(CWeaponConcussive, DT_WeaponConcGrenade)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_concussive, CWeaponConcussive );
PRECACHE_WEAPON_REGISTER(weapon_concussive);

BEGIN_DATADESC( CWeaponConcussive )
END_DATADESC()

CWeaponConcussive::CWeaponConcussive( )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponConcussive::Precache( void )
{
	BaseClass::Precache();

	UTIL_PrecacheOther( "grenade_concussive" );

	PrecacheScriptSound( "WeaponStun.Throw" );
	PrecacheScriptSound( "WeaponStun.Roll" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CWeaponConcussive::ThrowGrenade( CBasePlayer *pPlayer )
{
	Vector	vecEye = pPlayer->EyePosition();
	Vector	vForward, vRight;

	pPlayer->EyeVectors( &vForward, &vRight, NULL );
	Vector vecSrc = vecEye + vForward * 18.0f + vRight * 8.0f;
	CheckThrowPosition( pPlayer, vecEye, vecSrc );
//	vForward[0] += 0.1f;
	vForward[2] += 0.1f;

	Vector vecThrow;
	pPlayer->GetVelocity( &vecThrow, NULL );
	vecThrow += vForward * 1200;
	CGrenadeConcussive *pConcussive = (CGrenadeConcussive*)Create( "grenade_concussive", vecSrc, vec3_angle, GetOwner() );
	pConcussive->SetAbsVelocity( vecThrow );
	pConcussive->SetThrower( GetOwner() );
	pConcussive->SetOwnerEntity( ((CBaseEntity*)GetOwner()) );

	m_bRedraw = true;

	WeaponSound( SINGLE );

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CWeaponConcussive::LobGrenade( CBasePlayer *pPlayer )
{
	Vector	vecEye = pPlayer->EyePosition();
	Vector	vForward, vRight;

	pPlayer->EyeVectors( &vForward, &vRight, NULL );
	Vector vecSrc = vecEye + vForward * 18.0f + vRight * 8.0f + Vector( 0, 0, -8 );
	CheckThrowPosition( pPlayer, vecEye, vecSrc );
	
	Vector vecThrow;
	pPlayer->GetVelocity( &vecThrow, NULL );
	vecThrow += vForward * 350 + Vector( 0, 0, 50 );

	CGrenadeConcussive *pConcussive = (CGrenadeConcussive*)Create( "grenade_concussive", vecSrc, vec3_angle, GetOwner() );
	pConcussive->SetAbsVelocity( vecThrow );
	pConcussive->SetThrower( GetOwner() );
	pConcussive->SetOwnerEntity( ((CBaseEntity*)GetOwner()) );

	WeaponSound( WPN_DOUBLE );

	m_bRedraw = true;

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CWeaponConcussive::RollGrenade( CBasePlayer *pPlayer )
{
	// BUGBUG: Hardcoded grenade width of 4 - better not change the model :)
	Vector vecSrc;
	pPlayer->CollisionProp()->NormalizedToWorldSpace( Vector( 0.5f, 0.5f, 0.0f ), &vecSrc );
	vecSrc.z += CONCGRENADE_RADIUS;

	Vector vecFacing = pPlayer->BodyDirection2D( );
	// no up/down direction
	vecFacing.z = 0;
	VectorNormalize( vecFacing );
	trace_t tr;
	UTIL_TraceLine( vecSrc, vecSrc - Vector(0,0,16), MASK_PLAYERSOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction != 1.0 )
	{
		// compute forward vec parallel to floor plane and roll grenade along that
		Vector tangent;
		CrossProduct( vecFacing, tr.plane.normal, tangent );
		CrossProduct( tr.plane.normal, tangent, vecFacing );
	}
	vecSrc += (vecFacing * 18.0);
	CheckThrowPosition( pPlayer, pPlayer->WorldSpaceCenter(), vecSrc );

	Vector vecThrow;
	pPlayer->GetVelocity( &vecThrow, NULL );
	vecThrow += vecFacing * 700;
	// put it on its side
	QAngle orientation(0,pPlayer->GetLocalAngles().y,-90);
	// roll it
	AngularImpulse rotSpeed(0,0,720);

	CGrenadeConcussive *pConcussive = (CGrenadeConcussive*)Create( "grenade_concussive", vecSrc, vec3_angle, GetOwner() );
	pConcussive->SetAbsVelocity( vecThrow );
	pConcussive->SetLocalAngularVelocity( rotSpeed );
	pConcussive->SetThrower( GetOwner() );
	pConcussive->SetOwnerEntity( ((CBaseEntity*)GetOwner()) );

	WeaponSound( SPECIAL1 );

	m_bRedraw = true;

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
}

