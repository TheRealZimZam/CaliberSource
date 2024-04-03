//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_BEAMSPOTLIGHT_H
#define C_BEAMSPOTLIGHT_H
#ifdef _WIN32
#pragma once
#endif

#include "beam_shared.h"

class C_BeamSpotLight : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_BeamSpotLight, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_BeamSpotLight();
	~C_BeamSpotLight();

	bool ShouldDraw();
	void ClientThink( void );
	void OnDataChanged( DataUpdateType_t updateType );
	void Release( void );

private:

	Vector SpotlightCurrentPos(void);
	void SpotlightCreate(void);
	void SpotlightDestroy(void);

	// Computes render info for a spotlight
	void ComputeRenderInfo();

private:

	int		m_nHaloIndex;
	int		m_nRotationAxis;
	float	m_flRotationSpeed;
	

	bool m_bSpotlightOn;
	bool m_bHasDynamicLight;

	float m_flSpotlightMaxLength;
	float m_flSpotlightGoalWidth;
	float m_flHDRColorScale;

	Vector	m_vSpotlightTargetPos;
	Vector	m_vSpotlightCurrentPos;
	Vector	m_vSpotlightDir;

	CHandle<C_Beam>	m_hSpotlight;
	
	float	m_flSpotlightCurLength;

	float	m_flLightScale;

	dlight_t*	m_pDynamicLight;

	float m_lastTime;
	CSpotlightTraceCacheEntry *m_pCache;
};

#endif // C_BEAMSPOTLIGHT_H
