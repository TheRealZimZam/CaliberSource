//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "basehlcombatweapon_shared.h"

#ifndef C_BASEHLCOMBATWEAPON_H
#define C_BASEHLCOMBATWEAPON_H
#ifdef _WIN32
#pragma once
#endif

#if 0
class C_HLMachineGun : public C_BaseHLCombatWeapon
{
public:
	DECLARE_CLASS( C_HLMachineGun, C_BaseHLCombatWeapon );
	DECLARE_CLIENTCLASS();
};

class C_HLSelectFireMachineGun : public C_HLMachineGun
{
public:
	DECLARE_CLASS( C_HLSelectFireMachineGun, C_HLMachineGun );
	DECLARE_CLIENTCLASS();
};

class C_HLGrenadeLauncher : public C_HLMachineGun
{
public:
	DECLARE_CLASS( C_HLGrenadeLauncher, C_HLMachineGun );
	DECLARE_CLIENTCLASS();
};

class C_HLSniperRifle : public C_BaseHLCombatWeapon
{
public:
	DECLARE_CLASS( C_HLSniperRifle, C_BaseHLCombatWeapon );
	DECLARE_CLIENTCLASS();
};
#endif 

class C_HLThrowable : public C_BaseHLCombatWeapon
{
public:
	DECLARE_CLASS( C_HLThrowable, C_BaseHLCombatWeapon );
	DECLARE_CLIENTCLASS();
};


#endif // C_BASEHLCOMBATWEAPON_H
