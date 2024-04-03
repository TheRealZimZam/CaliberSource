//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: VGUI panel which can play back video, in-engine
//
//=============================================================================

#include "cbase.h"
#include <vgui/IVGui.h>
#include "vgui/IInput.h"
#include <vgui/ISurface.h>
#include "ienginevgui.h"
#include "iclientmode.h"
#include "vgui_video.h"
#include "engine/IEngineSound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#ifdef PORTAL2
using namespace BaseModUI;
#endif

static CUtlVector< VideoPanel * > g_vecVideoPanels;

enum VideoAllowInterrupt_t
{
	VIDEO_NO_INTERRUPT = 0,
	VIDEO_ALLOW_INTERRUPT = 1,
	VIDEO_ALLOW_INTERRUPT_DEV_ONLY,
};


// This is a hack due to the fact that the user can type quit with the video panel up, but it's parented to the GameUI dll root panel, which is already gone so
// we would crash in the destructor.
void VGui_ClearVideoPanels()
{
	for ( int i = g_vecVideoPanels.Count() - 1; i >= 0; --i )
	{
		if ( g_vecVideoPanels[ i ] )
		{
			delete g_vecVideoPanels[ i ];
		}
	}
	g_vecVideoPanels.RemoveAll();
}

void VGui_StopAllVideoPanels()
{
	for ( int i = g_vecVideoPanels.Count() - 1; i >= 0; --i )
	{
		if ( g_vecVideoPanels[ i ] )
		{
			// instant termination required
			// no exit commands are allowed to run
			// no deferring shutdown or fade out possible
			g_vecVideoPanels[ i ]->StopPlayback( true );
		}
	}
}

bool VGui_IsPlayingFullScreenVideo()
{
	if ( !enginevgui )
	{
		// not sure this interface is available when we get called
		return false;
	}

#ifdef PORTAL2
	vgui::VPANEL pParent = enginevgui->GetPanel( PANEL_GAMEDLL );
#else
	vgui::VPANEL pParent = enginevgui->GetPanel( PANEL_GAMEUIDLL );	//PANEL_ROOT
#endif

	for ( int i = g_vecVideoPanels.Count() - 1; i >= 0; --i )
	{
		VideoPanel *pVideoPanel = g_vecVideoPanels[i];
		if ( !pVideoPanel )
			continue;

		if ( pVideoPanel->GetVParent() != pParent )
			continue;

		if ( pVideoPanel->IsVisible() && pVideoPanel->IsEnabled() && pVideoPanel->IsPlaying() )
		{
			int wide;
			int tall;
			pVideoPanel->GetSize( wide, tall );

			int screenWide;
			int screenTall;
			vgui::surface()->GetScreenSize( screenWide, screenTall );

			if ( wide == screenWide && tall == screenTall )
			{
				return true;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
VideoPanel::VideoPanel( unsigned int nXPos, unsigned int nYPos, unsigned int nHeight, unsigned int nWidth ) : 
	BaseClass( NULL, "VideoPanel" ),
	m_VideoMaterial( NULL ),
	m_BIKHandle( BIKHANDLE_INVALID ),
	m_nPlaybackWidth( 0 ),
	m_nPlaybackHeight( 0 ),
	m_nShutdownCount( 0 ),
	m_bLooping( false ),
	m_bStopAllSounds( true ),
	m_bAllowAlternateMedia( true ),
	m_nAllowInterruption( VIDEO_NO_INTERRUPT ),
	m_bStarted( false ),
	m_bShouldPreload( false )
{
	m_flStartPlayTime = 0;
	m_flFadeInTime = 0;
	m_flFadeInEndTime = 0;
	m_flFadeOutTime = 0;
	m_flFadeOutEndTime = 0;

	// Must be passed in, off by default
	m_szExitCommand[0] = '\0';

	m_flCurrentVolume = 0;
	m_bBlackBackground = true;

#ifdef PORTAL2
	vgui::VPANEL pParent = enginevgui->GetPanel( PANEL_GAMEDLL );
#else
	vgui::VPANEL pParent = enginevgui->GetPanel( PANEL_GAMEUIDLL );	//PANEL_ROOT
#endif // PORTAL2
	SetParent( pParent );
	SetVisible( false );

	SetKeyBoardInputEnabled( true );
	SetMouseInputEnabled( false );

	SetProportional( false );
	SetVisible( true );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	SetPostChildPaintEnabled( true );
	
	// Set us up
	SetTall( nHeight );
	SetWide( nWidth );
	SetPos( nXPos, nYPos );

	SetScheme(vgui::scheme()->LoadSchemeFromFile( "resource/VideoPanelScheme.res", "VideoPanelScheme"));
	LoadControlSettings( "resource/UI/VideoPanel.res" );

	// Let us update
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	g_vecVideoPanels.AddToTail( this );
}

void VideoPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	MakeReadyForUse();
}

//-----------------------------------------------------------------------------
// Properly shutdown out materials
//-----------------------------------------------------------------------------
VideoPanel::~VideoPanel( void )
{
	SetParent( (vgui::Panel *) NULL );

	g_vecVideoPanels.FindAndRemove( this );

	// Shut down this video, destroy the video material
	if ( g_pVideo != NULL && m_VideoMaterial != NULL )
	{
		g_pVideo->DestroyVideoMaterial( m_VideoMaterial );
		m_VideoMaterial = NULL;
	}
	if ( m_BIKHandle != BIKHANDLE_INVALID )
	{
		bik->DestroyMaterial( m_BIKHandle );
		m_BIKHandle = BIKHANDLE_INVALID;
		m_pMaterial = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Keeps a tab on when the movie is ending and allows a frame to pass to prevent threading issues
//-----------------------------------------------------------------------------
void VideoPanel::OnTick( void ) 
{ 
	if ( m_nShutdownCount > 0 )
	{
		m_nShutdownCount++;
		if ( m_nShutdownCount > 10 )
		{
			OnClose();
			m_nShutdownCount = 0;
		}
	}
#ifdef PORTAL2	//SRC2013
	else
	{
		ConVarRef volumeConVar( "volume" );
		float flMasterVolume = volumeConVar.IsValid() ? volumeConVar.GetFloat() : 0;
		if ( m_flCurrentVolume != flMasterVolume )
		{
			// for safety, only update on real change
			m_flCurrentVolume = flMasterVolume;
			bik->UpdateVolume( m_BIKHandle );
		}
	}
#endif

	BaseClass::OnTick(); 
}

void VideoPanel::OnVideoOver()
{
	StopPlayback();
}

//-----------------------------------------------------------------------------
// Purpose: Begins playback of a movie
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool VideoPanel::BeginPlayback( const char *pFilename )
{
	// Who the heck hacked this in?
#ifdef _X360
	XVIDEO_MODE videoMode;
	XGetVideoMode( &videoMode );

	// for 50Hz PAL, load a 25Hz version of the ep1_recap movie.
	if( ( videoMode.RefreshRate < 59.0f ) && ( Q_stricmp( pFilename, "media/ep1_recap.bik" ) == 0 ) )
	{
		pFilename = "media/ep1_recap_25fps.bik";
	}
#endif

	if ( pFilename == NULL_STRING || pFilename[ 0 ] == '\0' )
	{
		Warning( "VideoPanel::BeginPlayback() - Invalid Filename!: %s\n", pFilename );
		return false;
	}

	// Destroy any previously allocated video
	if ( m_VideoMaterial != NULL )
	{
		g_pVideo->DestroyVideoMaterial( m_VideoMaterial );
		m_VideoMaterial = NULL;
	}
	if ( m_BIKHandle != BIKHANDLE_INVALID )
	{
		bik->DestroyMaterial( m_BIKHandle );
		m_BIKHandle = BIKHANDLE_INVALID;
	}

	m_pMaterial = NULL;

	// Check if its anything other than .bik, if not, use the old bink system
	bool bIsBIK = !Q_stricmp( Q_GetFileExtension( pFilename ), "bik" );

	// need working video services
	if ( g_pVideo == NULL )
	{
		Warning( "VideoPanel::BeginPlayback() - IVideoServices Error! Restricting to .bik playback. : %s\n", pFilename );
		bIsBIK = true; //Slam it to true
	}

	if (!bIsBIK)
	{
		m_VideoMaterial = g_pVideo->CreateVideoMaterial( "VideoMaterial", pFilename, "GAME",
												VideoPlaybackFlags::DEFAULT_MATERIAL_OPTIONS,
												VideoSystem::DETERMINE_FROM_FILE_EXTENSION, m_bAllowAlternateMedia );
	}
	else
	{
		// Load and create our BINK video
#if 0
		int nFlags = ( m_bLooping ? BIK_LOOP : 0 );

		// only preload the transition videos
		// in-game videos are precached with aan alternate system
		if (  m_bShouldPreload )
			nFlags |= BIK_PRELOAD;

		char szMaterialName[ FILENAME_MAX ];
		Q_snprintf( szMaterialName, sizeof( szMaterialName ), "VideoBIKMaterial%i", bik->GetGlobalMaterialAllocationNumber() );
#endif

		m_BIKHandle = bik->CreateMaterial( "VideoBIKMaterial", pFilename, "GAME"/*, nFlags*/ );
		if ( m_BIKHandle == BIKHANDLE_INVALID )
		{
			Warning( "VideoPanel::BeginPlayback() - Video was not AVI or BIK, Cannot continue!\n" );
			return false;
		}
	}

	m_bStarted = true;

	// If we want to be the sole audio source
	if ( m_bStopAllSounds )
		enginesound->NotifyBeginMoviePlayback();

	int nWidth, nHeight;
	if ( m_VideoMaterial != NULL )
	{
		m_VideoMaterial->GetVideoImageSize( &nWidth, &nHeight );
		m_VideoMaterial->GetVideoTexCoordRange( &m_flU, &m_flV );
		m_pMaterial = m_VideoMaterial->GetMaterial();
		DevMsg( "Quicktime Video \"%s\" size: %d %d\n", pFilename, nWidth, nHeight );
	}
	else
	{
		bik->GetFrameSize( m_BIKHandle, &nWidth, &nHeight );
		bik->GetTexCoordRange( m_BIKHandle, &m_flU, &m_flV );
		m_pMaterial = bik->GetMaterial( m_BIKHandle );
		DevMsg( "Bink Video \"%s\" size: %d %d\n", pFilename, nWidth, nHeight );
	}

	float flFrameRatio = ( (float) GetWide() / (float) GetTall() );
	float flVideoRatio = ( (float) nWidth / (float) nHeight );

	if ( flVideoRatio > flFrameRatio )
	{
		m_nPlaybackWidth = GetWide();
		m_nPlaybackHeight = ( GetWide() / flVideoRatio );
	}
	else if ( flVideoRatio < flFrameRatio )
	{
		m_nPlaybackWidth = ( GetTall() * flVideoRatio );
		m_nPlaybackHeight = GetTall();
	}
	else
	{
		m_nPlaybackWidth = GetWide();
		m_nPlaybackHeight = GetTall();
	}

	m_flStartPlayTime = gpGlobals->realtime;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VideoPanel::StopPlayback( bool bTerminate )
{
	SetVisible( false );

	if ( !bTerminate )
	{
		// Start the deferred shutdown process
		m_nShutdownCount = 1;
	}
	else
	{
		// caller wants instant termination
		// prevent any exit command
		m_nShutdownCount = 0;
//		m_ExitCommand.Clear();
		OnClose();

		// Destroy any previously allocated video
		if ( m_BIKHandle != BIKHANDLE_INVALID )
		{
			bik->DestroyMaterial( m_BIKHandle );
			m_BIKHandle = BIKHANDLE_INVALID;
			m_pMaterial = NULL;
		}
	}
}

void VideoPanel::SetFadeInTime( float flTime ) 
{ 
	m_flFadeInTime = flTime;
	m_flFadeInEndTime = gpGlobals->realtime + flTime; 
	SetAlpha( 0 );
}

void VideoPanel::SetFadeOutTime( float flTime ) 
{ 
	m_flFadeOutTime = flTime;
	m_flFadeOutEndTime = gpGlobals->realtime + flTime; 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VideoPanel::Activate( void )
{
	MoveToFront();
	RequestFocus();
	SetVisible( true );
	SetEnabled( true );

	vgui::surface()->SetMinimized( GetVPanel(), false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VideoPanel::DoModal( void )
{
	MakePopup();
	Activate();

	vgui::input()->SetAppModalSurface( GetVPanel() );
	vgui::surface()->RestrictPaintToSinglePanel( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VideoPanel::OnKeyCodeTyped( vgui::KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		StopPlayback( true );
	}
	else
	{
		BaseClass::OnKeyCodeTyped( code );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle keys that should cause us to close
//-----------------------------------------------------------------------------
void VideoPanel::OnKeyCodePressed( vgui::KeyCode code )
{
	// These keys cause the panel to shutdown
	if ( code == KEY_ESCAPE || 
		 code == KEY_BACKQUOTE || 
		 code == KEY_SPACE || 
		 code == KEY_ENTER ||
		 code == KEY_XBUTTON_A || 
		 code == KEY_XBUTTON_B ||
		 code == KEY_XBUTTON_X || 
		 code == KEY_XBUTTON_Y || 
		 code == KEY_XBUTTON_START || 
		 code == KEY_XBUTTON_BACK )
	{
		StopPlayback();
	}
	else
	{
		BaseClass::OnKeyCodePressed( code );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VideoPanel::OnClose( void )
{
	DevMsg( "VideoPanel::OnClose() played video for %.2f seconds\n", gpGlobals->realtime - m_flStartPlayTime );

	if ( m_bStopAllSounds )
		enginesound->NotifyEndMoviePlayback();

	BaseClass::OnClose();

	if ( vgui::input()->GetAppModalSurface() == GetVPanel() )
	{
		vgui::input()->ReleaseAppModalSurface();
	}

	vgui::surface()->RestrictPaintToSinglePanel( NULL );

	// Fire an exit command if we're asked to do so
	if ( m_szExitCommand[0] )
	{
		engine->ClientCmd( m_szExitCommand );
	}

	SetVisible( false );
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VideoPanel::GetPanelPos( int &xpos, int &ypos )
{
	xpos = ( (float) ( GetWide() - m_nPlaybackWidth ) / 2 );
	ypos = ( (float) ( GetTall() - m_nPlaybackHeight ) / 2 );
}

//-----------------------------------------------------------------------------
// Purpose: Update and draw the frame
//-----------------------------------------------------------------------------
void VideoPanel::Paint( void )
{
	BaseClass::Paint();

	// If its not a IMovie, check for BIK
	if ( m_VideoMaterial == NULL )
	{
		if ( m_BIKHandle == BIKHANDLE_INVALID || m_pMaterial == NULL )
			return;
	}

	// Update our frame, but only if Bink is ready for us to process another frame.
	// We aren't really swapping here, but ReadyForSwap is a good way to throttle.
	// We'd rather throttle this way so that we don't limit the overall frame rate of the system.
	if ( m_VideoMaterial->Update() == false && bik->Update( m_BIKHandle ) == false )
		OnVideoOver();	// Issue a close command

	// Sit in the "center"
	int xpos, ypos;
	GetPanelPos( xpos, ypos );
	LocalToScreen( xpos, ypos );

	float alpha = ((float)GetFgColor()[3]/255.0f);
	float frac = 1.0f;
	if ( m_flFadeOutTime > 0 )
	{
		m_flFadeInTime = 0;
		frac = ( m_flFadeOutEndTime - gpGlobals->realtime ) / m_flFadeOutTime;
	}
	else if ( m_flFadeInTime > 0 )
	{
		m_flFadeOutTime = 0;
		frac = 1.0f - (( m_flFadeInEndTime - gpGlobals->realtime ) / m_flFadeInTime);
	}

	//alpha = frac * 255;
	alpha = clamp( frac, 0.0f, 1.0f );

	// Black out the background (we could omit drawing under the video surface, but this is straight-forward)
	if ( m_bBlackBackground )
	{
		vgui::surface()->DrawSetColor(  0, 0, 0, 255 );
		vgui::surface()->DrawFilledRect( 0, 0, GetWide(), GetTall() );
	}

	// Draw the polys to draw this out
	CMatRenderContextPtr pRenderContext( materials );
	
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->Bind( m_pMaterial, NULL );

	CMeshBuilder meshBuilder;
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	float flLeftX = xpos;
	float flRightX = xpos + (m_nPlaybackWidth-1);

	float flTopY = ypos;
	float flBottomY = ypos + (m_nPlaybackHeight-1);

	// Map our UVs to cut out just the portion of the video we're interested in
	float flLeftU = 0.0f;
	float flTopV = 0.0f;

	// We need to subtract off a pixel to make sure we don't bleed
	float flRightU = m_flU - ( 1.0f / (float) m_nPlaybackWidth );
	float flBottomV = m_flV - ( 1.0f / (float) m_nPlaybackHeight );

	// Get the current viewport size
	int vx, vy, vw, vh;
	pRenderContext->GetViewport( vx, vy, vw, vh );

	// map from screen pixel coords to -1..1
	flRightX = FLerp( -1, 1, 0, vw, flRightX );
	flLeftX = FLerp( -1, 1, 0, vw, flLeftX );
	flTopY = FLerp( 1, -1, 0, vh ,flTopY );
	flBottomY = FLerp( 1, -1, 0, vh, flBottomY );

	for ( int corner=0; corner<4; corner++ )
	{
		bool bLeft = (corner==0) || (corner==3);
		meshBuilder.Position3f( (bLeft) ? flLeftX : flRightX, (corner & 2) ? flBottomY : flTopY, 0.0f );
		meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
		meshBuilder.TexCoord2f( 0, (bLeft) ? flLeftU : flRightU, (corner & 2) ? flBottomV : flTopV );
		meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
		meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
		meshBuilder.Color4f( 1.0f, 1.0f, 1.0f, alpha );
		meshBuilder.AdvanceVertex();
	}
	
	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

//	return alpha;
}

bool VideoPanel::IsPlaying()
{
	if ( m_VideoMaterial != NULL )
		return true;

	if ( m_BIKHandle != BIKHANDLE_INVALID || m_pMaterial != NULL )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Create and playback a video in a panel
// Input  : nWidth - Width of panel (in pixels) 
//			nHeight - Height of panel
//			*pVideoFilename - Name of the file to play
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool VideoPanel_Create( unsigned int nXPos, unsigned int nYPos, 
						unsigned int nWidth, unsigned int nHeight, 
						const char *pVideoFilename, 
						const char *pExitCommand /*= NULL*/,
						int nAllowInterruption /*= 0*/,
						float flFadeInTime /*= 1*/,
						bool bLoop /*= false*/)
{
	// Create the base video panel
	VideoPanel *pVideoPanel = new VideoPanel( nXPos, nYPos, nHeight, nWidth );
	if ( pVideoPanel == NULL )
		return false;

	// Toggle if we want the panel to allow interruption
	pVideoPanel->SetAllowInterrupt( nAllowInterruption );

	// Set the command we'll call (if any) when the video is interrupted or completes
	pVideoPanel->SetExitCommand( pExitCommand );

	pVideoPanel->SetLooping( bLoop );

	// Start it going
	if ( pVideoPanel->BeginPlayback( pVideoFilename ) == false )
	{
		delete pVideoPanel;
		return false;
	}

	pVideoPanel->SetFadeInTime( flFadeInTime );

	// Take control
	pVideoPanel->DoModal();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Take a raw filename and ensure it points to the correct directory and file extension
//-----------------------------------------------------------------------------
void ComposeBinkFilename( const char *lpszFilename, char *lpszOut, int nOutSize )
{
	Q_strncpy( lpszOut, "media/", nOutSize );	// Assume we must play out of the media directory
	char strFilename[MAX_PATH];
	Q_StripExtension( lpszFilename, strFilename, nOutSize );
	Q_strncat( lpszOut, lpszFilename, nOutSize );
//	Q_strncat( lpszOut, ".bik", nOutSize );		// Assume we're a .bik extension type
}

//-----------------------------------------------------------------------------
// Purpose: Create a video panel with the supplied commands
//-----------------------------------------------------------------------------
void CreateVideoPanel( const char *lpszFilename, const char *lpszExitCommand, int nWidth, int nHeight, int nAllowInterruption, float flFadeTime = 0, bool bLoop = false )
{
	char strFullpath[MAX_PATH];
	ComposeBinkFilename( lpszFilename, strFullpath, sizeof(strFullpath) );

	// Use the full screen size if they haven't specified an override
	unsigned int nScreenWidth = ( nWidth != 0 ) ? nWidth : ScreenWidth();
	unsigned int nScreenHeight = ( nHeight != 0 ) ? nHeight : ScreenHeight();

	// Create the panel and go!
	if ( VideoPanel_Create( 0, 0, nScreenWidth, nScreenHeight, strFullpath, lpszExitCommand, nAllowInterruption, flFadeTime, bLoop ) == false )
	{
		Warning( "Unable to play video: %s\n", strFullpath );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Used to launch a video playback (Debug) -
//  user must include file extension
//-----------------------------------------------------------------------------
CON_COMMAND( playvideo, "Plays a video: <filename> [width height]" )
{
	if ( args.ArgC() < 2 )
		return;

	unsigned int nScreenWidth = Q_atoi( args[2] );
	unsigned int nScreenHeight = Q_atoi( args[3] );
	
	CreateVideoPanel( args[1], NULL, nScreenWidth, nScreenHeight, VIDEO_ALLOW_INTERRUPT );
}


//-----------------------------------------------------------------------------
// Purpose: Used to launch a video playback and fire a command on completion
//-----------------------------------------------------------------------------
CON_COMMAND( playvideo_exitcommand, "Plays a video and fires and exit command when it is stopped or finishes: <filename> <exit command>" )
{
	if ( args.ArgC() < 2 )
		return;

	// Pull out the exit command we want to use
	char *pExitCommand = Q_strstr( args.GetCommandString(), args[2] );

	CreateVideoPanel( args[1], pExitCommand, 0, 0, VIDEO_ALLOW_INTERRUPT );
}


//-----------------------------------------------------------------------------
// Purpose: Cause all playback to stop
//-----------------------------------------------------------------------------
CON_COMMAND( stopvideos, "Stops all videos playing to the screen" )
{
	FOR_EACH_VEC( g_vecVideoPanels, itr )
	{
		g_vecVideoPanels[itr]->StopPlayback();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Cause all playback to fade out
//-----------------------------------------------------------------------------
CON_COMMAND( stopvideos_fadeout, "Fades out all videos playing to the screen: <time>" )
{
	if ( args.ArgC() < 1 )
		return;

	float flTime = Q_atoi( args[1] );
	if ( flTime <= 0 )
	{
		Warning( "Fade time needs to be greater than zero!  Setting to 0.1f\n" );
		flTime = 0.1f;
	}

	FOR_EACH_VEC( g_vecVideoPanels, itr )
	{
		g_vecVideoPanels[itr]->SetFadeOutTime( flTime );
	}
}
