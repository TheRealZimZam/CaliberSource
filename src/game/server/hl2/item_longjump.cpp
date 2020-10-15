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

	CollisionProp()->UseTriggerBounds( true, 16.0f );
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
	CHL2_Player *pHL2Player = (CHL2_Player*)pPlayer;

	if ( pHL2Player->m_bHasLongJump == true )	//m_fLongJump
	{
		return false;
	}

	if ( pHL2Player->IsSuitEquipped() )
	{
		pHL2Player->m_bHasLongJump = true;// player now has longjump module

		CSingleUserRecipientFilter user( pHL2Player );
		user.MakeReliable();

		UserMessageBegin( user, "ItemPickup" );
			WRITE_STRING( STRING(m_iClassname) );
		MessageEnd();

		UTIL_EmitSoundSuit( pHL2Player->edict(), "!HEV_A1" );	// Play the longjump sound UNDONE: Kelly? correct sound?
		return true;		
	}
	return false; 
}

