//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Fake bot - A npc that runs around and mimics a player
// Used in SP if we ever need a player-like character, or in MP as a stopgap
// Cannot cap points or do much of anything a player can do, only for deathmatch.
// Inherits from simpletalker, for response rules support.
//
// NOTENOTE; If you really want to, you CAN use this as a baseclass, but if you're
// doing that you SHOULD just use the real bot system (if its implemented)...
//
//=============================================================================//

#include "cbase.h"
#include "ai_basenpc.h"
#include "fakebot.h"
#include "ai_senses.h"
#include "ai_tacticalservices.h"
#include "ai_interactions.h"
#include "globalstate.h"
#include "npcevent.h"
#include "gamerules.h"
#include "game.h"
#include "KeyValues.h"
#include "engine/IEngineSound.h"
#include "soundent.h"
#include "player.h"
#include "gib.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Do we actually spawn another fakebot, 
// OR just fake a respawn by resetting me and zipping me back to a spawnpoint?
#define FAKEBOT_REAL_RESPAWN 0

// For fake weapon switching, simulate a real player
#define FAKEBOT_WANTS_MELEE		1
#define FAKEBOT_SLOT1			"weapon_swing"
#define FAKEBOT_WANTS_PISTOL	2
#define FAKEBOT_SLOT2			"weapon_pistol"
#define FAKEBOT_WANTS_SMG		3
#define FAKEBOT_SLOT3			"weapon_smg"
#define FAKEBOT_WANTS_SHOTGUN	4
#define FAKEBOT_SLOT4			"weapon_shotgun"
#define FAKEBOT_WANTS_RPG		5
#define FAKEBOT_SLOT5			"weapon_rpg"


BEGIN_DATADESC( CFakeBot )
	DEFINE_FIELD( m_bPissed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_pPissedTarget, FIELD_EHANDLE ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( fakebot, CFakeBot );

//-----------------------------------------------------------------------------
// Purpose: precaches all resources this NPC needs
//-----------------------------------------------------------------------------
void CFakeBot::Precache()
{
	BaseClass::Precache();

	SetModelName( AllocPooledString( FAKEBOT_DEFAULT_MODEL ) );

	// TODO; This is where we could grab a couple of the MP models
	PrecacheModel( STRING( GetModelName() ) );

	PrecacheScriptSound( "GenericNPC.GunSound" );
	PrecacheScriptSound( "Player.Pain" );
	PrecacheScriptSound( "Player.Death" );
}

//=========================================================
// Classify - indicates this NPC's place in the 
// relationship table.
//=========================================================
Class_T	CFakeBot::Classify( void )
{
	return	CLASS_PLAYER;	//NOTENOTE; This might be dangerous... -MM
}

//=========================================================
// MaxYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
float CFakeBot::MaxYawSpeed( void )
{
	if ( m_NPCState == NPC_STATE_IDLE )
		return 45;
	else
		return 60;
}

//-----------------------------------------------------------------------------
bool CFakeBot::ShouldGib( const CTakeDamageInfo &info )
{
	// If we're being hoisted, we only want to gib when the barnacle hurts us with his bite!
	if ( IsEFlagSet( EFL_IS_BEING_LIFTED_BY_BARNACLE ) )
	{
		if ( info.GetAttacker() && info.GetAttacker()->Classify() != CLASS_BARNACLE )
			return false;

		return true;
	}

	return BaseClass::ShouldGib( info );
}

bool CFakeBot::ShouldMoveAndShoot()
{
	return true;
}

//=========================================================
// GetSoundInterests
//=========================================================
int CFakeBot::GetSoundInterests( void )
{
	// The full suite
	return	SOUND_COMBAT			|
			SOUND_WORLD				|
			SOUND_PLAYER			|
			SOUND_DANGER			|
			SOUND_PHYSICS_DANGER	|
			SOUND_BULLET_IMPACT		|
			SMELL_CARCASS			|
			SOUND_MOVE_AWAY			|
			SOUND_VEHICLE			|
			SOUND_READINESS_LOW		|
			SOUND_READINESS_MEDIUM	|
			SOUND_READINESS_HIGH	|
			SOUND_WEAPON;
}

void CFakeBot::PainSound( const CTakeDamageInfo &info )
{
	if ( gpGlobals->curtime < m_flNextPainSoundTime )
		return;

	SentenceStop();	//Stop any response rules that are playing
	EmitSound( "Player.Pain" );
	m_flNextPainSoundTime = gpGlobals->curtime + 1;
}

void CFakeBot::DeathSound( const CTakeDamageInfo &info )
{
	// Sentences don't play on dead NPCs
	SentenceStop();
	EmitSound( "Player.Death" );
}

//=========================================================
// Spawn
//=========================================================
void CFakeBot::Spawn()
{
	if ( g_pGameRules->IsMultiplayer() && fakebots.GetInt() < 1 )
	{
		// No slots
		SUB_Remove();
		return;
	}

	// TODO; Check for slots (fakebots convar is how many fakebots we actually want)
	//

	Precache();
	BaseClass::Spawn();

	SetModel( STRING( GetModelName() ) );
	if ( FStrEq( STRING( GetModelName() ), "models/player.mdl" ) || FStrEq( STRING( GetModelName() ), "models/holo.mdl" ) )
		UTIL_SetSize(this, VEC_HULL_MIN, VEC_HULL_MAX);
	else
		SetHullType(HULL_HUMAN);

	SetHullSizeNormal();
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	m_bloodColor		= BLOOD_COLOR_RED;
	m_iHealth			= 100;
	m_iMaxHealth		= m_iHealth;
	m_flFieldOfView		= 0.766;// indicates the width of this NPC's forward view cone ( as a dotproduct result )
	m_NPCState			= NPC_STATE_NONE;
//	m_iFakeTeam			= 0;
	m_bPissed			= false;
	m_pPissedTarget		= NULL;

	SetSimulatedEveryTick( true );
	SetAnimatedEveryTick( true );

	CapabilitiesAdd( bits_CAP_HUMAN_MOVEMENT_GROUP | bits_CAP_DOORS_GROUP | bits_CAP_USE );
	CapabilitiesAdd( bits_CAP_USE_WEAPONS | bits_CAP_DUCK | bits_CAP_MOVE_SHOOT | bits_CAP_STRAFE );

	NPCInit();

	// Somehow need to hook this into gamerules
#if 0
	if ( !HasSpawnFlags(SF_FAKEBOT_SOLID) )
	{
		// TODO;
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( 1 );
		if ( InSameTeam( pPlayer ) )
			PhysDisableEntityCollisions( this, pPlayer );
	}
#endif
}

#if FAKEBOT_REAL_RESPAWN
CBaseEntity* CFakeBot::Respawn( void )
{
	CBaseEntity *pNewBot = CBaseEntity::Create( GetClassname(), GetAbsOrigin(), GetLocalAngles(), GetOwnerEntity() );
	pNewBot->Wake( false );
	pNewBot->SetThink( &CAI_BaseNPC::NPCInitThink );
	pNewBot->SetNextThink( gpGlobals->curtime + 0.01f );
	return pNewBot;
}
#endif

//-----------------------------------------------------------------------------
Disposition_t CFakeBot::IRelationType( CBaseEntity *pTarget )
{
	if ( pTarget->IsPlayer() || pTarget->Classify() == CLASS_PLAYER )
	{
		if ( g_pGameRules->IsDeathmatch() )
			return D_HT;

		if ( g_pGameRules->IsTeamplay() )
		{
			int	iClient;
			for ( iClient = 1; iClient <= gpGlobals->maxClients; iClient++ )
			{
				CBaseEntity *pPlayer = UTIL_PlayerByIndex( iClient );
				if ( !InSameTeam( pPlayer ) )
					return D_HT;
				else
					return D_LI;
			}
		}
	}

	return BaseClass::IRelationType( pTarget );
}

//=========================================================
// Thinking
//=========================================================
void CFakeBot::NPCThink( void )
{
	BaseClass::NPCThink();

	if ( IsAlive() )
	{
		if ( GetFlags() & FL_DUCKING )
			SetCollisionBounds( VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
		else
			SetCollisionBounds( VEC_HULL_MIN, VEC_HULL_MAX );

		// TODO; This is where we would decide what weapon we want
	}
}

void CFakeBot::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();
}

void CFakeBot::GatherConditions( void )
{
	BaseClass::GatherConditions();
}

#ifdef HL2_DLL
//extern ConVar hl2_playertalk;
void CFakeBot::OnKilledNPC( CBaseCombatCharacter *pKilled )
{
	// Spew some liners/taunts, like the player
//	if ( hl2_playertalk.GetBool() )
//	{
//		// Use special taunts for enemy players
//		if ( pKilled->IsPlayer() )
//		{
//			
//		}
//		else
//		{
//			// Standard NPC, just use the stock frag lines
//
//		}
//	}
	BaseClass::OnKilledNPC( pKilled );
}
#endif

//=========================================================
// Doing
//=========================================================
void CFakeBot::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();

	if ( m_NPCState == NPC_STATE_IDLE )
	{
		// Almost anything can interrupt me when in idle
		SetCustomInterruptCondition( COND_LIGHT_DAMAGE );
		SetCustomInterruptCondition( COND_HEAVY_DAMAGE );
		SetCustomInterruptCondition( COND_SEE_ENEMY );
		SetCustomInterruptCondition( COND_HEAR_COMBAT );
		SetCustomInterruptCondition( COND_HEAR_DANGER );
		SetCustomInterruptCondition( COND_HEAR_MOVE_AWAY );
	}
}

int CFakeBot::SelectSchedule( void )
{
	switch( m_NPCState )
	{
		case NPC_STATE_DEAD:
		case NPC_STATE_PLAYDEAD:
		case NPC_STATE_PRONE:
		case NPC_STATE_SCRIPT:
			return BaseClass::SelectSchedule();
		break;

		case NPC_STATE_IDLE:
			// If its danger in idle, just move away
			if ( HasCondition( COND_HEAR_DANGER ) )
				return SCHED_MOVE_AWAY;

			// Always place an emphasis on reloading
			if ( HasCondition( COND_NO_PRIMARY_AMMO ) || HasCondition( COND_NO_SECONDARY_AMMO ) )
				return SCHED_HIDE_AND_RELOAD;

			if ( AI_IsSinglePlayer() )
			{
				// In SP, we should follow/tag along with a friendly player if we're idle
				if ( IRelationType( UTIL_GetLocalPlayer() ) >= D_LI )
				{
					m_FollowBehavior.SetFollowTarget( UTIL_GetLocalPlayer() );
					if ( m_FollowBehavior.CanSelectSchedule() )
					{
						DeferSchedulingToBehavior( &m_FollowBehavior );
						return BaseClass::SelectSchedule();
					}
					else
						// Cant follow, for some odd reason
						return SCHED_FAKEBOT_IDLE;
				}
			}
			else if ( g_pGameRules->IsDeathmatch() )
				// If its Deathmatch, Start hunting for enemies
				return SCHED_IDLE_WANDER;
			else if ( g_pGameRules->IsTeamplay() )
				// If its teamplay, start running towards the objective, we'll get interrupted when we get an enemy
				return SCHED_FORCED_GO_RUN;	//TEMPTEMP

			// Else choose a spot and mess about
			return SCHED_FAKEBOT_IDLE;
		break;

		case NPC_STATE_ALERT:
		case NPC_STATE_COMBAT:
		{
			// Turn off following if its on
			if ( m_FollowBehavior.IsRunning() )
			{
				m_FollowBehavior.SetFollowTarget( NULL ); 
				DeferSchedulingToBehavior( NULL );
			}

			if ( HasCondition( COND_HEAR_DANGER ) )
				return SCHED_TAKE_COVER_FROM_BEST_SOUND;

			if ( HasCondition( COND_NO_PRIMARY_AMMO ) || HasCondition( COND_NO_SECONDARY_AMMO ) )
				return SCHED_HIDE_AND_RELOAD;

			if ( AI_IsSinglePlayer() )
			{
				if ( HasCondition( COND_LOST_ENEMY ) || HasCondition( COND_ENEMY_UNREACHABLE ) )
				{
					SetEnemy( NULL );
					SetIdealState( NPC_STATE_IDLE );
					return SCHED_FAKEBOT_IDLE;
				}
			}

			// Basic Checks, translate does the higher level stuff
			if ( HasCondition( COND_SEE_ENEMY ) )
			{
				return SCHED_FAKEBOT_ATTACK;
			}
			else if ( HasCondition( COND_ENEMY_OCCLUDED ) )
			{
				return SCHED_FAKEBOT_CHASE;
			}
			else if ( HasCondition( COND_REPEATED_DAMAGE ) )
			{
				return SCHED_FAKEBOT_RETREAT;
			}
			else if ( HasCondition( COND_ENEMY_DEAD ) )
				return SCHED_VICTORY_DANCE;
		}
		break;

		default:
			DevMsg( "CFakeBot is confused! Resorting to baseclass.\n" );
		break;
	}

	// This should never happen... but just in case
	return BaseClass::SelectSchedule();
}

//=========================================================

int CFakeBot::TranslateSchedule( int scheduleType ) 
{
	switch( scheduleType )
	{
		case SCHED_FAKEBOT_IDLE:
			{
				// If im still in combat, never idle
				if ( GetEnemy() )
					return TranslateSchedule(SCHED_FAKEBOT_ATTACK);

				return SCHED_FAKEBOT_IDLE;
			}

		case SCHED_FAKEBOT_RETREAT:
			{
				// Can i help/get help from a teammate?
				if (HasCondition( COND_FAKEBOT_HELP_TEAMMATE ))
					return TranslateSchedule(SCHED_FAKEBOT_HELP_TEAMMATE);

				// Am I pissed, and do i have health to spare?
				if ( GetEnemy() && m_bPissed && m_iHealth > (m_iMaxHealth/2) )
					return TranslateSchedule(SCHED_FAKEBOT_ATTACK);

				// Else, just run away
				return SCHED_FAKEBOT_RETREAT;
			}

		case SCHED_FAKEBOT_ATTACK:
			{
				// If i'm pissed, attack aggressively
				if ( m_bPissed && m_iHealth > (m_iMaxHealth-15) )
					return TranslateSchedule(SCHED_FAKEBOT_CHASE);

				// Attack! Attack! Attack!!!
				return SCHED_FAKEBOT_ATTACK;
			}

		case SCHED_FAKEBOT_HELP_TEAMMATE:
			{
				// Is my team-mate a friendly-firing douche? Leave him for dead


				// Run towards my teammate
				return SCHED_FAKEBOT_HELP_TEAMMATE;
			}

		case SCHED_VICTORY_DANCE:
			{
				// Only thrust if the game is over
				if ( g_fGameOver )
					return SCHED_VICTORY_DANCE;	//Use default victory dance sched
				else
				{
					// Random chance to try and dip-dip-potato-chip OR if the guy i just killed was my target
					if ( random->RandomInt(0,2) == 2 /*|| m_pPissedTarget */ )
						return SCHED_FAKEBOT_TBAG;
					else
						return TranslateSchedule(SCHED_FAKEBOT_IDLE);
				}
			}
	}

	return BaseClass::TranslateSchedule( scheduleType );
}


//-----------------------------------------------------------------------------
// Purpose: Respawn
//-----------------------------------------------------------------------------
void CFakeBot::Event_Killed( const CTakeDamageInfo &info )
{
	// Tell my buddies theres danger here to avoid
	if ( info.GetDamageType() & DMG_CRUSH )
		CSoundEnt::InsertSound( SOUND_PHYSICS_DANGER, GetAbsOrigin(), 384, 0.5f, this );

	//!!TODO; This is where we create another fakebot, invisible and intangible, 
	// but otherwise thats just like us, at a info_player point - set to activate
	// after a small respawn period
#if FAKEBOT_REAL_RESPAWN
	if (g_pGameRules->IsMultiplayer())
	{
		// respawn player
		CBaseEntity *pSpot;

		pSpot = gEntList.FindEntityByClassname( pSpot, "info_player_start" );

		// If startspot is set, (re)spawn there.
		if ( g_pGameRules->IsDeathmatch() )
		{
			// Randomize the start spot
			for ( int i = random->RandomInt(1,5); i > 0; i-- )
				pSpot = gEntList.FindEntityByClassname( pSpot, "info_player_deathmatch" );
			if ( !pSpot )  // skip over the null point
				pSpot = gEntList.FindEntityByClassname( pSpot, "info_player_deathmatch" );
		}
#if 0
		else if ( g_pGameRules->IsTeamplay() )
		{
			if ( GetTeamNumber() == 2 )
				pSpot = gEntList.FindEntityByClassname( pSpot, "info_player_combine" );
			else
				pSpot = gEntList.FindEntityByClassname( pSpot, "info_player_rebel" );
		}
#endif

		// Slam back to default class
		if ( !pSpot )
			pSpot = gEntList.FindEntityByClassname( pSpot, "info_player_start" );

		// Create a new fakebot
		CFakeBot *pNewBot = (CBaseEntity *)CreateEntityByName( "fakebot" );
		if ( pNewBot )
		{
			Vector vecOrigin = pSpot->GetAbsOrigin();
			QAngle vecAngles = pSpot->GetAbsAngles();
			pNewBot->SetAbsOrigin( vecOrigin );
			pNewBot->SetAbsAngles( vecAngles );
			DispatchSpawn( pNewBot );

			// Effectively disable until respawn timer is up
			pNewBot->AddSolidFlags( FSOLID_NOT_SOLID );
			pNewBot->AddEffects( EF_NODRAW );
			pNewBot->SetMoveType( MOVETYPE_NONE );
			pNewBot->SetThink( &CFakeBot::Respawn );
			pNewBot->SetNextThink( g_pGameRules->FlPlayerSpawnTime( this ) );
		}
	}
#endif

	// Let the baseclass create a corpse
	BaseClass::Event_Killed( info );
}


//=========================================================
AI_BEGIN_CUSTOM_NPC( fakebot, CFakeBot )

//DECLARE_TASK( TASK_FAKEBOT_TEST )

DECLARE_CONDITION( COND_FAKEBOT_HELP_TEAMMATE )



//=========================================================
// Schedules
//=========================================================

// 1. Idle - walk around randomly
DEFINE_SCHEDULE
(
	SCHED_FAKEBOT_IDLE,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_IDLE_WANDER"
	"		TASK_SET_TOLERANCE_DISTANCE		48"
	"		TASK_SET_ROUTE_SEARCH_TIME		5"
	"		TASK_GET_PATH_TO_RANDOM_NODE	200"
	"		TASK_WALK_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_WAIT_RANDOM				10"
	"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_FAKEBOT_IDLE" // keep doing it
	""
	"	Interrupts"
	"	COND_NEW_ENEMY"
	"	COND_ENEMY_DEAD"
	"	COND_SEE_ENEMY"
	"	COND_HEAR_COMBAT"
	"	COND_HEAR_BULLET_IMPACT"
	"	COND_HEAR_MOVE_AWAY"
	"	COND_HEAR_DANGER"
	"	COND_FAKEBOT_HELP_TEAMMATE"
)

// 2. TBag - Run up to an enemy and try to T-Bag
DEFINE_SCHEDULE
(
	SCHED_FAKEBOT_TBAG,

	"	Tasks"
	"		TASK_FACE_ENEMY					0"
	"		TASK_WAIT						0.5"
	"		TASK_GET_PATH_TO_ENEMY_CORPSE	0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_WAIT						0.5"
	"		TASK_SET_ACTIVITY				ACTIVITY:ACT_CROUCHIDLE"
	"		TASK_WAIT						0.8"
	"		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"
	"		TASK_WAIT						0.8"
	"		TASK_SET_ACTIVITY				ACTIVITY:ACT_CROUCHIDLE"
	"		TASK_WAIT						0.8"
	"		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"
	"		TASK_WAIT						0.8"
	"		TASK_SET_ACTIVITY				ACTIVITY:ACT_CROUCHIDLE"
	"		TASK_WAIT						0.8"
	"		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"
	"		TASK_WAIT_RANDOM				2"
	"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_FAKEBOT_IDLE"
	""
	"	Interrupts"
	"	COND_NEW_ENEMY"
	"	COND_SEE_ENEMY"
	"	COND_HEAR_DANGER"
	"	COND_FAKEBOT_HELP_TEAMMATE"
)

// Combat Schedules
// All these schedules are only for movement/nav, thus they loop until cancelled/interrupted.
// Aiming and shooting is taken care of by the move-shoot behaviour.

// 3. Retreat - Run backwards while in combat
DEFINE_SCHEDULE
(
	SCHED_FAKEBOT_RETREAT,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_FAIL_TAKE_COVER"
	"		TASK_SET_TOLERANCE_DISTANCE		24"
	"		TASK_FIND_COVER_FROM_ENEMY		0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_REMEMBER					MEMORY:INCOVER"
	"		TASK_FACE_ENEMY					0"
	"		TASK_WAIT_RANDOM				1"
	"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_FAKEBOT_RETREAT"
	""
	"	Interrupts"
	"	COND_NEW_ENEMY"
	"	COND_ENEMY_DEAD"
	"	COND_HEAR_DANGER"
)

// 4. Attack - Run towards enemy/strafe in combat
DEFINE_SCHEDULE
(
	SCHED_FAKEBOT_ATTACK,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_FAKEBOT_RETREAT"
	"		TASK_GET_PATH_TO_ENEMY_LOS		0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_WAIT_RANDOM				1"
	"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_FAKEBOT_ATTACK"
	""
	"	Interrupts"
	"	COND_NEW_ENEMY"
	"	COND_ENEMY_DEAD"
	"	COND_HEAR_DANGER"
	"	COND_FAKEBOT_HELP_TEAMMATE"
)

// 5. Chase - Charge towards your enemy
DEFINE_SCHEDULE
(
	SCHED_FAKEBOT_CHASE,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_FAKEBOT_ATTACK"
	"		TASK_GET_PATH_TO_ENEMY_LKP_LOS	0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_WAIT_RANDOM				1"
	"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_FAKEBOT_CHASE"
	""
	"	Interrupts"
	"	COND_NEW_ENEMY"
	"	COND_ENEMY_DEAD"
	"	COND_HEAR_DANGER"
	"	COND_FAKEBOT_HELP_TEAMMATE"
)

// 6. Help Teammate - An allied player/npc is in trouble, run towards him
DEFINE_SCHEDULE
(
	SCHED_FAKEBOT_HELP_TEAMMATE,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_FAKEBOT_RETREAT"
	"		TASK_GET_PATH_TO_TARGET			0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_WAIT_RANDOM				1"
	""
	"	Interrupts"
	"	COND_NEW_ENEMY"
	"	COND_ENEMY_DEAD"
	"	COND_HEAR_DANGER"
)

AI_END_CUSTOM_NPC()
//=========================================================
