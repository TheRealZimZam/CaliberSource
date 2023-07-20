//========= Copyright � 2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: The Garry-Man, misunderstood servant of the people
//
// $NoKeywords: $
//
//=============================================================================//


//-----------------------------------------------------------------------------
// Generic NPC - purely for scripted sequence work.
//-----------------------------------------------------------------------------
#include "cbase.h"
#include "npcevent.h"
#include "ai_basenpc.h"
#include "ai_hull.h"
#include "ai_behavior_follow.h"
#include "ai_basetalker.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CNPC_GMan : public CAI_BaseTalker
{
public:
	DECLARE_CLASS( CNPC_GMan, CAI_BaseTalker );
	DECLARE_DATADESC();

	void	Spawn( void );
	void	Precache( void );
	Class_T Classify( void );
	void	HandleAnimEvent( animevent_t *pEvent );
	virtual Disposition_t IRelationType(CBaseEntity *pTarget);
	int		GetSoundInterests( void );
	bool	CreateBehaviors( void );
	int		SelectSchedule( void );

	int		OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo );
//	void	TraceAttack( CBaseEntity *pAttacker, float flDamage, const Vector &vecDir, trace_t *ptr, int bitsDamageType);

private:
	CAI_FollowBehavior	m_FollowBehavior;
};

LINK_ENTITY_TO_CLASS( npc_gman, CNPC_GMan );
#ifndef HL1_DLL
LINK_ENTITY_TO_CLASS( monster_gman, CNPC_GMan );
#endif
LINK_ENTITY_TO_CLASS( npc_stranger, CNPC_GMan );

BEGIN_DATADESC( CNPC_GMan )
// (auto saved by AI)
//	DEFINE_FIELD( m_FollowBehavior, FIELD_EMBEDDED ),	(auto saved by AI)
END_DATADESC()

//-----------------------------------------------------------------------------
// Classify - indicates this NPC's place in the 
// relationship table.
//-----------------------------------------------------------------------------
Class_T	CNPC_GMan::Classify( void )
{
	return CLASS_NONE;
}

//-----------------------------------------------------------------------------
// HandleAnimEvent - catches the NPC-specific messages
// that occur when tagged animation frames are played.
//-----------------------------------------------------------------------------
void CNPC_GMan::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
	case NPC_EVENT_LEFTFOOT:
		{
			EmitSound( "npc_gman.FootstepLeft", pEvent->eventtime );
		}
		break;
	case NPC_EVENT_RIGHTFOOT:
		{
			EmitSound( "npc_gman.FootstepRight", pEvent->eventtime );
		}
		break;

	default:
		BaseClass::HandleAnimEvent( pEvent );
		break;
	}
}

//-----------------------------------------------------------------------------
// GetSoundInterests - generic NPC can't hear.
//-----------------------------------------------------------------------------
int CNPC_GMan::GetSoundInterests( void )
{
	return NULL;
}

//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CNPC_GMan::Spawn()
{
	Precache();

	BaseClass::Spawn();

	SetModel( "models/gman.mdl" );

	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	SetBloodColor( BLOOD_COLOR_BLUE );
	m_iHealth			= 9999;	//disgusting hack - FIXME
	m_flFieldOfView		= 0.5;// indicates the width of this NPC's forward view cone ( as a dotproduct result )
	m_NPCState			= NPC_STATE_NONE;
	SetImpactEnergyScale( 0.0f ); // no physics damage on the gman
	
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_OPEN_DOORS | bits_CAP_ANIMATEDFACE | bits_CAP_TURN_HEAD );
	CapabilitiesAdd( bits_CAP_FRIENDLY_DMG_IMMUNE );
	AddEFlags( EFL_NO_DISSOLVE | EFL_NO_MEGAPHYSCANNON_RAGDOLL );

	NPCInit();
}

//-----------------------------------------------------------------------------
// Precache - precaches all resources this NPC needs
//-----------------------------------------------------------------------------
void CNPC_GMan::Precache()
{
	PrecacheModel( "models/gman.mdl" );
	PrecacheScriptSound( "npc_gman.FootstepLeft" );
	PrecacheScriptSound( "npc_gman.FootstepRight" );
		
	BaseClass::Precache();
}	

//-----------------------------------------------------------------------------
// The G-Man isn't scared of anything (but vorts).
//-----------------------------------------------------------------------------
Disposition_t CNPC_GMan::IRelationType(CBaseEntity *pTarget)
{
	return D_NU;
}


//=========================================================
// Purpose:
//=========================================================
bool CNPC_GMan::CreateBehaviors()
{
	AddBehavior( &m_FollowBehavior );
	
	return BaseClass::CreateBehaviors();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_GMan::SelectSchedule( void )
{
	if ( !BehaviorSelectSchedule() )
	{
	}

	return BaseClass::SelectSchedule();
}

//=========================================================
// Override all damage
//=========================================================
int CNPC_GMan::OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo )
{
	m_iHealth = m_iMaxHealth / 2; // always trigger the 50% damage aitrigger

	if ( inputInfo.GetDamage() > 0 )
		 SetCondition( COND_LIGHT_DAMAGE );

	if ( inputInfo.GetDamage() >= 20 )
		 SetCondition( COND_HEAVY_DAMAGE );

	return true;
}

#if 0
void CNPC_GMan::TraceAttack( CBaseEntity *pAttacker, float flDamage, const Vector &vecDir, trace_t *ptr, int bitsDamageType)
{
	g_pEffects->Ricochet( ptr->endpos, ptr->plane.normal );
//	AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
}
#endif

//-----------------------------------------------------------------------------
// AI Schedules Specific to this NPC
//-----------------------------------------------------------------------------

	//=========================================================
	// > SCHED_GMAN_TELEPORT
	//=========================================================

