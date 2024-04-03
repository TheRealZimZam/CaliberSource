//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Keycard item and Weapon
//
// TODO; Code this thing!
//=============================================================================//

#include "cbase.h"
#include "hl2_player.h"
#include "basecombatweapon.h"
#include "gamerules.h"
#include "items.h"
#include "in_buttons.h"
#include "engine/IEngineSound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CItemKeycard : public CItem
{
public:
	DECLARE_CLASS( CItemKeycard, CItem );

	void Spawn( void );
	void Precache( void );

	bool KeyValue( const char *szKeyName, const char *szValue );
	bool MyTouch( CBasePlayer *pPlayer );

private:
	string_t	m_usetarget;	//Pass the name of the usetarget onto the fake weapon
};

LINK_ENTITY_TO_CLASS(item_keycard, CItemKeycard);
PRECACHE_REGISTER(item_keycard);

BEGIN_DATADESC( CItemKeycard )
END_DATADESC()

//-----------------------------------------------------------------------------

void CItemKeycard::Spawn( void )
{
	Precache( );
	SetModel( "models/items/keycard.mdl" );
	BaseClass::Spawn();
}

void CItemKeycard::Precache( void )
{
	PrecacheModel ("models/items/keycard.mdl");
	PrecacheScriptSound( "Battery.Touch" );
	BaseClass::Precache();
}

bool CItemKeycard::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "usetarget"))
		m_usetarget = AllocPooledString(szValue);
	else
		return BaseClass::KeyValue( szKeyName, szValue );

	return true;
}

bool CItemKeycard::MyTouch( CBasePlayer *pPlayer )
{
	// Give card with info on the usetarget
	//TODO;
/*
	CHL2_Player *pHL2Player = dynamic_cast<CHL2_Player *>( pPlayer );

	if ( pHL2Player->Weapon_OwnsThisType( "weapon_keycard", NULL) )
		return false;


	// Create keycard weapon

	// equip
	pHL2Player->Weapon_Equip( pKeycard );
*/
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: CWeaponKeycard
//-----------------------------------------------------------------------------
class CWeaponKeycard : public CBaseCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponKeycard, CBaseCombatWeapon );
//	DECLARE_SERVERCLASS();

	void Precache( void );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	
	CWeaponKeycard(void);

private:
	string_t	m_target;	//The entity this keycard works for
};

LINK_ENTITY_TO_CLASS(weapon_keycard, CWeaponKeycard);
PRECACHE_WEAPON_REGISTER( weapon_keycard );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponKeycard::CWeaponKeycard( void )
{
	m_fMinRange1		= 0;
	m_fMaxRange1		= 64;
}

void CWeaponKeycard::Precache( void )
{
	BaseClass::Precache();
}

void CWeaponKeycard::PrimaryAttack( void )
{
	//Use the entity, if my targetname matches
	//TODO;
}

void CWeaponKeycard::SecondaryAttack( void )
{
	PrimaryAttack();
}
