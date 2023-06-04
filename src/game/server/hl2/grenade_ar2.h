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

#define	AR2_AIRBURST_NO_COLLIDE_TIME 0.2	//Time spent before activation - can bounce within this timeframe
#define	AR2_AIRBURST_DIST 64				//Distance from nearest target to detonate
//#define OLD_SMOKE_TRAIL 1

#ifdef OLD_SMOKE_TRAIL
class SmokeTrail;
#else
class CSpriteTrail;
#endif
class CWeaponAR2;

class CGrenadeAR2 : public CBaseGrenade	//CBaseGrenadeContact
{
public:
	DECLARE_CLASS( CGrenadeAR2, CBaseGrenade );

	void EXPORT				Detonate(void);
	CGrenadeAR2(void);

	DECLARE_DATADESC();

	void		Spawn( void );
	void		Precache( void );
	void 		GrenadeAR2Touch( CBaseEntity *pOther );
	void		GrenadeAR2Think( void );
	void		BounceSound( void );
	void		Event_Killed( const CTakeDamageInfo &info );

	// New Movement
	void		ResolveFlyCollisionCustom( trace_t &trace, Vector &vecVelocity );
	bool		CreateVPhysics( void );

protected:
#ifdef OLD_SMOKE_TRAIL
	CHandle<SmokeTrail>	m_hSmokeTrail;
#else
	CHandle<CSpriteTrail>	m_hSmokeTrail;
#endif
	float				m_fSpawnTime;
	float				m_fDangerRadius;
	bool 				m_bWantsToDetonate;
	static int			m_nAirburstSprite;
};

//Airburst/player variant
class CGrenadeAR2Airburst : public CGrenadeAR2
{
public:
	DECLARE_CLASS( CGrenadeAR2Airburst, CGrenadeAR2 );

	void 		GrenadeAR2Touch( CBaseEntity *pOther );
	void		GrenadeAR2Think( void );
	void		ManualDetonate( void );
	void EXPORT				Detonate(void);

};

//Shrapnel variant
class CGrenadeAR2Shrapnel : public CGrenadeAR2
{
public:
	DECLARE_CLASS( CGrenadeAR2Shrapnel, CGrenadeAR2 );

	void 		GrenadeAR2Touch( CBaseEntity *pOther );
	void		GrenadeAR2Think( void );
	void EXPORT				Detonate(void);

};

#endif	//GRENADEAR2_H
