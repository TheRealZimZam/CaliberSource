//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a sniper rifle weapon.
//			
//			Primary attack: fires a single high-powered shot, then reloads.
//			Secondary attack: cycles sniper scope through zoom levels.
//
// NOTENOTE; This can be used as a baseclass for weapons that have a zoom
//=============================================================================//

#ifndef WEAPON_SNIPERRIFLE_H
#define WEAPON_SNIPERRIFLE_H

#include "basehlcombatweapon_shared.h"

#if defined( _WIN32 )
#pragma once
#endif

#define SNIPER_CONE_PLAYER					Vector( 0.06976, 0.06976, 0.06976 )	// Spread cone when fired by the player.
#define SNIPER_CONE_PLAYER_ZOOMED			vec3_origin	// Spread cone when fired by the player.
#define SNIPER_CONE_NPC						vec3_origin	// Spread cone when fired by NPCs.
#define SNIPER_BULLET_COUNT_PLAYER			1			// Fire n bullets per shot fired by the player.
#define SNIPER_BULLET_COUNT_NPC				1			// Fire n bullets per shot fired by NPCs.
#define SNIPER_TRACER_FREQUENCY_PLAYER		0			// Draw a tracer every nth shot fired by the player.
#define SNIPER_TRACER_FREQUENCY_NPC			0			// Draw a tracer every nth shot fired by NPCs.
#define SNIPER_KICKBACK						8			// Range for punchangle when firing.

#ifdef CLIENT_DLL
#define CHLSniperRifle C_HLSniperRifle
#endif
class CHLSniperRifle : public CBaseHLCombatWeapon
{
	DECLARE_CLASS( CHLSniperRifle, CBaseHLCombatWeapon );
public:
	CHLSniperRifle();

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();

	void Precache( void );

	const Vector &GetBulletSpread( void );

	bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	void ItemPostFrame( void );
	void PrimaryAttack( void );
	bool Reload( void );
	bool CycleBolt( void );
	void Zoom( void );
	bool IsWeaponZoomed() { return m_nZoomLevel!=0; }

#ifndef CLIENT_DLL
	void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
#endif

protected:
	CNetworkVar( int,	m_nZoomLevel );
	CNetworkVar( float,	m_fNextZoom );
	bool m_bBoltAction;	//Use special bolt-action reloading mechanism
	bool m_bUseProjectile;	//Use the sniperbullet projectile instead of a hitscan

};

#endif // WEAPON_SNIPERRIFLE_H
