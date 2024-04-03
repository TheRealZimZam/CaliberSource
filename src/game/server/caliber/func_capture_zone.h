//======= Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Control Zone entity
//
//=============================================================================//
#ifndef FUNC_CAPTURE_ZONE_H
#define FUNC_CAPTURE_ZONE_H
#ifdef _WIN32
#pragma once
#endif

#include "triggers.h"

//=============================================================================
class CCaptureZone : public CBaseTrigger
{
	DECLARE_CLASS( CCaptureZone, CBaseTrigger );

public:
	DECLARE_SERVERCLASS();

	void	Spawn();
	void	Touch( CBaseEntity *pOther );
	void	Think( void );

	bool	IsDisabled( void );
	void	SetDisabled( bool bDisabled );

	// Input handlers
	void	InputEnable( inputdata_t &inputdata );
	void	InputDisable( inputdata_t &inputdata );

	int		UpdateTransmitState( void );

	// outputs
	int GetControllingTeam( void ) { return m_ControllingTeam.Get(); };
	COutputInt m_ControllingTeam; // outputs the team currently controlling this spot, whenever it changes - this is -1 when contended

private:

	bool			m_bDisabled;		// Enabled/Disabled?
	int				m_nCapturePoint;	// Used in non-CTF maps to identify this capture point
	int				m_iOwningTeam;		// the original owning team
	int				m_iCapturingTeam;	// the capturing team

	float			m_flTimeTillCaptured;	// time that the control zone has to be uncontested for it to succesfully change teams
	float			m_flNextTouchingEnemyZoneWarning;	// don't spew warnings to the player who is touching the wrong cap
	COutputEvent	m_outputOnCapture;	// Fired a flag is captured on this point.

	DECLARE_DATADESC();
};

#endif // FUNC_CAPTURE_ZONE_H
