//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: DOOM Entities/Weapons for bonus level
//
// TODO; Code this thing!
//=============================================================================//

#ifndef DOOM_H
#define DOOM_H
#ifdef _WIN32
#pragma once
#endif


#include "baseanimating.h"


//-----------------------------------------------------------------------------
// Base DOOM NPC
//-----------------------------------------------------------------------------
class CBaseDoomNPC : public CAI_BaseNPC
{
	DECLARE_CLASS( CBaseDoomNPC, CAI_BaseNPC );
public:
	CBaseDoomNPC( void );
	float	GetIdealAccel() const { return 1000; }	//Stupid-high number, doompcs dont use accel

private:

};


//-----------------------------------------------------------------------------
// Base DOOM Gun
//-----------------------------------------------------------------------------
class CBaseDoomCombatWeapon : public CBaseHLCombatWeapon
{
	DECLARE_CLASS( CBaseDoomCombatWeapon, CBaseHLCombatWeapon );
	DECLARE_DATADESC();

public:
	CBaseDoomCombatWeapon( void );

private:

};

// All the subclasses are declared in the .cpp, they arent going to be referenced anywhere else anyway... -MM

#endif // DOOM_H
