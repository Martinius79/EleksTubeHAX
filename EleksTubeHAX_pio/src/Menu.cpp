#include "Menu.h"

// Big ol' state machine: menu and buttons as the state, buttons as the transition triggers.

// The menu is a state machine.  It has a number of states, and a number of transitions between those states.
// The transitions are triggered by the buttons.  The buttons are also a state machine, with a number of states
// and transitions between those states.  The buttons are polled in the main loop, and the menu is updated based
// on the button state transitions.

// We are only watching for the edge transitions of the buttons!.
// the button passes the down_edge state only once, when the button is pressed down.
// The button passes the up_edge state only once, when the button is released.
// The button passes the down_long_edge state only once, when the button is pressed down for a long time.
// The button passes the up_long_edge state only once, when the button is released after a long press.


#ifndef ONE_BUTTON_ONLY_MENU
void Menu::loop(Buttons &buttons)
{
  Button::state left_state = buttons.left.getState();   // decrement value
  Button::state right_state = buttons.right.getState(); // increment value
  Button::state power_state = buttons.power.getState(); // exit menu
  Button::state mode_state = buttons.mode.getState();   // next menu

  // Reset the change value in every case.  We don't always change the state though.
  change = 0;
  state_changed = false;

  // Early out for idle state, which will be most of the time.
  if (state == idle && left_state == Button::idle && right_state == Button::idle && mode_state == Button::idle)
  {
    // Everything is idle.
    return;
  }

  // Go idle if the user hasn't pressed a button in a long time.
  if (state != idle && millis() - millis_last_button_press > idle_timeout_ms)
  {
    // Go idle.
    state = idle;
    state_changed = true;
#ifdef DEBUG_OUTPUT_MENU
        Serial.println("MENU: Go idle if the user hasn't pressed a button in a long time.");
#endif
    return;
  }

  // Menu is idle. A button is pressed, go into the menu, but don't act on the button press. It just wakes up the menu.
  if (state == idle && (left_state == Button::down_edge || right_state == Button::down_edge || mode_state == Button::down_edge))
  {
    state = states(1); // Start at the beginning of the menu.

    millis_last_button_press = millis();
    state_changed = true;
#ifdef DEBUG_OUTPUT_MENU
    Serial.println("MENU: Menu is idle. A button is pressed, go into the menu, but don't act on the button press. It just wakes up the menu.");
#endif
    return;
  }

  // Go to the next menu option
  if (state != idle && mode_state == Button::down_edge)
  {
    uint8_t new_state = (uint8_t(state) + 1) % num_states;
    if (new_state == 0)
    {
      new_state = 1; // Skip over idle when incrementing through the menu.
    }
    state = states(new_state);

    millis_last_button_press = millis();
    state_changed = true;
#ifdef DEBUG_OUTPUT_MENU
        Serial.print("MENU: Go to the next menu option! New menu_state: ");Serial.println(menu_state);
#endif
    return;
  }

  // Exit with a power button.
  if (state != idle && (power_state == Button::down_edge))
  {
    state = idle;
    state_changed = true;
#ifdef DEBUG_OUTPUT_MENU
    Serial.println("MENU: Exit with a power button.");
#endif
    return;
  }

  // In a menu, and a left (negative change value) or right button (positive change value) has been pressed!
  if (state != idle && (left_state == Button::down_edge || right_state == Button::down_edge))
  {
    // Pressing both left and right at the same time cancels out?  Sure, why not...
    if (left_state == Button::down_edge)
    {
#ifdef DEBUG_OUTPUT_MENU
      Serial.println("MENU: In a menu, and a left (negative change value) button has been pressed!");
#endif
      change--;
    }
    if (right_state == Button::down_edge)
    {
#ifdef DEBUG_OUTPUT_MENU
      Serial.println("MENU: In a menu, and a right (positive change value) button has been pressed!");
#endif
      change++;
    }

    millis_last_button_press = millis();
    state_changed = true;
    return;
  }
  // Some other button state, but it doesn't trigger any change in state.  There are LOTS of states that will
  // get here, but I think they're all "just do nothing."  If there's an explicit state we want to handle,
  // add an if() block above.
}
#endif

#ifdef ONE_BUTTON_ONLY_MENU
void Menu::loop(Buttons &buttons)
{
  Button::state mode_state = buttons.mode.getState(); // next menu

  // Reset the change value in every case.  We don't always change the state though.
  change = 0;
  state_changed = false;

  // Early out for idle state, which will be most of the time.
  if (state == idle && mode_state == Button::idle)
  {
    // Everything is idle. Do nothing.
    return;
  }

  // Go idle if the user hasn't pressed a button in a long time.
  if (state != idle && millis() - millis_last_button_press > idle_timeout_ms)
  {
    // Go idle.
    state = idle;
    state_changed = true;
    #ifdef DEBUG_OUTPUT_MENU
    Serial.println("MENU: Go idle if the user hasn't pressed a button in a long time.");
    Serial.println("-----------------------------------------------------------------------------------");
    #endif
    return;
  }

  // Menu is idle. A button is pressed, go into the menu, but don't act on the button press. It just wakes up the menu.
  if (state == idle && (mode_state == Button::up_edge))
  {
    state = states(1); // Start at the beginning of the menu.

    millis_last_button_press = millis();
    state_changed = true;
    #ifdef DEBUG_OUTPUT_MENU
    Serial.println("MENU: Menu is idle. A button is pressed, go into the menu (STATE CHANGED!), but don't act on the button press. It just wakes up the menu.");
    Serial.print("MENU: menu state is now: ");Serial.println(state);
    #endif
    return;
  }

  // In a menu, and button LONG pressed! -> simulate right button press
  /// Must be done BEFORE the next menu option
  if (state != idle && (mode_state == Button::down_long_edge))
  {
    change++;

    millis_last_button_press = millis();
    state_changed = true;
    #ifdef DEBUG_OUTPUT_MENU
    Serial.println("MENU: In a menu, and button LONG pressed! -> simulate right button press!");
    Serial.println("-----------------------------------------------------------------------------------");
    #endif
    return;
  }

  // In a menu, and button SHORT pressed! -> go to next menu option
  // Go to the next menu option
  if (state != idle && mode_state == Button::up_edge)
  {
    uint8_t new_state = (uint8_t(state) + 1) % num_states;
    if (new_state == 0)
    {
      new_state = 1; // Skip over idle when incrementing through the menu.
    }
    state = states(new_state);

    millis_last_button_press = millis();
    state_changed = true;
    #ifdef DEBUG_OUTPUT_MENU
    Serial.println("MENU: In a menu, and button SHORT pressed!");
    Serial.print("MENU: Go to the next menu option! New menu_state: ");Serial.println(state);
    Serial.println("-----------------------------------------------------------------------------------");
    #endif
    return;
  }

  // Some other button state, but it doesn't trigger any change in state.  There are LOTS of states that will
  // get here, but I think they're all "just do nothing."  If there's an explicit state we want to handle,
  // add an if() block above.
}
#endif

// menu has one more point of entry, if WPS is used
#ifndef WIFI_USE_WPS
const String Menu::state_str[Menu::num_states] = {
    "idle",
    "backlight_pattern",
    "pattern_color",
    "backlight_intensity",
    "twelve_hour",
    "blank_hours_zero",
    "utc_offset_hour",
    "utc_offset_15m",
    "selected_graphic"};
#else
const String Menu::state_str[Menu::num_states] = {
    "idle",
    "backlight_pattern",
    "pattern_color",
    "backlight_intensity",
    "twelve_hour",
    "blank_hours_zero",
    "utc_offset_hour",
    "utc_offset_15m",
    "selected_graphic",
    "start_wps"};
#endif
