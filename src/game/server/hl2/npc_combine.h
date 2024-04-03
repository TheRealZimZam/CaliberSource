//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:  Your classic malicious-military-marauders.
// TODO's: Auto-squadding, COND_COMBINE_DROP_GRENADE for tripmines, looping radio sound while talking, integrate with metropolice somehow??
//=============================================================================//

#ifndef NPC_COMBINE_H
#define NPC_COMBINE_H
#ifdef _WIN32
#pragma once
#endif

#include "ai_basenpc.h"
#include "ai_basehumanoid.h"
#include "ai_behavior.h"
#include "ai_behavior_assault.h"
#include "ai_behavior_standoff.h"
#include "ai_behavior_follow.h"
#include "ai_behavior_functank.h"
#include "ai_behavior_rappel.h"
#include "ai_behavior_actbusy.h"
#include "ai_sentence.h"
#include "ai_baseactor.h"

// Used when only what combine to react to what the spotlight sees
#define SF_COMBINE_AUTOSQUAD ( 1 << 15 )
#define SF_COMBINE_NO_LOOK	(1 << 16)
#define SF_COMBINE_NO_GRENADEDROP ( 1 << 17 )
#define SF_COMBINE_NO_AR2DROP ( 1 << 18 )

//=========================================================
//	>> CNPC_Combine
//=========================================================
enum SoldierType_t
{
	REGULAR,
	DEMOLITIONS,
	SHOCK,
	FLAMETHROWER,
	ELITE,
};

class CNPC_Combine : public CAI_BaseActor
{
	DECLARE_CLASS( CNPC_Combine, CAI_BaseActor );

public:
	CNPC_Combine();

	// Create components
	virtual bool	CreateComponents();

	void			Spawn( void );
	void			Precache( void );
	void			Activate();
	bool			KeyValue( const char *szKeyName, const char *szValue );

	int				GetGrenadeConditions( float flDot, float flDist, const Vector &vecTarget );
	int				CheckCanThrowGrenade( const Vector &vecTarget );
	int				RangeAttack2Conditions( float flDot, float flDist ); // For innate grenade attack
	int				MeleeAttack1Conditions( float flDot, float flDist ); // For kick/punch

	int				CanAltFireEnemy( float flDot, float flDist );	// For weapon altfire

	bool			FVisible( CBaseEntity *pEntity, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = NULL );
	virtual bool	IsCurTaskContinuousMove();

	virtual float	GetJumpGravity() const		{ return 1.8f; }

	virtual Vector  GetCrouchEyeOffset( void );

	void Event_Killed( const CTakeDamageInfo &info );
	int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );

	void SetActivity( Activity NewActivity );
	NPC_STATE		SelectIdealState( void );
	void			OnStateChange( NPC_STATE OldState, NPC_STATE NewState );

	// Input handlers.
	void InputLookOn( inputdata_t &inputdata );
	void InputLookOff( inputdata_t &inputdata );
	void InputStartPatrolling( inputdata_t &inputdata );
	void InputStopPatrolling( inputdata_t &inputdata );
	void InputAssault( inputdata_t &inputdata );
	void InputHitByBugbait( inputdata_t &inputdata );
	void InputThrowGrenadeAtTarget( inputdata_t &inputdata );
//!	void InputThrowTeleportGrenadeAtTarget( inputdata_t &inputdata );

	bool			UpdateEnemyMemory( CBaseEntity *pEnemy, const Vector &position, CBaseEntity *pInformer = NULL );

	Class_T			Classify( void );
	bool			IsDemolition() { return m_sType == DEMOLITIONS; }
	bool			IsShock() { return m_sType == SHOCK; }
	bool			IsFlamethrower() { return m_sType == FLAMETHROWER; }
	bool			IsElite() { return m_sType == ELITE; }
	void			DelayAltFireAttack( float flDelay );
	void			DelaySquadAltFireAttack( float flDelay );
	float			MaxYawSpeed( void );
	bool			ShouldMoveAndShoot();
	void			OnUpdateShotRegulator();
	bool			OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval );;
	void			HandleAnimEvent( animevent_t *pEvent );
	Vector			Weapon_ShootPosition( );

	Vector			EyeOffset( Activity nActivity );
	Vector			EyePosition( void );
	Vector			BodyTarget( const Vector &posSrc, bool bNoisy = true );
	Vector			GetAltFireTarget();

	void			StartTask( const Task_t *pTask );
	void			RunTask( const Task_t *pTask );
	void			GatherConditions();
	virtual void	PrescheduleThink();

	Activity		NPC_TranslateActivity( Activity eNewActivity );
	void			BuildScheduleTestBits( void );
	virtual int		SelectSchedule( void );
	int				SelectScheduleAttack();
	int				SelectCombatSchedule();

	bool			CreateBehaviors();

	bool			OnBeginMoveAndShoot();
	void			OnEndMoveAndShoot();

	// Combat
	WeaponProficiency_t CalcWeaponProficiency( CBaseCombatWeapon *pWeapon );
	bool			HasShotgun();
	bool			ActiveWeaponIsFullyLoaded();
	bool			CanDoSignal();
	void			DetatchHelmet();

	bool			HandleInteraction(int interactionType, void *data, CBaseCombatCharacter *sourceEnt);
	const char*		GetSquadSlotDebugName( int iSquadSlot );

	bool			IsUsingTacticalVariant( int variant );
	bool			IsUsingPathfindingVariant( int variant ) { return m_iPathfindingVariant == variant; }

	bool			IsRunningApproachEnemySchedule();

	// -------------
	// Sounds
	// -------------
	void			DeathSound( const CTakeDamageInfo &info );
	void			PainSound( const CTakeDamageInfo &info );
	void			IdleSound( void );
	void			AlertSound( void );
	void			LostEnemySound( void );
	void			FoundEnemySound( void );
	void			AnnounceAssault( void );
	void			AnnounceEnemyType( CBaseEntity *pEnemy );
	void			AnnounceEnemyKill( CBaseEntity *pEnemy );

	void			NotifyDeadFriend( CBaseEntity* pFriend );

	virtual float	HearingSensitivity( void ) { return 1.0; };
	int				GetSoundInterests( void );
	virtual bool	QueryHearSound( CSound *pSound );

	// Speaking
	//virtual char	*GetSentencePrefix( const char *pszSoundName );
	void			SpeakSentence( int sentType );

	virtual int		TranslateSchedule( int scheduleType );

protected:
	// Time Variables
	float			m_flNextPainSoundTime;
	float			m_flNextAlertSoundTime;
	float			m_flNextGrenadeCheck;	
	float			m_flNextLostSoundTime;
	float			m_flAlertPatrolTime;		// When to stop doing alert patrol
	float			m_flNextSignalTime;			// Clock until a signal can be used
	float			m_flNextAltFireTime;		// Elites only. Next time to begin considering alt-fire attack.

	int				m_iNewEnemies;				// When this hits three, new enemy ai is deactivated until combat is over to prevent oscillation.
	int				m_nShots;
	float			m_flShotDelay;
	float			m_flStopMoveShootTime;

	void			SetKickDamage( int nDamage ) { m_nKickDamage = nDamage; }
	CAI_Sentence< CNPC_Combine > *GetSentences() { return &m_Sentences; }

private:
	//=========================================================
	// Combine schedules
	//=========================================================
	enum
	{
		SCHED_COMBINE_SUPPRESS = BaseClass::NEXT_SCHEDULE,
		SCHED_COMBINE_COMBAT_FAIL,
		SCHED_COMBINE_VICTORY_DANCE,
		SCHED_COMBINE_COMBAT_FACE,
		SCHED_COMBINE_COVER_AND_RELOAD,
		SCHED_COMBINE_HIDE_AND_RELOAD,
		SCHED_COMBINE_SIGNAL_SUPPRESS,
		SCHED_COMBINE_ASSAULT,
//		SCHED_COMBINE_GRENADE_ASSAULT,
		SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE,
		SCHED_COMBINE_FOUND_ENEMY,
		SCHED_COMBINE_PRESS_ATTACK,
		SCHED_COMBINE_SIGNAL_PRESS_ATTACK,
		SCHED_COMBINE_WAIT_IN_COVER,
		SCHED_COMBINE_RANGE_ATTACK1,
		SCHED_COMBINE_MELEE_ATTACK1,
		SCHED_COMBINE_RANGE_ATTACK2,
		SCHED_COMBINE_TAKE_COVER1,
		SCHED_COMBINE_TAKE_COVER2,
		SCHED_COMBINE_TAKE_COVER_FROM_BEST_SOUND,
		SCHED_COMBINE_RUN_AWAY_FROM_BEST_SOUND,
		SCHED_COMBINE_SIGNAL_TAKE_COVER,
		SCHED_COMBINE_GRENADE_COVER1,
		SCHED_COMBINE_TAKECOVER_FAILED,
		SCHED_COMBINE_GRENADE_AND_RELOAD,
		SCHED_COMBINE_PATROL,
		SCHED_COMBINE_BUGBAIT_DISTRACTION,
		SCHED_COMBINE_CHARGE_TURRET,
		SCHED_COMBINE_DROP_GRENADE,
		SCHED_COMBINE_DEFUSE,
		SCHED_COMBINE_CHARGE_PLAYER,
		SCHED_COMBINE_PATROL_ENEMY,
		SCHED_COMBINE_BURNING_STAND,
//		SCHED_COMBINE_EVADE,
		SCHED_COMBINE_AR2_ALTFIRE,
//		SCHED_COMBINE_SMG1_ALTFIRE,
		SCHED_COMBINE_FORCED_GRENADE_THROW,
		SCHED_COMBINE_MOVE_TO_FORCED_GREN_LOS,
		NEXT_SCHEDULE,
	};

	//=========================================================
	// Combine Tasks
	//=========================================================
	enum 
	{
		TASK_COMBINE_FACE_TOSS_DIR = BaseClass::NEXT_TASK,
		TASK_COMBINE_IGNORE_ATTACKS,
		TASK_COMBINE_SIGNAL_BEST_SOUND,
		TASK_COMBINE_DEFER_SQUAD_GRENADES,
		TASK_COMBINE_CHASE_ENEMY_CONTINUOUSLY,
		TASK_COMBINE_DIE_INSTANTLY,
		TASK_COMBINE_PLAY_SEQUENCE_FACE_ALTFIRE_TARGET,
		TASK_COMBINE_GET_PATH_TO_FORCED_GREN_LOS,
		TASK_COMBINE_SET_STANDING,
//		TASK_COMBINE_MOVE_AND_SHOOT,
//		TASK_COMBINE_MOVE_AND_AIM,
//!		TASK_WAIT_FOR_TELEPORT,
//!		TASK_ADVISOR_LEVITATE,
		NEXT_TASK
	};

	//=========================================================
	// Combine Conditions
	//=========================================================
	enum Combine_Conds
	{
		COND_COMBINE_NO_FIRE = BaseClass::NEXT_CONDITION,
		COND_COMBINE_SHOULD_PATROL,
		COND_COMBINE_HIT_BY_BUGBAIT,
		COND_COMBINE_DROP_GRENADE,
		COND_COMBINE_ON_FIRE,
		COND_COMBINE_ATTACK_SLOT_AVAILABLE,
		NEXT_CONDITION
	};

public:
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;

private:
	// Should we charge the player?
	bool ShouldChargePlayer();

	// Chase the enemy, updating the target position as the player moves
	void StartTaskChaseEnemyContinuously( const Task_t *pTask );
	void RunTaskChaseEnemyContinuously( const Task_t *pTask );

	class CCombineStandoffBehavior : public CAI_ComponentWithOuter<CNPC_Combine, CAI_StandoffBehavior>
	{
		typedef CAI_ComponentWithOuter<CNPC_Combine, CAI_StandoffBehavior> BaseClass;

		virtual int SelectScheduleAttack()
		{
			int result = GetOuter()->SelectScheduleAttack();
			if ( result == SCHED_NONE )
				result = BaseClass::SelectScheduleAttack();
			return result;
		}
	};

	// Rappel
	virtual bool IsWaitingToRappel( void ) { return m_RappelBehavior.IsWaitingToRappel(); }
	void BeginRappel() { m_RappelBehavior.BeginRappel(); }
/*
	void OnTossedTeleportProjectile( CBaseEntity *pProjectile );
	void OnTeleportProjectileAction( CBaseEntity *pProjectile, bool bTeleport );

	void FullyLoadWeaponClips();
*/

	int				m_nKickDamage;
	int				m_voicePitch;
	int				m_lastGrenadeCondition;
	Vector			m_vecTossVelocity;
	EHANDLE			m_hForcedGrenadeTarget;
	bool			m_bShouldAutosquad;	//If not in a squad, should i try to join one?
	bool			m_bShouldPatrol;
	bool			m_bHelmet;	// Is the helmet still on after a headshot?

//	float			m_flDistToEnemy = ( GetEnemy()->GetAbsOrigin() - GetAbsOrigin() ).Length();

	CAI_Sentence< CNPC_Combine > m_Sentences;

	int			m_iNumGrenades;
	CAI_AssaultBehavior			m_AssaultBehavior;
	CCombineStandoffBehavior	m_StandoffBehavior;
	CAI_FollowBehavior			m_FollowBehavior;
	CAI_FuncTankBehavior		m_FuncTankBehavior;
	CAI_RappelBehavior			m_RappelBehavior;
	CAI_ActBusyBehavior			m_ActBusyBehavior;

public:
	int				m_iLastAnimEventHandled;
	SoldierType_t	m_sType;
	Vector			m_vecAltFireTarget;

	int				m_iTacticalVariant;
	int				m_iPathfindingVariant;
};


#endif // NPC_COMBINE_H
