//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:	Doggo - man's best friend, player's biggest enemy.
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef NPC_HOUND_H
#define NPC_HOUND_H
#pragma once


#include	"AI_BaseNPC.h"

#include	"energy_wave.h"

class CNPC_Hound : public CAI_BaseNPC
{
	DECLARE_CLASS( CNPC_Hound, CAI_BaseNPC );

	DEFINE_CUSTOM_AI;
	
	DECLARE_DATADESC();

public:
	void			Spawn( void );
	void			Precache( void );
	Class_T			Classify ( void );
	void			HandleAnimEvent( animevent_t *pEvent );
	float			MaxYawSpeed ( void );
	void			WarmUpSound ( void );
	void			AlertSound( void );
	void			DeathSound( const CTakeDamageInfo &info );
	void			WarnSound( void );
	void			PainSound( const CTakeDamageInfo &info );
	void			IdleSound( void );
	void			StartTask( const Task_t *pTask );
	void			RunTask( const Task_t *pTask );
	int				GetSoundInterests( void );
	void			SonicAttack( void );
	void			PrescheduleThink( void );
	void			WriteBeamColor ( void );
	int				RangeAttack1Conditions ( float flDot, float flDist );
	bool			FCanActiveIdle ( void );
	virtual int		TranslateSchedule( int scheduleType );
	Activity		NPC_TranslateActivity( Activity eNewActivity );
	virtual int		SelectSchedule( void );
	bool			HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt);
	void			NPCThink(void);
	int				OnTakeDamage_Alive( const CTakeDamageInfo &info );
	void			Event_Killed( const CTakeDamageInfo &info );
	bool			IsAnyoneInSquadAttacking( void );
	void			SpeakSentence( int sentenceType );

private:
	float			m_flNextSecondaryAttack;
	bool			m_bLoopClockwise;

	CEnergyWave*	m_pEnergyWave;
	float			m_flEndEnergyWaveTime;

	bool			m_fAsleep;// some houndeyes sleep in idle mode if this is set, the houndeye is lying down
	bool			m_fDontBlink;// don't try to open/close eye if this bit is set!

private:
	//=========================================================
	// Specialized Tasks 
	//=========================================================
	enum 
	{
		TASK_HOUND_THREAT_DISPLAY = LAST_SHARED_TASK,
		TASK_HOUND_FALL_ASLEEP,
		TASK_HOUND_WAKE_UP,
		TASK_HOUND_HOP_BACK,

		TASK_HOUND_GET_PATH_TO_CIRCLE,
		TASK_HOUND_REVERSE_STRAFE_DIR,
	};

	//-----------------------------------------------------------------------------
	// Custom Conditions
	//-----------------------------------------------------------------------------
	enum Hound_Conds
	{
		COND_HOUND_GROUP_ATTACK = LAST_SHARED_CONDITION,
		COND_HOUND_GROUP_RETREAT,
		COND_HOUND_GROUP_RALLEY,
	};

	//=========================================================
	// Specialized Shedules
	//=========================================================
	enum
	{
		SCHED_HOUND_AGITATED = LAST_SHARED_SCHEDULE,
		SCHED_HOUND_HOP_RETREAT,
		SCHED_HOUND_RANGE_ATTACK1,

		SCHED_HOUND_ATTACK_STRAFE,
		SCHED_HOUND_ATTACK_STRAFE_REVERSE,
		SCHED_HOUND_GROUP_ATTACK,
		SCHED_HOUND_GROUP_RETREAT,
		SCHED_HOUND_CHASE_ENEMY,
		SCHED_HOUND_COVER_WAIT,
		SCHED_HOUND_GROUP_RALLEY,
	};

};


#endif // NPC_HOUND_H
