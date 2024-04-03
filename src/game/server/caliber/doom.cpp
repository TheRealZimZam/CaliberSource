//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: DOOM Entities/Weapons for bonus level
//
// TODO; Code this thing!
//=============================================================================//
#include "cbase.h"
#include "doom.h"
#include "basehlcombatweapon.h"
#include "player.h"
#include "gamerules.h"
#include "ammodef.h"
#include "in_buttons.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//=============================================================================
// NPCS
//=============================================================================

//-----------------------------------------------------------------------------
// Purpose: Doom Basenpc
//-----------------------------------------------------------------------------
CBaseDoomNPC::CBaseDoomNPC( void )
{

}

//
// ZOMBIEMAN
//
class CDoomZombieman : public CBaseDoomNPC
{
	DECLARE_CLASS( CDoomZombieman, CBaseDoomNPC );
public:

private:

};

//
// SHOTGUNNER
//
class CDoomShotgunner : public CDoomZombieman
{
	// Shotgunner inherits from zombieman
	DECLARE_CLASS( CDoomShotgunner, CDoomZombieman );
public:

private:

};

//
// CHAINGUNNER
//
class CDoomChaingunner : public CDoomZombieman
{
	// Chaingunner inherits from zombieman
	DECLARE_CLASS( CDoomChaingunner, CDoomZombieman );
public:

private:

};

//
// IMP
//
class CDoomImp : public CBaseDoomNPC
{
	DECLARE_CLASS( CDoomImp, CBaseDoomNPC );
public:

private:

};

//=============================================================================
// WEAPONS
//=============================================================================


//-----------------------------------------------------------------------------
// Purpose: Doom Baseweapon
//-----------------------------------------------------------------------------
CBaseDoomCombatWeapon::CBaseDoomCombatWeapon( void )
{
	// Weapon defaults
	m_fMinRange1		= 0;
	m_fMaxRange1		= 2048;
	m_bCanJam			= false;
	m_bReloadsSingly	= false;
	m_bReloadsFullClip	= false;
	m_bFiresUnderwater	= true;
}

//
// PISTOL
//
