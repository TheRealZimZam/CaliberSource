//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Super simple entity to check the current skill level.
//			Shoots out two outputs, depending on the check.
//			Doesnt have to think, or do much of anything really. Super cheap.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "entityinput.h"
#include "entityoutput.h"
#include "eventqueue.h"
#include "mathlib/mathlib.h"
#include "gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

const int SF_REMOVE_ON_FIRE				= 0x001;	// This will remove itself after being triggered.

class CLogicSkill : public CLogicalEntity
{
public:
	DECLARE_CLASS( CLogicSkill, CLogicalEntity );

	CLogicSkill();

	void InputCheckSkill( inputdata_t &inputdata );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	//FIXME; Is this needed?
	int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();

private:
	int m_iSkillLevel;	//Skill level to check for
	enum SkillCheck_t
	{
		EASY = 1,
		NORMAL,
		HARD,
		VERYHARD,
		HARDANDVERYHARD,
		EASYANDNORMAL,
	};

	COutputEvent m_OnFailed;
	COutputEvent m_OnSuccess;
};

LINK_ENTITY_TO_CLASS(logic_skill, CLogicSkill);

BEGIN_DATADESC( CLogicSkill )

	DEFINE_KEYFIELD(m_iSkillLevel, FIELD_INTEGER, "skilllevel"),

	// Inputs
	DEFINE_INPUTFUNC(FIELD_VOID, "CheckSkill", InputCheckSkill),

	// Outputs
	DEFINE_OUTPUT(m_OnFailed, "OnFailed"),
	DEFINE_OUTPUT(m_OnSuccess, "OnSuccess"),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CLogicSkill::CLogicSkill(void)
{
}

//------------------------------------------------------------------------------
// Purpose: Check Skill
//------------------------------------------------------------------------------
void CLogicSkill::InputCheckSkill( inputdata_t &inputdata )
{
	Use(inputdata.pActivator, inputdata.pCaller, USE_ON, 0);
}

void CLogicSkill::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	bool bSuccess = false;

	switch( m_iSkillLevel )
	{
	case HARDANDVERYHARD:
		// Stomp the normal check
		if ( g_pGameRules->IsSkillLevel( SKILL_HARD ) || g_pGameRules->IsSkillLevel( SKILL_VERYHARD ) )
			bSuccess = true;
		break;

	case EASYANDNORMAL:
		// Stomp the normal check
		if ( g_pGameRules->IsSkillLevel( SKILL_EASY ) || g_pGameRules->IsSkillLevel( SKILL_NORMAL ) )
			bSuccess = true;
		break;

	default:
		// Do a normal check
		if ( m_iSkillLevel == g_pGameRules->GetSkillLevel() )
			bSuccess = true;
		break;
	}
	
	if (bSuccess)
	{
		SUB_UseTargets( pActivator, USE_ON, 0 );
		m_OnSuccess.FireOutput( this, this );
	}
	else
		m_OnFailed.FireOutput( this, this );

	// Fire once, fornevermore
	if ( m_spawnflags & SF_REMOVE_ON_FIRE )
		UTIL_Remove(this);
}

