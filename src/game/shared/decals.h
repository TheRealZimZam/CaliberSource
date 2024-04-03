//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef DECALS_H
#define DECALS_H
#ifdef _WIN32
#pragma once
#endif

// NOTE: If you add a tex type, be sure to modify the s_pImpactEffect
// array in fx_impact.cpp to get an effect when that surface is shot.
#define CHAR_TEX_ANTLION		'A'
#define CHAR_TEX_BLOODYFLESH	'B' ///< Gibs.
#define	CHAR_TEX_CONCRETE		'C'
#define CHAR_TEX_DIRT			'D'
#define CHAR_TEX_EGGSHELL		'E' ///< the egg sacs in the tunnels in ep2.
#define CHAR_TEX_FLESH			'F' ///< Corpses.
#define CHAR_TEX_GRATE			'G'
#define CHAR_TEX_ALIENFLESH		'H'
#define CHAR_TEX_CLIP			'I' ///< DO NOT USE
#define CHAR_TEX_GRASS			'J' ///< Fuzzy dirt, less dust when hit.
#define CHAR_TEX_SNOW			'K' ///< Cold dirt, doesnt leave bulletholes.
#define CHAR_TEX_PLASTIC		'L' ///< Rubber, Plastic items.
#define CHAR_TEX_METAL			'M'
#define CHAR_TEX_SAND			'N' ///< Sand (extra crunchy and dusty dirt).
#define CHAR_TEX_FOLIAGE		'O' ///< Small plants, nature stuff, shoots leaves on impact.
#define CHAR_TEX_PLASTER		'P' ///< Brick, very old concrete and clay.
#define CHAR_TEX_ASPHALT		'Q' ///< Road/cheap concrete, the flaky easily crackable stuff.
#define CHAR_TEX_COMPUTER		'R' ///< Computers/Sparky metal things.
#define CHAR_TEX_SLOSH			'S' ///< Water
#define CHAR_TEX_TILE			'T' ///< Porcelain/ceramic tiles.
#define CHAR_TEX_CARDBOARD		'U' ///< Cardboard and plywood. No flecks when hit.
#define CHAR_TEX_VENT			'V'
#define CHAR_TEX_WOOD			'W' ///< Natural wood/thick wood.
#define CHAR_TEX_UNUSED			'X' ///< DO NOT USE - "fake" materials use this (ladders, wading, clips, etc)
#define CHAR_TEX_GLASS			'Y'
#define CHAR_TEX_WARPSHIELD		'Z' ///< wierd-looking jello effect for advisor shield.

abstract_class IDecalEmitterSystem
{
public:
	virtual int	GetDecalIndexForName( char const *decalname ) = 0;
	virtual char const *TranslateDecalForGameMaterial( char const *decalName, unsigned char gamematerial ) = 0;
};

extern IDecalEmitterSystem *decalsystem;

#endif // DECALS_H
