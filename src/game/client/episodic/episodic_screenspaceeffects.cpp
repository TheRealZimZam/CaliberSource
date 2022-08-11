//
// Episodic screen-space effects
//

#include "cbase.h"
#include "screenspaceeffects.h"
#include "rendertexture.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialvar.h"
#include "cdll_client_int.h"
#include "materialsystem/itexture.h"
#include "keyvalues.h"
#include "ClientEffectPrecacheSystem.h"

#include "episodic_screenspaceeffects.h"


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#ifdef _X360
#define STUN_TEXTURE "_rt_FullFrameFB2"
#else
#define STUN_TEXTURE "_rt_WaterRefraction"
#endif

static float g_flDX7NoiseScale = 4.0f; 

// ================================================================================================================
//
//  Ep 1. Intro blur
//
// ================================================================================================================

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEP1IntroEffect::Init( void ) 
{
	m_flDuration = 0.0f;
	m_flFinishTime = 0.0f;
	m_bUpdateView = true;
	m_bFadeOut = false;

	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetString( "$basetexture", STUN_TEXTURE );
	m_EffectMaterial.Init( "__ep1introeffect", TEXTURE_GROUP_CLIENT_EFFECTS, pVMTKeyValues );
	m_StunTexture.Init( STUN_TEXTURE, TEXTURE_GROUP_CLIENT_EFFECTS );
}

void CEP1IntroEffect::Shutdown( void ) 
{
	m_EffectMaterial.Shutdown();
	m_StunTexture.Shutdown();
}


//------------------------------------------------------------------------------
// Purpose: Pick up changes in our parameters
//------------------------------------------------------------------------------
void CEP1IntroEffect::SetParameters( KeyValues *params )
{
	if( params->FindKey( "duration" ) )
	{
		m_flDuration = params->GetFloat( "duration" );
		m_flFinishTime = gpGlobals->curtime + m_flDuration;
	}

	if( params->FindKey( "fadeout" ) )
	{
		m_bFadeOut = ( params->GetInt( "fadeout" ) == 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the alpha value depending on various factors and time
//-----------------------------------------------------------------------------
inline unsigned char CEP1IntroEffect::GetFadeAlpha( void )
{
	// Find our percentage between fully "on" and "off" in the pulse range
	float flEffectPerc = ( m_flDuration == 0.0f ) ? 0.0f : ( m_flFinishTime - gpGlobals->curtime ) / m_flDuration;
	flEffectPerc = clamp( flEffectPerc, 0.0f, 1.0f );

	if  ( m_bFadeOut )
	{
		// HDR requires us to be more subtle, or we get uber-brightening
		if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE )
			return (unsigned char) clamp( 50.0f * flEffectPerc, 0.0f, 50.0f );

		// Non-HDR
		return (unsigned char) clamp( 64.0f * flEffectPerc, 0.0f, 64.0f );
	}
	else
	{
		// HDR requires us to be more subtle, or we get uber-brightening
		if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE )
			return (unsigned char) clamp( 64.0f * flEffectPerc, 50.0f, 64.0f );

		// Non-HDR
		return (unsigned char) clamp( 128.0f * flEffectPerc, 64.0f, 128.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Render the effect
//-----------------------------------------------------------------------------
void CEP1IntroEffect::Render( int x, int y, int w, int h )
{
	if ( ( m_flFinishTime == 0 ) || ( IsEnabled() == false ) )
		return;

	CMatRenderContextPtr pRenderContext( materials );
	
	// Set ourselves to the proper rendermode
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	// Draw the texture if we're using it
	if ( m_bUpdateView )
	{
		// Save off this pass
		Rect_t srcRect;
		srcRect.x = x;
		srcRect.y = y;
		srcRect.width = w;
		srcRect.height = h;
		pRenderContext->CopyRenderTargetToTextureEx( m_StunTexture, 0, &srcRect, NULL );
		m_bUpdateView = false;
	}

	byte overlaycolor[4] = { 255, 255, 255, 0 };
	
	// Get our fade value depending on our fade duration
	overlaycolor[3] = GetFadeAlpha();
	if ( g_pMaterialSystemHardwareConfig->UsesSRGBCorrectBlending() )
	{
		// For DX10 cards, alpha blending happens in linear space, so try to adjust by hacking alpha to 50%
		overlaycolor[3] *= 0.7f;
	}

	// Disable overself if we're done fading out
	if ( m_bFadeOut && overlaycolor[3] == 0 )
	{
		// Takes effect next frame (we don't want to hose our matrix stacks here)
		g_pScreenSpaceEffects->DisableScreenSpaceEffect( "introblur" );
		m_bUpdateView = true;
	}

	// Calculate some wavey noise to jitter the view by
	float vX = 2.0f * -fabs( cosf( gpGlobals->curtime ) * cosf( gpGlobals->curtime * 6.0 ) );
	float vY = 2.0f * cosf( gpGlobals->curtime ) * cosf( gpGlobals->curtime * 5.0 );

	// Scale percentage
	float flScalePerc = 0.02f + ( 0.01f * cosf( gpGlobals->curtime * 2.0f ) * cosf( gpGlobals->curtime * 0.5f ) );
	
	// Scaled offsets for the UVs (as texels)
	float flUOffset = ( m_StunTexture->GetActualWidth() - 1 ) * flScalePerc * 0.5f;
	float flVOffset = ( m_StunTexture->GetActualHeight() - 1 ) * flScalePerc * 0.5f;
	
	// New UVs with scaling offsets
	float flU1 = flUOffset;
	float flU2 = ( m_StunTexture->GetActualWidth() - 1 ) - flUOffset;
	float flV1 = flVOffset;
	float flV2 = ( m_StunTexture->GetActualHeight() - 1 ) - flVOffset;

	// Draw the "zoomed" overlay
	pRenderContext->DrawScreenSpaceRectangle( m_EffectMaterial, vX, vY, w, h,
		flU1, flV1, 
		flU2, flV2, 
		m_StunTexture->GetActualWidth(), m_StunTexture->GetActualHeight() );

	render->ViewDrawFade( overlaycolor, m_EffectMaterial );

	// Save off this pass
	Rect_t srcRect;
	srcRect.x = x;
	srcRect.y = y;
	srcRect.width = w;
	srcRect.height = h;
	pRenderContext->CopyRenderTargetToTextureEx( m_StunTexture, 0, &srcRect, NULL );

	// Restore our state
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
}

// ================================================================================================================
//
//	Watersplash
//
// ================================================================================================================

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWatersplashEffect::Init( void ) 
{
	m_flFinishTime = 0.0f;
	m_bUpdateView = true;
	m_bFadeOut = false;

	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetString( "$basetexture", "effects/splashwake1" );

	m_EffectMaterial.Init( "__watersplash", TEXTURE_GROUP_CLIENT_EFFECTS, pVMTKeyValues );
	m_EffectMaterial->Refresh( );
	m_BlurTexture.Init( STUN_TEXTURE, TEXTURE_GROUP_CLIENT_EFFECTS );
}

void CWatersplashEffect::Shutdown( void ) 
{
	m_EffectMaterial.Shutdown();
	m_BlurTexture.Shutdown();
}

//------------------------------------------------------------------------------
// Purpose: Pick up changes in our parameters
//------------------------------------------------------------------------------
void CWatersplashEffect::SetParameters( KeyValues *params )
{
	if( params->FindKey( "fadeout" ) )
	{
		m_bFadeOut = ( params->GetInt( "fadeout" ) == 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the alpha value depending on various factors and time
//-----------------------------------------------------------------------------
inline unsigned char CWatersplashEffect::GetFadeAlpha( void )
{

}

//-----------------------------------------------------------------------------
// Purpose: Render the effect
//-----------------------------------------------------------------------------
void CWatersplashEffect::Render( int x, int y, int w, int h )
{
	//TODO; Draw random drops, a full splash texture, and blur everything except the effect a bit
}


// ================================================================================================================
//
//	Raindrops
//
// ================================================================================================================

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CRaindropsEffect::Init( void ) 
{
	m_flDuration = 0.0f;
	m_flFinishTime = 0.0f;
	m_bUpdateView = true;
	m_bFadeOut = false;

	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetString( "$basetexture", "effects/splashwake1" );

	m_EffectMaterial.Init( "__raindrops", TEXTURE_GROUP_CLIENT_EFFECTS, pVMTKeyValues );
	m_EffectMaterial->Refresh( );
}

void CRaindropsEffect::Shutdown( void ) 
{
	m_EffectMaterial.Shutdown();
}

//------------------------------------------------------------------------------
// Purpose: Pick up changes in our parameters
//------------------------------------------------------------------------------
void CRaindropsEffect::SetParameters( KeyValues *params )
{
	if( params->FindKey( "duration" ) )
	{
		m_flDuration = params->GetFloat( "duration" );
		m_flFinishTime = gpGlobals->curtime + m_flDuration;
	}

	if( params->FindKey( "fadeout" ) )
	{
		m_bFadeOut = ( params->GetInt( "fadeout" ) == 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the alpha value depending on various factors and time
//-----------------------------------------------------------------------------
inline unsigned char CRaindropsEffect::GetFadeAlpha( void )
{

}

//-----------------------------------------------------------------------------
// Purpose: Render the effect
//-----------------------------------------------------------------------------
void CRaindropsEffect::Render( int x, int y, int w, int h )
{
	//TODO; Draw random drops
}


// ================================================================================================================
//
//  Ep 2. Groggy-player view
//
// ================================================================================================================

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEP2StunEffect::Init( void ) 
{
	m_flDuration = 0.0f;
	m_flFinishTime = 0.0f;
	m_bUpdateView = true;
	m_bFadeOut = false;

	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetString( "$basetexture", STUN_TEXTURE );
	m_EffectMaterial.Init( "__ep2stuneffect", TEXTURE_GROUP_CLIENT_EFFECTS, pVMTKeyValues );
	m_StunTexture.Init( STUN_TEXTURE, TEXTURE_GROUP_CLIENT_EFFECTS );
}

void CEP2StunEffect::Shutdown( void ) 
{
	m_EffectMaterial.Shutdown();
	m_StunTexture.Shutdown();
}

//------------------------------------------------------------------------------
// Purpose: Pick up changes in our parameters
//------------------------------------------------------------------------------
void CEP2StunEffect::SetParameters( KeyValues *params )
{
	if( params->FindKey( "duration" ) )
	{
		m_flDuration = params->GetFloat( "duration" );
		m_flFinishTime = gpGlobals->curtime + m_flDuration;
	}

	if( params->FindKey( "fadeout" ) )
	{
		m_bFadeOut = ( params->GetInt( "fadeout" ) == 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the alpha value depending on various factors and time
//-----------------------------------------------------------------------------
inline unsigned char CEP2StunEffect::GetFadeAlpha( void )
{
	// Find our percentage between fully "on" and "off" in the pulse range
	float flEffectPerc = ( m_flDuration == 0.0f ) ? 0.0f : ( m_flFinishTime - gpGlobals->curtime ) / m_flDuration;
	flEffectPerc = clamp( flEffectPerc, 0.0f, 1.0f );

	if  ( m_bFadeOut )
	{
		// HDR requires us to be more subtle, or we get uber-brightening
		if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE )
			return (unsigned char) clamp( 50.0f * flEffectPerc, 0.0f, 50.0f );

		// Non-HDR
		return (unsigned char) clamp( 64.0f * flEffectPerc, 0.0f, 64.0f );
	}
	else
	{
		// HDR requires us to be more subtle, or we get uber-brightening
		if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE )
			return (unsigned char) clamp( 164.0f * flEffectPerc, 128.0f, 164.0f );

		// Non-HDR
		return (unsigned char) clamp( 164.0f * flEffectPerc, 128.0f, 164.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Render the effect
//-----------------------------------------------------------------------------
void CEP2StunEffect::Render( int x, int y, int w, int h )
{
	if ( ( m_flFinishTime == 0 ) || ( IsEnabled() == false ) )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	// Set ourselves to the proper rendermode
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	if ( m_bUpdateView )
	{
		// Save off this pass
		Rect_t srcRect;
		srcRect.x = x;
		srcRect.y = y;
		srcRect.width = w;
		srcRect.height = h;
		pRenderContext->CopyRenderTargetToTextureEx( m_StunTexture, 0, &srcRect, NULL );
		m_bUpdateView = false;
	}

	byte overlaycolor[4] = { 255, 255, 255, 0 };

	// Get our fade value depending on our fade duration
	overlaycolor[3] = GetFadeAlpha();

	// Disable overself if we're done fading out
	if ( m_bFadeOut && overlaycolor[3] == 0 )
	{
		// Takes effect next frame (we don't want to hose our matrix stacks here)
		g_pScreenSpaceEffects->DisableScreenSpaceEffect( "ep2_groggy" );
		m_bUpdateView = true;
	}

	// Calculate some wavey noise to jitter the view by
	float vX = 4.0f * cosf( gpGlobals->curtime ) * cosf( gpGlobals->curtime * 6.0 );
	float vY = 2.0f * cosf( gpGlobals->curtime ) * cosf( gpGlobals->curtime * 5.0 );

	float flBaseScale = 0.2f + 0.005f * sinf( gpGlobals->curtime * 4.0f );

	// Scale percentage
	float flScalePerc = flBaseScale + ( 0.01f * cosf( gpGlobals->curtime * 2.0f ) * cosf( gpGlobals->curtime * 0.5f ) );

    // Scaled offsets for the UVs (as texels)
	float flUOffset = ( m_StunTexture->GetActualWidth() - 1 ) * flScalePerc * 0.5f;
	float flVOffset = ( m_StunTexture->GetActualHeight() - 1 ) * flScalePerc * 0.5f;

	// New UVs with scaling offsets
	float flU1 = flUOffset;
	float flU2 = ( m_StunTexture->GetActualWidth() - 1 ) - flUOffset;
	float flV1 = flVOffset;
	float flV2 = ( m_StunTexture->GetActualHeight() - 1 ) - flVOffset;

	// Draw the "zoomed" overlay
	pRenderContext->DrawScreenSpaceRectangle( m_EffectMaterial, vX, vY, w, h,
		flU1, flV1, 
		flU2, flV2, 
		m_StunTexture->GetActualWidth(), m_StunTexture->GetActualHeight() );

	render->ViewDrawFade( overlaycolor, m_EffectMaterial );

	// Save off this pass
	Rect_t srcRect;
	srcRect.x = x;
	srcRect.y = y;
	srcRect.width = w;
	srcRect.height = h;
	pRenderContext->CopyRenderTargetToTextureEx( m_StunTexture, 0, &srcRect, NULL );

	// Restore our state
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
}


// ================================================================================================================
//
//  Advisor Effects/Psychedelic
//
// ================================================================================================================

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CStunEffect::Init( void ) 
{
	m_flDuration = 0.0f;
	m_flFinishTime = 0.0f;
	m_bUpdateView = true;

	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetString( "$basetexture", STUN_TEXTURE );
	m_EffectMaterial.Init( "__stuneffect", TEXTURE_GROUP_CLIENT_EFFECTS, pVMTKeyValues );
	m_StunTexture.Init( STUN_TEXTURE, TEXTURE_GROUP_CLIENT_EFFECTS );
}

void CStunEffect::Shutdown( void )
{
	m_EffectMaterial.Shutdown();
	m_StunTexture.Shutdown();
}

//------------------------------------------------------------------------------
// Purpose: Pick up changes in our parameters
//------------------------------------------------------------------------------
void CStunEffect::SetParameters( KeyValues *params )
{
	if( params->FindKey( "duration" ) )
	{
		m_flDuration = params->GetFloat( "duration" );
		m_flFinishTime = gpGlobals->curtime + m_flDuration;
		m_bUpdateView = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Render the effect
//-----------------------------------------------------------------------------
void CStunEffect::Render( int x, int y, int w, int h )
{
	// Make sure we're ready to play this effect
	if ( ( m_flFinishTime == 0 ) || ( IsEnabled() == false ) )
		return;

	CMatRenderContextPtr pRenderContext( materials );
	
	// Set ourselves to the proper rendermode
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	// Draw the texture if we're using it
	if ( m_bUpdateView )
	{
		// Save off this pass
		Rect_t srcRect;
		srcRect.x = x;
		srcRect.y = y;
		srcRect.width = w;
		srcRect.height = h;
		pRenderContext->CopyRenderTargetToTextureEx( m_StunTexture, 0, &srcRect, NULL );
		m_bUpdateView = false;
	}

	float flEffectPerc = ( m_flFinishTime - gpGlobals->curtime ) / m_flDuration;

	float viewOffs = ( flEffectPerc * 32.0f ) * ( cos( gpGlobals->curtime * 40.0f ) * sin( gpGlobals->curtime * 17.0f ) );
	float vX = x + viewOffs;

	if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 80 )
	{
		if ( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_NONE )
		{
			m_EffectMaterial->ColorModulate( 1.0f, 1.0f, 1.0f );
		}
		else
		{
			// This is a stupid fix, but I don't have time to do a cleaner implementation. Since
			// the introblur.vmt material uses unlit generic, it will tone map, so I need to undo the tone mapping
			// using color modulate.  The proper fix would be to use a different material type that
			// supports alpha blending but not tone mapping, which I don't think exists. Whatever. This works when
			// the tone mapping scalar is less than 1.0, which it is in the cases it's used in game.
			float flUnTonemap = pow( 1.0f / pRenderContext->GetToneMappingScaleLinear().x, 1.0f / 2.2f );
			m_EffectMaterial->ColorModulate( flUnTonemap, flUnTonemap, flUnTonemap );
		}

		// Set alpha blend value
		float flOverlayAlpha = clamp( ( 150.0f / 255.0f ) * flEffectPerc, 0.0f, 1.0f );
		m_EffectMaterial->AlphaModulate( flOverlayAlpha );

		// Draw full screen alpha-blended quad
		pRenderContext->DrawScreenSpaceRectangle( m_EffectMaterial, 0, 0, w, h,
			vX, 0, (m_StunTexture->GetActualWidth()-1)+vX, (m_StunTexture->GetActualHeight()-1), 
			m_StunTexture->GetActualWidth(), m_StunTexture->GetActualHeight() );
	}

	// Save off this pass
	Rect_t srcRect;
	srcRect.x = x;
	srcRect.y = y;
	srcRect.width = w;
	srcRect.height = h;
	pRenderContext->CopyRenderTargetToTextureEx( m_StunTexture, 0, &srcRect, NULL );

	// Restore our state
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
}


// ================================================================================================================
//
//  Critical Health Effect
//
// ================================================================================================================

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
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
void CCriticalHealthEffect::Shutdown( void )
{
	m_BlurMaterial.Shutdown();
}

//------------------------------------------------------------------------------
// CCriticalHealthEffect SetParameters
//------------------------------------------------------------------------------
void CCriticalHealthEffect::SetParameters( KeyValues *params )
{
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
		m_flEffectAlpha = params->GetInt( "effect_alpha" );
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
	if( !m_bEnabled )
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
// Console Interface for testing
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

static ConVar mat_neardeath( "mat_neardeath", "0", FCVAR_CLIENTDLL, "Enable/disable critical health post effect", EnableNeardeath );
