//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: VGUI panel which can play back video, in-engine
//
//=============================================================================

#ifndef VGUI_VIDEO_H
#define VGUI_VIDEO_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <vgui_controls/EditablePanel.h>
#include "avi/iavi.h"
#include "avi/ibik.h"
#include "video/ivideoservices.h"


class VideoPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( VideoPanel, vgui::EditablePanel );
public:

	VideoPanel( unsigned int nXPos, unsigned int nYPos, unsigned int nHeight, unsigned int nWidth );

	virtual ~VideoPanel( void );

	virtual void Activate( void );
	virtual void Paint( void );
	virtual void DoModal( void );
	virtual void OnKeyCodeTyped( vgui::KeyCode code );
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void OnClose( void );
	virtual void GetPanelPos( int &xpos, int &ypos );

	void SetExitCommand( const char *pExitCommand )
	{
		if ( pExitCommand && pExitCommand[0] )
		{
			Q_strncpy( m_szExitCommand, pExitCommand, MAX_PATH );
		}
	}

	bool BeginPlayback( const char *pFilename );

	// terminate is a forceful instant shutdown, no exit commands fire
	void StopPlayback( bool bTerminate = false );

	void SetLooping( bool bLooping ) { m_bLooping = bLooping; }
	void SetFadeInTime( float flTime );
	void SetFadeOutTime( float flTime );
	void SetBlackBackground( bool bBlack ){ m_bBlackBackground = bBlack; }
	void SetAllowInterrupt( int nAllowInterrupt ) { m_nAllowInterruption = nAllowInterrupt; }
	void SetShouldPreload( bool bShouldPreload ) { m_bShouldPreload = bShouldPreload; }

	bool IsPlaying();

protected:

	virtual void OnTick( void );
	virtual void OnCommand( const char *pcCommand ) { BaseClass::OnCommand( pcCommand ); }
	virtual void OnVideoOver();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

protected:
	IVideoMaterial *m_VideoMaterial;	//For AVIs, WMVs, Etc.
	BIKMaterial_t	m_BIKHandle;		//Biks/Smks
	AVIMaterial_t	m_AVIHandle;		//For old avis (source03/06)
	
	IMaterial		*m_pMaterial;
	int				m_nPlaybackHeight;			// Calculated to address ratio changes
	int				m_nPlaybackWidth;
	char			m_szExitCommand[MAX_PATH];	// This call is fired at the engine when the video finishes or is interrupted

	float			m_flU;	// U,V ranges for video on its sheet
	float			m_flV;

	float			m_flStartPlayTime;
	float			m_flFadeInTime;
	float			m_flFadeInEndTime;
	float			m_flFadeOutTime;
	float			m_flFadeOutEndTime;

	int				m_nAllowInterruption;
	int				m_nShutdownCount;

	bool			m_bLooping;
	bool			m_bStopAllSounds;
	bool			m_bBlackBackground;
	bool			m_bAllowAlternateMedia;
	bool			m_bShouldPreload;
	bool			m_bStarted;

private:
	float			m_flCurrentVolume;
};


// Creates a VGUI panel which plays a video and executes a client command at its finish (if specified)
extern bool VideoPanel_Create( unsigned int nXPos, unsigned int nYPos, 
							   unsigned int nWidth, unsigned int nHeight, 
							   const char *pVideoFilename, 
							   const char *pExitCommand = NULL );

#endif // VGUI_VIDEO_H
