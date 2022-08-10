//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Game-specific impact effect hooks
//
//=============================================================================//
#include "cbase.h"
#include "fx_impact.h"
#include "fx.h"
#include "decals.h"
#include "fx_quad.h"
#include "fx_sparks.h"

#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Handle jeep impacts
//-----------------------------------------------------------------------------
void ImpactJeepCallback( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity )
	{
		// This happens for impacts that occur on an object that's then destroyed.
		// Clear out the fraction so it uses the server's data
		tr.fraction = 1.0;
		PlayImpactSound( pEntity, tr, vecOrigin, nSurfaceProp );
		return;
	}

	// If we hit, perform our custom effects and play the sound
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr ) )
	{
		// Check for custom effects based on the Decal index
		PerformCustomEffects( vecOrigin, tr, vecShotDir, iMaterial, 2 );
	}

	PlayImpactSound( pEntity, tr, vecOrigin, nSurfaceProp );
}

DECLARE_CLIENT_EFFECT( "ImpactJeep", ImpactJeepCallback );


//-----------------------------------------------------------------------------
// Purpose: Handle crowbar impacts
// TODO; Why is this in here??? Move this up a level, other games might
// want to use this
//-----------------------------------------------------------------------------
void MeleeImpactCallback( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity )
	{
		// This happens for impacts that occur on an object that's then destroyed.
		// Clear out the fraction so it uses the server's data
		tr.fraction = 1.0;
		PlayImpactSound( pEntity, tr, vecOrigin, nSurfaceProp );
		return;
	}

	// If we hit, perform our custom effects and play the sound
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr, IMPACT_NODECAL ) )
	{
		// Check for custom effects based on the Decal index
		PerformCustomEffects( vecOrigin, tr, vecShotDir, iMaterial, 1 );
	}

	PlayImpactSound( pEntity, tr, vecOrigin, nSurfaceProp );
}

DECLARE_CLIENT_EFFECT( "MeleeImpact", MeleeImpactCallback );

//-----------------------------------------------------------------------------
// Purpose: Creates the AR2 impact fx
// Input  : &data - 
//-----------------------------------------------------------------------------
void AR2ImpactCallback( const CEffectData &data )
{
	// Fast Lines
	Vector vecNormal;
	Vector vecPosition;
	vecPosition = data.m_vOrigin;
	vecNormal = data.m_vNormal;

#ifdef _XBOX
	// Impact sprite
	FX_AddQuad( vecPosition, 
				vecNormal, 
				random->RandomFloat( 24, 32 ),
				0,
				0.75f, 
				1.0f,
				0.0f,
				0.4f,
				random->RandomInt( 0, 360 ), 
				0,
				Vector( 1.0f, 1.0f, 1.0f ), 
				0.25f,
				"effects/ar2_impact",
				(FXQUAD_BIAS_SCALE|FXQUAD_BIAS_ALPHA) );
#else
	FX_MicroExplosion( vecPosition, vecNormal );
#endif
}

DECLARE_CLIENT_EFFECT( "AR2Impact", AR2ImpactCallback );

//-----------------------------------------------------------------------------
// Purpose: Handle gauss impacts
//-----------------------------------------------------------------------------
void ImpactGaussCallback( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity )
	{
		// This happens for impacts that occur on an object that's then destroyed.
		// Clear out the fraction so it uses the server's data
		tr.fraction = 1.0;
		PlayImpactSound( pEntity, tr, vecOrigin, nSurfaceProp );
		return;
	}

	// If we hit, perform our custom effects and play the sound
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr ) )
	{
		// Check for custom effects based on the Decal index
		PerformCustomEffects( vecOrigin, tr, vecShotDir, iMaterial, 2 );
	}

	PlayImpactSound( pEntity, tr, vecOrigin, nSurfaceProp );
}

DECLARE_CLIENT_EFFECT( "ImpactGauss", ImpactGaussCallback );

//-----------------------------------------------------------------------------
// Purpose: Handle weapon impacts
//-----------------------------------------------------------------------------
void ImpactCallback( const CEffectData &data )
{
	VPROF_BUDGET( "ImpactCallback", VPROF_BUDGETGROUP_PARTICLE_RENDERING );

	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity )
	{
		// This happens for impacts that occur on an object that's then destroyed.
		// Clear out the fraction so it uses the server's data
		tr.fraction = 1.0;
		PlayImpactSound( pEntity, tr, vecOrigin, nSurfaceProp );
		return;
	}

	// If we hit, perform our custom effects and play the sound
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr ) )
	{
		// Check for custom effects based on the Decal index
		PerformCustomEffects( vecOrigin, tr, vecShotDir, iMaterial, 1.0 );
	}

	PlayImpactSound( pEntity, tr, vecOrigin, nSurfaceProp );
}

DECLARE_CLIENT_EFFECT( "Impact", ImpactCallback );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&normal - 
//			scale - 
//-----------------------------------------------------------------------------
void FX_AirboatGunImpact( const Vector &origin, const Vector &normal, float scale )
{
	Vector offset = origin + ( normal * 1.0f );

	CSmartPtr<CTrailParticles> sparkEmitter = CTrailParticles::Create( "FX_AirboatGun 1" );

	if ( sparkEmitter == NULL )
		return;

	//Setup our information
	sparkEmitter->SetSortOrigin( offset );
	sparkEmitter->SetFlag( bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );
	sparkEmitter->SetVelocityDampen( 8.0f );
	sparkEmitter->SetGravity( 800.0f );
	sparkEmitter->SetCollisionDamped( 0.25f );
	sparkEmitter->GetBinding().SetBBox( offset - Vector( 32, 32, 32 ), offset + Vector( 32, 32, 32 ) );

	int	numSparks = random->RandomInt( 4, 8 );

	TrailParticle	*pParticle;
	PMaterialHandle	hMaterial = sparkEmitter->GetPMaterial( "effects/spark" );
	Vector			dir;

	float	length	= 0.1f;

	//Dump out sparks
	for ( int i = 0; i < numSparks; i++ )
	{
		pParticle = (TrailParticle *) sparkEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

		if ( pParticle == NULL )
			return;

		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= random->RandomFloat( 0.05f, 0.1f );

		float	spreadOfs = random->RandomFloat( 0.0f, 2.0f );

		dir[0] = normal[0] + random->RandomFloat( -(0.5f*spreadOfs), (0.5f*spreadOfs) );
		dir[1] = normal[1] + random->RandomFloat( -(0.5f*spreadOfs), (0.5f*spreadOfs) );
		dir[2] = normal[2] + random->RandomFloat( -(0.5f*spreadOfs), (0.5f*spreadOfs) );

		VectorNormalize( dir );

		pParticle->m_flWidth		= random->RandomFloat( 1.0f, 4.0f );
		pParticle->m_flLength		= random->RandomFloat( length*0.25f, length );

		pParticle->m_vecVelocity	= dir * random->RandomFloat( (128.0f*(2.0f-spreadOfs)), (512.0f*(2.0f-spreadOfs)) );

		Color32Init( pParticle->m_color, 255, 255, 255, 255 );
	}

#ifdef _XBOX
	// Add a quad to highlite the hit point
	FX_AddQuad( origin, 
				normal, 
				random->RandomFloat( 16, 32 ),
				random->RandomFloat( 32, 48 ),
				0.75f, 
				1.0f,
				0.0f,
				0.4f,
				random->RandomInt( 0, 360 ), 
				0,
				Vector( 1.0f, 1.0f, 1.0f ), 
				0.05f, 
				"effects/combinemuzzle2_nocull",
				(FXQUAD_BIAS_SCALE|FXQUAD_BIAS_ALPHA) );
#else
	CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "FX_AirboatGun 2" );
	pSimple->SetSortOrigin( offset );

	SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "effects/combinemuzzle2_nocull" ), offset );

	if ( sParticle )
	{
		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= 0.05f;
		
		sParticle->m_vecVelocity.Init();

		sParticle->m_uchColor[0]	= 255;
		sParticle->m_uchColor[1]	= 255;
		sParticle->m_uchColor[2]	= 255;
		sParticle->m_uchStartAlpha	= random->RandomInt( 224, 255 );
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random->RandomFloat( 16, 32 );
		sParticle->m_uchEndSize		= random->RandomFloat( 32, 48 );
		sParticle->m_flRoll			= random->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= 0.0f;
	}
#endif

}

//-----------------------------------------------------------------------------
// Purpose: Handle weapon impacts from the airboat gun shooting (cheaper versions)
//-----------------------------------------------------------------------------
void ImpactAirboatGunCallback( const CEffectData &data )
{
	VPROF_BUDGET( "ImpactAirboatGunCallback", VPROF_BUDGETGROUP_PARTICLE_RENDERING );

	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity )
	{
		// This happens for impacts that occur on an object that's then destroyed.
		// Clear out the fraction so it uses the server's data
		tr.fraction = 1.0;
		PlayImpactSound( pEntity, tr, vecOrigin, nSurfaceProp );
		return;
	}

#if !defined( _XBOX )
	// If we hit, perform our custom effects and play the sound. Don't create decals
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr, IMPACT_NODECAL | IMPACT_REPORT_RAGDOLL_IMPACTS ) )
	{
		FX_AirboatGunImpact( vecOrigin, tr.plane.normal, 2 );
	}
#else
	FX_AirboatGunImpact( vecOrigin, tr.plane.normal, 1 );
#endif
}

DECLARE_CLIENT_EFFECT( "AirboatGunImpact", ImpactAirboatGunCallback );


//-----------------------------------------------------------------------------
// Purpose: Handle weapon impacts from the helicopter shooting (cheaper versions)
//-----------------------------------------------------------------------------
void ImpactHelicopterCallback( const CEffectData &data )
{
	VPROF_BUDGET( "ImpactHelicopterCallback", VPROF_BUDGETGROUP_PARTICLE_RENDERING );

	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity )
	{
		// This happens for impacts that occur on an object that's then destroyed.
		// Clear out the fraction so it uses the server's data
		tr.fraction = 1.0;
		PlayImpactSound( pEntity, tr, vecOrigin, nSurfaceProp );
		return;
	}

	// If we hit, perform our custom effects and play the sound. Don't create decals
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr, IMPACT_NODECAL | IMPACT_REPORT_RAGDOLL_IMPACTS ) )
	{
		FX_AirboatGunImpact( vecOrigin, tr.plane.normal, IsXbox() ? 1 : 2 );

		// Only do metal + computer custom effects
		if ( (iMaterial == CHAR_TEX_METAL) || (iMaterial == CHAR_TEX_COMPUTER) )
		{
			PerformCustomEffects( vecOrigin, tr, vecShotDir, iMaterial, 1.0, FLAGS_CUSTIOM_EFFECTS_NOFLECKS );
		}
	}

	PlayImpactSound( pEntity, tr, vecOrigin, nSurfaceProp );
}

DECLARE_CLIENT_EFFECT( "HelicopterImpact", ImpactHelicopterCallback );

//-----------------------------------------------------------------------------
// Purpose: Creates the Shotgun impact fx
//-----------------------------------------------------------------------------
void ShotgunImpactCallback( const CEffectData &data )
{
	FX_AddQuad( data.m_vOrigin, 
				data.m_vNormal, 
				random->RandomFloat( 24, 32 ),
				0,
				0.75f, 
				1.0f,
				0.0f,
				0.4f,
				random->RandomInt( 0, 360 ), 
				0,
				Vector( 1.0f, 1.0f, 1.0f ), 
				0.25f, 
				"effects/combinemuzzle2_nocull",
				(FXQUAD_BIAS_SCALE|FXQUAD_BIAS_ALPHA) );
}

DECLARE_CLIENT_EFFECT( "ShotgunImpact", ShotgunImpactCallback );

