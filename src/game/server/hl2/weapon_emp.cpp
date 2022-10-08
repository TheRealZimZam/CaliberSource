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
#include "grenade_concussive.h"
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
class CWeaponEMP: public CHLThrowable
{
public:
	DECLARE_CLASS( CWeaponEMP, CHLThrowable );

	DECLARE_SERVERCLASS();

	CWeaponEMP();

	void	Precache( void );
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	void	ThrowGrenade( CBaseCombatCharacter *pOwner );
	void	RollGrenade( CBaseCombatCharacter *pOwner );
	void	LobGrenade( CBaseCombatCharacter *pOwner );

private:
	DECLARE_ACTTABLE();

	DECLARE_DATADESC();
};

acttable_t	CWeaponEMP::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_THROW, true },
};

IMPLEMENT_ACTTABLE(CWeaponEMP);

IMPLEMENT_SERVERCLASS_ST(CWeaponEMP, DT_WeaponEMP)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_emp, CWeaponEMP );
PRECACHE_WEAPON_REGISTER(weapon_emp);

BEGIN_DATADESC( CWeaponEMP )
END_DATADESC()

CWeaponEMP::CWeaponEMP( )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponEMP::Precache( void )
{
	BaseClass::Precache();

	UTIL_PrecacheOther( "grenade_concussive" );

	PrecacheScriptSound( "WeaponFrag.Throw" );
	PrecacheScriptSound( "WeaponFrag.Roll" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CWeaponEMP::ThrowGrenade( CBaseCombatCharacter *pOwner )
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
		vecAiming = pPlayer->GetAutoaimVector( AUTOAIM_2DEGREES );
		vecAiming.z += 0.10; // Raise up so passes through reticle
		gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
	}

	CGrenadeConcussive *pConcussive = (CGrenadeConcussive*)Create( "grenade_concussive", vecSrc, vec3_angle, GetOwner() );
	pConcussive->SetAbsVelocity( vecAiming*740 );
	// Tumble through the air
	QAngle angVel( random->RandomFloat ( -100, -500 ), random->RandomFloat ( -100, -500 ), random->RandomFloat ( -100, -500 ) ); 
	pConcussive->SetLocalAngularVelocity( angVel );

	pConcussive->SetThrower( GetOwner() );
	pConcussive->SetOwnerEntity( ((CBaseEntity*)GetOwner()) );

	m_bRedraw = true;

	WeaponSound( SINGLE );

	m_iPrimaryAttacks++;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CWeaponEMP::LobGrenade( CBaseCombatCharacter *pOwner )
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
		vecAiming = pPlayer->GetAutoaimVector( AUTOAIM_2DEGREES );
		vecAiming.z += 0.20; // Raise up so passes through reticle
		gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
	}

	CGrenadeConcussive *pConcussive = (CGrenadeConcussive*)Create( "grenade_concussive", vecSrc, vec3_angle, GetOwner() );
	pConcussive->SetAbsVelocity( vecAiming*360 );
	// Tumble through the air
	QAngle angVel( random->RandomFloat ( -100, -500 ), random->RandomFloat ( -100, -500 ), random->RandomFloat ( -100, -500 ) ); 
	pConcussive->SetLocalAngularVelocity( angVel );

	pConcussive->SetThrower( GetOwner() );
	pConcussive->SetOwnerEntity( ((CBaseEntity*)GetOwner()) );

	m_bRedraw = true;

	WeaponSound( WPN_DOUBLE );

	m_iPrimaryAttacks++;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CWeaponEMP::RollGrenade( CBaseCombatCharacter *pOwner )
{
	LobGrenade( pOwner );
}

