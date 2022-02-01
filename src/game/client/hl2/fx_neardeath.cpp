//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Prototype screen effects for critical health
//
//=============================================================================//

#include "cbase.h"
#include "keyvalues.h"
#include "cdll_client_int.h"
#include "view_scene.h"
#include "viewrender.h"
#include "tier0/icommandline.h"
#include "materialsystem/IMesh.h"
#include "materialsystem/IMaterial.h"
#include "materialsystem/IMaterialSystemHardwareConfig.h"
#include "materialsystem/IMaterialVar.h"

#include "ScreenSpaceEffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static float g_flDX7NoiseScale = 4.0f; 

//------------------------------------------------------------------------------
// Film grain post-processing effect
//------------------------------------------------------------------------------
class CCriticalHealthEffect : public IScreenSpaceEffect
{
public:													

	CCriticalHealthEffect( );
   ~CCriticalHealthEffect( );

	void Init( );
	void Shutdown( );

	void SetParameters( KeyValues *params );

	void Render( int x, int y, int w, int h );

	void Enable( bool bEnable );
	bool IsEnabled( );

private:

	void DrawSplotch( float x, float y, float width, float height, float u, float v, float uWidth, float vHeight, int orientation, int alpha=255 );

	bool				m_bEnable;

	CMaterialReference	m_BlurMaterial;

	Vector4D			m_NoiseScale;

	float				m_flUpdateRate;
	float				m_flEffectAlpha;

	int					m_nOverlayAlpha;
};

ADD_SCREENSPACE_EFFECT( CCriticalHealthEffect, neardeath );

//------------------------------------------------------------------------------
// CCriticalHealthEffect constructor
//------------------------------------------------------------------------------
CCriticalHealthEffect::CCriticalHealthEffect( )
{
	m_NoiseScale = Vector4D( 0.14f, 0.14f, 0.14f, 0.78f );

	m_flUpdateRate = 30.0f;
	m_flEffectAlpha = 1.0;

	m_nOverlayAlpha = 90;
}


//------------------------------------------------------------------------------
// CCriticalHealthEffect destructor
//------------------------------------------------------------------------------
CCriticalHealthEffect::~CCriticalHealthEffect( )
{
}

//------------------------------------------------------------------------------
// CCriticalHealthEffect init
//------------------------------------------------------------------------------
void CCriticalHealthEffect::Init( )
{
	KeyValues *pVMTKeyValues = new KeyValues( "filmgrain" );
	pVMTKeyValues->SetString( "$GRAIN_TEXTURE", "Effects/FilmScan256" );
	pVMTKeyValues->SetString( "$SCALEBIAS", "[0.0 0.0 0.0 0.0]" );
	pVMTKeyValues->SetString( "$NOISESCALE", "[0.0 1.0 0.5 1.0]" );

	m_BlurMaterial.Init( "engine/filmgrain", pVMTKeyValues );
	m_BlurMaterial->Refresh( );
}

//------------------------------------------------------------------------------
// CCriticalHealthEffect shutdown
//------------------------------------------------------------------------------
void CCriticalHealthEffect::Shutdown( )
{
	m_BlurMaterial.Shutdown();
}

//------------------------------------------------------------------------------
// CCriticalHealthEffect enable
//------------------------------------------------------------------------------
void CCriticalHealthEffect::Enable( bool bEnable )
{
	m_bEnable = bEnable;
}

bool CCriticalHealthEffect::IsEnabled( )
{
	return m_bEnable;
}

//------------------------------------------------------------------------------
// CCriticalHealthEffect SetParameters
//------------------------------------------------------------------------------
void CCriticalHealthEffect::SetParameters( KeyValues *params )
{
	if( params->FindKey( "noise_scale" ) )
	{
		Color noise_color = params->GetColor( "noise_scale" );
		int r, g, b, a;
		noise_color.GetColor( r, g, b, a );
		m_NoiseScale.x = r/255.0f;;
		m_NoiseScale.y = g/255.0f;;
		m_NoiseScale.z = b/255.0f;;
		m_NoiseScale.w = a/255.0f;;
	}

	if( params->FindKey( "update_rate" ) )
	{
		m_flUpdateRate = params->GetFloat( "update_rate" );
	}

	if( params->FindKey( "overlay_alpha" ) )
	{
		m_nOverlayAlpha = params->GetInt( "overlay_alpha" );
	}

	if( params->FindKey( "effect_alpha" ) )
	{
		m_flEffectAlpha = params->GetFloat( "effect_alpha" );
	}
}

//-----------------------------------------------------------------------------
// Draws a quad to resolve accumulation buffer samples as needed
//-----------------------------------------------------------------------------
void CCriticalHealthEffect::DrawSplotch( float x, float y, float flWidth, float flHeight, float u, float v, float uWidth, float vWidth, int orientation, int alpha )
{
	float flAlpha = ( alpha / 255.0f ) * m_flEffectAlpha;

	float tempU[4] = { u, u, u+uWidth, u+uWidth };
	float tempV[4] = { v, v+vWidth, v+vWidth, v };

	float _u[4], _v[4];
	for( int i=0;i<4;i++ )
	{
		_u[ (i + orientation)%4 ] = tempU[ i ];
		_v[ (i + orientation)%4 ] = tempV[ i ];
	}

	CMatRenderContextPtr pRenderContext( materials );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();
	CMeshBuilder meshBuilder;

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.Position3f( x, y, 0.0f );	// Upper left
	meshBuilder.TexCoord2f( 0, _u[0], _v[0] );
	meshBuilder.Color4f( 1.0f, 1.0f, 1.0f, flAlpha );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( x, y+flHeight, 0.0f );	// Lower left
	meshBuilder.TexCoord2f( 0, _u[1], _v[1] );
	meshBuilder.Color4f( 1.0f, 1.0f, 1.0f, flAlpha );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( x+flWidth, y+flHeight, 0.0 );		// Lower right
	meshBuilder.TexCoord2f( 0, _u[2], _v[2] );
	meshBuilder.Color4f( 1.0f, 1.0f, 1.0f, flAlpha );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( x+flWidth, y, 0.0 );		// Upper right
	meshBuilder.TexCoord2f( 0, _u[3], _v[3] );
	meshBuilder.Color4f( 1.0f, 1.0f, 1.0f, flAlpha );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
}

//------------------------------------------------------------------------------
// CCriticalHealthEffect render
//------------------------------------------------------------------------------
void CCriticalHealthEffect::Render( int x, int y, int w, int h )
{
	if( !m_bEnable )
		return;

	int nTime = (int)(gpGlobals->curtime * m_flUpdateRate);
	float flTime = (float)nTime/m_flUpdateRate;

	// Set up random offsets for grain texture
	float flBiasU  = 0.1f + fmod( flTime * 10.0f, 0.712f );
	float flBiasV  = 0.3f + fmod( flTime * 3.0f, 0.849f );
	float flScaleU = w / 256.0f;
	float flScaleV = h / 256.0f;

	flBiasU *= w;
	flBiasV *= h;

	int paramCount = m_BlurMaterial->ShaderParamCount();
	IMaterialVar **pParams = m_BlurMaterial->GetShaderParams();
	for( int i=0;i<paramCount;i++ )
	{
		IMaterialVar *pVar = pParams[i];

		// alpha operates from 1.0 -> m_NoiseScale.w
		float alpha = 1.0 - ( 1.0 - m_NoiseScale.w ) * m_flEffectAlpha;
		
		if( !Q_stricmp( pVar->GetName(), "$noisescale" ) )
		{
			if( g_pMaterialSystemHardwareConfig->GetDXSupportLevel()<=70 )
			{
			 	pVar->SetVecValue( m_NoiseScale.x*g_flDX7NoiseScale * m_flEffectAlpha,
									m_NoiseScale.y*g_flDX7NoiseScale * m_flEffectAlpha,
									m_NoiseScale.z*g_flDX7NoiseScale * m_flEffectAlpha,
									alpha );
			}
			else
			{
			 	pVar->SetVecValue( m_NoiseScale.x * m_flEffectAlpha,
									m_NoiseScale.y * m_flEffectAlpha,
									m_NoiseScale.z * m_flEffectAlpha,
									alpha );
			}
		}
	}

	// Render Effect
	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->Bind( m_BlurMaterial );
	DrawSplotch( -1.0f, -1.0f, 2.0f, 2.0f, flBiasU, flBiasV, flScaleU, flScaleV, 0 );

	int screenWidth, screenHeight;
 	pRenderContext->GetRenderTargetDimensions( screenWidth, screenHeight );

	// Now let's do the red outline
	IMaterial *pHurtMaterial = materials->FindMaterial( "effects/neardeath", TEXTURE_GROUP_OTHER, false );
	if( !IsErrorMaterial( pHurtMaterial ) )
	{
		pHurtMaterial->Refresh();
		pHurtMaterial->SetMaterialVarFlag( MATERIAL_VAR_VERTEXALPHA, true );
		pRenderContext->Bind( pHurtMaterial );

		int nFlickerAlpha = (int)( m_nOverlayAlpha * m_flEffectAlpha );

		DrawSplotch( -1.0f, -1.0f, 2.0f, 2.0f, 0.0f, 1.0f, 1.0f, -1.0f, 0, nFlickerAlpha );
	}
}


//------------------------------------------------------------------------------
// Console Interface
//------------------------------------------------------------------------------
static void EnableNeardeath( IConVar *pConVar, char const *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	if( var.GetBool() )
	{
		g_pScreenSpaceEffects->EnableScreenSpaceEffect( "neardeath" );
	}
	else
	{
		g_pScreenSpaceEffects->DisableScreenSpaceEffect( "neardeath" );
	}
}

static ConVar mat_neardeath( "mat_neardeath", "0", FCVAR_CLIENTDLL, "Enable/disable near-death post effect", EnableNeardeath );

