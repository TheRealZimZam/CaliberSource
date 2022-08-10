//=========== (C) Copyright 2000 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: This is the molotov weapon
//
//=============================================================================

#include "cbase.h"
#include "basehlthrowable.h"
#include "player.h"
#include "gamerules.h"
#include "grenade_molotov.h"
#include "npcevent.h"
#include "engine/IEngineSound.h"
#include "items.h"
#include "in_buttons.h"
#include "soundent.h"
#include "gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Fire grenade
//-----------------------------------------------------------------------------
class CWeaponMolotov: public CHLThrowable
{
public:
	DECLARE_CLASS( CWeaponMolotov, CHLThrowable );

	DECLARE_SERVERCLASS();

	CWeaponMolotov();

	void	Precache( void );
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	float	GetFuseTime( void ) { return 0; }

	void	ThrowGrenade( CBaseCombatCharacter *pOwner );
	void	RollGrenade( CBaseCombatCharacter *pOwner );
	void	LobGrenade( CBaseCombatCharacter *pOwner );

private:
	DECLARE_ACTTABLE();

	DECLARE_DATADESC();
};

acttable_t	CWeaponMolotov::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_THROW, true },
};

IMPLEMENT_ACTTABLE(CWeaponMolotov);

IMPLEMENT_SERVERCLASS_ST(CWeaponMolotov, DT_WeaponMolotov)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_molotov, CWeaponMolotov );
PRECACHE_WEAPON_REGISTER(weapon_molotov);

BEGIN_DATADESC( CWeaponMolotov )
END_DATADESC()

CWeaponMolotov::CWeaponMolotov( )
{
}

//------------------------------------------------------------------------------
void CWeaponMolotov::Precache( void )
{
	BaseClass::Precache();

	UTIL_PrecacheOther( "grenade_molotov" );

	PrecacheScriptSound( "WeaponMolotov.Throw" );
	PrecacheScriptSound( "WeaponMolotov.Light" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOwner - 
//-----------------------------------------------------------------------------
void CWeaponMolotov::ThrowGrenade( CBaseCombatCharacter *pOwner )
{
	Vector vecSrc		= pOwner->WorldSpaceCenter();
	Vector vecFacing	= pOwner->BodyDirection3D( );
	vecSrc				= vecSrc + vecFacing * 18.0;
	// BUGBUG: is this some hack because it's not at the eye position????
	vecSrc.z		   += 24.0f;

	Vector vecAiming;
	GetVectors( &vecAiming, NULL, NULL );
	if ( pOwner->IsPlayer() )
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		vecAiming = pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
		vecAiming.z += 0.20; // Raise up so passes through reticle
	}

	CGrenadeMolotov *pMolotov = (CGrenadeMolotov*)Create( "grenade_molotov", vecSrc, vec3_angle, GetOwner() );
	pMolotov->SetAbsVelocity( vecAiming*800 );
	// Tumble through the air
	QAngle angVel( random->RandomFloat ( -100, -500 ), random->RandomFloat ( -100, -500 ), random->RandomFloat ( -100, -500 ) ); 
	pMolotov->SetLocalAngularVelocity( angVel );

	pMolotov->SetThrower( GetOwner() );
	pMolotov->SetOwnerEntity( ((CBaseEntity*)GetOwner()) );

	m_bRedraw = true;

	WeaponSound( SINGLE );

	m_iPrimaryAttacks++;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOwner - 
//-----------------------------------------------------------------------------
void CWeaponMolotov::LobGrenade( CBaseCombatCharacter *pOwner )
{
	Vector vecSrc		= pOwner->WorldSpaceCenter();
	Vector vecFacing	= pOwner->BodyDirection3D( );
	vecSrc				= vecSrc + vecFacing * 18.0;
	// BUGBUG: is this some hack because it's not at the eye position????
	vecSrc.z		   += 20.0f;

	Vector vecAiming;
	GetVectors( &vecAiming, NULL, NULL );
	if ( pOwner->IsPlayer() )
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		vecAiming = pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
		vecAiming.z += 0.20; // Raise up so passes through reticle
	}

	CGrenadeMolotov *pMolotov = (CGrenadeMolotov*)Create( "grenade_molotov", vecSrc, vec3_angle, GetOwner() );
	pMolotov->SetAbsVelocity( vecAiming*400 );
	// Tumble through the air
	QAngle angVel( random->RandomFloat ( -100, -500 ), random->RandomFloat ( -100, -500 ), random->RandomFloat ( -100, -500 ) ); 
	pMolotov->SetLocalAngularVelocity( angVel );

	pMolotov->SetThrower( GetOwner() );
	pMolotov->SetOwnerEntity( ((CBaseEntity*)GetOwner()) );

	m_bRedraw = true;

	WeaponSound( WPN_DOUBLE );

	m_iPrimaryAttacks++;
}

//-----------------------------------------------------------------------------
// Purpose: Molotov doesnt roll
// Input  : *pOwner - 
//-----------------------------------------------------------------------------
void CWeaponMolotov::RollGrenade( CBaseCombatCharacter *pOwner )
{
	LobGrenade( pOwner );
}
