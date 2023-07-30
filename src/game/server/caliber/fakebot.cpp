//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Fake bot - A npc that runs around and mimics a player
// Used in SP if we ever need a player-like character, or in MP as a stopgap
// Cannot cap points or do much of anything a player can do, only for deathmatch.
//
// TODO; Code this thing!
//=============================================================================//

#include "cbase.h"
#include "ai_basenpc.h"
#include "ai_baseactor.h"
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

// We normally dont collide with friendly players and phase through like in MP,
// In-case we want to though, for some odd reason
#define	SF_FAKEBOT_SOLID					(1 << 16) 

#define FAKEBOT_MODEL "models/player.mdl"

// For fake weapon slot switching
#define FAKEBOT_WANTS_MELEE		1
#define FAKEBOT_SLOT1			"weapon_swing"
#define FAKEBOT_WANTS_PISTOL	2
#define FAKEBOT_SLOT2			"weapon_pistol"
#define FAKEBOT_WANTS_SMG		3
#define FAKEBOT_SLOT3			"weapon_smg2"
#define FAKEBOT_WANTS_SHOTGUN	4
#define FAKEBOT_SLOT4			"weapon_shotgun"
#define FAKEBOT_WANTS_RPG		5
#define FAKEBOT_SLOT5			"weapon_rpg"

//=========================================================
// NPC's Anim Events Go Here
//=========================================================

class CFakeBot : public CAI_BaseActor
{
	DECLARE_CLASS( CFakeBot, CAI_BaseActor );
public:
	void	Precache( void );
	void	Spawn( void );

	Class_T Classify( void );
	Disposition_t	IRelationType( CBaseEntity *pTarget );
	bool	ShouldGib( const CTakeDamageInfo &info );
	bool	ShouldMoveAndShoot();

	// Speed/Movement
	float	GetIdealAccel( void ) const{ return GetIdealSpeed() * 2.0; };
	float	MaxYawSpeed( void );

	// Hearing
	int		GetSoundInterests( void );
	float	HearingSensitivity( void ) { return 1.0; };
	void	PainSound( const CTakeDamageInfo &info );
	void	DeathSound( const CTakeDamageInfo &info );
	// Never go back to idle after we get in a fight
	virtual bool	ShouldGoToIdleState( void ) { return false; }

	//void	HandleAnimEvent( animevent_t *pEvent );

	// Thinking
	void	NPCThink( void );
	void	PrescheduleThink( void );
	void	OnKilledNPC( CBaseCombatCharacter *pKilled );

	// Doing
	void		BuildScheduleTestBits( void );
	virtual int	SelectSchedule( void );
	virtual int	TranslateSchedule( int scheduleType );

	// See if we should respawn
	void	Event_Killed( const CTakeDamageInfo &info );

private:
	int		m_iFakeTeam;
	int		m_iActiveSlot;
	int		m_iWantsSlot;

private:
	// Conds
	enum
	{
		COND_FAKEBOT_HELP_TEAMMATE = BaseClass::NEXT_CONDITION,
		NEXT_CONDITION,
	};

	// Scheds
	enum
	{
		SCHED_FAKEBOT_IDLE = BaseClass::NEXT_SCHEDULE,
		SCHED_FAKEBOT_TBAG,
		SCHED_FAKEBOT_RETREAT,
		SCHED_FAKEBOT_ATTACK,
		SCHED_FAKEBOT_CHASE,
		SCHED_FAKEBOT_HELP_TEAMMATE,
		NEXT_SCHEDULE,
	};

protected:
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
};

BEGIN_DATADESC( CFakeBot )
	DEFINE_FIELD( m_iFakeTeam, FIELD_INTEGER ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( fakebot, CFakeBot );

//-----------------------------------------------------------------------------
// Purpose: precaches all resources this NPC needs
//-----------------------------------------------------------------------------
void CFakeBot::Precache()
{
	BaseClass::Precache();

	SetModelName( AllocPooledString( FAKEBOT_MODEL ) );

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
	return	CLASS_PLAYER;
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

	EmitSound( "Player.Pain" );
	m_flNextPainSoundTime = gpGlobals->curtime + 1;
}

void CFakeBot::DeathSound( const CTakeDamageInfo &info )
{
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
	m_iFakeTeam			= 0;

	SetSimulatedEveryTick( true );
	SetAnimatedEveryTick( true );

	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_MOVE_JUMP | bits_CAP_DOORS_GROUP | bits_CAP_DUCK | bits_CAP_USE );
	CapabilitiesAdd( bits_CAP_USE_WEAPONS | bits_CAP_MOVE_SHOOT | bits_CAP_STRAFE );

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


//-----------------------------------------------------------------------------
Disposition_t CFakeBot::IRelationType( CBaseEntity *pTarget )
{
	if ( pTarget->IsPlayer() )
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
	else
	{
		SUB_StartFadeOut();
	}
}

void CFakeBot::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();
}


//extern ConVar hl2_playertalk;
void CFakeBot::OnKilledNPC( CBaseCombatCharacter *pKilled )
{
	// Spew some liners, like the player
//	if ( hl2_playertalk.GetBool() )
//	{
//
//	}
	BaseClass::OnKilledNPC( pKilled );
}

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

		//TODO; We should follow/tag along with a friendly player if we're idle
		case NPC_STATE_IDLE:
			// If its danger in idle, just move away
			if ( HasCondition( COND_HEAR_DANGER ) )
				return SCHED_MOVE_AWAY;

			// Always place an emphasis on reloading
			if ( HasCondition( COND_NO_PRIMARY_AMMO ) || HasCondition( COND_NO_SECONDARY_AMMO ) )
				return SCHED_HIDE_AND_RELOAD;
			else
				return SCHED_FAKEBOT_IDLE;
		break;

		case NPC_STATE_ALERT:
		case NPC_STATE_COMBAT:
		{
			if ( HasCondition( COND_HEAR_DANGER ) )
				return SCHED_TAKE_COVER_FROM_BEST_SOUND;

			if ( HasCondition( COND_NO_PRIMARY_AMMO ) || HasCondition( COND_NO_SECONDARY_AMMO ) )
				return SCHED_HIDE_AND_RELOAD;

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
				if ( GetEnemy() )
					return SCHED_FAKEBOT_ATTACK;

				return SCHED_FAKEBOT_IDLE;
			}
		case SCHED_FAKEBOT_ATTACK:
			{
				// If i'm pissed, attack aggressively
				if ( GetEnemy() && false )
					return SCHED_FAKEBOT_CHASE;

				return SCHED_FAKEBOT_ATTACK;
			}
		case SCHED_VICTORY_DANCE:
			{
				// Only thrust if the game is over
				if ( g_fGameOver )
					return SCHED_VICTORY_DANCE;
				else
				{
					// Random chance to try and dip-dip-potato-chip
					if ( random->RandomInt(0,1) )
						return SCHED_FAKEBOT_TBAG;
					else
						return TranslateSchedule( SCHED_FAKEBOT_IDLE );
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
	{
		CSoundEnt::InsertSound( SOUND_PHYSICS_DANGER, GetAbsOrigin(), 384, 0.5f, this );
	}

	//!!TODO; This is where we create another fakebot thats just like us, at a info_player point
#if 0
	if (g_pGameRules->IsMultiplayer())
	{
		// respawn player
		pNewBot->Spawn();
	}
#endif

	// Let the baseclass kill me
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
	"		TASK_WAIT						0.5"
	"		TASK_WAIT_RANDOM				1"
	"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_FAKEBOT_IDLE"
	""
	"	Interrupts"
	"	COND_NEW_ENEMY"
	"	COND_SEE_ENEMY"
	"	COND_HEAR_DANGER"
	"	COND_FAKEBOT_HELP_TEAMMATE"
)

// Combat Schedules

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
