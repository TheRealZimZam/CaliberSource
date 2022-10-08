//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef NPC_COMBINES_H
#define NPC_COMBINES_H
#ifdef _WIN32
#pragma once
#endif

#include "npc_combine.h"

//=========================================================
//	>> CNPC_CombineS
//=========================================================
class CNPC_CombineS : public CNPC_Combine
{
	DECLARE_CLASS( CNPC_CombineS, CNPC_Combine );
#if HL2_EPISODIC
	DECLARE_DATADESC();
#endif

public: 
	void		Spawn( void );
	void		Precache( void );
	void		Event_Killed( const CTakeDamageInfo &info );

	void		BuildScheduleTestBits( void );
	void		ClearAttackConditions( void );
	// -----------------------------------------------------------------------------------
	// -----------------------------------------------------------------------------------
	void		DeathSound( const CTakeDamageInfo &info );
	void		PainSound( const CTakeDamageInfo &info );
	int			SelectSchedule( void );
	void		HandleAnimEvent( animevent_t *pEvent );
	void		OnChangeActivity( Activity eNewActivity );
	void		OnListened();

	bool		m_fIsBlocking;

	bool		IsLightDamage( const CTakeDamageInfo &info );
	bool		IsHeavyDamage( const CTakeDamageInfo &info );

	virtual	bool		AllowedToIgnite( void ) { return true; }

#if HL2_EPISODIC
public:
	Activity	NPC_TranslateActivity( Activity eNewActivity );

protected:
	/// whether to use the more casual march anim in ep2_outland_05
	int			m_iUseMarch;
#endif

};

#endif // NPC_COMBINES_H
