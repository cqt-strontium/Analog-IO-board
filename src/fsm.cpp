#include <cstdlib>
#include "analog_io.h"
#include "servo/intensity_servo_helper.hpp"
#include "servo/controllers/iir_controller.hpp"
#include "serial_reader.hpp"
#include "servo/servo_system.hpp"
#include "tinyfsm.hpp"
#include "trigger.h"

struct SerialEvent : tinyfsm::Event {};
struct TurnOnSweep : tinyfsm::Event {};
struct TurnOnServo : tinyfsm::Event {};
struct TurnReadLim : tinyfsm::Event {};

SerialEvent ser;
TurnOnServo servo;
TurnOnSweep sweep;
struct ServoMachine;

struct Servo;
struct Sweep;

using fsm_handle = ServoMachine;

struct ServoMachine : tinyfsm::Fsm<ServoMachine> {
    static Controller *sweep_c;
    virtual void react(SerialEvent const &) {}
    virtual void react(TurnOnServo const &) {}
    virtual void react(TurnOnSweep const &) {}
    virtual void entry(void) {} /* entry actions in some states */
    void exit(void) {}          /* no exit actions */
};

static void servo_loop() {
    if (digitalReadFast(GLOBAL_ENABLE_PIN)) {
        for (int ch = 0; ch < 4; ++ch)
            if (servoes[ch]->on)
                servoes[ch]->writer(servoes[ch]->hold_output);
        return;
    }
    for (int ch = 0; ch < 4; ++ch) {
        if (servoes[ch]->on)
            servoes[ch]->update();
    }
}

static void channel_parser(uint8_t ch) {
    if (ch & (1 << 7))
        servoes[ch & 3]->on = true;
    else
        servoes[ch & 3]->on = false;
    for (int i = 0; i < 4; ++i)
        Serial.printf("CH%d: %s; ", i, (servoes[i]->on) ? "*" : "o");
    Serial.printf("\n");
}

struct Idle : ServoMachine {
    void entry() override {
    }
    void react(SerialEvent const &) override {
        uint8_t c  = SerialReader();
        uint8_t ch = SerialReader();
        switch (c) {
            case CHANNEL: {
                channel_parser(ch);
                transit<Servo>();
                break;
            }
            case HSP: {
                for (int ch = 0; ch < 4; ++ch) {
                    servoes[ch]->hold_output = SerialReader();
                }
                Serial.printf("New hold setpoint: %u, %u, %u, %u.\n",
                              servoes[0]->hold_output,
                              servoes[1]->hold_output,
                              servoes[2]->hold_output,
                              servoes[3]->hold_output);
            }
            default: {
                servoes[ch]->read_from_serial(c);
                // delay for continuous update
                delay(200);
                if (Serial.available())
                    fsm_handle::dispatch(ser);
                else {
                    if (c == 1) {
                        ServoMachine::sweep_c = servoes[ch];
                        transit<Sweep>();
                    } else if (c == 2)
                        transit<Servo>();
                }
                break;
            }
        }
    }
};

struct Servo : ServoMachine {
    void entry() override {
        servo_loop();
    }
    void react(SerialEvent const &) override { transit<Idle>(); }
    void react(TurnOnServo const &) override { transit<Servo>(); }
};

struct Sweep : ServoMachine {
    void entry() override {
        get_best_power(ServoMachine::sweep_c);
    }
    void react(SerialEvent const &) override { transit<Idle>(); }
    void react(TurnOnSweep const &) override { transit<Sweep>(); }
};

FSM_INITIAL_STATE(ServoMachine, Idle)

void init_fsm() {
    fsm_handle::start();
}

void state_machine_loop() {
    if (Serial.available())
        fsm_handle::dispatch(ser);
    fsm_handle::dispatch(servo);
    fsm_handle::dispatch(sweep);
}

Controller *ServoMachine::sweep_c;
