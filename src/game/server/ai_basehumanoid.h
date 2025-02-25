//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Humanoid baseclass - this doesnt contain any proper ai, all that still
// needs to be defined in the subclass - this is for utility functions and very basic
// states applicable to ALL human npcs like falling/jumping, heavydamage, sneakattacks, etc.
//
// TODO; Limb gibbing
//=============================================================================//

#ifndef AI_BASEHUMANOID_H
#define AI_BASEHUMANOID_H

#include "ai_basenpc.h"
#include "ai_behavior.h"
#include "ai_blended_movement.h"

//-----------------------------------------------------------------------------
// CLASS: CAI_BaseHumanoid
//-----------------------------------------------------------------------------

typedef CAI_BlendingHost< CAI_BehaviorHost<CAI_BaseNPC> > CAI_BaseHumanoidBase;

// MOVED TO BASENPC
#if 0
//=========================================================
// tasks
//=========================================================
enum 
{
	TASK_FIND_DODGE_POSITION = LAST_SHARED_TASK,
	TASK_DODGE,
	NEXT_TASK
};

//=========================================================
// schedules
//=========================================================
enum
{
	SCHED_DODGE = LAST_SHARED_SCHEDULE,
	NEXT_SCHEDULE,
};
#endif

class CAI_BaseHumanoid : public CAI_BaseHumanoidBase
{
	DECLARE_CLASS( CAI_BaseHumanoid, CAI_BaseHumanoidBase );

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

public:
	void Precache( void );
	void Spawn( void );

	bool HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt);
	virtual void HandleAnimEvent( animevent_t *pEvent );

	// Tasks
	virtual void StartTask( const Task_t *pTask );
	virtual void RunTask( const Task_t *pTask );
	virtual void BuildScheduleTestBits( );

	// Navigation
	virtual float GetIdealAccel();
	virtual float GetIdealSpeed();
	bool OnMoveBlocked( AIMoveResult_t *pResult );
	virtual bool ShouldMoveAndShoot( void );
	virtual bool ShouldInvestigateSound() { return (CapabilitiesGet() & bits_CAP_HEAR) != 0; }

	// Damage
	virtual bool ShouldPickADeathPose( void );
	virtual bool ShouldGib( const CTakeDamageInfo &info );
	virtual void Ignite( float flFlameLifetime, bool bNPCOnly = true, float flSize = 0.0f, bool bCalledByLevelDesigner = false );
	void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	void Event_Killed( const CTakeDamageInfo &info );
	int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	bool IsKnockedDown() { return m_bKnockedDown; }
	bool IsInjured() { return ((float)GetHealth() / (float)GetMaxHealth() <= 0.25f); }
	
	Activity NPC_TranslateActivity( Activity NewActivity );
	virtual int	TranslateSchedule( int scheduleType );

	// Various start tasks
	virtual	void StartTaskRangeAttack1( const Task_t *pTask );

	// Various run tasks
	virtual void RunTaskRangeAttack1( const Task_t *pTask );

	// Purpose: check ammo
	virtual void CheckAmmo( void );
	virtual bool CanReload( void );
	virtual void CalculateClips( void );

public:
	int			m_iClips;			// How many clips i have in reserve
	bool		m_bKnockedDown;		// Has enough damage been done in one attack in order to knock me down?
	string_t	m_spawnEquipmentSecondary;	// Secondary equipment, usually a melee weapon

private:
	CNetworkVar(bool, m_bHeadGibbed );	//Has my head been liquified by a shotgun blast?
//	CNetworkVar(int, m_iLimbGibbed1 );	//1st limb gibbed
//	CNetworkVar(int, m_iLimbGibbed2 );	//2 limbs gibbed - max for now

};

//-----------------------------------------------------------------------------

#endif
