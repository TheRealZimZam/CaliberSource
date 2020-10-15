//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: FX Perf convars that are shared by both the client and server
//
// $NoKeywords: $
//=============================================================================//

#ifndef PERFDEFS_H
#define PERFDEFS_H
#ifdef _WIN32
#pragma once
#endif

//------------------------------------------------------------------------------
// FX Related Perf
//------------------------------------------------------------------------------
ConVar perf_explosion_misc( "perf_explosion_misc", "1" );
#define PERF_EXPLOSION_MISC		perf_explosion_misc.GetBool()	// Generate high-perf explosion fx
ConVar perf_explosion_scale( "perf_explosion_scale", "1" );
#define PERF_EXPLOSION_SCALE		perf_explosion_scale.GetInt()	// Amount of particles in explosions

// FIRE
ConVar perf_fire_entity_glow( "perf_fire_entity_glow", "1" );
#define PERF_ENTITY_FLAME_GLOW		perf_fire_entity_glow.GetBool()	// Do entites on fire emit glow/smoke
ConVar perf_fire_flamelets( "perf_fire_flamelets", "0" );
#define PERF_FIRE_FLAMELETS		perf_fire_flamelets.GetInt()	// Small bits of flame that drift off the base flame
ConVar perf_fire_wet( "perf_fire_wet", "1" );
#define PERF_FIRE_WET		perf_fire_wet.GetBool()	// If on-fire near or in a water source, generate steam and decrease size of flames

ConVar perf_rain_trails( "perf_rain_trails", "0" );
#define PERF_RAIN_TRAILS		perf_rain_trails.GetBool()	// Additional water trails when rain collides with a vertical/sloped wall

//------------------------------------------------------------------------------
// AI Perf
//------------------------------------------------------------------------------

#endif // PERFDEFS_H
