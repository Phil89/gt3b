/*
    menu - handle showing to display, menus
    Copyright (C) 2011 Pavel Semerad

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/



#include <string.h>
#include "menu.h"
#include "config.h"
#include "calc.h"
#include "timer.h"
#include "ppm.h"
#include "lcd.h"
#include "buzzer.h"
#include "input.h"




#define TRIM_MAX  99
#define SUBTRIM_MAX 99




// flags for wakeup after each ADC measure
_Bool menu_takes_adc;
_Bool menu_wants_battery;
// battery low flag
_Bool menu_battery_low;



// key beep if enabled in config
static void key_beep(void) {
    if (cg.key_beep)  beep(1);
}




// calibrate menu
static void calibrate(void) {
    u8 channel = 1;
    u16 last_val = 0xffff;
    u16 val;
    u8 seg;

    menu_takes_adc = 1;

    // cleanup screen and disable possible low bat warning
    buzzer_off();
    menu_battery_low = 0;	// it will be set automatically again
    backlight_set_default(BACKLIGHT_MAX);
    backlight_on();
    lcd_clear();

    btnra();

    // show intro text
    lcd_chars("CAL");
    lcd_update();
    delay_menu(200);

    // show channel number and not-yet calibrated values
    lcd_segment(LS_SYM_CHANNEL, LS_ON);
    lcd_7seg(channel);
    lcd_menu(LM_MODEL | LM_NAME | LM_REV | LM_TRIM | LM_DR | LM_EXP);
    lcd_set_blink(LMENU, LB_SPC);

    while (1) {
	// check keys
	if (btnl(BTN_BACK))  break;
	if (btn(BTN_END | BTN_ROT_ALL)) {
	    if (btn(BTN_BACK))  key_beep();
	    // change channel number
	    if (btn(BTN_ROT_L)) {
		// down
		if (!--channel)  channel = 4;
	    }
	    else {
		// up
		if (++channel > 4)  channel = 1;
	    }
	    lcd_7seg(channel);
	    lcd_update();
	}
	else if (btn(BTN_ENTER)) {
	    // save calibrate value for channels 1 and 2
	    if (channel == 1) {
		key_beep();
		val = (adc_steering_ovs + ADC_OVS_RND) >> ADC_OVS_SHIFT;
		if (val < CALIB_ST_LOW_MID) {
		    cg.calib_steering_left = val;
		    seg = LS_MENU_MODEL;
		}
		else if (val <= CALIB_ST_MID_HIGH) {
		    cg.calib_steering_mid = val;
		    seg = LS_MENU_NAME;
		}
		else {
		    cg.calib_steering_right = val;
		    seg = LS_MENU_REV;
		}
		lcd_segment(seg, LS_OFF);
		lcd_update();
	    }
	    else if (channel == 2) {
		key_beep();
		val = (adc_throttle_ovs + ADC_OVS_RND) >> ADC_OVS_SHIFT;
		if (val < CALIB_TH_LOW_MID) {
		    cg.calib_throttle_fwd = val;
		    seg = LS_MENU_TRIM;
		}
		else if (val <= CALIB_TH_MID_HIGH) {
		    cg.calib_throttle_mid = val;
		    seg = LS_MENU_DR;
		}
		else {
		    cg.calib_throttle_bck = val;
		    seg = LS_MENU_EXP;
		}
		lcd_segment(seg, LS_OFF);  // set corresponding LCD off
		lcd_update();
	    }
	}

	// show ADC value if other than last val
	if (channel == 4)  val = adc_battery;
	else  val = (adc_all_ovs[channel-1] + ADC_OVS_RND) >> ADC_OVS_SHIFT;
	if (val != last_val) {
	    last_val = val;
	    lcd_char_num3(val);
	    lcd_update();
	}

	btnra();
	stop();
    }

    menu_takes_adc = 0;
    beep(60);
    lcd_menu(0);
    lcd_update();
    config_global_save();
    backlight_set_default(cg.backlight_time);
    backlight_on();
}


// key test menu
static u8 *key_ids[] = {
    "T1L", "T1R",
    "T2F", "T2B",
    "T3-", "T3+",
    "DR-", "DR+",
    "ENT",
    "BCK",
    "END",
    "CH3",
    "ROL", "ROR"
};
static void key_test(void) {
    u8 i;
    u16 bit;
    
    // cleanup screen and disable possible low bat warning
    buzzer_off();
    menu_battery_low = 0;	// it will be set automatically again

    // do full screen blink
    lcd_set_full_on();
    delay_menu(200);
    lcd_clear();

    btnra();

    // show intro text
    lcd_chars("KEY");
    lcd_update_stop();		// wait for key

    while (1) {
	if (btnl(BTN_BACK))  break;

	for (i = 0, bit = 1; i < 14; i++, bit <<= 1) {
	    if (btn(bit)) {
		key_beep();
		lcd_chars(key_ids[i]);
		if (btnl(bit))  lcd_7seg(1);
		else lcd_set(L7SEG, LB_EMPTY);
		lcd_update();
		break;
	    }
	}
	btnra();
	stop();
    }
    key_beep();
}





// load model config from eeprom and set model settings
static void load_model(void) {
    config_model_read();

    ppm_set_channels(cm.channels);
}


// show model number, extra function to handle more than 10 models
static void show_model_number(u8 model) {
    lcd_7seg((u8)(model % 10));
    if (model >= 40) {
	// too much, set blinking arrows
	lcd_segment(LS_SYM_RIGHT, LS_ON);
	lcd_segment(LS_SYM_LEFT,  LS_ON);
	lcd_segment_blink(LS_SYM_RIGHT, LB_SPC);
	lcd_segment_blink(LS_SYM_LEFT,  LB_SPC);
    }
    else {
	lcd_segment(LS_SYM_RIGHT, (u8)((u8)(model / 10) & 1));
	lcd_segment(LS_SYM_LEFT, (u8)((u8)(model / 20) & 1));
    }
}


// menu stop - checks low battery
static void menu_stop(void) {
    static _Bool battery_low_on;
    stop();
    if (battery_low_on == menu_battery_low)  return;  // no change

    // battery low status changed
    if (menu_battery_low) {
	// battery low firstly
	battery_low_on = 1;
	lcd_segment(LS_SYM_LOWPWR, LS_ON);
	lcd_segment_blink(LS_SYM_LOWPWR, LB_SPC);
	buzzer_on(40, 160, BUZZER_MAX);
    }
    else {
	// battery low now OK
	battery_low_on = 0;
	lcd_segment(LS_SYM_LOWPWR, LS_OFF);
	buzzer_off();
    }
    lcd_update();
}





// show main screen (model number and name/battery/...)
#define MS_NAME		0
#define MS_BATTERY	1
static void main_screen(u8 item) {
    lcd_segment(LS_SYM_MODELNO, LS_ON);
    lcd_segment(LS_SYM_CHANNEL, LS_OFF);
    lcd_segment(LS_SYM_PERCENT, LS_OFF);
    show_model_number(cg.model);

    // chars is item dependent
    if (item == MS_NAME) {
	// model name
	lcd_segment(LS_SYM_DOT, LS_OFF);
	lcd_segment(LS_SYM_VOLTS, LS_OFF);
	lcd_chars(cm.name);
    }
    else if (item == MS_BATTERY) {
	// battery voltage
	lcd_segment(LS_SYM_DOT, LS_ON);
	lcd_segment(LS_SYM_VOLTS, LS_ON);
	lcd_chars("XXX");
    }
    lcd_update();
}




// common menu processing, selecting channel and then changing values
static _Bool menu_adc_direction;
static void menu_set_adc_direction(u8 channel) {
    u16 adc, calm;

    // for channel 2 use throttle, for others steering
    if (channel == 2) {
	adc = adc_throttle_ovs;
	calm = cg.calib_throttle_mid;
    }
    else {
	adc = adc_steering_ovs;
	calm = cg.calib_steering_mid;
    }
    // if over threshold to one side, set menu_adc_direction
    if (adc < ((calm - 40) << ADC_OVS_SHIFT)) {
	if (menu_adc_direction) {
	    menu_adc_direction = 0;
	    lcd_segment(LS_SYM_LEFT, LS_ON);
	    lcd_segment(LS_SYM_RIGHT, LS_OFF);
	}
    }
    else if (adc > ((calm + 40) << ADC_OVS_SHIFT)) {
	if (!menu_adc_direction) {
	    menu_adc_direction = 1;
	    lcd_segment(LS_SYM_LEFT, LS_OFF);
	    lcd_segment(LS_SYM_RIGHT, LS_ON);
	}
    }
}
static void menu_channel(u8 end_channel, u8 use_adc, void (*subfunc)(u8, u8)) {
    u8 channel = 1;
    u8 chan_val = 0;			// now in channel
    u8 last_direction = menu_adc_direction;

    if (use_adc) {
	menu_takes_adc = 1;
	menu_set_adc_direction(channel);
    }

    // show CHANNEL
    lcd_segment(LS_SYM_MODELNO, LS_OFF);
    lcd_segment(LS_SYM_LEFT, LS_OFF);
    lcd_segment(LS_SYM_RIGHT, LS_OFF);
    lcd_segment(LS_SYM_CHANNEL, LS_ON);

    lcd_7seg(channel);
    lcd_set_blink(L7SEG, LB_SPC);
    subfunc(channel, 0);		// show current value
    lcd_update();

    while (1) {
	btnra();
	menu_stop();

	if (btn(BTN_BACK | BTN_ENTER))  break;

	last_direction = menu_adc_direction;
	if (use_adc)  menu_set_adc_direction(channel);

	if (btn(BTN_ROT_ALL)) {
	    if (chan_val) {
		// change value
		subfunc(channel, 1);
	    }
	    else {
		// change channel number
		if (btn(BTN_ROT_L)) {
		    if (!--channel)  channel = end_channel;
		}
		else {
		    if (++channel > end_channel)  channel = 1;
		}
		lcd_7seg(channel);
		lcd_set_blink(L7SEG, LB_SPC);
		subfunc(channel, 0);
		lcd_update();
	    }
	    last_direction = menu_adc_direction;  // was already showed
	}

	else if (btn(BTN_END)) {
	    // switch channel/value
	    if (chan_val) {
		// switch to channel number
		lcd_set_blink(L7SEG, LB_OFF);
		lcd_set_blink(LCHR1, LB_SPC);
		lcd_set_blink(LCHR2, LB_SPC);
		lcd_set_blink(LCHR3, LB_SPC);
		chan_val = 0;
	    }
	    else {
		// switch to value
		lcd_set_blink(L7SEG, LB_SPC);
		lcd_set_blink(LCHR1, LB_OFF);
		lcd_set_blink(LCHR2, LB_OFF);
		lcd_set_blink(LCHR3, LB_OFF);
		chan_val = 1;
	    }
	}

	if (last_direction != menu_adc_direction) {
	    // show other dir value
	    subfunc(channel, 0);
	    lcd_update();
	}
    }

    menu_takes_adc = 0;
    key_beep();
    config_model_save();
}


// change value 
static s16 menu_change_val(s16 val, s16 min, s16 max) {
    u8 amount = 1;

    if (btn(BTN_ROT_L)) {
	// left
	if (btnl(BTN_ROT_L))  amount = 5;
	val -= amount;
	if (val < min)  val = min;
    }
    else {
	// right
	if (btnl(BTN_ROT_R))  amount = 5;
	val += amount;
	if (val > max)  val = max;
    }
    return val;
}




// global setup
static void global_setup(void) {
    // XXX
}




// selected submenus
// select model/save model as (to selected model position)
static void menu_model(u8 saveas) {
    s8 model = (s8)cg.model;
    u8 amount;

    if (saveas)  lcd_set_blink(LMENU, LB_SPC);
    lcd_set_blink(L7SEG, LB_SPC);

    while (1) {
	btnra();
	menu_stop();

	if (btn(BTN_ENTER | BTN_BACK))  break;
	if (btn(BTN_ROT_ALL)) {
	    amount = 1;
	    if (btn(BTN_ROT_L)) {
		if (btnl(BTN_ROT_L))  amount = 2;
		model -= amount;
		if (model < 0)  model += CONFIG_MODEL_MAX;
	    }
	    else {
		if (btnl(BTN_ROT_R))  amount = 2;
		model += amount;
		if (model >= CONFIG_MODEL_MAX)  model -= CONFIG_MODEL_MAX;
	    }
	    show_model_number(model);
	    lcd_set_blink(L7SEG, LB_SPC);
	    lcd_chars(config_model_name(model));
	    lcd_update();
	}
    }

    key_beep();
    // if new model choosed, save it
    if ((u8)model != cg.model) {
	cg.model = (u8)model;
	config_global_save();
	if (saveas) {
	    // save to new model position
	    config_model_save();
	}
	else {
	    // load selected model
	    load_model();
	    awake(CALC);	// must be awaked to do first PPM calc
	}
    }
    if (saveas)  lcd_set_blink(LMENU, LB_OFF);
}

// change model name
static void menu_name(void) {
    u8 pos = LCHR1;
    u8 letter = cm.name[0];

    lcd_set_blink(LCHR1, LB_SPC);

    while (1) {
	btnra();
	menu_stop();

	if (btn(BTN_ENTER | BTN_BACK))  break;
	if (btn(BTN_END)) {
	    key_beep();
	    // to next char
	    lcd_set_blink(pos, LB_OFF);
	    if (++pos > LCHR3)  pos = LCHR1;
	    lcd_set_blink(pos, LB_SPC);
	    lcd_update();
	    letter = cm.name[pos];
	}
	else if (btn(BTN_ROT_ALL)) {
	    // change letter
	    if (btn(BTN_ROT_L)) {
		// lower
		if (letter == '0')      letter = 'Z';
		else if (letter == 'A')	letter = '9';
		else                    letter--;
	    }
	    else {
		// upper
		if (letter == '9')      letter = 'A';
		else if (letter == 'Z')	letter = '0';
		else                    letter++;
	    }
	    cm.name[pos] = letter;
	    lcd_char(pos, letter);
	    lcd_set_blink(pos, LB_SPC);
	    lcd_update();
	}
    }

    key_beep();
    config_model_save();
}

// reset model to default values
static void menu_reset_model(void) {
    config_model_set_default();
    config_model_save();
}

// set reverse
void sf_reverse(u8 channel, u8 change) {
    u8 bit = (u8)(1 << (channel - 1));
    if (change)  cm.reverse ^= bit;
    if (cm.reverse & bit)  lcd_chars("REV");
    else                   lcd_chars("NOR");
}
@inline static void menu_reverse(void) {
    menu_channel(MAX_CHANNELS, 0, sf_reverse);
}

// set endpoints
void sf_endpoint(u8 channel, u8 change) {
    u8 *addr = &cm.endpoint[channel][menu_adc_direction];
    if (change)  *addr = (u8)menu_change_val(*addr, 0, cg.endpoint_max);
    lcd_char_num3(*addr);
}
@inline static void menu_endpoint(void) {
    menu_channel(MAX_CHANNELS, 1, sf_endpoint);
}

// set trims
static void sf_trim(u8 channel, u8 change) {
    s8 *addr = &cm.trim[channel];
    if (change)  *addr = (s8)menu_change_val(*addr, -TRIM_MAX, TRIM_MAX);
    if (channel == 1)  lcd_char_num2_lbl(*addr, "LNR");
    else               lcd_char_num2_lbl(*addr, "FNB");
}
@inline static void menu_trim(void) {
    menu_channel(2, 0, sf_trim);
}

// set subtrims
static void sf_subtrim(u8 channel, u8 change) {
    s8 *addr = &cm.subtrim[channel];
    if (change)  *addr = (s8)menu_change_val(*addr, -SUBTRIM_MAX, SUBTRIM_MAX);
    lcd_char_num2(*addr);
}
static void menu_subtrim(void) {
    lcd_set_blink(LMENU, LB_SPC);
    menu_channel(MAX_CHANNELS, 0, sf_subtrim);
    lcd_set_blink(LMENU, LB_OFF);
}

// set dualrate
static void sf_dualrate(u8 channel, u8 change) {
    u8 *addr = &cm.dualrate[channel];
    if (change)  *addr = (u8)menu_change_val(*addr, 0, 100);
    lcd_char_num3(*addr);
}
@inline static void menu_dualrate(void) {
    menu_channel(2, 0, sf_dualrate);
}

// set expos
static void sf_expo(u8 channel, u8 change) {
    s8 *addr = &cm.expo[channel];
    if (change)  *addr = (s8)menu_change_val(*addr, -99, 99);
    lcd_char_num2(*addr);
}
@inline static void menu_expo(void) {
    menu_channel(3, 0, sf_expo);
}

// set abs
static void menu_abs(void) {
    // XXX
}




// choose from menu items
static void select_menu(void) {
    u8 menu = LM_MODEL;
    lcd_menu(menu);
    main_screen(MS_NAME);	// show model number and name

    while (1) {
	btnra();
	menu_stop();

	// Back key
	if (btn(BTN_BACK))  break;

	// Enter key
	if (btn(BTN_ENTER)) {
	    key_beep();
	    if (menu == LM_MODEL)	menu_model((u8)(btnl(BTN_ENTER) ? 1 : 0));
	    else if (menu == LM_NAME) {
		if (btnl(BTN_ENTER))	menu_reset_model();
		else			menu_name();
	    }
	    else if (menu == LM_REV)	menu_reverse();
	    else if (menu == LM_EPO)	menu_endpoint();
	    else if (menu == LM_TRIM) {
		if (btnl(BTN_ENTER))	menu_subtrim();
		else			menu_trim();
	    }
	    else if (menu == LM_DR)	menu_dualrate();
	    else if (menu == LM_EXP)	menu_expo();
	    else 			menu_abs();
	    main_screen(MS_NAME);	// show model number and name
	    if (btn(BTN_BACK))  break;	// and exit when BACK
	}

	// rotate keys
	else if (btn(BTN_ROT_ALL)) {
	    if (btn(BTN_ROT_R)) {
		menu >>= 1;
		if (!menu)  menu = LM_MODEL;
	    }
	    else {
		menu <<= 1;
		if (!menu)  menu = LM_ABS;
	    }
	    lcd_menu(menu);
	    lcd_update();
	}
    }

    key_beep();
    lcd_menu(0);
}


// main menu loop, shows main screen and handle keys
static void menu_loop(void) {
    u8 item = MS_NAME;

    lcd_clear();
    main_screen(item);

    while (1) {
	btnra();
	menu_stop();

	// Enter long key
	if (btnl(BTN_ENTER)) {
	    key_beep();
	    if (adc_steering_ovs > (CALIB_ST_MID_HIGH << ADC_OVS_SHIFT))
		calibrate();
	    else if (adc_steering_ovs < (CALIB_ST_LOW_MID << ADC_OVS_SHIFT))
		key_test();
	    else global_setup;
	}

	// Enter key
	else if (btn(BTN_ENTER)) {
	    key_beep();
	    select_menu();
	    main_screen(item);
	}

	// trims

	// dualrate

	// channel 3 button

	// rotate encoder - change model name/battery/...
	else if (btn(BTN_ROT_ALL)) {
	    item = (u8)(1 - item);	// only name/battery now
	    main_screen(item);
	}
    }
}



// read config from eeprom, initialize and call menu loop
void menu_init(void) {
    // variables

    // read global config from eeprom, if calibrate values changed,
    //   call calibrate
    if (config_global_read())
	calibrate();
    // apply global settings
    button_autorepeat(cg.autorepeat);
    backlight_set_default(cg.backlight_time);
    backlight_on();

    // read model config from eeprom
    load_model();

    // and main loop
    menu_loop();
}

