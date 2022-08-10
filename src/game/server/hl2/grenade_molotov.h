//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose:		Molotov grenades
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================

#ifndef	GRENADEMOLOTOV_H
#define	GRENADEMOLOTOV_H

#include "basegrenade_shared.h"
#include "smoke_trail.h"

#define	MOLOTOV_NO_COLLIDE_TIME	0.15		//Time spent before activation - can bounce within this timeframe
#define	MOLOTOV_BREAK_VELOCITY	350			//Dentonate if smashing against something and going faster than this
#define	MOLOTOV_FUSE_TIME_MIN 	3			//Dentonate if i've been rolling around for this long
#define	MOLOTOV_FUSE_TIME_MAX 	5

class CGrenadeMolotov : public CBaseGrenade
{
public:
	DECLARE_CLASS( CGrenadeMolotov, CBaseGrenade );

	virtual void	Spawn( void );
	virtual void	Precache( void );
	virtual void	Detonate( void );
	void			ResolveFlyCollisionCustom( trace_t &trace, Vector &vecVelocity );
	void			MolotovTouch( CBaseEntity *pOther );
	void			MolotovThink( void );

protected:

	SmokeTrail		*m_pFireTrail;
	float			m_fSpawnTime;
	float			m_fFuseTime;

	DECLARE_DATADESC();
};

#endif	//GRENADEMOLOTOV_H
