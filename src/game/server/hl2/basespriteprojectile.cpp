//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Base class for sprite projectiles
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "basespriteprojectile.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CBaseSpriteProjectile )

END_DATADESC()

void CBaseSpriteProjectile::Precache( void )
{
	PrecacheModel( STRING( GetModelName() ) );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CBaseSpriteProjectile::Spawn( char *pszModel,
								int iSize,
								const Vector &vecVelocity,
								CBaseEntity *pOwner,
								MoveType_t	iMovetype,
								MoveCollide_t nMoveCollide,
								int	iGravity,
								CBaseEntity *pIntendedTarget )
{
	SetModelName( MAKE_STRING(pszModel) );
	Precache();

	AddFlag( FL_OBJECT );
	SetMoveType( iMovetype, nMoveCollide );
	SetSolid( SOLID_BBOX );
	SetSolidFlags( FSOLID_NOT_STANDABLE );

	SetModel( STRING(GetModelName()) );

	UTIL_SetSize( this, -iSize, iSize );
	UTIL_SetOrigin( this, GetAbsOrigin() );

	SetGravity( UTIL_ScaleForGravity( iGravity ) );
	SetAbsVelocity( vecVelocity );

	m_takedamage = DAMAGE_NO;

	SetOwnerEntity( pOwner );

	m_hIntendedTarget.Set( pIntendedTarget );

	// Call think for free the first time. It's up to derived classes to rethink.
	SetTouch( &CBaseSpriteProjectile::Touch );
	SetThink( &CBaseSpriteProjectile::FlyThink );
	SetNextThink( gpGlobals->curtime );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CBaseSpriteProjectile::Touch( CBaseEntity *pOther )
{
	BaseClass::Touch( pOther );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CBaseSpriteProjectile::Think()
{
	FlyThink();
}

//---------------------------------------------------------
//---------------------------------------------------------
void CBaseSpriteProjectile::FlyThink()
{
	StudioFrameAdvance();

	BaseClass::FlyThink();
}

