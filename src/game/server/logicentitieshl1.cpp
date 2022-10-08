//====== Copyright © 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "entityinput.h"
#include "entityoutput.h"
#include "ndebugoverlay.h"
#include "EntityList.h"
#include "mathlib/mathlib.h"
//#include "engine/IEngineSound.h"
#include "doors.h"
//#include "soundent.h"
#include "globalstate.h"
#include "vstdlib/random.h"
#include "gameinterface.h"
#include "soundscape.h"
#include "buttons.h"


/*********************
	TRIGGERS: trigger_auto, trigger_relay, multimanager: replaced in src by logic_auto and logic_relay
*********************/

// This trigger will fire when the level spawns (or respawns if not fire once)
// It will check a global state before firing.  
#define SF_AUTO_FIREONCE		0x0001

class CAutoTrigger : public CBaseEntity
{
	DECLARE_CLASS( CAutoTrigger, CBaseEntity );
public:
	void Spawn( void );
	void Precache( void );
	void Think( void );

	int ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();

private:

	COutputEvent	m_OnTrigger;
	string_t		m_globalstate;
};
LINK_ENTITY_TO_CLASS( trigger_auto, CAutoTrigger );

BEGIN_DATADESC( CAutoTrigger )
	DEFINE_KEYFIELD( m_globalstate, FIELD_STRING, "globalstate" ),

	// Outputs
	DEFINE_OUTPUT( m_OnTrigger, "OnTrigger"),
END_DATADESC()


void CAutoTrigger::Spawn( void )
{
	Precache();
}


void CAutoTrigger::Precache( void )
{
	SetNextThink( gpGlobals->curtime + 0.1f );
}


//-----------------------------------------------------------------------------
// Purpose: Checks the global state and fires targets if the global state is set.
//-----------------------------------------------------------------------------
void CAutoTrigger::Think( void )
{
	if ( !m_globalstate || GlobalEntity_GetState( m_globalstate ) == GLOBAL_ON )
	{
		m_OnTrigger.FireOutput(NULL, this);

		if ( m_spawnflags & SF_AUTO_FIREONCE )
			UTIL_Remove( this );
	}
}


#define SF_RELAY_FIREONCE		0x0001
class CTriggerRelay : public CBaseEntity
{
	DECLARE_CLASS( CTriggerRelay, CBaseEntity );
public:

	CTriggerRelay( void );

	bool KeyValue( const char *szKeyName, const char *szValue );
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void RefireThink( void );

	int ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();

private:
	USE_TYPE	triggerType;
	float		m_flRefireInterval;
	float		m_flRefireDuration;
	float		m_flTimeRefireDone;

	USE_TYPE	m_TargetUseType;
	float		m_flTargetValue;

	COutputEvent m_OnTrigger;
};
LINK_ENTITY_TO_CLASS( trigger_relay, CTriggerRelay );

BEGIN_DATADESC( CTriggerRelay )
	DEFINE_FIELD( triggerType, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_flRefireInterval, FIELD_FLOAT, "repeatinterval" ),
	DEFINE_KEYFIELD( m_flRefireDuration, FIELD_FLOAT, "repeatduration" ),
	DEFINE_FIELD( m_flTimeRefireDone, FIELD_TIME ),
	DEFINE_FIELD( m_TargetUseType, FIELD_INTEGER ),
	DEFINE_FIELD( m_flTargetValue, FIELD_FLOAT ),

	// Function Pointers
	DEFINE_FUNCTION( RefireThink ),

	// Outputs
	DEFINE_OUTPUT( m_OnTrigger, "OnTrigger"),
END_DATADESC()

CTriggerRelay::CTriggerRelay( void )
{
	m_flRefireInterval = -1;
	m_flRefireDuration = -1;
	m_flTimeRefireDone = -1;
}

bool CTriggerRelay::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "triggerstate"))
	{
		int type = atoi( szValue );
		switch( type )
		{
		case 0:
			triggerType = USE_OFF;
			break;
		case 2:
			triggerType = USE_TOGGLE;
			break;
		default:
			triggerType = USE_ON;
			break;
		}
	}
	else
	{
		return BaseClass::KeyValue( szKeyName, szValue );
	}

	return true;
}


void CTriggerRelay::Spawn( void )
{
}


void CTriggerRelay::RefireThink( void )
{
	// sending this as Activator and Caller right now. Seems the safest thing
	// since whatever fired the relay the first time may no longer exist. 
	Use( this, this, m_TargetUseType, m_flTargetValue ); 

	if( gpGlobals->curtime > m_flTimeRefireDone )
	{
		UTIL_Remove( this );
	}
	else
	{
		SetNextThink( gpGlobals->curtime + m_flRefireInterval );
	}
}

void CTriggerRelay::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_OnTrigger.FireOutput(pActivator, this);
	
	if ( m_spawnflags & SF_RELAY_FIREONCE )
	{
		UTIL_Remove( this );
	}

	else if( m_flRefireDuration != -1 && m_flTimeRefireDone == -1 )
	{
		// Set up to refire this target automatically
		m_TargetUseType = useType;
		m_flTargetValue = value;

		m_flTimeRefireDone = gpGlobals->curtime + m_flRefireDuration;
		SetThink( &CTriggerRelay::RefireThink );
		SetNextThink( gpGlobals->curtime + m_flRefireInterval );
	}
}


//**********************************************************
// The Multimanager Entity - when fired, will fire up to 16 targets 
// at specified times.

#define SF_MULTIMAN_THREAD		0x00000001

class CMultiManager : public CPointEntity
{
	DECLARE_CLASS( CMultiManager, CPointEntity );
public:

	bool KeyValue( const char *szKeyName, const char *szValue );
	void Spawn ( void );
	void ManagerThink ( void );
	void ManagerUse   ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

#if _DEBUG
	void ManagerReport( void );
#endif

	bool		HasTarget( string_t targetname );
	
	int ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();

	int			m_cTargets;	// the total number of targets in this manager's fire list.
	int			m_index;	// Current target
	float		m_flWait;
	EHANDLE		m_hActivator;
	float		m_startTime;// Time we started firing
	string_t	m_iTargetName	[ MAX_MULTI_TARGETS ];// list if indexes into global string array
	float		m_flTargetDelay [ MAX_MULTI_TARGETS ];// delay (in seconds) from time of manager fire to target fire

	COutputEvent m_OnTrigger;
	void InputManagerTrigger( inputdata_t &data );

private:
	bool IsClone( void ) { return (HasSpawnFlags( SF_MULTIMAN_THREAD ) ? TRUE : FALSE; }
	bool ShouldClone( void ) 
	{
		if ( IsClone() )
			return false;

		return (HasSpawnFlags( SF_MULTIMAN_THREAD ) ? TRUE : FALSE; 
	}

//	CMultiManager *Clone( void );
};
LINK_ENTITY_TO_CLASS( multi_manager, CMultiManager );

// Global Savedata for multi_manager
BEGIN_DATADESC( CMultiManager )
	DEFINE_KEYFIELD( m_flWait, FIELD_FLOAT, "wait" ),

	DEFINE_FIELD( m_cTargets, FIELD_INTEGER ),
	DEFINE_FIELD( m_index, FIELD_INTEGER ),
	DEFINE_FIELD( m_hActivator, FIELD_EHANDLE ),
	DEFINE_FIELD( m_startTime, FIELD_TIME ),
	DEFINE_ARRAY( m_iTargetName, FIELD_STRING, MAX_MULTI_TARGETS ),
	DEFINE_ARRAY( m_flTargetDelay, FIELD_FLOAT, MAX_MULTI_TARGETS ),

	// Function Pointers
	DEFINE_FUNCTION( ManagerThink ),
	DEFINE_FUNCTION( ManagerUse ),
#if _DEBUG
	DEFINE_FUNCTION( ManagerReport ),
#endif

	// Outputs
	DEFINE_OUTPUT( m_OnTrigger, "OnTrigger"),
	DEFINE_INPUTFUNC( FIELD_VOID, "Trigger", InputManagerTrigger ),
END_DATADESC()

void CMultiManager::InputManagerTrigger( inputdata_t &data )
{
	ManagerUse ( NULL, NULL, USE_TOGGLE, 0 );
}

bool CMultiManager::KeyValue( const char *szKeyName, const char *szValue )
{
	// UNDONE: Maybe this should do something like this:
	//CBaseToggle::KeyValue( pkvd );
	// if ( !pkvd->fHandled )
	// ... etc.

	if (FStrEq(szKeyName, "wait"))
	{
		m_flWait = atof(szValue);
		//pkvd->fHandled = TRUE;
		return true;
	}
	else // add this field to the target list
	{
		// this assumes that additional fields are targetnames and their values are delay values.
		if ( m_cTargets < MAX_MULTI_TARGETS )
		{
			char tmp[128];

			UTIL_StripToken( szKeyName, tmp );
			m_iTargetName [ m_cTargets ] = AllocPooledString( tmp );
			m_flTargetDelay [ m_cTargets ] = atof (szValue);
			m_cTargets++;
			//pkvd->fHandled = TRUE;
			return true;
		}
	}

	return true;
}

void CMultiManager :: Spawn( void )
{
	SetSolid( SOLID_NONE );
	SetUse ( &CMultiManager::ManagerUse );
	SetThink ( &CMultiManager::ManagerThink );

	// Sort targets
	// Quick and dirty bubble sort
	int swapped = 1;

	while ( swapped )
	{
		swapped = 0;
		for ( int i = 1; i < m_cTargets; i++ )
		{
			if ( m_flTargetDelay[i] < m_flTargetDelay[i-1] )
			{
				// Swap out of order elements
				string_t name = m_iTargetName[i];
				float delay = m_flTargetDelay[i];
				m_iTargetName[i] = m_iTargetName[i-1];
				m_flTargetDelay[i] = m_flTargetDelay[i-1];
				m_iTargetName[i-1] = name;
				m_flTargetDelay[i-1] = delay;
				swapped = 1;
			}
		}
	}
//	UTIL_Relink( this );
}

bool CMultiManager::HasTarget( string_t targetname )
{ 
	for ( int i = 0; i < m_cTargets; i++ )
		if ( FStrEq(STRING(targetname), STRING(m_iTargetName[i])) )
			return true;
	
	return false;
}

// Designers were using this to fire targets that may or may not exist -- 
// so I changed it to use the standard target fire code, made it a little simpler.
void CMultiManager :: ManagerThink ( void )
{
	float	time;

	time = gpGlobals->curtime - m_startTime;

	while ( m_index < m_cTargets && m_flTargetDelay[ m_index ] <= time )
	{
		FireTargets( STRING( m_iTargetName[ m_index ] ), m_hActivator, this, USE_TOGGLE, 0 );
		m_index++;
	}

	if ( m_index >= m_cTargets )// have we fired all targets?
	{
		SetThink( NULL );
		if ( IsClone() )
		{
			UTIL_Remove( this );
			return;
		}
		SetUse ( &CMultiManager::ManagerUse );// allow manager re-use 
	}
	else
	{
		SetNextThink( m_startTime + m_flTargetDelay[ m_index ] );
	}
}

#if 0
CMultiManager *CMultiManager::Clone( void )
{
	CMultiManager *pMulti = GetClassPtr( (CMultiManager *)NULL );

	edict_t *pEdict = pMulti->pev->pContainingEntity;
	memcpy( pMulti->pev, pev, sizeof(*pev) );
	pMulti->pev->pContainingEntity = pEdict;

	pMulti->pev->spawnflags |= SF_MULTIMAN_CLONE;
	pMulti->m_cTargets = m_cTargets;
	memcpy( pMulti->m_iTargetName, m_iTargetName, sizeof( m_iTargetName ) );
	memcpy( pMulti->m_flTargetDelay, m_flTargetDelay, sizeof( m_flTargetDelay ) );

	return pMulti;
}
#endif

// The USE function builds the time table and starts the entity thinking.
void CMultiManager :: ManagerUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// In multiplayer games, clone the MM and execute in the clone (like a thread)
	// to allow multiple players to trigger the same multimanager
#if 0
	if ( ShouldClone() )
	{
		CMultiManager *pClone = Clone();
		pClone->ManagerUse( pActivator, pCaller, useType, value );
		return;
	}
#endif

	m_hActivator = pActivator;
	m_index = 0;
	m_startTime = gpGlobals->curtime;

	m_OnTrigger.FireOutput(pActivator, this);
	
	// Calculate the time to re-enable the multimanager - just after the last output is fired.
	// dvsents2: need to disable multimanager until last output is fired
	//m_fEnableTime = gpGlobals->curtime + m_OnTrigger.GetMaxDelay();

	SetUse( NULL );// disable use until all targets have fired

	SetThink ( &CMultiManager::ManagerThink );
	SetNextThink( gpGlobals->curtime );
}

#if _DEBUG
void CMultiManager :: ManagerReport ( void )
{
	int	cIndex;

	for ( cIndex = 0 ; cIndex < m_cTargets ; cIndex++ )
	{
		Msg( "%s %f\n", STRING(m_iTargetName[cIndex]), m_flTargetDelay[cIndex] );
	}
}
#endif


//
// Render parameters trigger
//
// This entity will copy its render parameters (renderfx, rendermode, rendercolor, renderamt)
// to its targets when triggered.
//

// Flags to indicate masking off various render parameters that are normally copied to the targets
#define SF_RENDER_MASKFX	(1<<0)
#define SF_RENDER_MASKAMT	(1<<1)
#define SF_RENDER_MASKMODE	(1<<2)
#define SF_RENDER_MASKCOLOR	(1<<3)

class CRenderFxManager : public CBaseEntity
{
	DECLARE_CLASS( CRenderFxManager, CBaseEntity );
public:
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	// Input handlers.
	void	InputActivate( inputdata_t &inputdata );

	DECLARE_DATADESC();
};

BEGIN_DATADESC( CRenderFxManager )
	DEFINE_INPUTFUNC( FIELD_VOID, "Activate", InputActivate ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( env_render, CRenderFxManager );

void CRenderFxManager::Spawn( void )
{
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );
	AddEffects( EF_NODRAW );
}

void CRenderFxManager::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_target != NULL_STRING )
	{
		CBaseEntity *pEntity = NULL;
		while ( ( pEntity = gEntList.FindEntityByName( pEntity, STRING( m_target ), NULL ) ) != NULL )
		{
			if ( !HasSpawnFlags( SF_RENDER_MASKFX ) )
				pEntity->m_nRenderFX = m_nRenderFX;
			if ( !HasSpawnFlags( SF_RENDER_MASKAMT ) )
				pEntity->SetRenderColorA( GetRenderColor().a );
			if ( !HasSpawnFlags( SF_RENDER_MASKMODE ) )
				pEntity->m_nRenderMode = m_nRenderMode;
			if ( !HasSpawnFlags( SF_RENDER_MASKCOLOR ) )
				pEntity->m_clrRender = m_clrRender;
		}
	}
}

void CRenderFxManager::InputActivate( inputdata_t &inputdata )
{
	Use( inputdata.pActivator, inputdata.pCaller, USE_ON, 0 );
}


// Link env_sound to soundscape system
LINK_ENTITY_TO_CLASS( env_sound, CEnvSoundscape );


#define SF_ENDSECTION_USEONLY		0x0001
class CTriggerEndSection : public CBaseEntity
{
	DECLARE_CLASS( CTriggerEndSection, CBaseEntity );

public:
	void Spawn( void );
	void InputEndSection( inputdata_t &data  );
	
	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( trigger_endsection, CTriggerEndSection );

BEGIN_DATADESC( CTriggerEndSection )
	DEFINE_INPUTFUNC( FIELD_VOID, "EndSection", InputEndSection ),
END_DATADESC()

void CTriggerEndSection::Spawn( void )
{
	if ( gpGlobals->deathmatch )
	{
		UTIL_Remove( this );
		return;
	}
}

void CTriggerEndSection::InputEndSection( inputdata_t &data )
{
	CBaseEntity *pPlayer = UTIL_PlayerByIndex( 1 );

	if ( pPlayer )
	{
		//HACKY MCHACK - This works, but it's nasty. Alfred is going to fix a
		//bug in gameui that prevents you from dropping to the main menu after
		// calling disconnect.
		 engine->ClientCommand ( pPlayer->edict(), "toggleconsole;disconnect\n");
	}

	UTIL_Remove( this );
}
