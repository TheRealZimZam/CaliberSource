//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_ai_basenpc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_Franz : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS( C_Franz, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();

					C_Franz();
	virtual			~C_Franz();

private:
	C_Franz( const C_Franz & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_Franz, DT_NPC_Franz, CNPC_Franz)
END_RECV_TABLE()

C_Franz::C_Franz()
{
}


C_Franz::~C_Franz()
{
}


