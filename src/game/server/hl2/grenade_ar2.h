//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Projectile shot from the AR2 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef	GRENADEAR2_H
#define	GRENADEAR2_H

#include "basegrenade_shared.h"

#define	MAX_AR2_NO_COLLIDE_TIME 0.2		//Time spent before activation - can bounce within this timeframe
#define	AR2_AIRBURST_DIST 64			//Distance from nearest target to detonate

class SmokeTrail;
class CWeaponAR2;

class CGrenadeAR2 : public CBaseGrenade	//CBaseGrenadeContact
{
public:
	DECLARE_CLASS( CGrenadeAR2, CBaseGrenade );

	CHandle< SmokeTrail > m_hSmokeTrail;
	float				 m_fSpawnTime;
	float				m_fDangerRadius;

	void		Spawn( void );
	void		Precache( void );
	void 		GrenadeAR2Touch( CBaseEntity *pOther );
	void		GrenadeAR2Think( void );
	void		Event_Killed( const CTakeDamageInfo &info );

public:
	void EXPORT				Detonate(void);
	CGrenadeAR2(void);

	DECLARE_DATADESC();
};

//Airburst/player variant
class CGrenadeAR2Airburst : public CGrenadeAR2
{
public:
	DECLARE_CLASS( CGrenadeAR2Airburst, CGrenadeAR2 );

	void 		GrenadeAR2Touch( CBaseEntity *pOther );
	void		GrenadeAR2Think( void );

};

#endif	//GRENADEAR2_H
