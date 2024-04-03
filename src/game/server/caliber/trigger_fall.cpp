//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: Used at the bottom (or top) of maps where objects should fall away to infinity
// Plays special effects for players, everything else just fades away.
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "triggers.h"

//-----------------------------------------------------------------------------
// Purpose: That one scene in interstellar where the dude flies away but 5x faster
//-----------------------------------------------------------------------------
class CTriggerFall : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerFall, CBaseTrigger );
public:
	void Precache(void);
	void Spawn(void);
	void FallTouch( CBaseEntity *pOther );
	void FallThink(void);

	DECLARE_DATADESC();

private:
	bool m_bDidDamage;
	bool m_bHasStuff;
	float m_fCleanupTime;	//If an object has touched me this long, delete it
	COutputEvent m_OnFallingObject;
//	COutputEvent m_RemovedObject;
};

BEGIN_DATADESC( CTriggerFall )

	// Function Pointers
	DEFINE_FUNCTION( FallTouch ),
	DEFINE_FUNCTION( FallThink ),

	// Fields
	DEFINE_FIELD( m_bDidDamage, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bHasStuff, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_fCleanupTime, FIELD_TIME ),

	// Inputs/Outputs
	DEFINE_OUTPUT( m_OnFallingObject, "OnFallingObject" ),
//	DEFINE_OUTPUT( m_RemovedObject, "RemovedObject" ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( trigger_fall, CTriggerFall );


//-----------------------------------------------------------------------------
// Purpose: Called when spawning, after keyvalues have been handled.
//-----------------------------------------------------------------------------
void CTriggerFall::Precache( void )
{
	BaseClass::Precache();

	PrecacheScriptSound( "Player.Falling" );
}

void CTriggerFall::Spawn( void )
{
	BaseClass::Spawn();
	Precache();
	InitTrigger();

	m_bHasStuff = false;
	m_bDidDamage = false;
	m_fCleanupTime = 0;
	SetTouch( &CTriggerFall::FallTouch );
	SetThink( &CTriggerFall::FallThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: Make the object fall away
// Input  : pOther - The entity that is touching us.
//-----------------------------------------------------------------------------
void CTriggerFall::FallTouch( CBaseEntity *pOther )
{
	// If it's a player, make him yell, then kill him in a second or two
	if ( pOther->IsPlayer() )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pOther );

		if ( pPlayer->IsAlive() == false )
			return;

		// TODO; not finished, should never happen anyway
/*
		if ( !m_bDidDamage )
		{
			// Setup sound to fade over 2 or 3 seconds, then do damage
			float fScreamTime = random->RandomFloat( 2, 3 );
			pPlayer->EmitSound( "Player.Falling" );
			return;
		}
*/

		pPlayer->TakeDamage( CTakeDamageInfo( this, this, 999, DMG_FALL ) );
		m_bDidDamage = true;

		pPlayer->SetAnimation( PLAYER_FALL );
	}
	else 
	{
		// Dont touch logic entities OR other triggers!!!
		// -MM Its easier to just check what we SHOULD damage, than what we shouldnt
		// Npcs, pushables, and phys objects...
		if ( (pOther->IsNPC()) ||
			FClassnameIs(pOther, "func_pushable") ||
			(pOther->GetMoveType() == MOVETYPE_VPHYSICS))
		{
			// Anything except players tries to fade out into the clouds/void first
			m_bHasStuff = true;

			// You have one second to fade until deletion, get to it!
			pOther->SUB_FadeOut();	//SUB_StartFadeOutInstant

			// If our cleanup time has elapsed, just delete everything
			if (m_fCleanupTime <= gpGlobals->curtime)
			{
				m_bHasStuff = false;
			//	m_RemovedObject.FireOutput( pOther, this );
				UTIL_Remove( pOther );
			}
		}
	}

	// Fire our output
	m_OnFallingObject.FireOutput( pOther, this );
}

//-----------------------------------------------------------------------------
// Purpose: Reset cleanup timer
//-----------------------------------------------------------------------------
void CTriggerFall::FallThink( void )
{
	// If I (DONT) have stuff in me, refresh everything
	if ( !m_bHasStuff )
	{
		m_fCleanupTime = gpGlobals->curtime + 1;
		m_bDidDamage = false;
	}

	SetNextThink( gpGlobals->curtime + 0.5f );	//Lazy thinking, this trigger doesnt have to fire often
}
