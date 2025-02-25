//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 	Base class for projectiles
//
//=============================================================================

#ifndef BASEPROJECTILE_H
#define BASEPROJECTILE_H
#ifdef _WIN32
#pragma once
#endif

#include "basegrenade_shared.h"


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

	virtual void	Touch( CBaseEntity *pOther );
	virtual void	FlyThink( void );

	// Damage
	void			SetDamageType( int iType ) { m_iDamageType = iType; }
	virtual int		GetDamageType( void ) { return m_iDamageType; }

private:
	unsigned int	PhysicsSolidMaskForEntity( void ) const;

protected:
	int				m_iDamageType;
	EHANDLE			m_hIntendedTarget;

};

#endif // BASEPROJECTILE_H
