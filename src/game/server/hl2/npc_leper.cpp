//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Fast, Jumpy, Scratchy, Scrawny little bastard.
//
// TODO's: Code this thing
//=============================================================================//

//-----------------------------------------------------------------------------
// Heres a rundown - 
// On the surface a clone of the fast zombie - however this enemy is more
// pack-based. When left to the base ai, lepers will gather up in numbers,
// circle and stalk the player and his allies, then attack all at once 
// in an attempt to overwhelm. Is almost blind, but can hear and smell things 
// that other npcs cant. Normal frenzy/charging behavior is usually reserved 
// for massed enemies, but can be enabled for all circumstances
//-----------------------------------------------------------------------------

#include "cbase.h"
#include "npc_leper.h"
#include "ai_network.h"
#include "AI_Navigator.h"
#include "AI_Motor.h"
#include "ai_default.h"
#include "ai_schedule.h"
#include "ai_squadslot.h"
#include "ai_squad.h"
#include "ai_hull.h"
#include "ai_node.h"
#include "ai_memory.h"
#include "ai_senses.h"
#include "soundent.h"
#include "player.h"
#include "game.h"
#include "npcevent.h"
#include "entitylist.h"
#include "ai_task.h"
#include "activitylist.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "globals.h"
#include "movevars_shared.h"
#include "IEffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define LEPER_STALK_MAX				2		// Will not use stalk behavior when have more than this amount of targets
#define LEPER_STALK_RANGE			600		// Closest range at which to stalk enemy
#define LEPER_STALK_RANGE_MAX		1500	// Max range at which to stalk enemy

#define LEPER_CHARGE_DISTANCE		300		// At this distance always charge into melee

ConVar	sk_leper_health( "sk_leper_health","0");
ConVar	sk_leper_dmg_slash( "sk_leper_dmg_slash","0");
ConVar	sk_leper_dmg_jump( "sk_leper_dmg_jump","0");

//=========================================================
// Anim Events Go Here
//=========================================================
int AE_LEPER_ATTACK_RIGHT;
int AE_LEPER_ATTACK_LEFT;
int AE_LEPER_ATTACK_BOTH;
int AE_LEPER_LEAP;
int AE_LEPER_GALLOP_LEFT;
int AE_LEPER_GALLOP_RIGHT;
int AE_LEPER_CLIMB_LEFT;
int AE_LEPER_CLIMB_RIGHT;

//=========================================================
// activities
//=========================================================
Activity ACT_LEPER_LEAP_SOAR;
Activity ACT_LEPER_LEAP_STRIKE;
Activity ACT_LEPER_LAND_RIGHT;
Activity ACT_LEPER_LAND_LEFT;
Activity ACT_LEPER_FRENZY;
Activity ACT_LEPER_BIG_SLASH;

enum LeperAggressivenessLevel_t
{
	AVOID,
	STALK,
	CLOSE,
	CHARGE,
};

LINK_ENTITY_TO_CLASS( npc_leper, CNPC_Leper );

BEGIN_DATADESC( CNPC_Leper )

	DEFINE_FIELD( m_flDistFactor, FIELD_FLOAT ),
	DEFINE_FIELD( m_iClimbCount, FIELD_CHARACTER ),
	DEFINE_FIELD( m_fIsNavJumping, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fIsAttackJumping, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fHitApex, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flJumpDist, FIELD_FLOAT ),
	DEFINE_FIELD( m_fHasScreamed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flNextMeleeAttack, FIELD_TIME ),
	DEFINE_FIELD( m_fJustJumped, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flJumpStartAltitude, FIELD_FLOAT ),

	// Function Pointers
	DEFINE_ENTITYFUNC( LeapAttackTouch ),
	DEFINE_ENTITYFUNC( ClimbTouch ),
//	DEFINE_SOUNDPATCH( m_pLayer2 ),

#ifdef HL2_EPISODIC
	DEFINE_ENTITYFUNC( VehicleLeapAttackTouch ),
	DEFINE_INPUTFUNC( FIELD_STRING, "AttachToVehicle", InputAttachToVehicle ),
#endif	// HL2_EPISODIC

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CNPC_Leper::Precache( void )
{
	PrecacheModel("models/leper.mdl");
#ifdef HL2_EPISODIC
	PrecacheScriptSound( "NPC_Leper.CarEnter1" );
	PrecacheScriptSound( "NPC_Leper.CarEnter2" );
	PrecacheScriptSound( "NPC_Leper.CarEnter3" );
	PrecacheScriptSound( "NPC_Leper.CarEnter4" );
	PrecacheScriptSound( "NPC_Leper.CarScream" );
#endif
	PrecacheScriptSound( "NPC_Leper.NoSound" );
	PrecacheScriptSound( "NPC_Leper.LeapAttack" );
	PrecacheScriptSound( "NPC_Leper.FootstepRight" );
	PrecacheScriptSound( "NPC_Leper.FootstepLeft" );
	PrecacheScriptSound( "NPC_Leper.AttackHit" );
	PrecacheScriptSound( "NPC_Leper.AttackMiss" );
	PrecacheScriptSound( "NPC_Leper.LeapAttack" );
	PrecacheScriptSound( "NPC_Leper.Attack" );
	PrecacheScriptSound( "NPC_Leper.Pain" );
	PrecacheScriptSound( "NPC_Leper.Die" );
	PrecacheScriptSound( "NPC_Leper.Idle" );
	PrecacheScriptSound( "NPC_Leper.AlertFar" );
	PrecacheScriptSound( "NPC_Leper.AlertNear" );
	PrecacheScriptSound( "NPC_Leper.GallopLeft" );
	PrecacheScriptSound( "NPC_Leper.GallopRight" );
	PrecacheScriptSound( "NPC_Leper.Scream" );
	PrecacheScriptSound( "NPC_Leper.RangeAttack" );
	PrecacheScriptSound( "NPC_Leper.Frenzy" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Leper::Spawn( void )
{
	Precache( );

	SetModel("models/leper.mdl");
	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
#ifdef HL2_EPISODIC
	SetBloodColor( BLOOD_COLOR_ZOMBIE );
#else
	SetBloodColor( BLOOD_COLOR_YELLOW );
#endif // HL2_EPISODIC

	m_fJustJumped = false;

	m_iHealth			= sk_leper_health.GetFloat();
	m_flFieldOfView		= 0.1;	//Almost blind
	m_NPCState			= NPC_STATE_NONE;

	CapabilitiesClear();
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_MELEE_ATTACK1 );
	CapabilitiesAdd( bits_CAP_MOVE_CLIMB | bits_CAP_MOVE_JUMP | bits_CAP_INNATE_RANGE_ATTACK1 );
	CapabilitiesAdd( bits_CAP_SQUAD );

	m_flNextAttack = gpGlobals->curtime;

//	m_pLayer2 = NULL;
	m_iClimbCount = 0;
	EndNavJump();
	m_flDistFactor = 1.0;

	NPCInit();

#ifdef HL2_EPISODIC
	m_ActBusyBehavior.SetUseRenderBounds(true);
#endif

	// Zombies get to cheat for 6 seconds (sjb)
	GetEnemies()->SetFreeKnowledgeDuration( 6.0 );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CNPC_Leper::OnScheduleChange( void )
{
	if ( m_flNextMeleeAttack > gpGlobals->curtime + 1 )
	{
		// Allow melee attacks again.
		m_flNextMeleeAttack = gpGlobals->curtime + 0.5;
	}

	BaseClass::OnScheduleChange();
}

//---------------------------------------------------------
//---------------------------------------------------------
int CNPC_Leper::SelectSchedule( void )
{
#ifdef HL2_EPISODIC

	// Defer all decisions to the behavior if it's running
	if ( m_PassengerBehavior.CanSelectSchedule() )
	{
		DeferSchedulingToBehavior( &m_PassengerBehavior );
		return BaseClass::SelectSchedule();
	}

#endif //HL2_EPISODIC

	if ( m_NPCState != NPC_STATE_SCRIPT )
	{
		if ( HasCondition( COND_ON_FIRE ) )
		{
			if( !m_fHasScreamed )
			{
				EmitSound( "NPC_Leper.Scream" );
				m_fHasScreamed = true;
			}
		}

		if ( HasCondition( COND_LEPER_CLIMB_TOUCH ) )
		{
			return SCHED_LEPER_UNSTICK_JUMP;
		}

		if( HasCondition( COND_CAN_MELEE_ATTACK1 ) )
		{
			return SCHED_MELEE_ATTACK1;
		}
	}

	switch ( m_NPCState )
	{
	case NPC_STATE_IDLE:
		{
			// Investigate nearby sounds
			if( HasCondition( COND_SMELL ) || HasCondition ( COND_HEAR_PLAYER ) )
			{
				CSound *pSound = GetBestSound();
				if( pSound )
				{
					return SCHED_INVESTIGATE_SOUND;
				}
				return SCHED_ALERT_FACE_BESTSOUND;
			}
		}
		// NOTE: Fall through - Idle and Alert use the same set of schedules!

	case NPC_STATE_ALERT:
		if ( HasCondition( COND_LOST_ENEMY ) || ( HasCondition( COND_ENEMY_UNREACHABLE ) ) )
		{
			ClearCondition( COND_LOST_ENEMY );
			ClearCondition( COND_ENEMY_UNREACHABLE );
			SetEnemy( NULL );

			// Just lost track of our enemy. 
			// Wander around a bit so we don't look like a dingus.
			return SCHED_PATROL_WALK;
		}

		if ( HasCondition( COND_LIGHT_DAMAGE ) )
		{
			AI_EnemyInfo_t *pDanger = GetEnemies()->GetDangerMemory();
			if( pDanger && FInViewCone(pDanger->vLastKnownLocation) && !BaseClass::FVisible(pDanger->vLastKnownLocation) )
			{
				// I've been hurt, I'm facing the danger, but I don't see it, so move from this position.
				return SCHED_TAKE_COVER_FROM_ORIGIN;
			}
			else if ( SelectWeightedSequence( ACT_SMALL_FLINCH ) != ACTIVITY_NOT_AVAILABLE )
			{
				// I'm not facing the danger but it hasnt triggered an attack response
				return SCHED_ALERT_SMALL_FLINCH;
			}
		}

		// Investigate nearby sounds
		if( HasCondition( COND_HEAR_COMBAT ) || HasCondition ( COND_HEAR_PLAYER ) )
		{
			CSound *pSound = GetBestSound();
			if( pSound )
			{
				return SCHED_INVESTIGATE_SOUND;
			}
			return SCHED_ALERT_FACE_BESTSOUND;
		}

		// Scan around for new enemies
		if ( HasCondition( COND_ENEMY_DEAD ) && SelectWeightedSequence( ACT_VICTORY_DANCE ) != ACTIVITY_NOT_AVAILABLE )
		{
			return SCHED_VICTORY_DANCE;
		}
		break;

	case NPC_STATE_COMBAT:
		if ( HasCondition( COND_LOST_ENEMY ) || ( HasCondition( COND_ENEMY_UNREACHABLE ) ) )
		{
			// Set state to alert and recurse!
			SetState( NPC_STATE_ALERT );
			return SelectSchedule();
		}

		if ( HasCondition( COND_LIGHT_DAMAGE ) )
		{
			if ( GetEnemy() != NULL )
			{
				if ( !IsMoving() )
				{
					// Got blamm'd while being annoying
					return SCHED_SMALL_FLINCH;
				}
				return SCHED_TAKE_COVER_FROM_ENEMY;
			}
		}
		break;
	}

	return BaseClass::SelectSchedule();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Leper::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();

	if( GetGroundEntity() && GetGroundEntity()->Classify() == CLASS_HEADCRAB )
	{
		// Kill!
		CTakeDamageInfo info;
		info.SetDamage( GetGroundEntity()->GetHealth() );
		info.SetAttacker( this );
		info.SetInflictor( this );
		info.SetDamageType( DMG_GENERIC );
		GetGroundEntity()->TakeDamage( info );
	}

	// Crudely detect the apex of our jump
	if( IsNavJumping() && !m_fHitApex && GetAbsVelocity().z <= 0.0 )
	{
		OnNavJumpHitApex();
	}

	if( IsCurSchedule(SCHED_LEPER_RANGE_ATTACK1, false) )
	{
		// Think more frequently when flying quickly through the 
		// air, to update the server's location more often.
		SetNextThink(gpGlobals->curtime);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CNPC_Leper::MaxYawSpeed( void )
{
	switch( GetActivity() )
	{
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		return 45;
		break;
	case ACT_RUN:
		return 30;
		break;
	case ACT_WALK:
	case ACT_IDLE:
		return 25;
		break;
	default:
		return 20;
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: See if I can swat the player
//-----------------------------------------------------------------------------
int CNPC_Leper::MeleeAttack1Conditions( float flDot, float flDist )
{
	if( !(GetFlags() & FL_ONGROUND) )
	{
		// Have to be on the ground!
		return COND_NONE;
	}

	if( gpGlobals->curtime < m_flNextMeleeAttack )
	{
		return COND_NONE;
	}

	if (flDist > InnateRange1MaxRange() )
	{
		// Translate a hit vehicle into its passenger if found
		if ( GetEnemy() != NULL )
		{
#if defined(HL2_DLL) && !defined(HL2MP)
			// If the player is holding an object, knock it down.
			if( GetEnemy()->IsPlayer() )
			{
				CBasePlayer *pPlayer = ToBasePlayer( GetEnemy() );

				Assert( pPlayer != NULL );

				// Is the player carrying something?
				CBaseEntity *pObject = GetPlayerHeldEntity(pPlayer);

				if( !pObject )
				{
					pObject = PhysCannonGetHeldEntity( pPlayer->GetActiveWeapon() );
				}

				if( pObject )
				{
					float flDist = pObject->WorldSpaceCenter().DistTo( WorldSpaceCenter() );

					if( flDist <= InnateMelee1Range() )
						return COND_CAN_MELEE_ATTACK1;
				}
			}
#endif
		}
		return COND_TOO_FAR_TO_ATTACK;
	}

	if (flDot < 0.7)
	{
		return COND_NOT_FACING_ATTACK;
	}

	// Check Z
	if ( GetEnemy() && fabs(GetEnemy()->GetAbsOrigin().z - GetAbsOrigin().z) > InnateRange1MaxRange() )
		return COND_NONE;

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
// Purpose: Play a random attack hit sound
//-----------------------------------------------------------------------------
void CNPC_Leper::AttackHitSound( void )
{
	EmitSound( "NPC_Leper.AttackHit" );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack miss sound
//-----------------------------------------------------------------------------
void CNPC_Leper::AttackMissSound( void )
{
	EmitSound( "NPC_Leper.AttackMiss" );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack sound.
//-----------------------------------------------------------------------------
void CNPC_Leper::LeapAttackSound( void )
{
	EmitSound( "NPC_Leper.LeapAttack" );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack sound.
//-----------------------------------------------------------------------------
void CNPC_Leper::AttackSound( void )
{
	EmitSound( "NPC_Leper.Attack" );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random idle sound.
//-----------------------------------------------------------------------------
void CNPC_Leper::IdleSound( void )
{
	EmitSound( "NPC_Leper.Idle" );
	if ( HL2GameRules()->IsAlyxInDarknessMode() )
	{
		CSoundEnt::InsertSound( SOUND_COMBAT, EyePosition(), 360.0f, 0.5, this, SOUNDENT_CHANNEL_SPOOKY_NOISE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play a random pain sound.
//-----------------------------------------------------------------------------
void CNPC_Leper::PainSound( const CTakeDamageInfo &info )
{
	EmitSound( "NPC_Leper.Pain" );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Leper::DeathSound( const CTakeDamageInfo &info ) 
{
	EmitSound( "NPC_Leper.Die" );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random alert sound.
//-----------------------------------------------------------------------------
void CNPC_Leper::AlertSound( void )
{
	// Measure how far the player is, and play the appropriate type of alert sound. 
	// Doesn't matter if I'm getting mad at a different character, the player is the
	// one that hears the sound.
	CBaseEntity *pEnemy = GetEnemy();
	float flDist;
	
	flDist = ( GetAbsOrigin() - pEnemy->GetAbsOrigin() ).Length();

	if( flDist > 512 )
	{
		EmitSound( "NPC_Leper.AlertFar" );
	}
	else
	{
		EmitSound( "NPC_Leper.AlertNear" );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#define LEPER_MINLEAP			200

//-----------------------------------------------------------------------------
// Purpose: See if I can make my leaping attack!!
//
//
//-----------------------------------------------------------------------------
int CNPC_Leper::RangeAttack1Conditions( float flDot, float flDist )
{
	if ( !GetEnemy() )
	{
		return COND_NONE;
	}

	if( !(GetFlags() & FL_ONGROUND) )
	{
		return COND_NONE;
	}

	if( gpGlobals->curtime < m_flNextAttack )
	{
		return COND_NONE;
	}

	// make sure the enemy isn't on a roof and I'm in the streets (Ravenholm)
	float flZDist;
	flZDist = fabs( GetEnemy()->GetLocalOrigin().z - GetLocalOrigin().z );
	if( flZDist > LEPER_MAXLEAP_Z )
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	if( flDist > InnateRange1MaxRange() )
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	if( flDist < LEPER_MINLEAP )
	{
		return COND_NONE;
	}

	if (flDot < 0.8) 
	{
		return COND_NONE;
	}

	if ( !IsMoving() )
	{
		// I Have to be running!!!
		return COND_NONE;
	}

	// Don't jump at the player unless he's facing me.
	// This allows the player to get away if he turns and sprints
	CBasePlayer *pPlayer = static_cast<CBasePlayer*>( GetEnemy() );

	if( pPlayer )
	{
		// If the enemy is a player, don't attack from behind!
		if( !pPlayer->FInViewCone( this ) )
		{
			return COND_NONE;
		}
	}

	// Drumroll please!
	// The final check! Is the path from my position to halfway between me
	// and the player clear?
	trace_t tr;
	Vector vecDirToEnemy;

	vecDirToEnemy = GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter();
	Vector vecHullMin( -16, -16, -16 );
	Vector vecHullMax( 16, 16, 16 );

	// only check half the distance. (the first part of the jump)
	vecDirToEnemy = vecDirToEnemy * 0.5;

	AI_TraceHull( WorldSpaceCenter(), WorldSpaceCenter() + vecDirToEnemy, vecHullMin, vecHullMax, MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );

	if( tr.fraction != 1.0 )
	{
		// There's some sort of obstacle pretty much right in front of me.
		return COND_NONE;
	}

	return COND_CAN_RANGE_ATTACK1;
}

//-----------------------------------------------------------------------------
// Purpose:
//
//
//-----------------------------------------------------------------------------
void CNPC_Leper::HandleAnimEvent( animevent_t *pEvent )
{
#ifdef HL2_EPISODIC
	// Do the leap attack
	if ( pEvent->event == AE_LEPER_VEHICLE_LEAP )
	{
		VehicleLeapAttack();
		return;
	}

	// Die while doing an SS in a vehicle
	if ( pEvent->event == AE_LEPER_VEHICLE_SS_DIE )
	{
		if ( IsInAVehicle() )
		{
			// Get the vehicle's present speed as a baseline
			Vector vecVelocity = vec3_origin;
			CBaseEntity *pVehicle = m_PassengerBehavior.GetTargetVehicle();
			if ( pVehicle )
			{
				pVehicle->GetVelocity( &vecVelocity, NULL );
			}

			// TODO: We need to make this content driven -- jdw
			Vector vecForward, vecRight, vecUp;
			GetVectors( &vecForward, &vecRight, &vecUp );

			vecVelocity += ( vecForward * -2500.0f ) + ( vecRight * 200.0f ) + ( vecUp * 300 );
			
			// Always kill
			float flDamage = GetMaxHealth() + 10;

			// Take the damage and die
			CTakeDamageInfo info( this, this, vecVelocity * 25.0f, WorldSpaceCenter(), flDamage, (DMG_CRUSH|DMG_VEHICLE) );
			TakeDamage( info );
		}
		return;
	}
#endif

	switch( pEvent->event )
	{
	case NPC_EVENT_LEFTFOOT:
		{
			EmitSound( "NPC_Leper.FootstepLeft", pEvent->eventtime );
		}
		break;
	case NPC_EVENT_RIGHTFOOT:
		{
			EmitSound( "NPC_Leper.FootstepRight", pEvent->eventtime );
		}
		break;
	case AE_LEPER_CLIMB_LEFT:
	case AE_LEPER_CLIMB_RIGHT:
		if( ++m_iClimbCount % 3 == 0 )
		{
		//	ENVELOPE_CONTROLLER.SoundChangePitch( m_pLayer2, random->RandomFloat( 100, 150 ), 0.0 );
		//	ENVELOPE_CONTROLLER.SoundPlayEnvelope( m_pLayer2, SOUNDCTRL_CHANGE_VOLUME, envFastZombieVolumeClimb, ARRAYSIZE(envFastZombieVolumeClimb) );
		}
		break;
	case AE_LEPER_LEAP:
		{
			LeapAttack();
			return;
		}
		break;
	case AE_LEPER_GALLOP_LEFT:
		{
			EmitSound( "NPC_Leper.GallopLeft", pEvent->eventtime );
		}
		break;
	case AE_LEPER_GALLOP_RIGHT:
		{
			EmitSound( "NPC_Leper.GallopRight", pEvent->eventtime );
		}
		break;
	case AE_LEPER_ATTACK_LEFT:
		{
			Vector right;
			AngleVectors( GetLocalAngles(), NULL, &right, NULL );
			right = right * 50;
			ClawAttack( InnateRange1MaxRange(), sk_leper_dmg_slash.GetFloat(), QAngle( -3, 5, -3 ), right, LEPER_BLOOD_LEFT_HAND );
		}
		break;
	case AE_LEPER_ATTACK_RIGHT:
		{
			Vector right;
			AngleVectors( GetLocalAngles(), NULL, &right, NULL );
			right = right * -50;

			ClawAttack( InnateRange1MaxRange(), sk_leper_dmg_slash.GetFloat(), QAngle( -3, -5, -3 ), right, LEPER_BLOOD_RIGHT_HAND );
			return;
		}
		break;

	default:
		BaseClass::HandleAnimEvent( pEvent );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Claw the enemy!!
//-----------------------------------------------------------------------------
CBaseEntity *CNPC_Leper::ClawAttack( float flDist, int iDamage, QAngle &qaViewPunch, Vector &vecVelocityPunch, int BloodOrigin )
{
	// Added test because claw attack anim sometimes used when for cases other than melee
	int iDriverInitialHealth = -1;
	CBaseEntity *pDriver = NULL;
	if ( GetEnemy() )
	{
		trace_t	tr;
		AI_TraceHull( WorldSpaceCenter(), GetEnemy()->WorldSpaceCenter(), -Vector(8,8,8), Vector(8,8,8), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );

		if ( tr.fraction < 1.0f )
			return NULL;

		// CheckTraceHullAttack() can damage player in vehicle as side effect of melee attack damaging physics objects, which the car forwards to the player
		// need to detect this to get correct damage effects
		CBaseCombatCharacter *pCCEnemy = ( GetEnemy() != NULL ) ? GetEnemy()->MyCombatCharacterPointer() : NULL;
		CBaseEntity *pVehicleEntity;
		if ( pCCEnemy != NULL && ( pVehicleEntity = pCCEnemy->GetVehicleEntity() ) != NULL )
		{
			if ( pVehicleEntity->GetServerVehicle() && dynamic_cast<CPropVehicleDriveable *>(pVehicleEntity) )
			{
				pDriver = static_cast<CPropVehicleDriveable *>(pVehicleEntity)->GetDriver();
				if ( pDriver && pDriver->IsPlayer() )
				{
					iDriverInitialHealth = pDriver->GetHealth();
				}
				else
				{
					pDriver = NULL;
				}
			}
		}
	}

	//
	// Trace out a cubic section of our hull and see what we hit.
	//
	Vector vecMins = GetHullMins();
	Vector vecMaxs = GetHullMaxs();
	vecMins.z = vecMins.x;
	vecMaxs.z = vecMaxs.x;

	CBaseEntity *pHurt = NULL;
	if ( GetEnemy() && GetEnemy()->Classify() == CLASS_BULLSEYE )
	{ 
		// We always hit bullseyes we're targeting
		pHurt = GetEnemy();
		CTakeDamageInfo info( this, this, vec3_origin, GetAbsOrigin(), iDamage, DMG_SLASH );
		pHurt->TakeDamage( info );
	}
	else 
	{
		// Try to hit them with a trace
		pHurt = CheckTraceHullAttack( flDist, vecMins, vecMaxs, iDamage, DMG_SLASH );
	}

	if ( pDriver && iDriverInitialHealth != pDriver->GetHealth() )
	{
		pHurt = pDriver;
	}

	if ( !pHurt && m_hPhysicsEnt != NULL && IsCurSchedule(SCHED_LEPER_MELEE_ATTACK1) )
	{
		pHurt = m_hPhysicsEnt;

		Vector vForce = pHurt->WorldSpaceCenter() - WorldSpaceCenter(); 
		VectorNormalize( vForce );

		vForce *= 5 * 24;

		CTakeDamageInfo info( this, this, vForce, GetAbsOrigin(), iDamage, DMG_SLASH );
		pHurt->TakeDamage( info );

		pHurt = m_hPhysicsEnt;
	}

	if ( pHurt )
	{
		AttackHitSound();

		CBasePlayer *pPlayer = ToBasePlayer( pHurt );

		if ( pPlayer != NULL && !(pPlayer->GetFlags() & FL_GODMODE ) )
		{
			pPlayer->ViewPunch( qaViewPunch );
			
			pPlayer->VelocityPunch( vecVelocityPunch );
		}
		if( UTIL_ShouldShowBlood(pHurt->BloodColor()) )
		{
			// Hit an NPC. Bleed them!
			Vector vecBloodPos;

			switch( BloodOrigin )
			{
			case LEPER_BLOOD_LEFT_HAND:
				if( GetAttachment( "blood_left", vecBloodPos ) )
					SpawnBlood( vecBloodPos, g_vecAttackDir, pHurt->BloodColor(), min( iDamage, 30 ) );
				break;

			case LEPER_BLOOD_RIGHT_HAND:
				if( GetAttachment( "blood_right", vecBloodPos ) )
					SpawnBlood( vecBloodPos, g_vecAttackDir, pHurt->BloodColor(), min( iDamage, 30 ) );
				break;

			case LEPER_BLOOD_BOTH_HANDS:
				if( GetAttachment( "blood_left", vecBloodPos ) )
					SpawnBlood( vecBloodPos, g_vecAttackDir, pHurt->BloodColor(), min( iDamage, 30 ) );

				if( GetAttachment( "blood_right", vecBloodPos ) )
					SpawnBlood( vecBloodPos, g_vecAttackDir, pHurt->BloodColor(), min( iDamage, 30 ) );
				break;

			case LEPER_BLOOD_BITE:
				// No blood for these.
				break;
			}
		}
	}
	else 
	{
		AttackMissSound();
	}

	return pHurt;
}

//-----------------------------------------------------------------------------
// Purpose: Jump at the enemy!! (stole this from the headcrab)
//-----------------------------------------------------------------------------
void CNPC_Leper::LeapAttack( void )
{
	SetGroundEntity( NULL );

	BeginAttackJump();

	LeapAttackSound();

	//
	// Take him off ground so engine doesn't instantly reset FL_ONGROUND.
	//
	UTIL_SetOrigin( this, GetLocalOrigin() + Vector( 0 , 0 , 1 ));

	Vector vecJumpDir;
	CBaseEntity *pEnemy = GetEnemy();

	if ( pEnemy )
	{
		Vector vecEnemyPos = pEnemy->WorldSpaceCenter();

		float gravity = sv_gravity.GetFloat();
		if ( gravity <= 1 )
		{
			gravity = 1;
		}

		//
		// How fast does the zombie need to travel to reach my enemy's eyes given gravity?
		//
		float height = ( vecEnemyPos.z - GetAbsOrigin().z );

		if ( height < 16 )
		{
			height = 16;
		}
		else if ( height > 120 )
		{
			height = 120;
		}
		float speed = sqrt( 2 * gravity * height );
		float time = speed / gravity;

		//
		// Scale the sideways velocity to get there at the right time
		//
		vecJumpDir = vecEnemyPos - GetAbsOrigin();
		vecJumpDir = vecJumpDir / time;

		//
		// Speed to offset gravity at the desired height.
		//
		vecJumpDir.z = speed;

		//
		// Don't jump too far/fast.
		//
#define CLAMP 1000.0
		float distance = vecJumpDir.Length();
		if ( distance > CLAMP )
		{
			vecJumpDir = vecJumpDir * ( CLAMP / distance );
		}

		// try speeding up a bit.
		SetAbsVelocity( vecJumpDir );
		m_flNextAttack = gpGlobals->curtime + 2;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Leper::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_LEPER_VERIFY_ATTACK:
		// Simply ensure that the zombie still has a valid melee attack
		if( HasCondition( COND_CAN_MELEE_ATTACK1 ) )
		{
			TaskComplete();
		}
		else
		{
			TaskFail("");
		}
		break;

	case TASK_LEPER_JUMP_BACK:
		{
			SetActivity( ACT_IDLE );

			SetGroundEntity( NULL );

			BeginAttackJump();

			Vector forward;
			AngleVectors( GetLocalAngles(), &forward );

			//
			// Take him off ground so engine doesn't instantly reset FL_ONGROUND.
			//
			UTIL_SetOrigin( this, GetLocalOrigin() + Vector( 0 , 0 , 1 ));

			ApplyAbsVelocityImpulse( forward * -200 + Vector( 0, 0, 200 ) );
		}
		break;

	case TASK_LEPER_UNSTICK_JUMP:
		{
			SetGroundEntity( NULL );

			// Call begin attack jump. A little bit later if we fail to pathfind, we check
			// this value to see if we just jumped. If so, we assume we've jumped 
			// to someplace that's not pathing friendly, and so must jump again to get out.
			BeginAttackJump();

			//
			// Take him off ground so engine doesn't instantly reset FL_ONGROUND.
			//
			UTIL_SetOrigin( this, GetLocalOrigin() + Vector( 0 , 0 , 1 ));

			CBaseEntity *pEnemy = GetEnemy();
			Vector vecJumpDir;

			if ( GetActivity() == ACT_CLIMB_UP || GetActivity() == ACT_CLIMB_DOWN )
			{
				// Jump off the pipe backwards!
				Vector forward;

				GetVectors( &forward, NULL, NULL );

				ApplyAbsVelocityImpulse( forward * -200 );
			}
			else if( pEnemy )
			{
				vecJumpDir = pEnemy->GetLocalOrigin() - GetLocalOrigin();
				VectorNormalize( vecJumpDir );
				vecJumpDir.z = 0;

				ApplyAbsVelocityImpulse( vecJumpDir * 300 + Vector( 0, 0, 200 ) );
			}
			else
			{
				DevMsg("UNHANDLED CASE! Stuck Fast Zombie with no enemy!\n");
			}
		}
		break;

	case TASK_WAIT_FOR_MOVEMENT:
		// If we're waiting for movement, that means that pathfinding succeeded, and
		// we're about to be moving. So we aren't stuck. So clear this flag. 
		m_fJustJumped = false;

		BaseClass::StartTask( pTask );
		break;

	case TASK_FACE_ENEMY:
		{
			// We don't use the base class implementation of this, because GetTurnActivity
			// stomps our landing scrabble animations (sjb)
			Vector flEnemyLKP = GetEnemyLKP();
			GetMotor()->SetIdealYawToTarget( flEnemyLKP );
		}
		break;

	case TASK_LEPER_LAND_RECOVER:
		{
			// Set the ideal yaw
			Vector flEnemyLKP = GetEnemyLKP();
			GetMotor()->SetIdealYawToTarget( flEnemyLKP );

			// figure out which way to turn.
			float flDeltaYaw = GetMotor()->DeltaIdealYaw();

			if( flDeltaYaw < 0 )
			{
				SetIdealActivity( (Activity)ACT_LEPER_LAND_RIGHT );
			}
			else
			{
				SetIdealActivity( (Activity)ACT_LEPER_LAND_LEFT );
			}


			TaskComplete();
		}
		break;

	case TASK_RANGE_ATTACK1:
		// Make melee attacks impossible until we land!
		m_flNextMeleeAttack = gpGlobals->curtime + 60;

		SetTouch( &CNPC_Leper::LeapAttackTouch );
		break;

	case TASK_LEPER_DO_ATTACK:
		SetActivity( (Activity)ACT_LEPER_LEAP_SOAR );
		break;

	default:
		BaseClass::StartTask( pTask );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Leper::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_LEPER_JUMP_BACK:
	case TASK_LEPER_UNSTICK_JUMP:
		if( GetFlags() & FL_ONGROUND )
		{
			TaskComplete();
		}
		break;

	case TASK_RANGE_ATTACK1:
		if( ( GetFlags() & FL_ONGROUND ) || ( m_pfnTouch == NULL ) )
		{
			// All done when you touch the ground, or if our touch function has somehow cleared.
			TaskComplete();

			// Allow melee attacks again.
			m_flNextMeleeAttack = gpGlobals->curtime + 0.5;
			return;
		}
		break;

	default:
		BaseClass::RunTask( pTask );
		break;
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
int CNPC_Leper::TranslateSchedule( int scheduleType )
{
	switch( scheduleType )
	{
	case SCHED_LEPER_UNSTICK_JUMP:
		if ( GetActivity() == ACT_CLIMB_UP || GetActivity() == ACT_CLIMB_DOWN || GetActivity() == ACT_CLIMB_DISMOUNT )
		{
			return SCHED_LEPER_CLIMBING_UNSTICK_JUMP;
		}
		else
		{
			return SCHED_LEPER_UNSTICK_JUMP;
		}
		break;

	case SCHED_RANGE_ATTACK1:
		{
			// Scream right now, cause in half a second, we're gonna jump!!
			if( !m_fHasScreamed )
			{
				// Only play that over-the-top attack scream once per combat state.
				EmitSound( "NPC_Leper.Scream" );
				m_fHasScreamed = true;
			}
			else
			{
				EmitSound( "NPC_Leper.RangeAttack" );
			}
			return SCHED_LEPER_RANGE_ATTACK1;
		}

	case SCHED_MELEE_ATTACK1:
		return SCHED_LEPER_MELEE_ATTACK1;
		break;

	case SCHED_CHASE_ENEMY:
		return SCHED_LEPER_CHASE_ENEMY;
		break;

	case SCHED_TAKE_COVER_FROM_ENEMY:
		if ( HasCondition( COND_CAN_MELEE_ATTACK1 ) )
		{
			return TranslateSchedule( SCHED_MELEE_ATTACK1 );
		}
		break;

	case SCHED_CHASE_ENEMY_FAILED:
		return SCHED_COMBAT_FACE;
		break;

	case SCHED_ESTABLISH_LINE_OF_FIRE:
	case SCHED_MOVE_TO_WEAPON_RANGE:
		{
			float flZDist = fabs( GetEnemy()->GetLocalOrigin().z - GetLocalOrigin().z );
			if ( flZDist > LEPER_MAXLEAP_Z )
				return TranslateSchedule( SCHED_CHASE_ENEMY );
			else // fall through to default
				return BaseClass::TranslateSchedule( scheduleType );
			break;
		}

	default:
		return BaseClass::TranslateSchedule( scheduleType );
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
Activity CNPC_Leper::NPC_TranslateActivity( Activity eNewActivity )
{
	if ( eNewActivity == ACT_CLIMB_DOWN )
		return ACT_CLIMB_UP;

	switch ( eNewActivity )
	{
		case ACT_IDLE:
			if ( m_NPCState == NPC_STATE_COMBAT )
			{
				eNewActivity = ACT_IDLE_ANGRY;
			}
		break;

		case ACT_WALK:
			if ( m_NPCState == NPC_STATE_COMBAT )
			{
				eNewActivity = ACT_WALK_ANGRY;
			}
		break;
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CNPC_Leper::LeapAttackTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsSolid() )
	{
		// Touching a trigger or something.
		return;
	}

	// Stop the zombie and knock the player back
	Vector vecNewVelocity( 0, 0, GetAbsVelocity().z );
	SetAbsVelocity( vecNewVelocity );

	Vector forward;
	AngleVectors( GetLocalAngles(), &forward );
	QAngle qaPunch( 15, random->RandomInt(-5,5), random->RandomInt(-5,5) );
	
	ClawAttack( InnateRange1MaxRange(), 5, qaPunch, forward * 500, LEPER_BLOOD_BOTH_HANDS );

	SetTouch( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Lets us know if we touch the player while we're climbing.
//-----------------------------------------------------------------------------
void CNPC_Leper::ClimbTouch( CBaseEntity *pOther )
{
	if ( pOther->IsPlayer() )
	{
		// If I hit the player, shove him aside.
		Vector vecDir = pOther->WorldSpaceCenter() - WorldSpaceCenter();
		vecDir.z = 0.0; // planar
		VectorNormalize( vecDir );

		if( IsXbox() )
		{
			vecDir *= 400.0f;
		}
		else
		{
			vecDir *= 200.0f;
		}

		pOther->VelocityPunch( vecDir );

		if ( GetActivity() != ACT_CLIMB_DISMOUNT || 
			 ( pOther->GetGroundEntity() == NULL &&
			   GetNavigator()->IsGoalActive() &&
			   pOther->GetAbsOrigin().z - GetNavigator()->GetCurWaypointPos().z < -1.0 ) )
		{
			SetCondition( COND_LEPER_CLIMB_TOUCH );
		}

		SetTouch( NULL );
	}
	else if ( dynamic_cast<CPhysicsProp *>(pOther) )
	{
		NPCPhysics_CreateSolver( this, pOther, true, 5.0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if a reasonable jumping distance
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CNPC_Leper::IsJumpLegal(const Vector &startPos, const Vector &apex, const Vector &endPos) const
{
	const float MAX_JUMP_RISE		= 220.0f;
	const float MAX_JUMP_DISTANCE	= 512.0f;
	const float MAX_JUMP_DROP		= 384.0f;

	if ( BaseClass::IsJumpLegal( startPos, apex, endPos, MAX_JUMP_RISE, MAX_JUMP_DROP, MAX_JUMP_DISTANCE ) )
	{
		// Hang onto the jump distance. The AI is going to want it.
		m_flJumpDist = (startPos - endPos).Length();

		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

bool CNPC_Leper::MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost )
{
	float delta = vecEnd.z - vecStart.z;

	float multiplier = 1;
	if ( moveType == bits_CAP_MOVE_JUMP )
	{
		multiplier = ( delta < 0 ) ? 0.5 : 1.5;
	}
	else if ( moveType == bits_CAP_MOVE_CLIMB )
	{
		multiplier = ( delta > 0 ) ? 0.5 : 4.0;
	}

	*pCost *= multiplier;

	return ( multiplier != 1 );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

bool CNPC_Leper::ShouldFailNav( bool bMovementFailed )
{
	if ( !BaseClass::ShouldFailNav( bMovementFailed ) )
	{
		DevMsg( 2, "Fast zombie in scripted sequence probably hit bad node configuration at %s\n", VecToString( GetAbsOrigin() ) );
		
		if ( GetNavigator()->GetPath()->CurWaypointNavType() == NAV_JUMP && GetNavigator()->RefindPathToGoal( false ) )
		{
			return false;
		}
		DevMsg( 2, "Fast zombie failed to get to scripted sequence\n" );
	}

	return true;
}


//---------------------------------------------------------
// Purpose: Notifier that lets us know when the fast
//			zombie has hit the apex of a navigational jump.
//---------------------------------------------------------
void CNPC_Leper::OnNavJumpHitApex( void )
{
	m_fHitApex = true;	// stop subsequent notifications
}

//---------------------------------------------------------
// Purpose: Overridden to detect when the zombie goes into
//			and out of his climb state and his navigation
//			jump state.
//---------------------------------------------------------
void CNPC_Leper::OnChangeActivity( Activity NewActivity )
{
	if ( NewActivity == ACT_LEPER_FRENZY )
	{
		// Scream!!!!
		EmitSound( "NPC_Leper.Frenzy" );
		SetPlaybackRate( random->RandomFloat( .9, 1.1 ) );	
	}

	if( NewActivity == ACT_JUMP )
	{
		BeginNavJump();
	}
	else if( GetActivity() == ACT_JUMP )
	{
		EndNavJump();
	}

	if ( NewActivity == ACT_LAND )
	{
		m_flNextAttack = gpGlobals->curtime + 1.0;
	}

	if ( NewActivity == ACT_GLIDE )
	{
		// Started a jump.
		BeginNavJump();
	}
	else if ( GetActivity() == ACT_GLIDE )
	{
		// Landed a jump
		EndNavJump();
	}

	if ( NewActivity == ACT_CLIMB_UP )
	{
		SetTouch( &CNPC_Leper::ClimbTouch );
	}
	else if ( GetActivity() == ACT_CLIMB_DISMOUNT || ( GetActivity() == ACT_CLIMB_UP && NewActivity != ACT_CLIMB_DISMOUNT ) )
	{
		SetTouch( NULL );
	}

	BaseClass::OnChangeActivity( NewActivity );
}


//=========================================================
// 
//=========================================================
int CNPC_Leper::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	if ( m_fJustJumped )
	{
		// Assume we failed cause we jumped to a bad place.
		m_fJustJumped = false;
		return SCHED_LEPER_UNSTICK_JUMP;
	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//=========================================================
// Purpose: Do some record keeping for jumps made for 
//			navigational purposes (i.e., not attack jumps)
//=========================================================
void CNPC_Leper::BeginNavJump( void )
{
	m_fIsNavJumping = true;
	m_fHitApex = false;
}

void CNPC_Leper::EndNavJump( void )
{
	m_fIsNavJumping = false;
	m_fHitApex = false;
}

//=========================================================
void CNPC_Leper::BeginAttackJump( void )
{
	// Set this to true. A little bit later if we fail to pathfind, we check
	// this value to see if we just jumped. If so, we assume we've jumped 
	// to someplace that's not pathing friendly, and so must jump again to get out.
	m_fJustJumped = true;

	m_flJumpStartAltitude = GetLocalOrigin().z;
}

//=========================================================
void CNPC_Leper::EndAttackJump( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Leper::BuildScheduleTestBits( void )
{
	// FIXME: This is probably the desired call to make, but it opts into an untested base class path, we'll need to
	//		  revisit this and figure out if we want that. -- jdw
	// BaseClass::BuildScheduleTestBits();
	//
	// For now, make sure our active behavior gets a chance to add its own bits
	if ( GetRunningBehavior() )
		GetRunningBehavior()->BridgeBuildScheduleTestBits(); 

	if ( !IsCurSchedule( SCHED_BURNING_RUN ) )
	{
		SetCustomInterruptCondition( COND_ON_FIRE );
	}

#ifdef HL2_EPISODIC
	SetCustomInterruptCondition( COND_PROVOKED );
#endif	// HL2_EPISODIC

	// Any schedule that makes us climb should break if we touch player
	if ( GetActivity() == ACT_CLIMB_UP || GetActivity() == ACT_CLIMB_DOWN || GetActivity() == ACT_CLIMB_DISMOUNT)
	{
		SetCustomInterruptCondition( COND_LEPER_CLIMB_TOUCH );
	}
	else
	{
		ClearCustomInterruptCondition( COND_LEPER_CLIMB_TOUCH );
	}
}

//=========================================================
// 
//=========================================================
void CNPC_Leper::OnStateChange( NPC_STATE OldState, NPC_STATE NewState )
{
	if( ( NewState == NPC_STATE_IDLE || NewState == NPC_STATE_ALERT ) ) ///!!!HACKHACK - sjb
	{
		// Set it up so that if the zombie goes into combat state sometime down the road
		// that he'll be able to scream.
		m_fHasScreamed = false;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Leper::Event_Killed( const CTakeDamageInfo &info )
{
	// Shut up my screaming sounds.
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "NPC_Leper.NoSound" );

	CTakeDamageInfo dInfo = info;

#if 0
	// Become a server-side ragdoll and create a constraint at the hand
	if ( m_PassengerBehavior.GetPassengerState() == PASSENGER_STATE_INSIDE )
	{
		IPhysicsObject *pVehiclePhys = m_PassengerBehavior.GetTargetVehicle()->GetServerVehicle()->GetVehicleEnt()->VPhysicsGetObject();
		CBaseAnimating *pVehicleAnimating = m_PassengerBehavior.GetTargetVehicle()->GetServerVehicle()->GetVehicleEnt()->GetBaseAnimating();
		int nRightHandBone = 31;//GetBaseAnimating()->LookupBone( "ValveBiped.Bip01_R_Finger2" );
		Vector vecRightHandPos;
		QAngle vecRightHandAngle;
		GetAttachment( LookupAttachment( "Blood_Right" ), vecRightHandPos, vecRightHandAngle );
		//CTakeDamageInfo dInfo( GetEnemy(), GetEnemy(), RandomVector( -200, 200 ), WorldSpaceCenter(), 50.0f, DMG_CRUSH );
		dInfo.SetDamageType( info.GetDamageType() | DMG_REMOVENORAGDOLL );
		dInfo.ScaleDamageForce( 10.0f );
		CBaseEntity *pRagdoll = CreateServerRagdoll( GetBaseAnimating(), 0, info, COLLISION_GROUP_DEBRIS );
		GetBaseAnimating()->GetBonePosition( nRightHandBone, vecRightHandPos, vecRightHandAngle );

		CBaseEntity *pRagdoll = CreateServerRagdollAttached(	GetBaseAnimating(), 
																vec3_origin, 
																-1, 
																COLLISION_GROUP_DEBRIS, 
																pVehiclePhys,
																pVehicleAnimating, 
																0, 
																vecRightHandPos,
																nRightHandBone,	
																vec3_origin );*/

	}
#endif

	BaseClass::Event_Killed( dInfo );
}

//=============================================================================
#ifdef HL2_EPISODIC

//-----------------------------------------------------------------------------
// Purpose: Add the passenger behavior to our repertoire
//-----------------------------------------------------------------------------
bool CNPC_Leper::CreateBehaviors( void )
{
#ifdef HL2_EPISODIC
	AddBehavior( &m_ActBusyBehavior );
	AddBehavior( &m_PassengerBehavior );
#endif // HL2_EPISODIC	

	return BaseClass::CreateBehaviors();
}

//-----------------------------------------------------------------------------
// Purpose: Get on the vehicle!
//-----------------------------------------------------------------------------
void CNPC_Leper::InputAttachToVehicle( inputdata_t &inputdata )
{
	// Interrupt us
	SetCondition( COND_PROVOKED );

	// Find the target vehicle
	CBaseEntity *pEntity = FindNamedEntity( inputdata.value.String() );
	CPropJeepEpisodic *pVehicle = dynamic_cast<CPropJeepEpisodic *>(pEntity);

	// Get in the car if it's valid
	if ( pVehicle && CanEnterVehicle( pVehicle ) )
	{
		// Set her into a "passenger" behavior
		m_PassengerBehavior.Enable( pVehicle );
		m_PassengerBehavior.AttachToVehicle();
	}

	RemoveSpawnFlags( SF_NPC_GAG );
}

//-----------------------------------------------------------------------------
// Purpose: Passed along from the vehicle's callback list
//-----------------------------------------------------------------------------
void CNPC_Leper::VPhysicsCollision( int index, gamevcollisionevent_t *pEvent )
{
	// Only do the override while riding on a vehicle
	if ( m_PassengerBehavior.CanSelectSchedule() && m_PassengerBehavior.GetPassengerState() != PASSENGER_STATE_OUTSIDE )
	{
		int damageType = 0;
		float flDamage = CalculatePhysicsImpactDamage( index, pEvent, gZombiePassengerImpactDamageTable, 1.0, true, damageType );

		if ( flDamage > 0  )
		{
			Vector damagePos;
			pEvent->pInternalData->GetContactPoint( damagePos );
			Vector damageForce = pEvent->postVelocity[index] * pEvent->pObjects[index]->GetMass();
			CTakeDamageInfo info( this, this, damageForce, damagePos, flDamage, (damageType|DMG_VEHICLE) );
			TakeDamage( info );
		}
		return;
	}

	BaseClass::VPhysicsCollision( index, pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: FIXME: Fold this into LeapAttack using different jump targets!
//-----------------------------------------------------------------------------
void CNPC_Leper::VehicleLeapAttack( void )
{
	CBaseEntity *pEnemy = GetEnemy();
	if ( pEnemy == NULL )
		return;

	Vector vecEnemyPos;
	UTIL_PredictedPosition( pEnemy, 1.0f, &vecEnemyPos );

	// Move
	SetGroundEntity( NULL );
	BeginAttackJump();
	LeapAttackSound();

	// Take him off ground so engine doesn't instantly reset FL_ONGROUND.
	UTIL_SetOrigin( this, GetLocalOrigin() + Vector( 0 , 0 , 1 ));

	// FIXME: This should be the exact position we'll enter at, but this approximates it generally
	//vecEnemyPos[2] += 16;

	Vector vecMins = GetHullMins();
	Vector vecMaxs = GetHullMaxs();
	Vector vecJumpDir = VecCheckToss( this, GetAbsOrigin(), vecEnemyPos, 0.1f, 1.0f, false, &vecMins, &vecMaxs );

	SetAbsVelocity( vecJumpDir );
	m_flNextAttack = gpGlobals->curtime + 2.0f;
	SetTouch( &CNPC_Leper::VehicleLeapAttackTouch );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Leper::CanEnterVehicle( CPropJeepEpisodic *pVehicle )
{
	if ( pVehicle == NULL )
		return false;

	return pVehicle->NPC_CanEnterVehicle( this, false );
}

//-----------------------------------------------------------------------------
// Purpose: FIXME: Move into behavior?
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CNPC_Leper::VehicleLeapAttackTouch( CBaseEntity *pOther )
{
	if ( pOther->GetServerVehicle() )
	{
		m_PassengerBehavior.AttachToVehicle();

		// HACK: Stop us cold
		SetLocalVelocity( vec3_origin );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Determine whether we're in a vehicle or not
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Leper::IsInAVehicle( void )
{
	// Must be active and getting in/out of vehicle
	if ( m_PassengerBehavior.IsEnabled() && m_PassengerBehavior.GetPassengerState() != PASSENGER_STATE_OUTSIDE )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Override our efficiency so that we don't jitter when we're in the middle
//			of our enter/exit animations.
// Input  : bInPVS - Whether we're in the PVS or not
//-----------------------------------------------------------------------------
void CNPC_Leper::UpdateEfficiency( bool bInPVS )
{ 
	// If we're transitioning and in the PVS, we override our efficiency
	if ( IsInAVehicle() && bInPVS )
	{
		PassengerState_e nState = m_PassengerBehavior.GetPassengerState();
		if ( nState == PASSENGER_STATE_ENTERING || nState == PASSENGER_STATE_EXITING )
		{
			SetEfficiency( AIE_NORMAL );
			return;
		}
	}

	// Do the default behavior
	BaseClass::UpdateEfficiency( bInPVS );
}

#endif	// HL2_EPISODIC
//=============================================================================

//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_leper, CNPC_Leper )

	DECLARE_ACTIVITY( ACT_LEPER_LEAP_SOAR )
	DECLARE_ACTIVITY( ACT_LEPER_LEAP_STRIKE )
	DECLARE_ACTIVITY( ACT_LEPER_LAND_RIGHT )
	DECLARE_ACTIVITY( ACT_LEPER_LAND_LEFT )
	DECLARE_ACTIVITY( ACT_LEPER_FRENZY )
	DECLARE_ACTIVITY( ACT_LEPER_BIG_SLASH )
	
	DECLARE_TASK( TASK_LEPER_DO_ATTACK )
	DECLARE_TASK( TASK_LEPER_LAND_RECOVER )
	DECLARE_TASK( TASK_LEPER_UNSTICK_JUMP )
	DECLARE_TASK( TASK_LEPER_JUMP_BACK )
	DECLARE_TASK( TASK_LEPER_VERIFY_ATTACK )

	DECLARE_CONDITION( COND_LEPER_CLIMB_TOUCH )

	//Adrian: events go here
	DECLARE_ANIMEVENT( AE_LEPER_LEAP )
	DECLARE_ANIMEVENT( AE_LEPER_GALLOP_LEFT )
	DECLARE_ANIMEVENT( AE_LEPER_GALLOP_RIGHT )
	DECLARE_ANIMEVENT( AE_LEPER_CLIMB_LEFT )
	DECLARE_ANIMEVENT( AE_LEPER_CLIMB_RIGHT )

#ifdef HL2_EPISODIC
	// FIXME: Move!
	DECLARE_ANIMEVENT( AE_PASSENGER_PHYSICS_PUSH )
	DECLARE_ANIMEVENT( AE_LEPER_VEHICLE_LEAP )
	DECLARE_ANIMEVENT( AE_LEPER_VEHICLE_SS_DIE )
#endif	// HL2_EPISODIC


	//=========================================================
	// I have landed somewhere that's pathfinding-unfriendly
	// just try to jump out.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_LEPER_UNSTICK_JUMP,

		"	Tasks"
		"		TASK_LEPER_UNSTICK_JUMP	0"
		"	"
		"	Interrupts"
	);

	DEFINE_SCHEDULE
	(
		SCHED_LEPER_CLIMBING_UNSTICK_JUMP,

		"	Tasks"
		"		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"
		"		TASK_LEPER_UNSTICK_JUMP	0"
		"	"
		"	Interrupts"
	);

	//=========================================================
	// > Range_Attack1
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_LEPER_RANGE_ATTACK1,

		"	Tasks"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_RANGE_ATTACK1"
		"		TASK_SET_ACTIVITY				ACTIVITY:ACT_LEPER_LEAP_STRIKE"
		"		TASK_RANGE_ATTACK1				0"
		"		TASK_WAIT						0.1"
		"		TASK_LEPER_LAND_RECOVER	0" // essentially just figure out which way to turn.
		"		TASK_FACE_ENEMY					0"
		"	"
		"	Interrupts"
	);

	//=========================================================
	// > Melee_Attack1
	//=========================================================
	//!!! Shouldnt "frenzy" be a seperate sched???
	DEFINE_SCHEDULE
	(
		SCHED_LEPER_MELEE_ATTACK1,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_FACE_ENEMY					0"
		"		TASK_MELEE_ATTACK1				0"
		"		TASK_MELEE_ATTACK1				0"
		"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_LEPER_FRENZY"

		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_ENEMY_OCCLUDED"
		"		COND_HEAVY_DAMAGE"
	);

	DEFINE_SCHEDULE
	(
		SCHED_LEPER_FRENZY,

		"	Tasks"
		"		TASK_FACE_ENEMY					0"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_LEPER_FRENZY"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY"
		"		TASK_LEPER_VERIFY_ATTACK		0"
		"		TASK_PLAY_SEQUENCE_FACE_ENEMY	ACTIVITY:ACT_LEPER_BIG_SLASH"

		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_ENEMY_OCCLUDED"
	);

	//=========================================================
	// > ChaseEnemy
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_LEPER_CHASE_ENEMY,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY_FAILED"
	//	"		TASK_SET_TOLERANCE_DISTANCE		24"
		"		TASK_GET_CHASE_PATH_TO_ENEMY	300"
		"		TASK_RUN_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_FACE_ENEMY					0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_ENEMY_UNREACHABLE"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_TOO_CLOSE_TO_ATTACK"
		"		COND_TASK_FAILED"
		"		COND_LOST_ENEMY"
		"		COND_BETTER_WEAPON_AVAILABLE"
		"		COND_HEAR_DANGER"
		"		COND_HEAVY_DAMAGE"
	);

AI_END_CUSTOM_NPC()


