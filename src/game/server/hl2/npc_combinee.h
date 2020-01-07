//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// Purpose:		This is the camo version of the combine soldier
//
//=============================================================================

#ifndef	NPC_COMBINE_E_H
#define	NPC_COMBINE_E_H
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
	void		DeathSound( const CTakeDamageInfo &info );
	virtual void	PrescheduleThink();

	bool		m_fIsBlocking;
	void		HandleAnimEvent( animevent_t *pEvent );

	bool		ShouldMoveAndShoot();

	bool		IsLightDamage( const CTakeDamageInfo &info );
	bool		IsHeavyDamage( const CTakeDamageInfo &info );

private:
	// Time Variables
	float			m_flStopMoveShootTime;

};

#endif	//NPC_COMBINE_E_H
