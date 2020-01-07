//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: The Longer-Jumper; Double stamina and increase jump range
//
// $NoKeywords: $
//
//=============================================================================//


#include "cbase.h"
#include "player.h"
//#include "weapons.h"
#include "gamerules.h"
#include "items.h"
#include "in_buttons.h"
#include "engine/IEngineSound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


class CItemLongJump : public CItem
{
public:
	DECLARE_CLASS( CItemLongJump, CItem );

	void Spawn( void );
	void Precache( void );
	bool MyTouch( CBasePlayer *pPlayer );

};

LINK_ENTITY_TO_CLASS( item_longjump, CItemLongJump );
PRECACHE_REGISTER(item_longjump);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItemLongJump::Spawn( void )
{
	Precache( );
	SetModel( "models/w_longjump.mdl" );

	BaseClass::Spawn( );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItemLongJump::Precache( void )
{
	PrecacheModel ("models/w_longjump.mdl");

	PrecacheScriptSound( "Longjump.Touch" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
// Output : 
//-----------------------------------------------------------------------------
bool CItemLongJump::MyTouch( CBasePlayer *pPlayer )
{
	if ( pPlayer->m_fLongJump )
	{
		return FALSE;
	}

	if ( pPlayer->IsSuitEquipped() )
	{
		pPlayer->m_fLongJump = TRUE;// player now has longjump module

		CSingleUserRecipientFilter user( pPlayer );
		user.MakeReliable();

		UserMessageBegin( user, "ItemPickup" );
			WRITE_STRING( STRING(pev->classname) );
		MessageEnd();

		UTIL_EmitSoundSuit( pPlayer->edict(), "!HEV_A1" );	// Play the longjump sound UNDONE: Kelly? correct sound?
		return true;		
	}
	return false;
}

