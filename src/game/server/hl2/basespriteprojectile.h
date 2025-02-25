//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Base class for sprite projectiles
//
// $NoKeywords: $
//=============================================================================//

#ifndef BASESPRITEPROJECTILE_H
#define BASESPRITEPROJECTILE_H
#ifdef _WIN32
#pragma once
#endif

#include "Sprite.h"
#include "baseprojectile.h"

enum MoveType_t;
enum MoveCollide_t;


//=============================================================================
//=============================================================================
class CBaseSpriteProjectile : public CBaseProjectile
{
	DECLARE_CLASS( CBaseSpriteProjectile, CBaseProjectile );

public:
	void Spawn(	char *pszModel,
				int iSize,
				const Vector &vecVelocity,
				CBaseEntity *pOwner,
				MoveType_t	iMovetype,
				MoveCollide_t nMoveCollide,
				int	iGravity,
				CBaseEntity *pIntendedTarget = NULL );

	virtual void	Precache( void );

	virtual void Touch( CBaseEntity *pOther );

	virtual void Think();
	virtual void FlyThink();

	DECLARE_DATADESC();
};

#endif // BASESPRITEPROJECTILE_H
