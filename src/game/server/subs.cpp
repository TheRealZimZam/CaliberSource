//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Frequently used global functions.
//
// $NoKeywords: $
//=============================================================================//


#include "cbase.h"
#include "doors.h"
#include "entitylist.h"
#include "globals.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Landmark class
void CPointEntity::Spawn( void )
{
	SetSolid( SOLID_NONE );
//	UTIL_SetSize(this, vec3_origin, vec3_origin);
}


class CNullEntity : public CBaseEntity
{
public:
	DECLARE_CLASS( CNullEntity, CBaseEntity );

	void Spawn( void );
};


// Null Entity, remove on startup
void CNullEntity::Spawn( void )
{
	UTIL_Remove( this );
}
LINK_ENTITY_TO_CLASS(info_null,CNullEntity);

class CBaseDMStart : public CPointEntity
{
public:
	DECLARE_CLASS( CBaseDMStart, CPointEntity );

	bool IsTriggered( CBaseEntity *pEntity );

	DECLARE_DATADESC();

	string_t m_Master;

private:
};

BEGIN_DATADESC( CBaseDMStart )

	DEFINE_KEYFIELD( m_Master, FIELD_STRING, "master" ),

END_DATADESC()


// These are the new entry points to entities. 
#if !defined( SDK_DLL )		//Tony; don't use the normal info_player_deathmatch in the SDK, use the custom one.
LINK_ENTITY_TO_CLASS(info_player_deathmatch,CBaseDMStart);
#endif

LINK_ENTITY_TO_CLASS(info_player_start,CPointEntity);
LINK_ENTITY_TO_CLASS(info_landmark,CPointEntity);

bool CBaseDMStart::IsTriggered( CBaseEntity *pEntity )
{
	bool master = UTIL_IsMasterTriggered( m_Master, pEntity );

	return master;
}


// Convenient way to delay removing oneself
void CBaseEntity::SUB_Remove( void )
{
	if (m_iHealth > 0)
	{
		// this situation can screw up NPCs who can't tell their entity pointers are invalid.
		m_iHealth = 0;
#ifdef _DEBUG
		DevWarning( 2, "SUB_Remove called on entity with health > 0\n");
#endif
	}

	UTIL_Remove( this );
}


// Convenient way to explicitly do nothing (passed to functions that require a method)
void CBaseEntity::SUB_DoNothing( void )
{
}

//
// fade out - slowly fades a entity out, then removes it.
//
// DON'T USE ME FOR GIBS AND STUFF IN MULTIPLAYER!
// SET A FUTURE THINK AND A RENDERMODE!!
#define MIN_CORPSE_FADE_TIME		10.0
#define	MIN_CORPSE_FADE_DIST		256.0
#define	MAX_CORPSE_FADE_DIST		1500.0

bool CBaseEntity::SUB_AllowedToFade( void )
{
	// players CANNOT be faded out and/or util_remove'd!
	if ( IsPlayer() )
		return false;

	if( VPhysicsGetObject() )
	{
		if( VPhysicsGetObject()->GetGameFlags() & FVPHYSICS_PLAYER_HELD || GetEFlags() & EFL_IS_BEING_LIFTED_BY_BARNACLE )
			return false;
	}

	// on Xbox, allow these to fade out even if a player is looking
#ifndef _XBOX
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( pPlayer && pPlayer->FInViewCone( this ) )
		return false;
#endif

	return true;
}

void CBaseEntity::SUB_StartFadeOut( float delay, bool notSolid )
{
	SetThink( &CBaseEntity::SUB_FadeOut );
	SetNextThink( gpGlobals->curtime + delay );
	SetRenderColorA( 255 );
	m_nRenderMode = kRenderNormal;

	if ( notSolid )
	{
		AddSolidFlags( FSOLID_NOT_SOLID );
		SetLocalAngularVelocity( vec3_angle );
	}
}

void CBaseEntity::SUB_StartFadeOutInstant()
{
	SUB_StartFadeOut( 0, true );
}

//-----------------------------------------------------------------------------
// Purpose: Vanish when players aren't looking
//-----------------------------------------------------------------------------
void CBaseEntity::SUB_Vanish( void )
{
	//Always think again next frame
	SetNextThink( gpGlobals->curtime + 0.1f );

	CBasePlayer *pPlayer;

	//Get all players
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		//Get the next client
		if ( ( pPlayer = UTIL_PlayerByIndex( i ) ) != NULL )
		{
			Vector corpseDir = (GetAbsOrigin() - pPlayer->WorldSpaceCenter() );

			float flDistSqr = corpseDir.LengthSqr();
			//If the player is close enough, don't fade out
			if ( flDistSqr < (MIN_CORPSE_FADE_DIST*MIN_CORPSE_FADE_DIST) )
				return;

			// If the player's far enough away, we don't care about looking at it
			if ( flDistSqr < (MAX_CORPSE_FADE_DIST*MAX_CORPSE_FADE_DIST) )
			{
				VectorNormalize( corpseDir );

				Vector	plForward;
				pPlayer->EyeVectors( &plForward );

				float dot = plForward.Dot( corpseDir );

				if ( dot > 0.0f )
					return;
			}
		}
	}

	//If we're here, then we can vanish safely
	m_iHealth = 0;
	SetThink( &CBaseEntity::SUB_Remove );
}

//-----------------------------------------------------------------------------
// Purpose: Fade out slowly
//-----------------------------------------------------------------------------
void CBaseEntity::SUB_PerformFadeOut( void )
{
	float dt = gpGlobals->frametime;
	if ( dt > 0.1f )
	{
		dt = 0.1f;
	}
	m_nRenderMode = kRenderTransTexture;
	int speed = max(1,256*dt); // fade out over 1 second
	SetRenderColorA( UTIL_Approach( 0, m_clrRender->a, speed ) );
}

void CBaseEntity::SUB_FadeOut( void )
{
	if ( SUB_AllowedToFade() == false )
	{
		SetNextThink( gpGlobals->curtime + 1 );
		SetRenderColorA( 255 );
		return;
	}

	SUB_PerformFadeOut();

	if ( m_clrRender->a == 0 )
		SUB_Remove();
	else
		SetNextThink( gpGlobals->curtime );
}


/*
==============================
SUB_UseTargets

If self.delay is set, a DelayedUse entity will be created that will actually
do the SUB_UseTargets after that many seconds have passed.

Removes all entities with a targetname that match self.killtarget,
and removes them, so some events can remove other triggers.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function (if they have one)

==============================
*/
void CBaseEntity::SUB_UseTargets( CBaseEntity *pActivator, USE_TYPE useType, float value )
{
	// fire targets
	if (m_target != NULL_STRING)
	{
		FireTargets( STRING(m_target), pActivator, this, useType, value );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Finds all active entities with the given targetname and calls their
//			'Use' function.
// Input  : targetName - Target name to search for.
//			pActivator - 
//			pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void FireTargets( const char *targetName, CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pTarget = NULL;
	if ( !targetName || !targetName[0] )
		return;

	DevMsg( 2, "Firing: (%s)\n", targetName );

	for (;;)
	{
		CBaseEntity *pSearchingEntity = pActivator;
		pTarget = gEntList.FindEntityByName( pTarget, targetName, pSearchingEntity, pActivator, pCaller );
		if ( !pTarget )
			break;

		if (!pTarget->IsMarkedForDeletion() )	// Don't use dying ents
		{
			DevMsg( 2, "[%03d] Found: %s, firing (%s)\n", gpGlobals->tickcount%1000, pTarget->GetDebugName(), targetName );
			pTarget->Use( pActivator, pCaller, useType, value );
		}
	}
}

enum togglemovetypes_t
{
	MOVE_TOGGLE_NONE = 0,
	MOVE_TOGGLE_LINEAR = 1,
	MOVE_TOGGLE_ANGULAR = 2,
};

// Global Savedata for Toggle
BEGIN_DATADESC( CBaseToggle )

	DEFINE_FIELD( m_toggle_state, FIELD_INTEGER ),
	DEFINE_FIELD( m_flMoveDistance, FIELD_FLOAT ),
	DEFINE_FIELD( m_flWait, FIELD_FLOAT ),
	DEFINE_FIELD( m_flLip, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecPosition1, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecPosition2, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecMoveAng, FIELD_VECTOR ),		// UNDONE: Position could go through transition, but also angle?
	DEFINE_FIELD( m_vecAngle1, FIELD_VECTOR ),		// UNDONE: Position could go through transition, but also angle?
	DEFINE_FIELD( m_vecAngle2, FIELD_VECTOR ),		// UNDONE: Position could go through transition, but also angle?
	DEFINE_FIELD( m_flHeight, FIELD_FLOAT ),
	DEFINE_FIELD( m_hActivator, FIELD_EHANDLE ),
	DEFINE_FIELD( m_vecFinalDest, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecFinalAngle, FIELD_VECTOR ),
	DEFINE_FIELD( m_sMaster, FIELD_STRING),
	DEFINE_FIELD( m_movementType, FIELD_INTEGER ),	// Linear or angular movement? (togglemovetypes_t)

END_DATADESC()


CBaseToggle::CBaseToggle()
{
#ifdef _DEBUG
	// necessary since in debug, we initialize vectors to NAN for debugging
	m_vecPosition1.Init();
	m_vecPosition2.Init();
	m_vecAngle1.Init();
	m_vecAngle2.Init();
	m_vecFinalDest.Init();
	m_vecFinalAngle.Init();
#endif
}

bool CBaseToggle::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "lip"))
	{
		m_flLip = atof(szValue);
	}
	else if (FStrEq(szKeyName, "wait"))
	{
		m_flWait = atof(szValue);
	}
	else if (FStrEq(szKeyName, "master"))
	{
		m_sMaster = AllocPooledString(szValue);
	}
	else if (FStrEq(szKeyName, "distance"))
	{
		m_flMoveDistance = atof(szValue);
	}
	else
		return BaseClass::KeyValue( szKeyName, szValue );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Calculate m_vecVelocity and m_flNextThink to reach vecDest from
//			GetOrigin() traveling at flSpeed.
// Input  : Vector	vecDest - 
//			flSpeed - 
//-----------------------------------------------------------------------------
void CBaseToggle::LinearMove( const Vector &vecDest, float flSpeed )
{
	ASSERTSZ(flSpeed != 0, "LinearMove:  no speed is defined!");
	
	m_vecFinalDest = vecDest;

	m_movementType = MOVE_TOGGLE_LINEAR;
	// Already there?
	if (vecDest == GetLocalOrigin())
	{
		MoveDone();
		return;
	}
		
	// set destdelta to the vector needed to move
	Vector vecDestDelta = vecDest - GetLocalOrigin();
	
	// divide vector length by speed to get time to reach dest
	float flTravelTime = vecDestDelta.Length() / flSpeed;

	// set m_flNextThink to trigger a call to LinearMoveDone when dest is reached
	SetMoveDoneTime( flTravelTime );

	// scale the destdelta vector by the time spent traveling to get velocity
	SetLocalVelocity( vecDestDelta / flTravelTime );
}


void CBaseToggle::MoveDone( void )
{
	switch ( m_movementType )
	{
	case MOVE_TOGGLE_LINEAR:
		LinearMoveDone();
		break;
	case MOVE_TOGGLE_ANGULAR:
		AngularMoveDone();
		break;
	}
	m_movementType = MOVE_TOGGLE_NONE;
	BaseClass::MoveDone();
}
//-----------------------------------------------------------------------------
// Purpose: After moving, set origin to exact final destination, call "move done" function.
//-----------------------------------------------------------------------------
void CBaseToggle::LinearMoveDone( void )
{
	UTIL_SetOrigin( this, m_vecFinalDest);
	SetAbsVelocity( vec3_origin );
	SetMoveDoneTime( -1 );
}


// DVS TODO: obselete, remove?
bool CBaseToggle::IsLockedByMaster( void )
{
	if (m_sMaster != NULL_STRING && !UTIL_IsMasterTriggered(m_sMaster, m_hActivator))
		return true;
	else
		return false;
}


//-----------------------------------------------------------------------------
// Purpose: Calculate m_vecVelocity and m_flNextThink to reach vecDest from
//			GetLocalOrigin() traveling at flSpeed. Just like LinearMove, but rotational.
// Input  : vecDestAngle - 
//			flSpeed - 
//-----------------------------------------------------------------------------
void CBaseToggle::AngularMove( const QAngle &vecDestAngle, float flSpeed )
{
	ASSERTSZ(flSpeed != 0, "AngularMove:  no speed is defined!");
	
	m_vecFinalAngle = vecDestAngle;

	m_movementType = MOVE_TOGGLE_ANGULAR;
	// Already there?
	if (vecDestAngle == GetLocalAngles())
	{
		MoveDone();
		return;
	}
	
	// set destdelta to the vector needed to move
	QAngle vecDestDelta = vecDestAngle - GetLocalAngles();
	
	// divide by speed to get time to reach dest
	float flTravelTime = vecDestDelta.Length() / flSpeed;

	const float MinTravelTime = 0.01f;
	if ( flTravelTime < MinTravelTime )
	{
		// If we only travel for a short time, we can fail WillSimulateGamePhysics()
		flTravelTime = MinTravelTime;
		flSpeed = vecDestDelta.Length() / flTravelTime;
	}

	// set m_flNextThink to trigger a call to AngularMoveDone when dest is reached
	SetMoveDoneTime( flTravelTime );

	// scale the destdelta vector by the time spent traveling to get velocity
	SetLocalAngularVelocity( vecDestDelta * (1.0 / flTravelTime) );
}


//-----------------------------------------------------------------------------
// Purpose: After rotating, set angle to exact final angle, call "move done" function.
//-----------------------------------------------------------------------------
void CBaseToggle::AngularMoveDone( void )
{
	SetLocalAngles( m_vecFinalAngle );
	SetLocalAngularVelocity( vec3_angle );
	SetMoveDoneTime( -1 );
}


float CBaseToggle::AxisValue( int flags, const QAngle &angles )
{
	if ( FBitSet(flags, SF_DOOR_ROTATE_ROLL) )
		return angles.z;
	if ( FBitSet(flags, SF_DOOR_ROTATE_PITCH) )
		return angles.x;

	return angles.y;
}


void CBaseToggle::AxisDir( void )
{
	if ( m_spawnflags & SF_DOOR_ROTATE_ROLL )
		m_vecMoveAng = QAngle( 0, 0, 1 );	// angles are roll
	else if ( m_spawnflags & SF_DOOR_ROTATE_PITCH )
		m_vecMoveAng = QAngle( 1, 0, 0 );	// angles are pitch
	else
		m_vecMoveAng = QAngle( 0, 1, 0 );		// angles are yaw
}


float CBaseToggle::AxisDelta( int flags, const QAngle &angle1, const QAngle &angle2 )
{
	// UNDONE: Use AngleDistance() here?
	if ( FBitSet (flags, SF_DOOR_ROTATE_ROLL) )
		return angle1.z - angle2.z;
	
	if ( FBitSet (flags, SF_DOOR_ROTATE_PITCH) )
		return angle1.x - angle2.x;

	return angle1.y - angle2.y;
}


