//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Humanoid Talking NPCS
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef TALKNPC_H
#define TALKNPC_H

#ifdef _LINUX
#undef time
#include <time.h>
#endif

#ifndef _XBOX
#pragma warning(push)
#include <set>
#pragma warning(pop)
#endif

#ifdef _WIN32
#pragma once
#endif

#ifdef _LINUX
// the include <set> monkey's with the max() define, unbreak it
#undef MINMAX_H
#include "minmax.h"
#endif

#include "ai_basetalker.h"

#include "soundflags.h"

#include "ai_task.h"
#include "ai_schedule.h"
#include "ai_default.h"
#include "ai_speech.h"
#include "ai_basenpc.h"
#include "ai_behavior.h"
#include "ai_behavior_follow.h"

#include "tier0/memdbgon.h"

//=========================================================
// Talking NPC base class
// Used for scientists and barneys
//=========================================================

#define SF_NPC_PREDISASTER			( 1 << 16 )	// This is a predisaster scientist or barney. Influences how they speak.
#define TLK_CFRIENDS		4
#define MAX_TIME_BETWEEN_BARRELS_EXPLODING			5.0f
#define MAX_TIME_BETWEEN_CONSECUTIVE_PLAYER_KILLS	3.0f

//=============================================================================
// >> CNPC_SimpleTalker
//=============================================================================

#define MONOLOGNAME_LEN	16				// sentence names passed as monolog may be no longer than this.
#define AI_SP_START_MONOLOG 	'~'
#define AI_SP_MONOLOG_LINE  	'@'

class CNPC_SimpleTalker;

class CNPC_SimpleTalkerExpresser : public CAI_ComponentWithOuter<CNPC_SimpleTalker, CAI_Expresser>
{
public:
	CNPC_SimpleTalkerExpresser( CNPC_SimpleTalker *pOuter )
		: CAI_ComponentWithOuter<CNPC_SimpleTalker, CAI_Expresser>( pOuter )
	{
		EndMonolog();
	}

	virtual int SpeakRawSentence( const char *pszSentence, float delay, float volume = VOL_NORM, soundlevel_t soundlevel = SNDLVL_TALKING, CBaseEntity *pListener = NULL );

	// --------------------------------
	//
	// Monologue operations
	//
	
	bool HasMonolog( void ) { return m_iMonologIndex != -1; };
	void BeginMonolog( char *pszSentenceName, CBaseEntity *pListener );
	void EndMonolog( void );
	void SpeakMonolog( void );

	void SuspendMonolog( float flInterval );
	void ResumeMonolog( void );

	CBaseEntity *GetMonologueTarget()						{ return m_hMonologTalkTarget.Get(); }

	// --------------------------------
	//
	// Monologue data
	//
	char		m_szMonologSentence[MONOLOGNAME_LEN];	// The name of the sentence group for the monolog I'm speaking.
	int			m_iMonologIndex;						// Which sentence from the group I should be speaking.
	bool		m_fMonologSuspended;
	EHANDLE		m_hMonologTalkTarget;					// Who I'm trying to deliver my monolog to. 

	DECLARE_SIMPLE_DATADESC();
};

//-------------------------------------

class CNPC_SimpleTalker : public CAI_BaseTalker
{
	DECLARE_CLASS( CNPC_SimpleTalker, CAI_BaseTalker );
public:
	void			Precache( void );
	virtual bool	KeyValue( const char *szKeyName, const char *szValue );

	virtual CAI_Expresser *CreateExpresser() { return new CNPC_SimpleTalkerExpresser(this); }
	
	virtual void			StartFollowing( CBaseEntity *pLeader );
	virtual void			StopFollowing( void );
	CBaseEntity		*GetFollowTarget( void ) { return m_FollowBehavior.GetFollowTarget(); }

	virtual void	OnChangeRunningBehavior( CAI_BehaviorBase *pOldBehavior,  CAI_BehaviorBase *pNewBehavior );
	bool			OnBehaviorChangeStatus( CAI_BehaviorBase *pBehavior, bool fCanFinishSchedule );

	int				PlayScriptedSentence( const char *pszSentence, float delay, float volume, soundlevel_t soundlevel, bool bConcurrent, CBaseEntity *pListener );
	virtual void 	FollowerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual bool	SelectPlayerUseSpeech( CBaseEntity *pPlayer = NULL );
	void			Event_Killed( const CTakeDamageInfo &info );
	virtual void	OnPlayerKilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info );
	int				OnTakeDamage_Alive( const CTakeDamageInfo &info );

	bool CreateBehaviors()
	{
		AddBehavior( &m_FollowBehavior );
		return BaseClass::CreateBehaviors();
	}

	void		BuildScheduleTestBits( void );
	void			PrescheduleThink( void );
	virtual int		SelectSchedule( void );
	virtual int		SelectNonCombatSpeechSchedule();
	void			StartTask( const Task_t *pTask );
	void			RunTask( const Task_t *pTask );
	void			HandleAnimEvent( animevent_t *pEvent );
	Activity	NPC_TranslateActivity( Activity newActivity );

	virtual void	OnStartingFollow( CBaseEntity *pTarget );
	virtual void	OnStoppingFollow( CBaseEntity *pTarget );

	virtual void	DeferAllIdleSpeech( float flDelay, CAI_BaseNPC *pIgnore = NULL );
	virtual void	IdleHeadTurn( CBaseEntity *pTarget, float flDuration = 0.0, float flImportance = 1.0f );

#ifdef HL1_DLL
	bool		ShouldSpeakRandom( int iChance, float flModifier );
#endif

	// For following
	virtual void	DeclineFollowing( void ) {}
	void			LimitFollowers( CBaseEntity *pPlayer, int maxFollowers );

	float			GetUseTime() const { return m_useTime; }

	//=========================================================
	// TalkNPC schedules
	//=========================================================
	enum
	{
		SCHED_TALKER_IDLE_RESPONSE = BaseClass::NEXT_SCHEDULE,
		SCHED_TALKER_IDLE_SPEAK,
		SCHED_TALKER_IDLE_HELLO,
		SCHED_TALKER_IDLE_WATCH_CLIENT,
		SCHED_TALKER_IDLE_WATCH_CLIENT_STARE,
		SCHED_TALKER_IDLE_EYE_CONTACT,
		SCHED_TALKER_STOP_SHOOTING,
		SCHED_TALKER_BETRAYED,
		SCHED_TALKER_PROVOKED,

		// !ALWAYS LAST!
		NEXT_SCHEDULE,	
	};

	//=========================================================
	// TalkNPC tasks
	//=========================================================
	enum
	{
		TASK_TALKER_RESPOND = BaseClass::NEXT_TASK,		// say my response
		TASK_TALKER_SPEAK,			// question or remark
		TASK_TALKER_HELLO,			// Try to say hello to player
		TASK_TALKER_BETRAYED,		// Player killed an ally
		TASK_TALKER_PROVOKED,		// Player killed multiple allies
		TASK_TALKER_HEADRESET,		// reset head position
		TASK_TALKER_STOPSHOOTING,	// tell player to stop shooting friend
		TASK_TALKER_STARE,			// let the player know I know he's staring at me.
		TASK_TALKER_LOOK_AT_CLIENT,// faces player if not moving and not talking and in idle.
		TASK_TALKER_CLIENT_STARE,	// same as look at client, but says something if the player stares.
		TASK_TALKER_EYECONTACT,	// maintain eyecontact with person who I'm talking to
		TASK_TALKER_IDEALYAW,		// set ideal yaw to face who I'm talking to
		TASK_FIND_LOCK_HINTNODE_HEALTH, // Find & lock a nearby healthkit hintnode to heal myself at
		TASK_TALKER_WAIT_FOR_SEMAPHORE,

		// !ALWAYS LAST!
		NEXT_TASK,		
	};

//private:
	virtual bool IsValidSpeechTarget( int flags, CBaseEntity *pEntity );
	
	CBaseEntity		*FindNearestFriend(bool fPlayer);

	void SayHelloToPlayer( CBaseEntity *pPlayer );
	virtual bool CanSayHello( void );
	virtual int	 FIdleHello( void );

	// Inputs
	void InputIdleRespond( inputdata_t &inputdata );

	// Conversations / communication
	void			IdleRespond( void );
	int				FIdleSpeak( void );
	void			FIdleSpeakWhileMoving( void );
	int				FIdleStare( void );
	bool			SpeakQuestionFriend( CBaseEntity *pFriend );
	bool			SpeakAnswerFriend( CBaseEntity *pFriend );
	void			TrySmellTalk( void );

	virtual void	SetAnswerQuestion( CNPC_SimpleTalker *pSpeaker );
	
	bool ShouldSuspendMonolog( void );
	bool ShouldResumeMonolog( void );
	void OnResumeMonolog() 		{	Speak( TLK_RESUME ); }
	
	int			m_nSpeak;						// number of times initiated talking

	static char *m_szFriends[TLK_CFRIENDS];		// array of friend names
	CBaseEntity		*EnumFriends( CBaseEntity *pentPrevious, int listNumber, bool bTrace );

	virtual int		FriendNumber( int arrayNumber )	{ return arrayNumber; }
	void			ShutUpFriends( void );
	void			AlertFriends( CBaseEntity *pKiller );

	string_t	m_iszUse;						// Custom +USE sentence group (follow)
	string_t	m_iszUnUse;						// Custom +USE sentence group (stop following)

protected:
	CAI_FollowBehavior m_FollowBehavior;
	float		m_useTime;						// Don't allow +USE until this time
	float	m_fLastBarrelExploded;
	float	m_fLastPlayerKill;
	int		m_iNumConsecutiveBarrelsExploded;  // Companions keep track of the # of consecutive barrels exploded by the player and speaks a response as it increases
	int		m_iNumConsecutivePlayerKills;  // Alyx keeps track of the # of consecutive kills by the player and speaks a response as it increases

	//---------------------------------

	DECLARE_DATADESC();
#ifndef _XBOX
	DEFINE_CUSTOM_AI;
#else
public:
	DEFINE_CUSTOM_AI;
private:
#endif
};

#include "tier0/memdbgoff.h"

#endif		//TALKNPC_H
