#include "trigger.h"
#include "analog_io.h"
#include "servo_system.hpp"

#define TRIG_A0 37

static void trig_isr() {
    // set trigger flag for each channel 
    for (int i = 0; i < 4; ++i) 
        servoes[i]->reference->clear_reference();
}

void init_trigger() {
    pinMode(TRIG_A0, INPUT_PULLUP);
    pinMode(GLOBAL_ENABLE_PIN, INPUT_PULLDOWN);
    attachInterrupt(TRIG_A0, trig_isr, RISING);
}
