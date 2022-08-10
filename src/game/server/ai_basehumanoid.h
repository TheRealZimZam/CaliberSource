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

#include "ai_behavior.h"
#include "ai_blended_movement.h"

//-----------------------------------------------------------------------------
// CLASS: CAI_BaseHumanoid
//-----------------------------------------------------------------------------

typedef CAI_BlendingHost< CAI_BehaviorHost<CAI_BaseNPC> > CAI_BaseHumanoidBase;

class CAI_BaseHumanoid : public CAI_BaseHumanoidBase
{
	DECLARE_CLASS( CAI_BaseHumanoid, CAI_BaseHumanoidBase );

public:
	bool HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt);

	// Tasks
	virtual void StartTask( const Task_t *pTask );
	virtual void RunTask( const Task_t *pTask );
	virtual void BuildScheduleTestBits( );

	// Navigation
	bool OnMoveBlocked( AIMoveResult_t *pResult );
	virtual bool ShouldMoveAndShoot( void );

	// Damage
	virtual bool ShouldPickADeathPose( void );
	virtual bool ShouldGib( const CTakeDamageInfo &info );
	virtual void Ignite( float flFlameLifetime, bool bNPCOnly = true, float flSize = 0.0f, bool bCalledByLevelDesigner = false );
	void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	void Event_Killed( const CTakeDamageInfo &info );
	int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	bool IsKnockedDown() { return m_bKnockedDown; }

	// Various start tasks
	virtual	void StartTaskRangeAttack1( const Task_t *pTask );

	// Various run tasks
	virtual void RunTaskRangeAttack1( const Task_t *pTask );

	// Purpose: check ammo
	virtual void CheckAmmo( void );

private:
	bool		m_bKnockedDown;		//Has enough damage been done in one attack in order to knock me down?

};

//-----------------------------------------------------------------------------

#endif
