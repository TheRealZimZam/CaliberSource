//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		This is the camo version of the combine soldier
//
//=============================================================================//

#ifndef	NPC_COMBINEE_H
#define	NPC_COMBINEE_H
#ifdef _WIN32
#pragma once
#endif

#include "npc_combine.h"

//=========================================================
//	>> CNPC_CombineE
//=========================================================
class CNPC_CombineE : public CNPC_Combine
{
	DECLARE_CLASS( CNPC_CombineE, CNPC_Combine );

public:
	void		Spawn( void );
	void		Precache( void );
	void		Event_Killed( const CTakeDamageInfo &info );

	void		BuildScheduleTestBits( void );
	void		ClearAttackConditions( void );
	// -----------------------------------------------------------------------------------
	// -----------------------------------------------------------------------------------
	void			DeathSound( const CTakeDamageInfo &info );
	void			PainSound( const CTakeDamageInfo &info );
#if 0
	void			IdleSound( void );
	void			AlertSound( void );
	void			LostEnemySound( void );
	void			FoundEnemySound( void ) { return; };
	void			AnnounceAssault( void );
	void			AnnounceEnemyType( CBaseEntity *pEnemy );
	void			AnnounceEnemyKill( CBaseEntity *pEnemy );
	void			NotifyDeadFriend( CBaseEntity* pFriend );
	void			SpeakSentence( int sentType );
#endif

	void			StartTask( const Task_t *pTask );

	bool		m_fIsBlocking;
	void		HandleAnimEvent( animevent_t *pEvent );

	bool		ShouldMoveAndShoot();

	bool		IsLightDamage( const CTakeDamageInfo &info );
	bool		IsHeavyDamage( const CTakeDamageInfo &info );

	//Nope
//!	virtual	bool		AllowedToIgnite( void ) { return true; }
};

#endif	//NPC_COMBINEE_H
