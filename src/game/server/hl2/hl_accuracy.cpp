//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Spread system
//
// TODO's; implement this with just the movement modifers
//
//=============================================================================
#include "cbase.h"
#include "player.h"
#include "hl2_player.h"
#include "basecombatweapon.h"
#include "vstdlib/random.h"


// Accuracy is measured as the weapons spread in inches at 1024 units (~85 feet)
// Accuracy is sent to the client, where it's used to generate the size of the accuracy representation.
// Accuracy is a "floating" value, in that it's always moving towards a target accuracy, and takes some time to change

// Spread Multipliers
// < 1 increases accuracy, > 1 decreases
#define ACCMULT_DUCKING				0.75			// Player is ducking
#define ACCMULT_WALKING				0.85			// Player is walking/crouchwalking
#define ACCMULT_RUNNING				1.0				// Player is moving normally
#define ACCMULT_SPRINTING			1.5				// Player is sprinting

// Ricochet Multiplier
// This works differently to other acc multipliers.
#define ACCMULT_RICOCHET			1.0				// Player is being suppressed by bullet fire
#define ACC_RICOCHET_TIME			1.0				// Amount of time accuracy is affected by a ricochet near the player
#define ACC_RICOCHET_MULTIPLE		0.25			// The effect of ricochets on accuracy is multiplied by this by the number of ricochets nearby
#define ACC_RICOCHET_CAP			10				// Maximum number of bullets to register for suppression fire

#define ACCURACY_CHANGE_SPEED		5

//-----------------------------------------------------------------------------
// Purpose: Calculates the players "accuracy" level
//-----------------------------------------------------------------------------
void CHL2_Player::CalculateAccuracy( void )
{
	static flLastTime = 0;

	// Get the time since the last calculation
	float flTimeSlice = (gpGlobals->curtime - flLastTime);
	m_flTargetAccuracy = 0;

	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	if ( pPlayer == NULL )
		return;

	// TODO; This only works for hitscans, might need a case here for projectile based weapons,
	// like rocket-launchers and such

	// Get the base accuracy from the current weapon
	if ( m_hActiveWeapon )
	{
		m_flTargetAccuracy = m_hActiveWeapon->GetBulletSpread();

		// Spread is decreased if the player's crouching
		if ( GetFlags() & FL_DUCKING )
			m_flTargetAccuracy *= ACCMULT_DUCKING;

		// Spread is decreased if the player's walking
		if ( m_vecVelocity.Length2D() <= HL2_WALK_SPEED )
			m_flTargetAccuracy *= ACCMULT_WALKING;

#if 0
		// Spread is increased if the player's moving
		if ( m_vecVelocity.Length2D() >= HL2_NORM_SPEED )
			m_flTargetAccuracy *= ACCMULT_RUNNING;
#endif

		// Spread is heavily increased if the player's sprinting or otherwise flying fast
		if ( m_vecVelocity.Length2D() >= HL2_SPRINT_SPEED )
			m_flTargetAccuracy *= ACCMULT_SPRINTING;
	}

	// Spread is increased if this player's being supressed (bullets/explosions impacting nearby)
	float flFarTime = (m_flLastRicochetNearby + ACC_RICOCHET_TIME);
	if ( gpGlobals->curtime <= flFarTime )
		m_flTargetAccuracy *= 1 + (m_flNumberOfRicochets * ACC_RICOCHET_MULTIPLE) * (ACCMULT_RICOCHET * ((flFarTime - gpGlobals->curtime) / ACC_RICOCHET_TIME));

	// Now float towards the target accuracy
	if ( m_bSnapAccuracy )
	{
		m_bSnapAccuracy = false;
		m_flAccuracy = m_flTargetAccuracy;
	}
	else
	{
		if ( m_flAccuracy < m_flTargetAccuracy )
		{
			m_flAccuracy += (flTimeSlice * ACCURACY_CHANGE_SPEED);
			if ( m_flAccuracy > m_flTargetAccuracy )
				m_flAccuracy = m_flTargetAccuracy ;
		}
		else if ( m_flAccuracy > m_flTargetAccuracy )
		{
			m_flAccuracy -= (flTimeSlice * ACCURACY_CHANGE_SPEED);
			if ( m_flAccuracy < m_flTargetAccuracy )
				m_flAccuracy = m_flTargetAccuracy ;
		}
	}

	// Clip to prevent silly accuracies
	if ( m_flAccuracy > 1024 )
		m_flAccuracy = 1024;

	flLastTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Snap the players accuracy immediately
//-----------------------------------------------------------------------------
void CHL2_Player::SnapAccuracy( void )
{
	m_bSnapAccuracy = true;
}

//-----------------------------------------------------------------------------
// Purpose: Return the player's current accuracy
//-----------------------------------------------------------------------------
float CHL2_Player::GetAccuracy( void )
{
	return m_flAccuracy;
}

//-----------------------------------------------------------------------------
// Purpose: Bullets/Explosions are hitting near the player. Reduce thou accuracy
//-----------------------------------------------------------------------------
void CHL2_Player::Suppress( void )
{
	if ( gpGlobals->curtime <= (m_flLastRicochetNearby + ACC_RICOCHET_TIME) )
	{
		m_flNumberOfRicochets = min( ACC_RICOCHET_CAP, m_flNumberOfRicochets + 1 );
	}
	else
	{
		m_flNumberOfRicochets = 1;
	}

	m_flLastRicochetNearby = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector CHL2_Player::GenerateFireVector( Vector *viewVector )
{
	// Calculate the weapon spread from the player's accuracy
	float flAcc = (GetAccuracy() * 0.5) / ACCURACY_DISTANCE;
	float flAccuracyAngle = RAD2DEG( atan( flAcc ) );
	// If the user passed in a viewVector, use it, otherwise use player's v_angle
	Vector angShootAngles = viewVector ? *viewVector : pl->v_angle;
	if ( flAccuracyAngle )
	{
		float x, y, z;
		do {
			x = random->RandomFloat(-0.5,0.5) + random->RandomFloat(-0.5,0.5);
			y = random->RandomFloat(-0.5,0.5) + random->RandomFloat(-0.5,0.5);
			z = x*x+y*y;
		} while (z > 1);

		angShootAngles.x = UTIL_AngleMod( angShootAngles.x + (x * flAccuracyAngle) );
		angShootAngles.y = UTIL_AngleMod( angShootAngles.y + (y * flAccuracyAngle) );
	}
	
	Vector forward;
	AngleVectors( angShootAngles, &forward );
	return forward;
}
