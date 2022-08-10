//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements breakables and pushables. func_breakable is a bmodel
//			that breaks into pieces after taking damage.
//
//=============================================================================//

#include "cbase.h"
//#include "trains.h"
#include "ndebugoverlay.h"
#include "engine/IEngineSound.h"
#include "doors.h"
#include "soundent.h"
#include "buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

/**********************
	    Pendulum
/**********************/
#define SF_PENDULUM_SWING 2
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

LINK_ENTITY_TO_CLASS( func_pendulum, CPendulum );

BEGIN_DATADESC( CPendulum )
	DEFINE_FIELD( m_flAccel, FIELD_FLOAT ),
	DEFINE_FIELD( m_flTime, FIELD_TIME ),
	DEFINE_FIELD( m_flMaxSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDampSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_vCenter, FIELD_VECTOR ),
	DEFINE_FIELD( m_vStart, FIELD_VECTOR ),
	
	DEFINE_KEYFIELD( m_flMoveDistance, FIELD_FLOAT, "pendistance" ),
	DEFINE_KEYFIELD( m_flDamp, FIELD_FLOAT, "damp" ),
	DEFINE_KEYFIELD( m_flBlockDamage, FIELD_FLOAT, "dmg" ),
	
	DEFINE_FUNCTION( PendulumUse ),
	DEFINE_FUNCTION( Swing ),
	DEFINE_FUNCTION( Stop ),
	DEFINE_FUNCTION( RopeTouch ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Activate", InputActivate ),
END_DATADESC()

void CPendulum::Spawn( void )
{
	CBaseToggle :: AxisDir();

	m_flDamp *=  0.001;
	
	if ( FBitSet ( m_spawnflags, SF_DOOR_PASSABLE ) )
		SetSolid( SOLID_NONE );
	else
		SetSolid( SOLID_BBOX );

	SetMoveType( MOVETYPE_PUSH );
	SetModel( STRING(GetModelName()) );

	if ( m_flMoveDistance != 0 )
	{
		if ( m_flSpeed == 0 )
			 m_flSpeed = 100;

		m_flAccel = ( m_flSpeed * m_flSpeed ) / ( 2 * fabs( m_flMoveDistance ));	// Calculate constant acceleration from speed and distance
		m_flMaxSpeed = m_flSpeed;
		m_vStart = GetAbsAngles();
		m_vCenter = GetAbsAngles() + ( m_flMoveDistance * 0.05 ) * m_vecMoveAng;

		if ( FBitSet( m_spawnflags, SF_BRUSH_ROTATE_START_ON ) )
		{		
			SetThink( &CBaseEntity::SUB_CallUseToggle );
			SetNextThink( gpGlobals->curtime + 0.1f );
		}

		m_flSpeed = 0;
		SetUse( &CPendulum::PendulumUse );

		VPhysicsInitShadow( false, false );
		///VPhysicsGetObject()->SetPosition( GetAbsOrigin(), pev->absangles );
	}

//	Relink();

	if ( FBitSet( m_spawnflags, SF_PENDULUM_SWING ) )
	{
		SetTouch( &CPendulum::RopeTouch );
	}
}


void CPendulum::PendulumUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_flSpeed )		// Pendulum is moving, stop it and auto-return if necessary
	{
		if ( FBitSet( m_spawnflags, SF_BRUSH_ROTATE_START_ON ) )
		{		
			float	delta;

			delta = CBaseToggle :: AxisDelta( m_spawnflags, GetAbsAngles(), m_vStart );

			SetLocalAngularVelocity( m_flMaxSpeed * m_vecMoveAng );
			SetNextThink( gpGlobals->curtime + delta / m_flMaxSpeed);
			SetThink( &CPendulum::Stop );
		}
		else
		{
			m_flSpeed = 0;		// Dead stop
			SetThink( NULL );
			SetLocalAngularVelocity( QAngle( 0, 0, 0 ) );
		}
	}
	else
	{
		SetNextThink( gpGlobals->curtime + 0.1f );		// Start the pendulum moving
		m_flTime = gpGlobals->curtime;		// Save time to calculate dt
		SetThink( &CPendulum::Swing );
		m_flDampSpeed = m_flMaxSpeed;
	}
}

void CPendulum::InputActivate( inputdata_t &inputdata )
{
	SetNextThink( gpGlobals->curtime + 0.1f );		// Start the pendulum moving
	m_flTime = gpGlobals->curtime;				// Save time to calculate dt
	SetThink( &CPendulum::Swing );
	m_flDampSpeed = m_flMaxSpeed;
}

void CPendulum::Stop( void )
{
	SetAbsAngles( m_vStart );
	m_flSpeed = 0;
	SetThink( NULL );
	SetLocalAngularVelocity( QAngle ( 0, 0, 0 ) );
}


void CPendulum::Blocked( CBaseEntity *pOther )
{
	m_flTime = gpGlobals->curtime;
}

void CPendulum::Swing( void )
{
	float delta, dt;
	
	delta = CBaseToggle :: AxisDelta( m_spawnflags, GetAbsAngles(), m_vCenter );
	dt = gpGlobals->curtime - m_flTime;	// How much time has passed?
	m_flTime = gpGlobals->curtime;		// Remember the last time called

	if ( delta > 0 && m_flAccel > 0 )
		m_flSpeed -= m_flAccel * dt;	// Integrate velocity
	else 
		m_flSpeed += m_flAccel * dt;

	if ( m_flSpeed > m_flMaxSpeed )
		 m_flSpeed = m_flMaxSpeed;
	else if ( m_flSpeed < -m_flMaxSpeed )
		m_flSpeed = -m_flMaxSpeed;

	// scale the destdelta vector by the time spent traveling to get velocity
	SetLocalAngularVelocity( m_flSpeed * m_vecMoveAng );

	// Call this again
	SetNextThink( gpGlobals->curtime + 0.1f );
	SetMoveDoneTime( 0.1 );
	
	if ( m_flDamp )
	{
		m_flDampSpeed -= m_flDamp * m_flDampSpeed * dt;
		if ( m_flDampSpeed < 30.0 )
		{
			SetAbsAngles( m_vCenter );
			m_flSpeed = 0;
			SetThink( NULL );
			SetLocalAngularVelocity( QAngle( 0, 0, 0 ) );
		}
		else if ( m_flSpeed > m_flDampSpeed )
			m_flSpeed = m_flDampSpeed;
		else if ( m_flSpeed < -m_flDampSpeed )
			m_flSpeed = -m_flDampSpeed;
	}
}

void CPendulum::Touch( CBaseEntity *pOther )
{
	if ( m_flBlockDamage <= 0 )
		 return;

	// we can't hurt this thing, so we're not concerned with it
	if ( !pOther->m_takedamage )
		  return;

	// calculate damage based on rotation speed
	float damage = m_flBlockDamage * m_flSpeed * 0.01;

	if ( damage < 0 )
		 damage = -damage;

	pOther->TakeDamage( CTakeDamageInfo( this, this, damage, DMG_CRUSH ) );
	
	Vector vNewVel = (pOther->GetAbsOrigin() - GetAbsOrigin());
	
	VectorNormalize( vNewVel );

	pOther->SetAbsVelocity( vNewVel * damage );
}

void CPendulum::RopeTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
	{// not a player!
		DevMsg ( 2, "Not a client\n" );
		return;
	}

	if ( pOther == GetEnemy() )
		 return;
	
	m_hEnemy = pOther;
	pOther->SetAbsVelocity( Vector ( 0, 0, 0 ) );
	pOther->SetMoveType( MOVETYPE_NONE );
}

