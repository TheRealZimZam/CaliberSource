//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Weapon stubbies
//
//=============================================================================//

#include "cbase.h"
#include "c_weapon__stubs.h"
#include "basehlcombatweapon_shared.h"
#include "c_basehlcombatweapon.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

STUB_WEAPON_CLASS( cycler_weapon, WeaponCycler, C_BaseCombatWeapon );
STUB_WEAPON_CLASS( weapon_cubemap, WeaponCubemap, C_BaseCombatWeapon );

// Weapons that arent supported by MP
STUB_WEAPON_CLASS( weapon_alyxgun, WeaponAlyxGun, C_HLSelectFireMachineGun );
STUB_WEAPON_CLASS( weapon_annabelle, WeaponAnnabelle, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_binoculars, WeaponBinoculars, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_brickbat, WeaponBrickbat, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_bugbait, WeaponBugBait, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_citizenpackage, WeaponCitizenPackage, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_citizensuitcase, WeaponCitizenSuitcase, C_WeaponCitizenPackage );
STUB_WEAPON_CLASS( weapon_flaregun, Flaregun, C_BaseHLCombatWeapon );					//Ball of fire shooter
STUB_WEAPON_CLASS( weapon_flameprojector, WeaponFlameprojector, C_BaseHLCombatWeapon );	//Napalm Launcher
#ifdef HL2_LOSTCOAST
STUB_WEAPON_CLASS( weapon_oldmanharpoon, WeaponOldManHarpoon, C_WeaponCitizenPackage );
#endif

// Weapons that are supported in MP
#ifndef HL2MP
STUB_WEAPON_CLASS( weapon_357, Weapon357, C_BaseHLCombatWeapon );						//Super Magnum
//STUB_WEAPON_CLASS( weapon_ar1, WeaponAR1, C_HLMachineGun );								//Assault Rifle
STUB_WEAPON_CLASS( weapon_ar2, WeaponAR2, C_HLMachineGun );								//Battle Rifle
STUB_WEAPON_CLASS( weapon_crossbow, WeaponCrossbow, C_BaseHLCombatWeapon );				//Hot-Stick Launcher
STUB_WEAPON_CLASS( weapon_crowbar, WeaponCrowbar, C_BaseHLBludgeonWeapon );
STUB_WEAPON_CLASS( weapon_flamethrower, WeaponFlamethrower, C_BaseHLCombatWeapon );		//Flamethrower
STUB_WEAPON_CLASS( weapon_frag, WeaponFrag, C_BaseHLCombatWeapon );						//Frag 'Nade
STUB_WEAPON_CLASS( weapon_gauss, WeaponGaussGun, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_gpistol, WeaponGPistol, C_BaseHLCombatWeapon );				//Golden Gun
STUB_WEAPON_CLASS( weapon_hmg1, WeaponHMG1, C_HLSelectFireMachineGun );					//Small-Machine Gun
STUB_WEAPON_CLASS( weapon_lightrpg, WeaponLightRPG, C_BaseHLCombatWeapon );				//Rawket Lawnchairx4
//STUB_WEAPON_CLASS( weapon_mitcl, WeaponMITCL, C_BaseHLCombatWeapon );					//Sticky Lawnchair
STUB_WEAPON_CLASS( weapon_molotov, WeaponMolotov, C_BaseHLCombatWeapon );				//Finnish Booze
//!STUB_WEAPON_CLASS( weapon_physcannon, WeaponPhysCannon, C_BaseHLCombatWeapon );			//!!!ALREADY DEFINED
STUB_WEAPON_CLASS( weapon_pistol, WeaponPistol, C_BaseHLCombatWeapon );					//Pistol
STUB_WEAPON_CLASS( weapon_rpg, WeaponRPG, C_BaseHLCombatWeapon );						//Rawket Lawnchair
STUB_WEAPON_CLASS( weapon_shotgun, WeaponShotgun, C_BaseHLCombatWeapon );				//Elephant Gun
STUB_WEAPON_CLASS( weapon_slam, Weapon_SLAM, C_BaseHLCombatWeapon );					//SLAM'd
STUB_WEAPON_CLASS( weapon_smg1, WeaponSMG1, C_HLSelectFireMachineGun );					//Small-Machine Gun
STUB_WEAPON_CLASS( weapon_smg2, WeaponSMG2, C_HLSelectFireMachineGun );					//Small-Machine Gun 2
STUB_WEAPON_CLASS( weapon_sniperrifle, WeaponSniperRifle, C_BaseHLCombatWeapon );		//Snooper Rifle
STUB_WEAPON_CLASS( weapon_stab, WeaponStab, C_BaseHLBludgeonWeapon );
STUB_WEAPON_CLASS( weapon_stun, WeaponStun, C_BaseHLCombatWeapon );						//Stun 'Nade
STUB_WEAPON_CLASS( weapon_supershotgun, WeaponSuperShotgun, C_BaseHLCombatWeapon );		//Auto-Shotgun
STUB_WEAPON_CLASS( weapon_swing, WeaponSwing, C_BaseHLBludgeonWeapon );
STUB_WEAPON_CLASS( weapon_hopwire, WeaponHopwire, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_proto1, WeaponProto1, C_BaseHLCombatWeapon );
#endif

