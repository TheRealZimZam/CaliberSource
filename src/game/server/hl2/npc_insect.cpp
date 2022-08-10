//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Rats, bugs, and crabs oh my! - Insects n' other small varmin
//
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "game.h"
#include "ai_basenpc.h"
#include "ai_hull.h"
#include "ai_hint.h"
#include "ai_motor.h"
#include "ai_navigator.h"
#include "ai_route.h"
#include "ai_senses.h"
#include "hl2_shareddefs.h"
#include "npcevent.h"
#include "gib.h"
#include "ai_interactions.h"
#include "ndebugoverlay.h"
#include "soundent.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "movevars_shared.h"
#include "ai_moveprobe.h"

#define		ROACH_IDLE				0
#define		ROACH_BORED				1
#define		ROACH_SCARED_BY_ENT		2
#define		ROACH_SCARED_BY_LIGHT	3
#define		ROACH_SMELL_FOOD		4
#define		ROACH_EAT				5

class CNPC_Insect : public CAI_BaseNPC
{
	DECLARE_CLASS( CNPC_Insect, CAI_BaseNPC );
	
public:

	void Spawn( void );
	void Precache( void );
	float MaxYawSpeed( void );

//	DEFINE_CUSTOM_AI;
//	DECLARE_DATADESC();

	void NPCThink ( void );
	void PickNewDest ( int iCondition );
	void Look ( int iDistance );
	void Move ( float flInterval );

	Class_T  Classify( void ) { return CLASS_EARTH_FAUNA; }

	void Touch ( CBaseEntity *pOther );

	void Event_Killed( const CTakeDamageInfo &info );
	int GetSoundInterests ( void );

	void Eat( float flFullDuration );
	bool ShouldEat( void );

	bool ShouldGib( const CTakeDamageInfo &info ) { return false; }

	float m_flLastLightLevel;
	float m_flNextSmellTime;
		
	// UNDONE: These don't necessarily need to be save/restored, but if we add more data, it may
	bool m_fLightHacked;
	int m_iMode;

	float m_flHungryTime;
	// -----------------------------
};

//-----------------------------------------------------------------------------
// Purpose: Rat. Roach with a different model.
//-----------------------------------------------------------------------------
class CNPC_Rat : public CNPC_Insect
{
	DECLARE_CLASS( CNPC_Rat, CNPC_Insect );

public:
	void Precache( void );
	void Spawn( void )
	{
		BaseClass::Spawn();

		SetModelName( AllocPooledString("models/rat.mdl") );
		SetBloodColor( BLOOD_COLOR_RED );
	}

	void Event_Killed( const CTakeDamageInfo &info );
};

LINK_ENTITY_TO_CLASS( npc_cockroach, CNPC_Insect );
#ifndef HL1_DLL
LINK_ENTITY_TO_CLASS( monster_cockroach, CNPC_Insect );
#endif
LINK_ENTITY_TO_CLASS( npc_roach, CNPC_Insect );
LINK_ENTITY_TO_CLASS( npc_rat, CNPC_Rat );

//BEGIN_DATADESC( CNPC_Insect )

//	DEFINE_FUNCTION( CNPC_Insect, RoachTouch ),
	
//END_DATADESC()


//=========================================================
// Spawn
//=========================================================
void CNPC_Insect::Spawn()
{
	Precache( );

	BaseClass::Spawn();

	SetModelName( AllocPooledString( "models/roach.mdl" ) );
	UTIL_SetSize( this, Vector( -1, -1, 0 ), Vector( 1, 1, 2 ) );

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );

	SetMoveType( MOVETYPE_STEP );
	SetBloodColor( BLOOD_COLOR_YELLOW );
	ClearEffects();

	m_iHealth			= 1;
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_NPCState			= NPC_STATE_NONE;

	SetRenderColor( 255, 255, 255, 255 );

	NPCInit();
	SetActivity ( ACT_IDLE );

	SetViewOffset ( Vector ( 0, 0, 1 ) );// position of the eyes relative to monster's origin.
	m_takedamage		= DAMAGE_YES;
	m_fLightHacked		= FALSE;
	m_flLastLightLevel	= -1;
	m_iMode				= ROACH_IDLE;
	m_flNextSmellTime	= gpGlobals->curtime;

	AddEffects( EF_NOSHADOW );
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_Insect::Precache()
{
	PrecacheModel("models/roach.mdl");

	PrecacheScriptSound( "npc_roach.die" );
	PrecacheScriptSound( "npc_roach.walk" );
	PrecacheScriptSound( "npc_roach.smash" );
}

float CNPC_Insect::MaxYawSpeed( void )
{
	return 120.0f;
}

void CNPC_Insect::Eat( float flFullDuration )
{
	m_flHungryTime = gpGlobals->curtime + flFullDuration;
}

bool CNPC_Insect::ShouldEat( void )
{
	if ( m_flHungryTime > gpGlobals->curtime )
	{
		return false;
	}

	return true;
}

//=========================================================
// MonsterThink, overridden for roaches.
//=========================================================
void CNPC_Insect::NPCThink( void )
{
	if ( FNullEnt( UTIL_FindClientInPVS( edict() ) ) )
		SetNextThink( gpGlobals->curtime + random->RandomFloat( 1.0f , 1.5f ) );
	else
		SetNextThink( gpGlobals->curtime + 0.1f );// keep monster thinking

	float flInterval = gpGlobals->curtime - GetLastThink();

	StudioFrameAdvance( ); // animate

	if ( !m_fLightHacked )
	{
		// if light value hasn't been collection for the first time yet, 
		// suspend the creature for a second so the world finishes spawning, then we'll collect the light level.
		SetNextThink( gpGlobals->curtime + 1 );
		m_fLightHacked = TRUE;
		return;
	}
	else if ( m_flLastLightLevel < 0 )
	{
		// collect light level for the first time, now that all of the lightmaps in the roach's area have been calculated.
		m_flLastLightLevel = 0;
	}

	switch ( m_iMode )
	{
	case ROACH_IDLE:
	case ROACH_EAT:
		{
			// if not moving, sample environment to see if anything scary is around. Do a radius search 'look' at random.
			if ( random->RandomInt( 0, 3 ) == 1 )
			{
				Look( 150 );

				if ( HasCondition( COND_SEE_FEAR ) )
				{
					// if see something scary
					//ALERT ( at_aiconsole, "Scared\n" );
					Eat( 30 +  ( random->RandomInt( 0, 14 ) ) );// roach will ignore food for 30 to 45 seconds
					PickNewDest( ROACH_SCARED_BY_ENT );
					SetActivity( ACT_RUN );
				}
				else if ( random->RandomInt( 0,149 ) == 1 )
				{
					// if roach doesn't see anything, there's still a chance that it will move. (boredom)
					//ALERT ( at_aiconsole, "Bored\n" );
					PickNewDest( ROACH_BORED );
					SetActivity( ACT_WALK );

					if ( m_iMode == ROACH_EAT )
					{
						// roach will ignore food for 30 to 45 seconds if it got bored while eating. 
						Eat( 30 +  ( random->RandomInt(0,14) ) );
					}
				}
			}
	
			// don't do this stuff if eating!
			if ( m_iMode == ROACH_IDLE )
			{
				if ( ShouldEat() )
				{
					GetSenses()->Listen();
				//	GetSenses()->PerformSensing();
				}

				if ( 0 > m_flLastLightLevel )
				{
					// someone turned on lights!
					//ALERT ( at_console, "Lights!\n" );
					PickNewDest( ROACH_SCARED_BY_LIGHT );
					SetActivity( ACT_RUN );
				}
				else if ( HasCondition( COND_SMELL ) )
				{
					CSound *pSound = GetLoudestSoundOfType( ALL_SOUNDS );

					// roach smells food and is just standing around. Go to food unless food isn't on same z-plane.
					if ( pSound && abs( pSound->GetSoundOrigin().z - GetAbsOrigin().z ) <= 3 )
					{
						PickNewDest( ROACH_SMELL_FOOD );
						SetActivity( ACT_WALK );
					}
				}
			}

			break;
		}
	case ROACH_SCARED_BY_LIGHT:
		{
			// if roach was scared by light, then stop if we're over a spot at least as dark as where we started!
			if ( 0 <= m_flLastLightLevel )
			{
				SetActivity( ACT_IDLE );
				m_flLastLightLevel = 0;// make this our new light level.
			}
			break;
		}
	}
	
	if ( GetActivity() != ACT_IDLE )
	{
		Move( flInterval );
	}
}

void CNPC_Insect :: PickNewDest ( int iCondition )
{
	Vector	vecNewDir;
	Vector	vecDest;
	float	flDist;

	m_iMode = iCondition;

    GetNavigator()->ClearGoal();

	if ( m_iMode == ROACH_SMELL_FOOD )
	{
		// find the food and go there.
		CSound *pSound = GetLoudestSoundOfType( ALL_SOUNDS );

		if ( pSound )
		{
			GetNavigator()->SetRandomGoal( 3 - random->RandomInt( 0,5 ) );
			return;
		}
	}
	do 
	{
		// picks a random spot, requiring that it be at least 128 units away
		// else, the roach will pick a spot too close to itself and run in 
		// circles. this is a hack but buys me time to work on the real monsters.
		vecNewDir.x = random->RandomInt( -1, 1 );
		vecNewDir.y = random->RandomInt( -1, 1 );
		flDist		= 256 + ( random->RandomInt(0,255) );
		vecDest = GetAbsOrigin() + vecNewDir * flDist;

	} while ( ( vecDest - GetAbsOrigin() ).Length2D() < 128 );

	Vector vecLocation;

	vecLocation.x = vecDest.x;
	vecLocation.y = vecDest.y;
	vecLocation.z = GetAbsOrigin().z;
	
	AI_NavGoal_t goal( GOALTYPE_LOCATION, vecLocation, ACT_WALK );

	GetNavigator()->SetGoal( goal );

	if ( random->RandomInt( 0, 9 ) == 1 )
	{
		// every once in a while, a roach will play a skitter sound when they decide to run
		CPASAttenuationFilter filter( this );
		EmitSound( "npc_roach.walk" );
	}
}

//=========================================================
// Look - overriden for the roach, which can virtually see 
// 360 degrees.
//=========================================================
void CNPC_Insect::Look( int iDistance )
{
	CBaseEntity	*pSightEnt = NULL;// the current visible entity that we're dealing with

	// DON'T let visibility information from last frame sit around!
	ClearCondition( COND_SEE_HATE | COND_SEE_DISLIKE |  COND_SEE_ENEMY | COND_SEE_FEAR );

	// don't let monsters outside of the player's PVS act up, or most of the interesting
	// things will happen before the player gets there!
	if ( FNullEnt( UTIL_FindClientInPVS( edict() ) ) )
	{
		return;
	}
	
	// Does sphere also limit itself to PVS?
	// Examine all entities within a reasonable radius
	// !!!PERFORMANCE - let's trivially reject the ent list before radius searching!

	for ( CEntitySphereQuery sphere( GetAbsOrigin(), iDistance ); ( pSightEnt = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		// only consider ents that can be damaged. !!!temporarily only considering other monsters and clients
		if ( pSightEnt->IsPlayer() || FBitSet ( pSightEnt->GetFlags(), FL_NPC ) )
		{
			if ( /*FVisible( pSightEnt ) &&*/ !FBitSet( pSightEnt->GetFlags(), FL_NOTARGET ) && pSightEnt->m_iHealth > 0 )
			{
				// don't add the Enemy's relationship to the conditions. We only want to worry about conditions when
				// we see monsters other than the Enemy.
				switch ( IRelationType( pSightEnt ) )
				{
				case D_FR:		
					SetCondition( COND_SEE_FEAR );
					break;
				case D_NU:
					break;
				default:
					DevWarning( 2, "%s can't assess %s\n", GetClassname(), pSightEnt->GetClassname() );
					break;
				}
			}
		}
	}
}

//=========================================================
// roach's move function
//=========================================================
void CNPC_Insect::Move( float flInterval ) 
{
	float		flWaypointDist;
	Vector		vecApex;

	// local move to waypoint.
	flWaypointDist = ( GetNavigator()->GetGoalPos() - GetAbsOrigin() ).Length2D();
	
	GetMotor()->SetIdealYawToTargetAndUpdate( GetNavigator()->GetGoalPos() );

	float speed = 150 * flInterval;

	Vector vToTarget = GetNavigator()->GetGoalPos() - GetAbsOrigin();
	vToTarget.NormalizeInPlace();
	Vector vMovePos = vToTarget * speed;

	if ( random->RandomInt( 0,7 ) == 1 )
	{
		// randomly change direction
		PickNewDest( m_iMode );
	}

	if( !WalkMove( vMovePos, MASK_NPCSOLID ) )
	{
		PickNewDest( m_iMode );
	}

	// if the waypoint is closer than step size, then stop after next step (ok for roach to overshoot)
	if ( flWaypointDist <= m_flGroundSpeed * flInterval )
	{
		// take truncated step and stop

		SetActivity( ACT_IDLE );
		m_flLastLightLevel = 0;// this is roach's new comfortable light level

		if ( m_iMode == ROACH_SMELL_FOOD )
		{
			m_iMode = ROACH_EAT;
		}
		else
		{
			m_iMode = ROACH_IDLE;
		}
	}

	if ( random->RandomInt( 0,149 ) == 1 && m_iMode != ROACH_SCARED_BY_LIGHT && m_iMode != ROACH_SMELL_FOOD )
	{
		// random skitter while moving as long as not on a b-line to get out of light or going to food
		PickNewDest( FALSE );
	}
}

void CNPC_Insect::Touch( CBaseEntity *pOther )
{
	Vector		vecSpot;
	trace_t		tr;

	if ( pOther->GetAbsVelocity() == vec3_origin || !pOther->IsPlayer() )
	{
		return;
	}

	vecSpot = GetAbsOrigin() + Vector ( 0 , 0 , 8 );//move up a bit, and trace down.
	//UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -24 ),  ignore_monsters, ENT(pev), & tr);

	UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -24 ), MASK_ALL, this, COLLISION_GROUP_NONE, &tr);

	// This isn't really blood.  So you don't have to screen it out based on violence levels (UTIL_ShouldShowBlood())
	UTIL_DecalTrace( &tr, "YellowBlood" );

	// DMG_GENERIC because we don't want any physics force generated
	TakeDamage( CTakeDamageInfo( pOther, pOther, m_iHealth, DMG_GENERIC ) );
}

void CNPC_Insect::Event_Killed( const CTakeDamageInfo &info )
{
	RemoveSolidFlags( FSOLID_NOT_SOLID );
	
	CPASAttenuationFilter filter( this );
	
	//random sound
	if ( random->RandomInt(0,4) == 1 )
	{
		EmitSound( "npc_roach.die" );
	}
	else
	{
		EmitSound( "npc_roach.smash" );
	}
	
	CSoundEnt::InsertSound ( SOUND_WORLD, GetAbsOrigin(), 128, 1 );

	UTIL_Remove( this );
}

int CNPC_Insect::GetSoundInterests ( void) 
{
	return	SOUND_CARCASS	|
			SOUND_MEAT;
}


//-----------------------------------------------------------------------------
// Purpose: Rat. Roach with a different model.
//-----------------------------------------------------------------------------
void CNPC_Rat::Precache()
{
	engine->PrecacheModel("models/rat.mdl");

	PrecacheScriptSound( "npc_rat.die" );
	PrecacheScriptSound( "npc_roach.walk" );
}

void CNPC_Rat::Event_Killed( const CTakeDamageInfo &info )
{
	RemoveSolidFlags( FSOLID_NOT_SOLID );
	
	CPASAttenuationFilter filter( this );
	
	EmitSound( "npc_rat.die" );
	CSoundEnt::InsertSound ( SOUND_WORLD, GetAbsOrigin(), 128, 1 );

	//TODO; spawn dead model
	UTIL_Remove( this );
}
