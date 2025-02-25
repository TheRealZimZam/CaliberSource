//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef EPISODIC_SCREENSPACEEFFECTS_H
#define EPISODIC_SCREENSPACEEFFECTS_H
#ifdef _WIN32
#pragma once
#endif

#include "screenspaceeffects.h"

//
//  EP1 Intro Blur
//
class CEP1IntroEffect : public IScreenSpaceEffect
{
public:
	CEP1IntroEffect( void ) : 
		  m_flDuration( 0.0f ), 
		  m_flFinishTime( 0.0f ), 
		  m_bUpdateView( true ),
		  m_bEnabled( false ),
		  m_bFadeOut( false ) {}

	virtual void Init( void );
	virtual void Shutdown( void );
	virtual void SetParameters( KeyValues *params );
	virtual void Enable( bool bEnable ) { m_bEnabled = bEnable; }
	virtual bool IsEnabled( ) { return m_bEnabled; }

	virtual void Render( int x, int y, int w, int h );

private:
	inline unsigned char	GetFadeAlpha( void );

	CTextureReference m_StunTexture;
	CMaterialReference m_EffectMaterial;
	float		m_flDuration;
	float		m_flFinishTime;
	bool		m_bUpdateView;
	bool		m_bEnabled;
	bool		m_bFadeOut;
};

ADD_SCREENSPACE_EFFECT( CEP1IntroEffect, introblur );

//
//  Watersplash
//
class CWatersplashEffect : public IScreenSpaceEffect
{
public:
	CWatersplashEffect( void ) : 
	  m_flFinishTime( 0.0f ), 
	  m_bUpdateView( true ),
	  m_bEnabled( false ),
	  m_bFadeOut( false ) {}

	virtual void Init( void );
	virtual void Shutdown( void );
	virtual void SetParameters( KeyValues *params );
	virtual void Enable( bool bEnable ) { m_bEnabled = bEnable; }
	virtual bool IsEnabled( ) { return m_bEnabled; }

	virtual void Render( int x, int y, int w, int h );

private:
	inline unsigned char	GetFadeAlpha( void );

	CMaterialReference m_EffectMaterial;
	CTextureReference m_BlurTexture;
	float		m_flFinishTime;
	bool		m_bUpdateView;
	bool		m_bEnabled;
	bool		m_bFadeOut;
};

ADD_SCREENSPACE_EFFECT( CWatersplashEffect, waterspray );

//
//  Raindrops
//
class CRaindropsEffect : public IScreenSpaceEffect
{
public:
	CRaindropsEffect( void ) : 
	  m_flDuration( 0.0f ), 
	  m_flFinishTime( 0.0f ), 
	  m_bUpdateView( true ),
	  m_bEnabled( false ),
	  m_bFadeOut( false ) {}

	virtual void Init( void );
	virtual void Shutdown( void );
	virtual void SetParameters( KeyValues *params );
	virtual void Enable( bool bEnable ) { m_bEnabled = bEnable; }
	virtual bool IsEnabled( ) { return m_bEnabled; }

	virtual void Render( int x, int y, int w, int h );

private:
	inline unsigned char	GetFadeAlpha( void );

	CMaterialReference m_EffectMaterial;
	float		m_flDuration;
	float		m_flFinishTime;
	bool		m_bUpdateView;
	bool		m_bEnabled;
	bool		m_bFadeOut;
};

ADD_SCREENSPACE_EFFECT( CRaindropsEffect, raindrops );

//
//  EP2 Player Stunned Effect
//
class CEP2StunEffect : public IScreenSpaceEffect
{
public:
	CEP2StunEffect( void ) : 
	  m_flDuration( 0.0f ), 
	  m_flFinishTime( 0.0f ), 
	  m_bUpdateView( true ),
	  m_bEnabled( false ),
	  m_bFadeOut( false ) {}

	virtual void Init( void );
	virtual void Shutdown( void );
	virtual void SetParameters( KeyValues *params );
	virtual void Enable( bool bEnable ) { m_bEnabled = bEnable; }
	virtual bool IsEnabled( ) { return m_bEnabled; }

	virtual void Render( int x, int y, int w, int h );

private:
	inline unsigned char	GetFadeAlpha( void );

	CTextureReference m_StunTexture;
	CMaterialReference m_EffectMaterial;
	float		m_flDuration;
	float		m_flFinishTime;
	bool		m_bUpdateView;
	bool		m_bEnabled;
	bool		m_bFadeOut;
};

ADD_SCREENSPACE_EFFECT( CEP2StunEffect, ep2_groggy );

//
// Advisor Degauss - Now Psychedelic
//
class CStunEffect : public IScreenSpaceEffect
{
public:
	CStunEffect( void ) : 
		m_flDuration( 0.0f ), 
		m_flFinishTime( 0.0f ), 
		m_bUpdateView( true ) {}

	virtual void Init( void );
	virtual void Shutdown( void );
	virtual void SetParameters( KeyValues *params );
	virtual void Enable( bool bEnable ) { m_bEnabled = bEnable; }
	virtual bool IsEnabled( ) { return m_bEnabled; }

	virtual void Render( int x, int y, int w, int h );

private:
	CTextureReference m_StunTexture;
	CMaterialReference m_EffectMaterial;
	float		m_flDuration;
	float		m_flFinishTime;
	bool		m_bUpdateView;
	bool		m_bEnabled;
};

ADD_SCREENSPACE_EFFECT( CStunEffect, episodic_stun );

//
//  Critical Health Effect
//
class CCriticalHealthEffect : public IScreenSpaceEffect
{
public:													

	CCriticalHealthEffect( );
   ~CCriticalHealthEffect( );

	virtual void Init( );
	virtual void Shutdown( void );
	virtual void SetParameters( KeyValues *params );
	virtual void Render( int x, int y, int w, int h );
	virtual void Enable( bool bEnable ) { m_bEnabled = bEnable; }
	virtual bool IsEnabled( ) { return m_bEnabled; }

private:
	void DrawSplotch( float x, float y, float width, float height, float u, float v, float uWidth, float vHeight, int orientation, int alpha=255 );
	bool				m_bEnabled;
	CMaterialReference	m_BlurMaterial;
	Vector4D			m_NoiseScale;
	float				m_flUpdateRate;
	float				m_flEffectAlpha;
	int					m_nOverlayAlpha;

};

ADD_SCREENSPACE_EFFECT( CCriticalHealthEffect, neardeath );

#endif // EPISODIC_SCREENSPACEEFFECTS_H
