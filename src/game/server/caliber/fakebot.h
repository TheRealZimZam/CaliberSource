//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Fake bot - A npc that runs around and mimics a player
// Used in SP if we ever need a player-like character, or in MP as a stopgap
// Cannot cap points or do much of anything a player can do, only for deathmatch.
// Inherits from simpletalker, for response rules.
//
// NOTENOTE; If you really want to, you CAN use this as a baseclass, but if you're
// doing that you SHOULD just use the real bot system (if its implemented)...
//
//=============================================================================//

#ifndef FAKEBOT_H
#define FAKEBOT_H
#pragma once

#include "npc_talker.h"

// We normally dont collide with friendly players and phase through like in MP,
// In-case we want to though, for some odd reason
#define	SF_FAKEBOT_SOLID					(1 << 16) 

#define FAKEBOT_DEFAULT_MODEL "models/player.mdl"


class CFakeBot : public CNPC_SimpleTalker
{
	DECLARE_CLASS( CFakeBot, CNPC_SimpleTalker );
public:
	void	Precache( void );
	void	Spawn( void );
//	virtual CBaseEntity* Respawn( void );

	Class_T Classify( void );
	Disposition_t	IRelationType( CBaseEntity *pTarget );
	bool	ShouldGib( const CTakeDamageInfo &info );
	bool	ShouldMoveAndShoot();

	// Speed/Movement
	float	GetIdealAccel( void ) const{ return GetIdealSpeed() * 2.0; };
	float	MaxYawSpeed( void );

	// Hearing
	int		GetSoundInterests( void );
	float	HearingSensitivity( void ) { return 1.0; };
	void	PainSound( const CTakeDamageInfo &info );
	void	DeathSound( const CTakeDamageInfo &info );
	// Never go back to idle after we get in a fight
	virtual bool	ShouldGoToIdleState( void ) { return false; }

	//void	HandleAnimEvent( animevent_t *pEvent );

	// Thinking
	void	NPCThink( void );
	void	PrescheduleThink( void );
	void	GatherConditions( void );
#ifdef HL2_DLL
	void	OnKilledNPC( CBaseCombatCharacter *pKilled );
#endif

	// Doing
	void		BuildScheduleTestBits( void );
	virtual int	SelectSchedule( void );
	virtual int	TranslateSchedule( int scheduleType );

	// See if we should respawn
	void	Event_Killed( const CTakeDamageInfo &info );

private:
//	int		m_iFakeTeam;
	int		m_iActiveSlot;
	int		m_iWantsSlot;

	bool	m_bPissed;	//Have i been dominated by a specific enemy, or has he taken longer than 30 secs to kill?
	CHandle<CBaseCombatCharacter>	m_pPissedTarget;	// Handle of the bastid' im pissed at

private:
	// Conds
	enum
	{
		COND_FAKEBOT_HELP_TEAMMATE = BaseClass::NEXT_CONDITION,
		NEXT_CONDITION,
	};

	// Scheds
	enum
	{
		SCHED_FAKEBOT_IDLE = BaseClass::NEXT_SCHEDULE,
		SCHED_FAKEBOT_TBAG,
		SCHED_FAKEBOT_RETREAT,
		SCHED_FAKEBOT_ATTACK,
		SCHED_FAKEBOT_CHASE,
		SCHED_FAKEBOT_HELP_TEAMMATE,
		NEXT_SCHEDULE,
	};

protected:
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
};


#endif // FAKEBOT_H
