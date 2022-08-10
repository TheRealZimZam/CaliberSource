//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "ai_behavior_holster.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_DATADESC( CAI_HolsterBehavior )
	DEFINE_FIELD( m_bWeaponOut, FIELD_BOOLEAN ),
END_DATADESC();

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_HolsterBehavior::CAI_HolsterBehavior()
{
	// m_AssaultCue = CUE_NO_ASSAULT;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTask - 
//-----------------------------------------------------------------------------
void CAI_HolsterBehavior::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_HOLSTER_WEAPON:
		{
			if ( !GetOuter()->GetActiveWeapon() )
			{
				TaskFail( "No active weapon to holster" );
				return;
			}

			// For some odd reason we are already holstered, or cant do holster animations
			if ( GetOuter()->IsWeaponHolstered() || !GetOuter()->CanHolsterWeapon() )
			{
				m_bWeaponOut = false;
				TaskComplete();
			}
			else
			{
				GetOuter()->SetDesiredWeaponState( DESIREDWEAPONSTATE_HOLSTERED );
			}
			break;
		}
	case TASK_DRAW_WEAPON:
		{
			if ( !GetOuter()->GetActiveWeapon() )
			{
				TaskFail( "No active weapon to draw" );
				return;
			}

			// For some odd reason we are already drew, or cant do holster animations
			if ( !GetOuter()->IsWeaponHolstered() || !GetOuter()->CanHolsterWeapon() )
			{
				m_bWeaponOut = true;
				TaskComplete();
			}
			else
			{
				GetOuter()->SetDesiredWeaponState( DESIREDWEAPONSTATE_UNHOLSTERED );
			}
			break;
		}
	default:
		BaseClass::StartTask( pTask );
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTask - 
//-----------------------------------------------------------------------------
void CAI_HolsterBehavior::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_HOLSTER_WEAPON:
		{
			if ( !GetOuter()->IsWeaponStateChanging() )
			{
				m_bWeaponOut = false;
				TaskComplete();
			}
			break;
		}
	case TASK_DRAW_WEAPON:
		{
			if ( !GetOuter()->IsWeaponStateChanging() )
			{
				m_bWeaponOut = true;
				TaskComplete();
			}
			break;
		}
	default:
		BaseClass::RunTask( pTask );
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAI_HolsterBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
		return false;

	if ( GetOuter()->HasCondition( COND_RECEIVED_ORDERS ) )
		return false;

	if ( GetOuter()->IsInAScript() )
		return false;

	if ( GetEnemy() )
	{
		// make sure weapon is out when fighting
		if (!m_bWeaponOut)
		{
			return true;
		}
	}
	else if ( GetOuter()->GetState() == NPC_STATE_IDLE && m_bWeaponOut )
	{
		// if i should be going back to idle, put it away
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CAI_HolsterBehavior::SelectSchedule()
{
	if (!m_bWeaponOut)
		return SCHED_DRAW_WEAPON;
	else
		return SCHED_HOLSTER_WEAPON;
}


AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_HolsterBehavior )

	DECLARE_TASK( TASK_HOLSTER_WEAPON )
	DECLARE_TASK( TASK_DRAW_WEAPON )

	//=========================================================
	//=========================================================
	DEFINE_SCHEDULE 
	(
		SCHED_HOLSTER_WEAPON,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_HOLSTER_WEAPON				0"
		"	"
		"	Interrupts"
	)

	DEFINE_SCHEDULE 
	(
		SCHED_DRAW_WEAPON,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_DRAW_WEAPON				0"
		"	"
		"	Interrupts"
	)

AI_END_CUSTOM_SCHEDULE_PROVIDER()
