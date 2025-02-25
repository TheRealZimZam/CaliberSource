//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef EFFECT_COLOR_TABLES_H
#define EFFECT_COLOR_TABLES_H
#ifdef _WIN32
#pragma once
#endif

struct colorentry_t
{
	unsigned char	index;
	
	unsigned char	r;
	unsigned char	g;
	unsigned char	b;
};

#define COLOR_TABLE_SIZE(ct) sizeof(ct)/sizeof(colorentry_t)

// Commander mode indicators (HL2)
enum
{
	COMMAND_POINT_RED = 0,
	COMMAND_POINT_BLUE,
	COMMAND_POINT_GREEN,
	COMMAND_POINT_YELLOW,
};

// Commander mode table
static colorentry_t commandercolors[] =
{
	{ COMMAND_POINT_RED,	1.0,	0.0,	0.0	},
	{ COMMAND_POINT_BLUE,	0.0,	0.0,	1.0	},
	{ COMMAND_POINT_GREEN,	0.0,	1.0,	0.0	},
	{ COMMAND_POINT_YELLOW,	1.0,	1.0,	0.0	},
};

static colorentry_t bloodcolors[] =
{
	{ BLOOD_COLOR_RED,		96,		0,		0	},
	{ BLOOD_COLOR_YELLOW,	155,	155,	0	},
	{ BLOOD_COLOR_GREEN,	64,		155,	0	},
	{ BLOOD_COLOR_BLUE,		32,		96,		155	},
	{ BLOOD_COLOR_MECH,		20,		20,		20	},	//Leak some oil
	{ BLOOD_COLOR_ANTLION,	195,	195,	0	},
	{ BLOOD_COLOR_ZOMBIE,	195,	195,	0	},
#ifdef HL2_EPISODIC
	{ BLOOD_COLOR_ANTLION_WORKER,	150,	195,	0	},
#endif
};

#endif // EFFECT_COLOR_TABLES_H
