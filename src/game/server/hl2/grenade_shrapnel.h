//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Launched Grenade
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef	GRENADESHRAPNEL_H
#define	GRENADESHRAPNEL_H

#include "basegrenade_shared.h"

#define	SHRAPNEL_NO_COLLIDE_TIME 0.2	//Time spent before activation - will blow up after this long

class CGrenadeShrapnel : public CBaseGrenade	//CBaseGrenadeContact
{
public:
	DECLARE_CLASS( CGrenadeShrapnel, CBaseGrenade );

	CGrenadeShrapnel(void);

	DECLARE_DATADESC();

	void		Spawn( void );
	void		Precache( void );
	void 		GrenadeShrapnelTouch( CBaseEntity *pOther );
	void		GrenadeShrapnelThink( void );
	void		Event_Killed( const CTakeDamageInfo &info );

	void		Detonate(void);

protected:
	float				m_fSpawnTime;

};

#endif	//GRENADESHRAPNEL_H
