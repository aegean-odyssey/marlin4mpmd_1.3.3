/**
 * Marlin 3D Printer Firmware
 * Copyright (C) 2016 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * malyanlcd.cpp
 *
 * LCD implementation for Malyan's LCD, a separate ESP8266 MCU running
 * on Serial1 for the M200 board. This module outputs a pseudo-gcode
 * wrapped in curly braces which the LCD implementation translates into
 * actual G-code commands.
 *
 * Jul 07, 18 - MCheah
 * Added for use with Delta/Malyan M300 
 * Added {P:P} {P:R} commands
 * Still some unknown command related to the wifi implementation
 * 
 * Added to Marlin for Mini/Malyan M200
 * Unknown commands as of Jan 2018: {H:}
 * Not currently implemented:
 * {E:} when sent by LCD. Meaning unknown.
 *
 * Notes for connecting to boards that are not Malyan:
 * The LCD is 3.3v, so if powering from a RAMPS 1.4 board or
 * other 5v/12v board, use a buck converter to power the LCD and
 * the 3.3v side of a logic level shifter. Aux1 on the RAMPS board
 * has Serial1 and 12v, making it perfect for this.
 * Copyright (c) 2017 Jason Nelson (xC0000005)
 */

#include "MarlinConfig.h"
#if ENABLED(MALYAN_LCD)
#include "ultralcd.h"
#include "temperature.h"
#include "planner.h"
#include "stepper.h"
#include "duration_t.h"
#include "printcounter.h"
#include "configuration_store.h"
#if ENABLED(STM32_USE_USB_CDC)
	#ifdef __cplusplus
	 extern "C" {
	#endif
	#include "usbd_cdc_interface.h"
	#ifdef __cplusplus
	}
	#endif
#endif
#include "Marlin.h"
#if ENABLED(SDSUPPORT)
  #include "cardreader.h"
#else
  #define LONG_FILENAME_LENGTH 0
#endif

typedef enum {
	MALYAN_IDLE,
	MALYAN_PRINTING,
	MALYAN_PAUSED
} MALYAN_PRINT_STATUS;

// On the Malyan M200, this will be Serial1. On a RAMPS board,
// it might not be.
#define LCD_SERIAL customizedSerial2

// This is based on longest sys command + a filename, plus some buffer
// in case we encounter some data we don't recognize
// There is no evidence a line will ever be this long, but better safe than sorry
#define MAX_CURLY_COMMAND (32 + LONG_FILENAME_LENGTH) * 2
#define LCD_MAX_FILES 64

// Track incoming command bytes from the LCD
int inbound_count;
static int listing_ind = 0;
// For sending print completion messages
MALYAN_PRINT_STATUS last_printing_status = MALYAN_IDLE;

// Everything written needs the high bit set.
void write_to_lcd_P(const char * const message) {
  char encoded_message[MAX_CURLY_COMMAND];
  uint8_t message_length = min(strlen(message), sizeof(encoded_message));
  if (DEBUGGING(COMMUNICATION))
	  BSP_CdcPrintf("-%s\n",message);
  for (uint8_t i = 0; i < message_length; i++)
    encoded_message[i] = /*pgm_read_byte*/(message[i]) | 0x80;

  LCD_SERIAL.write((uint8_t *)encoded_message, message_length);
}

void write_to_lcd(const char * const message) {
  char encoded_message[MAX_CURLY_COMMAND];
  const uint8_t message_length = min(strlen(message), sizeof(encoded_message));
  if (DEBUGGING(COMMUNICATION))
	  BSP_CdcPrintf("-%s\n",message);
  for (uint8_t i = 0; i < message_length; i++)
    encoded_message[i] = message[i] | 0x80;

  LCD_SERIAL.write((uint8_t *)encoded_message, message_length);
}

/**
 * Process an LCD 'C' command.
 * These are currently all temperature commands
 * {C:T0190}
 * Set temp for hotend to 190
 * {C:P050}
 * Set temp for bed to 50
 *
 * {C:S09} set feedrate to 90 %.
 * {C:S12} set feedrate to 120 %.
 *
 * the command portion begins after the :
 */
void process_lcd_c_command(const char* command) {
  switch (command[0]) {
    case 'S': {
      int raw_feedrate = atoi(command + 1);
      feedrate_percentage = raw_feedrate * 10;
      feedrate_percentage = constrain(feedrate_percentage, 10, 999);
    } break;
    case 'T': {
      thermalManager.setTargetHotend(atoi(command + 1), 0);
    } break;
    case 'P': {
      thermalManager.setTargetBed(atoi(command + 1));
    } break;

    default:
      SERIAL_ECHOPAIR("UNKNOWN C COMMAND", command);
      SERIAL_EOL;
      return;
  }
}

/**
 * Process an LCD 'B' command.
 * {B:0} results in: {T0:008/195}{T1:000/000}{TP:000/000}{TQ:000C}{TT:000000}
 * T0/T1 are hot end temperatures, TP is bed, TQ is percent, and TT is probably
 * time remaining (HH:MM:SS). The UI can't handle displaying a second hotend,
 * but the stock firmware always sends it, and it's always zero.
 */
void process_lcd_eb_command(const char* command) {
  duration_t elapsed;
  switch (command[0]) {
    case '0': {
	char message_buffer[MAX_CURLY_COMMAND];
      sprintf(message_buffer,
    		  PSTR("{T0:%03.0f/%03i}"),
              thermalManager.degHotend(0),
			  (int)thermalManager.degTargetHotend(0));
      write_to_lcd(message_buffer);

      sprintf(message_buffer,
    		  PSTR("{T1:000/000}{TP:%03.0f/%03i}"),
#if HAS_TEMP_BED
		  thermalManager.degBed(),
			(int)thermalManager.degTargetBed());
#else
		  0, 0));
#endif
	  write_to_lcd(message_buffer);
#if ENABLED(SDSUPPORT)
	  if (last_printing_status==MALYAN_PRINTING)
	    progress = (int)card.percentDone();
#endif
	  sprintf(message_buffer,
			  PSTR("{TQ:%03i}"),progress);

      write_to_lcd(message_buffer);
      elapsed = print_job_timer.duration();
      sprintf_P(message_buffer,
    		  PSTR("{TT:%02u%02u%02u}"),
			  uint16_t(elapsed.hour()),
			  uint16_t(elapsed.minute()) % 60,
			  uint16_t(elapsed.second()) % 60);
      write_to_lcd(message_buffer);
    } break;

    default:
      SERIAL_ECHOPAIR("UNKNOWN E/B COMMAND", command);
      SERIAL_EOL;
      return;
  }
}

/**
 * Process an LCD 'J' command.
 * These are currently all movement commands.
 * The command portion begins after the :
 * Move X Axis
 *
 * {J:E}{J:X-200}{J:E}
 * {J:E}{J:X+200}{J:E}
 * X, Y, Z, A (extruder)
 */
void process_lcd_j_command(const char* command) {
//  static bool steppers_enabled = false;
  bool isRelative = relative_mode;
  char axis = command[0];
#if ENABLED(SDSUPPORT)
  if(!card.sdprinting && !card.saving && last_printing_status!=MALYAN_PRINTING && progress<=0)
#else
  if(last_printing_status!=MALYAN_PRINTING && progress<=0)
#endif
  {
  enqueue_and_echo_command_now("G91");
  switch (axis) {
    case 'E':
      // enable or disable steppers
      // switch to relative
//TODO: E command has a tendency to break USB prints in progress if the move menu is accidentally accessed
//		Since there is no method for moving the XYZ axes manually by LCD (at least for M300), this
//    	functionality seems dangerous to leave in, as the next commands issued will think it's relative
//    	and dive into the bed
//    	enqueue_and_echo_command_now(steppers_enabled ? "M18" : "M17");
//      steppers_enabled = !steppers_enabled;
      break;
    case 'A':
      axis = 'E';
      // no break
    case 'Y':
    case 'Z':
    case 'X': {
      // G0 <AXIS><distance>
      // The M200 class UI seems to send movement in .1mm values.
      char cmd[20];
      sprintf_P(cmd, PSTR("G1 %c%03.1f"), axis, atof(command + 1) / 10.0);
      enqueue_and_echo_command_now(cmd);
    } break;
    default:
      SERIAL_ECHOPAIR("UNKNOWN J COMMAND", command);
      SERIAL_EOL;
      return;
  }
  if(!isRelative)
	  enqueue_and_echo_command_now("G90"); //Set back to absolute positioning afterwards
  }
}

/**
 * Process an LCD 'P' command, related to homing and printing.
 * Pause:
 * {P:P}
 * Printer responds with:
 * {SYS:PAUSE}
 * {SYS:PAUSED}
 * Resume:
 * {P:R}
 * Printer responds with:
 * {SYS:RESUME}
 * {SYS:RESUMED}
 * Cancel:
 * {P:X}
 *
 * Home all axes:
 * {P:H}
 *
 * Print a file:
 * {P:000}
 * The File number is specified as a three digit value.
 * Printer responds with:
 * {PRINTFILE:Mini_SNES_Bottom.gcode}
 * {SYS:BUILD}echo:Now fresh file: Mini_SNES_Bottom.gcode
 * File opened: Mini_SNES_Bottom.gcode Size: 5805813
 * File selected
 * {SYS:BUILD}
 * T:-2526.8 E:0
 * T:-2533.0 E:0
 * T:-2537.4 E:0
 * Note only the curly brace stuff matters.
 */
void process_lcd_p_command(const char* command) {

  switch (command[0]) {
    case 'X': {
      #if ENABLED(SDSUPPORT)
    	bool sdprint = card.sdprinting;
        // cancel print
        write_to_lcd_P(PSTR("{SYS:CANCELING}"));
        last_printing_status = MALYAN_IDLE;
        if(sdprint)
        	card.stopSDPrint();
        clear_command_queue();
        quickstop_stepper();
        print_job_timer.stop();
        thermalManager.disable_all_heaters();
        #if FAN_COUNT > 0
          for (uint8_t i = 0; i < FAN_COUNT; i++) fanSpeeds[i] = 0;
        #endif
        wait_for_heatup = false;
        write_to_lcd_P(PSTR("{SYS:STARTED}"));
      #endif //ENABLED(SDSUPPORT)
		//Send command to octoprint to cancel print
        MYSERIAL.write("//action:cancel\n");
      break; }
    case 'H':
      // Home all axis
    	enqueue_and_echo_command_now("G28");
      break;
    case 'P':
#if ENABLED(SDSUPPORT)
    	// pause print
    	last_printing_status = MALYAN_PAUSED;
    	write_to_lcd(PSTR(MSG_PAUSE));
    	card.pauseSDPrint();
    	write_to_lcd(PSTR(MSG_PAUSED));
		//Send command to octoprint to pause print
    	MYSERIAL.write("//action:paused\n");
#endif
    	break;
    case 'R':
#if ENABLED(SDSUPPORT)
    	// resume print
    	last_printing_status = MALYAN_PRINTING;
    	write_to_lcd(PSTR(MSG_RESUME));
    	card.startFileprint();
    	write_to_lcd(PSTR(MSG_RESUMED));
		//Send command to octoprint to resume print
    	MYSERIAL.write("//action:resumed\n");
#endif
    	break;
    default: {
      #if ENABLED(SDSUPPORT)
        // Print file 000 - a three digit number indicating which
        // file to print in the SD card. If it's a directory,
        // then switch to the directory.

        // Find the name of the file to print.
        // It's needed to echo the PRINTFILE option.
        // The {S:L} command should've ensured the SD card was mounted.
    	int fileInd = atoi(command);
    	if(fileInd==LCD_MAX_FILES-1) { //63 is a special case to continue listing the next page
            char message_buffer[MAX_CURLY_COMMAND];
            listing_ind += LCD_MAX_FILES-1;
            uint16_t file_count = card.get_num_Files();
            for (uint16_t i = listing_ind; i < MIN(file_count,listing_ind+LCD_MAX_FILES-1); i++) {
              card.getfilename(i);
              sprintf_P(message_buffer, card.filenameIsDir ? PSTR("{DIR:%.20s}") : PSTR("{FILE:%.20s}"), card.longFilename);
              write_to_lcd(message_buffer);
            }
            if(file_count-(listing_ind)>=LCD_MAX_FILES) {
            	sprintf_P(message_buffer, "{DIR:Next...}");
                write_to_lcd(message_buffer);
            }
            write_to_lcd_P(PSTR("{SYS:OK}"));
    	}
    	else {
			card.getfilename(listing_ind + fileInd);
			// There may be a difference in how V1 and V2 LCDs handle subdirectory
			// prints. Investigate more. This matches the V1 motion controller actions
			// but the V2 LCD switches to "print" mode on {SYS:DIR} response.
			if (card.filenameIsDir) {
			//TODO: traversing directories works, but selecting files within them.  Disabling folders for now
//          card.chdir(card.filename);
			  write_to_lcd_P(PSTR("{SYS:DIR}"));
			}
			else {
				write_to_lcd_P(PSTR("{SYS:BUILD}"));
			  card.openAndPrintFile(card.longFilename);
			  //Let octoprint knwo we're starting a print
			  MYSERIAL.write("//action:resumed\n");
			}
    	}

      #endif
    } break; // default
  } // switch
}

/**
 * Handle an lcd 'S' command
 * {S:I} - Temperature request
 * {T0:999/000}{T1:000/000}{TP:004/000}
 *
 * {S:L} - File Listing request
 * Printer Response:
 * {FILE:buttons.gcode}
 * {FILE:update.bin}
 * {FILE:nupdate.bin}
 * {FILE:fcupdate.flg}
 * {SYS:OK}
 */
void process_lcd_s_command(const char* command) {
  switch (command[0]) {
    case 'I': {
      // temperature information
      char message_buffer[MAX_CURLY_COMMAND];
      sprintf_P(message_buffer, PSTR("{T0:%03.0f/%03i}{T1:000/000}{TP:%03.0f/%03i}"),
        thermalManager.degHotend(0), (int)thermalManager.degTargetHotend(0),
        #if HAS_TEMP_BED
          thermalManager.degBed(), (int)thermalManager.degTargetBed()
        #else
          0, 0
        #endif
      );
      write_to_lcd(message_buffer);
    } break;

    case 'H':
      // Home all axis
      enqueue_and_echo_command("G28");
      break;

    case 'L': {
      #if ENABLED(SDSUPPORT)
        // A more efficient way to do this would be to
        // implement a callback in the ls_SerialPrint code, but
        // that requires changes to the core cardreader class that
        // would not benefit the majority of users. Since one can't
        // select a file for printing during a print, there's
        // little reason not to do it this way.
    	// MalyanLCD crashes if you send more than 20 characters
        char message_buffer[MAX_CURLY_COMMAND];
        listing_ind = 0;
        uint16_t file_count = card.get_num_Files();
        for (uint16_t i = listing_ind; i < MIN(file_count,listing_ind+LCD_MAX_FILES-1); i++) {
          card.getfilename(i);
          sprintf_P(message_buffer, card.filenameIsDir ? PSTR("{DIR:%.20s}") : PSTR("{FILE:%.20s}"), card.longFilename);
          write_to_lcd(message_buffer);
        }
        if(file_count-(listing_ind)>=LCD_MAX_FILES) {
        	sprintf_P(message_buffer, "{DIR:Next...}");
            write_to_lcd(message_buffer);
        }
        write_to_lcd_P(PSTR("{SYS:OK}"));
      #endif
    } break;

    default:
      SERIAL_ECHOPAIR("UNKNOWN S COMMAND", command);
      SERIAL_EOL;
      return;
  }
}

/**
 * Receive a curly brace command and translate to G-code.
 * Currently {E:0} is not handled. Its function is unknown,
 * but it occurs during the temp window after a sys build.
 */
void process_lcd_command(const char* command) {
  const char *current = command;
  if (DEBUGGING(COMMUNICATION))
	  BSP_CdcPrintf("%s}\n",current);
  current++; // skip the leading {. The trailing one is already gone.
  byte command_code = *current++;
  if (*current != ':') {
    SERIAL_ECHOPAIR("UNKNOWN COMMAND FORMAT", command);
    SERIAL_EOL;
    return;
  }

  current++; // skip the :

  switch (command_code) {
    case 'S':
      process_lcd_s_command(current);
      break;
    case 'J':
      process_lcd_j_command(current);
      break;
    case 'P':
      process_lcd_p_command(current);
      break;
    case 'C':
      process_lcd_c_command(current);
      break;
    case 'B':
    case 'E':
      process_lcd_eb_command(current);
      break;
    default:
//      SERIAL_ECHOLNPAIR("UNKNOWN COMMAND", command);?
      SERIAL_ECHOPAIR("UNKNOWN COMMAND", command);
      SERIAL_EOL;
      return;
  }
}

/**
 * UC means connected.
 * UD means disconnected
 * The stock firmware considers USB initialied as "connected."
 */
void update_usb_status(const bool forceUpdate) {
  static bool last_usb_connected_status = false;
  // This is mildly different than stock, which
  // appears to use the usb discovery status.
  // This is more logical.
  if (forceUpdate) {
#if ENABLED(STM32_USE_USB_CDC)
	last_usb_connected_status = CDC_Itf_IsConnected();
#else
	last_usb_connected_status = false;
#endif
    write_to_lcd_P(last_usb_connected_status ? PSTR("{R:UC}\r\n") : PSTR("{R:UD}\r\n"));
  }
}

/**
 * - from printer on startup:
 * {SYS:STARTED}{VER:29}{SYS:STARTED}{R:UD}
 * The optimize attribute fixes a register Compile
 * error for amtel.
 */
void lcd_update() {
  static char inbound_buffer[MAX_CURLY_COMMAND];

  // First report USB status.
  update_usb_status(false);

  // now drain commands...
  while (LCD_SERIAL.available()) {
    const byte b = (byte)LCD_SERIAL.read() & 0x7F;
    inbound_buffer[inbound_count++] = b;
    if (b == '}' || inbound_count == sizeof(inbound_buffer) - 1) {
      inbound_buffer[inbound_count - 1] = '\0';
      process_lcd_command(inbound_buffer);
      inbound_count = 0;
      inbound_buffer[0] = 0;
    }
  }

  #if ENABLED(SDSUPPORT)
    // The way last printing status works is simple:
    // The UI needs to see at least one TQ which is not 100%
    // and then when the print is complete, one which is.
  if(card.updateLCD) {
    if (card.sdprinting || card.saving) {
        if (card.percentDone() != progress) {
        char message_buffer[10];
        progress = card.percentDone();
        sprintf_P(message_buffer, PSTR("{TQ:%03i}"), (int)progress);
        write_to_lcd(message_buffer);

        if (last_printing_status==MALYAN_IDLE) last_printing_status = MALYAN_PRINTING;
      }
    }
    else {
      // If there was a print in progress, we need to emit the final
      // print status as {TQ:100}. Reset last percent done so a new print will
      // issue a percent of 0.
      if (last_printing_status==MALYAN_PRINTING) {
        last_printing_status = MALYAN_IDLE;
        progress = 0;
        write_to_lcd_P(PSTR(MSG_COMPLETE));
      }
    }
  }
  #endif
}

/**
 * The Malyan LCD actually runs as a separate MCU on Serial 1.
 * This code's job is to siphon the weird curly-brace commands from
 * it and translate into gcode, which then gets injected into
 * the command queue where possible.
 */
void lcd_init() {
  inbound_count = 0;
  LCD_SERIAL.begin(510638);

  // Signal init
  write_to_lcd_P(PSTR("{SYS:STARTED}\r\n"));

  // send a version that says "unsupported"
  write_to_lcd_P(PSTR("{VER:133}\r\n"));
  HAL_Delay(2000); //Leave version on the screen
  // No idea why it does this twice.
  write_to_lcd_P(PSTR("{SYS:STARTED}\r\n"));
  update_usb_status(true);
}

/**
 * Set an alert.
 */
void lcd_setalertstatusPGM(const char* message) {
  char message_buffer[MAX_CURLY_COMMAND];
  sprintf_P(message_buffer, PSTR("{E:%s}"), message);
  write_to_lcd(message_buffer);
}
/**
 * Send an arbitrary message
 */
void lcd_setstatus(const char* message, const bool persist) {
	write_to_lcd(message);
}
void lcd_setstatuspgm(const char* message, uint8_t level) {
	write_to_lcd_P(message);
}
void lcd_setpercent(uint8_t percent) {
	  progress = percent;
	  char message_buffer[10];
      sprintf_P(message_buffer, PSTR("{TQ:%03i}"), (int)progress);
      lcd_setstatus(message_buffer);
	  if(percent==0)
		lcd_setstatuspgm(PSTR(MSG_BUILD));
	  else if(percent>=100)
	  {
		lcd_setstatuspgm(PSTR(MSG_COMPLETE));
		progress = 0;
	  }
}


#endif // MALYAN_LCD
