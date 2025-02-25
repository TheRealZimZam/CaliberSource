//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Satchel Charge
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef	SATCHEL_H
#define	SATCHEL_H

#ifdef _WIN32
#pragma once
#endif

#include "basegrenade_shared.h"
#include "weapon_slam.h"

class CSoundPatch;
class CSprite;

class CSatchelCharge : public CBaseGrenade
{
public:
	DECLARE_CLASS( CSatchelCharge, CBaseGrenade );

	void			Precache( void );
	void			Spawn( void );
	void			UpdateSlideSound( void );
	void			KillSlideSound(void);
	void			SatchelTouch( CBaseEntity *pOther );
	void			SatchelThink( void );
	void			SatchelUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	// Movement
	void			ResolveFlyCollisionCustom( trace_t &trace, Vector &vecVelocity );
	void			BounceSound( void );

	// Input handlers
	void			InputExplode( inputdata_t &inputdata );

	CSoundPatch*	m_soundSlide;
	float			m_flSlideVolume;
	float			m_flNextBounceSoundTime;
	bool			m_bInAir;
	Vector			m_vLastPosition;

public:
	CWeapon_SLAM*	m_pMyWeaponSLAM;	// Who shot me..
	bool			m_bIsAttached;
	void			Deactivate( void );

	CSatchelCharge();
	~CSatchelCharge();

	DECLARE_DATADESC();

private:
	void InitSlideSound( void );
	void CreateEffects( void );
	CHandle<CSprite>	m_hGlowSprite;
};

#endif	//SATCHEL_H
