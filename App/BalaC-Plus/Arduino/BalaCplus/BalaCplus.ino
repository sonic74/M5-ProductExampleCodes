//
// BalaC balancing robot (IMU:MPU6886)
// by Kiraku Labo
//
// 1. Lay the robot flat, and power on.
// 2. Wait until Gal-1 (Pitch Gyro calibration) completes.
// 3. Hold still the robot upright in balance until Cal-2 (Accel & Yaw Gyro cal)
// completes.
//
// short push of power button: Gyro calibration
// long push (>1sec) of power button: switch mode between standig and
// demo(circle)
//

#include <M5StickCPlus.h>


#include "OTAWebUpdater.h"

//#include "BluetoothSerial.h"
#ifdef _BLUETOOTH_SERIAL_H_
BluetoothSerial SerialBT;
#else
#include <XboxSeriesXControllerESP32_asukiaaa.hpp>
XboxSeriesXControllerESP32_asukiaaa::Core xboxController;
#endif


#define LED      10
#define N_CAL1   100
#define N_CAL2   100
#define LCDV_MID 60

boolean serialMonitor = true;
boolean standing      = false;
int16_t counter       = 0;
uint32_t time0 = 0, time1 = 0;
int16_t counterOverPwr = 0, maxOvp = 20;
float power, powerR, powerL, yawPower;
float varAng, varOmg, varSpd, varDst, varIang;
float gyroXoffset, gyroYoffset, gyroZoffset, accXoffset;
float gyroXdata, gyroYdata, gyroZdata, accXdata, accZdata;
float aveAccX = 0.0, aveAccZ = 0.0, aveAbsOmg = 0.0;
float cutoff            = 0.1;                     //~=2 * pi * f (Hz)
const float clk         = 0.01;                    // in sec,
const uint32_t interval = (uint32_t)(clk * 1000);  // in msec
float Kang, Komg, KIang, Kyaw, Kdst, Kspd;
int16_t maxPwr;
float yawAngle = 0.0;
float moveDestination, moveTarget;
float moveRate        = 0.0;
const float moveStep  = 0.2 * clk;
int16_t fbBalance     = 0;
int16_t motorDeadband = 0;
float mechFactR, mechFactL;
int8_t motorRDir = 0, motorLDir = 0;
bool spinContinuous = false;
float spinDest, spinTarget, spinFact = 1.0;
float spinStep  = 0.0;  // deg per 10msec
int16_t ipowerL = 0, ipowerR = 0;
int16_t motorLdir = 0, motorRdir = 0;  // 0:stop 1:+ -1:-
float vBatt, voltAve             = 3.7;
int16_t punchPwr, punchPwr2, punchDur, punchCountL = 0, punchCountR = 0;
byte demoMode = 0;


char buf[80];


void imuInit();
void resetMotor();
void resetPara();
void resetVar();
void calib1();
void drvMotor(byte ch, int8_t sp);
void drvMotorL(int16_t pwm);
void drvMotorR(int16_t pwm);
void setMode(bool inc);
void dispBatVolt();
void getGyro();
void checkButtonP();
void calib2();
void startDemo();
void drive();
void calDelay(int n);
void sendStatus();
void readGyro();


void setup() {
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
    M5.begin();
    snprintf(buf, sizeof(buf), "millis()-time0 stand accX power ang");
    if (serialMonitor) Serial.print(buf);
    Wire.begin(0, 26);  // SDA,SCL
    imuInit();
    M5.Axp.ScreenBreath(100);
    M5.Lcd.setRotation(2);
    M5.Lcd.setTextFont(4);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(1);
    resetMotor();
    resetPara();
    resetVar();
    calib1();
#ifdef DEBUG
    debugSetup();
#else
    setMode(false);
#endif


setup_OTAWebUpdater();
#ifdef _BLUETOOTH_SERIAL_H_
SerialBT.setPin("1111");
SerialBT.begin("ESP32test");
SerialBT.println(buf);
#else
xboxController.begin();
#endif
}

void loop() {
    checkButtonP();
#ifdef DEBUG
    if (debugLoop1()) return;
#endif
    getGyro();
    if (!standing) {
        dispBatVolt();
        aveAbsOmg = aveAbsOmg * 0.9 + abs(varOmg) * 0.1;
        aveAccZ   = aveAccZ * 0.9 + accZdata * 0.1;
        M5.Lcd.setCursor(30, 130);
        M5.Lcd.printf("%5.2f   ", -aveAccZ);
    M5.Lcd.printf("\n\nm %4.2f\n", moveRate);
    M5.Lcd.printf("s %4.2f\n", spinStep);
        if (abs(aveAccZ) > 0.9 && aveAbsOmg < 1.5) {
            calib2();
            if (demoMode == 1) startDemo();
            standing = true;
        }
    } else {
        if (abs(varAng) > 30.0 || counterOverPwr > maxOvp) {
            resetMotor();
            resetVar();
            standing = false;
            setMode(false);
        } else {
            drive();
        }
    }
    counter += 1;
    if (counter >= 100) {
        counter = 0;
        dispBatVolt();
        if (serialMonitor) sendStatus();
    }


loop_OTAWebUpdater();
#ifndef _BLUETOOTH_SERIAL_H_
  xboxController.onLoop();
  if (xboxController.isConnected()) {
    if (xboxController.isWaitingForFirstNotification()) {
//      Serial.println("waiting for first notification");
    } else {
      spinStep=(float)xboxController.xboxNotif.joyLHori/XboxControllerNotificationParser::maxJoy*2.0f-1.0f;
      moveRate=(float)xboxController.xboxNotif.joyLVert/XboxControllerNotificationParser::maxJoy*2.0f-1.0f;
      if(abs(spinStep)>0.1f) spinContinuous=true;
      else spinContinuous=false;
    }
  } else {
//    Serial.println("not connected");
    if (xboxController.getCountFailedConnection() > 2) {
//      ESP.restart();
    }
  }
#endif


    do time1 = millis();
    while (time1 - time0 < interval);
    time0 = time1;
}

void calib1() {
    calDelay(30);
    digitalWrite(LED, LOW);
    calDelay(80);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(30, LCDV_MID);
    M5.Lcd.print(" Cal-1  ");
    gyroYoffset = 0.0;
    for (int i = 0; i < N_CAL1; i++) {
        readGyro();
        gyroYoffset += gyroYdata;
        delay(9);
    }
    gyroYoffset /= (float)N_CAL1;
    M5.Lcd.fillScreen(BLACK);
    digitalWrite(LED, HIGH);
}

void calib2() {
    resetVar();
    resetMotor();
    digitalWrite(LED, LOW);
    calDelay(80);
    M5.Lcd.setCursor(30, LCDV_MID);
    M5.Lcd.println(" Cal-2  ");
    accXoffset  = 0.0;
    gyroZoffset = 0.0;
    for (int i = 0; i < N_CAL2; i++) {
        readGyro();
        accXoffset += accXdata;
        gyroZoffset += gyroZdata;
        delay(9);
    }
    accXoffset /= (float)N_CAL2;
    gyroZoffset /= (float)N_CAL2;
    M5.Lcd.fillScreen(BLACK);
    digitalWrite(LED, HIGH);
}

void checkButtonP() {
    byte pbtn = M5.Axp.GetBtnPress();
    if (pbtn == 2) {
        if (serialMonitor) Serial.println("pbtn == 2");
        calib1();  // short push
    } else if (pbtn == 1) {
        if (serialMonitor) Serial.println("pbtn == 1");
        setMode(true);  // long push
    }
}

void calDelay(int n) {
    for (int i = 0; i < n; i++) {
        getGyro();
        delay(9);
    }
}

void setMode(bool inc) {
    if (inc) demoMode = ++demoMode % 2;
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(30, 5);
    if (demoMode == 0)
        M5.Lcd.print("Stand ");
    else if (demoMode == 1)
        M5.Lcd.print("Demo ");
}

void startDemo() {
    moveRate       = 1.0;
    spinContinuous = true;
    spinStep       = -40.0 * clk;
}

void resetPara() {
    Kang          = 37.0;
    Komg          = 0.84;
    KIang         = 800.0;
    Kyaw          = 4.0;
    Kdst          = 85.0;
    Kspd          = 2.7;
    mechFactL     = 0.45;
    mechFactR     = 0.45;
    punchPwr      = 20;
    punchDur      = 1;
    fbBalance     = -3;
    motorDeadband = 10;
    maxPwr        = 120;
    punchPwr2     = max(punchPwr, motorDeadband);
}

void getGyro() {
    readGyro();
    varOmg = (gyroYdata - gyroYoffset);           // unit:deg/sec
    yawAngle += (gyroZdata - gyroZoffset) * clk;  // unit:g
    varAng += (varOmg + ((accXdata - accXoffset) * 57.3 - varAng) * cutoff) *
              clk;  // complementary filter
}

void readGyro() {
    float gX, gY, gZ, aX, aY, aZ;
    M5.Imu.getGyroData(&gX, &gY, &gZ);
    M5.Imu.getAccelData(&aX, &aY, &aZ);
    gyroYdata = gX;
    gyroZdata = -gY;
    gyroXdata = -gZ;
    accXdata  = aZ;
    accZdata  = aY;
}

void drive() {
#ifdef DEBUG
    debugDrive();
#endif
    if (abs(moveRate) > 0.1)
        spinFact = constrain(-(powerR + powerL) / 10.0, -1.0, 1.0);  // moving
    else
        spinFact = 1.0;  // standing
    if (spinContinuous)
        spinTarget += spinStep * spinFact;
    else {
        if (spinTarget < spinDest) spinTarget += spinStep;
        if (spinTarget > spinDest) spinTarget -= spinStep;
    }
    moveTarget += moveStep * (moveRate + (float)fbBalance / 100.0);
    varSpd += power * clk;
    varDst += Kdst * (varSpd * clk - moveTarget);
    varIang += KIang * varAng * clk;
    power =
        varIang + varDst + (Kspd * varSpd) + (Kang * varAng) + (Komg * varOmg);
    if (abs(power) > 1000.0)
        counterOverPwr += 1;
    else
        counterOverPwr = 0;
    if (counterOverPwr > maxOvp) return;
    power    = constrain(power, -maxPwr, maxPwr);
    yawPower = (yawAngle - spinTarget) * Kyaw;
    powerR   = power - yawPower;
    powerL   = power + yawPower;

    ipowerL      = (int16_t)constrain(powerL * mechFactL, -maxPwr, maxPwr);
    int16_t mdbn = -motorDeadband;
    int16_t pp2n = -punchPwr2;
    if (ipowerL > 0) {
        if (motorLdir == 1)
            punchCountL = constrain(++punchCountL, 0, 100);
        else
            punchCountL = 0;
        motorLdir = 1;
        if (punchCountL < punchDur)
            drvMotorL(max(ipowerL, punchPwr2));
        else
            drvMotorL(max(ipowerL, motorDeadband));
    } else if (ipowerL < 0) {
        if (motorLdir == -1)
            punchCountL = constrain(++punchCountL, 0, 100);
        else
            punchCountL = 0;
        motorLdir = -1;
        if (punchCountL < punchDur)
            drvMotorL(min(ipowerL, pp2n));
        else
            drvMotorL(min(ipowerL, mdbn));
    } else {
        drvMotorL(0);
        motorLdir = 0;
    }

    ipowerR = (int16_t)constrain(powerR * mechFactR, -maxPwr, maxPwr);
    if (ipowerR > 0) {
        if (motorRdir == 1)
            punchCountR = constrain(++punchCountR, 0, 100);
        else
            punchCountR = 0;
        motorRdir = 1;
        if (punchCountR < punchDur)
            drvMotorR(max(ipowerR, punchPwr2));
        else
            drvMotorR(max(ipowerR, motorDeadband));
    } else if (ipowerR < 0) {
        if (motorRdir == -1)
            punchCountR = constrain(++punchCountR, 0, 100);
        else
            punchCountR = 0;
        motorRdir = -1;
        if (punchCountR < punchDur)
            drvMotorR(min(ipowerR, pp2n));
        else
            drvMotorR(min(ipowerR, mdbn));
    } else {
        drvMotorR(0);
        motorRdir = 0;
    }
}

void drvMotorL(int16_t pwm) {
    drvMotor(0, (int8_t)constrain(pwm, -127, 127));
}

void drvMotorR(int16_t pwm) {
    drvMotor(1, (int8_t)constrain(-pwm, -127, 127));
}

void drvMotor(byte ch, int8_t sp) {
    Wire.beginTransmission(0x38);
    Wire.write(ch);
    Wire.write(sp);
    Wire.endTransmission();
}

void resetMotor() {
    drvMotorR(0);
    drvMotorL(0);
    counterOverPwr = 0;
}

void resetVar() {
    power          = 0.0;
    moveTarget     = 0.0;
    moveRate       = 0.0;
    spinContinuous = false;
    spinDest       = 0.0;
    spinTarget     = 0.0;
    spinStep       = 0.0;
    yawAngle       = 0.0;
    varAng         = 0.0;
    varOmg         = 0.0;
    varDst         = 0.0;
    varSpd         = 0.0;
    varIang        = 0.0;
}

void sendStatus() {
    snprintf(buf, sizeof(buf), "%i %d %f %f %f", millis()-time0, standing, accXdata, power, varAng);
    Serial.println(buf);
#ifdef _BLUETOOTH_SERIAL_H_
    SerialBT.println(buf);
#endif
}

void imuInit() {
    M5.Imu.Init();
    // if (M5.Imu.imuType = M5.Imu.IMU_MPU6886) {
    //     M5.Mpu6886.SetGyroFsr(
    //         M5.Mpu6886.GFS_250DPS);  // 250DPS 500DPS 1000DPS 2000DPS
    //     M5.Mpu6886.SetAccelFsr(M5.Mpu6886.AFS_4G);  // 2G 4G 8G 16G
    //     if (serialMonitor) Serial.println("MPU6886 found");
    // } else if (serialMonitor)
    //     Serial.println("MPU6886 not found");

    M5.Imu.SetGyroFsr(
    M5.Imu.GFS_250DPS);  // 250DPS 500DPS 1000DPS 2000DPS
    M5.Imu.SetAccelFsr(M5.Imu.AFS_4G);  // 2G 4G 8G 16G
    if (serialMonitor) Serial.println("MPU6886 found");
}

void dispBatVolt() {
    M5.Lcd.setCursor(35, LCDV_MID);
    vBatt = M5.Axp.GetBatVoltage();
    M5.Lcd.printf("%4.2fv ", vBatt);
}
