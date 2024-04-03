//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines a class for objects that break after taking a certain amount
//			of damage.
//
// $NoKeywords: $
//=============================================================================//

#ifndef FUNC_PENDULUM
#define FUNC_PENDULUM
#ifdef _WIN32
#pragma once
#endif


/**********************
	    Pendulum
/**********************/

class CPendulum : public CBaseToggle
{
	DECLARE_CLASS( CPendulum, CBaseToggle );
public:
	void	Spawn ( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Swing( void );
	void	PendulumUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	Stop( void );
	void	Touch( CBaseEntity *pOther );
	void	RopeTouch ( CBaseEntity *pOther );// this touch func makes the pendulum a rope
	void	Blocked( CBaseEntity *pOther );

	// Input handlers.
	void	InputActivate( inputdata_t &inputdata );

	DECLARE_DATADESC();
	
	float	m_flAccel;			// Acceleration
	float	m_flTime;
	float	m_flDamp;
	float	m_flMaxSpeed;
	float	m_flDampSpeed;
	QAngle	m_vCenter;
	QAngle	m_vStart;
	float   m_flBlockDamage;

	EHANDLE		m_hEnemy;
};

#endif	// FUNC_PENDULUM
