//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Humanoid Talking NPCS
//
// NOTENOTE; Alot of stuff in here is #ifdef'd because said things are handled in
// the baseclass (ai_basetalker). If you're compiling HL1, it will automagically
// plug everything back in, resulting in a mostly faithful recreation of hl1's talker
// system. Everything that isnt ifdef'd is new stuff from 2002.
//
//=============================================================================//
#include "cbase.h"

#include "npc_talker.h"
#include "basemultiplayerplayer.h"
#include "npcevent.h"
#include "scriptevent.h"
#include "props.h"
#include "ai_memory.h"
#include "ai_senses.h"
#include "ai_squad.h"
#ifdef HL2_EPISODIC
#include "grenade_frag.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_SIMPLE_DATADESC( CNPC_SimpleTalkerExpresser )
	//									m_pSink		(reconnected on load)
	DEFINE_AUTO_ARRAY(	m_szMonologSentence,	FIELD_CHARACTER	),
	DEFINE_FIELD(		m_iMonologIndex,		FIELD_INTEGER	),
	DEFINE_FIELD(		m_fMonologSuspended,	FIELD_BOOLEAN	),
	DEFINE_FIELD(		m_hMonologTalkTarget,	FIELD_EHANDLE	),
END_DATADESC()

BEGIN_DATADESC( CNPC_SimpleTalker )
	DEFINE_FIELD( m_useTime, FIELD_TIME ),
	DEFINE_FIELD( m_nSpeak, FIELD_INTEGER ),
	DEFINE_FIELD( m_iszUse, FIELD_STRING ),
	DEFINE_FIELD( m_iszUnUse, FIELD_STRING ),
	DEFINE_FIELD( m_fLastBarrelExploded, FIELD_TIME ),
	DEFINE_FIELD( m_iNumConsecutiveBarrelsExploded, FIELD_INTEGER ),
	DEFINE_FIELD( m_fLastPlayerKill, FIELD_TIME ),
	DEFINE_FIELD( m_iNumConsecutivePlayerKills, FIELD_INTEGER ),
	// 							m_FollowBehavior (auto saved by AI)
	// Function Pointers
	DEFINE_USEFUNC( FollowerUse ),

END_DATADESC()

// array of friend names
char *CNPC_SimpleTalker::m_szFriends[TLK_CFRIENDS] = 
{
	"monster_barney",
	"monster_scientist",
	"monster_sitting_scientist",
	NULL,
};

bool CNPC_SimpleTalker::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "UseSentence"))
	{
		m_iszUse = AllocPooledString(szValue);
	}
	else if (FStrEq(szKeyName, "UnUseSentence"))
	{
		m_iszUnUse = AllocPooledString(szValue);
	}
	else 
		return BaseClass::KeyValue( szKeyName, szValue );

	return true;
}

void CNPC_SimpleTalker::Precache( void )
{
	// FIXME:  Need to figure out how to hook these...
/*
	if ( m_iszUse != NULL_STRING )
		GetExpresser()->ModifyConcept( TLK_STARTFOLLOW, STRING( m_iszUse ) );
	if ( m_iszUnUse != NULL_STRING )
		GetExpresser()->ModifyConcept( TLK_STOPFOLLOW, STRING( m_iszUnUse ) );
*/

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_SimpleTalker::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();

	// Assume that if I move from the player, I can respond to a question
	if ( ConditionInterruptsCurSchedule( COND_PLAYER_PUSHING ) || ConditionInterruptsCurSchedule( COND_PROVOKED ) )
	{
		SetCustomInterruptCondition( COND_TALKER_RESPOND_TO_QUESTION );
	}
}

void CNPC_SimpleTalker::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();
	
	(assert_cast<CNPC_SimpleTalkerExpresser *>(GetExpresser()))->SpeakMonolog();
}

bool CNPC_SimpleTalker::ShouldSuspendMonolog( void )
{
	float flDist;

	flDist = ((assert_cast<CNPC_SimpleTalkerExpresser *>(GetExpresser()))->GetMonologueTarget()->GetAbsOrigin() - GetAbsOrigin()).Length();
	
	if( flDist >= 384 )
	{
		return true;
	}

	return false;
}

bool CNPC_SimpleTalker::ShouldResumeMonolog( void )
{
	float flDist;

	if( HasCondition( COND_SEE_PLAYER ) )
	{
		flDist = ((assert_cast<CNPC_SimpleTalkerExpresser *>(GetExpresser()))->GetMonologueTarget()->GetAbsOrigin() - GetAbsOrigin()).Length();
		
		if( flDist <= 256 )
		{
			return true;
		}
	}

	return false;
}

int CNPC_SimpleTalker::SelectSchedule( void )
{
	if ( !HasCondition(COND_RECEIVED_ORDERS) )
	{
		if ( GetState() == NPC_STATE_IDLE )
		{
			// if never seen player, try to greet him
			// Filter might be preventing us from ever greeting the player
			if ( HasCondition( COND_SEE_PLAYER ) && CanSayHello())
			{
				return SCHED_TALKER_IDLE_HELLO;
			}

			TrySmellTalk();
		}
	}

	return BaseClass::SelectSchedule();
}

void CNPC_SimpleTalker::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_TALKER_WAIT_FOR_SEMAPHORE:
		if ( GetExpresser()->SemaphoreIsAvailable( this ) )
			TaskComplete();
		break;

	case TASK_TALKER_SPEAK:
		// ask question or make statement
		FIdleSpeak();
		TaskComplete();
		break;

	case TASK_TALKER_RESPOND:
		// respond to question
		IdleRespond();
		TaskComplete();
		break;

	case TASK_TALKER_HELLO:
		// greet player
		FIdleHello();
		TaskComplete();
		break;
	
	case TASK_TALKER_STARE:
		// let the player know I know he's staring at me.
		FIdleStare();
		TaskComplete();
		break;

	case TASK_TALKER_LOOK_AT_CLIENT:
	case TASK_TALKER_CLIENT_STARE:
		// track head to the client for a while.
		SetWait( pTask->flTaskData );
		break;

	case TASK_TALKER_EYECONTACT:
		break;

	case TASK_TALKER_IDEALYAW:
		if (GetSpeechTarget() != NULL)
		{
			GetMotor()->SetIdealYawToTarget( GetSpeechTarget()->GetAbsOrigin() );
		}
		TaskComplete();
		break;

	case TASK_TALKER_HEADRESET:
		// reset head position after looking at something
		SetSpeechTarget( NULL );
		TaskComplete();
		break;

	case TASK_TALKER_BETRAYED:
		if (IsOkToSpeak(SPEECH_PRIORITY, true))
			Speak( TLK_BETRAYED );

		// Player is sussy
		if ( HasMemory( bits_MEMORY_SUSPICIOUS ) )
			Remember( bits_MEMORY_PROVOKED );
		else
			Remember( bits_MEMORY_SUSPICIOUS );

		TaskComplete();
		break;

	case TASK_TALKER_PROVOKED:
		if (IsOkToSpeak(SPEECH_PRIORITY, true))
			Speak( TLK_PROVOKED );

		Remember( bits_MEMORY_PROVOKED );

		TaskComplete();
		break;

	case TASK_TALKER_STOPSHOOTING:
		// tell player to stop shooting
		if(IsOkToSpeak(SPEECH_PRIORITY))
			Speak( TLK_NOSHOOT );

		TaskComplete();
		break;
	default:
		BaseClass::StartTask( pTask );
	}
}

void CNPC_SimpleTalker::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_TALKER_WAIT_FOR_SEMAPHORE:
		if ( GetExpresser()->SemaphoreIsAvailable( this ) )
			TaskComplete();
		break;

	case TASK_TALKER_CLIENT_STARE:
	case TASK_TALKER_LOOK_AT_CLIENT:
		// track head to the client for a while.
		if ( AI_IsSinglePlayer() )
		{
			// Get edict for one player
			CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
			Assert( pPlayer );

			if ( m_NPCState == NPC_STATE_IDLE && !IsMoving() && !GetExpresser()->IsSpeaking() )
			{
				if ( pPlayer )
					IdleHeadTurn( pPlayer );
			}
			else
			{
				// started moving or talking
				TaskFail( "Moved away" );
				return;
			}

			if ( pTask->iTask == TASK_TALKER_CLIENT_STARE )
			{
				// fail out if the player looks away or moves away.
				if ( ( pPlayer->GetAbsOrigin() - GetAbsOrigin() ).Length2D() > TALKER_STARE_DIST )
				{
					// player moved away.
					TaskFail("Player moved away");
				}

				Vector forward;
				AngleVectors( pPlayer->GetLocalAngles(), &forward );
				if ( UTIL_DotPoints( pPlayer->GetAbsOrigin(), GetAbsOrigin(), forward ) < m_flFieldOfView )
				{
					// player looked away
					TaskFail("Player looked away");
				}
			}
		}

		if ( IsWaitFinished() )
		{
			TaskComplete();
		}
		break;

	case TASK_TALKER_EYECONTACT:
		if (GetExpresser()->IsSpeaking() && GetSpeechTarget() != NULL)
		{
			// ALERT( at_console, "waiting %f\n", m_flStopTalkTime - gpGlobals->time );
			IdleHeadTurn( GetSpeechTarget(), GetExpresser()->GetTimeSpeechComplete() - gpGlobals->curtime );
		}

		if (IsMoving() || !GetExpresser()->IsSpeaking() || GetSpeechTarget() == NULL)
		{
			TaskComplete();
		}
		break;

	case TASK_WAIT_FOR_MOVEMENT:
		FIdleSpeakWhileMoving();
		BaseClass::RunTask( pTask );
		break;

	default:
		BaseClass::RunTask( pTask );
	}
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
Activity CNPC_SimpleTalker::NPC_TranslateActivity( Activity eNewActivity )
{
#if 0
	if ((eNewActivity == ACT_IDLE) && (GetExpresser()->IsSpeaking()) && 
		(SelectWeightedSequence ( ACT_SIGNAL2 ) != ACTIVITY_NOT_AVAILABLE) )
		return ACT_SIGNAL2;
#endif

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}


void CNPC_SimpleTalker::Event_Killed( const CTakeDamageInfo &info )
{
#ifdef HL1_DLL
	AlertFriends( info.GetAttacker() );
	if ( info.GetAttacker()->GetFlags() & FL_CLIENT )
	{
		LimitFollowers( info.GetAttacker(), 0 );
	}
#endif
	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
// Purpose: Player in our squad killed something
// Input  : *pVictim - Who he killed
//			&info - How they died
//-----------------------------------------------------------------------------
void CNPC_SimpleTalker::OnPlayerKilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info )
{
	// filter everything that comes in here that isn't an NPC
	CAI_BaseNPC *pCombatVictim = dynamic_cast<CAI_BaseNPC *>( pVictim );
	if ( !pCombatVictim )
		return;

	// don't comment on kills when we can't see the victim
	if ( !FVisible( pVictim ) )
		return;

	// Only do the expensive playerkill logic if its against someone or thing i dont like
	if ( IRelationType( pCombatVictim ) == D_LI )
	{
		// Squadmates scold the player first, then go hostile the second time around
		if ( (IsInPlayerSquad() || FindNearestFriend(true)) )
		{
			SetSchedule( SCHED_TALKER_BETRAYED );
		}
		else
		{
#ifdef ALLIES_CAN_BE_PROVOKED
			// Non-squadmates get instantly provoked IF the victim is in the same squad
			if ( m_pSquad && m_pSquad->SquadIsMember( pCombatVictim ))
				SetSchedule( SCHED_TALKER_PROVOKED );	//Go straight to provoked
			else
#endif
			{
				// TODO; Gotta see if the player is justified... self-defence situation, etc.etc. -MM
				SetSchedule( SCHED_TALKER_BETRAYED );
			}
		}
	}
	else
	{
		if ( IRelationType( pCombatVictim ) != D_HT )
			return;	// Neutral or ERROR

		CBaseEntity *pInflictor = info.GetInflictor();
		int		iNumBarrels = 0;
		int		iConsecutivePlayerKills = 0;
		bool	bPuntedGrenade = false;
		bool	bVictimWasEnemy = false;
		bool	bVictimWasMob = false;
		bool	bVictimWasAttacker = false;
		bool	bHeadshot = false;
		bool	bOneShot = false;

		if ( dynamic_cast<CBreakableProp *>( pInflictor ) && ( info.GetDamageType() & DMG_BLAST ) )
		{
			// if a barrel explodes that was initiated by the player within a few seconds of the previous one,
			// increment a counter to keep track of how many have exploded in a row.
			if ( gpGlobals->curtime - m_fLastBarrelExploded >= MAX_TIME_BETWEEN_BARRELS_EXPLODING )
			{
				m_iNumConsecutiveBarrelsExploded = 0;
			}
			m_iNumConsecutiveBarrelsExploded++;
			m_fLastBarrelExploded = gpGlobals->curtime;

			iNumBarrels = m_iNumConsecutiveBarrelsExploded;
		}
		else
		{
			// if player kills an NPC within a few seconds of the previous kill,
			// increment a counter to keep track of how many he's killed in a row.
			if ( gpGlobals->curtime - m_fLastPlayerKill >= MAX_TIME_BETWEEN_CONSECUTIVE_PLAYER_KILLS )
			{
				m_iNumConsecutivePlayerKills = 0;
			}
			m_iNumConsecutivePlayerKills++;
			m_fLastPlayerKill = gpGlobals->curtime;
			iConsecutivePlayerKills = m_iNumConsecutivePlayerKills;
		}

		// check if the player killed an enemy by punting a grenade
#if HL2_EPISODIC
		if ( pInflictor && Fraggrenade_WasPunted( pInflictor ) && Fraggrenade_WasCreatedByCombine( pInflictor ) )
			bPuntedGrenade = true;
#endif
		// check if the victim was my enemy
		if ( GetEnemy() == pVictim )
			bVictimWasEnemy = true;

		AI_EnemyInfo_t *pEMemory = GetEnemies()->Find( pVictim );
		if ( pEMemory != NULL ) 
		{
			// was Alyx being mobbed by this enemy?
			bVictimWasMob = pEMemory->bMobbedMe;

			// has Alyx recieved damage from this enemy?
			if ( pEMemory->timeLastReceivedDamageFrom > 0 )
				bVictimWasAttacker = true;
		}

		// Was it a headshot?
		if ( ( pCombatVictim->LastHitGroup() == HITGROUP_HEAD ) && ( info.GetDamageType() & DMG_BULLET ) )
			bHeadshot = true;

		// Did the player kill the enemy with 1 shot?
		if ( ( pCombatVictim->GetDamageCount() == 1 ) && ( info.GetDamageType() & DMG_BULLET ) )
			bOneShot = true;

		// set up the speech modifiers
		CFmtStrN<512> modifiers( "num_barrels:%d,distancetoplayerenemy:%f,playerammo:%s,consecutive_player_kills:%d,"
			"punted_grenade:%d,victim_was_enemy:%d,victim_was_mob:%d,victim_was_attacker:%d,headshot:%d,oneshot:%d",
			iNumBarrels, EnemyDistance( pVictim ), info.GetAmmoName(), iConsecutivePlayerKills,
			bPuntedGrenade, bVictimWasEnemy, bVictimWasMob, bVictimWasAttacker, bHeadshot, bOneShot );

		SpeakIfAllowed( TLK_PLAYER_KILLED_NPC, modifiers );
	}

	BaseClass::OnPlayerKilledOther( pVictim, info );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity	*CNPC_SimpleTalker::EnumFriends( CBaseEntity *pPrevious, int listNumber, bool bTrace )
{
	CBaseEntity *pFriend = pPrevious;
	char *pszFriend;
	trace_t tr;
	Vector vecCheck;

	pszFriend = m_szFriends[ FriendNumber(listNumber) ];
	while ( pszFriend != NULL && ((pFriend = gEntList.FindEntityByClassname( pFriend, pszFriend )) != NULL) )
	{
		if (pFriend == this || !pFriend->IsAlive())
			// don't talk to self or dead people
			continue;

		if ( bTrace )
		{
			Vector vecCheck;
			pFriend->CollisionProp()->NormalizedToWorldSpace( Vector( 0.5f, 0.5f, 1.0f ), &vecCheck );
			UTIL_TraceLine( GetAbsOrigin(), vecCheck, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);
		}
		else
		{
			tr.fraction = 1.0;
		}

		if (tr.fraction == 1.0)
		{
			return pFriend;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pKiller - 
//-----------------------------------------------------------------------------
void CNPC_SimpleTalker::AlertFriends( CBaseEntity *pKiller )
{
	CBaseEntity *pFriend = NULL;
	int i;

	// for each friend in this bsp...
	for ( i = 0; i < TLK_CFRIENDS; i++ )
	{
		while ((pFriend = EnumFriends( pFriend, i, true )) != NULL )
		{
			CAI_BaseNPC *pNPC = pFriend->MyNPCPointer();
			if ( pNPC->IsAlive() )
			{
				// If a client killed me, make everyone else mad/afraid of him
				if ( pKiller->GetFlags() & FL_CLIENT )
				{
					CNPC_SimpleTalker*pTalkNPC = (CNPC_SimpleTalker *)pFriend;
					pTalkNPC->SetSchedule( SCHED_TALKER_BETRAYED );
				}
				else
				{
					if( IRelationType(pKiller) == D_HT)
					{
						// Killed by an enemy!!!
						CNPC_SimpleTalker *pAlly = (CNPC_SimpleTalker *)pNPC;
						
						if( pAlly && pAlly->GetExpresser()->CanSpeakConcept( TLK_ALLY_KILLED ) )
						{
							pAlly->SpeakIfAllowed( TLK_ALLY_KILLED );
						}
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_SimpleTalker::ShutUpFriends( void )
{
	CBaseEntity *pFriend = NULL;
	int i;

	// for each friend in this bsp...
	for ( i = 0; i < TLK_CFRIENDS; i++ )
	{
		while ((pFriend = EnumFriends( pFriend, i, true )) != NULL)
		{
			CAI_BaseNPC *pNPC = pFriend->MyNPCPointer();
			if ( pNPC )
			{
				pNPC->SentenceStop();
			}
		}
	}
}


// UNDONE: Keep a follow time in each follower, make a list of followers in this function and do LRU
// UNDONE: Check this in Restore to keep restored NPCs from joining a full list of followers
void CNPC_SimpleTalker::LimitFollowers( CBaseEntity *pPlayer, int maxFollowers )
{
	CBaseEntity *pFriend = NULL;
	int i, count;

	count = 0;
	// for each friend in this bsp...
	for ( i = 0; i < TLK_CFRIENDS; i++ )
	{
		while ((pFriend = EnumFriends( pFriend, i, false )) != NULL)
		{
			CAI_BaseNPC *pNPC = pFriend->MyNPCPointer();
			CNPC_SimpleTalker *pTalker;
			if ( pNPC )
			{
				if ( pNPC->GetTarget() == pPlayer )
				{
					count++;
					if ( count > maxFollowers && (pTalker = dynamic_cast<CNPC_SimpleTalker *>( pNPC ) ) != NULL )
						pTalker->StopFollowing();
				}
			}
		}
	}
}

//=========================================================
// HandleAnimEvent - catches the NPC-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CNPC_SimpleTalker::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{		
	case SCRIPT_EVENT_SENTENCE_RND1:		// Play a named sentence group 25% of the time
		if (random->RandomInt(0,99) < 75)
			break;
		// fall through...
	case SCRIPT_EVENT_SENTENCE:				// Play a named sentence group
		ShutUpFriends();
		PlaySentence( pEvent->options, random->RandomFloat(2.8, 3.4) );
		//Msg( "script event speak\n");
		break;

	default:
		BaseClass::HandleAnimEvent( pEvent );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Scan for nearest, visible friend. If fPlayer is true, look for nearest player
//-----------------------------------------------------------------------------
bool CNPC_SimpleTalker::IsValidSpeechTarget( int flags, CBaseEntity *pEntity )
{
	return BaseClass::IsValidSpeechTarget( flags, pEntity );
}

CBaseEntity *CNPC_SimpleTalker::FindNearestFriend(bool fPlayer)
{
	return FindSpeechTarget( (fPlayer) ? AIST_PLAYERS : AIST_NPCS );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Purpose: Respond to a previous question
//-----------------------------------------------------------------------------
void CNPC_SimpleTalker::IdleRespond( void )
{
	if (!IsOkToSpeak())
		return;

	// play response
	SpeakAnswerFriend( GetSpeechTarget() );

	DeferAllIdleSpeech( random->RandomFloat( TALKER_DEFER_IDLE_SPEAK_MIN, TALKER_DEFER_IDLE_SPEAK_MAX ) );
}

//-----------------------------------------------------------------------------
// Purpose: Find a nearby friend to stare at
//-----------------------------------------------------------------------------
int CNPC_SimpleTalker::FIdleStare( void )
{
	// Don't idly speak if our speech filter is preventing us
	if ( GetSpeechFilter() && GetSpeechFilter()->GetIdleModifier() == 0 )
		return true;

	SpeakIfAllowed( TLK_STARE );

	SetSpeechTarget( FindNearestFriend( true ) );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Try to greet player first time he's seen
// Output : int
//-----------------------------------------------------------------------------
int CNPC_SimpleTalker::FIdleHello( void )
{
	// Filter might be preventing us from ever greeting the player
	if ( !CanSayHello() )
		return false;

	// get a player
	CBaseEntity *pPlayer = FindNearestFriend(true);

	if (pPlayer)
	{
		if (FInViewCone(pPlayer) && FVisible(pPlayer))
		{
			SayHelloToPlayer( pPlayer );
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Say hello to the specified player
//-----------------------------------------------------------------------------
void CNPC_SimpleTalker::SayHelloToPlayer( CBaseEntity *pPlayer )
{
	Assert( !GetExpresser()->SpokeConcept(TLK_HELLO) );

	SetSpeechTarget( pPlayer );

	SpeakIfAllowed( TLK_HELLO );
	DeferAllIdleSpeech( random->RandomFloat( 5, 10 ) );

	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	CAI_BaseTalker *pTalker;
	for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
	{
		pTalker = dynamic_cast<CAI_BaseTalker *>(ppAIs[i]);

		if( pTalker && FVisible( pTalker ) )
		{
			// Tell this guy he's already said hello to the player, too.
			pTalker->GetExpresser()->SetSpokeConcept( TLK_HELLO, NULL );
		}
	}
}


//---------------------------------------------------------
// Stop all allies from idle speech for a fixed amount
// of time. Mostly filthy hack to hold us over until
// acting comes online.
//---------------------------------------------------------
void CNPC_SimpleTalker::DeferAllIdleSpeech( float flDelay, CAI_BaseNPC *pIgnore )
{
	// Brute force. Just plow through NPC list looking for talkers.
#ifdef HL1_DLL
	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	CNPC_SimpleTalker *pTalker;

	float flTime = gpGlobals->curtime + flDelay;

	for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
	{
		if( ppAIs[i] != pIgnore )
		{
			pTalker = dynamic_cast<CNPC_SimpleTalker *>(ppAIs[i]);

			if( pTalker )
			{
				pTalker->m_flNextIdleSpeechTime = flTime;
			}
		}
	}
#endif

	BaseClass::DeferAllIdleSpeech( flDelay, pIgnore );
}

//---------------------------------------------------------
// Make eyecontact with a target
//---------------------------------------------------------
void CNPC_SimpleTalker::IdleHeadTurn( CBaseEntity *pTarget, float flDuration, float flImportance )
{
	// Must be able to turn our head
	if (!(CapabilitiesGet() & bits_CAP_TURN_HEAD))
		return;

	// If the target is invalid, or we're in a script, do nothing
	if ( ( !pTarget ) || ( m_NPCState == NPC_STATE_SCRIPT ) )
		return;

	// Fill in a duration if we haven't specified one
	if ( flDuration == 0.0f )
		 flDuration = random->RandomFloat( 2.0, 4.0 );

	// Add a look target
	AddLookTarget( pTarget, flImportance, flDuration, 0.1 );
}

//=========================================================
// FIdleSpeak
// ask question of nearby friend, or make statement
//=========================================================
int CNPC_SimpleTalker::FIdleSpeak( void )
{ 
	// try to start a conversation, or make statement
	int pitch;

	if (!IsOkToSpeak())
		return false;

	Assert( GetExpresser()->SemaphoreIsAvailable( this ) );
	
	pitch = GetExpresser()->GetVoicePitch();
		
	// player using this entity is alive and wounded?
	CBaseEntity *pTarget = GetTarget();

	if ( pTarget != NULL )
	{
		if ( pTarget->IsPlayer() )
		{
			if ( pTarget->IsAlive() )
			{
				SetSpeechTarget( GetTarget() );
				if (GetTarget()->m_iHealth <= GetTarget()->m_iMaxHealth / 8)
				{
					SpeakIfAllowed( TLK_PLHURT3 );
					return true;
				}
				else if (GetTarget()->m_iHealth <= GetTarget()->m_iMaxHealth / 4)
				{
					SpeakIfAllowed( TLK_PLHURT2 );
					return true;
				}
				else if (GetTarget()->m_iHealth <= GetTarget()->m_iMaxHealth / 2)
				{
					SpeakIfAllowed( TLK_PLHURT1 );
					return true;
				}
			}
			else
			{
				//!!!KELLY - here's a cool spot to have the talkNPC talk about the dead player if we want.
				// "Oh dear, Gordon Freeman is dead!" -Scientist
				// "Damn, I can't do this without you." -Barney
			}
		}
	}

	// ROBIN: Disabled idle question & answer for now
	// MM: This is handled in the baseclass
	/*
	// if there is a friend nearby to speak to, play sentence, set friend's response time, return
	CBaseEntity *pFriend = FindNearestFriend(false);

	// 75% chance of talking to another citizen if one is available.
	if (pFriend && !(pFriend->IsMoving()) && random->RandomInt( 0, 3 ) != 0 )
	{
		if ( SpeakQuestionFriend( pFriend ) )
		{
			// force friend to answer
			CAI_BaseTalker *pTalkNPC = dynamic_cast<CAI_BaseTalker *>(pFriend);
			if (pTalkNPC && !pTalkNPC->HasSpawnFlags(SF_NPC_GAG) && !pTalkNPC->IsInAScript() )
			{
				SetSpeechTarget( pFriend );
				pTalkNPC->SetAnswerQuestion( this );
				pTalkNPC->GetExpresser()->BlockSpeechUntil( GetExpresser()->GetTimeSpeechComplete() );

				m_nSpeak++;
			}

			// Don't let anyone else butt in.
			DeferAllIdleSpeech( random->RandomFloat( TALKER_DEFER_IDLE_SPEAK_MIN, TALKER_DEFER_IDLE_SPEAK_MAX ), pTalkNPC );
			return true;
		}
	}
	*/

	// Otherwise, play an idle statement, try to face client when making a statement.
	CBaseEntity *pFriend = FindNearestFriend(true);
	if ( pFriend )
	{
		SetSpeechTarget( pFriend );

		// If we're about to talk to the player, and we've never said hello, say hello first
		if ( !GetSpeechFilter() || !GetSpeechFilter()->NeverSayHello() )
		{
			if ( GetExpresser()->CanSpeakConcept( TLK_HELLO ) && !GetExpresser()->SpokeConcept( TLK_HELLO ) )
			{
				SayHelloToPlayer( pFriend );
				return true;
			}
		}

		if ( SpeakIfAllowed( TLK_IDLE ) )
		{
			DeferAllIdleSpeech( random->RandomFloat( TALKER_DEFER_IDLE_SPEAK_MIN, TALKER_DEFER_IDLE_SPEAK_MAX ) );
			m_nSpeak++;
		}
		else
		{
			// We failed to speak. Don't try again for a bit.
			m_flNextIdleSpeechTime = gpGlobals->curtime + 3;
		}
		return true;
	}

	// didn't speak
	m_flNextIdleSpeechTime = gpGlobals->curtime + 3;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Speak the right question based upon who we're asking
//-----------------------------------------------------------------------------
bool CNPC_SimpleTalker::SpeakQuestionFriend( CBaseEntity *pFriend )
{
	return SpeakIfAllowed( TLK_QUESTION );
}

//-----------------------------------------------------------------------------
// Purpose: Speak the right answer based upon who we're answering
//-----------------------------------------------------------------------------
bool CNPC_SimpleTalker::SpeakAnswerFriend( CBaseEntity *pFriend )
{
	return SpeakIfAllowed( TLK_ANSWER );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_SimpleTalker::FIdleSpeakWhileMoving( void )
{
	if ( GetExpresser() && GetExpresser()->CanSpeak() )
	{
		if (!GetExpresser()->IsSpeaking() || GetSpeechTarget() == NULL)
		{
			// override so that during walk, a scientist may talk and greet player
			FIdleHello();

#ifdef HL1_DLL
			if ( ShouldSpeakRandom( m_nSpeak * 20, GetSpeechFilter() ? GetSpeechFilter()->GetIdleModifier() : 1.0 ) )
				FIdleSpeak();
#else
			 if ( ShouldSpeakRandom( TLK_IDLE, m_nSpeak * 20 ) )
				 FIdleSpeak();
#endif
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_SimpleTalker::PlayScriptedSentence( const char *pszSentence, float delay, float volume, soundlevel_t soundlevel, bool bConcurrent, CBaseEntity *pListener )
{
	if ( !bConcurrent )
		ShutUpFriends();

	int sentenceIndex = BaseClass::PlayScriptedSentence( pszSentence, delay, volume, soundlevel, bConcurrent, pListener );
	delay += engine->SentenceLength( sentenceIndex );
	if ( delay < 0 )
		delay = 0;
	m_useTime = gpGlobals->curtime + delay;

	// Stop all idle speech until after the sentence has completed
	DeferAllIdleSpeech( delay + random->RandomInt( 3.0f, 5.0f ) );

	return sentenceIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Tell this NPC to answer a question from another NPC
//-----------------------------------------------------------------------------
void CNPC_SimpleTalker::SetAnswerQuestion( CNPC_SimpleTalker *pSpeaker )
{
	if ( !m_hCine )
	{
		SetCondition( COND_TALKER_RESPOND_TO_QUESTION );
	}

	SetSpeechTarget( (CAI_BaseNPC *)pSpeaker );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_SimpleTalker::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	CTakeDamageInfo subInfo = info;

#ifdef HL1_DLL
	// if player damaged this entity, have other friends talk about it.
	if (subInfo.GetAttacker() && (subInfo.GetAttacker()->GetFlags() & FL_CLIENT) && subInfo.GetDamage() < GetHealth() )
	{
		CBaseEntity *pFriend = FindNearestFriend(false);

		if (pFriend && pFriend->IsAlive())
		{
			// only if not dead or dying!
			CNPC_SimpleTalker *pTalkNPC = (CNPC_SimpleTalker *)pFriend;
#if 0
			if (pTalkNPC && pTalkNPC->IsOkToCombatSpeak())
			{
				pTalkNPC->Speak( TLK_NOSHOOT );
			}
#else
			pTalkNPC->SetSchedule( SCHED_TALKER_STOP_SHOOTING );
#endif
		}
	}
#endif

	// A supposed ally has shot me and im not in a barnacle
	if ( m_NPCState != NPC_STATE_PRONE )
	{
		CBaseCombatCharacter *jackAss = info.GetAttacker()->MyCombatCharacterPointer();
		bool bAttackedByPlayer = false;
		bool bWasAccidental = true;

		//CFmtStrN<128> modifiers( "attacked_by_player:%d,distance_from_attack:%f,was_accidental:%d", bAttackedByPlayer, EnemyDistance( jackAss ), bWasAccidental );

		// its hopefully in good faith
		if ( (jackAss && jackAss->IRelationType( this ) >= D_LI) )
		{
			if ( (info.GetAttacker()->GetFlags() & FL_CLIENT) )
			{
				// Player attacked me
				bAttackedByPlayer = true;

				// Store the players body direction
				Vector	idiotDir = GetAbsOrigin() - jackAss->GetAbsOrigin();
				VectorNormalize( idiotDir );
				Vector	idiotBodyDir = jackAss->BodyDirection3D();
				float idiotDot = DotProduct( idiotBodyDir, idiotDir );
				if ( idiotDot > 0.97f )
					bWasAccidental = false;

				// I've already been attacked in bad faith, so it makes this easy
				if (m_afMemory & bits_MEMORY_SUSPICIOUS)
				{
					// 50/50 roll for instant aggression
					if ( random->RandomInt( 0, 1 ) )
					{
#ifdef ALLIES_CAN_BE_PROVOKED
						SetCondition( COND_PROVOKED );
						SetSchedule( SCHED_TALKER_PROVOKED );
#endif
						CapabilitiesRemove(bits_CAP_NO_HIT_PLAYER);
					}
					else
					{
						// Look at the player very intensely
						if ( !GetEnemy() )
							SetSchedule( SCHED_TALKER_STOP_SHOOTING );
						else
						{
							GetMotor()->SetIdealYawToTarget( jackAss->WorldSpaceCenter() );
							if (IsMoving())
								AddFacingTarget( jackAss, 1.0, 0.2, 0 );
						}
					}
				}
				else
				{
					// Alright fine... lets do the logics now
					if ( GetEnemy() == NULL )
					{
						if ( HasCondition( COND_HEAR_COMBAT ) )
						{
							// There's some shooting over there, it might have been a ricochet - lower chances of suspicion
							if ( !bWasAccidental || HasCondition( COND_HEAVY_DAMAGE ) )
							{
								Remember( bits_MEMORY_SUSPICIOUS );
							}
						}
						else
						{
							// I know that was on purpose...
							Remember( bits_MEMORY_SUSPICIOUS );
						}
					}
					else
					{
						// We are properly fighting, so be a little more forgiving
						if ( subInfo.GetDamageType() & (DMG_BURN|DMG_BLAST|DMG_SONIC) )
						{
							// Watch what you're doing!
							bWasAccidental = true;
						}
						else
						{
							// That was a little too close!
							if ( !bWasAccidental || random->RandomInt( 0, 3 ) == 3 )
								Remember( bits_MEMORY_SUSPICIOUS );
						}
					}

					if ( IsOkToSpeak(SPEECH_PRIORITY) )
						Speak( TLK_NOSHOOT );
				}
			}
			else
			{
				if (IsOkToCombatSpeak())
					Speak( TLK_NOSHOOT );

				// Also tell the jackass to say sorry
				CNPC_SimpleTalker *pTalkNPC = dynamic_cast<CNPC_SimpleTalker *>( jackAss );
				if( pTalkNPC && pTalkNPC->GetExpresser()->CanSpeakConcept( TLK_ANSWER ) )
					pTalkNPC->SpeakIfAllowed( TLK_ANSWER );
			}
		}
	}

	return BaseClass::OnTakeDamage_Alive( subInfo );
}


int CNPC_SimpleTalker::SelectNonCombatSpeechSchedule()
{
	if ( !IsOkToSpeak() )
		return SCHED_NONE;
		
	// talk about world
#ifdef HL1_DLL
	if ( ShouldSpeakRandom( m_nSpeak * 2, GetSpeechFilter() ? GetSpeechFilter()->GetIdleModifier() : 1.0 ) )
	{
		//Msg("standing idle speak\n" );
		return SCHED_TALKER_IDLE_SPEAK;
	}
#else
	if ( ShouldSpeakRandom( TLK_IDLE, m_nSpeak * 2 ) )
		return SCHED_TALKER_IDLE_SPEAK;
#endif

	// failed to speak, so look at the player if he's around
	if ( AI_IsSinglePlayer() && GetExpresser()->CanSpeak() && HasCondition( COND_SEE_PLAYER ) && random->RandomInt( 0, 6 ) == 0 )
	{
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		Assert( pPlayer );

		if ( pPlayer )
		{
			// watch the client.
			Vector forward;
			AngleVectors( pPlayer->GetLocalAngles(), &forward );
			if ( ( pPlayer->GetAbsOrigin() - GetAbsOrigin() ).Length2D() < TALKER_STARE_DIST	&& 
				 UTIL_DotPoints( pPlayer->GetAbsOrigin(), GetAbsOrigin(), forward ) >= m_flFieldOfView )
			{
				// go into the special STARE schedule if the player is close, and looking at me too.
				return SCHED_TALKER_IDLE_WATCH_CLIENT_STARE;
			}

			return SCHED_TALKER_IDLE_WATCH_CLIENT;
		}
	}
	else
	{
		// look at who we're talking to
		if ( GetSpeechTarget() && GetExpresser()->IsSpeaking() )
			return SCHED_TALKER_IDLE_EYE_CONTACT;
	}
	return SCHED_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_SimpleTalker::CanSayHello( void )
{
	if ( GetSpeechFilter() && GetSpeechFilter()->NeverSayHello() )
		return false;

	if ( !GetExpresser()->CanSpeakConcept(TLK_HELLO) || GetExpresser()->SpokeConcept(TLK_HELLO) )
		return false;

	if ( !IsOkToSpeak() )
		return false;

	return true;
}

void CNPC_SimpleTalker::OnStartingFollow( CBaseEntity *pTarget )
{
	GetExpresser()->SetSpokeConcept( TLK_HELLO, NULL );	// Don't say hi after you've started following
	if ( IsOkToSpeak() ) // don't speak if idle talk is blocked. player commanded/use follow will always speak
		Speak( TLK_STARTFOLLOW );
	SetSpeechTarget( GetTarget() );
	ClearCondition( COND_PLAYER_PUSHING );
}

void CNPC_SimpleTalker::OnStoppingFollow( CBaseEntity *pTarget )
{
	if ( IsPlayerAlly() )
	{
		if ( IsOkToCombatSpeak() )
		{
			if ( pTarget == NULL )
				Speak( TLK_STOPFOLLOW );
			else
				Speak( TLK_STOP );
		}
		SetSpeechTarget( FindNearestFriend(true) );
	}
}

void CNPC_SimpleTalker::FollowerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Don't allow use during a scripted_sentence
	if ( GetUseTime() > gpGlobals->curtime )
		return;

	m_useTime = gpGlobals->curtime + 0.2;

	if ( pCaller != NULL && pCaller->IsPlayer() )
	{
#ifdef HL1_DLL
		// Pre-disaster followers can't be used
		if ( HasSpawnFlags(SF_NPC_PREDISASTER) )
		{
			DeclineFollowing();
		}
#endif
		if ( !m_FollowBehavior.GetFollowTarget() && IsInterruptable() )
		{
			LimitFollowers( pCaller, 1 );
			if ( m_afMemory & bits_MEMORY_PROVOKED )
				DevMsg( "I'm not following you, you evil person!\n" );
			else
			{
				StartFollowing( pCaller );
			}
		}
		else if ( m_FollowBehavior.GetFollowTarget() )
		{
			StopFollowing();
		}
		else
		{
			SelectPlayerUseSpeech(pCaller);
		}
	}

	BaseClass::NPCUse(pActivator,pCaller,useType,value);
}

bool CNPC_SimpleTalker::SelectPlayerUseSpeech( CBaseEntity *pPlayer )
{
	bool bTalked = false;	//At least one party talked

	// See if I have a custom use response
	if( IsOkToSpeakInResponseToPlayer() )
	{
		if ( m_iszUse != NULL_STRING )
			SpeakIfAllowed( STRING( m_iszUse ));
		else
			SpeakIfAllowed( TLK_USE );

		DeferAllIdleSpeech(random->RandomFloat( TALKER_DEFER_IDLE_SPEAK_MIN, TALKER_DEFER_IDLE_SPEAK_MAX ));
		bTalked = true;
	}

	// See if I want the player to reply
	if ( pPlayer != NULL )
	{
		CBaseMultiplayerPlayer *pTalkPlayer = ToBaseMultiplayerPlayer( pPlayer );
		if ( pTalkPlayer && m_iszUnUse != NULL_STRING )
		{
			if ( pTalkPlayer->SpeakIfAllowed( STRING( m_iszUnUse )))
				bTalked = true;
		}
	}

	return bTalked;
}

void CNPC_SimpleTalker::StartFollowing( CBaseEntity *pLeader )
{
	if ( (m_afMemory & bits_MEMORY_PROVOKED) )
		return;

	if ( !HasSpawnFlags( SF_NPC_GAG ) )
	{
		SetSpeechTarget( pLeader );
		SpeakIfAllowed( TLK_STARTFOLLOW );
	}

	m_FollowBehavior.SetFollowTarget( pLeader ); 
	DeferSchedulingToBehavior( &m_FollowBehavior );
}

void CNPC_SimpleTalker::StopFollowing( void )
{
	if ( !HasSpawnFlags( SF_NPC_GAG ) )
	{
		SetSpeechTarget( GetFollowTarget() );
		SpeakIfAllowed( TLK_STOPFOLLOW );
	}

	m_FollowBehavior.SetFollowTarget( NULL ); 
	DeferSchedulingToBehavior( NULL );
}

//-----------------------------------------------------------------------------
void CNPC_SimpleTalker::InputIdleRespond( inputdata_t &inputdata )
{
	// We've been told to respond. Check combat speak, not isoktospeak, because
	// we don't want to check the idle speech time.
	if (!IsOkToCombatSpeak())
		return;

	IdleRespond();
}

int CNPC_SimpleTalkerExpresser::SpeakRawSentence( const char *pszSentence, float delay, float volume, soundlevel_t soundlevel, CBaseEntity *pListener )
{
	char szSpecificSentence[1024];
	int sentenceIndex = -1;

	if ( !pszSentence )
		return sentenceIndex;

	if ( pszSentence[0] == AI_SP_START_MONOLOG )
	{
		// this sentence command will start this NPC speaking 
		// lengthy monolog from smaller sentences. 
		BeginMonolog( (char *)pszSentence, pListener );
		return -1;
	}
	else if ( pszSentence[0] == AI_SP_MONOLOG_LINE )
	{
		Q_strncpy(szSpecificSentence, pszSentence, sizeof(szSpecificSentence) );
		szSpecificSentence[0] = AI_SP_SPECIFIC_SENTENCE;
		pszSentence = szSpecificSentence;
	}
	else
	{
		// this bit of speech is interrupting my monolog!
		SuspendMonolog( 0 );
	}

	return CAI_Expresser::SpeakRawSentence( pszSentence, delay, volume, soundlevel, pListener );
}

//-------------------------------------

void CNPC_SimpleTalkerExpresser::BeginMonolog( char *pszSentenceName, CBaseEntity *pListener )
{
	if( pListener )
	{
		m_hMonologTalkTarget = pListener;
	}
	else
	{
		Warning( "NULL Listener in BeginMonolog()!\n" );
		Assert(0);
		EndMonolog();
		return;
	}

	Q_strncpy( m_szMonologSentence, pszSentenceName ,sizeof(m_szMonologSentence));

	// change the "AI_SP_START_MONOLOG" to an "AI_SP_MONOLOG_LINE". m_sMonologSentence is now the 
	// string we'll tack numbers onto to play sentences from this group in 
	// sequential order.
	m_szMonologSentence[0] = AI_SP_MONOLOG_LINE;

	m_fMonologSuspended = false;

	m_iMonologIndex = 0;
}

//-------------------------------------

void CNPC_SimpleTalkerExpresser::EndMonolog( void )
{
	m_szMonologSentence[0] = 0;
	m_iMonologIndex = -1;
	m_fMonologSuspended = false;
	m_hMonologTalkTarget = NULL;
}

//-------------------------------------

void CNPC_SimpleTalkerExpresser::SpeakMonolog( void )
{
	int i;
	char szSentence[ MONOLOGNAME_LEN ];

	if( !HasMonolog() )
	{
		return;
	}

	if( CanSpeak() )
	{
		if( m_fMonologSuspended )
		{
			if ( GetOuter()->ShouldResumeMonolog() )
			{
				ResumeMonolog();
			}

			return;
		}

		Q_snprintf( szSentence,sizeof(szSentence), "%s%d", m_szMonologSentence, m_iMonologIndex );
		m_iMonologIndex++;

		i = SpeakRawSentence( szSentence, 0, VOL_NORM );

		if ( i == -1 )
		{
			EndMonolog();
		}
	}
	else
	{
		if( GetOuter()->ShouldSuspendMonolog() )
		{
			SuspendMonolog( 0 );
		}
	}
}

//-------------------------------------

void CNPC_SimpleTalkerExpresser::SuspendMonolog( float flInterval )
{
	if( HasMonolog() )
	{
		m_fMonologSuspended = true;
	}
	
	// free up other characters to speak.
	if ( GetSink()->UseSemaphore() )
	{
		GetSpeechSemaphore( GetOuter() )->Release();
	}
}

//-------------------------------------

void CNPC_SimpleTalkerExpresser::ResumeMonolog( void )
{
	if( m_iMonologIndex > 0 )
	{
		// back up and repeat what I was saying
		// when interrupted.
		m_iMonologIndex--;
	}

	GetOuter()->OnResumeMonolog();
	m_fMonologSuspended = false;
}

//-----------------------------------------------------------------------------
// Purpose: try to smell something
//-----------------------------------------------------------------------------
void CNPC_SimpleTalker::TrySmellTalk( void )
{
	if ( !IsOkToSpeak() )
		return;

	if ( HasCondition( COND_SMELL ) && GetExpresser()->CanSpeakConcept( TLK_SMELL ) )
		SpeakIfAllowed( TLK_SMELL );
}

void CNPC_SimpleTalker::OnChangeRunningBehavior( CAI_BehaviorBase *pOldBehavior,  CAI_BehaviorBase *pNewBehavior )
{
	BaseClass::OnChangeRunningBehavior( pOldBehavior,  pNewBehavior );

	CAI_FollowBehavior *pFollowBehavior;
	if ( ( pFollowBehavior = dynamic_cast<CAI_FollowBehavior *>(pNewBehavior) ) != NULL  )
	{
		OnStartingFollow( pFollowBehavior->GetFollowTarget() );
	}
	else if ( ( pFollowBehavior = dynamic_cast<CAI_FollowBehavior *>(pOldBehavior) ) != NULL  )
	{
		OnStoppingFollow( pFollowBehavior->GetFollowTarget() );
	}
}


bool CNPC_SimpleTalker::OnBehaviorChangeStatus( CAI_BehaviorBase *pBehavior, bool fCanFinishSchedule )
{
	bool interrupt = BaseClass::OnBehaviorChangeStatus( pBehavior, fCanFinishSchedule );
	if ( !interrupt )
	{
		interrupt = ( dynamic_cast<CAI_FollowBehavior *>(pBehavior) != NULL && ( m_NPCState == NPC_STATE_IDLE || m_NPCState == NPC_STATE_ALERT ) );
	}
	return interrupt;

}

//-----------------------------------------------------------------------------
// Purpose: Return true if I should speak based on the chance & the speech filter's modifier
//-----------------------------------------------------------------------------
#ifdef HL1_DLL
bool CNPC_SimpleTalker::ShouldSpeakRandom( int iChance, float flModifier )
{
	if ( flModifier != 1.0 )
	{
		// Avoid divide by zero
		if ( !flModifier )
			return false;

		iChance = floor( (float)iChance / flModifier );
	}

	return (random->RandomInt(0,iChance) == 0);
}
#endif


AI_BEGIN_CUSTOM_NPC(talk_monster,CNPC_SimpleTalker)
	DECLARE_USES_SCHEDULE_PROVIDER( CAI_FollowBehavior )

	DECLARE_TASK(TASK_TALKER_RESPOND)
	DECLARE_TASK(TASK_TALKER_SPEAK)
	DECLARE_TASK(TASK_TALKER_HELLO)
	DECLARE_TASK(TASK_TALKER_BETRAYED)
	DECLARE_TASK(TASK_TALKER_PROVOKED)
	DECLARE_TASK(TASK_TALKER_HEADRESET)
	DECLARE_TASK(TASK_TALKER_STOPSHOOTING)
	DECLARE_TASK(TASK_TALKER_STARE)
	DECLARE_TASK(TASK_TALKER_LOOK_AT_CLIENT)
	DECLARE_TASK(TASK_TALKER_CLIENT_STARE)
	DECLARE_TASK(TASK_TALKER_EYECONTACT)
	DECLARE_TASK(TASK_TALKER_IDEALYAW)
	DECLARE_TASK(TASK_TALKER_WAIT_FOR_SEMAPHORE)

	//=========================================================
	// > SCHED_TALKER_IDLE_RESPONSE
	//=========================================================
	DEFINE_SCHEDULE 
	(
		SCHED_TALKER_IDLE_RESPONSE,

		"	Tasks"
		"		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"	// Stop and listen
		"		TASK_WAIT						0.5"				// Wait until sure it's me they are talking to
		"		TASK_TALKER_IDEALYAW			0"			// face who I'm talking to
		"		TASK_FACE_IDEAL					0"
		"		TASK_TALKER_EYECONTACT			0"					// Wait until speaker is done
		"		TASK_TALKER_WAIT_FOR_SEMAPHORE	0"
		"		TASK_TALKER_EYECONTACT			0"					// Wait until speaker is done
		"		TASK_TALKER_RESPOND				0"					// Wait and then say my response
		"		TASK_TALKER_IDEALYAW			0"					// face who I'm talking to
		"		TASK_FACE_IDEAL					0"
		"		TASK_SET_ACTIVITY				ACTIVITY:ACT_SIGNAL2"
		"		TASK_TALKER_EYECONTACT			0"					// Wait until speaker is done
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_COMBAT"
		"		COND_PLAYER_PUSHING"
		"		COND_GIVE_WAY"
	)

	//=========================================================
	// > SCHED_TALKER_IDLE_SPEAK
	//=========================================================
	DEFINE_SCHEDULE 
	(
		SCHED_TALKER_IDLE_SPEAK,

		"	Tasks"
		"		TASK_TALKER_SPEAK			0"			// question or remark
		"		TASK_TALKER_IDEALYAW		0"			// face who I'm talking to
		"		TASK_FACE_IDEAL				0"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_SIGNAL2"
		"		TASK_TALKER_EYECONTACT		0"		// make sure you're looking at him
		"		TASK_WAIT_RANDOM			0.5"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_COMBAT"
		"		COND_PLAYER_PUSHING"
		"		COND_GIVE_WAY"
	)

	//=========================================================
	// > SCHED_TALKER_IDLE_HELLO
	//=========================================================
	DEFINE_SCHEDULE 
	(
		SCHED_TALKER_IDLE_HELLO,

		"	Tasks"
		"		 TASK_SET_ACTIVITY				ACTIVITY:ACT_SIGNAL1"	// Stop and talk
		"		 TASK_TALKER_HELLO				0"			// Try to say hello to player
		"		 TASK_TALKER_EYECONTACT			0"		// make sure you're looking at him
		"		 TASK_WAIT						0.5"		// wait a bit
		"		 TASK_TALKER_HELLO				0"			// Try to say hello to player
		"		 TASK_TALKER_EYECONTACT			0"		// make sure you're looking at him
		"		 TASK_WAIT						0.5"		// wait a bit
		"		 TASK_TALKER_HELLO				0"			// Try to say hello to player
		"		 TASK_TALKER_EYECONTACT			0"		// make sure you're looking at him
		"		 TASK_WAIT						0.5"		// wait a bit
		"		 TASK_TALKER_HELLO				0"			// Try to say hello to player
		"		 TASK_TALKER_EYECONTACT			0"		// make sure you're looking at him
		"		 TASK_WAIT						0.5"		// wait a bit
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_PROVOKED"
		"		COND_HEAR_COMBAT"
		"		COND_HEAR_DANGER"
		"		COND_PLAYER_PUSHING"
		"		COND_GIVE_WAY"
	)

	//=========================================================
	// > SCHED_TALKER_STOP_SHOOTING
	//=========================================================
	DEFINE_SCHEDULE 
	(
		SCHED_TALKER_STOP_SHOOTING,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_ALERT_FACE"
		"		TASK_TALKER_STOPSHOOTING		0"	// tell player to stop shooting friend
		"		TASK_TALKER_IDEALYAW			0"	// face who I'm talking to
		"		TASK_WAIT_FOR_SPEAK_FINISH		0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
	)

	//=========================================================
	// Scold the player before attacking.
	//=========================================================
	DEFINE_SCHEDULE 
	(
		SCHED_TALKER_BETRAYED,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_ALERT_FACE"
		"		TASK_TALKER_BETRAYED		0"	// call the player a dumbass
		"		TASK_TALKER_IDEALYAW		0"	// face who I'm talking to
		"		TASK_WAIT_FOR_SPEAK_FINISH	0"
		""
		"	Interrupts"
		"		COND_HEAR_DANGER"
	)

	DEFINE_SCHEDULE 
	(
		SCHED_TALKER_PROVOKED,

		"	Tasks"
		"		TASK_TALKER_PROVOKED		0"	// call the player a dumbass
		"		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_ALERT_FACE"
		"		TASK_TALKER_IDEALYAW		0"	// face who I'm talking to
		"		TASK_WAIT_FOR_SPEAK_FINISH	0"
		""
		"	Interrupts"
		"		COND_HEAR_DANGER"
	)

	//=========================================================
	// > SCHED_TALKER_IDLE_WATCH_CLIENT
	//=========================================================
	DEFINE_SCHEDULE 
	(
		SCHED_TALKER_IDLE_WATCH_CLIENT,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"
		"		TASK_TALKER_LOOK_AT_CLIENT		6"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_PROVOKED"
		"		COND_HEAR_COMBAT"		// sound flags - change these and you'll break the talking code.
		"		COND_HEAR_DANGER"
		"		COND_SMELL"
		"		COND_PLAYER_PUSHING"
		"		COND_TALKER_CLIENTUNSEEN"
		"		COND_GIVE_WAY"
		"		COND_IDLE_INTERRUPT"
	)
	 
	//=========================================================
	// > SCHED_TALKER_IDLE_WATCH_CLIENT_STARE
	//=========================================================
	DEFINE_SCHEDULE 
	(
		SCHED_TALKER_IDLE_WATCH_CLIENT_STARE,

		"	Tasks"
		"		 TASK_STOP_MOVING				0"
		"		 TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"
		"		 TASK_TALKER_CLIENT_STARE		6"
		"		 TASK_TALKER_STARE				0"
		"		 TASK_TALKER_IDEALYAW			0"			// look at who I'm talking to
		"		 TASK_FACE_IDEAL				0"
		"		 TASK_TALKER_EYECONTACT			0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_PROVOKED"
		"		COND_HEAR_COMBAT"		// sound flags - change these and you'll break the talking code.
		"		COND_HEAR_DANGER"
		"		COND_SMELL"
		"		COND_PLAYER_PUSHING"
		"		COND_TALKER_CLIENTUNSEEN"
		"		COND_GIVE_WAY"
		"		COND_IDLE_INTERRUPT"
	)

	//=========================================================
	// > SCHED_TALKER_IDLE_EYE_CONTACT
	//=========================================================
	DEFINE_SCHEDULE 
	(
		SCHED_TALKER_IDLE_EYE_CONTACT,

		"	Tasks"
		"		TASK_TALKER_IDEALYAW			0"			// look at who I'm talking to
		"		TASK_FACE_IDEAL					0"
		"		TASK_TALKER_EYECONTACT			0"			// Wait until speaker is done
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_COMBAT"
		"		COND_PLAYER_PUSHING"
		"		COND_GIVE_WAY"
		"		COND_IDLE_INTERRUPT"
	)

AI_END_CUSTOM_NPC()
