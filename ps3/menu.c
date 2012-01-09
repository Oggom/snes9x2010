/******************************************************************************* 
 * menu.c - SNES9x PS3
 *
 *  Created on: Oct 10, 2010
********************************************************************************/

#include <cell/sysmodule.h>
#include <sysutil/sysutil_screenshot.h>

#include "cellframework2/input/pad_input.h"

/*emulator-specific*/
#include "../src/snes9x.h"
#include "emu-ps3.h"

#include "menu.h"

#include "cellframework2/fileio/file_browser.h"

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

#define NUM_ENTRY_PER_PAGE 19

#define ROM_EXTENSIONS "smc|fig|sfc|gd3|gd7|dx2|bsx|swc|zip|jma|SMC|FIG|SFC|BSX|GD3|GD7|DX2|SWC|ZIP|JMA"

#define PRINT_HELP_MESSAGE(menu, currentsetting) \
			cellDbgFontPrintf(menu.items[currentsetting].comment_xpos, menu.items[currentsetting].comment_ypos, menu.items[currentsetting].comment_scalefont, menu.items[currentsetting].comment_color, menu.items[currentsetting].comment);

#define PRINT_HELP_MESSAGE_YESNO(menu, currentsetting) \
			snprintf(menu.items[currentsetting].comment, sizeof(menu.items[currentsetting].comment), *(menu.items[currentsetting].setting_ptr) ? menu.items[currentsetting].comment_yes : menu.items[currentsetting].comment_no); \
			PRINT_HELP_MESSAGE(menu, currentsetting);

menu menuStack[25];
int menuStackindex = 0;
uint32_t menu_is_running = false;		/* is the menu running?*/
bool update_item_colors = true;
static bool set_initial_dir_tmpbrowser;
filebrowser_t browser;				/* main file browser->for rom browser*/
filebrowser_t tmpBrowser;			/* tmp file browser->for everything else*/
uint32_t set_shader = 0;
static uint32_t currently_selected_controller_menu = 0;

#include "menu/menu-entries.h"

static menu menu_filebrowser = {
	"FILE BROWSER |",		/* title*/
	FILE_BROWSER_MENU,		/* enum*/
	0,				/* selected item*/
	0,				/* page*/
	1,				/* maxpages */
	1,				/* refreshpage*/
	NULL				/* items*/
};

static menu menu_generalvideosettings = {
	"VIDEO |",			/* title*/
	GENERAL_VIDEO_MENU,		/* enum*/
	FIRST_VIDEO_SETTING,		/* selected item*/
	0,				/* page*/
	MAX_NO_OF_VIDEO_SETTINGS/NUM_ENTRY_PER_PAGE,	/* max pages */
	1,				/* refreshpage*/
	FIRST_VIDEO_SETTING,		/* first setting*/
	MAX_NO_OF_VIDEO_SETTINGS,	/* max no of path settings*/
	items_generalsettings		/* items*/
};

static menu menu_generalaudiosettings = {
	"AUDIO |",			/* title*/
	GENERAL_AUDIO_MENU,		/* enum*/
	FIRST_AUDIO_SETTING,		/* selected item*/
	0,				/* page*/
	MAX_NO_OF_AUDIO_SETTINGS/NUM_ENTRY_PER_PAGE,	/* max pages */
	1,				/* refreshpage*/
	FIRST_AUDIO_SETTING,		/* first setting*/
	MAX_NO_OF_AUDIO_SETTINGS,	/* max no of path settings*/
	items_generalsettings		/* items*/
};

static menu menu_emu_settings = {
	"SNES9X |",						/* title*/
	EMU_GENERAL_MENU,					/* enum*/
	FIRST_EMU_SETTING,					/* selected item*/
	0,							/* page*/
	MAX_NO_OF_EMU_SETTINGS/NUM_ENTRY_PER_PAGE,		/* max pages*/
	1,                      				/* refreshpage*/
	FIRST_EMU_SETTING,					/* first setting*/
	MAX_NO_OF_EMU_SETTINGS,					/* max no of path settings*/
	items_generalsettings					/* items*/
};

static menu menu_emu_audiosettings = {
	"SNES9X AUDIO |",					/* title*/
	EMU_AUDIO_MENU,						/* enum*/
	FIRST_EMU_AUDIO_SETTING,				/* selected item*/
	0,							/* page*/
	MAX_NO_OF_EMU_AUDIO_SETTINGS/NUM_ENTRY_PER_PAGE,	/* max pages*/
	1,							/* refreshpage*/
	FIRST_EMU_AUDIO_SETTING,				/* first setting*/
	MAX_NO_OF_EMU_AUDIO_SETTINGS,				/* max no of path settings*/
	items_generalsettings					/* items*/
};

static menu menu_pathsettings = {
	"PATH |",						/* title*/
	PATH_MENU,						/* enum*/
	FIRST_PATH_SETTING,					/* selected item*/
	0,							/* page*/
	MAX_NO_OF_PATH_SETTINGS/NUM_ENTRY_PER_PAGE,		/* max pages*/
	1,							/* refreshpage*/
	FIRST_PATH_SETTING,					/* first setting*/
	MAX_NO_OF_PATH_SETTINGS,				/* max no of path settings*/
	items_generalsettings					/* items*/
};

static menu menu_controlssettings = {
	"CONTROLS |",						/* title*/
	CONTROLS_MENU,						/* enum*/
	FIRST_CONTROLS_SETTING_PAGE_1,				/* selected item*/
	0,							/* page*/
	MAX_NO_OF_CONTROLS_SETTINGS/NUM_ENTRY_PER_PAGE,		/* max pages*/
	1,							/* refreshpage*/
	FIRST_CONTROLS_SETTING_PAGE_1,				/* first setting*/
	MAX_NO_OF_CONTROLS_SETTINGS,				/* max no of path settings*/
	items_generalsettings					/* items*/
};

static void produce_menubar(uint32_t menu_enum)
{
	cellDbgFontPuts    (0.09f,  0.05f,  Emulator_GetFontSize(),  menu_enum == GENERAL_VIDEO_MENU ? RED : GREEN,   menu_generalvideosettings.title);
	cellDbgFontPuts    (0.19f,  0.05f,  Emulator_GetFontSize(),  menu_enum == GENERAL_AUDIO_MENU ? RED : GREEN,  menu_generalaudiosettings.title);
	cellDbgFontPuts    (0.29f,  0.05f,  Emulator_GetFontSize(),  menu_enum == EMU_GENERAL_MENU ? RED : GREEN,  menu_emu_settings.title);
	cellDbgFontPuts    (0.40f,  0.05f,  Emulator_GetFontSize(),  menu_enum == EMU_AUDIO_MENU ? RED : GREEN,   menu_emu_audiosettings.title);
	cellDbgFontPuts    (0.60f,  0.05f,  Emulator_GetFontSize(),  menu_enum == PATH_MENU ? RED : GREEN,  menu_pathsettings.title);
	cellDbgFontPuts    (0.70f,  0.05f,  Emulator_GetFontSize(), menu_enum == CONTROLS_MENU ? RED : GREEN,  menu_controlssettings.title); 
	cellDbgFontDraw();
}

static void UpdateBrowser(filebrowser_t * b)
{
	static uint64_t old_state = 0;
	uint64_t state, diff_state, button_was_pressed;

	state = cell_pad_input_poll_device(0);
	diff_state = old_state ^ state;
	button_was_pressed = old_state & diff_state;

	if(frame_count < special_action_msg_expired)
	{
	}
	else
	{
		if (CTRL_LSTICK_DOWN(state))
		{
			if(b->currently_selected < b->file_count-1)
			{
				FILEBROWSER_INCREMENT_ENTRY_POINTER(b);
				set_text_message("", 4);
			}
		}

		if (CTRL_DOWN(state))
		{
			if(b->currently_selected < b->file_count-1)
			{
				FILEBROWSER_INCREMENT_ENTRY_POINTER(b);
				set_text_message("", 7);
			}
		}

		if (CTRL_LSTICK_UP(state))
		{
			if(b->currently_selected > 0)
			{
				FILEBROWSER_DECREMENT_ENTRY_POINTER(b);
				set_text_message("", 4);
			}
		}

		if (CTRL_UP(state))
		{
			if(b->currently_selected > 0)
			{
				FILEBROWSER_DECREMENT_ENTRY_POINTER(b);
				set_text_message("", 7);
			}
		}

		if (CTRL_RIGHT(state))
		{
			b->currently_selected = (MIN(b->currently_selected + 5, b->file_count-1));
			set_text_message("", 7);
		}

		if (CTRL_LSTICK_RIGHT(state))
		{
			b->currently_selected = (MIN(b->currently_selected + 5, b->file_count-1));
			set_text_message("", 4);
		}

		if (CTRL_LEFT(state))
		{
			if (b->currently_selected <= 5)
				b->currently_selected = 0;
			else
				b->currently_selected -= 5;

			set_text_message("", 7);
		}

		if (CTRL_LSTICK_LEFT(state))
		{
			if (b->currently_selected <= 5)
				b->currently_selected = 0;
			else
				b->currently_selected -= 5;

			set_text_message("", 4);
		}

		if (CTRL_R1(state))
		{
			b->currently_selected = (MIN(b->currently_selected + NUM_ENTRY_PER_PAGE, b->file_count-1));
			set_text_message("", 7);
		}

		if (CTRL_L1(state))
		{
			if (b->currently_selected <= NUM_ENTRY_PER_PAGE)
				b->currently_selected= 0;
			else
				b->currently_selected -= NUM_ENTRY_PER_PAGE;

			set_text_message("", 7);
		}

		if (CTRL_CIRCLE(button_was_pressed))
		{
			old_state = state;
			filebrowser_pop_directory(b);
		}


		if (CTRL_L3(state) && CTRL_R3(state))
		{
			/* if a rom is loaded then resume it */
			if (Emulator_IsROMLoaded())
			{
				menu_is_running = 0;
				Emulator_StartROMRunning(1);
				set_text_message("", 15);
			}
		}

		old_state = state;
	}
}

static void RenderBrowser(filebrowser_t * b)
{
	uint32_t file_count = b->file_count;
	int current_index, page_number, page_base, i;
	float currentX, currentY, ySpacing;

	current_index = b->currently_selected;
	page_number = current_index / NUM_ENTRY_PER_PAGE;
	page_base = page_number * NUM_ENTRY_PER_PAGE;

	currentX = 0.09f;
	currentY = 0.09f;
	ySpacing = 0.035f;

	for ( i = page_base; i < file_count && i < page_base + NUM_ENTRY_PER_PAGE; ++i)
	{
		currentY = currentY + ySpacing;
		cellDbgFontPuts(currentX, currentY, Emulator_GetFontSize(), i == current_index ? RED : b->cur[i].d_type == CELL_FS_TYPE_DIRECTORY ? GREEN : WHITE, b->cur[i].d_name);
		cellDbgFontDraw();
	}
	cellDbgFontDraw();
}

static void do_select_file(uint32_t menu_id)
{
	char extensions[256], title[256], object[256], comment[256], dir_path[MAX_PATH_LENGTH],
	path[MAX_PATH_LENGTH], *separatorslash;
	uint64_t state, diff_state, button_was_pressed;
	static uint64_t old_state = 0;

	state = cell_pad_input_poll_device(0);
	diff_state = old_state ^ state;
	button_was_pressed = old_state & diff_state;

	switch(menu_id)
	{
		case GAME_AWARE_SHADER_CHOICE:
			strncpy(dir_path, GAME_AWARE_SHADER_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "cfg|CFG", sizeof(extensions));
			strncpy(title, "GAME AWARE SHADER SELECTION", sizeof(title));
			strncpy(object, "Game Aware Shader", sizeof(object));
			strncpy(comment, "INFO - Select a 'Game Aware Shader' script from the menu by pressing X.", sizeof(comment));
			break;
		case SHADER_CHOICE:
			strncpy(dir_path, SHADERS_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "cg|CG", sizeof(extensions));
			strncpy(title, "SHADER SELECTION", sizeof(title));
			strncpy(object, "Shader", sizeof(object));
			strncpy(comment, "INFO - Select a shader from the menu by pressing the X button.", sizeof(comment));
			break;
		case PRESET_CHOICE:
			strncpy(dir_path, PRESETS_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "conf|CONF", sizeof(extensions));
			strncpy(title, "SHADER PRESETS SELECTION", sizeof(title));
			strncpy(object, "Shader", sizeof(object));
			strncpy(object, "Shader preset", sizeof(object));
                        strncpy(comment, "INFO - Select a shader preset from the menu by pressing the X button. ", sizeof(comment));
			break;
		case INPUT_PRESET_CHOICE:
			strncpy(dir_path, INPUT_PRESETS_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "conf|CONF", sizeof(extensions));
			strncpy(title, "INPUT PRESETS SELECTION", sizeof(title));
			strncpy(object, "Input", sizeof(object));
			strncpy(object, "Input preset", sizeof(object));
                        strncpy(comment, "INFO - Select an input preset from the menu by pressing the X button. ", sizeof(comment));
			break;
		case BORDER_CHOICE:
			strncpy(dir_path, BORDERS_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "png|PNG|jpg|JPG|JPEG|jpeg", sizeof(extensions));
			strncpy(title, "BORDER SELECTION", sizeof(title));
			strncpy(object, "Border", sizeof(object));
			strncpy(object, "Border image file", sizeof(object));
			strncpy(comment, "INFO - Select a border image file from the menu by pressing the X button. ", sizeof(comment));
			break;
	}

	if(set_initial_dir_tmpbrowser)
	{
		filebrowser_new(&tmpBrowser, dir_path, extensions);
		set_initial_dir_tmpbrowser = false;
	}

	UpdateBrowser(&tmpBrowser);

	if (CTRL_START(button_was_pressed))
		filebrowser_reset_start_directory(&tmpBrowser, "/", extensions);

	if (CTRL_CROSS(button_was_pressed))
	{
		if(FILEBROWSER_IS_CURRENT_A_DIRECTORY(tmpBrowser))
		{
			/*if 'filename' is in fact '..' - then pop back directory instead of 
			adding '..' to filename path */
			if(tmpBrowser.currently_selected == 0)
			{
				old_state = state;
				filebrowser_pop_directory(&tmpBrowser);
			}
			else
			{
                                separatorslash = (strcmp(FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser),"/") == 0) ? "" : "/";
				snprintf(path, sizeof(path), "%s%s%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser), separatorslash, FILEBROWSER_GET_CURRENT_FILENAME(tmpBrowser));
				filebrowser_push_directory(&tmpBrowser, path, true);
			}
		}
		else if (FILEBROWSER_IS_CURRENT_A_FILE(tmpBrowser))
		{
			snprintf(path, sizeof(path), "%s/%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser), FILEBROWSER_GET_CURRENT_FILENAME(tmpBrowser));
			printf("path: %s\n", path);

			switch(menu_id)
			{
				case GAME_AWARE_SHADER_CHOICE:
					emulator_implementation_set_gameaware(path);
					strncpy(Settings.GameAwareShaderPath, path, sizeof(Settings.GameAwareShaderPath));
					break;
				case SHADER_CHOICE:
					if(set_shader)
						strncpy(Settings.PS3CurrentShader2, path, sizeof(Settings.PS3CurrentShader2));
					else
						strncpy(Settings.PS3CurrentShader, path, sizeof(Settings.PS3CurrentShader));
					ps3graphics_load_fragment_shader(path, set_shader);
					break;
				case PRESET_CHOICE:
					emulator_implementation_set_shader_preset(path);
					break;
				case INPUT_PRESET_CHOICE:
					emulator_set_controls(path, READ_CONTROLS, "");
					break;
				case BORDER_CHOICE:
					strncpy(Settings.PS3CurrentBorder, path, sizeof(Settings.PS3CurrentBorder));
					emulator_implementation_set_texture(path);
					break;
			}	

			menuStackindex--;
		}
	}

	if (CTRL_TRIANGLE(button_was_pressed))
		menuStackindex--;

        cellDbgFontPrintf(0.09f, 0.09f, Emulator_GetFontSize(), YELLOW, "PATH: %s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser));
	cellDbgFontPuts	(0.09f,	0.05f,	Emulator_GetFontSize(),	RED,	title);
	cellDbgFontPrintf(0.09f, 0.92f, 0.92, YELLOW, "X - Select %s  /\\ - return to settings  START - Reset Startdir", object);
	cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "%s", comment);
	cellDbgFontDraw();

	RenderBrowser(&tmpBrowser);
	old_state = state;
}

static void do_pathChoice(uint32_t menu_id)
{
        char path[1024], newpath[1024], *separatorslash;
	uint64_t state, diff_state, button_was_pressed;
        static uint64_t old_state = 0;

        state = cell_pad_input_poll_device(0);
        diff_state = old_state ^ state;
        button_was_pressed = old_state & diff_state;

	if(set_initial_dir_tmpbrowser)
	{
		filebrowser_new(&tmpBrowser, "/\0", "empty");
		set_initial_dir_tmpbrowser = false;
	}

        UpdateBrowser(&tmpBrowser);

        if (CTRL_START(button_was_pressed))
		filebrowser_reset_start_directory(&tmpBrowser, "/","empty");

        if (CTRL_SQUARE(button_was_pressed))
        {
                if(FILEBROWSER_IS_CURRENT_A_DIRECTORY(tmpBrowser))
                {
                        snprintf(path, sizeof(path), "%s/%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser), FILEBROWSER_GET_CURRENT_FILENAME(tmpBrowser));
                        switch(menu_id)
                        {
                                case PATH_SAVESTATES_DIR_CHOICE:
                                        strcpy(Settings.PS3PathSaveStates, path);
                                        break;
                                case PATH_SRAM_DIR_CHOICE:
                                        strcpy(Settings.PS3PathSRAM, path);
                                        break;
                                case PATH_DEFAULT_ROM_DIR_CHOICE:
                                        strcpy(Settings.PS3PathROMDirectory, path);
                                        break;
				case PATH_CHEATS_DIR_CHOICE:
					strcpy(Settings.PS3PathCheats, path);
					break;
                        }
                        menuStackindex--;
                }
        }
        if (CTRL_TRIANGLE(button_was_pressed))
        {
                strcpy(path, usrDirPath);
                switch(menu_id)
                {
                        case PATH_SAVESTATES_DIR_CHOICE:
                                strcpy(Settings.PS3PathSaveStates, path);
                                break;
                        case PATH_SRAM_DIR_CHOICE:
                                strcpy(Settings.PS3PathSRAM, path);
                                break;
                        case PATH_DEFAULT_ROM_DIR_CHOICE:
                                strcpy(Settings.PS3PathROMDirectory, path);
                                break;
			case PATH_CHEATS_DIR_CHOICE:
				strcpy(Settings.PS3PathCheats, path);
				break;
                }
                menuStackindex--;
        }
        if (CTRL_CROSS(button_was_pressed))
        {
                if(FILEBROWSER_IS_CURRENT_A_DIRECTORY(tmpBrowser))
                {
                        /* if 'filename' is in fact '..' - then pop back 
			directory instead of adding '..' to filename path */

                        if(tmpBrowser.currently_selected == 0)
                        {
                                old_state = state;
				filebrowser_pop_directory(&tmpBrowser);
                        }
                        else
                        {
                                separatorslash = (strcmp(FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser),"/") == 0) ? "" : "/";
                                snprintf(newpath, sizeof(newpath), "%s%s%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser), separatorslash, FILEBROWSER_GET_CURRENT_FILENAME(tmpBrowser));
                                filebrowser_push_directory(&tmpBrowser, newpath, false);
                        }
                }
        }

        cellDbgFontPrintf (0.09f,  0.09f, Emulator_GetFontSize(), YELLOW, 
	"PATH: %s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser));
        cellDbgFontPuts (0.09f, 0.05f,  Emulator_GetFontSize(), RED,    "DIRECTORY SELECTION");
        cellDbgFontPuts(0.09f, 0.93f, 0.92f, YELLOW,
	"X - Enter dir  /\\ - return to settings  START - Reset Startdir");
        cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "%s",
	"INFO - Browse to a directory and assign it as the path by\npressing SQUARE button.");
        cellDbgFontDraw();

        RenderBrowser(&tmpBrowser);
        old_state = state;
}

static void DisplayHelpMessage(int currentsetting)
{
	switch(currentsetting)
	{
		case SETTING_SNES9X_FORCE_PAL:
		case SETTING_SNES9X_FORCE_NTSC:
		case SETTING_SNES9X_AUTO_APPLY_CHEATS:
		case SETTING_SNES9X_AUTO_APPLY_PATCH:
		case SETTING_SNES9X_SRAM_WRITEPROTECT:
		case SETTING_SNES9X_ACCESSORY_TYPE:
			PRINT_HELP_MESSAGE_YESNO(menu_emu_settings, currentsetting);
			break;
		case SETTING_SNES9X_ACCESSORY_AUTODETECTION:
			if(Settings.AccessoryAutoDetection == ACCESSORY_AUTODETECTION_ENABLED)
				cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "%s", "INFO - Accessory Autodetection is set to 'ON'. Games which support\nMouse/Scope/Multitap will be automatically detected and enabled.");
			else if(Settings.AccessoryAutoDetection == ACCESSORY_AUTODETECTION_CONFIRM)
				cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "%s", "INFO - Accessory Autodetection is set to 'Confirm'. When detecting a Mouse/\nScope/Multitap-compatible game, you are asked if you want to enable it.");
			else if(Settings.AccessoryAutoDetection == ACCESSORY_AUTODETECTION_NONE)
				cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "%s", "INFO - Accessory Autodetection is set to 'OFF'. Accessories will not be\ndetected or enabled - standard two joypad controls.");
			break;
		case SETTING_HW_TEXTURE_FILTER:
		case SETTING_HW_TEXTURE_FILTER_2:
		case SETTING_SCALE_ENABLED:
		case SETTING_ENABLE_SCREENSHOTS:
		case SETTING_TRIPLE_BUFFERING:
		case SETTING_THROTTLE_MODE:
		case SETTING_APPLY_SHADER_PRESET_ON_STARTUP:
			PRINT_HELP_MESSAGE_YESNO(menu_generalvideosettings, currentsetting);
			break;
		case SETTING_SCALE_FACTOR:
			snprintf(menu_generalvideosettings.items[currentsetting].comment, sizeof(menu_generalvideosettings.items[currentsetting].comment), "INFO - [Custom Scaling Factor] is set to: '%dx'.", Settings.ScaleFactor);
			PRINT_HELP_MESSAGE(menu_generalvideosettings, currentsetting);
			break;
		case SETTING_KEEP_ASPECT_RATIO:
			cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - [Aspect ratio] is set to '%d:%d'.", ps3graphics_get_aspect_ratio_int(0), ps3graphics_get_aspect_ratio_int(1));
			break;
		case SETTING_SOUND_MODE:
			snprintf(menu_generalaudiosettings.items[currentsetting].comment, sizeof(menu_generalaudiosettings.items[currentsetting].comment), Settings.SoundMode == SOUND_MODE_RSOUND ? "INFO - [Sound Output] is set to 'RSound' - the sound will be streamed over the\n network to the RSound audio server." : Settings.SoundMode == SOUND_MODE_HEADSET ? "INFO - [Sound Output] is set to 'USB/Bluetooth Headset' - sound will\n be output through the headset" : "INFO - [Sound Output] is set to 'Normal' - normal audio output will be\nused.");
			PRINT_HELP_MESSAGE(menu_generalaudiosettings, currentsetting);
			break;
		case SETTING_BORDER:
		case SETTING_GAME_AWARE_SHADER:
		case SETTING_SHADER:
		case SETTING_SHADER_2:
		case SETTING_FONT_SIZE:
		case SETTING_CHANGE_RESOLUTION:
		case SETTING_HW_OVERSCAN_AMOUNT:
		case SETTING_DEFAULT_VIDEO_ALL:
		case SETTING_SAVE_SHADER_PRESET:
			PRINT_HELP_MESSAGE(menu_generalvideosettings, currentsetting);
			break;
		case SETTING_DEFAULT_AUDIO_ALL:
		case SETTING_RSOUND_SERVER_IP_ADDRESS:
			PRINT_HELP_MESSAGE(menu_generalaudiosettings, currentsetting);
			break;
		case SETTING_EMU_CURRENT_SAVE_STATE_SLOT:
		case SETTING_SNES9X_PAL_TIMING:
		case SETTING_EMU_DEFAULT_ALL:
			PRINT_HELP_MESSAGE(menu_emu_settings, currentsetting);
			break;
		case SETTING_EMU_AUDIO_DEFAULT_ALL:
		case SETTING_SNES9X_SOUND_INPUT_RATE:
		case SETTING_SNES9X_MUTE_SOUND:
			PRINT_HELP_MESSAGE(menu_emu_audiosettings, currentsetting);
			break;
		case SETTING_PATH_SAVESTATES_DIRECTORY:
		case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
		case SETTING_PATH_SRAM_DIRECTORY:
		case SETTING_PATH_CHEATS:
		case SETTING_PATH_DEFAULT_ALL:
			PRINT_HELP_MESSAGE(menu_pathsettings, currentsetting);
			break;
			/*
			   case SETTING_PAL60_MODE:
			   cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "%s", Settings.PS3PALTemporalMode60Hz ? "INFO - PAL 60Hz mode is enabled - 60Hz NTSC games will run correctly at 576p PAL\nresolution. NOTE: This is configured on-the-fly." : "INFO - PAL 60Hz mode disabled - 50Hz PAL games will run correctly at 576p PAL\nresolution. NOTE: This is configured on-the-fly.");
			   break;
			 */
		case SETTING_CONTROLS_SCHEME:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - Input Control scheme preset [%s] is selected.\n", Settings.PS3CurrentInputPresetTitle);
			break;
		case SETTING_CONTROLS_DPAD_UP:
		case SETTING_CONTROLS_DPAD_DOWN:
		case SETTING_CONTROLS_DPAD_LEFT:
		case SETTING_CONTROLS_DPAD_RIGHT:
		case SETTING_CONTROLS_BUTTON_CIRCLE:
		case SETTING_CONTROLS_BUTTON_CROSS:
		case SETTING_CONTROLS_BUTTON_TRIANGLE:
		case SETTING_CONTROLS_BUTTON_SQUARE:
		case SETTING_CONTROLS_BUTTON_SELECT:
		case SETTING_CONTROLS_BUTTON_START:
		case SETTING_CONTROLS_BUTTON_L1:
		case SETTING_CONTROLS_BUTTON_R1:
		case SETTING_CONTROLS_BUTTON_L2:
		case SETTING_CONTROLS_BUTTON_R2:
		case SETTING_CONTROLS_BUTTON_L3:
		case SETTING_CONTROLS_BUTTON_R3:
		case SETTING_CONTROLS_BUTTON_L2_BUTTON_L3:
		case SETTING_CONTROLS_BUTTON_L2_BUTTON_R3:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_RIGHT:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_LEFT:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_UP:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_DOWN:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_RIGHT:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_LEFT:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_UP:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_DOWN:
		case SETTING_CONTROLS_BUTTON_R2_BUTTON_R3:
		case SETTING_CONTROLS_BUTTON_R3_BUTTON_L3:
		case SETTING_CONTROLS_ANALOG_R_UP:
		case SETTING_CONTROLS_ANALOG_R_DOWN:
		case SETTING_CONTROLS_ANALOG_R_LEFT:
		case SETTING_CONTROLS_ANALOG_R_RIGHT:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [%s] on the PS3 controller is mapped to action:\n[%s].", menu_controlssettings.items[currentsetting].text, Input_PrintMappedButton(control_binds[currently_selected_controller_menu][currentsetting-FIRST_CONTROL_BIND]));
			break;
		case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
			cellDbgFontPuts(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - Save the custom control settings.\nNOTE: This option will not do anything with Control Scheme [New] or [Default].");
			break;
		case SETTING_CONTROLS_DEFAULT_ALL:
			cellDbgFontPuts(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - Set all 'Controls' settings back to their default values.");
			break;

	}
}

static void producelabelvalue(uint64_t switchvalue)
{
	switch(switchvalue)
	{
		case SETTING_CHANGE_RESOLUTION:
			cellDbgFontPrintf(0.5f, menu_generalvideosettings.items[switchvalue].text_ypos, Emulator_GetFontSize(), ps3graphics_get_initial_resolution() == ps3graphics_get_current_resolution() ? GREEN : ORANGE, ps3graphics_get_resolution_label(ps3graphics_get_current_resolution()));
			cellDbgFontDraw();
			break;
#if 0
		case SETTING_PAL60_MODE: 
			cellDbgFontPuts		(menu_generalvideosettings.items[switchvalue].text_xpos,	menu_generalvideosettings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	currently_selected_setting == menu_generalvideosettings.items[switchvalue].enum_id ? YELLOW : WHITE,	"PAL60 Mode (576p only)");
			cellDbgFontPrintf	(0.5f,	menu_generalvideosettings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	Settings.PS3PALTemporalMode60Hz ? ORANGE : GREEN, Settings.PS3PALTemporalMode60Hz ? "ON" : "OFF");
			break;
#endif
		case SETTING_GAME_AWARE_SHADER:
			cellDbgFontPrintf(0.5f, menu_generalvideosettings.items[menu_generalvideosettings.items[switchvalue].enum_id].text_ypos, Emulator_GetFontSize(), (strcmp(Settings.GameAwareShaderPath, "") == 0) ? GREEN : ORANGE, Settings.GameAwareShaderPath);
			break;
		case SETTING_SHADER_PRESETS:
			cellDbgFontPrintf(0.5f, menu_generalvideosettings.items[menu_generalvideosettings.items[switchvalue].enum_id].text_ypos, Emulator_GetFontSize(), Settings.ShaderPresetPath == DEFAULT_PRESET_FILE ? GREEN : ORANGE, "%s", Settings.ShaderPresetTitle);
			break;
		case SETTING_BORDER:
			{
				extract_filename_only(Settings.PS3CurrentBorder);
				cellDbgFontPrintf(0.5f, menu_generalvideosettings.items[menu_generalvideosettings.items[switchvalue].enum_id].text_ypos, Emulator_GetFontSize(), GREEN, "%s", fname_without_path_extension);
			}
			break;
		case SETTING_SHADER:
			{
				extract_filename_only(ps3graphics_get_fragment_shader_path(0));
				cellDbgFontPrintf(0.5f, menu_generalvideosettings.items[menu_generalvideosettings.items[switchvalue].enum_id].text_ypos, Emulator_GetFontSize(), GREEN, "%s", fname_without_path_extension);
			}
			break;
		case SETTING_SHADER_2:
			{
				extract_filename_only(ps3graphics_get_fragment_shader_path(1));
				cellDbgFontPrintf(0.5f, menu_generalvideosettings.items[switchvalue].text_ypos, Emulator_GetFontSize(), !(Settings.ScaleEnabled) ? SILVER : GREEN, "%s", fname_without_path_extension);
			}
			break;
		case SETTING_FONT_SIZE:
			cellDbgFontPrintf(0.5f,	menu_generalvideosettings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	Settings.PS3FontSize == 100 ? GREEN : ORANGE, "%f", Emulator_GetFontSize());
			break;
		case SETTING_SNES9X_SOUND_INPUT_RATE:
			cellDbgFontPrintf	(0.5f,	menu_emu_audiosettings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	Settings.SoundInputRate == 31950 ? GREEN : ORANGE, "%d", Settings.SoundInputRate);
			cellDbgFontDraw();
			break;
		case SETTING_SNES9X_MUTE_SOUND:
			cellDbgFontPrintf	(0.5f,	menu_emu_audiosettings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	*(menu_emu_audiosettings.items[switchvalue].setting_ptr) ? GREEN : ORANGE, *(menu_emu_audiosettings.items[switchvalue].setting_ptr) ? "OFF" : "ON");
			break;
		case SETTING_SNES9X_AUTO_APPLY_PATCH:
		case SETTING_SNES9X_AUTO_APPLY_CHEATS:
			cellDbgFontPuts(0.5f,  menu_emu_settings.items[switchvalue].text_ypos, Emulator_GetFontSize(), *(menu_emu_settings.items[switchvalue].setting_ptr) ? ORANGE : GREEN, *(menu_emu_settings.items[switchvalue].setting_ptr) ? "OFF" : "ON");
			break;
#if 0
		case SETTING_SNES9X_SKIP_FRAMES:
			if(Settings.SkipFrames == AUTO_FRAMERATE)
				cellDbgFontPuts(0.5f, menu_emu_settings.items[switchvalue].text_ypos, Emulator_GetFontSize(), GREEN, "AUTO");
			else
				cellDbgFontPrintf(0.5f, menu_emu_settings.items[switchvalue].text_ypos, Emulator_GetFontSize(), ORANGE, "%d", Settings.SkipFrames);
			break;
#endif
		case SETTING_SNES9X_FORCE_PAL:
		case SETTING_SNES9X_FORCE_NTSC:
		case SETTING_SNES9X_SRAM_WRITEPROTECT:
			cellDbgFontPuts(0.5f,  menu_emu_settings.items[switchvalue].text_ypos, Emulator_GetFontSize(), *(menu_emu_settings.items[switchvalue].setting_ptr) == 0 ? GREEN : ORANGE, *(menu_emu_settings.items[switchvalue].setting_ptr) ? "ON" : "OFF");
			cellDbgFontDraw();
			break;
		case SETTING_SNES9X_PAL_TIMING:
			cellDbgFontPrintf(0.5f, menu_emu_settings.items[switchvalue].text_ypos, Emulator_GetFontSize(), Settings.FrameTimePAL == 20000 ? GREEN : ORANGE, "%d", Settings.FrameTimePAL);
			break;
		case SETTING_SNES9X_ACCESSORY_AUTODETECTION:
			cellDbgFontPrintf	(0.5f,	menu_emu_settings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	Settings.AccessoryAutoDetection == ACCESSORY_AUTODETECTION_CONFIRM ? GREEN : ORANGE, "%s", Settings.AccessoryAutoDetection == ACCESSORY_AUTODETECTION_ENABLED ? "ON" : Settings.AccessoryAutoDetection == ACCESSORY_AUTODETECTION_CONFIRM ? "Confirm" : "OFF");
			cellDbgFontDraw();
			break;
		case SETTING_SNES9X_ACCESSORY_TYPE:
			cellDbgFontPrintf	(0.5f,	menu_emu_settings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	Settings.AccessoryAutoDetection == SETTING_SNES9X_ACCESSORY_TYPE ? ORANGE : GREEN,  Settings.AccessoryType ? "USB/Bluetooth Mouse" : "Left analog stick");
			cellDbgFontDraw();
			break;
		case SETTING_KEEP_ASPECT_RATIO:
			cellDbgFontPrintf(0.5f, menu_generalvideosettings.items[switchvalue].text_ypos, 0.91f, ps3graphics_get_aspect_ratio_float(Settings.PS3KeepAspect) == SCREEN_4_3_ASPECT_RATIO ? GREEN : ORANGE, "%s%d:%d", ps3graphics_calculate_aspect_ratio_before_game_load() ? "(Auto)" : "", (int)ps3graphics_get_aspect_ratio_int(0), (int)ps3graphics_get_aspect_ratio_int(1));
			cellDbgFontDraw();
			break;
		case SETTING_HW_TEXTURE_FILTER:
			cellDbgFontPrintf(0.5f, menu_generalvideosettings.items[switchvalue].text_ypos, Emulator_GetFontSize(), Settings.PS3Smooth ? GREEN : ORANGE, Settings.PS3Smooth ? "Linear interpolation" : "Point filtering");
			break;
		case SETTING_HW_TEXTURE_FILTER_2:
			cellDbgFontPrintf(0.5f, menu_generalvideosettings.items[switchvalue].text_ypos, Emulator_GetFontSize(), Settings.PS3Smooth2 ? GREEN : ORANGE, Settings.PS3Smooth2 ? "Linear interpolation" : "Point filtering");
			break;
		case SETTING_SCALE_FACTOR:
			cellDbgFontPrintf(0.5f,	menu_generalvideosettings.items[menu_generalvideosettings.items[switchvalue].enum_id].text_ypos,	Emulator_GetFontSize(),	Settings.ScaleFactor == 2 ? GREEN : ORANGE, "%dx", Settings.ScaleFactor);
			break;
		case SETTING_HW_OVERSCAN_AMOUNT:
			cellDbgFontPrintf	(0.5f,	menu_generalvideosettings.items[menu_generalvideosettings.items[switchvalue].enum_id].text_ypos,	Emulator_GetFontSize(),	Settings.PS3OverscanAmount == 0 ? GREEN : ORANGE, "%f", (float)Settings.PS3OverscanAmount/100);
			break;
		case SETTING_SOUND_MODE:
			cellDbgFontPuts(0.5f, menu_generalaudiosettings.items[menu_generalaudiosettings.items[switchvalue].enum_id].text_ypos, Emulator_GetFontSize(), Settings.SoundMode == SOUND_MODE_NORMAL ? GREEN : ORANGE, Settings.SoundMode == SOUND_MODE_RSOUND ? "RSound" : Settings.SoundMode == SOUND_MODE_HEADSET ? "USB/Bluetooth Headset" : "Normal");
			break;
		case SETTING_RSOUND_SERVER_IP_ADDRESS:
			cellDbgFontPuts(0.5f, menu_generalaudiosettings.items[menu_generalaudiosettings.items[switchvalue].enum_id].text_ypos, Emulator_GetFontSize(), strcmp(Settings.RSoundServerIPAddress,"0.0.0.0") ? ORANGE : GREEN, Settings.RSoundServerIPAddress);
			break;
		case SETTING_THROTTLE_MODE:
		case SETTING_ENABLE_SCREENSHOTS:
		case SETTING_TRIPLE_BUFFERING:
		case SETTING_SCALE_ENABLED:
		case SETTING_APPLY_SHADER_PRESET_ON_STARTUP:
			cellDbgFontPuts(0.5f, menu_generalvideosettings.items[menu_generalvideosettings.items[switchvalue].enum_id].text_ypos, Emulator_GetFontSize(), *(menu_generalvideosettings.items[switchvalue].setting_ptr) ? GREEN : ORANGE, *(menu_generalvideosettings.items[switchvalue].setting_ptr) ? "ON" : "OFF");
			break;
		case SETTING_EMU_CURRENT_SAVE_STATE_SLOT:
			cellDbgFontPrintf(0.5f, menu_emu_settings.items[menu_emu_settings.items[switchvalue].enum_id].text_ypos, Emulator_GetFontSize(), Settings.CurrentSaveStateSlot == 0 ? GREEN : ORANGE, "%d", Settings.CurrentSaveStateSlot);
			break;
		case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
			cellDbgFontPuts		(0.5f,	menu_pathsettings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	!(strcmp(Settings.PS3PathROMDirectory,"/")) ? GREEN : ORANGE, Settings.PS3PathROMDirectory);
			break;
		case SETTING_PATH_SAVESTATES_DIRECTORY:
			cellDbgFontPuts		(0.5f,	menu_pathsettings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	!(strcmp(Settings.PS3PathSaveStates,usrDirPath)) ? GREEN : ORANGE, Settings.PS3PathSaveStates);
			break;
		case SETTING_PATH_SRAM_DIRECTORY:
			cellDbgFontPuts		(0.5f,	menu_pathsettings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	!(strcmp(Settings.PS3PathSRAM,usrDirPath)) ? GREEN : ORANGE, Settings.PS3PathSRAM);
			break;
		case SETTING_PATH_CHEATS:
			cellDbgFontPuts		(0.5f,	menu_pathsettings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	!(strcmp(Settings.PS3PathCheats,usrDirPath)) ? GREEN : ORANGE, Settings.PS3PathCheats);
			break;
		case SETTING_DEFAULT_VIDEO_ALL:
		case SETTING_CONTROLS_DEFAULT_ALL:
		case SETTING_SAVE_SHADER_PRESET:
		case SETTING_DEFAULT_AUDIO_ALL:
		case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
			cellDbgFontDraw();
			break;
		case SETTING_CONTROLS_SCHEME:
			cellDbgFontPrintf(0.5f,   menu_controlssettings.items[switchvalue].text_ypos,   Emulator_GetFontSize(), Settings.ControlScheme == CONTROL_SCHEME_DEFAULT ? GREEN : ORANGE, Settings.PS3CurrentInputPresetTitle);
			break;
		case SETTING_CONTROLS_NUMBER:
			cellDbgFontPrintf(0.5f,	menu_controlssettings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	currently_selected_controller_menu == 0 ? GREEN : ORANGE, "%d", currently_selected_controller_menu+1);
			break;
		case SETTING_CONTROLS_DPAD_UP:
		case SETTING_CONTROLS_DPAD_DOWN:
		case SETTING_CONTROLS_DPAD_LEFT:
		case SETTING_CONTROLS_DPAD_RIGHT:
		case SETTING_CONTROLS_BUTTON_CIRCLE:
		case SETTING_CONTROLS_BUTTON_CROSS:
		case SETTING_CONTROLS_BUTTON_TRIANGLE:
		case SETTING_CONTROLS_BUTTON_SQUARE:
		case SETTING_CONTROLS_BUTTON_SELECT:
		case SETTING_CONTROLS_BUTTON_START:
		case SETTING_CONTROLS_BUTTON_L1:
		case SETTING_CONTROLS_BUTTON_R1:
		case SETTING_CONTROLS_BUTTON_L2:
		case SETTING_CONTROLS_BUTTON_R2:
		case SETTING_CONTROLS_BUTTON_L3:
		case SETTING_CONTROLS_BUTTON_R3:
		case SETTING_CONTROLS_BUTTON_L2_BUTTON_L3:
		case SETTING_CONTROLS_BUTTON_L2_BUTTON_R3:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_RIGHT:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_LEFT:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_UP:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_DOWN:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_RIGHT:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_LEFT:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_UP:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_DOWN:
		case SETTING_CONTROLS_BUTTON_R2_BUTTON_R3:
		case SETTING_CONTROLS_BUTTON_R3_BUTTON_L3:
		case SETTING_CONTROLS_ANALOG_R_UP:
		case SETTING_CONTROLS_ANALOG_R_DOWN:
		case SETTING_CONTROLS_ANALOG_R_LEFT:
		case SETTING_CONTROLS_ANALOG_R_RIGHT:
			cellDbgFontPuts(0.5f,	menu_controlssettings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	control_binds[currently_selected_controller_menu][switchvalue-(FIRST_CONTROL_BIND)] == default_control_binds[switchvalue-FIRST_CONTROL_BIND] ? GREEN : ORANGE, Input_PrintMappedButton(control_binds[currently_selected_controller_menu][switchvalue-FIRST_CONTROL_BIND]));
			break;
	}
}

#include "menu/settings-logic.h"

static void do_settings(menu * menu_obj)
{
	uint64_t state, diff_state, button_was_pressed, i;
	static uint64_t old_state = 0;

	state = cell_pad_input_poll_device(0);
	diff_state = old_state ^ state;
	button_was_pressed = old_state & diff_state;


	if(frame_count < special_action_msg_expired)
	{
	}
	else
	{
		/* back to ROM menu if CIRCLE is pressed */
		if (CTRL_L1(button_was_pressed) || CTRL_CIRCLE(button_was_pressed))
		{
			menuStackindex--;
			old_state = state;
			return;
		}

		if (CTRL_R1(button_was_pressed))
		{
			switch(menu_obj->enum_id)
			{
				case GENERAL_VIDEO_MENU:
					menuStackindex++;
					menuStack[menuStackindex] = menu_generalaudiosettings;
					old_state = state;
					break;
				case GENERAL_AUDIO_MENU:
					menuStackindex++;
					menuStack[menuStackindex] = menu_emu_settings;
					old_state = state;
					break;
				case EMU_GENERAL_MENU:
					menuStackindex++;
					menuStack[menuStackindex] = menu_emu_audiosettings;
					old_state = state;
					break;
				case EMU_AUDIO_MENU:
					menuStackindex++;
					menuStack[menuStackindex] = menu_pathsettings;
					old_state = state;
					break;
				case PATH_MENU:
					menuStackindex++;
					menuStack[menuStackindex] = menu_controlssettings;
					old_state = state;
					break;
				case CONTROLS_MENU:
					break;
			}
		}

		if (CTRL_DOWN(state) || CTRL_LSTICK_DOWN(state))	/* down to next setting */
		{
			menu_obj->selected++;

			if (menu_obj->selected >= menu_obj->max_settings)
				menu_obj->selected = menu_obj->first_setting; 

			if (menu_obj->items[menu_obj->selected].page != menu_obj->page)
				menu_obj->page = menu_obj->items[menu_obj->selected].page;

			set_text_message("", 7);
		}

		if (CTRL_UP(state) || CTRL_LSTICK_UP(state))	/* up to previous setting */
		{
			if (menu_obj->selected == menu_obj->first_setting)
				menu_obj->selected = menu_obj->max_settings-1;
			else
				menu_obj->selected--;

			if (menu_obj->items[menu_obj->selected].page != menu_obj->page)
				menu_obj->page = menu_obj->items[menu_obj->selected].page;

			set_text_message("", 7);
		}

		if (CTRL_L3(state) && CTRL_R3(state))
		{
			/* if a rom is loaded then resume it */
			if (Emulator_IsROMLoaded())
			{
				menu_is_running = 0;
				Emulator_StartROMRunning(1);
				set_text_message("", 15);
			}
			old_state = state;
			return;
		}


		producesettingentry(menu_obj->selected);
	}

	produce_menubar(menu_obj->enum_id);
	cellDbgFontDraw();

	for ( i = menu_obj->first_setting; i < menu_obj->max_settings; i++)
	{
		if(menu_obj->items[i].page == menu_obj->page)
		{
			cellDbgFontPuts(menu_obj->items[i].text_xpos, menu_obj->items[i].text_ypos, Emulator_GetFontSize(), menu_obj->selected == menu_obj->items[i].enum_id ? menu_obj->items[i].text_selected_color : menu_obj->items[i].text_unselected_color, menu_obj->items[i].text);
			producelabelvalue(i);
			cellDbgFontDraw();
		}
	}

	DisplayHelpMessage(menu_obj->selected);

	cellDbgFontPuts(0.09f, 0.91f, Emulator_GetFontSize(), YELLOW, "UP/DOWN - select  L3+R3 - resume game   X/LEFT/RIGHT - change");
	cellDbgFontPuts(0.09f, 0.95f, Emulator_GetFontSize(), YELLOW, "START - default   L1/CIRCLE - go back   R1 - go forward");
	cellDbgFontDraw();
	old_state = state;
}

static void do_ROMMenu(void)
{
	char rom_path[MAX_PATH_LENGTH], newpath[1024], *separatorslash;
	uint64_t state, diff_state, button_was_pressed;
	static uint64_t old_state = 0;

	state = cell_pad_input_poll_device(0);
	diff_state = old_state ^ state;
	button_was_pressed = old_state & diff_state;

	UpdateBrowser(&browser);

	if (CTRL_SELECT(button_was_pressed))
	{
		menuStackindex++;
		menuStack[menuStackindex] = menu_generalvideosettings;
	}

	if (CTRL_START(button_was_pressed))
		filebrowser_reset_start_directory(&browser, "/", ROM_EXTENSIONS);

	if (CTRL_CROSS(button_was_pressed))
	{
		if(FILEBROWSER_IS_CURRENT_A_DIRECTORY(browser))
		{
			/*if 'filename' is in fact '..' - then pop back directory 
			instead of adding '..' to filename path */

			if(browser.currently_selected == 0)
			{
				old_state = state;
				filebrowser_pop_directory(&browser);
			}
			else
			{
				separatorslash = (strcmp(FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser),"/") == 0) ? "" : "/";
				snprintf(newpath, sizeof(newpath), "%s%s%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser), separatorslash, FILEBROWSER_GET_CURRENT_FILENAME(browser));
				filebrowser_push_directory(&browser, newpath, true);
			}
		}
		else if (FILEBROWSER_IS_CURRENT_A_FILE(browser))
		{
			snprintf(rom_path, sizeof(rom_path), "%s/%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser), FILEBROWSER_GET_CURRENT_FILENAME(browser));

			menu_is_running = 0;

			/* switch emulator to emulate mode*/
			Emulator_StartROMRunning(1);

			Emulator_RequestLoadROM(rom_path);

			old_state = state;
			return;
		}
	}


	if (FILEBROWSER_IS_CURRENT_A_DIRECTORY(browser))
	{
		if(!strcmp(FILEBROWSER_GET_CURRENT_FILENAME(browser),"app_home") || !strcmp(FILEBROWSER_GET_CURRENT_FILENAME(browser),"host_root"))
			cellDbgFontPrintf(0.09f, 0.83f, 0.91f, RED, "WARNING - This path only works on DEX PS3 systems. Do not attempt to open\n this directory on CEX PS3 systems, or you might have to restart!");
		else if(!strcmp(FILEBROWSER_GET_CURRENT_FILENAME(browser),".."))
			cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to go back to the previous directory.");
		else
			cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to enter the directory.");
	}

	if (FILEBROWSER_IS_CURRENT_A_FILE(browser))
		cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to load the game. ");

	cellDbgFontPuts	(0.09f,	0.05f,	Emulator_GetFontSize(),	RED,	"FILE BROWSER");
	cellDbgFontPrintf (0.7f, 0.05f, 0.82f, WHITE, "%s v%s", EMULATOR_NAME, EMULATOR_VERSION);
	cellDbgFontPrintf (0.09f, 0.09f, Emulator_GetFontSize(), YELLOW,
	"PATH: %s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser));
	cellDbgFontPuts   (0.09f, 0.93f, Emulator_GetFontSize(), YELLOW,
	"L3 + R3 - resume game           SELECT - Settings screen");
	cellDbgFontDraw();

	RenderBrowser(&browser);
	old_state = state;
}

static void init_settings_pages(menu * menu_obj)
{
	int page, i, j;
	float increment;

	page = 0;
	j = 0;
	increment = 0.13f;

	for(i = menu_obj->first_setting; i < menu_obj->max_settings; i++)
	{
		if(!(j < (NUM_ENTRY_PER_PAGE)))
		{
			j = 0;
			increment = 0.13f;
			page++;
		}

		menu_obj->items[i].text_xpos = 0.09f;
		menu_obj->items[i].text_ypos = increment; 
		menu_obj->items[i].page = page;
		increment += 0.03f;
		j++;
	}
	menu_obj->refreshpage = 0;
}

void MenuInit(void)
{
	filebrowser_new(&browser, Settings.PS3PathROMDirectory, ROM_EXTENSIONS);

	init_settings_pages(&menu_generalvideosettings);
	init_settings_pages(&menu_generalaudiosettings);
	init_settings_pages(&menu_emu_settings);
	init_settings_pages(&menu_emu_audiosettings);
	init_settings_pages(&menu_pathsettings);
	init_settings_pages(&menu_controlssettings);
}

void MenuMainLoop(void)
{
	menuStack[0] = menu_filebrowser;
	menuStack[0].enum_id = FILE_BROWSER_MENU;

	menu_is_running = true;

	do
	{
		glClear(GL_COLOR_BUFFER_BIT);
		ps3graphics_draw_menu();

		switch(menuStack[menuStackindex].enum_id)
		{
			case FILE_BROWSER_MENU:
				do_ROMMenu();
				break;
			case GENERAL_VIDEO_MENU:
			case GENERAL_AUDIO_MENU:
			case EMU_GENERAL_MENU:
			case EMU_AUDIO_MENU:
			case PATH_MENU:
			case CONTROLS_MENU:
				do_settings(&menuStack[menuStackindex]);
				break;
			case GAME_AWARE_SHADER_CHOICE:
			case SHADER_CHOICE:
			case PRESET_CHOICE:
			case BORDER_CHOICE:
			case INPUT_PRESET_CHOICE:
				do_select_file(menuStack[menuStackindex].enum_id);
				break;
			case PATH_SAVESTATES_DIR_CHOICE:
			case PATH_DEFAULT_ROM_DIR_CHOICE:
			case PATH_CHEATS_DIR_CHOICE:
			case PATH_SRAM_DIR_CHOICE:
				do_pathChoice(menuStack[menuStackindex].enum_id);
				break;
		}

		psglSwap();
		cell_console_poll();
		cellSysutilCheckCallback();
	}while (menu_is_running);
}
