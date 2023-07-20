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

	bool	ShouldGib( const CTakeDamageInfo &info );
	Class_T Classify( void );
	Disposition_t	IRelationType( CBaseEntity *pTarget );

	// Speed/Movement
	float	GetIdealAccel( void ) const{ return GetIdealSpeed() * 2.0; };
	float	MaxYawSpeed( void );

	// Hearing
	int		GetSoundInterests( void );
	float	HearingSensitivity( void ) { return 1.0; };
	// Never go back to idle after we get in a fight
	virtual bool	ShouldGoToIdleState( void ) { return false; }

	//void	HandleAnimEvent( animevent_t *pEvent );

	// Thinking
	void	NPCThink( void );
	void	PrescheduleThink( void );
	void	OnKilledNPC( CBaseCombatCharacter *pKilled );

	// Doing
	virtual int	SelectSchedule( void );
	virtual int	TranslateSchedule( int scheduleType );

	// See if we should respawn
	void	Event_Killed( const CTakeDamageInfo &info );

private:
	int		m_iFakeTeam;
	int		m_iActiveSlot;
	int		m_iWantsSlot;

private:
#if 0
	// Conds
	enum
	{
		COND_FAKEBOT_TEST = BaseClass::NEXT_CONDITION,
		NEXT_CONDITION
	};

	// Scheds
	enum
	{
		SCHED_FAKEBOT_TEST = BaseClass::NEXT_SCHEDULE,
		NEXT_SCHEDULE
	};

	// Tasks
	enum 
	{
		TASK_FAKEBOT_TEST = BaseClass::NEXT_TASK,
		NEXT_TASK
	};
#endif

protected:
	DECLARE_DATADESC();
//	DEFINE_CUSTOM_AI;
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

	if ( FStrEq( STRING( GetModelName() ), "models/player.mdl" ) || FStrEq( STRING( GetModelName() ), "models/holo.mdl" ) )
		UTIL_SetSize(this, VEC_HULL_MIN, VEC_HULL_MAX);
	else
		UTIL_SetSize(this, NAI_Hull::Mins(HULL_HUMAN), NAI_Hull::Maxs(HULL_HUMAN));

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

	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_OPEN_DOORS | bits_CAP_DUCK | bits_CAP_USE );
	CapabilitiesAdd( bits_CAP_MOVE_SHOOT );

	if ( !HasSpawnFlags(SF_FAKEBOT_SOLID) )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( 1 );
		if ( InSameTeam( pPlayer ) )
			PhysDisableEntityCollisions( this, pPlayer );
	}
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

	if ( GetFlags() & FL_DUCKING )
		SetCollisionBounds( VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
	else
		SetCollisionBounds( VEC_HULL_MIN, VEC_HULL_MAX );
}

void CFakeBot::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();
}

void CFakeBot::OnKilledNPC( CBaseCombatCharacter *pKilled )
{
	BaseClass::OnKilledNPC( pKilled );
}

//=========================================================
// Doing
//=========================================================
int CFakeBot::SelectSchedule( void )
{
	switch( m_NPCState )
	{
		//TODO; We should follow a friendly player if we're idle
		case NPC_STATE_IDLE:
			return SCHED_RUN_RANDOM;
		break;

		case NPC_STATE_ALERT:
		case NPC_STATE_COMBAT:
		{
			if ( HasCondition( COND_ENEMY_DEAD ) )
			{
				return SCHED_VICTORY_DANCE;
			}
		}
		break;

		default:
			DevMsg( "CFakeBot is confused! Resorting to baseclass.\n" );
		break;
	}

	return BaseClass::SelectSchedule();
}

//=========================================================

int CFakeBot::TranslateSchedule( int scheduleType ) 
{
	switch( scheduleType )
	{
		case SCHED_VICTORY_DANCE:
			{
				// Only taunt if the game is over
				if ( g_fGameOver )
					return SCHED_VICTORY_DANCE;
				else
					return TranslateSchedule( SCHED_ALERT_FACE );
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
