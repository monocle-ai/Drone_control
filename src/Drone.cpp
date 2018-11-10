/*

Drone control

*/

#include <Common/Util.h>
#include <unistd.h>
#include <memory>
#include <iostream>
#include <string>

#include <ctime>
#include <chrono>
#include <math.h>

#include "RCInputManager.cpp"
#include "ServoManager.cpp"
#include "IMU.cpp"
#include "Stab/Stabilisation.cpp"
#include "LEDManager.cpp"
#include "Remote.cpp"

#include "Ref.h"

using namespace std;

typedef std::chrono::high_resolution_clock TimeM;

int i = 0;
string version = "0.1.4";

RCInputManager rc = RCInputManager();
ServoManager servo = ServoManager();
IMU imu = IMU();
LEDManager led = LEDManager();
Remote remote = Remote();
Stabilisation stab = Stabilisation();

auto time_start = TimeM::now();

auto last = TimeM::now();
auto t_last = TimeM::now();
float t = 0; // time
float ang[3] = {0, 0, 0};

float acceleration[3] = {0, 0, 0};
float rates[3] = {0, 0, 0};

int commands[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

// remote
int pid_out[3] = {0, 0, 0};
int pid_debug[3] = {0, 0, 0};

float motors[4] = {0, 0, 0, 0};
int motors_output[4] = {0, 0, 0, 0};
int sensors[2] = {0, 0}; // pressure, temperature
int frequency_crtl[] = {0, 0};

bool isArmed = false;
bool isArming = false;

// time
auto arming_t_started = TimeM::now();

void safety()
{
    //cout << "throttle : " << commands[cmd_throttle] << " arming : " << commands[cmd_arming] <<  "\n";
    if (commands[cmd_throttle] < 1100) // evaluation must be done at 0 throttle !!!!
    {
        if (commands[cmd_arming] > 1500)
        {
            double t_diff = chrono::duration<double, nano>(t_last - arming_t_started).count();

            if (isArming == false) // is arming
            {
                cout << "ARMING\n";
                isArming = true;
                arming_t_started = TimeM::now();
                led.setArming();
            }
            else if (isArming && t_diff * pow(10, -9) > 2 && isArmed == false)
            {
                cout << "ARMED\n";
                isArmed = true;
                led.setArmed();
            }
        }
        else
        {
            isArmed = false;
            isArming = false;
            led.setOK();
        }
    }
}

void setup()
{
    led.setKO();

    // check if root
    if (getuid())
    {
        printf("Not root. Please launch with root permission: sudo \n");
        throw "Not root";
    }

    if (check_apm()) // check if apm (ardupilot) is running
    {
        cout << "APM is running ... could not run";
        throw "APM running";
    }

    cout << "[ MAIN ] : " << version << "\n";
    printf("[ MAIN ] : Setup \n");
    servo.initialize();

    // launch remote

    remote.launch(ang, acceleration, rates, pid_out, pid_debug, sensors, &t);

    led.setOK();
}

void loop()
{
    // get informations
    auto now = TimeM::now();
    imu.update(); // get imu data just after timing

    rc.read(commands);

    // process informations
    double now_n = chrono::duration<double, nano>(now - time_start).count();
    double diff_nano = chrono::duration<double, nano>(now - t_last).count();

    t_last = now;

    // dt : convert nano to seconds
    float dt = diff_nano * pow(10, -9.0);
    t = now_n * pow(10, -9.0);

    // update imu
    imu.setDt(dt); // update dt

    // safety :
    safety();

    // angles
    imu.getComplementar(ang); // get angles
    imu.getRates(rates);
    imu.getAcceleration(acceleration);

    // stabilize
    stab.Stabilize(motors_output, isArmed, commands, ang, rates, dt);
    servo.setDuty(motors_output);
    i++;

    // display frequency
    int r = 900;
    if (i % r == 0)
    {

        auto now = TimeM::now();
        float diff = chrono::duration<double, nano>(now - last).count();

        float f = r / (diff * pow(10, -9.0));
        cout << "f : " << f << "Hz "
             << "\n";
        last = now;

        //        pid.displayK();
    }
}

auto last_call = TimeM::now();
bool useTimer = true;
int main(int argc, char *argv[])
{
    double frequency = 900;      //Hz
    double f_dt = 1 / frequency; //seconds
    long long int f_dt_n = f_dt * pow(10, 9);
    cout << "[ MAIN ] : frequency : " << f_dt_n << "Hz \n";
    setup();

    // timer variables
    double next_call = 0;
    last_call = TimeM::now(); // reset timmer
    // loop
    while (true)
    {
        if (useTimer)
        {
            last_call += chrono::nanoseconds(f_dt_n);
            while (pow(10, -9) * chrono::duration<double, nano>(last_call - TimeM::now()).count() > 0)
            {
                // micro second
                // if error is superior to 10 micro second, reset clock
                if (pow(10, -3) * chrono::duration<double, nano>(TimeM::now() - last_call).count() > 10)
                {
                    cout << "Clock reset\n";
                    last_call = TimeM::now();
                }
            }
        }

        // wait that we are at beggining

        // call loop
        loop();
    }
}
