//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 	Base class for simple projectiles
//
//=============================================================================

#ifndef BASEPROJECTILE_H
#define BASEPROJECTILE_H
#ifdef _WIN32
#pragma once
#endif

#include "basegrenade_shared.h"

// Creation.
struct baseprojectilecreate_t
{
	Vector vecOrigin;
	Vector vecVelocity;
	CBaseEntity *pOwner;
	string_t iszModel;
	int iDamageType;
	float flDamageScale;
};

//=============================================================================
//
// Generic projectile
//
class CBaseProjectile : public CBaseGrenade
{
	DECLARE_CLASS( CBaseProjectile, CBaseGrenade );
public:
	DECLARE_DATADESC();

	void	Spawn( void );
	void	Precache( void );

	virtual void	ProjectileTouch( CBaseEntity *pOther );
	void			FlyThink( void );

	// Damage
	void			SetDamageScale( float &flScale ) { m_flDamageScale = flScale; }
	void			SetDamageType( int iType ) { m_iDamageType = iType; }

	virtual float	GetDamageScale( void ) { return m_flDamageScale; }
	virtual int		GetDamageType( void ) { return m_iDamageType; }

private:
	unsigned int	PhysicsSolidMaskForEntity( void ) const;

protected:
	int				m_iDamageType;
	float			m_flDamageScale;
};

#endif // BASEPROJECTILE_H
