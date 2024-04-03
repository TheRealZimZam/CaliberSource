//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Clientside music functions
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include <KeyValues.h>
#include "engine/ienginesound.h"
#include "filesystem.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "soundchars.h"
#include "engine/ivdebugoverlay.h"
#include "tier0/icommandline.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_MusicController, DT_MusicController, CMusicController)
END_RECV_TABLE()


C_MusicController::C_MusicController()
{
}
