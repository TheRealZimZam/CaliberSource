//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Human gib fx
//
//=============================================================================//

#include "cbase.h"
#include "fx.h"
#include "c_gib.h"
#include "c_te_effect_dispatch.h"
#include "iefx.h"
#include "decals.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

PMaterialHandle	g_Material_Blood[2] = { NULL, NULL };

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//-----------------------------------------------------------------------------
void FX_Gib( const Vector &origin, const Vector &direction, float scale )
{
#ifdef _XBOX

	//
	// Throw some blood
	//

	CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "FX_Gib" );
	pSimple->SetSortOrigin( origin );
	pSimple->GetBinding().SetBBox( origin - Vector(64,64,64), origin + Vector(64,64,64) );

	// Cache this if we're not already
	if ( g_Material_Blood[0] == NULL )
	{
		g_Material_Blood[0] = g_Mat_BloodPuff[0];
	}
	
	if ( g_Material_Blood[1] == NULL )
	{
		g_Material_Blood[1] = g_Mat_BloodPuff[1];
	}

	Vector	vDir;
	vDir.Random( -1.0f, 1.0f );

	// Gore bits
	for ( int i = 0; i < 4; i++ )
	{
		SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Material_Blood[0], origin + RandomVector(-16,16));
		if ( sParticle == NULL )
			return;

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= random->RandomFloat( 0.25f, 0.5f );
			
		float speed = random->RandomFloat( 16.0f, 64.0f );

		sParticle->m_vecVelocity.Init();

		sParticle->m_uchColor[0]	= 128;
		sParticle->m_uchColor[1]	= 32;
		sParticle->m_uchColor[2]	= 32;
		sParticle->m_uchStartAlpha	= 255;
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random->RandomInt( 4, 16 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize * 4;
		sParticle->m_flRoll			= random->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= 0.0f;
	}

	// Middle core
	SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Material_Blood[1], origin );
	if ( sParticle == NULL )
		return;

	sParticle->m_flLifetime		= 0.0f;
	sParticle->m_flDieTime		= random->RandomFloat( 0.5f, 0.75f );

	float speed = random->RandomFloat( 16.0f, 64.0f );

	sParticle->m_vecVelocity	= vDir * -speed;
	sParticle->m_vecVelocity[2] += 16.0f;

	sParticle->m_uchColor[0]	= 128;
	sParticle->m_uchColor[1]	= 32;
	sParticle->m_uchColor[2]	= 32;
	sParticle->m_uchStartAlpha	= random->RandomInt( 64, 128 );
	sParticle->m_uchEndAlpha	= 0;
	sParticle->m_uchStartSize	= random->RandomInt( 16, 32 );
	sParticle->m_uchEndSize		= sParticle->m_uchStartSize * 3;
	sParticle->m_flRoll			= random->RandomInt( 0, 360 );
	sParticle->m_flRollDelta	= random->RandomFloat( -0.2f, 0.2f );

#else

	//
	// Non-XBox blood
	//

	CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "FX_Gib" );
	pSimple->SetSortOrigin( origin );

	Vector	vDir;

	vDir.Random( -1.0f, 1.0f );

	for ( int i = 0; i < 4; i++ )
	{
		SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_BloodPuff[0], origin );

		if ( sParticle == NULL )
			return;

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= random->RandomFloat( 0.5f, 0.75f );

		float	speed = random->RandomFloat( 16.0f, 64.0f );

		sParticle->m_vecVelocity	= vDir * -speed;
		sParticle->m_vecVelocity[2] += 16.0f;

		sParticle->m_uchColor[0]	= 128;
		sParticle->m_uchColor[1]	= 32;
		sParticle->m_uchColor[2]	= 32;
		sParticle->m_uchStartAlpha	= 255;
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random->RandomInt( 16, 32 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize * 2;
		sParticle->m_flRoll			= random->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random->RandomFloat( -1.0f, 1.0f );
	}

	for ( int i = 0; i < 4; i++ )
	{
		SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_BloodPuff[1], origin );

		if ( sParticle == NULL )
		{
			return;
		}

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= random->RandomFloat( 0.5f, 0.75f );

		float	speed = random->RandomFloat( 16.0f, 64.0f );

		sParticle->m_vecVelocity	= vDir * -speed;
		sParticle->m_vecVelocity[2] += 16.0f;

		sParticle->m_uchColor[0]	= 128;
		sParticle->m_uchColor[1]	= 32;
		sParticle->m_uchColor[2]	= 32;
		sParticle->m_uchStartAlpha	= random->RandomInt( 64, 128 );
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random->RandomInt( 16, 32 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize * 2;
		sParticle->m_flRoll			= random->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random->RandomFloat( -1.0f, 1.0f );
	}

#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void GibCallback( const CEffectData &data )
{
	FX_Gib( data.m_vOrigin, data.m_vNormal, data.m_flScale );
}

DECLARE_CLIENT_EFFECT( "Gib", GibCallback );
