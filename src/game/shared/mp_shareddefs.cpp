//====== Copyright � 1996-2007, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "mp_shareddefs.h"

const char *g_pszMPConcepts[] =
{
	"TLK_FIREWEAPON",				// MP_CONCEPT_FIREWEAPON
	"TLK_HURT",						// MP_CONCEPT_HURT
	"TLK_PLAYER_EXPRESSION",		// MP_CONCEPT_PLAYER_EXPRESSION
	"TLK_WINDMINIGUN",				// MP_CONCEPT_WINDMINIGUN
	"TLK_FIREMINIGUN",				// MP_CONCEPT_FIREMINIGUN
	"TLK_PLAYER_MEDIC",				// MP_CONCEPT_PLAYER_MEDIC
	"TLK_DETONATED_OBJECT",			// MP_CONCEPT_DETONATED_OBJECT 
	"TLK_KILLED_PLAYER",			// MP_CONCEPT_KILLED_PLAYER
	"TLK_KILLED_OBJECT",			// MP_CONCEPT_KILLED_OBJECT
	"TLK_PLAYER_PAIN",				// MP_CONCEPT_PLAYER_PAIN
	"TLK_PLAYER_ATTACKER_PAIN",		// MP_CONCEPT_PLAYER_ATTACKER_PAIN
	"TLK_PLAYER_TAUNT",				// MP_CONCEPT_PLAYER_TAUNT
	"TLK_PLAYER_HELP",				// MP_CONCEPT_PLAYER_HELP
	"TLK_PLAYER_GO",				// MP_CONCEPT_PLAYER_GO
	"TLK_PLAYER_STOP",				// MP_CONCEPT_PLAYER_STOP
	"TLK_PLAYER_MOVEUP",			// MP_CONCEPT_PLAYER_MOVEUP
	"TLK_PLAYER_LEFT",				// MP_CONCEPT_PLAYER_LEFT
	"TLK_PLAYER_RIGHT",				// MP_CONCEPT_PLAYER_RIGHT
	"TLK_PLAYER_YES",				// MP_CONCEPT_PLAYER_YES
	"TLK_PLAYER_NO",				// MP_CONCEPT_PLAYER_NO
	"TLK_PLAYER_INCOMING",			// MP_CONCEPT_PLAYER_INCOMING
	"TLK_PLAYER_CLOAKEDSPY",		// MP_CONCEPT_PLAYER_CLOAKEDSPY
	"TLK_PLAYER_SENTRYAHEAD",		// MP_CONCEPT_PLAYER_SENTRYAHEAD
	"TLK_PLAYER_TELEPORTERHERE",	// MP_CONCEPT_PLAYER_TELEPORTERHERE
	"TLK_PLAYER_DISPENSERHERE",		// MP_CONCEPT_PLAYER_DISPENSERHERE
	"TLK_PLAYER_SENTRYHERE",		// MP_CONCEPT_PLAYER_SENTRYHERE
	"TLK_PLAYER_ACTIVATECHARGE",	// MP_CONCEPT_PLAYER_ACTIVATECHARGE
	"TLK_PLAYER_CHARGEREADY",		// MP_CONCEPT_PLAYER_CHARGEREADY
	"TLK_PLAYER_TAUNT"	,			// MP_CONCEPT_PLAYER_TAUNT
	"TLK_PLAYER_BATTLECRY",			// MP_CONCEPT_PLAYER_BATTLECRY
	"TLK_PLAYER_CHEERS",			// MP_CONCEPT_PLAYER_CHEERS
	"TLK_PLAYER_JEERS",				// MP_CONCEPT_PLAYER_JEERS
	"TLK_PLAYER_POSITIVE",			// MP_CONCEPT_PLAYER_POSITIVE
	"TLK_PLAYER_NEGATIVE",			// MP_CONCEPT_PLAYER_NEGATIVE
	"TLK_PLAYER_NICESHOT",			// MP_CONCEPT_PLAYER_NICESHOT
	"TLK_PLAYER_GOODJOB",			// MP_CONCEPT_PLAYER_GOODJOB
	"TLK_MEDIC_STARTEDHEALING",		// MP_CONCEPT_MEDIC_STARTEDHEALING
	"TLK_MEDIC_CHARGEREADY",		// MP_CONCEPT_MEDIC_CHARGEREADY
	"TLK_MEDIC_STOPPEDHEALING",		// MP_CONCEPT_MEDIC_STOPPEDHEALING
	"TLK_MEDIC_CHARGEDEPLOYED",		// MP_CONCEPT_MEDIC_CHARGEDEPLOYED
	"TLK_FLAGPICKUP",				// MP_CONCEPT_FLAGPICKUP
	"TLK_FLAGCAPTURED",				// MP_CONCEPT_FLAGCAPTURED
	"TLK_ROUND_START",				// MP_CONCEPT_ROUND_START
	"TLK_SUDDENDEATH_START",		// MP_CONCEPT_SUDDENDEATH_START
	"TLK_ONFIRE",					// MP_CONCEPT_ONFIRE
	"TLK_STALEMATE",				// MP_CONCEPT_STALEMATE
	"TLK_BUILDING_OBJECT",			// MP_CONCEPT_BUILDING_OBJECT
	"TLK_LOST_OBJECT",				// MP_CONCEPT_LOST_OBJECT
	"TLK_SPY_SAPPER",				// MP_CONCEPT_SPY_SAPPER
	"TLK_TELEPORTED",				// MP_CONCEPT_TELEPORTED
	"TLK_LOST_CONTROL_POINT",		// MP_CONCEPT_LOST_CONTROL_POINT
	"TLK_CAPTURED_POINT",			// MP_CONCEPT_CAPTURED_POINT
	"TLK_CAPTURE_BLOCKED",			// MP_CONCEPT_CAPTURE_BLOCKED
	"TLK_HEALTARGET_STARTEDHEALING",// MP_CONCEPT_HEALTARGET_STARTEDHEALING
	"TLK_HEALTARGET_CHARGEREADY",	// MP_CONCEPT_HEALTARGET_CHARGEREADY
	"TLK_HEALTARGET_STOPPEDHEALING",// MP_CONCEPT_HEALTARGET_STOPPEDHEALING
	"TLK_HEALTARGET_CHARGEDEPLOYED",// MP_CONCEPT_HEALTARGET_CHARGEDEPLOYED
	"TLK_MINIGUN_FIREWEAPON",		// MP_CONCEPT_MINIGUN_FIREWEAPON
	"TLK_DIED",						// MP_CONCEPT_DIED
	"TLK_PLAYER_THANKS",			// MP_CONCEPT_PLAYER_THANKS
	"TLK_CART_MOVING_FORWARD",		// MP_CONCEPT_CART_MOVING_FORWARD
	"TLK_CART_MOVING_BACKWARD",		// MP_CONCEPT_CART_MOVING_BACKWARD
	"TLK_CART_STOP",				// MP_CONCEPT_CART_STOP
	"TLK_ATE_FOOD",					// MP_CONCEPT_ATE_FOOD
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int GetMPConceptIndexFromString( const char *pszConcept )
{
	// Make sure our concept string and enum arrays are the same length
	Assert( ARRAYSIZE( g_pszMPConcepts ) == MP_TF_CONCEPT_COUNT );

	for ( int iConcept = 0; iConcept < ARRAYSIZE( g_pszMPConcepts ); ++iConcept )
	{
		if ( !Q_stricmp( pszConcept, g_pszMPConcepts[iConcept] ) )
			return iConcept;
	}

	return MP_CONCEPT_NONE;
}
