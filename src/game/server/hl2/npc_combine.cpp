//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Your classic malicious-military-marauders.
// TODO's: Fix sentence prefix - the ifelite check works for now to get the demo out, but its gross and a better system would
// be nice if speech was ever expanded to more unique types of soldier (leader, medic, support, etc.). 
// The minimally invasive way I tried didnt work and I dont know how else to do it without a large re-write.
// My idea is to have a think function interpret and store the finished sentence, and then play it when given the chance, 
// rather than on a purely on-call basis as it is right now. I dont know about the perf costs of that, but
// it shouldnt be too bad if its only updated every other think or something similar. - m.
//
// Auto-squadding, COND_COMBINE_DROP_GRENADE for tripmines, integrate with metrocops somehow??
//
// Note; Hard actually has slightly worse AI squad-wise, but can move and shoot more often
//=============================================================================//

//-----------------------------------------------------------------------------
// Heres a rundown - 
// The most aggressive enemy in the game, at least by hitscan standards
// Works best as a squad, can throw grenades and easily eliminate isolated
// targets - comes in second place in terms of intelligence, barely being
// one-upped by citizen17 - can be an extremely tough enemy, 
// especially on higher difficulties, so use sparingly
//-----------------------------------------------------------------------------
#include "cbase.h"
#include "ai_hull.h"
#include "ai_navigator.h"
#include "ai_motor.h"
#include "ai_squadslot.h"
#include "ai_squad.h"
#include "ai_route.h"
#include "ai_interactions.h"
#include "ai_tacticalservices.h"
#include "soundent.h"
#include "game.h"
#include "npcevent.h"
#include "npc_combine.h"
#include "activitylist.h"
#include "player.h"
#include "basecombatweapon.h"
#include "basegrenade_shared.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "globals.h"
#include "grenade_frag.h"
#include "grenade_ar2.h"
#include "ndebugoverlay.h"
#include "weapon_physcannon.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
//#include "npc_headcrab.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

int g_fCombineQuestion;				// true if an idle grunt asked a question. Cleared when someone answers. YUCK old global from grunt code
extern ConVar sk_npc_dmg_smg1_grenade;
extern ConVar sk_ar2_grenade_radius;

#define COMBINE_SKIN_DEFAULT		0
#define COMBINE_SKIN_SHOTGUNNER		1
#define USE_OLD_SHOOT				0

ConVar	sk_combine_grenadespeed( "sk_combine_grenadespeed", "650");
#define COMBINE_GRENADE_THROW_SPEED sk_combine_grenadespeed.GetFloat()
ConVar	sk_combine_launchspeed( "sk_combine_launchspeed", "650");
#define COMBINE_GRENADE_LAUNCH_SPEED sk_combine_launchspeed.GetFloat()	//This needs to grab the weapons speed
#define COMBINE_GRENADE_TIMER		3.5	//TODO; Skill this
#define	COMBINE_MIN_GRENADE_CLEAR_DIST	250

ConVar	npc_combine_grenade_chance( "npc_combine_grenade_chance",	"50");
#define COMBINE_GRENADE_CHANCE		npc_combine_grenade_chance.GetFloat()		// Chance of throwing a grenade (in most situations).
ConVar npc_combine_grenade_time( "npc_combine_grenade_time", "5" );	//This should not be changed, its only here if some very minor adjustments need to be made
#define COMBINE_GRENADE_TIME		npc_combine_grenade_time.GetFloat()		// Chance of throwing a grenade (in most situations).

ConVar	npc_combine_limp_health( "npc_combine_limp_health",	"20");
#define COMBINE_LIMP_HEALTH		npc_combine_limp_health.GetFloat()

#define COMBINE_EYE_STANDING_POSITION	Vector( 0, 0, 66 )
#define COMBINE_GUN_STANDING_POSITION	Vector( 0, 0, 57 )
#define COMBINE_EYE_CROUCHING_POSITION	Vector( 0, 0, 40 )
#define COMBINE_GUN_CROUCHING_POSITION	Vector( 0, 0, 36 )
#define COMBINE_SHOTGUN_STANDING_POSITION	Vector( 0, 0, 36 )
#define COMBINE_SHOTGUN_CROUCHING_POSITION	Vector( 0, 0, 36 )
#define COMBINE_MIN_CROUCH_DISTANCE		192.0
#define COMBINE_MIN_SIGNAL_DIST			256.0

//Whether or not the combine should spawn health on death
ConVar	npc_combine_drop_health( "npc_combine_drop_health", "1" );

//-----------------------------------------------------------------------------
// Static stuff local to this file.
//-----------------------------------------------------------------------------
// This is the index to the name of the shotgun's classname in the string pool
// so that we can get away with an integer compare rather than a string compare.
string_t	s_iszShotgunClassname;

//-----------------------------------------------------------------------------
// Interactions
//-----------------------------------------------------------------------------
int	g_interactionCombineBash		= 0; // melee bash attack

//=========================================================
// Combines's Anim Events Go Here
//=========================================================
#define COMBINE_AE_RELOAD			( 2 )
#define COMBINE_AE_KICK				( 3 )
#define COMBINE_AE_AIM				( 4 )
#define COMBINE_AE_GREN_TOSS		( 7 )
#define COMBINE_AE_GREN_LAUNCH		( 8 )
#define COMBINE_AE_GREN_DROP		( 9 )
#define COMBINE_AE_CAUGHT_ENEMY		( 10) // grunt established sight with an enemy (player only) that had previously eluded the squad.

int COMBINE_AE_BEGIN_ALTFIRE;
int COMBINE_AE_ALTFIRE;

//=========================================================
// Combine activities
//=========================================================
Activity ACT_COMBINE_THROW_GRENADE;
Activity ACT_COMBINE_LAUNCH_GRENADE;
Activity ACT_COMBINE_BUGBAIT;
Activity ACT_COMBINE_AR2_ALTFIRE;
Activity ACT_WALK_EASY;
Activity ACT_WALK_MARCH;

// -----------------------------------------------
//	> Squad slots
// -----------------------------------------------
enum SquadSlot_T
{	
	SQUAD_SLOT_GRENADE1 = LAST_SHARED_SQUADSLOT,
	SQUAD_SLOT_GRENADE2,
	SQUAD_SLOT_ATTACK_OCCLUDER,
};

enum TacticalVariant_T
{
	TACTICAL_VARIANT_DEFAULT = 0,
	TACTICAL_VARIANT_PRESSURE_ENEMY,				// Always try to close in on the player.
	TACTICAL_VARIANT_PRESSURE_ENEMY_UNTIL_CLOSE,	// Act like VARIANT_PRESSURE_ENEMY, but go to VARIANT_DEFAULT once within 30 feet
	TACTICAL_VARIANT_DEFEND_POSITION,				// Never try to close in on the player.
};

enum PathfindingVariant_T
{
	PATHFINDING_VARIANT_DEFAULT = 0,
};


#define bits_MEMORY_PAIN_LIGHT_SOUND		bits_MEMORY_CUSTOM1
#define bits_MEMORY_PAIN_HEAVY_SOUND		bits_MEMORY_CUSTOM2
#define bits_MEMORY_PLAYER_HURT				bits_MEMORY_CUSTOM3

LINK_ENTITY_TO_CLASS( npc_combine, CNPC_Combine );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_Combine )

DEFINE_FIELD( m_nKickDamage, FIELD_INTEGER ),
DEFINE_FIELD( m_voicePitch, FIELD_INTEGER ),
DEFINE_FIELD( m_vecTossVelocity, FIELD_VECTOR ),
DEFINE_FIELD( m_hForcedGrenadeTarget, FIELD_EHANDLE ),
DEFINE_FIELD( m_lastGrenadeCondition, FIELD_INTEGER ),
DEFINE_FIELD( m_bShouldPatrol, FIELD_BOOLEAN ),
DEFINE_FIELD( m_bFirstEncounter, FIELD_BOOLEAN ),
DEFINE_FIELD( m_flNextPainSoundTime, FIELD_TIME ),
DEFINE_FIELD( m_flNextAlertSoundTime, FIELD_TIME ),
DEFINE_FIELD( m_flNextGrenadeCheck, FIELD_TIME ),
DEFINE_FIELD( m_flNextSignalTime, FIELD_TIME ),
DEFINE_FIELD( m_flNextLostSoundTime, FIELD_TIME ),
DEFINE_FIELD( m_flAlertPatrolTime, FIELD_TIME ),
DEFINE_FIELD( m_flNextAltFireTime, FIELD_TIME ),
DEFINE_FIELD( m_iNewEnemies, FIELD_INTEGER ),
#ifdef USE_OLD_SHOOT
DEFINE_FIELD( m_nShots, FIELD_INTEGER ),
DEFINE_FIELD( m_flShotDelay, FIELD_FLOAT ),
#endif
DEFINE_FIELD( m_flStopMoveShootTime, FIELD_TIME ),
DEFINE_KEYFIELD( m_iNumGrenades, FIELD_INTEGER, "NumGrenades" ),
DEFINE_EMBEDDED( m_Sentences ),

//							m_AssaultBehavior (auto saved by AI)
//							m_StandoffBehavior (auto saved by AI)
//							m_FollowBehavior (auto saved by AI)
//							m_FuncTankBehavior (auto saved by AI)
//							m_RappelBehavior (auto saved by AI)
//							m_ActBusyBehavior (auto saved by AI)

DEFINE_INPUTFUNC( FIELD_VOID,	"LookOff",	InputLookOff ),
DEFINE_INPUTFUNC( FIELD_VOID,	"LookOn",	InputLookOn ),

DEFINE_INPUTFUNC( FIELD_VOID,	"StartPatrolling",	InputStartPatrolling ),
DEFINE_INPUTFUNC( FIELD_VOID,	"StopPatrolling",	InputStopPatrolling ),

DEFINE_INPUTFUNC( FIELD_STRING,	"Assault", InputAssault ),

DEFINE_INPUTFUNC( FIELD_VOID,	"HitByBugbait",		InputHitByBugbait ),

DEFINE_INPUTFUNC( FIELD_STRING,	"ThrowGrenadeAtTarget",	InputThrowGrenadeAtTarget ),

DEFINE_FIELD( m_iLastAnimEventHandled, FIELD_INTEGER ),
DEFINE_FIELD( m_fIsDemolition, FIELD_BOOLEAN ),
DEFINE_FIELD( m_fIsElite, FIELD_BOOLEAN ),
DEFINE_FIELD( m_vecAltFireTarget, FIELD_VECTOR ),

DEFINE_KEYFIELD( m_iTacticalVariant, FIELD_INTEGER, "tacticalvariant" ),
DEFINE_KEYFIELD( m_iPathfindingVariant, FIELD_INTEGER, "pathfindingvariant" ),

END_DATADESC()


//------------------------------------------------------------------------------
// Constructor.
//------------------------------------------------------------------------------
CNPC_Combine::CNPC_Combine()
{
	m_vecTossVelocity.Init();	//= vec3_origin
}


//-----------------------------------------------------------------------------
// Create components
//-----------------------------------------------------------------------------
bool CNPC_Combine::CreateComponents()
{
	if ( !BaseClass::CreateComponents() )
		return false;

	m_Sentences.Init( this, "NPC_Combine.SentenceParameters" );
	return true;
}


//------------------------------------------------------------------------------
// Purpose: Don't look, only get info from squad.
//------------------------------------------------------------------------------
void CNPC_Combine::InputLookOff( inputdata_t &inputdata )
{
	m_spawnflags |= SF_COMBINE_NO_LOOK;
}

//------------------------------------------------------------------------------
// Purpose: Enable looking.
//------------------------------------------------------------------------------
void CNPC_Combine::InputLookOn( inputdata_t &inputdata )
{
	m_spawnflags &= ~SF_COMBINE_NO_LOOK;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Combine::InputStartPatrolling( inputdata_t &inputdata )
{
	m_bShouldPatrol = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Combine::InputStopPatrolling( inputdata_t &inputdata )
{
	m_bShouldPatrol = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Combine::InputAssault( inputdata_t &inputdata )
{
	m_AssaultBehavior.SetParameters( AllocPooledString(inputdata.value.String()), CUE_DONT_WAIT, RALLY_POINT_SELECT_DEFAULT );
}

//-----------------------------------------------------------------------------
// We were hit by bugbait
//-----------------------------------------------------------------------------
void CNPC_Combine::InputHitByBugbait( inputdata_t &inputdata )
{
	SetCondition( COND_COMBINE_HIT_BY_BUGBAIT );
}

//-----------------------------------------------------------------------------
// Purpose: Force the combine soldier to throw a grenade at the target
//			If I'm a combine elite, fire my combine ball at the target instead.
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CNPC_Combine::InputThrowGrenadeAtTarget( inputdata_t &inputdata )
{
	// Ignore if we're inside a scripted sequence
	if ( m_NPCState == NPC_STATE_SCRIPT && m_hCine )
		return;

	CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, inputdata.value.String(), NULL, inputdata.pActivator, inputdata.pCaller );
	if ( !pEntity )
	{
		DevMsg("%s (%s) received ThrowGrenadeAtTarget input, but couldn't find target entity '%s'\n", GetClassname(), GetDebugName(), inputdata.value.String() );
		return;
	}

	m_hForcedGrenadeTarget = pEntity;
	m_flNextGrenadeCheck = 0;

	ClearSchedule( "Told to throw grenade via input" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Combine::Precache()
{
	PrecacheModel("models/Weapons/w_grenade.mdl");
	UTIL_PrecacheOther( "npc_handgrenade" );

	PrecacheScriptSound( "NPC_Combine.GrenadeLaunch" );
	PrecacheScriptSound( "NPC_Combine.WeaponBash" );
	PrecacheScriptSound( "Weapon_CombineGuard.Special1" );
	enginesound->PrecacheSentenceGroup( "COMBINE" );

	// get voice pitch
	if (m_fIsDemolition)
		m_voicePitch = 90;
	else
		m_voicePitch = 96 + random->RandomInt(0,12);

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::Activate()
{
	//TODO; This needs to also consider normal shotgun, for scripts that use it/
	//Compatibility with base hl2
	s_iszShotgunClassname = FindPooledString( "weapon_supershotgun" );
	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CNPC_Combine::Spawn( void )
{
	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	SetBloodColor( BLOOD_COLOR_RED );
//	m_flFieldOfView			= 0.2;	//*150
	m_NPCState				= NPC_STATE_NONE;
	m_flNextGrenadeCheck	= gpGlobals->curtime + 1;
	m_flNextSignalTime		= 0;
	m_flNextAlertSoundTime	= 0;
	m_flNextPainSoundTime	= 0;
//	m_flNextNewEnemyTime	= 0;
	m_iNewEnemies			= 0;
	m_bShouldPatrol			= false;

	if ( !HasSpawnFlags( SF_NPC_START_EFFICIENT ) )
	{
		CapabilitiesAdd( bits_CAP_TURN_HEAD );
#ifndef USE_OLD_SHOOT
		CapabilitiesAdd( bits_CAP_USE_SHOT_REGULATOR );
#endif
	}

	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_MOVE_CLIMB);
	CapabilitiesAdd( bits_CAP_USE_WEAPONS | bits_CAP_AIM_GUN );

	// Innate range attack for kicking
	CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1 );

	// Can be in a squad
	CapabilitiesAdd( bits_CAP_SQUAD | bits_CAP_NO_HIT_SQUADMATES );

	CapabilitiesAdd( bits_CAP_DUCK );				// In reloading and cover

	m_bFirstEncounter	= true;// this is true when the grunt spawns, because he hasn't encountered an enemy yet.

	m_HackedGunPos = Vector ( 0, 0, 55 );

	m_flStopMoveShootTime = FLT_MAX; // Move and shoot defaults on.
	m_MoveAndShootOverlay.SetInitialDelay( 0.75 ); // But with a bit of a delay.

	m_flNextLostSoundTime		= 0;
	m_flAlertPatrolTime			= 0;

	m_flNextAltFireTime = gpGlobals->curtime;

	NPCInit();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Combine::CreateBehaviors()
{
	AddBehavior( &m_RappelBehavior );
	AddBehavior( &m_ActBusyBehavior );
	AddBehavior( &m_AssaultBehavior );
	AddBehavior( &m_StandoffBehavior );
	AddBehavior( &m_FollowBehavior );
	AddBehavior( &m_FuncTankBehavior );

	return BaseClass::CreateBehaviors();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::GatherConditions()
{
	BaseClass::GatherConditions();

	ClearCondition( COND_COMBINE_ATTACK_SLOT_AVAILABLE );

	if( GetState() == NPC_STATE_COMBAT )
	{
		if( IsCurSchedule( SCHED_COMBINE_WAIT_IN_COVER, false ) )
		{
			// Soldiers that are standing around doing nothing poll for attack slots so
			// that they can respond quickly when one comes available. If they can 
			// occupy a vacant attack slot, they do so. This holds the slot until their
			// schedule breaks and schedule selection runs again, essentially reserving this
			// slot. If they do not select an attack schedule, then they'll release the slot.
			if( OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
			{
				SetCondition( COND_COMBINE_ATTACK_SLOT_AVAILABLE );
			}
		}

		if( IsUsingTacticalVariant(TACTICAL_VARIANT_PRESSURE_ENEMY_UNTIL_CLOSE) )
		{
			if( GetEnemy() != NULL && !HasCondition(COND_ENEMY_OCCLUDED) )
			{
				// Now we're close to our enemy, stop using the tactical variant.
				if( GetAbsOrigin().DistToSqr(GetEnemy()->GetAbsOrigin()) < Square(30.0f * 12.0f) )
					m_iTacticalVariant = TACTICAL_VARIANT_DEFAULT;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Combine::PrescheduleThink()
{
	BaseClass::PrescheduleThink();

	// Speak any queued sentences
	m_Sentences.UpdateSentenceQueue();

	extern ConVar ai_debug_shoot_positions;
	if ( ai_debug_shoot_positions.GetBool() )
		NDebugOverlay::Cross3D( EyePosition(), 16, 0, 255, 0, false, 0.1 );

#if 0
	if( gpGlobals->curtime >= m_flStopMoveShootTime )
	{
		// Time to stop move and shoot and start facing the way I'm running.
		// This makes the combine look attentive when disengaging, but prevents
		// them from always running around facing you.
		//
		// Only do this if it won't be immediately shut off again.
		if( GetNavigator()->GetPathTimeToGoal() > 1.0f )
		{
			m_MoveAndShootOverlay.SuspendMoveAndShoot( 5.0f );
			m_flStopMoveShootTime = FLT_MAX;
		}
	}

	if( m_flGroundSpeed > 0 && GetState() == NPC_STATE_COMBAT && m_MoveAndShootOverlay.IsSuspended() )
	{
		// Return to move and shoot when near my goal so that I 'tuck into' the location facing my enemy.
		if( GetNavigator()->GetPathTimeToGoal() <= 1.0f )
		{
			m_MoveAndShootOverlay.SuspendMoveAndShoot( 0 );
		}
	}
#endif

}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::DelayAltFireAttack( float flDelay )
{
	float flNextAltFire = gpGlobals->curtime + flDelay;

	if( flNextAltFire > m_flNextAltFireTime )
	{
		// Don't let this delay order preempt a previous request to wait longer.
		m_flNextAltFireTime = flNextAltFire;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::DelaySquadAltFireAttack( float flDelay )
{
	// Make sure to delay my own alt-fire attack.
	DelayAltFireAttack( flDelay );

	AISquadIter_t iter;
	CAI_BaseNPC *pSquadmate = m_pSquad ? m_pSquad->GetFirstMember( &iter ) : NULL;
	while ( pSquadmate )
	{
		CNPC_Combine *pCombine = dynamic_cast<CNPC_Combine*>(pSquadmate);

		if( pCombine && pCombine->IsDemolition() )
		{
			pCombine->DelayAltFireAttack( flDelay );
		}

		pSquadmate = m_pSquad->GetNextMember( &iter );
	}
}

//-----------------------------------------------------------------------------
// Purpose: degrees to turn in 0.1 seconds
//-----------------------------------------------------------------------------
float CNPC_Combine::MaxYawSpeed( void )
{
	if ( IsMoving() && HasPoseParameter( GetSequence(), m_poseMove_Yaw ) )
	{
		return( 20 );
	}
	else
	{
		switch( GetActivity() )
		{
		case ACT_IDLE:		
			return 30;
			break;
		case ACT_COVER_LOW:
			return 25;
			break;
		case ACT_TURN_LEFT:
		case ACT_TURN_RIGHT:
		case ACT_RANGE_ATTACK1:
		case ACT_RANGE_ATTACK2:
		case ACT_MELEE_ATTACK1:
		case ACT_MELEE_ATTACK2:
			return 90;
		default:
			return 45;
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CNPC_Combine::ShouldMoveAndShoot()
{
	// Set this timer so that gpGlobals->curtime can't catch up to it. 
	// Essentially, we're saying that we're not going to interfere with 
	// what the AI wants to do with move and shoot. 
	//
	// If any code below changes this timer, the code is saying 
	// "It's OK to move and shoot until gpGlobals->curtime == m_flStopMoveShootTime"
	m_flStopMoveShootTime = FLT_MAX;

	// Dont move n' shoot when hurt
	if ( m_iHealth <= COMBINE_LIMP_HEALTH )
		return false;

	// Only move-and-shoot when on the Assault
	if( IsCurSchedule( SCHED_COMBINE_ASSAULT, false ) )
		return true;

	if( IsCurSchedule( SCHED_COMBINE_PRESS_ATTACK, false ) )
		return true;

	if( IsCurSchedule( SCHED_RANGE_ATTACK1, false ) )
		return true;

#ifdef HL2_EPISODIC
	if( IsCurSchedule( SCHED_COMBINE_CHARGE_PLAYER, false ) )
		return true;

	if( IsCurSchedule( SCHED_COMBINE_COVER_AND_RELOAD, false ) )
		return true;

	if( IsCurSchedule( SCHED_COMBINE_TAKE_COVER2, false ) )
		return true;
#endif	// HL2_EPISODIC

	return BaseClass::ShouldMoveAndShoot();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Combine::OnUpdateShotRegulator()
{
	BaseClass::OnUpdateShotRegulator();

	if( IsCurSchedule( SCHED_COMBINE_SUPPRESS ) )
	{
		// Longer burst
		CBaseCombatWeapon *pWeapon = GetActiveWeapon();
		if ( pWeapon->GetMinBurst() != pWeapon->GetMaxBurst() )
		{
			GetShotRegulator()->SetBurstShotsRemaining( GetShotRegulator()->GetBurstShotsRemaining() * 3 );
		}
		else
		{
			// Shorter Rest interval
			float flMinInterval, flMaxInterval;
			GetShotRegulator()->GetRestInterval( &flMinInterval, &flMaxInterval );
			GetShotRegulator()->SetRestInterval( flMinInterval * 0.4f, flMaxInterval * 0.4f );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: turn in the direction of movement
// Output :
//-----------------------------------------------------------------------------
bool CNPC_Combine::OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval )
{
#if 0
  	// FIXME: this will break scripted sequences that walk when they have an enemy
  	if (GetEnemy() && (GetNavigator()->GetMovementActivity() == ACT_WALK_AIM || GetNavigator()->GetMovementActivity() == ACT_WALK || GetNavigator()->GetMovementActivity() == ACT_RUN_AIM))
  	{
		Vector vecEnemyLKP = GetEnemyLKP();
		AddFacingTarget( GetEnemy(), vecEnemyLKP, 1.0, 0.2 );
	}
#endif

	return BaseClass::OverrideMoveFacing( move, flInterval );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Class_T	CNPC_Combine::Classify ( void )
{
	return CLASS_COMBINE;
}

//-----------------------------------------------------------------------------
// Continuous movement tasks
//-----------------------------------------------------------------------------
bool CNPC_Combine::IsCurTaskContinuousMove()
{
	const Task_t* pTask = GetTask();
	if ( pTask && (pTask->iTask == TASK_COMBINE_CHASE_ENEMY_CONTINUOUSLY) )
		return true;

	return BaseClass::IsCurTaskContinuousMove();
}

//-----------------------------------------------------------------------------
// Chase the enemy, updating the target position as the player moves
//-----------------------------------------------------------------------------
void CNPC_Combine::StartTaskChaseEnemyContinuously( const Task_t *pTask )
{
	CBaseEntity *pEnemy = GetEnemy();
	if ( !pEnemy )
	{
		TaskFail( FAIL_NO_ENEMY );
		return;
	}

	// We're done once we get close enough
	if ( WorldSpaceCenter().DistToSqr( pEnemy->WorldSpaceCenter() ) <= pTask->flTaskData * pTask->flTaskData )
	{
		TaskComplete();
		return;
	}

	// TASK_GET_PATH_TO_ENEMY
	if ( IsUnreachable( pEnemy ) )
	{
		TaskFail(FAIL_NO_ROUTE);
		return;
	}

	if ( !GetNavigator()->SetGoal( GOALTYPE_ENEMY, AIN_NO_PATH_TASK_FAIL ) )
	{
		// no way to get there =( 
		DevWarning( 2, "GetPathToEnemy failed!!\n" );
		RememberUnreachable( pEnemy );
		TaskFail(FAIL_NO_ROUTE);
		return;
	}

	// NOTE: This is TaskRunPath here.
	if ( TranslateActivity( ACT_RUN ) != ACT_INVALID )
	{
		GetNavigator()->SetMovementActivity( ACT_RUN );
	}
	else
	{
		GetNavigator()->SetMovementActivity( ACT_WALK );
	}

	// Cover is void once I move
	Forget( bits_MEMORY_INCOVER );

	if (GetNavigator()->GetGoalType() == GOALTYPE_NONE)
	{
		TaskComplete();
		GetNavigator()->ClearGoal();		// Clear residual state
		return;
	}

	// No shooting delay when in this mode
	m_MoveAndShootOverlay.SetInitialDelay( 0.0 );

	if (!GetNavigator()->IsGoalActive())
	{
		SetIdealActivity( GetStoppedActivity() );
	}
	else
	{
		// Check validity of goal type
		ValidateNavGoal();
	}

	// set that we're probably going to stop before the goal
	GetNavigator()->SetArrivalDistance( pTask->flTaskData );
	m_vSavePosition = GetEnemy()->WorldSpaceCenter();
}

void CNPC_Combine::RunTaskChaseEnemyContinuously( const Task_t *pTask )
{
	if (!GetNavigator()->IsGoalActive())
	{
		SetIdealActivity( GetStoppedActivity() );
	}
	else
	{
		// Check validity of goal type
		ValidateNavGoal();
	}

	CBaseEntity *pEnemy = GetEnemy();
	if ( !pEnemy )
	{
		TaskFail( FAIL_NO_ENEMY );
		return;
	}

	// We're done once we get close enough
	if ( WorldSpaceCenter().DistToSqr( pEnemy->WorldSpaceCenter() ) <= pTask->flTaskData * pTask->flTaskData )
	{
		GetNavigator()->StopMoving();
		TaskComplete();
		return;
	}

	// Recompute path if the enemy has moved too much
	if ( m_vSavePosition.DistToSqr( pEnemy->WorldSpaceCenter() ) < (pTask->flTaskData * pTask->flTaskData) )
		return;

	if ( IsUnreachable( pEnemy ) )
	{
		TaskFail(FAIL_NO_ROUTE);
		return;
	}

	if ( !GetNavigator()->RefindPathToGoal() )
	{
		TaskFail(FAIL_NO_ROUTE);
		return;
	}

	m_vSavePosition = pEnemy->WorldSpaceCenter();
}


//=========================================================
// start task
//=========================================================
void CNPC_Combine::StartTask( const Task_t *pTask )
{
	// NOTE: This reset is required because we change it in TASK_COMBINE_CHASE_ENEMY_CONTINUOUSLY
	m_MoveAndShootOverlay.SetInitialDelay( 0.75 );

	switch ( pTask->iTask )
	{
	case TASK_COMBINE_SET_STANDING:
		{
			if ( pTask->flTaskData == 1.0f)
			{
				Stand();
			}
			else
			{
				Crouch();
			}
			TaskComplete();
		}
		break;

	case TASK_COMBINE_CHASE_ENEMY_CONTINUOUSLY:
		StartTaskChaseEnemyContinuously( pTask );
		break;

	case TASK_COMBINE_PLAY_SEQUENCE_FACE_ALTFIRE_TARGET:
		SetIdealActivity( (Activity)(int)pTask->flTaskData );
		GetMotor()->SetIdealYawToTargetAndUpdate( m_vecAltFireTarget, AI_KEEP_YAW_SPEED );
		break;

	case TASK_COMBINE_SIGNAL_BEST_SOUND:
		if( IsInSquad() && GetSquad()->NumMembers() > 1 )
		{
#ifdef HL2MP
			CBasePlayer *pPlayer = AI_GetNearestPlayer( GetAbsOrigin() );
#else
			CBasePlayer *pPlayer = AI_GetSinglePlayer();
#endif

			if( pPlayer && OccupyStrategySlot( SQUAD_SLOT_EXCLUSIVE_HANDSIGN ) )
			{
				CSound *pSound;
				pSound = GetBestSound( SOUND_DANGER );

				Assert( pSound != NULL );

				if ( pSound )
				{
					Vector right, tosound;

					GetVectors( NULL, &right, NULL );

					tosound = pSound->GetSoundReactOrigin() - GetAbsOrigin();
					VectorNormalize( tosound);

					tosound.z = 0;
					right.z = 0;

					Activity	signalActivity = ACT_SIGNAL_FORWARD;
					
					if( DotProduct( right, tosound ) > 0 )
					{
						// Right
						signalActivity = ACT_SIGNAL_RIGHT;
					}
					else if( DotProduct( right, tosound ) < 0 )
					{
						// Left
						signalActivity = ACT_SIGNAL_LEFT;
					}

					SetIdealActivity( signalActivity );

					break;
				}
			}
		}

		// Otherwise, just skip it.
		TaskComplete();
		break;

	case TASK_ANNOUNCE_ATTACK:
		{
			// If Primary Attack
			if ((int)pTask->flTaskData == 1)
			{
				// -----------------------------------------------------------
				// If enemy isn't facing me and I haven't attacked in a while
				// annouce my attack before I start wailing away
				// -----------------------------------------------------------
				CBaseCombatCharacter *pBCC = GetEnemyCombatCharacterPointer();

				if	(pBCC && pBCC->IsPlayer() && (!pBCC->FInViewCone( this )) &&
					(gpGlobals->curtime - m_flLastAttackTime > 3.0) )
				{
					m_flLastAttackTime = gpGlobals->curtime;

					if ( m_pSquad && m_pSquad->IsLeader( this ) )
					{
						m_Sentences.Speak( "COMBINE_ANNOUNCE_LEADER", SENTENCE_PRIORITY_MEDIUM );
					}
					else
					{
						m_Sentences.Speak( "COMBINE_ANNOUNCE", SENTENCE_PRIORITY_MEDIUM );
					}

					// Wait one second
					SetWait( 1.0 );

					//TODO; Is this bit necessary?
					if ( !IsCrouching() )
					{
						SetActivity(ACT_IDLE);
					}
					else
					{
						SetActivity(ACT_CROUCHIDLE);
					}
				}
				// -------------------------------------------------------------
				//  Otherwise move on
				// -------------------------------------------------------------
				else
				{
					TaskComplete();
				}
			}
			else
			{
				m_Sentences.Speak( "COMBINE_THROW_GRENADE", SENTENCE_PRIORITY_HIGH );
				SetActivity(ACT_IDLE);

				// Wait a few seconds
				SetWait( 2.0 );
			}
			break;
		}	

	case TASK_WALK_PATH:
	case TASK_RUN_PATH:
		// grunt no longer assumes he is covered if he moves
		Forget( bits_MEMORY_INCOVER );
		BaseClass::StartTask( pTask );
		break;

	case TASK_COMBINE_FACE_TOSS_DIR:
		break;

	case TASK_COMBINE_GET_PATH_TO_FORCED_GREN_LOS:
		{
			if ( !m_hForcedGrenadeTarget )
			{
				TaskFail(FAIL_NO_ENEMY);
				return;
			}

			float flMaxRange = 2000;
			float flMinRange = 0;

			Vector vecEnemy = m_hForcedGrenadeTarget->GetAbsOrigin();
			Vector vecEnemyEye = vecEnemy + m_hForcedGrenadeTarget->GetViewOffset();

			Vector posLos;
			bool found = false;

			if ( GetTacticalServices()->FindLateralLos( vecEnemyEye, &posLos ) )
			{
				float dist = ( posLos - vecEnemyEye ).Length();
				if ( dist < flMaxRange && dist > flMinRange )
					found = true;
			}

			if ( !found && GetTacticalServices()->FindLos( vecEnemy, vecEnemyEye, flMinRange, flMaxRange, 1.0, &posLos ) )
			{
				found = true;
			}

			if ( !found )
			{
				TaskFail( FAIL_NO_SHOOT );
			}
			else
			{
				// else drop into run task to offer an interrupt
				m_vInterruptSavePosition = posLos;
			}
		}
		break;
#if 0
	// This is handled with an overlay now (sjb)
	case TASK_COMBINE_MOVE_AND_SHOOT:
	case TASK_COMBINE_MOVE_AND_AIM:
		{
			m_nShots = GetActiveWeapon()->GetRandomBurst();
			ChainStartTask( TASK_WAIT_FOR_MOVEMENT, pTask->flTaskData );
		}
		break;
#endif

	case TASK_COMBINE_IGNORE_ATTACKS:
		// must be in a squad
		if (m_pSquad && m_pSquad->NumMembers() > 2)
		{
			// the enemy must be far enough away
#ifdef USE_OLD_SHOOT
			if (GetEnemy() && (GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter()).Length() > 512.0 )
			{
				m_flNextAttack	= gpGlobals->curtime + pTask->flTaskData;
			}
#else
			if (GetEnemy() && (GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter()).Length() > 512.0 )
			{
				m_flNextAttack	= GetShotRegulator()->FireNoEarlierThan( gpGlobals->curtime + pTask->flTaskData );
			}	
#endif
		}
		TaskComplete( );
		break;

	case TASK_COMBINE_DEFER_SQUAD_GRENADES:
		{
			if ( m_pSquad )
			{
				// iterate my squad and stop everyone from throwing grenades for a little while.
				AISquadIter_t iter;

				CAI_BaseNPC *pSquadmate = m_pSquad ? m_pSquad->GetFirstMember( &iter ) : NULL;
				while ( pSquadmate )
				{
					CNPC_Combine *pCombine = dynamic_cast<CNPC_Combine*>(pSquadmate);

					if( pCombine )
					{
						pCombine->m_flNextGrenadeCheck = m_flNextGrenadeCheck + 3;	//This adds onto everybody elses individual time
					}

					pSquadmate = m_pSquad->GetNextMember( &iter );
				}
			}

			TaskComplete();
			break;
		}

	case TASK_FACE_IDEAL:
	case TASK_FACE_ENEMY:
		{
			if( pTask->iTask == TASK_FACE_ENEMY && HasCondition( COND_CAN_RANGE_ATTACK1 ) )
			{
				TaskComplete();
				return;
			}

			BaseClass::StartTask( pTask );
			bool bIsFlying = (GetMoveType() == MOVETYPE_FLY) || (GetMoveType() == MOVETYPE_FLYGRAVITY);
			if (bIsFlying)
			{
				SetIdealActivity( ACT_GLIDE );
			}

		}
		break;

	case TASK_FIND_COVER_FROM_ENEMY:
		{
			if (GetHintGroup() == NULL_STRING)
			{
				CBaseEntity *pEntity = GetEnemy();

				// FIXME: this should be generalized by the schedules that are selected, or in the definition of 
				// what "cover" means (i.e., trace attack vulnerability vs. physical attack vulnerability
				if ( pEntity )
				{
					// NOTE: This is a good time to check to see if the player is hurt.
					// Have the combine notice this and call out
					if ( !HasMemory(bits_MEMORY_PLAYER_HURT) && pEntity->IsPlayer() && pEntity->GetHealth() <= 20 )
					{
						if ( m_pSquad )
						{
							m_pSquad->SquadRemember(bits_MEMORY_PLAYER_HURT);
						}

						const char *pSentenceName = "COMBINE_PLAYERHIT";
						if( IsElite() )
							pSentenceName = "COMBINE_ELITE_PLAYERHIT";

						m_Sentences.Speak( pSentenceName, SENTENCE_PRIORITY_INVALID );
						JustMadeSound( SENTENCE_PRIORITY_HIGH );
					}
					if ( pEntity->MyNPCPointer() )
					{
						if ( !(pEntity->MyNPCPointer()->CapabilitiesGet( ) & bits_CAP_WEAPON_RANGE_ATTACK1) && 
							!(pEntity->MyNPCPointer()->CapabilitiesGet( ) & bits_CAP_INNATE_RANGE_ATTACK1) )
						{
							TaskComplete();
							return;
						}
					}
				}
			}
			BaseClass::StartTask( pTask );
		}
		break;

#ifdef USE_OLD_SHOOT
	// This is handled by the shot regulator now
	case TASK_RANGE_ATTACK1:
		{
			m_nShots = GetActiveWeapon()->GetRandomBurst();
			if( IsCurSchedule( SCHED_COMBINE_SUPPRESS ) )
			{
				// Longer burst
				m_nShots = (GetActiveWeapon()->GetRandomBurst() * 3 );
			}
			m_flShotDelay = GetActiveWeapon()->GetFireRate();

			m_flNextAttack = gpGlobals->curtime + m_flShotDelay - 0.1;
			ResetIdealActivity( ACT_RANGE_ATTACK1 );
			m_flLastAttackTime = gpGlobals->curtime;
		}
		break;
#endif

	case TASK_COMBINE_DIE_INSTANTLY:
		{
			CTakeDamageInfo info;

			info.SetAttacker( this );
			info.SetInflictor( this );
			info.SetDamage( m_iHealth );
			info.SetDamageType( pTask->flTaskData );
			info.SetDamageForce( Vector( 0.1, 0.1, 0.1 ) );

			TakeDamage( info );

			TaskComplete();
		}
		break;

	default: 
		BaseClass:: StartTask( pTask );
		break;
	}
}

//=========================================================
// RunTask
//=========================================================
void CNPC_Combine::RunTask( const Task_t *pTask )
{
	/*
	{
	CBaseEntity *pEnemy = GetEnemy();
	if (pEnemy)
	{
	NDebugOverlay::Line(Center(), pEnemy->Center(), 0,255,255, false, 0.1);
	}

	}
	*/

	/*
	if (m_iMySquadSlot != SQUAD_SLOT_NONE)
	{
	char text[64];
	Q_snprintf( text, strlen( text ), "%d", m_iMySquadSlot );

	NDebugOverlay::Text( Center() + Vector( 0, 0, 72 ), text, false, 0.1 );
	}
	*/

	switch ( pTask->iTask )
	{
	case TASK_COMBINE_CHASE_ENEMY_CONTINUOUSLY:
		RunTaskChaseEnemyContinuously( pTask );
		break;

	case TASK_COMBINE_SIGNAL_BEST_SOUND:
		AutoMovement( );
		if ( IsActivityFinished() )
		{
			TaskComplete();
		}
		break;

	case TASK_ANNOUNCE_ATTACK:
		{
			// Stop waiting if enemy facing me or lost enemy
		//	if ((int)pTask->flTaskData == 1)
		//	{
				CBaseCombatCharacter* pBCC = GetEnemyCombatCharacterPointer();
				if	(!pBCC || pBCC->FInViewCone( this ))
				{
					TaskComplete();
				}
		//	}

			if ( IsWaitFinished() )
			{
				TaskComplete();
			}
		}
		break;

	case TASK_COMBINE_PLAY_SEQUENCE_FACE_ALTFIRE_TARGET:
		{
			GetMotor()->SetIdealYawToTargetAndUpdate( m_vecAltFireTarget, AI_KEEP_YAW_SPEED );

			if ( IsActivityFinished() )
			{
				TaskComplete();
			}
		}
		break;

	case TASK_COMBINE_FACE_TOSS_DIR:
		{
			// project a point along the toss vector and turn to face that point.
			GetMotor()->SetIdealYawToTargetAndUpdate( GetLocalOrigin() + m_vecTossVelocity * 64, AI_KEEP_YAW_SPEED );

			if ( FacingIdeal() )
			{
				TaskComplete( true );
			}
		}
		break;

	case TASK_COMBINE_GET_PATH_TO_FORCED_GREN_LOS:
		{
			if ( !m_hForcedGrenadeTarget )
			{
				TaskFail(FAIL_NO_ENEMY);
				return;
			}

			if ( GetTaskInterrupt() > 0 )
			{
				ClearTaskInterrupt();

				Vector vecEnemy = m_hForcedGrenadeTarget->GetAbsOrigin();
				AI_NavGoal_t goal( m_vInterruptSavePosition, ACT_RUN, AIN_HULL_TOLERANCE );

				GetNavigator()->SetGoal( goal, AIN_CLEAR_TARGET );
				GetNavigator()->SetArrivalDirection( vecEnemy - goal.dest );
			}
			else
			{
				TaskInterrupt();
			}
		}
		break;

#if 0
	case TASK_COMBINE_MOVE_AND_AIM:
		{
			Vector vecEnemyLKP = GetEnemyLKP();
			float flEDist = UTIL_DistApprox2D( Center(), vecEnemyLKP );
			if (GetNavigator()->IsGoalActive())
			{
				float flGDist = GetNavigator()->GetPathDistToGoal( );

				if ((flEDist < 500.0 || flGDist < 60.0) && !HasCondition(COND_TOO_FAR_TO_ATTACK) && !HasCondition(COND_LIGHT_DAMAGE) && !HasCondition(COND_REPEATED_DAMAGE)) // FIXME: use weapon distance?
				{
					GetNavigator()->SetMovementActivity(ACT_WALK_AIM); 	// FIXME: this is probably evil
				}
				else
				{
					GetNavigator()->SetMovementActivity(ACT_RUN_AIM);
				}
			}

			// FIXME: HACK.  Need a clean way to run multiple tasks
			ChainRunTask( TASK_WAIT_FOR_MOVEMENT, pTask->flTaskData );
		}
		break;

	case TASK_COMBINE_MOVE_AND_SHOOT:
		{
			if (GetNavigator()->IsGoalActive())
			{
			// can they shoot?
				bool bShouldShoot = false;
				if (HasCondition( COND_CAN_RANGE_ATTACK1 ) && gpGlobals->curtime >= m_flNextAttack)
				{
					bShouldShoot = true;
				}

				if (bShouldShoot)
				{
					/*
					if (GetNavigator()->GetMovementActivity() != ACT_WALK_AIM)
					{
						// transitioning, wait a bit
						m_flNextAttack = gpGlobals->curtime + 0.3;
					}
					*/
					
					// walk, don't run
					GetNavigator()->SetMovementActivity(ACT_WALK_AIM); 	// FIXME: this is probably evil

					Activity activity = TranslateActivity( ACT_GESTURE_RANGE_ATTACK1 );

					Assert( activity != ACT_INVALID );

					// time to fire?
					if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) && gpGlobals->curtime >= m_flNextAttack )
					{
						if (--m_nShots > 0)
						{
							SetLastAttackTime( gpGlobals->curtime );
							AddGesture( activity );
							// FIXME: this seems a bit wacked
							Weapon_SetActivity( Weapon_TranslateActivity( ACT_RANGE_ATTACK1 ), 0 );
							m_flNextAttack = gpGlobals->curtime + GetActiveWeapon()->GetFireRate() - 0.1;
						}
						else
						{
							m_nShots = random->RandomInt( 4, 7 );
							m_flNextAttack = gpGlobals->curtime + random->RandomFloat( 0.3, 0.5 );
						}
					}
				}
				else
				{
					GetNavigator()->SetMovementActivity(ACT_RUN_AIM);
				}

				// keep enemy if dead but try to look for a new one
				if (!GetEnemy() || !GetEnemy()->IsAlive())
				{
					CBaseEntity *pNewEnemy = BestEnemy();

					if( pNewEnemy != NULL )
					{
						//New enemy! Clear the timers and set conditions.
 						SetCondition( COND_NEW_ENEMY );
						SetEnemy( pNewEnemy );
					}
					else
					{
						ClearAttackConditions();
					}
					// SetEnemy( NULL );
				}
			}

			// FIXME: HACK.  Need a clean way to run multiple tasks
			ChainRunTask( TASK_WAIT_FOR_MOVEMENT, pTask->flTaskData );
		}
		break;
#endif

#ifdef USE_OLD_SHOOT
	case TASK_RANGE_ATTACK1:
		{
			AutoMovement( );

			Vector vecEnemyLKP = GetEnemyLKP();
			if (!FInAimCone( vecEnemyLKP ))
			{
				GetMotor()->SetIdealYawToTargetAndUpdate( vecEnemyLKP, AI_KEEP_YAW_SPEED );
			}
			else
			{
				GetMotor()->SetIdealYawAndUpdate( GetMotor()->GetIdealYaw(), AI_KEEP_YAW_SPEED );
			}

			if ( gpGlobals->curtime >= m_flNextAttack )
			{
				if ( IsActivityFinished() )
				{
					if (--m_nShots > 0)
					{
						ResetIdealActivity( ACT_RANGE_ATTACK1 );
						m_flLastAttackTime = gpGlobals->curtime;
						m_flNextAttack = gpGlobals->curtime + m_flShotDelay - 0.1;
					}
					else
					{
						TaskComplete();
					}
				}
			}
			else
			{
				// DevMsg("Wait\n");
			}
		}
		break;
#endif

	default:
		{
			BaseClass::RunTask( pTask );
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Purpose : Override to always shoot at eyes (for ducking behind things)
// Input   :
// Output  :
//------------------------------------------------------------------------------
Vector CNPC_Combine::BodyTarget( const Vector &posSrc, bool bNoisy ) 
{
	Vector result = BaseClass::BodyTarget( posSrc, bNoisy );

	// @TODO (toml 02-02-04): this seems wrong. Isn't this already be accounted for 
	// with the eye position used in the base BodyTarget()
	if ( GetFlags() & FL_DUCKING )
		result -= Vector(0,0,24);

	return result;
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
bool CNPC_Combine::FVisible( CBaseEntity *pEntity, int traceMask, CBaseEntity **ppBlocker )
{
	if( m_spawnflags & SF_COMBINE_NO_LOOK )
	{
		// When no look is set, if enemy has eluded the squad, 
		// he's always invisble to me
		if (GetEnemies()->HasEludedMe(pEntity))
		{
			return false;
		}
	}
	return BaseClass::FVisible(pEntity, traceMask, ppBlocker);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::Event_Killed( const CTakeDamageInfo &info )
{
	// if I was killed before I could finish throwing my grenade, drop
	// a grenade item that the player can retrieve.
	if( GetActivity() == ACT_RANGE_ATTACK2 )
	{
		if( m_iLastAnimEventHandled != COMBINE_AE_GREN_TOSS )
		{
			// Drop the grenade as an item.
			Vector vecStart;
			GetAttachment( "lefthand", vecStart );

			CBaseEntity *pItem = DropItem( "weapon_frag", vecStart, RandomAngle(0,360) );

			if ( pItem )
			{
				IPhysicsObject *pObj = pItem->VPhysicsGetObject();

				if ( pObj )
				{
					Vector			vel;
					vel.x = random->RandomFloat( -100.0f, 100.0f );
					vel.y = random->RandomFloat( -100.0f, 100.0f );
					vel.z = random->RandomFloat( 800.0f, 1200.0f );
					AngularImpulse	angImp	= RandomAngularImpulse( -300.0f, 300.0f );

					vel[2] = 0.0f;
					pObj->AddVelocity( &vel, &angImp );
				}

				// In the Citadel we need to dissolve this
				if ( PlayerHasMegaPhysCannon() )
				{
					CBaseCombatWeapon *pWeapon = static_cast<CBaseCombatWeapon *>(pItem);

					pWeapon->Dissolve( NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL );
				}
			}
		}
	}

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Combine::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	//!!!TODO; This might be a bit costly for what it does, if theres a better solution, find it!
	// If im getting shot by an ally and hes not being very cewth about it, at least try ducking out of his way
	CBaseCombatCharacter *dumbo = info.GetAttacker()->MyCombatCharacterPointer();
	if ( m_pSquad->SquadIsMember(info.GetAttacker()) || !IsMoving() && (dumbo && dumbo->IRelationType( this ) == D_LI) )
	{
		DesireCrouch();
		Crouch();
	}

	//Should this be in here???
#if 1
	if( info.GetDamageType() & DMG_BURN )
	{
		Scorch( 8, 75 );
	}
#endif

	return BaseClass::OnTakeDamage_Alive( info );
}

//-----------------------------------------------------------------------------
// Purpose: Override.  Don't update if I'm not looking
// Input  :
// Output : Returns true is new enemy, false is known enemy
//-----------------------------------------------------------------------------
bool CNPC_Combine::UpdateEnemyMemory( CBaseEntity *pEnemy, const Vector &position, CBaseEntity *pInformer )
{
	if( m_spawnflags & SF_COMBINE_NO_LOOK )
	{
		return false;
	}

	return BaseClass::UpdateEnemyMemory( pEnemy, position, pInformer );
}


//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_Combine::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();

	SetCustomInterruptCondition( COND_COMBINE_HIT_BY_BUGBAIT );
#ifdef USE_OLD_SHOOT
	if (gpGlobals->curtime < m_flNextAttack)
	{
		ClearCustomInterruptCondition( COND_CAN_RANGE_ATTACK1 );
		ClearCustomInterruptCondition( COND_CAN_RANGE_ATTACK2 );
	}
#endif

	// Allow interrupting reload if I can melee attack
	if ( IsCurSchedule( SCHED_RELOAD ) )
	{
		SetCustomInterruptCondition( COND_CAN_MELEE_ATTACK1 );
	}

	if ( !IsCurSchedule( SCHED_COMBINE_BURNING_STAND ) )
	{
		SetCustomInterruptCondition( COND_ON_FIRE );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Translate base class activities into combot activites
//-----------------------------------------------------------------------------
Activity CNPC_Combine::NPC_TranslateActivity( Activity eNewActivity )
{
	//Slaming this back to ACT_COMBINE_BUGBAIT since we don't want ANYTHING to change our activity while we burn.
	if ( HasCondition( COND_ON_FIRE ) )
		return BaseClass::NPC_TranslateActivity( ACT_COMBINE_BUGBAIT );

	switch ( eNewActivity )
	{
		case ACT_RANGE_ATTACK2:
			// grunt is going to a secondary long range attack. This may be a thrown 
			// grenade or fired grenade, we must determine which and pick proper sequence
			if ( IsCurSchedule( SCHED_COMBINE_AR2_ALTFIRE ) || Weapon_OwnsThisType( "weapon_grenadelauncher" ) )
			{
				return ( Activity )ACT_RANGE_ATTACK2;	//ACT_COMBINE_LAUNCH_GRENADE -- Use the weapon anim instead
			}
			else
			{
				return ( Activity )ACT_COMBINE_THROW_GRENADE;
			}
		break;
		
		case ACT_RANGE_ATTACK_THROW:
			eNewActivity = ACT_COMBINE_THROW_GRENADE;
			break;

		case ACT_RUN:
		case ACT_RUN_AIM:
			if ( m_iHealth <= COMBINE_LIMP_HEALTH )
			{
				// limp!
				eNewActivity = ACT_RUN_HURT;
			}
			else if ( IsCurSchedule( SCHED_TAKE_COVER_FROM_BEST_SOUND ) || IsCurSchedule( SCHED_FLEE_FROM_BEST_SOUND ) )
			{
				if ( random->RandomInt( 0, 1 ) && HaveSequenceForActivity( ACT_RUN_PROTECTED ) )
					eNewActivity = ACT_RUN_PROTECTED;
			}
		break;

		case ACT_WALK:
		case ACT_WALK_AIM:
			if ( m_iHealth <= COMBINE_LIMP_HEALTH )
			{
				// limp!
				eNewActivity = ACT_WALK_HURT;
			}
		break;

		case ACT_IDLE:
			if ( !IsCrouching() && ( m_NPCState == NPC_STATE_COMBAT || m_NPCState == NPC_STATE_ALERT ) )
			{
				eNewActivity = ACT_IDLE_ANGRY;
			}
		break;
	}

	// If you are doing a schedule that allows for move'n'shoots, then stay aimed
	if ( m_AssaultBehavior.IsRunning() || ShouldMoveAndShoot() )
	{
		switch ( eNewActivity )
		{
		case ACT_RUN:
			eNewActivity = ACT_RUN_AIM;
			break;

		case ACT_WALK:
			eNewActivity = ACT_WALK_AIM;
			break;

/*
		case ACT_IDLE:
			eNewActivity = ACT_IDLE_ANGRY;
			break;
*/
		}
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}


//-----------------------------------------------------------------------------
// Purpose: Overidden for human grunts because they hear the DANGER sound
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Combine::GetSoundInterests( void )
{
	return	SOUND_COMBAT			|
			SOUND_WORLD				|
			SOUND_PLAYER			|
			SOUND_DANGER			|
			SOUND_PHYSICS_DANGER	|
			SOUND_BULLET_IMPACT		|
			SOUND_MOVE_AWAY			|
			SOUND_VEHICLE			|
			SOUND_WEAPON;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this NPC can hear the specified sound
//-----------------------------------------------------------------------------
bool CNPC_Combine::QueryHearSound( CSound *pSound )
{
	if ( pSound->SoundContext() & SOUND_CONTEXT_COMBINE_ONLY )
		return true;

	if ( pSound->SoundContext() & SOUND_CONTEXT_EXCLUDE_COMBINE )
		return false;

	return BaseClass::QueryHearSound( pSound );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Combine::SelectSchedule( void )
{
	if ( IsWaitingToRappel() && BehaviorSelectSchedule() )
	{
		return BaseClass::SelectSchedule();
	}

	if ( HasCondition( COND_ON_FIRE ) )
		return SCHED_COMBINE_BURNING_STAND;

	int nSched = SelectFlinchSchedule();
	if ( nSched != SCHED_NONE )
		return nSched;

	if ( m_hForcedGrenadeTarget )
	{
		if ( m_flNextGrenadeCheck < gpGlobals->curtime )
		{
			Vector vecTarget = m_hForcedGrenadeTarget->WorldSpaceCenter();

			// If we can, throw a grenade at the target. 
			// Ignore grenade count / distance / etc
			if ( IsDemolition() )
			{
				float flDist = EnemyDistance(GetEnemy());
				if ( FVisible( m_hForcedGrenadeTarget ) && CanAltFireEnemy( 0, flDist ) )
				{
					m_vecAltFireTarget = vecTarget;
					m_hForcedGrenadeTarget = NULL;
					return SCHED_COMBINE_AR2_ALTFIRE;
				}
			}
			else if ( CheckCanThrowGrenade( vecTarget ) )
			{
				m_hForcedGrenadeTarget = NULL;
				return SCHED_COMBINE_FORCED_GRENADE_THROW;
			}
			
		}

		// Can't throw at the target, so lets try moving to somewhere where I can see it
		if ( !FVisible( m_hForcedGrenadeTarget ) )
			return SCHED_COMBINE_MOVE_TO_FORCED_GREN_LOS;
	}

	if ( m_NPCState != NPC_STATE_SCRIPT )
	{
		// If we're hit by bugbait, thrash around
		if ( HasCondition( COND_COMBINE_HIT_BY_BUGBAIT ) )
		{
			// Don't do this if we're mounting a func_tank
			if ( m_FuncTankBehavior.IsMounted() == true )
			{
				m_FuncTankBehavior.Dismount();
			}

			ClearCondition( COND_COMBINE_HIT_BY_BUGBAIT );
			return SCHED_COMBINE_BUGBAIT_DISTRACTION;
		}

		// These things are done in any state but dead and prone
		if (m_NPCState != NPC_STATE_DEAD && m_NPCState != NPC_STATE_PRONE )
		{
			// We've been told to move away from a target to make room for a grenade to be thrown at it
			if ( HasCondition( COND_HEAR_MOVE_AWAY ) )
			{
				return SCHED_MOVE_AWAY;
			}

			// Cower when physics objects are thrown at me
			if ( HasCondition( COND_HEAR_PHYSICS_DANGER ) )
			{
				return SCHED_FLINCH_PHYSICS;
			}

			// grunts place HIGH priority on running away from danger sounds.
			if ( HasCondition( COND_HEAR_DANGER ) )
			{
				CSound *pSound = GetBestSound( SOUND_DANGER );
				Assert( pSound != NULL );
				if ( pSound )
				{
					// I hear something dangerous, probably need to take cover.
					// dangerous sound nearby!, call it out
					const char *pSentenceName = "COMBINE_DANGER";

					CBaseEntity *pSoundOwner = pSound->m_hOwner;
					if ( pSoundOwner )
					{
						CBaseGrenade *pGrenade = dynamic_cast<CBaseGrenade *>(pSoundOwner);
						if ( pGrenade && pGrenade->GetThrower() )
						{
							// Absolute Buffoon
							pSentenceName = "COMBINE_FRIENDLY_FIRE";

							if ( IRelationType( pGrenade->GetThrower() ) != D_LI )
							{
								// Its not a completely botched throw
								pSentenceName = "COMBINE_GREN";
							}
						}
					}
					if ( IsElite() )
					{
						pSentenceName = "COMBINE_ELITE_DANGER";
					}
					m_Sentences.Speak( pSentenceName, SENTENCE_PRIORITY_MEDIUM );

					// If the sound is approaching danger, I have no enemy, and I don't see it, turn to face.
					if( !GetEnemy() && pSound->IsSoundType(SOUND_CONTEXT_DANGER_APPROACH) && !FInViewCone(pSound->GetSoundReactOrigin()) )
					{
						GetMotor()->SetIdealYawToTarget( pSound->GetSoundReactOrigin() );
						// If I can jump out of the way of the incoming danger, do so!
					//	return SCHED_COMBINE_EVADE;
					}
					return SCHED_TAKE_COVER_FROM_BEST_SOUND;
				}
				else
				{
					// Heard a danger sound, but its not a danger sou- wait what??
					return SCHED_TAKE_COVER_FROM_ORIGIN;
				}
			}
		}

		if( BehaviorSelectSchedule() )
		{
			return BaseClass::SelectSchedule();
		}
	}

	switch	( m_NPCState )
	{
	case NPC_STATE_IDLE:
		{
			if ( m_bShouldPatrol )
				return SCHED_COMBINE_PATROL;
		}
		// NOTE: Fall through - Idle and Alert use the same set of schedules!

	case NPC_STATE_ALERT:
		{
			if( HasCondition( COND_LIGHT_DAMAGE ) || HasCondition( COND_HEAVY_DAMAGE ) )
			{
				AI_EnemyInfo_t *pDanger = GetEnemies()->GetDangerMemory();
				if( pDanger && FInViewCone(pDanger->vLastKnownLocation) && !BaseClass::FVisible(pDanger->vLastKnownLocation) )
				{
					// I've been hurt, I'm facing the danger, but I don't see it, so move from this position.
					return SCHED_TAKE_COVER_FROM_ORIGIN;
				}
			}

#if 0
			// Remove nearby tripmines/satchels
			if ( HasCondition( COND_HEAR_THUMPER ) )
			{
				// If I see one by myself just avoid it
				
				// If im in a squad, get the squad away from it, then drop a grenade and run!
				if( m_pSquad && m_pSquad->GetSquadMemberNearestTo( Tripmine->GetAbsOrigin() ) == this && OccupyStrategySlot( SQUAD_SLOT_INVESTIGATE_SOUND ) )
				{
					return SCHED_COMBINE_DEFUSE;
				}
			}
#endif

			// Investigate nearby sounds
			if( HasCondition( COND_HEAR_COMBAT ) || HasCondition ( COND_HEAR_PLAYER ) )
			{
				CSound *pSound = GetBestSound();
				if( pSound )
				{
					if( m_pSquad && m_pSquad->GetSquadMemberNearestTo( pSound->GetSoundReactOrigin() ) == this && OccupyStrategySlot( SQUAD_SLOT_INVESTIGATE_SOUND ) )
					{
						return SCHED_INVESTIGATE_SOUND;	//TODO; new sched for walk+speaksentence
					}
					return SCHED_ALERT_FACE_BESTSOUND;
				}
			}

			// Don't patrol if I'm in the middle of an assault, because I'll never return to the assault. 
			if ( !m_AssaultBehavior.HasAssaultCue() )
			{
				if( m_bShouldPatrol || HasCondition( COND_COMBINE_SHOULD_PATROL ) )
					return SCHED_COMBINE_PATROL;
			}

			// Scan around for new enemies
			if ( HasCondition( COND_ENEMY_DEAD ) && SelectWeightedSequence( ACT_VICTORY_DANCE ) != ACTIVITY_NOT_AVAILABLE )
			{
				return SCHED_VICTORY_DANCE;
			}
		}
		break;

	case NPC_STATE_COMBAT:
		{
			int nSched = SelectCombatSchedule();
			if ( nSched != SCHED_NONE )
				return nSched;
		}
		break;
	}

	// no special cases here, call the base class
	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Select the combat schedule
//-----------------------------------------------------------------------------
int CNPC_Combine::SelectCombatSchedule()
{
	// -----------
	// Dead Enemy
	// -----------
	if ( HasCondition( COND_ENEMY_DEAD ) )
	{
		// call base class, all code to handle dead enemies is centralized there.
		return SCHED_NONE;
	}

	// --------------------------------------------------------------
	// New Enemy -- For each new enemy that you havent seen before
	// --------------------------------------------------------------
	if ( HasCondition( COND_NEW_ENEMY ) )
	{
		CBaseEntity *pEnemy = GetEnemy();
		if ( m_pSquad && pEnemy && m_iNewEnemies < 4 )
		{
		//	m_iNewEnemies++;	//Why isnt this working??
			m_iNewEnemies = m_iNewEnemies + 1;

			if ( HasCondition( COND_SEE_ENEMY ) )
			{
				SpeakSentence( 0 );
				// This increases the chance of nading' an increasingly large enemy mob
#if 0
				if( OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) && random->RandomFloat( 0, 100 ) < COMBINE_GRENADE_CHANCE )
				{
					if( HasCondition( COND_CAN_RANGE_ATTACK2 ) )
					{
						// Punt one roight int tha wankahs face, if I can see his face!
						return SCHED_RANGE_ATTACK2;
					}
					else if ( CanAltFireEnemy() )
					{
						// If I cant punt, launch!
						return SCHED_COMBINE_AR2_ALTFIRE;
					}
				}
#endif
			}

			if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) && OccupyStrategySlot( SQUAD_SLOT_ATTACK1 ) )
			{
				// Start suppressing if someone isn't firing already (SLOT_ATTACK1). This means
				// I'm the guy who spotted the enemy. I should react immediately, regardless if
				// im the leader or not.
				return SCHED_COMBINE_SUPPRESS;
			}
			else if ( m_pSquad->IsLeader( this ) )
			{
				// I'm the leader, but I didn't get the job suppressing the enemy. We know this because
				// this code only runs if the code above didn't assign me SCHED_COMBINE_SUPPRESS. If thats the case,
				// ease the restrictions to make sure I can do my first contact hand-signal.
				if ( HasCondition( COND_WEAPON_HAS_LOS ) && OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
				{
					return SCHED_COMBINE_SUPPRESS;
				}
				// If I still didnt get an attack slot, move to somewhere while shooting.
				return SCHED_COMBINE_ASSAULT;
			}
			else
			{
				// First contact, and I'm solo, or not the squad leader.
				if ( m_pSquad->GetLeader() && FOkToMakeSound( SENTENCE_PRIORITY_MEDIUM ) )
				{
					// Squelch anything that isn't high priority so the leader can speak
					JustMadeSound( SENTENCE_PRIORITY_MEDIUM );
				}

				return SCHED_TAKE_COVER_FROM_ENEMY;
			}
		}
	}

	// --------------------------------------------------------------
	// No Ammo
	// --------------------------------------------------------------
#ifdef HL2_EPISODIC
	if ( !HasCondition( COND_CAN_MELEE_ATTACK1 ) )
	{
		if ( HasCondition( COND_NO_PRIMARY_AMMO ) )
		{
			// No ammo, run for cover!
			return SCHED_HIDE_AND_RELOAD;
		}
		else if ( HasCondition( COND_LOW_PRIMARY_AMMO ) && OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
		{
			// Low ammo and im attacking, try to find a good spot to reload
			return SCHED_COMBINE_COVER_AND_RELOAD;
		}
	}
#else
	if ( ( HasCondition( COND_NO_PRIMARY_AMMO ) || HasCondition( COND_LOW_PRIMARY_AMMO ) ) && !HasCondition( COND_CAN_MELEE_ATTACK1 ) )
	{
		return SCHED_HIDE_AND_RELOAD;
	}
#endif

	// Only reload secondary if you cant shoot and/or are not in an attack slot
	if ( HasCondition( COND_NO_SECONDARY_AMMO ) && !HasCondition( COND_CAN_RANGE_ATTACK1 ) )
	{
		if ( OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
		{
			VacateStrategySlot();
		}
		return SCHED_RELOAD;
	}

	// --------------------------------------------------------------
	// Light Damage -- Any nonzero damage
	// --------------------------------------------------------------
	if ( HasCondition( COND_LIGHT_DAMAGE ) )
	{
		// Only try to take cover if we actually have an enemy!
		if ( GetEnemy() != NULL )
		{
			// NOTE; The take cover schedule automatically crouches if theres
			// no cover around, thus, this is really just to determine if
			// the soldier should immediately crouch or not. Crouching
			// soldiers stay "dug in", and do not move for light damage
			int iPercent = random->RandomInt(0,10);

			// FIXME: need to take cover for enemy dealing the damage
			if( !IsCrouching() )
			{
				if( iPercent <= 2 && CouldShootIfCrouching( GetEnemy() ) )	//25% chance
				{
					DesireCrouch();
				}
				else
				{
					if( iPercent >= 4 || m_iHealth <= COMBINE_LIMP_HEALTH )	//66% chance normally, but if I'm hurt, always run!
					{
						//!!!KELLY - this grunt was hit and is going to run to cover.
					//!	m_Sentences.Speak( "COMBINE_COVER", SENTENCE_PRIORITY_MEDIUM );
						return SCHED_TAKE_COVER_FROM_ENEMY;
					}
#if 0
					else if ( GetEnemy()->IsPlayer() )
					{
						float flDistSq = GetEnemy()->WorldSpaceCenter().DistToSqr( WorldSpaceCenter() );
						if ( flDistSq > Square(600) )
						{
							// If I'm not going to run, at least try to shake his aim
							return SCHED_COMBINE_EVADE;
						}
					}
#endif
				}
			}
		}
		else
		{
			// Taking damage should have entered a "null" enemy position into memory
			Assert( GetEnemy() != NULL );
		//!	return SCHED_SMALL_FLINCH;
#if 0
			//TODO; If damage came from player, he must be shooting me from a cheeky spot, take cover!
#ifdef HL2MP
			CBasePlayer *pPlayer = AI_GetNearestPlayer( GetAbsOrigin() );
#else
			CBasePlayer *pPlayer = AI_GetSinglePlayer();
#endif
			if( pPlayer && !BaseClass::FVisible(pPlayer->vLastKnownLocation) )
			{
				return SCHED_TAKE_COVER_FROM_ORIGIN;
			}
#endif
		}
	}

	// --------------------------------------------------------------
	// Run away!
	// --------------------------------------------------------------
	if ( IRelationType( GetEnemy() ) == D_FR )
	{
		if (HasCondition( COND_SEE_ENEMY )	|| 
			HasCondition( COND_SEE_FEAR )	|| 
			HasCondition( COND_LIGHT_DAMAGE ) || 
			HasCondition( COND_HEAVY_DAMAGE ))
		{
			FearSound();
			return SCHED_RUN_FROM_ENEMY;
		}

		// If I've seen the enemy recently, cower. Ignore the time for unforgettable enemies.
		AI_EnemyInfo_t *pMemory = GetEnemies()->Find( GetEnemy() );
		if ( (pMemory && pMemory->bUnforgettable) || (GetEnemyLastTimeSeen() > (gpGlobals->curtime - 5.0)) )
		{
			// If we're facing him, just look ready. Otherwise, face him.
			if ( FInAimCone( GetEnemy()->EyePosition() ) )
				return SCHED_COMBAT_STAND;

			return SCHED_FEAR_FACE;
		}
	}

	int attackSchedule = SelectScheduleAttack();
	if ( attackSchedule != SCHED_NONE )
		return attackSchedule;

	// --------------------------------------------------------------
	// No LOS - Nobody in the squad can see the enemy
	// --------------------------------------------------------------
	if ( HasCondition( COND_ENEMY_OCCLUDED ) )
	{
		if ( !IsUsingTacticalVariant(TACTICAL_VARIANT_DEFEND_POSITION) )
		{
			if( GetEnemy() && !(GetEnemy()->GetFlags() & FL_NOTARGET) && OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
			{
				// Charge in and break the enemy's cover!
				if( IsUsingTacticalVariant(TACTICAL_VARIANT_PRESSURE_ENEMY) || IRelationPriority(GetEnemy()) >= 10 )
					return SCHED_COMBINE_PRESS_ATTACK;

				// Try to get into a better shooting position!
				return SCHED_ESTABLISH_LINE_OF_FIRE;
			}
		}

		// If I'm a long, long way away, establish a LOF anyway. Once I get there I'll
		// start respecting the squad slots again.
		// TODO; Get max weapon range
		float flDistSq = GetEnemy()->WorldSpaceCenter().DistToSqr( WorldSpaceCenter() );
		if ( flDistSq > Square(3000) )
		{
			return SCHED_ESTABLISH_LINE_OF_FIRE;
		}

		// Otherwise, tuck in.
		Remember( bits_MEMORY_INCOVER );
		return SCHED_COMBINE_WAIT_IN_COVER;
	}

	// --------------------------------------------------------------
	// Another member of the squad can see the enemy
	// --------------------------------------------------------------
	if ( HasCondition( COND_SEE_ENEMY ) && !HasCondition( COND_CAN_RANGE_ATTACK1 ) )
	{
		// Charge in!
		if ( HasCondition( COND_TOO_FAR_TO_ATTACK ) || IsUsingTacticalVariant( TACTICAL_VARIANT_PRESSURE_ENEMY ) )
		{
			return SCHED_COMBINE_PRESS_ATTACK;
		}
		else if ( OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
		{
			// Run in shooting!
			return SCHED_COMBINE_ASSAULT;
		}

		// Else just fire from where I am
		return SCHED_ESTABLISH_LINE_OF_FIRE;
	}

	return SCHED_NONE;
}

//-----------------------------------------------------------------------------
// Should we charge the player?
//-----------------------------------------------------------------------------
bool CNPC_Combine::ShouldChargePlayer()
{
	return GetEnemy() && GetEnemy()->IsPlayer() && PlayerHasMegaPhysCannon() && !IsLimitingHintGroups();
}

//-----------------------------------------------------------------------------
// Select attack schedules
//-----------------------------------------------------------------------------
#define COMBINE_MEGA_PHYSCANNON_ATTACK_DISTANCE	192
#define COMBINE_MEGA_PHYSCANNON_ATTACK_DISTANCE_SQ	(COMBINE_MEGA_PHYSCANNON_ATTACK_DISTANCE*COMBINE_MEGA_PHYSCANNON_ATTACK_DISTANCE)

int CNPC_Combine::SelectScheduleAttack()
{
	// Drop a grenade?
	if ( HasCondition( COND_COMBINE_DROP_GRENADE ) )
		return SCHED_COMBINE_DROP_GRENADE;

	// Kick attack?
	if ( HasCondition( COND_CAN_MELEE_ATTACK1 ) )
	{
		//TODO; If the enemy in question is too fyat for me (guards, mechs, etc.), run away!
		return SCHED_MELEE_ATTACK1;
	}

	// If I'm fighting a turret, I can't really hurt it with bullets, so become grenade happy.
	if ( GetEnemy() && FClassnameIs(GetEnemy(), "npc_turret_floor") )
	{
		// Don't do this until I've been fighting the turret for a few seconds
		float flTimeAtFirstHand = GetEnemies()->TimeAtFirstHand(GetEnemy());
		if ( flTimeAtFirstHand != AI_INVALID_TIME )
		{
			float flTimeEnemySeen = gpGlobals->curtime - flTimeAtFirstHand;
			if ( flTimeEnemySeen > 4.0 )
			{
				if ( HasCondition( COND_CAN_RANGE_ATTACK2 ) && OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) )
					return SCHED_RANGE_ATTACK2;
			}
		}

		// If we're not in the viewcone of the turret, run up and hit it. Do this a bit later to
		// give other squadmembers a chance to throw a grenade before I run in.
		if ( !GetEnemy()->MyNPCPointer()->FInViewCone( this ) && OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) )
			return SCHED_COMBINE_CHARGE_TURRET;
	}

	// When fighting against the player who's wielding a mega-physcannon, 
	// always close the distance if possible
	// But don't do it if you're in a nav-limited hint group
	if ( ShouldChargePlayer() )
	{
		float flDistSq = GetEnemy()->WorldSpaceCenter().DistToSqr( WorldSpaceCenter() );
		if ( flDistSq <= COMBINE_MEGA_PHYSCANNON_ATTACK_DISTANCE_SQ )
		{
			if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) && OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
				return SCHED_RANGE_ATTACK1;
		}

		if ( HasCondition( COND_SEE_ENEMY ) && !IsUnreachable( GetEnemy() ) )
		{
			return SCHED_COMBINE_CHARGE_PLAYER;
		}
	}

	// Can I shoot?
	if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) )
	{
		// Fully engage if allowed, otherwise give a short burst then hide
		if ( m_pSquad )
		{
			if ( m_pSquad->GetLeader() != NULL )
			{
				// if the enemy has eluded the squad and a squad member has just located the enemy
				// and the enemy does not see the squad member, issue a call to the squad to waste a 
				// little time and give the player a chance to turn.
				CAI_BaseNPC *pSquadLeader = m_pSquad->GetLeader()->MyNPCPointer();
				if ( pSquadLeader && pSquadLeader->EnemyHasEludedMe() && !HasCondition( COND_ENEMY_FACING_ME ) )	//HasCondition( COND_BEHIND_ENEMY )
				{
					return SCHED_COMBINE_FOUND_ENEMY;
				}
			}
			if ( OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )	//GetShotRegulator()->IsInRestInterval()
			{
				// Give a quick burst then hide/move up
				return SCHED_RANGE_ATTACK1;
			}
			else if ( OccupyStrategySlot( SQUAD_SLOT_ATTACK_OCCLUDER ) )
			{
				// Suppress the enemy
				return SCHED_COMBINE_SUPPRESS;
			}
			return SCHED_TAKE_COVER_FROM_ENEMY;
		}
	}
	else if ( HasCondition( COND_WEAPON_SIGHT_OCCLUDED ) )
	{
		// If they are hiding behind something that we can destroy, like a physics prop, start shooting at it
		CBaseEntity *pBlocker = GetEnemyOccluder();
		if ( pBlocker && pBlocker->GetHealth() > 0 && OccupyStrategySlot( SQUAD_SLOT_ATTACK_OCCLUDER ) )
		{
			return SCHED_SHOOT_ENEMY_COVER;
		}
	}

	// Can't fire our weapon... can we throw a grenade?
	if ( HasCondition( COND_CAN_RANGE_ATTACK2 )  )
	{
		if( OccupyStrategySlotRange( SQUAD_SLOT_GRENADE1, SQUAD_SLOT_GRENADE2 ) )
		{
			return SCHED_RANGE_ATTACK2;
		}
	}

	return SCHED_NONE;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Combine::TranslateSchedule( int scheduleType ) 
{
	switch( scheduleType )
	{
	case SCHED_IDLE_STAND:
	case SCHED_ALERT_STAND:
		if ( GetEnemy() == NULL )
		{
			if ( m_pSquad && GetActiveWeapon() )
			{
				// If I have less than half a clip when not fighting, reload!
				CBaseCombatWeapon *pWeapon = GetActiveWeapon();

				if( pWeapon->UsesClipsForAmmo1() && pWeapon->Clip1() < ( pWeapon->GetMaxClip1() * .5 ) )
				{
					return SCHED_RELOAD;
				}
			}
			Stand();	//Always stand when in a idle schedule with no enemy, but focus on reloading first!
		}
		break;

	case SCHED_TAKE_COVER_FROM_ENEMY:
		{
			if ( m_pSquad )
			{
				// Have to explicitly check innate range attack condition as may have weapon with range attack 2
#if 0
				if (	g_pGameRules->IsSkillLevel( SKILL_HARD )	&& 
 						HasCondition( COND_CAN_RANGE_ATTACK2 )		&&
						OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) )
				{
					return SCHED_COMBINE_GRENADE_COVER1;
				}
#endif
				if ( GetEnemy() != NULL && m_pSquad->IsLeader( this ) && CanDoSignal() )
				{
					m_flNextSignalTime = gpGlobals->curtime + random->RandomFloat( 8, 15 );
					return SCHED_COMBINE_SIGNAL_TAKE_COVER;
				}

				return SCHED_COMBINE_TAKE_COVER1;
			}
			else
			{
				return SCHED_COMBINE_TAKE_COVER1;	//SCHED_COMBINE_TAKE_COVER2
			}
		}

	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
		return SCHED_COMBINE_TAKE_COVER_FROM_BEST_SOUND;
		break;

	case SCHED_COMBINE_TAKECOVER_FAILED:
		{
			if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) )
			{
				//Theres absolutely no cover around, so try crouching
				if( !IsCrouching() && CouldShootIfCrouching(GetEnemy()) )
				{
					DesireCrouch();
				}
				return TranslateSchedule( SCHED_RANGE_ATTACK1 );
			}
			return TranslateSchedule( SCHED_FAIL );
		}

	case SCHED_FAIL_ESTABLISH_LINE_OF_FIRE:
		{
			if ( !m_AssaultBehavior.HasAssaultCue() )
			{
				// Don't patrol if I'm in the middle of an assault, because 
				// I'll never return to the assault. 
				if ( GetEnemy() )
				{
					RememberUnreachable( GetEnemy() );
				}

				return TranslateSchedule( SCHED_COMBINE_PATROL );
			}
			return TranslateSchedule( SCHED_TAKE_COVER_FROM_ENEMY );
		}

	case SCHED_COMBINE_ASSAULT:
		{
			CBaseEntity *pEntity = GetEnemy();
			// FIXME: this should be generalized by the schedules that are selected, or in the definition of 
			// what "cover" means (i.e., trace attack vulnerability vs. physical attack vulnerability
			if (pEntity && pEntity->MyNPCPointer())
			{
				if ( !(pEntity->MyNPCPointer()->CapabilitiesGet( ) & bits_CAP_WEAPON_RANGE_ATTACK1) )
				{
					return TranslateSchedule( SCHED_ESTABLISH_LINE_OF_FIRE );
				}
			}
			else if (GetHintGroup() != NULL_STRING)
			{
				// Don't run about if there's a hint group
				return TranslateSchedule( SCHED_ESTABLISH_LINE_OF_FIRE );
			}
			return SCHED_COMBINE_ASSAULT;
		}

	case SCHED_ESTABLISH_LINE_OF_FIRE:
		{
			// Combine dont flush enemies out with grenades on easy, they only throw when shooting isnt possible/reloading
			if ( m_pSquad && !g_pGameRules->IsSkillLevel( SKILL_EASY ) && random->RandomInt(0,1) )	//(g_pGameRules->IsSkillLevel( SKILL_HARD ) || random->RandomInt( 0, 2 ) > 0)
			{
				// This is where the magic happens
				/*
				//NOTENOTE; As nice as it would be to, we cant use cond_can_range_attack2 here because
				//the way the conditions are gathered are independent from any schedule selection or function calls.
				//This is bad for situations where you really want grenades to be thrown on a per-schedule/on-call basis,
				//such as always throwing one just before charging in on a completely unseen enemy.
				//Because of this, this is the only place where the actual grenade conditions function has to be called upon.
				//The one upside of this is that a custom target can be given to check, in this case where I think the enemy
				//is, not just where I last saw him.
				*/
				CBaseEntity *pEnemy = GetEnemy();
				Vector vecEnemyLKP = GetEnemyLKP();

//!				float flDist = UTIL_DistApprox2D( Center(), vecEnemyLKP );
				float flDist = EnemyDistance(pEnemy);

				Vector vecTarget = vecEnemyLKP + pEnemy->GetAbsVelocity();
				if( pEnemy && HasCondition( COND_ENEMY_OCCLUDED ) && GetGrenadeConditions(0, flDist, vecTarget) && pEnemy->IsPlayer() )
				{
					// If I could throw a grenade where I think the enemy is,
					// do so first, then move into LOS!
					if( OccupyStrategySlot( SQUAD_SLOT_ATTACK1 ) || OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) )
						return SCHED_RANGE_ATTACK2;	//SCHED_COMBINE_GRENADE_ASSAULT
				}
			}
			return SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE;
		}

	case SCHED_COMBINE_PRESS_ATTACK:
		{
			if ( m_pSquad )
			{
				if ( GetEnemy() != NULL && m_pSquad->IsLeader( this ) && CanDoSignal() )
				{
					m_flNextSignalTime = gpGlobals->curtime + random->RandomFloat( 5, 10 );
					return SCHED_COMBINE_SIGNAL_PRESS_ATTACK;
				}
			}
			return SCHED_COMBINE_PRESS_ATTACK;
		}

	case SCHED_COMBINE_CHARGE_PLAYER:
		{
			if ( m_pSquad )
			{
				if ( GetEnemy() != NULL && m_pSquad->IsLeader( this ) && CanDoSignal() )
				{
					m_flNextSignalTime = gpGlobals->curtime + random->RandomFloat( 5, 10 );
					return SCHED_COMBINE_SIGNAL_PRESS_ATTACK;
				}
			}
			return SCHED_COMBINE_CHARGE_PLAYER;
		}

	case SCHED_HIDE_AND_RELOAD:
		{
#ifdef HL2_EPISODIC
			if ( HasCondition( COND_CAN_RANGE_ATTACK2 ) && OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) )
			{
				return SCHED_COMBINE_GRENADE_AND_RELOAD;
			}
#else
			if ( HasCondition( COND_CAN_RANGE_ATTACK2 ) && random->RandomInt( 0, 2 ) == 2 )
			{
				if ( OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) || !m_pSquad )
					return SCHED_COMBINE_GRENADE_AND_RELOAD;
			}
#endif

			// No running away in the citadel!
			if ( ShouldChargePlayer() || IsUsingTacticalVariant( TACTICAL_VARIANT_PRESSURE_ENEMY ) )
			{
				return SCHED_RELOAD;
			}
			return SCHED_COMBINE_HIDE_AND_RELOAD;
		}

	case SCHED_COMBINE_COVER_AND_RELOAD:
		{
#if 0
			// Empty whats left if the enemy is in my face
			float flDistToEnemy = ( GetEnemy()->GetAbsOrigin() - GetAbsOrigin() ).Length();
			if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) && flDistToEnemy < 300 )
			{
				return SCHED_RANGE_ATTACK1;
			}
#endif

#ifndef HL2_EPISODIC
			if ( HasCondition( COND_CAN_RANGE_ATTACK2 ) && OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) )
			{
				return SCHED_COMBINE_GRENADE_AND_RELOAD;
			}
#endif
			return TranslateSchedule( SCHED_HIDE_AND_RELOAD );	//SCHED_COMBINE_COVER_AND_RELOAD
		}

	case SCHED_RANGE_ATTACK1:
		{
			if ( HasCondition( COND_NO_PRIMARY_AMMO ) )
			{
				// Ditch the strategy slot for attacking (which we just reserved!)
				if ( OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
				{
					VacateStrategySlot();
				}
				return TranslateSchedule( SCHED_HIDE_AND_RELOAD );
			}
			if ( OccupyStrategySlot( SQUAD_SLOT_SPECIAL_ATTACK ) )
			{
				float flDist = EnemyDistance(GetEnemy());
				if ( CanAltFireEnemy( 0, flDist ) )
				{
					// Since I'm holding this squadslot, no one else can try right now. If I die before the shot 
					// goes off, I won't have affected anyone else's ability to use this attack at their nearest
					// convenience.
					return SCHED_COMBINE_AR2_ALTFIRE;
				}
			}

			if ( CrouchIsDesired() && !HasCondition( COND_HEAVY_DAMAGE ) ) //&& !HasCondition( COND_REPEATED_DAMAGE ) )
			{
				// See if we can crouch and shoot
				if (GetEnemy() != NULL)
				{
					float dist = (GetLocalOrigin() - GetEnemy()->GetLocalOrigin()).Length();

					// only crouch if they are relatively far away
					if (dist > COMBINE_MIN_CROUCH_DISTANCE)
					{
						// try crouching
						Crouch();

						Vector targetPos = GetEnemy()->BodyTarget(GetActiveWeapon()->GetLocalOrigin());

						// if we can't see it crouched, stand up
						if (!WeaponLOSCondition(GetLocalOrigin(),targetPos,false))
						{
							Stand();
						}
					}
				}
			}
			else
			{
				// always assume standing
				Stand();
			}
			return SCHED_COMBINE_RANGE_ATTACK1;
		}

	case SCHED_MELEE_ATTACK1:
		{
			// Stand for melee
		//	Stand();

			return SCHED_COMBINE_MELEE_ATTACK1;
		}

	case SCHED_RANGE_ATTACK2:
		{
			// If my weapon can range attack 2 use the weapon
			if (GetActiveWeapon() && GetActiveWeapon()->CapabilitiesGet() & bits_CAP_WEAPON_RANGE_ATTACK2)
			{
				return SCHED_RANGE_ATTACK2;
			}
			// Otherwise use innate attack
			return SCHED_COMBINE_RANGE_ATTACK2;
		}

	case SCHED_COMBINE_FOUND_ENEMY:
		return SCHED_COMBINE_FOUND_ENEMY;
		break;

		// SCHED_COMBINE_SWEEP:

	case SCHED_COMBAT_FACE:
		return SCHED_COMBINE_COMBAT_FACE;
		break;

	case SCHED_ALERT_FACE:
		return SCHED_COMBINE_ALERT_FACE;
		break;

	case SCHED_VICTORY_DANCE:
		{
			if ( m_pSquad )
			{
				if ( m_pSquad->IsLeader( this ) )
				{
					return SCHED_COMBINE_VICTORY_DANCE;
				}
				return TranslateSchedule( SCHED_COMBINE_PATROL );
			}
		}

	case SCHED_COMBINE_SUPPRESS:
		{
			if ( GetEnemy() != NULL && m_pSquad->IsLeader( this ) && CanDoSignal() ) //&& m_bFirstEncounter )
			{
				m_bFirstEncounter = false;// after first encounter, leader won't issue handsigns anymore when he has a new enemy
				m_flNextSignalTime = gpGlobals->curtime + random->RandomFloat( 8, 15 );
				return SCHED_COMBINE_SIGNAL_SUPPRESS;
			}
			return SCHED_COMBINE_SUPPRESS;
		}
#if 0
	case SCHED_COMBINE_REPEL:
		{
			Vector vecVelocity = GetAbsVelocity();
			if (vecVelocity.z > -128)
			{
				vecVelocity.z -= 32;
				SetAbsVelocity( vecVelocity );
			}
			return SCHED_COMBINE_REPEL;
		}

	case SCHED_COMBINE_REPEL_ATTACK:
		{
			Vector vecVelocity = GetAbsVelocity();
			if (vecVelocity.z > -128)
			{
				vecVelocity.z -= 32;
				SetAbsVelocity( vecVelocity );
			}
			return SCHED_COMBINE_REPEL_ATTACK;
		}
#endif

	case SCHED_FAIL:
		{
			if ( GetEnemy() != NULL )
			{
				return SCHED_COMBINE_COMBAT_FAIL;
			}
			return SCHED_FAIL;
		}

	case SCHED_COMBINE_PATROL:
		{
			// If I have an enemy, don't go off into random patrol mode.
			if ( GetEnemy() && GetEnemy()->IsAlive() )
				return SCHED_COMBINE_PATROL_ENEMY;

			return SCHED_COMBINE_PATROL;
		}
	}

	return BaseClass::TranslateSchedule( scheduleType );
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CNPC_Combine::HandleAnimEvent( animevent_t *pEvent )
{
	Vector vecShootDir;
	Vector vecShootOrigin;
	bool handledEvent = false;

	if (pEvent->type & AE_TYPE_NEWEVENTSYSTEM)
	{
		if ( pEvent->event == COMBINE_AE_BEGIN_ALTFIRE )
		{
			EmitSound( "Weapon_CombineGuard.Special1" );
			handledEvent = true;
		}
		else if ( pEvent->event == COMBINE_AE_ALTFIRE )
		{
			animevent_t fakeEvent;

			fakeEvent.pSource = this;
			fakeEvent.event = EVENT_WEAPON_AR2_ALTFIRE;
			GetActiveWeapon()->Operator_HandleAnimEvent( &fakeEvent, this );

			// Stop other squad members from combine balling for a while.
			DelaySquadAltFireAttack( 10.0f );

			// I'm disabling this decrementor. At the time of this change, the elites
			// don't bother to check if they have grenades anyway. This means that all
			// elites have infinite combine balls, even if the designer marks the elite
			// as having 0 grenades. By disabling this decrementor, yet enabling the code
			// that makes sure the elite has grenades in order to fire a combine ball, we
			// preserve the legacy behavior while making it possible for a designer to prevent
			// elites from shooting combine balls by setting grenades to '0' in hammer. (sjb) EP2_OUTLAND_10
			// m_iNumGrenades--;

			handledEvent = true;
		}
		else
		{
			BaseClass::HandleAnimEvent( pEvent );
		}
	}
	else
	{
		switch( pEvent->event )
		{
		case COMBINE_AE_AIM:	
			{
				handledEvent = true;
				break;
			}

		case COMBINE_AE_RELOAD:
			// We never actually run out of ammo, just need to refill the clip
			if (GetActiveWeapon())
			{
				GetActiveWeapon()->WeaponSound( RELOAD_NPC );
				GetActiveWeapon()->m_iClip1 = GetActiveWeapon()->GetMaxClip1(); 
				GetActiveWeapon()->m_iClip2 = GetActiveWeapon()->GetMaxClip2();  
			}
			ClearCondition(COND_LOW_PRIMARY_AMMO);
			ClearCondition(COND_NO_PRIMARY_AMMO);
			ClearCondition(COND_NO_SECONDARY_AMMO);
			handledEvent = true;
			break;

		case COMBINE_AE_GREN_TOSS:
			{
				Vector vecSpin;
				vecSpin.x = random->RandomFloat( -1000.0, 1000.0 );
				vecSpin.y = random->RandomFloat( -1000.0, 1000.0 );
				vecSpin.z = random->RandomFloat( -1000.0, 1000.0 );

				Vector vecStart;
				GetAttachment( "lefthand", vecStart );

				if( m_NPCState == NPC_STATE_SCRIPT )
				{
					// Use a fixed velocity for grenades thrown in scripted state.
					// Grenades thrown from a script do not count against grenades remaining for the AI to use.
					Vector forward, up, vecThrow;

					GetVectors( &forward, NULL, &up );
					vecThrow = forward * 750 + up * 175;
					Fraggrenade_Create( vecStart, vec3_angle, vecThrow, vecSpin, this, COMBINE_GRENADE_TIMER, true );
				}
				else
				{
					// Use the Velocity that AI gave us.
					Fraggrenade_Create( vecStart, vec3_angle, m_vecTossVelocity, vecSpin, this, COMBINE_GRENADE_TIMER, true );
					m_iNumGrenades--;
				}

				m_lastGrenadeCondition = COND_NONE;
				ClearCondition( COND_CAN_RANGE_ATTACK2 );
				if ( g_pGameRules->IsSkillLevel( SKILL_HARD ) )
					m_flNextGrenadeCheck = gpGlobals->curtime + (COMBINE_GRENADE_TIME * random->RandomFloat(0.5f, 0.75f));// Cut down the default time before considering another throw
				else
					m_flNextGrenadeCheck = gpGlobals->curtime + (COMBINE_GRENADE_TIME * random->RandomFloat(0.75f, 1.25f));// wait five seconds before considering another throw

			//	DevMsg( "Throwing a grenade!\n");
			}
			handledEvent = true;
			break;

			// UNUSED/LEGACY -- SEE SMG1.cpp
		case COMBINE_AE_GREN_LAUNCH:
			{
				CPASAttenuationFilter filter( this );
				EmitSound( filter, entindex(), "NPC_Combine.GrenadeLaunch" );
				/*
				CBaseEntity *pGrenade = CreateNoSpawn( "npc_contactgrenade", Weapon_ShootPosition(), vec3_angle, this );
				pGrenade->KeyValue( "velocity", m_vecTossVelocity );
				pGrenade->Spawn( );
				*/
				CGrenadeAR2 *pGrenade = (CGrenadeAR2*)Create( "grenade_ar2", Weapon_ShootPosition(), vec3_angle, this );
				pGrenade->SetAbsVelocity( m_vecTossVelocity );
				pGrenade->SetLocalAngularVelocity( QAngle( random->RandomFloat( -100, -500 ), 0, 0 ) );
				pGrenade->SetThrower( this );
				pGrenade->SetDamage( sk_npc_dmg_smg1_grenade.GetFloat() );
				
				m_lastGrenadeCondition = COND_NONE;
				ClearCondition( COND_CAN_RANGE_ATTACK2 );
				if ( g_pGameRules->IsSkillLevel( SKILL_HARD ) )
					m_flNextGrenadeCheck = gpGlobals->curtime + random->RandomFloat( 5, 8 );// wait a random amount of time before shooting again
				else
					m_flNextGrenadeCheck = gpGlobals->curtime + 8;// wait eight seconds before even looking again to see if a grenade can be thrown

			//	DevMsg( "Using grenade launcher to flush you out!\n");
			}
			handledEvent = true;
			break;

		case COMBINE_AE_GREN_DROP:
			{
				Vector vecStart;
				GetAttachment( "lefthand", vecStart );

				Vector forward, up, vecThrow;

				AngleVectors( GetLocalAngles(), &forward, NULL, &up );
				vecThrow = forward * 10; //+ up * 5

				Fraggrenade_Create( vecStart, vec3_angle, vecThrow, vec3_origin, this, (COMBINE_GRENADE_TIMER + 1), true );
				m_iNumGrenades--;

				m_lastGrenadeCondition = COND_NONE;
				ClearCondition( COND_CAN_RANGE_ATTACK2 );
				m_flNextGrenadeCheck = gpGlobals->curtime + (COMBINE_GRENADE_TIME * random->RandomFloat(0.75f, 1.25f));

			//	DevMsg( "Dropping a grenade!\n");
			}
			handledEvent = true;
			break;

		case COMBINE_AE_KICK:
			{
				// Does no damage, because damage is applied based upon whether the target can handle the interaction
				CBaseEntity *pHurt = CheckTraceHullAttack( 70, -Vector(16,16,18), Vector(16,16,18), 0, DMG_CLUB );
				CBaseCombatCharacter* pBCC = ToBaseCombatCharacter( pHurt );
				if (pBCC)
				{
					Vector forward, up;
					AngleVectors( GetLocalAngles(), &forward, NULL, &up );

					if ( !pBCC->DispatchInteraction( g_interactionCombineBash, NULL, this ) )
					{
						if ( pBCC->IsPlayer() )
						{
							pBCC->ViewPunch( QAngle(-12,-7,0) );
							pHurt->ApplyAbsVelocityImpulse( forward * 100 + up * 50 );
						}

						CTakeDamageInfo info( this, this, m_nKickDamage, DMG_CLUB );
						CalculateMeleeDamageForce( &info, forward, pBCC->GetAbsOrigin() );
						pBCC->TakeDamage( info );

						EmitSound( "NPC_Combine.WeaponBash" );
					}
				}
				m_Sentences.Speak( "COMBINE_KICK", SENTENCE_PRIORITY_INVALID );
			}
			handledEvent = true;
			break;

		case COMBINE_AE_CAUGHT_ENEMY:
			m_Sentences.Speak( "COMBINE_ALERT" );
			handledEvent = true;
			break;

		default:
			BaseClass::HandleAnimEvent( pEvent );
			break;
		}
	}

	if( handledEvent )
	{
		m_iLastAnimEventHandled = pEvent->event;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get shoot position of BCC at an arbitrary position
// Input  :
// Output :
//-----------------------------------------------------------------------------
Vector CNPC_Combine::Weapon_ShootPosition( )
{
	Vector right;
	GetVectors( NULL, &right, NULL );

	bool bStanding = !IsCrouching();
	if ((CapabilitiesGet() & bits_CAP_DUCK) )
	{
		if ( IsCrouchedActivity( GetActivity() ) )
		{
			bStanding = false;
		}
	}

	// FIXME: rename this "estimated" since it's not based on animation
	// FIXME: the orientation won't be correct when testing from arbitary positions for arbitary angles

	if  ( bStanding )
	{
		if( HasShotgun() )
		{
			return GetAbsOrigin() + COMBINE_SHOTGUN_STANDING_POSITION + right * 8;
		}
		else
		{
			return GetAbsOrigin() + COMBINE_GUN_STANDING_POSITION + right * 8;
		}
	}
	else
	{
		if( HasShotgun() )
		{
			return GetAbsOrigin() + COMBINE_SHOTGUN_CROUCHING_POSITION + right * 8;
		}
		else
		{
			return GetAbsOrigin() + COMBINE_GUN_CROUCHING_POSITION + right * 8;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Announce an assault if the enemy can see me and we are pretty 
//			close to him
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_Combine::AnnounceAssault( void )
{
	if (!m_pSquad)
		return;

	if (random->RandomInt(0,1))
		return;

	const char *pSentenceName = "COMBINE_ASSAULT";
	if( IsElite() )
		pSentenceName = "COMBINE_ELITE_ASSAULT";

	m_Sentences.Speak( pSentenceName, SENTENCE_PRIORITY_HIGH );
}

void CNPC_Combine::AnnounceEnemyType( CBaseEntity *pEnemy )
{
	if ( gpGlobals->curtime > m_flNextAlertSoundTime )
	{
		const char *pSentenceName = "COMBINE_MONST";
		if ( random->RandomInt(0,1) )
		{
			//This would be done with the sentences just refering to COMBINE_MONST,
			//But that doesnt seem to work, so this is hardcoded for now
			switch ( pEnemy->Classify() )
			{
			case CLASS_PLAYER:
				pSentenceName = "COMBINE_ALERT";
				break;

			case CLASS_PLAYER_ALLY:
			case CLASS_CITIZEN_REBEL:
			case CLASS_CITIZEN_PASSIVE:
			case CLASS_CONSCRIPT:
				pSentenceName = "COMBINE_MONST_CITIZENS";
				break;

			case CLASS_PLAYER_ALLY_VITAL:
				pSentenceName = "COMBINE_MONST_CHARACTER";
				break;

			case CLASS_VORTIGAUNT:
				pSentenceName = "COMBINE_MONST_VORTS";
				break;

			case CLASS_ANTLION:
			case CLASS_HOUNDEYE:
				pSentenceName = "COMBINE_MONST_BUGS";
				break;

			case CLASS_ZOMBIE:
				pSentenceName = "COMBINE_MONST_ZOMBIES";
				break;

			case CLASS_HEADCRAB:
			case CLASS_BARNACLE:
				pSentenceName = "COMBINE_MONST_PARASITES";
				break;
			}
		}
		if ( IsElite() )
		{
			pSentenceName = "COMBINE_ELITE_ALERT";
		}
		m_Sentences.Speak( pSentenceName, SENTENCE_PRIORITY_HIGH );
		m_flNextAlertSoundTime = gpGlobals->curtime + random->RandomFloat( 15, 25 );
	}
}

void CNPC_Combine::AnnounceEnemyKill( CBaseEntity *pEnemy )
{
	if (!pEnemy )
		return;

	if ( IsElite() )
		return;

	// 50% chance
	if (random->RandomInt(0,1))
		return;

	const char *pSentenceName = "COMBINE_KILL_MONST";
	switch ( pEnemy->Classify() )
	{
	case CLASS_PLAYER:
		pSentenceName = "COMBINE_PLAYER_DEAD";
		break;

	case CLASS_COMBINE:
		pSentenceName = "COMBINE_FRIENDLY_FIRE_REPLY";
		break;

	/*!!!Disabled for now, re-enable once speech is added
	// added sentences for these guys
	case CLASS_PLAYER_ALLY:
	case CLASS_CITIZEN_REBEL:
	case CLASS_CITIZEN_PASSIVE:
	case CLASS_CONSCRIPT:
	case CLASS_VORTIGAUNT:
	case CLASS_PLAYER_ALLY_VITAL:
		pSentenceName = "COMBINE_KILL_CITIZENS";
		break;

	case CLASS_ANTLION:
	case CLASS_HOUNDEYE:
//	case CLASS_EARTH_FAUNA:
		pSentenceName = "COMBINE_KILL_BUGS";
		break;

	case CLASS_ZOMBIE:
		pSentenceName = "COMBINE_KILL_ZOMBIES";
		break;
		
	case CLASS_HEADCRAB:
	case CLASS_BARNACLE:
		pSentenceName = "COMBINE_KILL_PARASITES";
		break;
	*/
	}

	m_Sentences.Speak( pSentenceName, SENTENCE_PRIORITY_MEDIUM );
}

//=========================================================
// Speak Sentence - say your cued up sentence.
//
// Some grunt sentences (take cover and charge) rely on actually
// being able to execute the intended action. It's really lame
// when a grunt says 'COVER ME' and then doesn't move. The problem
// is that the sentences were played when the decision to TRY
// to move to cover was made. Now the sentence is played after 
// we know for sure that there is a valid path. The schedule
// may still fail but in most cases, well after the grunt has 
// started moving.
//=========================================================
char *CNPC_Combine::GetSentencePrefix( const char *pszSoundName )
{
	char *Prefix = "COMBINE";

	if( IsElite() )
		Prefix = "COMBINE_ELITE";

#if 0
	if( IsDemolition() )
		Prefix = "COMBINE_LEADER";
#endif

	CFmtStr snd;
	snd.sprintf("%s_%s", Prefix, pszSoundName );

	return snd.Access();
}

void CNPC_Combine::SpeakSentence( int sentenceType )
{
	if (!FOkToMakeSound())
		return;

	switch( sentenceType )
	{
	case 0: // Spotted enemy
		AnnounceEnemyType( GetEnemy() );
		break;

	case 1: // assault/press attack
		AnnounceAssault();
		break;

	case 2: // Flanking the player
		// If I'm moving more than 20ft, I need to talk about it
		if ( GetNavigator()->GetPath()->GetPathLength() > 20 * 12.0f )
		{
			const char *pSentenceName = "COMBINE_MOVE";
			if ( m_pSquad )
			{
				if ( m_pSquad->IsLeader( this ) )
				{
					pSentenceName = "COMBINE_FLANK_LEADER";
				}
				else
				{
					pSentenceName = "COMBINE_FLANK";
				}
			}
			if ( IsElite() )
			{
				pSentenceName = "COMBINE_ELITE_FLANK";
			}
			m_Sentences.Speak( pSentenceName );
		}
		break;

	case 3: // going to hide for reload
		m_Sentences.Speak( "COMBINE_RELOAD", SENTENCE_PRIORITY_MEDIUM, SENTENCE_CRITERIA_IN_SQUAD );
		break;

	case 4: // Investigate
		m_Sentences.Speak( "COMBINE_INVESTIGATE", SENTENCE_PRIORITY_MEDIUM, SENTENCE_CRITERIA_IN_SQUAD );
		break;

	case 5: // Defuse
		m_Sentences.Speak( "COMBINE_CHARGE_DETECT", SENTENCE_PRIORITY_MEDIUM, SENTENCE_CRITERIA_IN_SQUAD );
		break;

	case 6: // Defuse Completed
		m_Sentences.Speak( "COMBINE_CHARGE_CLEAR", SENTENCE_PRIORITY_MEDIUM, SENTENCE_CRITERIA_IN_SQUAD );
		break;
	}
}

//=================================================================================================
// PainSound
//=================================================================================================
void CNPC_Combine::PainSound( const CTakeDamageInfo &info )
{
	if ( gpGlobals->curtime < m_flNextPainSoundTime )
		return;

	float healthRatio = (float)GetHealth() / (float)GetMaxHealth();
	if ( healthRatio > 0.0f )
	{
		const char *pSentenceName = "COMBINE_PAIN";
		if ( m_pSquad )
		{
			if ( !HasMemory(bits_MEMORY_PAIN_LIGHT_SOUND) && healthRatio > 0.8f )
			{
				// Shouts like "Target ineffective" and "Hes on me"
				Remember( bits_MEMORY_PAIN_LIGHT_SOUND );
				pSentenceName = "COMBINE_TAUNT";
			}
			else if ( !HasMemory(bits_MEMORY_PAIN_HEAVY_SOUND) && healthRatio < 0.3f )
			{
				// Shouts like "Medic" and "Falling back"
				Remember( bits_MEMORY_PAIN_HEAVY_SOUND );
				pSentenceName = "COMBINE_COVER";
			}
		}

		// This causes it to speak it no matter what; doesn't bother with setting sounds.
		m_Sentences.Speak( pSentenceName, SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS );
		m_flNextPainSoundTime = gpGlobals->curtime + 1;
	}
}

//-----------------------------------------------------------------------------
// Purpose: implemented by subclasses to give them an opportunity to make
//			a sound when they lose their enemy
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_Combine::LostEnemySound( void )
{
	if ( gpGlobals->curtime <= m_flNextLostSoundTime )
		return;

	const char *pSentence;
	if (!(CBaseEntity*)GetEnemy() || gpGlobals->curtime - GetEnemyLastTimeSeen() >= 15)
	{
		pSentence = "COMBINE_LOST_LONG";
	}
	else
	{
		pSentence = "COMBINE_LOST_SHORT";
	}

	if( IsElite() )
		pSentence = "COMBINE_ELITE_LOST";
	
	if ( m_Sentences.Speak( pSentence ) >= 0 )
	{
		m_flNextLostSoundTime = gpGlobals->curtime + random->RandomFloat(5.0,15.0);
	}
}

//-----------------------------------------------------------------------------
// Purpose: implemented by subclasses to give them an opportunity to make
//			a sound when they lose their enemy
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_Combine::FoundEnemySound( void )
{
	const char *pSentenceName = "COMBINE_REFIND_ENEMY";
	if( IsElite() )
		pSentenceName = "COMBINE_ELITE_REFIND_ENEMY";

	m_Sentences.Speak( pSentenceName, SENTENCE_PRIORITY_HIGH );
}

void CNPC_Combine::AlertSound( void )
{
	if ( gpGlobals->curtime > m_flNextAlertSoundTime )
	{
		const char *pSentenceName = "COMBINE_GO_ALERT";
		if( IsElite() )
			pSentenceName = "COMBINE_ELITE_GO_ALERT";

		m_Sentences.Speak( pSentenceName, SENTENCE_PRIORITY_HIGH );
		m_flNextAlertSoundTime = gpGlobals->curtime + 10.0f;
	}
}

//=========================================================
// NotifyDeadFriend
//=========================================================
void CNPC_Combine::NotifyDeadFriend( CBaseEntity* pFriend )
{
	BaseClass::NotifyDeadFriend(pFriend);

	if ( GetSquad()->NumMembers() < 2 )
	{
		const char *pSentenceName = "COMBINE_LAST_OF_SQUAD";
		if( IsElite() )
			pSentenceName = "COMBINE_ELITE_LAST_OF_SQUAD";

		m_Sentences.Speak( pSentenceName, SENTENCE_PRIORITY_HIGH, SENTENCE_CRITERIA_NORMAL );
		JustMadeSound();
		return;
	}

	if ( IsElite() )
		return;

	// relaxed visibility test so that guys say this more often, as it gets interrupted alot
	if( FInViewCone( pFriend ) )
	{
		m_Sentences.Speak( "COMBINE_MAN_DOWN", SENTENCE_PRIORITY_MEDIUM );
	}
}

//=========================================================
// DeathSound 
//=========================================================
void CNPC_Combine::DeathSound( const CTakeDamageInfo &info )
{
	// NOTE: The response system deals with this at the moment
	if ( GetFlags() & FL_DISSOLVING )
		return;

	const char *pSentenceName = "COMBINE_DIE";
	// If im gibbing, dont make a long, drawn-out, bone-chilling, almost shakespearean death rattle
	if ( ShouldGib( info ) )
	{
		pSentenceName = "COMBINE_GIB";
	}

	m_Sentences.Speak( pSentenceName, SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS );
}

//=========================================================
// IdleSound 
//=========================================================
void CNPC_Combine::IdleSound( void )
{
	if ( IsElite() )
	{
		if (random->RandomInt(0,1))
		{
			m_Sentences.Speak( "COMBINE_ELITE_IDLE" );
		}
		return;
	}

	if (g_fCombineQuestion || random->RandomInt(0,1))
	{
		if (!g_fCombineQuestion)
		{
			// ask question or make statement
			switch (random->RandomInt(0,2))
			{
			case 0: // check in
				if ( m_Sentences.Speak( "COMBINE_CHECK" ) >= 0 )
				{
					g_fCombineQuestion = 1;
				}
				break;

			case 1: // question
				if ( m_Sentences.Speak( "COMBINE_QUEST" ) >= 0 )
				{
					g_fCombineQuestion = 2;
				}
				break;

			case 2: // statement
				m_Sentences.Speak( "COMBINE_IDLE" );
				break;
			}
		}
		else
		{
			switch (g_fCombineQuestion)
			{
			case 1: // check in
				if ( m_Sentences.Speak( "COMBINE_CLEAR" ) >= 0 )
				{
					g_fCombineQuestion = 0;
				}
				break;
			case 2: // question 
				if ( m_Sentences.Speak( "COMBINE_ANSWER" ) >= 0 )
				{
					g_fCombineQuestion = 0;
				}
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: If the soldier can throw out a signal
//-----------------------------------------------------------------------------
bool CNPC_Combine::CanDoSignal()
{
	float flDistToEnemy = ( GetEnemy()->GetAbsOrigin() - GetAbsOrigin() ).Length();
	if ( flDistToEnemy < COMBINE_MIN_SIGNAL_DIST )
	{
		return false;
	}

	if ( gpGlobals->curtime <= m_flNextSignalTime )
		return false;

#if 0
	if ( random->RandomInt( 0, 1 ) )
		return false;
#endif

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//			This is for Grenade attacks.  As the test for grenade attacks
//			is expensive we don't want to do it every frame.  Return true
//			if we meet minimum set of requirements and then test for actual
//			throw later if we actually decide to do a grenade attack.
// Input  :
// Output :
//-----------------------------------------------------------------------------
ConVar npc_combine_altfire_supercede( "npc_combine_altfire_supercede", "1" );	// Can thrown grenades be paired with launched grenades?

int CNPC_Combine::RangeAttack2Conditions( float flDot, float flDist )
{
	if ( npc_combine_altfire_supercede.GetBool() )
	{
		if( IsDemolition() && FClassnameIs( GetActiveWeapon(), "weapon_smg*" ) || Weapon_OwnsThisType( "weapon_grenadelauncher" ) )
		{
			// Only use launched grenades
			return 0;
		}
	}

	// I'm not allowed to throw grenades during dustoff
	if ( IsCurSchedule( SCHED_DROPSHIP_DUSTOFF ) )
		return 0;

	// --------------------------------------
	//  Get target vector
	// --------------------------------------
	CBaseEntity *pEnemy = GetEnemy();
	if (!pEnemy)
		return 0;

	Vector vecTarget;

	// Toss it to where you last saw them
	vecTarget = GetEnemyLKP();	//GetEnemies()->LastSeenPosition( pEnemy )
	// Estimate position
#if 0
	if ( CBaseEntity::FVisible( vecTarget ) )
	{
		vecTarget = vecTarget + pEnemy->GetAbsVelocity();
	}
#endif

	m_lastGrenadeCondition = GetGrenadeConditions( flDot, flDist, vecTarget );
	return m_lastGrenadeCondition;
}


//-----------------------------------------------------------------------------
// Purpose: Return true if the grunt has grenades, hasn't checked lately, and
//			can throw a grenade at the target point.
// Input  : flDot - 
//			flDist - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CNPC_Combine::GetGrenadeConditions( float flDot, float flDist, const Vector &vecTarget )
{
	// Clear this condition first. The NPC will earn it again if the 
	// throw is valid.
	ClearCondition( COND_CAN_RANGE_ATTACK2 );

	if ( m_iNumGrenades < 1 )	// Out of grenades!
		return COND_NONE;

	// -----------------------------------------------
	// Not allowed to throw another grenade right now
	// -----------------------------------------------
	if ( gpGlobals->curtime < m_flNextGrenadeCheck )
		return m_lastGrenadeCondition;	//m_lastGrenadeCondition

	// -----------------------
	// If moving, don't check
	// -----------------------
	if ( m_flGroundSpeed != 0 )	//m_flGroundSpeed != 0
		return COND_NONE;

#if 0
	// -----------------------
	// Pass the random chance
	// -----------------------
	if ( random->RandomFloat( 0, 100 ) > COMBINE_GRENADE_CHANCE )
	{
		m_flNextGrenadeCheck = gpGlobals->curtime + 0.5; // half a second.
		return COND_NONE;
	}
#endif

	// --------------------------------------------------------------------------------
	// Use a rough distance check - more detailed check is done further down the line
	// --------------------------------------------------------------------------------
	if( flDist > 960 || flDist < 128 )	//960 = 60ft - real grenades can be thrown about 90ft (1440)
	{
		// Too close or too far!
		m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.
		return COND_NONE;	//COND_TOO_FAR_TO_ATTACK
	}

	// ------------------------
	// Enemy is not underwater
	// ------------------------
	if ( !( GetEnemy()->GetFlags() & FL_ONGROUND ) && GetEnemy()->GetWaterLevel() == 0 && GetEnemyLKP().z > (GetAbsOrigin().z + WorldAlignMaxs().z)  )
	{
		//!!!BUGBUG - we should make this check movetype and make sure it isn't FLY? Players who jump a lot are unlikely to be grenaded.
		// don't throw grenades at anything that isn't on the ground or otherwise swimming!
		m_flNextGrenadeCheck = gpGlobals->curtime + 0.5; // half a second.
		return COND_NONE;
	}

	// --------------------------------------------------------------------
	// Are any of my squad members near the intended grenade impact area?
	// --------------------------------------------------------------------
	if ( m_pSquad )
	{
		if ( m_pSquad->SquadMemberInRange( vecTarget, COMBINE_MIN_GRENADE_CLEAR_DIST ) )
		{
			// Crap, I might blow my friend up. Don't throw a grenade and don't check again for a while
			m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.
			// Tell my squad members to clear out so I can get a grenade in
		//!	CSoundEnt::InsertSound( SOUND_MOVE_AWAY | SOUND_CONTEXT_COMBINE_ONLY, vecTarget, COMBINE_MIN_GRENADE_CLEAR_DIST, 0.1 );
			return COND_WEAPON_BLOCKED_BY_FRIEND;
		}
	}

	return CheckCanThrowGrenade( vecTarget );
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the grunt can throw a grenade at the specified target point
// Input  : &vecTarget - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CNPC_Combine::CheckCanThrowGrenade( const Vector &vecTarget )
{
	//NDebugOverlay::Line( EyePosition(), vecTarget, 0, 255, 0, false, 5 );

	// ---------------------------------------------------------------------
	// Check that throw is legal and clear
	// ---------------------------------------------------------------------
	// FIXME: this is only valid for hand grenades, not RPG's
	Vector vecToss;
	Vector vecMins = -Vector(2,2,2);
	Vector vecMaxs = Vector(2,2,4);
	if( FInViewCone( vecTarget ) && CBaseEntity::FVisible( vecTarget ) )
	{
		vecToss = VecCheckThrow( this, EyePosition(), vecTarget, COMBINE_GRENADE_THROW_SPEED, 1.0, &vecMins, &vecMaxs );
	}
	else
	{
		// Do I have enough headroom?
		trace_t tr;
		AI_TraceLine( EyePosition(), EyePosition() + Vector(0,0,64), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
		if( tr.fraction != 1.0 )
		{
			return COND_NONE;
		}

		vecToss = VecCheckToss( this, EyePosition(), vecTarget, -1, 1.0, true, &vecMins, &vecMaxs );
	}

	if ( vecToss != vec3_origin )
	{
		m_vecTossVelocity = vecToss;

		m_flNextGrenadeCheck = gpGlobals->curtime + 0.3; // 1/3 second.
		return COND_CAN_RANGE_ATTACK2;
	}
	else
	{
		// don't check again for a while.
		m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.
		return COND_NONE;	//COND_WEAPON_SIGHT_OCCLUDED
	}
}

//-----------------------------------------------------------------------------
// Combine alt-fire conditions (Vector check in WeaponRangeAttack2Condition)
//-----------------------------------------------------------------------------
ConVar npc_combine_altfire( "npc_combine_altfire", "1" );	//Temp convar until animations are working

int CNPC_Combine::CanAltFireEnemy( float flDot, float flDist )
{
	if ( !npc_combine_altfire.GetBool() )
		return 0;

	if ( !IsDemolition() )
		return 0;

	// I'm not allowed to throw grenades during dustoff
	if ( IsCurSchedule( SCHED_DROPSHIP_DUSTOFF ) )
		return 0;

	if ( m_iNumGrenades < 1 )
		return 0;

	if( gpGlobals->curtime < m_flNextAltFireTime )
		return 0;

	if ( gpGlobals->curtime < m_flNextGrenadeCheck )
		return 0;

	// If you're crouching you're staying put, dont bother with a grenade
	if ( IsCrouching() )
		return 0;

	CBaseEntity *pEnemy = GetEnemy();
	if (!pEnemy)
		return 0;

#if 0
	if ( FClassnameIs( GetActiveWeapon(), "weapon_ar2" ) )
	{
		// Check a direct ball projectory
		Vector vecTarget;
		vecTarget = GetEnemies()->LastSeenPosition( pEnemy ) + (pEnemy->GetViewOffset()*0.75);// approximates the chest

		// Trace a hull about the size of the combine ball (don't shoot through grates!)
		trace_t tr;

		Vector mins( -12, -12, -12 );
		Vector maxs( 12, 12, 12 );

		Vector vShootPosition = EyePosition();

		if ( GetActiveWeapon() )
		{
			GetActiveWeapon()->GetAttachment( "muzzle", vShootPosition );
		}

		// Trace a hull about the size of the combine ball.
		UTIL_TraceHull( vShootPosition, vecTarget, mins, maxs, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

		float flLength = (vShootPosition - vecTarget).Length();

		flLength *= tr.fraction;

		//If the ball can travel at least 65% of the distance to the player then let the NPC shoot it.
		if( tr.fraction >= 0.65 && flLength > 128.0f )
		{
			// Target is valid
			m_vecAltFireTarget = vecTarget;
			return true;
		}

		// Check again later
		m_vecAltFireTarget = vec3_origin;
		m_flNextGrenadeCheck = gpGlobals->curtime + 1.0f;
		return false;
	}
#endif

	// Default to normal grenades
	return GetActiveWeapon()->WeaponRangeAttack2Condition(flDot, flDist);
}

//-----------------------------------------------------------------------------
// Purpose: For combine melee attack (kick/hit)
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Combine::MeleeAttack1Conditions( float flDot, float flDist )
{
	if (flDist > 64)
	{
		return COND_NONE; // COND_TOO_FAR_TO_ATTACK;
	}
	else if (flDot < 0.7)
	{
		return COND_NONE; // COND_NOT_FACING_ATTACK;
	}

	// Check Z
	if ( GetEnemy() && fabs(GetEnemy()->GetAbsOrigin().z - GetAbsOrigin().z) > 64 )
		return COND_NONE;

	// Make sure not trying to kick through a window or something. 
	trace_t tr;
	Vector vecSrc, vecEnd;

	vecSrc = WorldSpaceCenter();
	vecEnd = GetEnemy()->WorldSpaceCenter();

	AI_TraceLine(vecSrc, vecEnd, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
	if( tr.m_pEnt != GetEnemy() )
	{
		return COND_NONE;
	}

	return COND_CAN_MELEE_ATTACK1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Vector
//-----------------------------------------------------------------------------
Vector CNPC_Combine::EyePosition( void ) 
{
	if ( !IsCrouching() )
	{
		return GetAbsOrigin() + COMBINE_EYE_STANDING_POSITION;
	}
	else
	{
		return GetAbsOrigin() + COMBINE_EYE_CROUCHING_POSITION;
	}

	/*
	Vector m_EyePos;
	GetAttachment( "eyes", m_EyePos );
	return m_EyePos;
	*/
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
Vector CNPC_Combine::GetAltFireTarget()
{
	Assert( IsDemolition() );

	return m_vecAltFireTarget;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nActivity - 
// Output : Vector
//-----------------------------------------------------------------------------
Vector CNPC_Combine::EyeOffset( Activity nActivity )
{
	if (CapabilitiesGet() & bits_CAP_DUCK)
	{
		if ( IsCrouchedActivity( nActivity ) )
			return COMBINE_EYE_CROUCHING_POSITION;

	}
	// if the hint doesn't tell anything, assume current state
	if ( !IsCrouching() )
	{
		return COMBINE_EYE_STANDING_POSITION;
	}
	else
	{
		return COMBINE_EYE_CROUCHING_POSITION;
	}
}

Vector CNPC_Combine::GetCrouchEyeOffset( void )
{
	return COMBINE_EYE_CROUCHING_POSITION;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::SetActivity( Activity NewActivity )
{
	BaseClass::SetActivity( NewActivity );

	m_iLastAnimEventHandled = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
NPC_STATE CNPC_Combine::SelectIdealState( void )
{
	switch ( m_NPCState )
	{
	case NPC_STATE_COMBAT:
		{
			if ( HasCondition( COND_LOST_ENEMY ) )	//GetEnemy() == NULL && !HasCondition( COND_ENEMY_DEAD )
			{
				// Lost track of my enemy. Patrol.
				SetCondition( COND_COMBINE_SHOULD_PATROL );
				return NPC_STATE_ALERT;
			}
			else if ( HasCondition( COND_ENEMY_DEAD ) )
			{
				ClearCondition( COND_COMBINE_SHOULD_PATROL );
				AnnounceEnemyKill(GetEnemy());
			}
		}

	default:
		{
			return BaseClass::SelectIdealState();
		}
	}

	return GetIdealState();
}

//-----------------------------------------------------------------------------
void CNPC_Combine::OnStateChange( NPC_STATE OldState, NPC_STATE NewState )
{
	if( OldState == NPC_STATE_COMBAT )
	{
		// Things have settled down, turn the new enemy logic back on
		m_iNewEnemies = 0;
	}

	BaseClass::OnStateChange( OldState, NewState );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Combine::OnBeginMoveAndShoot()
{
	if ( BaseClass::OnBeginMoveAndShoot() )
	{
		if( HasStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
			return true; // already have the slot I need

		if( !HasStrategySlotRange( SQUAD_SLOT_GRENADE1, SQUAD_SLOT_ATTACK_OCCLUDER ) && OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
			return true;
	}
	return false;
}

void CNPC_Combine::OnEndMoveAndShoot()
{
	VacateStrategySlot();
}

//-----------------------------------------------------------------------------
// Purpose: Get weapon accuracy
//-----------------------------------------------------------------------------
WeaponProficiency_t CNPC_Combine::CalcWeaponProficiency( CBaseCombatWeapon *pWeapon )
{
	if( FClassnameIs( pWeapon, "weapon_ar2" ) )
	{
		return WEAPON_PROFICIENCY_GOOD;
	}
	else if( FClassnameIs( pWeapon, "weapon_supershotgun" )	)
	{
		if( m_nSkin != COMBINE_SKIN_SHOTGUNNER )
		{
			m_nSkin = COMBINE_SKIN_SHOTGUNNER;
		}

		return WEAPON_PROFICIENCY_AVERAGE;
	}
	else if( FClassnameIs( pWeapon, "weapon_smg1" ) || FClassnameIs( pWeapon, "weapon_smg2" ) )
	{
		if( IsElite() )
		{
			return WEAPON_PROFICIENCY_VERY_GOOD;
		}
		else
		{
			return WEAPON_PROFICIENCY_GOOD;
		}
	}
	else if( FClassnameIs( pWeapon, "weapon_pistol" ) )
	{
		return WEAPON_PROFICIENCY_VERY_GOOD;
	}

	return BaseClass::CalcWeaponProficiency( pWeapon );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Combine::HasShotgun()
{
	if( GetActiveWeapon() && GetActiveWeapon()->m_iClassname == s_iszShotgunClassname )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Only supports weapons that use clips.
//-----------------------------------------------------------------------------
bool CNPC_Combine::ActiveWeaponIsFullyLoaded()
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();

	if( !pWeapon )
		return false;

	if( !pWeapon->UsesClipsForAmmo1() )
		return false;

	return ( pWeapon->Clip1() >= pWeapon->GetMaxClip1() );
}


//-----------------------------------------------------------------------------
// Purpose:  This is a generic function (to be implemented by sub-classes) to
//			 handle specific interactions between different types of characters
//			 (For example the barnacle grabbing an NPC)
// Input  :  The type of interaction, extra info pointer, and who started it
// Output :	 true  - if sub-class has a response for the interaction
//			 false - if sub-class has no response
//-----------------------------------------------------------------------------
bool CNPC_Combine::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter *sourceEnt)
{
	if ( interactionType == g_interactionTurretStillStanding )
	{
		// A turret that I've kicked recently is still standing 5 seconds later. 
		if ( sourceEnt == GetEnemy() )
		{
			// It's still my enemy. Time to grenade it.
			SetCondition( COND_COMBINE_DROP_GRENADE );
			ClearSchedule( "Failed to kick over turret" );
		}
		return true;
	}

	return BaseClass::HandleInteraction( interactionType, data, sourceEnt );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char* CNPC_Combine::GetSquadSlotDebugName( int iSquadSlot )
{
	switch( iSquadSlot )
	{
	case SQUAD_SLOT_GRENADE1:			return "SQUAD_SLOT_GRENADE1";	
		break;
	case SQUAD_SLOT_GRENADE2:			return "SQUAD_SLOT_GRENADE2";	
		break;
	case SQUAD_SLOT_ATTACK_OCCLUDER:	return "SQUAD_SLOT_ATTACK_OCCLUDER";	
		break;
	}

	return BaseClass::GetSquadSlotDebugName( iSquadSlot );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Combine::IsUsingTacticalVariant( int variant )
{
	if( variant == TACTICAL_VARIANT_PRESSURE_ENEMY && m_iTacticalVariant == TACTICAL_VARIANT_PRESSURE_ENEMY_UNTIL_CLOSE )
	{
		// Essentially, fib. Just say that we are a 'pressure enemy' soldier.
		return true;
	}

	return m_iTacticalVariant == variant;
}

//-----------------------------------------------------------------------------
// For the purpose of determining whether to use a pathfinding variant, this
// function determines whether the current schedule is a schedule that 
// 'approaches' the enemy. 
//-----------------------------------------------------------------------------
bool CNPC_Combine::IsRunningApproachEnemySchedule()
{
	if( IsCurSchedule( SCHED_CHASE_ENEMY ) )
		return true;

	if( IsCurSchedule( SCHED_ESTABLISH_LINE_OF_FIRE ) )
		return true;

	if( IsCurSchedule( SCHED_COMBINE_ASSAULT ) )
		return true;

	if( IsCurSchedule( SCHED_COMBINE_PRESS_ATTACK ) )
		return true;

	return false;
}

#if 0
bool CNPC_Combine::ShouldPickADeathPose( void ) 
{
	return !IsCrouching(); 
}
#endif

//-----------------------------------------------------------------------------
//
// Schedules
//
// SCHED_COMBINE_EVADE; Simple side-step/jump to the left/right
// SCHED_COMBINE_ASSAULT; Establish line of fire but shoot while you're doing it
// SCHED_COMBINE_PRESS_ATTACK; Get the last seen postion of the enemy and run
// to it. Most of the time results in a mindless charge. Different from ELOF
// in that its not cancelled when the target is found again. If you want an
// even more aggressive charge (completely uninterruptable), use 
// SCHED_COMBINE_CHARGE_PLAYER.
//-----------------------------------------------------------------------------
 
AI_BEGIN_CUSTOM_NPC( npc_combine, CNPC_Combine )

//Tasks
DECLARE_TASK( TASK_COMBINE_FACE_TOSS_DIR )
DECLARE_TASK( TASK_COMBINE_IGNORE_ATTACKS )
DECLARE_TASK( TASK_COMBINE_SIGNAL_BEST_SOUND )
DECLARE_TASK( TASK_COMBINE_DEFER_SQUAD_GRENADES )
DECLARE_TASK( TASK_COMBINE_CHASE_ENEMY_CONTINUOUSLY )
DECLARE_TASK( TASK_COMBINE_DIE_INSTANTLY )
DECLARE_TASK( TASK_COMBINE_PLAY_SEQUENCE_FACE_ALTFIRE_TARGET )
DECLARE_TASK( TASK_COMBINE_GET_PATH_TO_FORCED_GREN_LOS )
DECLARE_TASK( TASK_COMBINE_SET_STANDING )
//DECLARE_TASK( TASK_COMBINE_MOVE_AND_SHOOT )
//DECLARE_TASK( TASK_COMBINE_MOVE_AND_AIM )

//Activities
DECLARE_ACTIVITY( ACT_COMBINE_THROW_GRENADE )
DECLARE_ACTIVITY( ACT_COMBINE_LAUNCH_GRENADE )
DECLARE_ACTIVITY( ACT_COMBINE_BUGBAIT )
DECLARE_ACTIVITY( ACT_COMBINE_AR2_ALTFIRE )
DECLARE_ACTIVITY( ACT_WALK_EASY )
DECLARE_ACTIVITY( ACT_WALK_MARCH )

DECLARE_ANIMEVENT( COMBINE_AE_BEGIN_ALTFIRE )
DECLARE_ANIMEVENT( COMBINE_AE_ALTFIRE )

DECLARE_SQUADSLOT( SQUAD_SLOT_GRENADE1 )
DECLARE_SQUADSLOT( SQUAD_SLOT_GRENADE2 )

DECLARE_CONDITION( COND_COMBINE_NO_FIRE )
DECLARE_CONDITION( COND_COMBINE_SHOULD_PATROL )
DECLARE_CONDITION( COND_COMBINE_HIT_BY_BUGBAIT )
DECLARE_CONDITION( COND_COMBINE_DROP_GRENADE )
DECLARE_CONDITION( COND_COMBINE_ATTACK_SLOT_AVAILABLE )

DECLARE_INTERACTION( g_interactionCombineBash );

//=========================================================
// SCHED_COMBINE_TAKE_COVER_FROM_BEST_SOUND
//
//	hide from the loudest sound source (to run from grenade)
//=========================================================
DEFINE_SCHEDULE	
(
 SCHED_COMBINE_TAKE_COVER_FROM_BEST_SOUND,

 "	Tasks"
 "		 TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_COMBINE_RUN_AWAY_FROM_BEST_SOUND"
 "		 TASK_STOP_MOVING					0"
 "		 TASK_COMBINE_SIGNAL_BEST_SOUND		0"
 "		 TASK_SET_TOLERANCE_DISTANCE		24"
 "		 TASK_FIND_COVER_FROM_BEST_SOUND	0"
//"		 TASK_RUN_PATH						0"
 "		 TASK_RUN_PATH_TIMED				2.5"
 "		 TASK_WAIT_FOR_MOVEMENT				0"
 "		 TASK_REMEMBER						MEMORY:INCOVER"
 "		 TASK_FACE_REASONABLE				0"
 ""
 "	Interrupts"
 )

 DEFINE_SCHEDULE	
 (
 SCHED_COMBINE_RUN_AWAY_FROM_BEST_SOUND,

 "	Tasks"
 "		 TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_COWER"
 "		 TASK_STORE_BESTSOUND_REACTORIGIN_IN_SAVEPOSITION	0"
 "		 TASK_GET_PATH_AWAY_FROM_BEST_SOUND	500"
 "		 TASK_RUN_PATH_TIMED				3"
 "		 TASK_STOP_MOVING					0"
 "		 TASK_FACE_SAVEPOSITION				0"
 ""
 "	Interrupts"
 )

 //=========================================================
 //	SCHED_COMBINE_COMBAT_FAIL
 //=========================================================
 DEFINE_SCHEDULE	
 (
 SCHED_COMBINE_COMBAT_FAIL,

 "	Tasks"
 "		TASK_STOP_MOVING			0"
 "		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
 "		TASK_WAIT_FACE_ENEMY		2"
 "		TASK_WAIT_PVS				0"
 ""
 "	Interrupts"
 "		COND_CAN_RANGE_ATTACK1"
 "		COND_CAN_RANGE_ATTACK2"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK2"
 )

 //=========================================================
 // SCHED_COMBINE_VICTORY_DANCE
 //
 //	Victory dance!
 //=========================================================
 DEFINE_SCHEDULE	
 (
 SCHED_COMBINE_VICTORY_DANCE,

 "	Tasks "
 "		TASK_STOP_MOVING					0"
 "		TASK_FACE_ENEMY						0"
 "		TASK_WAIT							1.5"
 "		TASK_GET_PATH_TO_ENEMY_CORPSE		0"
 "		TASK_WALK_PATH						0"
 "		TASK_WAIT_FOR_MOVEMENT				0"
 "		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_VICTORY_DANCE"
 ""
 "	Interrupts "
 "		COND_NEW_ENEMY"
 "		COND_LIGHT_DAMAGE"
 "		COND_HEAVY_DAMAGE"
 )

 //=========================================================
 // SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE
 //
 //	Get into the closest position where you can shoot the enemy
 //=========================================================
 DEFINE_SCHEDULE 
 (
 SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE,

 "	Tasks "
 "		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_FAIL_ESTABLISH_LINE_OF_FIRE"
//"		TASK_STOP_MOVING				0"
 "		TASK_WAIT_FACE_ENEMY			0.5" // wait just a little bit, in-case its just the player being spastic
 "		TASK_COMBINE_SET_STANDING		1"
 "		TASK_GET_PATH_TO_ENEMY_LOS		0"
 "		TASK_RUN_PATH					0"
 "		TASK_SPEAK_SENTENCE				2"
 "		TASK_WAIT_FOR_MOVEMENT			0"
 "		TASK_COMBINE_IGNORE_ATTACKS		0.0"
 "		TASK_SET_SCHEDULE				SCHEDULE:SCHED_COMBAT_FACE"
 ""
 "	Interrupts "
 "		COND_NEW_ENEMY"
 "		COND_ENEMY_DEAD"
 "		COND_HEAVY_DAMAGE"
 "		COND_NO_PRIMARY_AMMO"
 "		COND_LOW_PRIMARY_AMMO"
 "		COND_CAN_RANGE_ATTACK1"
 "		COND_CAN_RANGE_ATTACK2"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK2"
 "		COND_HEAR_DANGER"
 "		COND_HEAR_MOVE_AWAY"
 )

 //=========================================================
 // SCHED_COMBINE_ASSAULT
 //
 // Maintain distance around the target while shooting
 //=========================================================
 DEFINE_SCHEDULE 
 (
 SCHED_COMBINE_ASSAULT,

 "	Tasks "
 "		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE"
 "		TASK_WAIT_FACE_ENEMY			0.2"
 "		TASK_SET_TOLERANCE_DISTANCE		48"
 "		TASK_COMBINE_SET_STANDING		1"
 "		TASK_GET_PATH_TO_ENEMY_LKP_LOS	0"
 "		TASK_RUN_PATH					0"
 "		TASK_SPEAK_SENTENCE				1"
 "		TASK_WAIT_FOR_MOVEMENT			0"
 "		TASK_COMBINE_IGNORE_ATTACKS		0.0"
 "		TASK_SET_SCHEDULE				SCHEDULE:SCHED_COMBAT_FACE"
 ""
 "	Interrupts "
 "		COND_NEW_ENEMY"
 "		COND_ENEMY_UNREACHABLE"
 "		COND_HEAVY_DAMAGE"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK2"
 "		COND_TOO_FAR_TO_ATTACK"
 "		COND_HEAR_DANGER"
//"		COND_HEAR_MOVE_AWAY"
 )

/*
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_GRENADE_ASSAULT,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 "		TASK_COMBINE_FACE_TOSS_DIR			0"
 "		TASK_ANNOUNCE_ATTACK				2"	// 2 = grenade
 "		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_RANGE_ATTACK2"
 "		TASK_COMBINE_DEFER_SQUAD_GRENADES	0"
 "		TASK_SET_SCHEDULE					SCHEDULE:SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE"
 ""
 "	Interrupts"
 )
*/

 //=========================================================
 // SCHED_COMBINE_PRESS_ATTACK
 //
 //	Run to the last place you saw the enemy
 //=========================================================
 DEFINE_SCHEDULE 
 (
 SCHED_COMBINE_PRESS_ATTACK,

 "	Tasks "
 "		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE"
 "		TASK_WAIT_FACE_ENEMY			0.2"
 "		TASK_SET_TOLERANCE_DISTANCE		76"
 "		TASK_COMBINE_SET_STANDING		1"
 "		TASK_GET_PATH_TO_ENEMY_LKP		0"
 "		TASK_RUN_PATH					0"
 "		TASK_SPEAK_SENTENCE				1"
 "		TASK_WAIT_FOR_MOVEMENT			0"
 "		TASK_SET_SCHEDULE				SCHEDULE:SCHED_COMBAT_FACE"
 ""
 "	Interrupts "
 "		COND_NEW_ENEMY"
 "		COND_ENEMY_DEAD"
 "		COND_ENEMY_UNREACHABLE"
 "		COND_HEAVY_DAMAGE"
 "		COND_NO_PRIMARY_AMMO"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK2"
 "		COND_HEAR_DANGER"
//"		COND_HEAR_MOVE_AWAY"
 )

 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_SIGNAL_PRESS_ATTACK,

 "	Tasks"
 "	 	TASK_STOP_MOVING			0"
 "		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_SIGNAL_ADVANCE"
 "		TASK_SET_SCHEDULE			SCHEDULE:SCHED_COMBINE_PRESS_ATTACK"
 ""
 "	Interrupts"
 "		COND_HEAVY_DAMAGE"
 )

 //=========================================================
 // SCHED_COMBINE_COMBAT_FACE
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_COMBAT_FACE,

 "	Tasks"
 "		TASK_STOP_MOVING			0"
 "		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE_ANGRY"
 "		TASK_FACE_ENEMY				0"
 "		TASK_WAIT					0.2"
//"		 TASK_SET_SCHEDULE			SCHEDULE:SCHED_COMBINE_SWEEP"
 ""
 "	Interrupts"
 "		COND_NEW_ENEMY"
 "		COND_ENEMY_DEAD"
 "		COND_HEAVY_DAMAGE"
 "		COND_CAN_RANGE_ATTACK1"
 "		COND_CAN_RANGE_ATTACK2"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK2"
//"		COND_HEAR_DANGER"
 )

 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_ALERT_FACE,

 "	Tasks"
 "		TASK_STOP_MOVING			0"
 "		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
 "		TASK_FACE_IDEAL				0"
 "		TASK_WAIT					0.3"
//"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_ALERT_SCAN"
 ""
 "	Interrupts"
 "		COND_NEW_ENEMY"
 "		COND_SEE_FEAR"
 "		COND_HEAVY_DAMAGE"
 "		COND_PROVOKED"
 )

 //=========================================================
 // SCHED_HIDE_AND_RELOAD
 //	SCHED_COVER_AND_RELOAD
 //	Hide and reload - Find a place where the enemy can
 // %100 not see you, and reload in safety
 // Cover and reload - Find a place where the enemy
 // cant hit you, try to reload, but dont if the enemy
 // is pursuing
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_HIDE_AND_RELOAD,

 "	Tasks"
 "		TASK_STOP_MOVING			0"
 "		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_RELOAD"
//"		TASK_SPEAK_SENTENCE			3"
 "		TASK_FIND_NODE_COVER_FROM_ENEMY	0"
 "		TASK_RUN_PATH				0"
 "		TASK_WAIT_FOR_MOVEMENT		0"
 "		TASK_REMEMBER				MEMORY:INCOVER"
 "		TASK_SET_SCHEDULE			SCHEDULE:SCHED_RELOAD"
 ""
 "	Interrupts"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK2"
 "		COND_HEAR_DANGER"
 "		COND_HEAR_MOVE_AWAY"
 )

 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_COVER_AND_RELOAD,

 "	Tasks"
 "		TASK_STOP_MOVING			0"
 "		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_HIDE_AND_RELOAD"
 "		TASK_FIND_COVER_FROM_ENEMY	0"
 "		TASK_RUN_PATH				0"
 "		TASK_WAIT_FOR_MOVEMENT		0"
 "		TASK_WAIT					0.2"
 "		TASK_SET_SCHEDULE			SCHEDULE:SCHED_RELOAD"
 ""
 "	Interrupts"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK2"
 "		COND_NO_PRIMARY_AMMO"
 "		COND_HEAR_DANGER"
 "		COND_HEAR_MOVE_AWAY"
 "		COND_HEAVY_DAMAGE"
 "		COND_WEAPON_BLOCKED_BY_FRIEND"
 )

 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_GRENADE_AND_RELOAD,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 "		TASK_COMBINE_FACE_TOSS_DIR			0"
 "		TASK_ANNOUNCE_ATTACK				2"	// 2 = grenade
 "		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_SPECIAL_ATTACK1"	// Toss it
 "		TASK_COMBINE_DEFER_SQUAD_GRENADES	0"
 "		TASK_SET_SCHEDULE					SCHEDULE:SCHED_HIDE_AND_RELOAD"
 ""
 "	Interrupts"
 )

 //=========================================================
 // SCHED_COMBINE_SUPPRESS
 //	don't stop shooting until the clip is
 //	empty or grunt gets hurt. Different from range_attack1
 // in that there is much less possible interruption
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_SIGNAL_SUPPRESS,

 "	Tasks"
 "		TASK_STOP_MOVING				0"
 "		TASK_FACE_IDEAL					0"
 "		TASK_PLAY_SEQUENCE_FACE_ENEMY	ACTIVITY:ACT_SIGNAL_FORWARD"
 "		TASK_SET_SCHEDULE				SCHEDULE:SCHED_COMBINE_SUPPRESS"
 ""
 "	Interrupts"
 "		COND_HEAVY_DAMAGE"
 "		COND_HEAR_DANGER"
 "		COND_HEAR_MOVE_AWAY"
 "		COND_WEAPON_BLOCKED_BY_FRIEND"
 )

 DEFINE_SCHEDULE	
 (
 SCHED_COMBINE_SUPPRESS,

 "	Tasks"
 "		TASK_STOP_MOVING			0"
 "		TASK_FACE_ENEMY				0"
 "		TASK_WAIT					0.3"
 "		TASK_RANGE_ATTACK1			0"
 ""
 "	Interrupts"
 "		COND_ENEMY_DEAD"
 "		COND_LIGHT_DAMAGE"
 "		COND_HEAVY_DAMAGE"
 "		COND_NO_PRIMARY_AMMO"
 "		COND_WEAPON_BLOCKED_BY_FRIEND"
 "		COND_TOO_CLOSE_TO_ATTACK"
 "		COND_HEAR_DANGER"
 "		COND_HEAR_MOVE_AWAY"
 "		COND_COMBINE_NO_FIRE"
 )

 //=========================================================
 // GruntFoundEnemy - grunt established sight with an enemy
 // that was hiding from the squad.
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_FOUND_ENEMY,

 "	Tasks"
 "		TASK_STOP_MOVING				0"
 "		TASK_FACE_IDEAL					0"
 "		TASK_PLAY_SEQUENCE_FACE_ENEMY	ACTIVITY:ACT_SIGNAL_GROUP"
 ""
 "	Interrupts"
 "		COND_HEAVY_DAMAGE"
 "		COND_HEAR_DANGER"
 "		COND_HEAR_MOVE_AWAY"
 "		COND_WEAPON_BLOCKED_BY_FRIEND"
 "		COND_COMBINE_NO_FIRE"
 )

 //=========================================================
 // SCHED_COMBINE_WAIT_IN_COVER
 //	we don't allow danger or the ability
 //	to attack to break a grunt's run to cover schedule but
 //	when a grunt is in cover we do want them to attack if they can.
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_WAIT_IN_COVER,

 "	Tasks"
 "		TASK_STOP_MOVING				0"
 "		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"
 "		TASK_WAIT_FACE_ENEMY			1"
 ""
 "	Interrupts"
 "		COND_NEW_ENEMY"
 "		COND_HEAVY_DAMAGE"
 "		COND_CAN_RANGE_ATTACK1"
 "		COND_CAN_RANGE_ATTACK2"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK2"
 "		COND_HEAR_DANGER"
 "		COND_HEAR_MOVE_AWAY"
 "		COND_COMBINE_ATTACK_SLOT_AVAILABLE"
 )

 //=========================================================
 // SCHED_COMBINE_TAKE_COVER1-2
 //=========================================================
 DEFINE_SCHEDULE	
 (
 SCHED_COMBINE_TAKE_COVER1,

 "	Tasks"
 "	 	TASK_STOP_MOVING			0"
 "		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_COMBINE_TAKECOVER_FAILED"
 "		TASK_WAIT					0.2"
 "		TASK_FIND_COVER_FROM_ENEMY	0"
 "		TASK_RUN_PATH				0"
 "		TASK_WAIT_FOR_MOVEMENT		0"
 "		TASK_REMEMBER				MEMORY:INCOVER"
 "		TASK_SET_SCHEDULE			SCHEDULE:SCHED_COMBINE_WAIT_IN_COVER"
 ""
 "	Interrupts"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK2"
 )

 DEFINE_SCHEDULE	
 (
 SCHED_COMBINE_TAKE_COVER2,

 "	Tasks"
//"		TASK_STOP_MOVING			0"	//Disabled so soldiers can "blend in" to a walk
 "		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_COMBINE_TAKECOVER_FAILED"
 "		TASK_FIND_COVER_FROM_ENEMY	0"
 "		TASK_WALK_PATH				0"
 "		TASK_WAIT_FOR_MOVEMENT		0"
 "		TASK_REMEMBER				MEMORY:INCOVER"
 "		TASK_SET_SCHEDULE			SCHEDULE:SCHED_COMBINE_WAIT_IN_COVER"
 ""
 "	Interrupts"
 "		COND_HEAVY_DAMAGE"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK2"
 )

 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_TAKECOVER_FAILED,

 "	Tasks"
 "		TASK_STOP_MOVING			0"
 "		TASK_FACE_REASONABLE		0"
 ""
 "	Interrupts"
 )

 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_SIGNAL_TAKE_COVER,

 "	Tasks"
 "	 	TASK_STOP_MOVING			0"
 "		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_SIGNAL_TAKECOVER"
 "		TASK_SET_SCHEDULE			SCHEDULE:SCHED_COMBINE_TAKE_COVER1"
 ""
 "	Interrupts"
 "		COND_HEAVY_DAMAGE"
 )

 //=========================================================
 // SCHED_COMBINE_GRENADE_COVER1
 //
 // Toss grenade then run to cover.
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_GRENADE_COVER1,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 "		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_SPECIAL_ATTACK1"
 "		TASK_FIND_COVER_FROM_ENEMY			99"
 "		TASK_FIND_FAR_NODE_COVER_FROM_ENEMY	384"
 "		TASK_CLEAR_MOVE_WAIT				0"
 "		TASK_RUN_PATH						0"
 "		TASK_WAIT_FOR_MOVEMENT				0"
 "		TASK_SET_SCHEDULE					SCHEDULE:SCHED_COMBINE_WAIT_IN_COVER"
 ""
 "	Interrupts"
 )

 //=========================================================
 // SCHED_COMBINE_RANGE_ATTACK1
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_RANGE_ATTACK1,

 "	Tasks"
 "		TASK_STOP_MOVING				0"
 "		TASK_FACE_ENEMY					0"
 "		TASK_ANNOUNCE_ATTACK			1"	// 1 = primary attack
 "		TASK_WAIT_RANDOM				0.5"
 "		TASK_RANGE_ATTACK1				0"
 ""
 "	Interrupts"
 "		COND_NEW_ENEMY"
 "		COND_ENEMY_DEAD"
 "		COND_LIGHT_DAMAGE"
 "		COND_HEAVY_DAMAGE"
 "		COND_LOW_PRIMARY_AMMO"
 "		COND_NO_PRIMARY_AMMO"
 "		COND_ENEMY_OCCLUDED"
 "		COND_WEAPON_BLOCKED_BY_FRIEND"
 "		COND_TOO_CLOSE_TO_ATTACK"
 "		COND_GIVE_WAY"
 "		COND_HEAR_DANGER"
 "		COND_HEAR_MOVE_AWAY"
 "		COND_COMBINE_NO_FIRE"
 ""
 )

 DEFINE_SCHEDULE
 (
	SCHED_COMBINE_MELEE_ATTACK1,

	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_COMBINE_SET_STANDING		1"
	"		TASK_FACE_ENEMY			0"
	"		TASK_MELEE_ATTACK1		0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"		COND_HEAVY_DAMAGE"
	"		COND_ENEMY_OCCLUDED"
 )

 //=========================================================
 // Mapmaker forced grenade throw
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_FORCED_GRENADE_THROW,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 "		TASK_COMBINE_FACE_TOSS_DIR			0"
 "		TASK_ANNOUNCE_ATTACK				2"	// 2 = grenade
 "		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_RANGE_ATTACK2"
 "		TASK_COMBINE_DEFER_SQUAD_GRENADES	0"
 ""
 "	Interrupts"
 )

 //=========================================================
 // Move to LOS of the mapmaker's forced grenade throw target
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_MOVE_TO_FORCED_GREN_LOS,

 "	Tasks "
 "		TASK_SET_TOLERANCE_DISTANCE					48"
 "		TASK_COMBINE_GET_PATH_TO_FORCED_GREN_LOS	0"
 "		TASK_SPEAK_SENTENCE							2"
 "		TASK_RUN_PATH								0"
 "		TASK_WAIT_FOR_MOVEMENT						0"
 "	"
 "	Interrupts "
 "		COND_NEW_ENEMY"
 "		COND_ENEMY_DEAD"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK2"
 "		COND_HEAR_DANGER"
 "		COND_HEAR_MOVE_AWAY"
 "		COND_HEAVY_DAMAGE"
 )

 //=========================================================
 // Launcher attack
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_AR2_ALTFIRE,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 "		TASK_COMBINE_FACE_TOSS_DIR			0"
 "		TASK_ANNOUNCE_ATTACK				2"
 "		TASK_RANGE_ATTACK2					0"
//"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_RANGE_ATTACK2"
 "		TASK_SET_SCHEDULE					SCHEDULE:SCHED_COMBINE_WAIT_IN_COVER"	// don't run immediately after launching grenade.
 ""
 "	Interrupts"
 )

/*
// Old Altfire
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_AR2_ALTFIRE,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 "		TASK_ANNOUNCE_ATTACK				2"
 "		TASK_COMBINE_PLAY_SEQUENCE_FACE_ALTFIRE_TARGET		ACTIVITY:ACT_COMBINE_AR2_ALTFIRE"
 ""
 "	Interrupts"
 )
*/

 //=========================================================
 // 	SCHED_COMBINE_RANGE_ATTACK2	
 //
 //	secondary range attack. Overriden because base class stops attacking when the enemy is occluded.
 //	combines's grenade throw requires the enemy be occluded.
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_RANGE_ATTACK2,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 "		TASK_COMBINE_FACE_TOSS_DIR			0"
 "		TASK_ANNOUNCE_ATTACK				2"	// 2 = grenade
 "		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_RANGE_ATTACK2"	// Throw it
 "		TASK_COMBINE_DEFER_SQUAD_GRENADES	0"
 "		TASK_SET_SCHEDULE					SCHEDULE:SCHED_COMBINE_WAIT_IN_COVER"	// don't run immediately after throwing grenade.
 ""
 "	Interrupts"
 )

 //=========================================================
 // MISCS
 //=========================================================
 DEFINE_SCHEDULE	
 (
 SCHED_COMBINE_PATROL,

 "	Tasks"
 "		TASK_STOP_MOVING				0"
 "		TASK_WANDER						900540" 
 "		TASK_WALK_PATH					0"
 "		TASK_WAIT_FOR_MOVEMENT			0"
 "		TASK_STOP_MOVING				0"
 "		TASK_FACE_REASONABLE			0"
 "		TASK_WAIT						3"
 "		TASK_WAIT_RANDOM				3"
 "		TASK_SET_SCHEDULE				SCHEDULE:SCHED_COMBINE_PATROL" // keep doing it
 ""
 "	Interrupts"
 "		COND_NEW_ENEMY"
 "		COND_SEE_ENEMY"
 "		COND_ENEMY_DEAD"
 "		COND_LIGHT_DAMAGE"
 "		COND_HEAVY_DAMAGE"
 "		COND_CAN_RANGE_ATTACK1"
 "		COND_CAN_RANGE_ATTACK2"
 "		COND_HEAR_DANGER"
 "		COND_HEAR_COMBAT"
 "		COND_HEAR_MOVE_AWAY"
 )

 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_BUGBAIT_DISTRACTION,

 "	Tasks"
 "		TASK_STOP_MOVING		0"
 "		TASK_RESET_ACTIVITY		0"
 "		TASK_PLAY_SEQUENCE		ACTIVITY:ACT_COMBINE_BUGBAIT"
 ""
 "	Interrupts"
 ""
 )

 //=========================================================
 // SCHED_COMBINE_CHARGE_TURRET
 //
 //	Used to run straight at enemy turrets to knock them over.
 //  Prevents squadmates from throwing grenades during.
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_CHARGE_TURRET,

 "	Tasks"
 "		TASK_COMBINE_DEFER_SQUAD_GRENADES	0"
 "		TASK_STOP_MOVING					0"
 "		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_CHASE_ENEMY_FAILED"
 "		TASK_GET_CHASE_PATH_TO_ENEMY		300"
 "		TASK_RUN_PATH						0"
 "		TASK_WAIT_FOR_MOVEMENT				0"
 "		TASK_FACE_ENEMY						0"
 ""
 "	Interrupts"
 "		COND_NEW_ENEMY"
 "		COND_ENEMY_DEAD"
 "		COND_ENEMY_UNREACHABLE"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK2"
 "		COND_TOO_CLOSE_TO_ATTACK"
 "		COND_TASK_FAILED"
 "		COND_LOST_ENEMY"
 "		COND_BETTER_WEAPON_AVAILABLE"
 "		COND_HEAR_DANGER"
 )

 //=========================================================
 // SCHED_COMBINE_CHARGE_PLAYER
 //
 //	Used to run straight at enemy player since physgun combat
 //  is more fun when the enemies are close
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_CHARGE_PLAYER,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 "		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_CHASE_ENEMY_FAILED"
 "		TASK_COMBINE_CHASE_ENEMY_CONTINUOUSLY		256"
 "		TASK_FACE_ENEMY						0"
 ""
 "	Interrupts"
 "		COND_NEW_ENEMY"
 "		COND_ENEMY_DEAD"
 "		COND_ENEMY_UNREACHABLE"
 "		COND_HEAVY_DAMAGE"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK2"
 "		COND_TASK_FAILED"
 "		COND_LOST_ENEMY"
 )

 //=========================================================
 // SCHED_COMBINE_DROP_GRENADE
 //
 //	Place a grenade at my feet
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_DROP_GRENADE,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 "		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_SPECIAL_ATTACK2"
 "		TASK_FIND_COVER_FROM_ENEMY			99"
 "		TASK_FIND_FAR_NODE_COVER_FROM_ENEMY	384"
 "		TASK_CLEAR_MOVE_WAIT				0"
 "		TASK_RUN_PATH						0"
 "		TASK_WAIT_FOR_MOVEMENT				0"
 "		TASK_SET_SCHEDULE					SCHEDULE:SCHED_COMBINE_WAIT_IN_COVER"
 ""
 "	Interrupts"
 )

 //=========================================================
 // SCHED_COMBINE_DEFUSE
 //
 //	Defuse a nearby tripmine/satchel
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_DEFUSE,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 "		TASK_STORE_LASTPOSITION				0"
 "		TASK_GET_PATH_TO_BESTSOUND			0"
//"		TASK_GET_PATH_TO_TRIPMINE			0"
 "		TASK_WAIT							0.2"
 "		TASK_WALK_PATH						0"
 "		TASK_WAIT_FOR_MOVEMENT				0"
 "		TASK_STOP_MOVING					0"
 "		TASK_SPEAK_SENTENCE					4"
 "		TASK_WAIT							3"
 "		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_SPECIAL_ATTACK2"
 "		TASK_GET_PATH_TO_LASTPOSITION		0"
 "		TASK_RUN_PATH						0"
 "		TASK_WAIT_FOR_MOVEMENT				0"
 "		TASK_STOP_MOVING					0"
 "		TASK_SPEAK_SENTENCE					5"
 "		TASK_CLEAR_LASTPOSITION				0"
 "		TASK_FACE_IDEAL						0"
 ""
 "	Interrupts"
 "		COND_NEW_ENEMY"
 "		COND_SEE_ENEMY"
 "		COND_SEE_FEAR"
 "		COND_LIGHT_DAMAGE"
 "		COND_HEAVY_DAMAGE"
 "		COND_HEAR_DANGER"
 "		COND_HEAR_MOVE_AWAY"
 )

 //=========================================================
 // SCHED_COMBINE_PATROL_ENEMY
 //
 // Used instead if SCHED_COMBINE_PATROL if I have an enemy.
 // Wait for the enemy a bit in the hopes of ambushing him.
 //=========================================================
 DEFINE_SCHEDULE	
 (
 SCHED_COMBINE_PATROL_ENEMY,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 "		TASK_WAIT_FACE_ENEMY				1"
 "		TASK_WAIT_FACE_ENEMY_RANDOM			4"
 ""
 "	Interrupts"
 "		COND_NEW_ENEMY"
 "		COND_SEE_ENEMY"
 "		COND_ENEMY_DEAD"
 "		COND_LIGHT_DAMAGE"
 "		COND_HEAVY_DAMAGE"
 "		COND_CAN_RANGE_ATTACK1"
 "		COND_CAN_RANGE_ATTACK2"
 "		COND_HEAR_DANGER"
 "		COND_HEAR_MOVE_AWAY"
 )

 //=========================================================
 // Yooo you just got burned bro
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_BURNING_STAND,

 "	Tasks"
 "		TASK_SET_ACTIVITY				ACTIVITY:ACT_COMBINE_BUGBAIT"
 "		TASK_RANDOMIZE_FRAMERATE		12"
 "		TASK_WAIT						2"
 "		TASK_WAIT_RANDOM				3"
 "		TASK_COMBINE_DIE_INSTANTLY		DMG_BURN"
 "		TASK_WAIT						1.0"
 "	"
 "	Interrupts"
 )

 //=========================================================
 // SCHED_COMBINE_EVADE
 //
 // Roll away from incoming enemy.
 //=========================================================
 /*
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_EVADE,

 "	Tasks"
 "		TASK_STOP_MOVING						0"
 "		TASK_FIND_DODGE_POSITION				0"
 "		TASK_DODGE								0"
 ""
 "	Interrupts"
 )
 */

 AI_END_CUSTOM_NPC()
