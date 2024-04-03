//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#if !defined( BEAMSPOTLIGHT_H )
#define BEAMSPOTLIGHT_H
#ifdef _WIN32
#pragma once
#endif

#include "EnvLaser.h"

// Spawnflags
enum BeamSpotlightSpawnFlags_t
{
	SF_BEAM_SPOTLIGHT_START_LIGHT_ON    = 1,
	SF_BEAM_SPOTLIGHT_NO_DYNAMIC_LIGHT  = 2,
	SF_BEAM_SPOTLIGHT_START_ROTATE_ON   = 4,
	SF_BEAM_SPOTLIGHT_REVERSE_DIRECTION = 8,
	SF_BEAM_SPOTLIGHT_X_AXIS            = 16,
	SF_BEAM_SPOTLIGHT_Y_AXIS            = 32,
};


class CBeamSpotlight : public CBaseEntity
{
	DECLARE_CLASS( CBeamSpotlight, CBaseEntity );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CBeamSpotlight();

	void	Spawn( void );
	void	Precache( void );

	void InputTurnOn( inputdata_t &inputdata );
	void InputTurnOff( inputdata_t &inputdata );
	void InputStart( inputdata_t &inputdata );
	void InputStop( inputdata_t &inputdata );
	void InputReverse( inputdata_t &inputdata );

protected:
	bool KeyValue( const char *szKeyName, const char *szValue );

private:
	int  UpdateTransmitState();
	void RecalcRotation( void );

	CNetworkVar( int, m_nHaloIndex );

	CNetworkVar( bool, m_bSpotlightOn );
	CNetworkVar( bool, m_bHasDynamicLight );

	CNetworkVar( float, m_flSpotlightMaxLength );
	CNetworkVar( float,	m_flSpotlightGoalWidth );
	CNetworkVar( float,	m_flHDRColorScale );
	CNetworkVar( int, m_nMinDXLevel );
	CNetworkVar( int, m_nRotationAxis );
	CNetworkVar( float, m_flRotationSpeed );

	float m_flmaxSpeed;
	bool m_isRotating;
	bool m_isReversed;

public:
	COutputEvent m_OnOn, m_OnOff;     ///< output fires when turned on, off
};

#endif // BEAMSPOTLIGHT_H
