#include "Buttons.h"

//----------------------------------------------
// Implementation of Button class
// implements all functions for a single button
//----------------------------------------------

void Button::begin()
{
  millis_at_last_transition = millis();
  millis_at_last_loop = millis_at_last_transition;
#ifdef DEBUG_OUTPUT_BUTTONS
  Serial.print("BUTTON begin: millis_at_last_transition and millis_at_last_loop inital set to: ");Serial.println(millis_at_last_transition);
#endif

#ifdef DEBUG_OUTPUT_BUTTONS
  Serial.print("Init button: "); Serial.println(bpin);
#endif

  pinMode(bpin, INPUT);
  // check if the button is pressed right while initializing
  down_last_time = isButtonDown();
  if (down_last_time) // set the member variable down_last_time to down_edge, because the button is pressed while initializing
  {
#ifdef DEBUG_OUTPUT_BUTTONS
    Serial.print("BUTTON begin: millis_at_last_loop: ");Serial.println(millis_at_last_loop);
    Serial.print("BUTTON begin: millis_at_last_transition: ");Serial.println(millis_at_last_transition);
    Serial.print("BUTTON begin: millis_at_last_loop - millis_at_last_transition: ");Serial.println((millis_at_last_loop - millis_at_last_transition));
    Serial.println("BUTTON begin: button_state was NOTHING yet. Change button_state to DOWN_EDGE.");
#endif
    button_state = down_edge;
  }
  else // set the member variable down_last_time to idle, because the button is not pressed while initializing
  {
#ifdef DEBUG_OUTPUT_BUTTONS
    Serial.print("BUTTON begin: millis_at_last_loop: ");Serial.println(millis_at_last_loop);
    Serial.print("BUTTON begin: millis_at_last_transition: ");Serial.println(millis_at_last_transition);
    Serial.print("BUTTON begin: millis_at_last_loop - millis_at_last_transition: ");Serial.println((millis_at_last_loop - millis_at_last_transition));
    Serial.println("BUTTON begin: button_state was NOTHING yet. Change button_state to IDLE.");
#endif    
    button_state = idle;
  }
}

void Button::loop()
{
  // Get the current tick count (starting tick) in milliseconds and store it in a member variable (for the next loop, to do non-blocking things in an interval)
  millis_at_last_loop = millis();
  // Check every loop if the button is pressed or not (digital read from pin for button) and store it in the variable down_now (true or false)
  bool down_now = isButtonDown();

#ifdef DEBUG_OUTPUT_BUTTONS
  if (down_now) {
    Serial.println("-----------------------------------------");
    Serial.print("BUTTON: Actual Button IS pressed down! Button number: ");
    Serial.println(bpin);
    Serial.print("BUTTON: previous (last loop) button state was: ");
    Serial.println(state_str[button_state]);
  }
#endif

  // Set the previous state from the member variable button_state
  state previous_state = button_state;

  //--------------------------------------------------------------------------
  // Set the button state, based on the current state and the previous state
  //--------------------------------------------------------------------------

  // Check if the button was NOT pressed while in the last loop and is also NOT pressed now
  if (down_last_time == false && down_now == false)
  {
    // Set the member button state to "idle"
    button_state = idle;
  }
  // Check if the button was NOT pressed while in the last loop and IS pressed now
  else if (down_last_time == false && down_now == true)
  {
    // Button is just pressed down!
#ifdef DEBUG_OUTPUT_BUTTONS
    Serial.println("BUTTON: Just Pressed!");
#endif
    // Set the member button state to "down_edge"
    button_state = down_edge;
#ifdef DEBUG_OUTPUT_BUTTONS
    Serial.print("BUTTON: millis_at_last_loop: ");Serial.println(millis_at_last_loop);
    Serial.print("BUTTON: millis_at_last_transition: ");Serial.println(millis_at_last_transition);
    Serial.print("BUTTON: millis_at_last_loop - millis_at_last_transition: ");Serial.println((millis_at_last_loop - millis_at_last_transition));
    Serial.println("BUTTON: Just Pressed! Change button_state to DOWN_EDGE.");
#endif
    millis_at_last_transition = millis_at_last_loop;
  }
  // Check if the button WAS pressed while in the last loop and IS also pressed now
  else if (down_last_time == true && down_now == true)
  {
#ifdef DEBUG_OUTPUT_BUTTONS
    Serial.println("BUTTON: Still pressed!");
 #endif
    // Been pressed. For how long?
#ifdef DEBUG_OUTPUT_BUTTONS
    Serial.println("BUTTON: Been pressed. For how long?");
    Serial.print("BUTTON: millis_at_last_loop: "); Serial.println(millis_at_last_loop);
    Serial.print("BUTTON: millis_at_last_transition: "); Serial.println(millis_at_last_transition);
    Serial.print("BUTTON: millis_at_last_loop - millis_at_last_transition: "); Serial.println((millis_at_last_loop - millis_at_last_transition));
    Serial.print("BUTTON: long_press_ms: "); Serial.println(long_press_ms);
#endif
    // Check if the time between the last transition and the starting tick count is greater or equal to the long press time
    if (millis_at_last_loop - millis_at_last_transition >= long_press_ms)
    {
      // Long pressed. Did we just make a transition? -> this would trigger the menu or power off stuff!
#ifdef DEBUG_OUTPUT_BUTTONS
      Serial.println("BUTTON: Long pressed. Did we just make a transition?");
#endif
      
      // Check if the previous state was "down_long_edge" or "down_long"
      // If yes, set the member variable button_state to "down_long"
      if (previous_state == down_long_edge || previous_state == down_long)
      {
#ifdef DEBUG_OUTPUT_BUTTONS
        Serial.println("BUTTON: Long Pressed - Previous state was down_long_edge or down_long!");
#endif
        // No, we already detected the edge.
#ifdef DEBUG_OUTPUT_BUTTONS
Serial.println("BUTTON: No, we already detected the edge in the last loop.");
#endif        
#ifdef DEBUG_OUTPUT_BUTTONS
        Serial.print("BUTTON: millis_at_last_loop: ");Serial.println(millis_at_last_loop);
        Serial.print("BUTTON: millis_at_last_transition: ");Serial.println(millis_at_last_transition);
        Serial.print("BUTTON: millis_at_last_loop - millis_at_last_transition: ");Serial.println((millis_at_last_loop - millis_at_last_transition));
        Serial.println("BUTTON: Long Pressed - Previous state was down_long_edge or down_long! Change button_state from ");Serial.print(state_str[button_state]);Serial.println(" to DOWN_LONG.");
#endif
        button_state = down_long;
      }
      // If no, set the member button state to "down_long_edge"
      else
      {
#ifdef DEBUG_OUTPUT_BUTTONS
        Serial.println("BUTTON: Long Pressed - Previous state was NOT down_long_edge or down_long!");
#endif
        // Previous state was something else, so this is the transition.
        // down -> down_long_edge does NOT update millis_at_last_transition.
        // We'd rather know how long it's been down than been down_long.
#ifdef DEBUG_OUTPUT_BUTTONS
        Serial.print("BUTTON: millis_at_last_loop: ");Serial.println(millis_at_last_loop);
        Serial.print("BUTTON: millis_at_last_transition: ");Serial.println(millis_at_last_transition);
        Serial.print("BUTTON: millis_at_last_loop - millis_at_last_transition: ");Serial.println((millis_at_last_loop - millis_at_last_transition));
        Serial.println("BUTTON: Long Pressed - Previous state was NOT down_long_edge or down_long! Change button_state from ");Serial.print(state_str[button_state]);Serial.println(" to DOWN_LONG_EDGE.");
#endif
        button_state = down_long_edge;
      }
    } // if (millis_at_last_loop - millis_at_last_transition >= long_press_ms)    
    else
    // Not yet long pressed
    {
#ifdef DEBUG_OUTPUT_BUTTONS
      Serial.println("BUTTON: Not yet long pressed!");
#endif
      // Not yet long pressed
#ifdef DEBUG_OUTPUT_BUTTONS
      Serial.print("BUTTON: millis_at_last_loop: ");Serial.println(millis_at_last_loop);
      Serial.print("BUTTON: millis_at_last_transition: ");Serial.println(millis_at_last_transition);
      Serial.print("BUTTON: millis_at_last_loop - millis_at_last_transition: ");Serial.println((millis_at_last_loop - millis_at_last_transition));
      Serial.println("BUTTON: Not yet long pressed. Change button_state from ");Serial.print(state_str[button_state]);Serial.println(" to DOWN.");
#endif
      button_state = down;
    }
  }
  // Check if the button WAS pressed while in the last loop and is NOT pressed NOT
  // So the button was released just now (CLICK or LONG CLICK)
  else if (down_last_time == true && down_now == false)
  {
    // Just released. From how long?
#ifdef DEBUG_OUTPUT_BUTTONS
    Serial.println("-----------------------------------------");
    Serial.println("BUTTON: Just Released! From how long?");
#endif
    
    // Check if the previous state was "down_long_edge" or "down_long"
    // So we know if the button was long pressed -> LONG CLICK
    if (previous_state == down_long_edge || previous_state == down_long)
    {
#ifdef DEBUG_OUTPUT_BUTTONS
      Serial.println("BUTTON: Just Released from a LONG PRESS - Previous state was down_long_edge or down_long!");
#endif
      // Just released from a long press.
#ifdef DEBUG_OUTPUT_BUTTONS
      Serial.print("BUTTON: millis_at_last_loop: ");Serial.println(millis_at_last_loop);
      Serial.print("BUTTON: millis_at_last_transition: ");Serial.println(millis_at_last_transition);
      Serial.print("BUTTON: millis_at_last_loop - millis_at_last_transition: ");Serial.println((millis_at_last_loop - millis_at_last_transition));
      Serial.println("BUTTON: Just Released from a long press. Change button_state from ");Serial.print(state_str[button_state]);Serial.println(" to UP_LONG_EDGE.");
#endif
      button_state = up_long_edge;
    }
    else
    // If the previous state was NOT "down_long_edge" or "down_long"
    // So the button was short pressed -> set the member button state to "up_edge" -> CLICK
    {
#ifdef DEBUG_OUTPUT_BUTTONS
      Serial.println("BUTTON: Just Released - Previous state was NOT down_long_edge or down_long!");
#endif
      // Just released from a short press.
#ifdef DEBUG_OUTPUT_BUTTONS
      Serial.print("BUTTON: millis_at_last_loop: ");Serial.println(millis_at_last_loop);
      Serial.print("BUTTON: millis_at_last_transition: ");Serial.println(millis_at_last_transition);
      Serial.print("BUTTON: millis_at_last_loop - millis_at_last_transition: ");Serial.println((millis_at_last_loop - millis_at_last_transition));
      Serial.print("BUTTON: Just Released from a short press. Change button_state from ");Serial.print(state_str[button_state]);Serial.println(" to UP_EDGE.");
#endif
      button_state = up_edge;
    }
    // Store the current tick count in the member variable millis_at_last_transition
    millis_at_last_transition = millis_at_last_loop;
#ifdef DEBUG_OUTPUT_BUTTONS
    Serial.print("BUTTON: millis_at_last_transition set to: ");Serial.println(millis_at_last_transition);
#endif
  }

  // Check if the previous state is NOT equal to the current state
  // This means that the button state has changed in this loop
  // So set the member variable state_changed to true
  state_changed = previous_state != button_state;
#ifdef DEBUG_OUTPUT_BUTTONS
  if (state_changed) {
    Serial.println("BUTTON: State changed!");
    Serial.print("BUTTON: previous state was: ");Serial.println(state_str[previous_state]);
    Serial.print("BUTTON: current state is: ");Serial.println(state_str[button_state]);
    Serial.println("-----------------------------------------");
  }
#endif

// Store the current down state in the member variable down_last_time (true or false)
  down_last_time = down_now;
#ifdef DEBUG_OUTPUT_BUTTONS
if (down_last_time) {
    Serial.println("BUTTON: down_last_time: Set to true!");    
  }
#endif
}

const String Button::state_str[Button::num_states] =
    {"idle",
     "down_edge",
     "down",
     "down_long_edge",
     "down_long",
     "up_edge",
     "up_long_edge"};

//--------------------------------------------
// Implementation of Buttons class
// Superclass for all buttons used (max 4)
//--------------------------------------------

#ifndef ONE_BUTTON_ONLY_MENU
// device has 4 buttons -> define all 4 buttons
void Buttons::begin()
{
  left.begin();
  mode.begin();
  right.begin();
  power.begin();
}

void Buttons::loop()
{
  left.loop();
  mode.loop();
  right.loop();
  power.loop();
}

bool Buttons::stateChanged()
{
  return left.stateChanged() ||
         mode.stateChanged() ||
         right.stateChanged() ||
         power.stateChanged();
}
#endif

#ifdef ONE_BUTTON_ONLY_MENU
// device has one button only -> define "mode" button only
void Buttons::begin()
{
  mode.begin();
}

void Buttons::loop()
{
  mode.loop();
}

bool Buttons::stateChanged()
{
  return mode.stateChanged();
}
#endif