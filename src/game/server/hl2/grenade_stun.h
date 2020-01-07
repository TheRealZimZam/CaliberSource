//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GRENADE_STUN_H
#define GRENADE_STUN_H
#pragma once

class CBaseGrenade;
struct edict_t;

CBaseGrenade *Stungrenade_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, float timer, bool combineSpawned );
bool	Stungrenade_WasPunted( const CBaseEntity *pEntity );
bool	Stungrenade_WasCreatedByCombine( const CBaseEntity *pEntity );

#endif // GRENADE_STUN_H
