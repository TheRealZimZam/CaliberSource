//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef PROTO_SNIPER_H
#define PROTO_SNIPER_H
#ifdef _WIN32
#pragma once
#endif

#include "beam_shared.h"

#define SF_SNIPER_HIDDEN		(1 << 16) ///< No model, impervious to damage.
#define SF_SNIPER_VIEWCONE		(1 << 17) ///< when set, sniper only sees in a small cone around the laser.
#define SF_SNIPER_NOCORPSE		(1 << 18) ///< when set, no corpse
#define SF_SNIPER_STARTDISABLED	(1 << 19)
#define SF_SNIPER_FAST			(1 << 20) ///< This is semi-auto, faster-shooting sniper. Paint time is decreased 25%. Bullet speed increases 150%.
#define SF_SNIPER_NOSWEEP		(1 << 21) ///< This sniper doesn't sweep to the target or use decoys.
#define SF_SNIPER_TAKEALLDMG	(1 << 22) ///< This sniper can be counter-sniped.

#define NUM_PENETRATIONS	3
#define PENETRATION_THICKNESS	5

//=========================================================
//=========================================================
class CSniperTarget : public CPointEntity
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( CSniperTarget, CPointEntity );

	bool KeyValue( const char *szKeyName, const char *szValue );

	string_t m_iszGroupName;
};

//=========================================================
//=========================================================
class CSniperBullet : public CBaseEntity
{
public:
	DECLARE_CLASS( CSniperBullet, CBaseEntity );

	CSniperBullet( void ) { Init(); }

	Vector	m_vecDir;

	Vector		m_vecStart;
	Vector		m_vecEnd;

	float	m_flLastThink;
	float	m_SoundTime;
	int		m_AmmoType;
	int		m_PenetratedAmmoType;
	float	m_Speed;
	bool	m_bDirectShot;

	void Precache( void );
	bool IsActive( void ) { return m_fActive; }

	bool Start( const Vector &vecOrigin, const Vector &vecTarget, CBaseEntity *pOwner, bool bDirectShot );
	void Stop( void );

	void BulletThink( void );

	void Init( void );

	DECLARE_DATADESC();

private:
	// Only one shot per sniper at a time. If a bullet hasn't
	// hit, the shooter must wait.
	bool	m_fActive;

	// This tracks how many times this single bullet has 
	// struck. This is for penetration, so the bullet can
	// go through things.
	int		m_iImpacts;
};

#endif // PROTO_SNIPER_H
