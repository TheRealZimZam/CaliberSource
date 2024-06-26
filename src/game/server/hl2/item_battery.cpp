//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Handling for the suit batteries.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hl2_player.h"
#include "basecombatweapon.h"
#include "gamerules.h"
#include "items.h"
#include "engine/IEngineSound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CItemBattery : public CItem
{
public:
	DECLARE_CLASS( CItemBattery, CItem );

	void Spawn( void )
	{
		Precache( );
		SetModel( "models/items/battery.mdl" );
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/items/battery.mdl");

		PrecacheScriptSound( "Battery.Touch" );
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		CHL2_Player *pHL2Player = dynamic_cast<CHL2_Player *>( pPlayer );
		return ( pHL2Player && pHL2Player->ApplyBattery() );
	}
};

LINK_ENTITY_TO_CLASS(item_battery, CItemBattery);
PRECACHE_REGISTER(item_battery);

//-----------------------------------------------------------------------------
class CItemBigBattery : public CItem
{
public:
	DECLARE_CLASS( CItemBigBattery, CItem );

	void Spawn( void )
	{
		Precache( );
		SetModel( "models/items/armourplate.mdl" );
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/items/armourplate.mdl");

		PrecacheScriptSound( "Battery.Touch" );

	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		CHL2_Player *pHL2Player = dynamic_cast<CHL2_Player *>( pPlayer );
		return ( pHL2Player && pHL2Player->ApplyBattery(2) );
	}
};

LINK_ENTITY_TO_CLASS(item_armourplate, CItemBigBattery);
PRECACHE_REGISTER(item_armourplate);
