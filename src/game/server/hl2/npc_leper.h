//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose:
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================

#ifndef	NPC_LEPER_H
#define	NPC_LEPER_H
#ifdef _WIN32
#pragma once
#endif

#include	"ai_basenpc.h"

#define LEPER_MINLEAP				200
#define LEPER_MAXLEAP				300
#define LEPER_MAXLEAP_Z				128

#define LEPER_MELEE_REACH			60

// Pass these to claw attack so we know where to draw the blood.
#define LEPER_BLOOD_LEFT_HAND		0
#define LEPER_BLOOD_RIGHT_HAND		1
#define LEPER_BLOOD_BOTH_HANDS		2
#define LEPER_BLOOD_BITE			3

//=========================================================
//	>> CNPC_Leper
//=========================================================
class CNPC_Leper : public CAI_BaseNPC
{
	DECLARE_CLASS( CNPC_Leper, CAI_BaseNPC );

public:
	void Precache( void );
	void Spawn( void );

	int	TranslateSchedule( int scheduleType );

	Activity NPC_TranslateActivity( Activity eNewActivity );

	void LeapAttackTouch( CBaseEntity *pOther );
	void ClimbTouch( CBaseEntity *pOther );

	void StartTask( const Task_t *pTask );
	void RunTask( const Task_t *pTask );
	int SelectSchedule( void );
	void OnScheduleChange( void );

	void PrescheduleThink( void );

	float InnateRange1MaxRange( void ) { return LEPER_MAXLEAP; }
	float InnateMelee1Range( void ) { return LEPER_MELEE_REACH; }
	int RangeAttack1Conditions( float flDot, float flDist );
	int MeleeAttack1Conditions( float flDot, float flDist );

	void HandleAnimEvent( animevent_t *pEvent );

	virtual CBaseEntity *ClawAttack( float flDist, int iDamage, QAngle &qaViewPunch, Vector &vecVelocityPunch, int BloodOrigin );
	void LeapAttack( void );
	void LeapAttackSound( void );

	bool IsJumpLegal(const Vector &startPos, const Vector &apex, const Vector &endPos) const;
	bool MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost );
	bool ShouldFailNav( bool bMovementFailed );

	int	SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );

	void OnChangeActivity( Activity NewActivity );
	void OnStateChange( NPC_STATE OldState, NPC_STATE NewState );
	void Event_Killed( const CTakeDamageInfo &info );
	virtual	bool AllowedToIgnite( void ) { return true; }

	virtual Vector GetAutoAimCenter() { return WorldSpaceCenter() - Vector( 0, 0, 12.0f ); }

	void PainSound( const CTakeDamageInfo &info );
	void DeathSound( const CTakeDamageInfo &info ); 
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );
	void AttackHitSound( void );
	void AttackMissSound( void );

	void BuildScheduleTestBits( void );

	void BeginNavJump( void );
	void EndNavJump( void );

	bool IsNavJumping( void ) { return m_fIsNavJumping; }
	void OnNavJumpHitApex( void );

	void BeginAttackJump( void );
	void EndAttackJump( void );

	float		MaxYawSpeed( void );

private:
	// Sound stuff
	float			m_flDistFactor; 
	unsigned char	m_iClimbCount; // counts rungs climbed (for sound)
	bool			m_fIsNavJumping;
	bool			m_fIsAttackJumping;
	bool			m_fHitApex;
	mutable float	m_flJumpDist;
	bool			m_fHasScreamed;

private:
	//=========================================================
	// schedules
	//=========================================================
	enum
	{
		SCHED_LEPER_RANGE_ATTACK1 = BaseClass::NEXT_SCHEDULE,
		SCHED_LEPER_MELEE_ATTACK1,
		SCHED_LEPER_CHASE_ENEMY,
		SCHED_LEPER_STALK_ENEMY,
		SCHED_LEPER_AVOID_ENEMY,
		SCHED_LEPER_RUN_FROM_ENEMY,
		SCHED_LEPER_FRENZY,
		SCHED_LEPER_INVESTIGATE,
		SCHED_LEPER_VICTORY_DANCE,
	};

	//=========================================================
	// tasks
	//=========================================================
	enum 
	{
		TASK_LEPER_DO_ATTACK = BaseClass::NEXT_TASK,
		TASK_LEPER_LAND_RECOVER,
		TASK_LEPER_UNSTICK_JUMP,
		TASK_LEPER_JUMP_BACK,
		TASK_LEPER_VERIFY_ATTACK,
	};

	//=========================================================
	// conditions
	//=========================================================
	enum
	{
		COND_LEPER_CLIMB_TOUCH	= BaseClass::NEXT_CONDITION,
	};

private:
	float	m_flNextMeleeAttack;
	bool	m_fJustJumped;
	float	m_flJumpStartAltitude;

public:
	DEFINE_CUSTOM_AI;
	DECLARE_DATADESC();
};

#endif	//LEPER_H
