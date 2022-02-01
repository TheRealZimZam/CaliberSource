//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Nailbomb
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

class CParticleSystem;

#define SHRAPNEL_GRAVITY 600

class CGrenadeShrapnel : public CBaseGrenade
{
	DECLARE_CLASS( CGrenadeShrapnel, CBaseGrenade );

public:
						CGrenadeShrapnel( void );

	virtual void		Spawn( void );
	virtual void		Precache( void );
	virtual void		Event_Killed( const CTakeDamageInfo &info );

	virtual	unsigned int	PhysicsSolidMaskForEntity( void ) const { return ( BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_WATER ); }

	void 				GrenadeSpitTouch( CBaseEntity *pOther );
	void				Detonate( void );
	void				Think( void );

private:
	DECLARE_DATADESC();
	
	bool			m_bPlaySound;
};

#endif	//GRENADESHRAPNEL_H
