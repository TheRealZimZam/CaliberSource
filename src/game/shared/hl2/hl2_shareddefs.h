//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HL2_SHAREDDEFS_H
#define HL2_SHAREDDEFS_H

#ifdef _WIN32
#pragma once
#endif

#include "const.h"

//--------------
// HL2 SPECIFIC
//--------------
enum
{
	HL2COLLISION_GROUP_PLASMANODE = LAST_SHARED_COLLISION_GROUP,
	HL2COLLISION_GROUP_SPIT,
	HL2COLLISION_GROUP_HOMING_MISSILE,
	HL2COLLISION_GROUP_COMBINE_BALL,

	HL2COLLISION_GROUP_FIRST_NPC,
	HL2COLLISION_GROUP_HOUNDEYE,
	HL2COLLISION_GROUP_CROW,
	HL2COLLISION_GROUP_HEADCRAB,
	HL2COLLISION_GROUP_STRIDER,
	HL2COLLISION_GROUP_GUNSHIP,
	HL2COLLISION_GROUP_ANTLION,
	HL2COLLISION_GROUP_LAST_NPC,
	HL2COLLISION_GROUP_COMBINE_BALL_NPC,
};

#define DMG_SNIPER			(DMG_LASTGENERICFLAG<<1)	// This is sniper damage
#define DMG_MISSILEDEFENSE	(DMG_LASTGENERICFLAG<<2)	// The only kind of damage missiles take. (special missile defense)

// these are the damage types that are big enough to damage armored targets
#define DMG_ARMORPIERCING		( DMG_BLAST | DMG_SONIC | DMG_AIRBOAT | DMG_ENERGYBEAM | DMG_SNIPER )

// Length after raising the weapon during which releasing the button will cause a parry
#define PARRY_DETECTION_TIME			0.5
// Length after a parry detection in which a parry can occur
#define PARRY_OPPORTUNITY_LENGTH		0.3
// Length after a parry has finished before I can do anything again
#define PARRY_VULNERABLE_TIME			0.5

//--------------
// MP
//--------------

#define POINT_CONTESTED		1
#define POINT_FRIENDLY		2
#define POINT_ENEMY			3

// Sniper deploy state
enum
{
	SNIPER_DEPLOY_START = 1,
	SNIPER_DEPLOY_IDLE,
	SNIPER_DEPLOY_LEAVE,
};

// Powerups
enum
{
	POWERUP_BOOST,		// Medic Healthboost
	POWERUP_AMMO,		// Support ammo buff
	POWERUP_RUSH,		// Commando buff
	POWERUP_NUKE,		// 20x killstreak
	MAX_POWERUPS
};


#endif // HL2_SHAREDDEFS_H
