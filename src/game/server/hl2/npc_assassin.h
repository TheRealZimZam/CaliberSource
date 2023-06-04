//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Jumpy, Shooty thing
//
// $NoKeywords: $
//=============================================================================//

#ifndef NPC_ASSASSIN_H
#define NPC_ASSASSIN_H
#ifdef _WIN32
#pragma once
#endif

#include "ai_basenpc.h"
#include "ai_basehumanoid.h"
#include "ai_behavior.h"
#include "ai_behavior_assault.h"
#include "ai_behavior_follow.h"
#include "ai_behavior_rappel.h"
#include "ai_sentence.h"
#include "ai_baseactor.h"
#include "sprite.h"
#include "spritetrail.h"

//Eye states
enum eyeState_t
{
	ASSASSIN_EYE_SEE_TARGET = 0,		//Sees the target, bright and big
	ASSASSIN_EYE_SEEKING_TARGET,	//Looking for a target, blinking (bright)
	ASSASSIN_EYE_ACTIVE,			//Actively looking
	ASSASSIN_EYE_DORMANT,			//Not active
	ASSASSIN_EYE_DEAD,				//Completely invisible
};

//=========================================================
//=========================================================
class CNPC_Assassin : public CAI_BaseActor
{
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
	DECLARE_CLASS( CNPC_Assassin, CAI_BaseActor );
	// DECLARE_SERVERCLASS();

public:
	CNPC_Assassin( void );
	
	Class_T		Classify( void )			{ return CLASS_STALKER;	}
	int			GetSoundInterests( void )	{ return (SOUND_WORLD|SOUND_COMBAT|SOUND_PLAYER|SOUND_DANGER|SOUND_BULLET_IMPACT|SOUND_MOVE_AWAY|SOUND_VEHICLE|SOUND_WEAPON);	}
	virtual float	GetJumpGravity() const		{ return 1.8f; }
	
	void		Precache( void );
	void		Spawn( void );

	int			SelectSchedule( void );
	int			MeleeAttack1Conditions( float flDot, float flDist );
	int			RangeAttack1Conditions( float flDot, float flDist );
	int			RangeAttack2Conditions( float flDot, float flDist );

	void		PrescheduleThink( void );
	void		HandleAnimEvent( animevent_t *pEvent );
	void		StartTask( const Task_t *pTask );
	void		RunTask( const Task_t *pTask );
	void		OnScheduleChange( void );
//	void		GatherEnemyConditions( CBaseEntity *pEnemy );
	void		BuildScheduleTestBits( void );
	void		Event_Killed( const CTakeDamageInfo &info );

	bool		FValidateHintType ( CAI_Hint *pHint );
	bool		IsJumpLegal(const Vector &startPos, const Vector &apex, const Vector &endPos) const;
	bool		MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost );
	bool		IsFrustrated() { return (m_iFrustration > 5); }

	float		MaxYawSpeed( void );

	const Vector &GetViewOffset( void );

private:
	//=========================================================
	// Flip types
	//=========================================================
	enum 
	{
		FLIP_LEFT,
		FLIP_RIGHT,
		FLIP_FORWARD,
		FLIP_BACKWARD,
		NUM_FLIP_TYPES,
	};

	//=========================================================
	// Private conditions
	//=========================================================
/*
	enum Assassin_Conds
	{
		COND_ASSASSIN_ENEMY_TARGETTING_ME = LAST_SHARED_CONDITION,
	};
*/

	//=========================================================
	// Assassin schedules
	//=========================================================
	enum
	{
		SCHED_ASSASSIN_FIND_VANTAGE_POINT = LAST_SHARED_SCHEDULE,
		SCHED_ASSASSIN_EVADE,
		SCHED_ASSASSIN_STALK_ENEMY,
		SCHED_ASSASSIN_LUNGE,
		SCHED_ASSASSIN_SMOKE,
	};

	//=========================================================
	// Assassin tasks
	//=========================================================
	enum 
	{
		TASK_ASSASSIN_GET_PATH_TO_VANTAGE_POINT = LAST_SHARED_TASK,
		TASK_ASSASSIN_EVADE,
		TASK_ASSASSIN_SMOKE,
		TASK_ASSASSIN_SET_EYE_STATE,
		TASK_ASSASSIN_LUNGE,
	};

private:
	void		SetEyeState( eyeState_t state );
	void		FirePistol( int hand );
	bool		CanFlip( int flipType, Activity &activity, const Vector *avoidPosition );

	int			m_nNumFlips;
	int			m_nLastFlipType;
	int			m_iFrustration;
	float		m_flNextFlipTime;	//Next earliest time the assassin can flip again
	float		m_flNextLungeTime;
	float		m_flNextShotTime;
	float		m_flNextSmokeTime;

//	bool		m_bAggressive;		// Start throwing grenades/Focus on melee
	bool		m_bBlinkState;

	CSprite				*m_pEyeSprite;
	CSpriteTrail		*m_pEyeTrail;

};


#endif // NPC_ASSASSIN_H
