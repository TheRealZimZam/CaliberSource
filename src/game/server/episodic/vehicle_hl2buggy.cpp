//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
//
//
//=============================================================================

#include "cbase.h"
#include "vehicle_hl2buggy.h"
#include "collisionutils.h"
#include "npc_alyx_episodic.h"
#include "particle_parse.h"
#include "particle_system.h"
#include "hl2_player.h"
#include "in_buttons.h"
#include "vphysics/friction.h"
#include "vphysicsupdateai.h"
#include "physics_npc_solver.h"
#include "sprite.h"
#include "props.h"
#include "ai_dynamiclink.h"

extern ConVar phys_upimpactforcescale;

#define JEEP_AMMOCRATE_HITGROUP		5
#define	JEEP_AMMO_CRATE_CLOSE_DELAY	2.0f



//=========================================================
//=========================================================

//
//	CPropHL2Buggy
//

LINK_ENTITY_TO_CLASS( prop_vehicle_hl2buggy, CPropHL2Buggy );

BEGIN_DATADESC( CPropHL2Buggy )

	DEFINE_FIELD( m_bEntranceLocked, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bExitLocked, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bBlink, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hLinkControllerFront, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hLinkControllerRear, FIELD_EHANDLE ),
	// m_flNextAvoidBroadcastTime
	DEFINE_FIELD( m_flNextDust, FIELD_TIME ),
	DEFINE_FIELD( m_flNextWaterSound, FIELD_TIME ),
	
	DEFINE_OUTPUT( m_OnCompanionEnteredVehicle, "OnCompanionEnteredVehicle" ),
	DEFINE_OUTPUT( m_OnCompanionExitedVehicle, "OnCompanionExitedVehicle" ),
	DEFINE_OUTPUT( m_OnHostileEnteredVehicle, "OnHostileEnteredVehicle" ),
	DEFINE_OUTPUT( m_OnHostileExitedVehicle, "OnHostileExitedVehicle" ),
	
	DEFINE_INPUTFUNC( FIELD_VOID, "LockEntrance",				InputLockEntrance ),
	DEFINE_INPUTFUNC( FIELD_VOID, "UnlockEntrance",				InputUnlockEntrance ),
	DEFINE_INPUTFUNC( FIELD_VOID, "LockExit",					InputLockExit ),
	DEFINE_INPUTFUNC( FIELD_VOID, "UnlockExit",					InputUnlockExit ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DisablePhysGun",				InputDisablePhysGun ),
	DEFINE_INPUTFUNC( FIELD_VOID, "EnablePhysGun",				InputEnablePhysGun ),
	DEFINE_INPUTFUNC( FIELD_VOID, "CreateLinkController",		InputCreateLinkController ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DestroyLinkController",		InputDestroyLinkController ),
END_DATADESC();

//=============================================================================
// Episodic jeep

CPropHL2Buggy::CPropHL2Buggy( void ) : 
m_bEntranceLocked( false ),
m_bExitLocked( false ),
m_flNextAvoidBroadcastTime( 0.0f )
{
	m_bHasGun = true;
	m_bUnableToFire = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropHL2Buggy::Precache( void )
{
	PrecacheScriptSound( "Water.WaterSplash" );

//	PrecacheParticleSystem( "WheelDust" );
//	PrecacheParticleSystem( "WheelSplash" );	//CUT; Moving physics objects already create a splash

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropHL2Buggy::Spawn( void )
{
	BaseClass::Spawn();

	SetBlocksLOS( false );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CPropHL2Buggy::Activate()
{
	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropHL2Buggy::NPC_FinishedEnterVehicle( CAI_BaseNPC *pPassenger, bool bCompanion )
{
	// FIXME: This will be moved to the NPCs entering and exiting
	// Fire our outputs
	if ( bCompanion	)
	{
		m_OnCompanionEnteredVehicle.FireOutput( this, pPassenger );
	}
	else
	{
		m_OnHostileEnteredVehicle.FireOutput( this, pPassenger );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropHL2Buggy::NPC_FinishedExitVehicle( CAI_BaseNPC *pPassenger, bool bCompanion )
{
	// FIXME: This will be moved to the NPCs entering and exiting
	// Fire our outputs
	if ( bCompanion	)
	{
		m_OnCompanionExitedVehicle.FireOutput( this, pPassenger );
	}
	else
	{
		m_OnHostileExitedVehicle.FireOutput( this, pPassenger );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPassenger - 
//			bCompanion - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPropHL2Buggy::NPC_CanEnterVehicle( CAI_BaseNPC *pPassenger, bool bCompanion )
{
	// Must be unlocked
	if ( bCompanion && m_bEntranceLocked )
		return false;

	return BaseClass::NPC_CanEnterVehicle( pPassenger, bCompanion );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPassenger - 
//			bCompanion - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPropHL2Buggy::NPC_CanExitVehicle( CAI_BaseNPC *pPassenger, bool bCompanion )
{
	// Must be unlocked
	if ( bCompanion && m_bExitLocked )
		return false;

	return BaseClass::NPC_CanExitVehicle( pPassenger, bCompanion );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropHL2Buggy::InputLockEntrance( inputdata_t &data )
{
	m_bEntranceLocked = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropHL2Buggy::InputUnlockEntrance( inputdata_t &data )
{
	m_bEntranceLocked = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropHL2Buggy::InputLockExit( inputdata_t &data )
{
	m_bExitLocked = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropHL2Buggy::InputUnlockExit( inputdata_t &data )
{
	m_bExitLocked = false;
}

//-----------------------------------------------------------------------------
// Purpose: Override velocity if our passenger is transitioning or we're upside-down
//-----------------------------------------------------------------------------
Vector CPropHL2Buggy::PhysGunLaunchVelocity( const Vector &forward, float flMass )
{
	Vector vecPuntDir = BaseClass::PhysGunLaunchVelocity( forward, flMass );
	vecPuntDir.z = 150.0f;
	vecPuntDir *= 600.0f;
	return vecPuntDir;
}

//-----------------------------------------------------------------------------
// Purpose: Rolls the vehicle when its trying to upright itself from a punt
//-----------------------------------------------------------------------------
AngularImpulse CPropHL2Buggy::PhysGunLaunchAngularImpulse( void ) 
{ 
	if ( IsOverturned() )
		return AngularImpulse( 0, 300, 0 );

	// Don't spin randomly, always spin reliably
	return AngularImpulse( 0, 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: If the player uses the jeep while at the back, he gets ammo from the crate instead
//-----------------------------------------------------------------------------
void CPropHL2Buggy::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Fall back and get in the vehicle instead, skip giving ammo
	BaseClass::BaseClass::Use( pActivator, pCaller, useType, value );
}

#define	MIN_WHEEL_DUST_SPEED	9

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropHL2Buggy::UpdateWheelDust( void )
{
	// See if this wheel should emit dust
	const vehicleparams_t *vehicleData = m_pServerVehicle->GetVehicleParams();
	const vehicle_operatingparams_t *carState = m_pServerVehicle->GetVehicleOperatingParams();
	bool bAllowDust = vehicleData->steering.dustCloud;
	
	// Car must be active
//!	bool bCarOn = m_VehiclePhysics.IsOn();
//! Achtually no, if the player bails at mach5 we want dust to still generate

	// Must be moving quickly enough or skidding along the ground
	bool bCreateDust = ( //!bCarOn &&
						 bAllowDust && 
					   ( m_VehiclePhysics.GetSpeed() >= MIN_WHEEL_DUST_SPEED || carState->skidSpeed > DEFAULT_SKID_THRESHOLD ) );

	// Update our wheel dust
	Vector	vecPos;
	for ( int iWheel = 0; iWheel < NUM_WHEEL_EFFECTS; iWheel++ )
	{
		m_pServerVehicle->GetWheelContactPoint( iWheel, vecPos );
		if ( bCreateDust )
		{
			if ( m_WaterData.m_bWheelInWater[iWheel] )
			{
				// Set us up in the right position
				UTIL_Bubbles( vecPos + Vector( -2, -2, -2 ), vecPos + Vector( 2, 2, 2 ), random->RandomInt( 4, 8 ) );

				if ( m_WaterData.m_flNextRippleTime[iWheel] < gpGlobals->curtime )
				{
					// Stagger ripple times
					m_WaterData.m_flNextRippleTime[iWheel] = gpGlobals->curtime + RandomFloat( 0.1, 0.3 );
					CreateSplash( m_WaterData.m_vecWheelContactPoints[iWheel] );
					CreateRipple( m_WaterData.m_vecWheelContactPoints[iWheel] );
				}

				if ( m_flNextWaterSound < gpGlobals->curtime )
				{
					m_flNextWaterSound = gpGlobals->curtime + random->RandomFloat( 0.25f, 1.0f );
					EmitSound( "Water.WaterSplash" );
				}
				return;
			}
			else
			{
				float fldustscale = m_VehiclePhysics.GetSpeed() * random->RandomFloat( 0.1f, 0.25f );
				fldustscale = clamp( fldustscale, 0.1f, 10.0f );

				UTIL_Dust( vecPos, 0, fldustscale, fldustscale );
			}
		}
	}
}

#define VEHICLE_AVOID_BROADCAST_RATE	0.5f

//-----------------------------------------------------------------------------
// Purpose: This function isn't really what we want
//-----------------------------------------------------------------------------
void CPropHL2Buggy::CreateAvoidanceZone( void )
{
	if ( m_flNextAvoidBroadcastTime > gpGlobals->curtime )
		return;

	// Only do this when we're stopped
	if ( m_VehiclePhysics.GetSpeed() > 5.0f )
		return;

	float flHullRadius = CollisionProp()->BoundingRadius2D();
	
	Vector	vecPos;
	CollisionProp()->NormalizedToWorldSpace( Vector( 0.5f, 0.33f, 0.25f ), &vecPos );
	CSoundEnt::InsertSound( SOUND_MOVE_AWAY, vecPos, (flHullRadius*0.4f), VEHICLE_AVOID_BROADCAST_RATE, this );
	// NDebugOverlay::Sphere( vecPos, vec3_angle, flHullRadius*0.4f, 255, 0, 0, 0, true, VEHICLE_AVOID_BROADCAST_RATE );

	CollisionProp()->NormalizedToWorldSpace( Vector( 0.5f, 0.66f, 0.25f ), &vecPos );
	CSoundEnt::InsertSound( SOUND_MOVE_AWAY, vecPos, (flHullRadius*0.4f), VEHICLE_AVOID_BROADCAST_RATE, this );
	// NDebugOverlay::Sphere( vecPos, vec3_angle, flHullRadius*0.4f, 255, 0, 0, 0, true, VEHICLE_AVOID_BROADCAST_RATE );

	// Don't broadcast again until these are done
	m_flNextAvoidBroadcastTime = gpGlobals->curtime + VEHICLE_AVOID_BROADCAST_RATE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropHL2Buggy::Think( void )
{
	BaseClass::Think();

	// If our passenger is transitioning, then don't let the player drive off
	CNPC_Alyx *pAlyx = CNPC_Alyx::GetAlyx();
	if ( pAlyx && pAlyx->GetPassengerState() == PASSENGER_STATE_EXITING )
	{
		m_throttleDisableTime = gpGlobals->curtime + 0.25f;		
	}

	// See if the wheel dust should be on or off
	if ( m_flNextDust < gpGlobals->curtime )
	{
		UpdateWheelDust();
		m_flNextDust = gpGlobals->curtime + 0.1f;	
	}

	CreateAvoidanceZone();
}

// adds a collision solver for any small props that are stuck under the vehicle
static void SolveBlockingProps( CPropHL2Buggy *pVehicleEntity, IPhysicsObject *pVehiclePhysics )
{
	CUtlVector<CBaseEntity *> solveList;
	float vehicleMass = pVehiclePhysics->GetMass();
	Vector vehicleUp;
	pVehicleEntity->GetVectors( NULL, NULL, &vehicleUp );
	IPhysicsFrictionSnapshot *pSnapshot = pVehiclePhysics->CreateFrictionSnapshot();
	while ( pSnapshot->IsValid() )
	{
		IPhysicsObject *pOther = pSnapshot->GetObject(1);
		float otherMass = pOther->GetMass();
		CBaseEntity *pOtherEntity = static_cast<CBaseEntity *>(pOther->GetGameData());
		Assert(pOtherEntity);
		if ( pOtherEntity && pOtherEntity->GetMoveType() == MOVETYPE_VPHYSICS && pOther->IsMoveable() && (otherMass*4.0f) < vehicleMass )
		{
			Vector normal;
			pSnapshot->GetSurfaceNormal(normal);
			// this points down in the car's reference frame, then it's probably trapped under the car
			if ( DotProduct(normal, vehicleUp) < -0.9f )
			{
				Vector point, pointLocal;
				pSnapshot->GetContactPoint(point);
				VectorITransform( point, pVehicleEntity->EntityToWorldTransform(), pointLocal );
				Vector bottomPoint = physcollision->CollideGetExtent( pVehiclePhysics->GetCollide(), vec3_origin, vec3_angle, Vector(0,0,-1) );
				// make sure it's under the bottom of the car
				float bottomPlane = DotProduct(bottomPoint,vehicleUp)+8;	// 8 inches above bottom
				if ( DotProduct( pointLocal, vehicleUp ) <= bottomPlane )
				{
					//Msg("Solved %s\n", pOtherEntity->GetClassname());
					if ( solveList.Find(pOtherEntity) < 0 )
					{
						solveList.AddToTail(pOtherEntity);
					}
				}
			}
		}
		pSnapshot->NextFrictionData();
	}
	pVehiclePhysics->DestroyFrictionSnapshot( pSnapshot );
	if ( solveList.Count() )
	{
		for ( int i = 0; i < solveList.Count(); i++ )
		{
			EntityPhysics_CreateSolver( pVehicleEntity, solveList[i], true, 4.0f );
		}
		pVehiclePhysics->RecheckContactPoints();
	}
}

static void SimpleCollisionResponse( Vector velocityIn, const Vector &normal, float coefficientOfRestitution, Vector *pVelocityOut )
{
	Vector Vn = DotProduct(velocityIn,normal) * normal;
	Vector Vt = velocityIn - Vn;
	*pVelocityOut = Vt - coefficientOfRestitution * Vn;
}

void CPropHL2Buggy::DriveVehicle( float flFrameTime, CUserCmd *ucmd, int iButtonsDown, int iButtonsReleased )
{
	if ( ucmd->impulse == 100 )
	{
		if (HeadlightIsOn())
		{
			HeadlightTurnOff();
		}
		else 
		{
			HeadlightTurnOn();
		}
	}

	BaseClass::DriveVehicle(flFrameTime, ucmd, iButtonsDown, iButtonsReleased);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nRole - 
//-----------------------------------------------------------------------------
void CPropHL2Buggy::ExitVehicle( int nRole )
{
	BaseClass::ExitVehicle( nRole );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropHL2Buggy::HandleWater( void )
{
	// Only check the wheels and engine in water if we have a driver (player).
	if ( !GetDriver() )
		return;

	// Update our internal state
	CheckWater();

	// Save of data from last think.
	for ( int iWheel = 0; iWheel < JEEP_WHEEL_COUNT; ++iWheel )
	{
		m_WaterData.m_bWheelWasInWater[iWheel] = m_WaterData.m_bWheelInWater[iWheel];
	}
}

//-----------------------------------------------------------------------------
// Purpose: Report our lock state
//-----------------------------------------------------------------------------
int	CPropHL2Buggy::DrawDebugTextOverlays( void )
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if ( m_debugOverlays & OVERLAY_TEXT_BIT )
	{
		EntityText( text_offset, CFmtStr("Entrance: %s", m_bEntranceLocked ? "Locked" : "Unlocked" ), 0 );
		text_offset++;

		EntityText( text_offset, CFmtStr("Exit: %s", m_bExitLocked ? "Locked" : "Unlocked" ), 0 );
		text_offset++;
	}

	return text_offset;
}

//-----------------------------------------------------------------------------
// Purpose: Stop players punting the car around.
//-----------------------------------------------------------------------------
void CPropHL2Buggy::InputDisablePhysGun( inputdata_t &data )
{
	AddEFlags( EFL_NO_PHYSCANNON_INTERACTION );
}
//-----------------------------------------------------------------------------
// Purpose: Return to normal
//-----------------------------------------------------------------------------
void CPropHL2Buggy::InputEnablePhysGun( inputdata_t &data )
{
	RemoveEFlags( EFL_NO_PHYSCANNON_INTERACTION );
}

//-----------------------------------------------------------------------------
// Create and parent two radial node link controllers.
//-----------------------------------------------------------------------------
void CPropHL2Buggy::InputCreateLinkController( inputdata_t &data )
{
	Vector vecFront, vecRear;
	Vector vecWFL, vecWFR;	// Front wheels
	Vector vecWRL, vecWRR;	// Back wheels

	GetAttachment( "wheel_fr", vecWFR );
	GetAttachment( "wheel_fl", vecWFL );

	GetAttachment( "wheel_rr", vecWRR );
	GetAttachment( "wheel_rl", vecWRL );

	vecFront = (vecWFL + vecWFR) * 0.5f;
	vecRear = (vecWRL + vecWRR) * 0.5f;

	float flRadius = ( (vecFront - vecRear).Length() ) * 0.6f;

	CAI_RadialLinkController *pLinkController = (CAI_RadialLinkController *)CreateEntityByName( "info_radial_link_controller" );
	if( pLinkController != NULL && m_hLinkControllerFront.Get() == NULL )
	{
		pLinkController->m_flRadius = flRadius;
		pLinkController->Spawn();
		pLinkController->SetAbsOrigin( vecFront );
		pLinkController->SetOwnerEntity( this );
		pLinkController->SetParent( this );
		pLinkController->Activate();
		m_hLinkControllerFront.Set( pLinkController );

		//NDebugOverlay::Circle( vecFront, Vector(0,1,0), Vector(1,0,0), flRadius, 255, 255, 255, 128, false, 100 );
	}

	pLinkController = (CAI_RadialLinkController *)CreateEntityByName( "info_radial_link_controller" );
	if( pLinkController != NULL && m_hLinkControllerRear.Get() == NULL  )
	{
		pLinkController->m_flRadius = flRadius;
		pLinkController->Spawn();
		pLinkController->SetAbsOrigin( vecRear );
		pLinkController->SetOwnerEntity( this );
		pLinkController->SetParent( this );
		pLinkController->Activate();
		m_hLinkControllerRear.Set( pLinkController );

		//NDebugOverlay::Circle( vecRear, Vector(0,1,0), Vector(1,0,0), flRadius, 255, 255, 255, 128, false, 100 );
	}
}

void CPropHL2Buggy::InputDestroyLinkController( inputdata_t &data )
{
	if( m_hLinkControllerFront.Get() != NULL )
	{
		CAI_RadialLinkController *pLinkController = dynamic_cast<CAI_RadialLinkController*>(m_hLinkControllerFront.Get());
		if( pLinkController != NULL )
		{
			pLinkController->ModifyNodeLinks(false);
			UTIL_Remove( pLinkController );
			m_hLinkControllerFront.Set(NULL);
		}
	}

	if( m_hLinkControllerRear.Get() != NULL )
	{
		CAI_RadialLinkController *pLinkController = dynamic_cast<CAI_RadialLinkController*>(m_hLinkControllerRear.Get());
		if( pLinkController != NULL )
		{
			pLinkController->ModifyNodeLinks(false);
			UTIL_Remove( pLinkController );
			m_hLinkControllerRear.Set(NULL);
		}
	}
}

bool CPropHL2Buggy::AllowBlockedExit( CBaseCombatCharacter *pPassenger, int nRole )
{
	// Wait until we've settled down before we resort to blocked exits.
	// This keeps us from doing blocked exits in mid-jump, which can cause mayhem like
	// sticking the player through player clips or into geometry.
	return GetSmoothedVelocity().IsLengthLessThan( 50 );
}

