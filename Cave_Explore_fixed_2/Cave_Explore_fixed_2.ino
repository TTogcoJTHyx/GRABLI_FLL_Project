#include <Arduino.h>
#include "MeMegaPi.h"
#include <Wire.h>  
#include <SimpleDHT.h> 

MeEncoderOnBoard encoderLeft(SLOT1);
MeEncoderOnBoard encoderRight(SLOT2);
MeMegaPiDCMotor motor_left(PORT1B);
MeMegaPiDCMotor motor_right(PORT2B);
MeUltrasonicSensor ultraSensor_front(PORT_8);
MeUltrasonicSensor ultraSensor_right(PORT_6);
MeGyro gyro(PORT_7);

const int pinDHT11 = A8; 
SimpleDHT11 dht11(pinDHT11); // Передаємо пін одразу в конструктор

// Датчик газу на аналоговому піні A10
const int gasSensor = A9;

int distance_front;
int distance_right;
float error_count_L = 0;
float error_count_R = 0;
int loop_count = 0;
int packet_count = 0;  // ✅ объявлен
int sensors_count = 0;

int normal_right_distance = 7;
int safe_distance = 7;
bool isRunning = false;

float gyro_offset = 0;  // ✅ только один раз
float accumulated_angle = 0;

long prev_left = 0;
long prev_right = 0;

void isr_encoder_left() {
  if (digitalRead(encoderLeft.getPortB()) == 0) encoderLeft.pulsePosMinus();
  else encoderLeft.pulsePosPlus();
}

void isr_encoder_right() {
  if (digitalRead(encoderRight.getPortB()) == 0) encoderRight.pulsePosMinus();
  else encoderRight.pulsePosPlus();
}

void move(int speedL, int speedR){
  motor_left.run(speedL);
  motor_right.run(-speedR);
}

void motors_stop(){
  motor_left.stop();
  motor_right.stop();
}

void resetEncoders() {
  encoderLeft.setPulsePos(0);
  encoderRight.setPulsePos(0);
}

void recalibrate_gyro() {
  float angle_before = gyro.getAngleZ() - gyro_offset;
  
  float new_offset = 0;
  for(int i = 0; i < 50; i++) {
    gyro.update();
    new_offset += gyro.getAngleZ();
    delay(10);
  }
  new_offset /= 50;
  
  gyro_offset = new_offset - angle_before;
}

void sendData() {
  long curr_left  = -encoderLeft.getPulsePos();
  long curr_right =  encoderRight.getPulsePos();

  long delta_L = curr_left  - prev_left;
  long delta_R = curr_right - prev_right;

  prev_left  = curr_left;
  prev_right = curr_right;

  gyro.update();
  float theta = gyro.getAngleZ() - gyro_offset;

  Serial3.print(delta_L);
  Serial3.print("; ");
  Serial3.print(delta_R);
  Serial3.print("; ");
  Serial3.println(theta);
  if (sensors_count == 10) {
    sensors_count = 0;
    
    int gasValue = analogRead(gasSensor);
    Serial3.print(gasValue);
    Serial3.print("S ");
    byte temperature = 0;
    byte humidity = 0;
    int err = SimpleDHTErrSuccess;
    
    // Стандартний виклик функції read для сучасних версій SimpleDHT
    if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
      Serial3.print("DHT11 read failed, err="); 
      Serial3.println(SimpleDHTErrCode(err));
    } else {
      Serial3.print((int)temperature);
      Serial3.print("S ");
      Serial3.println((int)humidity);
    }
  
  }
  sensors_count += 1;
  packet_count++;  // ✅ считаем пакеты здесь
}

void runMyRobotCode() {
  Serial3.println("-> MegaPi активовано...");
  packet_count = 0;
  while (true) {
    distance_front = ultraSensor_front.distanceCm();
    distance_right = ultraSensor_right.distanceCm();

    if (distance_front == 0) distance_front = 400;
    if (distance_right == 0) distance_right = 400;

    loop_count += 1; 

    if (distance_front <= safe_distance) {
      while (distance_front <= 30) {
        distance_front = ultraSensor_front.distanceCm();
        if (distance_front == 0) distance_front = 400;
        
        move(-80, 80);
        sendData();
        delay(200);
      }
      error_count_L = 0;
      error_count_R = 0;
    } else {
      if (distance_right >= normal_right_distance - 1 && distance_right <= normal_right_distance + 1) {
        move(120, 120);
        error_count_L = 0;
        error_count_R = 0;
      } else if (distance_right < normal_right_distance - 1) {
        error_count_L += 1;
        error_count_R = 0;
        move(100 - 8 * error_count_L, 100 + 10 * error_count_L);
      } else {
        error_count_R += 1;
        error_count_L = 0;
        move(100 + 10 * error_count_R, 100 - 8 * error_count_R);
      }

      if (loop_count >= 5) {
        sendData();
        loop_count = 0;
      }
    }

    if (packet_count >= 500) {
      motors_stop();
      sendData();
      recalibrate_gyro();
      packet_count = 0;
    }

    delay(30);
  }
}

void setup() {
  Serial3.begin(115200); 
  gyro.begin();
  delay(1000);

  for(int i = 0; i < 100; i++) {
    gyro.update();
    gyro_offset += gyro.getAngleZ();
    delay(10);
  }
  gyro_offset /= 100;

  attachInterrupt(encoderLeft.getIntNum(),  isr_encoder_left,  RISING);
  attachInterrupt(encoderRight.getIntNum(), isr_encoder_right, RISING);
}

void loop() {
  while (Serial3.available() > 0) {
    char incomingChar = Serial3.read();
    if (incomingChar == 'G') {
      isRunning = true;
      Serial3.println("=== СИГНАЛ ОТРИМАНО! ЗАПУСК ===");
      runMyRobotCode();
    }
  }
}
