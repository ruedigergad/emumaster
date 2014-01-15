// (c) Copyright 2010 javicq, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licensing terms must be obtained.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <strings.h>
#include <glib.h>
#include <libosso.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <signal.h>

#include "maemo.h"
#include "../common/emu.h"
#include "emu.h"
#include "menu/gconf.h"

#define GPLAYER1_KEY kGConfPath "/player1" kGConfPlayerKeyboardPath "/"
#define GPLAYER2_KEY kGConfPath "/player2" kGConfPlayerKeyboardPath "/"

#define GPLAYER1_K kGConfPath "/player1"
#define GPLAYER2_K kGConfPath "/player2"

int use_gui = 0;
char **g_argv;
osso_context_t* osso = NULL;
GMainContext* g_context = NULL;
GMainLoop* g_loop = NULL;

static int game_started = 0;

int check_file( char* filename ) {
	if( !filename || !filename[0] ) return 0;
	FILE* f = fopen( filename, "rb" );
	if( !f ) return 0;
	fclose(f);
	return 1;
}

void cli_config( char* file ) {
	FILE* f = fopen( file, "r" );
	if( !f ) {
		printf( "Configuration file not found: %s\n", file );
		if( !use_gui ) {
			printf( "Cannot use default options, exiting\n" );
			exit(1);
		}
		return;
	}
	printf( "Reading config file %s\n", file );
	char opt[64], value[64];
	unsigned short* key_map = currentConfig.player[0].keymap;
	unsigned short* key_map2 = currentConfig.player[1].keymap;
	while( !feof(f) ) {
		fscanf( f, "%s %s\n", opt, value );
		if( opt[0] == '#' ) {
			while( fgetc(f) != '\n' );
			continue;
		}
		if( !strcmp( opt, "P1_K_A" ) ) key_map[atoi(value)] = MDB_A;
		else if( !strcmp( opt, "P1_K_B" ) ) key_map[atoi(value)] = MDB_B;
		else if( !strcmp( opt, "P1_K_C" ) ) key_map[atoi(value)] = MDB_C;
		else if( !strcmp( opt, "P1_K_X" ) ) key_map[atoi(value)] = MDB_X;
		else if( !strcmp( opt, "P1_K_Y" ) ) key_map[atoi(value)] = MDB_Y;
		else if( !strcmp( opt, "P1_K_Z" ) ) key_map[atoi(value)] = MDB_Z;
		else if( !strcmp( opt, "P1_K_UP" ) ) key_map[atoi(value)] = MDB_UP;
		else if( !strcmp( opt, "P1_K_DOWN" ) ) key_map[atoi(value)] = MDB_DOWN;
		else if( !strcmp( opt, "P1_K_LEFT" ) ) key_map[atoi(value)] = MDB_LEFT;
		else if( !strcmp( opt, "P1_K_RIGHT" ) ) key_map[atoi(value)] = MDB_RIGHT;
		else if( !strcmp( opt, "P1_K_UP_LEFT" ) ) key_map[atoi(value)] = MDB_UP | MDB_LEFT;
		else if( !strcmp( opt, "P1_K_DOWN_RIGHT" ) ) key_map[atoi(value)] = MDB_DOWN | MDB_RIGHT;
		else if( !strcmp( opt, "P1_K_DOWN_LEFT" ) ) key_map[atoi(value)] = MDB_LEFT | MDB_DOWN;
		else if( !strcmp( opt, "P1_K_UP_RIGHT" ) ) key_map[atoi(value)] = MDB_RIGHT | MDB_RIGHT;
		else if( !strcmp( opt, "P1_K_START" ) ) key_map[atoi(value)] = MDB_START;
		else if( !strcmp( opt, "P1_K_MODE" ) ) key_map[atoi(value)] = MDB_MODE;
		else if( !strcmp( opt, "CMD_MENU" ) ) key_map[atoi(value)] = CMD_QUIT;
		else if( !strcmp( opt, "CMD_QUICKSAVE_1" ) ) key_map[atoi(value)] = CMD_QUICKSAVE;
		else if( !strcmp( opt, "CMD_QUICKSAVE_2" ) ) key_map[atoi(value)] = CMD_QUICKSAVE + 1;
		else if( !strcmp( opt, "CMD_QUICKSAVE_3" ) ) key_map[atoi(value)] = CMD_QUICKSAVE + 2;
		else if( !strcmp( opt, "CMD_QUICKSAVE_4" ) ) key_map[atoi(value)] = CMD_QUICKSAVE + 3;
		else if( !strcmp( opt, "CMD_QUICKLOAD_1" ) ) key_map[atoi(value)] = CMD_QUICKLOAD;
		else if( !strcmp( opt, "CMD_QUICKLOAD_2" ) ) key_map[atoi(value)] = CMD_QUICKLOAD + 1;
		else if( !strcmp( opt, "CMD_QUICKLOAD_3" ) ) key_map[atoi(value)] = CMD_QUICKLOAD + 2;
		else if( !strcmp( opt, "CMD_QUICKLOAD_4" ) ) key_map[atoi(value)] = CMD_QUICKLOAD + 3;
		else if( !strcmp( opt, "P2_K_A" ) ) key_map2[atoi(value)] = MDB_A;
		else if( !strcmp( opt, "P2_K_B" ) ) key_map2[atoi(value)] = MDB_B;
		else if( !strcmp( opt, "P2_K_C" ) ) key_map2[atoi(value)] = MDB_C;
		else if( !strcmp( opt, "P2_K_X" ) ) key_map2[atoi(value)] = MDB_X;
		else if( !strcmp( opt, "P2_K_Y" ) ) key_map2[atoi(value)] = MDB_Y;
		else if( !strcmp( opt, "P2_K_Z" ) ) key_map2[atoi(value)] = MDB_Z;
		else if( !strcmp( opt, "P2_K_UP" ) ) key_map2[atoi(value)] = MDB_UP;
		else if( !strcmp( opt, "P2_K_DOWN" ) ) key_map2[atoi(value)] = MDB_DOWN;
		else if( !strcmp( opt, "P2_K_LEFT" ) ) key_map2[atoi(value)] = MDB_LEFT;
		else if( !strcmp( opt, "P2_K_RIGHT" ) ) key_map2[atoi(value)] = MDB_RIGHT;
		else if( !strcmp( opt, "P2_K_UP_LEFT" ) ) key_map2[atoi(value)] = MDB_UP | MDB_LEFT;
		else if( !strcmp( opt, "P2_K_DOWN_RIGHT" ) ) key_map2[atoi(value)] = MDB_DOWN | MDB_RIGHT;
		else if( !strcmp( opt, "P2_K_DOWN_LEFT" ) ) key_map2[atoi(value)] = MDB_LEFT | MDB_DOWN;
		else if( !strcmp( opt, "P2_K_UP_RIGHT" ) ) key_map2[atoi(value)] = MDB_RIGHT | MDB_RIGHT;
		else if( !strcmp( opt, "P2_K_START" ) ) key_map2[atoi(value)] = MDB_START;
		else if( !strcmp( opt, "P2_K_MODE" ) ) key_map2[atoi(value)] = MDB_MODE;
		else if( !strcmp( opt, "SCALING" ) ) currentConfig.scaling = atoi(value);
		else if( !strcmp( opt, "FRAMESKIP" ) ) currentConfig.Frameskip = atoi(value);
		else if( !strcmp( opt, "DISPLAY_FRAMERATE" ) ) { if( atoi(value) ) currentConfig.EmuOpt |= 0x02; else currentConfig.EmuOpt &= ~0x02; }
		else if( !strcmp( opt, "SOUND" ) ) { if( atoi(value) ) currentConfig.EmuOpt |= 0x04; else currentConfig.EmuOpt &= ~0x04; }
	}
	currentConfig.screensaver = 0;
	currentConfig.player[0].inputFlags = _P_ENABLE_KEY | _P_ENABLE_TS;
}

void parse_cmd_line(int argc, char *argv[])
{
	int x, unrecognized = 0;
	char* conf_file = "picodrive.ini";

	for(x = 1; x < argc; x++)
	{
		if(argv[x][0] == '-')
		{
			if(strcasecmp(argv[x], "-gui") == 0) {
				use_gui = 1;
			}
			if(strcasecmp(argv[x], "-o") == 0) {
				if( argc > x+1 ) conf_file = argv[++x];
				else unrecognized = 1;
			}
			else {
				unrecognized = 1;
				break;
			}
		} else {
			/* External Frontend: ROM Name */
			strncpy(romFileName, argv[x], PATH_MAX);
			romFileName[PATH_MAX-1] = 0;
			if( !check_file(romFileName) ) unrecognized = 1;
			engineState = PGS_ReloadRom;
			break;
		}
	}

	if (unrecognized) {
		printf("\n\n\nPicoDrive for Maemo v" G_STRINGIFY(GAME_VERSION) " (c) javicq, 2010\n");
		printf("usage: %s [options] <romfile>\n", argv[0]);
		printf("\nAvailable options:\n");
		printf("\t-o <config_file>\tLoad options from specified config_file. Default is picodrive.ini\n\n");
	} else {
		cli_config( conf_file );
	}
}

static gint dbus_callback( const gchar *interface, const gchar *method, GArray *arguments, gpointer data, osso_rpc_t *retval ) {
	retval->type = DBUS_TYPE_BOOLEAN;

	if( engineState == PGS_Menu ) {
		// Only if we haven't received the startup command yet.
		printf("Osso: Startup method is: %s\n", method);

		if (strcmp(method, "game_run") == 0) {
			emu_set_state( PGS_ReloadRom );
			retval->value.b = TRUE;
		} else if (strcmp(method, "game_continue") == 0) {
			emu_set_state( PGS_ReloadRomState );
			retval->value.b = TRUE;
		} else if (strcmp(method, "game_restart") == 0) {
			emu_set_state( PGS_ReloadRom );
			retval->value.b = TRUE;
		} else if (strcmp(method, "game_close") == 0) {
			maemo_quit();
			retval->value.b = TRUE;
		} else {
			emu_set_state( PGS_ReloadRom );
			retval->value.b = FALSE;
		}
	} else {
		if (strcmp(method, "game_close") == 0) {
			printf("Osso: quitting because of D-Bus close message\n");
			maemo_quit();
			retval->value.b = TRUE;
		} else {
			retval->value.b = FALSE;
		}
	}

	return OSSO_OK;
}

static void hw_callback( osso_hw_state_t *state, gpointer data )
{
	if( state->shutdown_ind ) {
		// Shutting down. Try to quit gracefully.
		maemo_quit();
	}
	if( state->system_inactivity_ind && currentConfig.screensaver ) {
		// Screen went off, and power saving is active.
		maemo_quit();
	}
}

static void disp_callback( osso_display_state_t state, gpointer data ) {
	if( state != OSSO_DISPLAY_ON ) {
		if( currentConfig.screensaver ) {
			maemo_quit();
		} else {
			osso_display_state_on( osso );
		}
	}
}

static void osso_init() {
	g_type_init();
	g_set_prgname( "picodrive" );
	g_set_application_name( "PicoDrive" );
	g_context = g_main_context_default();
	g_loop = g_main_loop_new( g_context, FALSE );

	osso = osso_initialize( "com.jcq.picodrive", "1.0", FALSE, NULL );
	if( !osso ) {
		fprintf( stderr, "Error initializing osso\n" );
		exit(1);
	}
	osso_rpc_set_default_cb_f( osso, dbus_callback, NULL );

	osso_hw_state_t hwStateFlags = { FALSE };
	hwStateFlags.shutdown_ind = TRUE;
	hwStateFlags.system_inactivity_ind = TRUE;
	osso_hw_set_event_cb( osso, &hwStateFlags, hw_callback, NULL );
	osso_hw_set_display_event_cb( osso, disp_callback, NULL );

	printf( "libosso initialized\n" );

}

static void osso_deinit() {
	if( !osso ) return;

	// Send a goodbye message to the launcher
	osso_return_t ret;
	osso_rpc_t retval = { 0 };
	ret = osso_rpc_run(osso, "com.jcq.picodrive.startup",
				"/com/jcq/picodrive/startup", "com.jcq.picodrive.startup",
				game_started ? "game_pause" : "game_close", &retval, DBUS_TYPE_INVALID);

	if (ret != OSSO_OK) {
		printf("Osso: failed to notify launcher\n");
	}
	osso_rpc_free_val(&retval);

	osso_deinitialize(osso);
	g_main_loop_unref(g_loop);
	g_main_context_unref(g_context);

	osso = NULL;
}

static void osso_config() {
	GConfClient* gcc = gconf_client_get_default();
	char* rom = gconf_client_get_string( gcc, kGConfRomFile, NULL );
	if( rom ) {
		strcpy( romFileName, rom );
		printf( "Loading ROM %s...\n", romFileName );
	}

	if( gconf_client_get_bool( gcc, kGConfSound, NULL ) )
		currentConfig.EmuOpt |= 0x04;
	else
		currentConfig.EmuOpt &= ~0x04;

	if( gconf_client_get_bool( gcc, kGConfDisplayFramerate, NULL ) )
		currentConfig.EmuOpt |= 0x02;
	else
		currentConfig.EmuOpt &= ~0x02;

	currentConfig.Frameskip = gconf_client_get_int( gcc, kGConfFrameskip, NULL ) - 1;
	currentConfig.scaling = gconf_client_get_int( gcc, kGConfScaler, NULL );

	currentConfig.screensaver = gconf_client_get_bool( gcc, kGConfSaver, NULL );
	currentConfig.displayTS = gconf_client_get_bool( gcc, GPLAYER1_K kGConfPlayerTouchscreenShow, NULL ) || gconf_client_get_bool( gcc, GPLAYER2_K kGConfPlayerTouchscreenShow, NULL );

	unsigned short* key_map = currentConfig.player[0].keymap;
	key_map[ gconf_client_get_int( gcc, GPLAYER1_KEY "a", NULL) & 0xFF ] = MDB_A;
	key_map[ gconf_client_get_int( gcc, GPLAYER1_KEY "b", NULL) & 0xFF ] = MDB_B;
	key_map[ gconf_client_get_int( gcc, GPLAYER1_KEY "c", NULL) & 0xFF ] = MDB_C;
	key_map[ gconf_client_get_int( gcc, GPLAYER1_KEY "x", NULL) & 0xFF ] = MDB_X;
	key_map[ gconf_client_get_int( gcc, GPLAYER1_KEY "y", NULL) & 0xFF ] = MDB_Y;
	key_map[ gconf_client_get_int( gcc, GPLAYER1_KEY "z", NULL) & 0xFF ] = MDB_Z;
	key_map[ gconf_client_get_int( gcc, GPLAYER1_KEY "dn", NULL) & 0xFF ] = MDB_UP;
	key_map[ gconf_client_get_int( gcc, GPLAYER1_KEY "ds", NULL) & 0xFF ] = MDB_DOWN;
	key_map[ gconf_client_get_int( gcc, GPLAYER1_KEY "de", NULL) & 0xFF ] = MDB_RIGHT;
	key_map[ gconf_client_get_int( gcc, GPLAYER1_KEY "dw", NULL) & 0xFF ] = MDB_LEFT;
	key_map[ gconf_client_get_int( gcc, GPLAYER1_KEY "start", NULL) & 0xFF ] = MDB_START;
	key_map[ gconf_client_get_int( gcc, GPLAYER1_KEY "dne", NULL) & 0xFF ] = MDB_UP | MDB_RIGHT;
	key_map[ gconf_client_get_int( gcc, GPLAYER1_KEY "dnw", NULL) & 0xFF ] = MDB_UP | MDB_LEFT;
	key_map[ gconf_client_get_int( gcc, GPLAYER1_KEY "dse", NULL) & 0xFF ] = MDB_DOWN | MDB_RIGHT;
	key_map[ gconf_client_get_int( gcc, GPLAYER1_KEY "dsw", NULL) & 0xFF ] = MDB_DOWN | MDB_LEFT;
	key_map[ gconf_client_get_int( gcc, GPLAYER1_KEY "quit", NULL) & 0xFF ] = CMD_QUIT;
	int i;
	for( i = 0; i < 4; i++ ) {
		char key[256];
		sprintf( key, "%sqs%d", GPLAYER1_KEY, i+1 );
		key_map[ gconf_client_get_int( gcc, key, NULL) & 0xFF ] = CMD_QUICKSAVE + i;
		sprintf( key, "%sql%d", GPLAYER1_KEY, i+1 );
		key_map[ gconf_client_get_int( gcc, key, NULL) & 0xFF ] = CMD_QUICKLOAD + i;
	}

	int accelSensitivities[] = { 300, 150, 75 };
	currentConfig.player[0].inputFlags =
		( gconf_client_get_bool( gcc, GPLAYER1_K kGConfPlayerAccelerometerEnable, NULL ) ? _P_ENABLE_ACC : 0 ) |
		( gconf_client_get_bool( gcc, GPLAYER1_K kGConfPlayerTouchscreenEnable, NULL ) ? _P_ENABLE_TS : 0 ) |
		( gconf_client_get_bool( gcc, GPLAYER1_K kGConfPlayerKeyboardEnable, NULL ) ? _P_ENABLE_KEY : 0 );
	currentConfig.player[0].accelSensitivity = accelSensitivities[ gconf_client_get_int( gcc, GPLAYER1_K kGConfPlayerAccelerometerSensitivity, NULL ) ];

	key_map = currentConfig.player[1].keymap;
	key_map[ gconf_client_get_int( gcc, GPLAYER2_KEY "a", NULL) & 0xFF ] = MDB_A;
	key_map[ gconf_client_get_int( gcc, GPLAYER2_KEY "b", NULL) & 0xFF ] = MDB_B;
	key_map[ gconf_client_get_int( gcc, GPLAYER2_KEY "c", NULL) & 0xFF ] = MDB_C;
	key_map[ gconf_client_get_int( gcc, GPLAYER2_KEY "x", NULL) & 0xFF ] = MDB_X;
	key_map[ gconf_client_get_int( gcc, GPLAYER2_KEY "y", NULL) & 0xFF ] = MDB_Y;
	key_map[ gconf_client_get_int( gcc, GPLAYER2_KEY "z", NULL) & 0xFF ] = MDB_Z;
	key_map[ gconf_client_get_int( gcc, GPLAYER2_KEY "dn", NULL) & 0xFF ] = MDB_UP;
	key_map[ gconf_client_get_int( gcc, GPLAYER2_KEY "ds", NULL) & 0xFF ] = MDB_DOWN;
	key_map[ gconf_client_get_int( gcc, GPLAYER2_KEY "de", NULL) & 0xFF ] = MDB_RIGHT;
	key_map[ gconf_client_get_int( gcc, GPLAYER2_KEY "dw", NULL) & 0xFF ] = MDB_LEFT;
	key_map[ gconf_client_get_int( gcc, GPLAYER2_KEY "start", NULL) & 0xFF ] = MDB_START;
	key_map[ gconf_client_get_int( gcc, GPLAYER2_KEY "dne", NULL) & 0xFF ] = MDB_UP | MDB_RIGHT;
	key_map[ gconf_client_get_int( gcc, GPLAYER2_KEY "dnw", NULL) & 0xFF ] = MDB_UP | MDB_LEFT;
	key_map[ gconf_client_get_int( gcc, GPLAYER2_KEY "dse", NULL) & 0xFF ] = MDB_DOWN | MDB_RIGHT;
	key_map[ gconf_client_get_int( gcc, GPLAYER2_KEY "dsw", NULL) & 0xFF ] = MDB_DOWN | MDB_LEFT;
	currentConfig.player[1].inputFlags =
		( gconf_client_get_bool( gcc, GPLAYER2_K kGConfPlayerAccelerometerEnable, NULL ) ? _P_ENABLE_ACC : 0 ) |
		( gconf_client_get_bool( gcc, GPLAYER2_K kGConfPlayerTouchscreenEnable, NULL ) ? _P_ENABLE_TS : 0 ) |
		( gconf_client_get_bool( gcc, GPLAYER2_K kGConfPlayerKeyboardEnable, NULL ) ? _P_ENABLE_KEY : 0 );
	currentConfig.player[1].accelSensitivity = accelSensitivities[ gconf_client_get_int( gcc, GPLAYER2_K kGConfPlayerAccelerometerSensitivity, NULL ) ];
}

gboolean osso_events( gboolean block ) {
	static unsigned char frames;
	if( !++frames && !currentConfig.screensaver ) osso_display_blanking_pause( osso );
	if( g_context ) return g_main_context_iteration( g_context, block );
	return FALSE;
}

void sig_handler( int signal ) {
	printf( "Caught signal %d, exiting...\n", signal );
	osso_deinit();
	exit(1);
}

int main(int argc, char *argv[])
{
	romFileName[0] = 0;
	putenv("SDL_VIDEO_X11_WMCLASS=picodrive");
	g_argv = argv;

	//chdir( "/opt/picodrive" );
	engineState = PGS_ReloadRom;
	emu_setDefaultConfig();

	if (argc > 1) parse_cmd_line( argc, argv );
	if( use_gui ){
		osso_init();
		engineState = PGS_Menu;
		osso_config();
	}

	if( !check_file(romFileName) ) {
		fprintf( stderr, "No ROM loaded, exiting\n" );
		osso_deinit();
		exit(1);
	}
	signal( SIGSEGV, sig_handler );
	signal( SIGBUS, sig_handler );
	signal( SIGILL, sig_handler );
	signal( SIGFPE, sig_handler );
	signal( SIGINT, sig_handler );
	signal( SIGABRT, sig_handler );
	signal( SIGHUP, sig_handler );
	signal( SIGTERM, sig_handler );

	maemo_init();
	//sharedmem_init();
	emu_Init();


	for (;;)
	{
		switch (engineState)
		{
			case PGS_Menu:
				osso_events( TRUE );
				break;

			case PGS_ReloadRom:
				if (emu_ReloadRom())
					engineState = PGS_Running;
				else {
					printf("PGS_ReloadRom == 0\n");
					maemo_quit();
				}
				break;
			case PGS_ReloadRomState:
				if (emu_ReloadRom()) {
					engineState = PGS_Running;
					emu_load_snapshot();
				}
				else {
					printf("PGS_ReloadRom == 0\n");
					maemo_quit();
				}
				break;
			case PGS_RestartRun:
				engineState = PGS_Running;

			case PGS_Running:
				game_started = 1;
				emu_Loop();
				break;

			case PGS_QuickLoad:
				emu_quickload( quickSlot );
				emu_set_state( PGS_Running );
				break;

			case PGS_QuickSave:
				emu_quicksave( quickSlot );
				emu_set_state( PGS_Running );
				break;

			case PGS_Quit:
				if( use_gui ) emu_save_snapshot();
				goto endloop;

			default:
				printf("engine got into unknown state (%i), exiting\n", engineState);
				goto endloop;
		}
	}

	endloop:

	osso_deinit();

	emu_Deinit();
	//sharedmem_deinit();
	maemo_deinit();

	return 0;
}
