#include <Arduino.h>
/// --- Налаштування ---
#include "MeMegaPi.h"
#include <Wire.h>  

MeEncoderOnBoard encoderLeft(SLOT1);
MeEncoderOnBoard encoderRight(SLOT2);

MeMegaPiDCMotor motor_left(PORT1B);
MeMegaPiDCMotor motor_right(PORT2B);

MeUltrasonicSensor ultraSensor_front(PORT_7);
MeUltrasonicSensor ultraSensor_right(PORT_8);
MeGyro gyro(PORT_6);

int distance_front;
int distance_right;
float error_count_L = 0;
float error_count_R = 0;
int loop_count = 0;

int normal_right_distance = 7; //нормальна дистанція до правої стіни
int safe_distance = 12; //дистанція знаходження передньої стіни

bool isRunning = false; 

// --- Обробники переривань енкодерів ---
void isr_encoder_left() {
  if (digitalRead(encoderLeft.getPortB()) == 0) {
    encoderLeft.pulsePosMinus();
  } else {
    encoderLeft.pulsePosPlus();
  }
}

void isr_encoder_right() {
  if (digitalRead(encoderRight.getPortB()) == 0) {
    encoderRight.pulsePosMinus();
  } else {
    encoderRight.pulsePosPlus();
  }
}

// --- Функції руху --- 
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

// --- Головний код ---
void runMyRobotCode() {
  Serial3.println("-> MegaPi активовано...");
  while (true) {
    distance_front = ultraSensor_front.distanceCm();
    distance_right = ultraSensor_right.distanceCm();

    // Защита от "слепой зоны" датчиков: если датчик ничего не видит, 
    // он выдает 0. Мы искусственно меняем 0 на 400 см (бесконечность).
    if (distance_front == 0) distance_front = 400;
    if (distance_right == 0) distance_right = 400;

    loop_count += 1; 

    if (distance_front <= safe_distance) {
      Serial.println("turn left (obstacle)");
      while (distance_front <= 25) {
        distance_front = ultraSensor_front.distanceCm();
        if (distance_front == 0) distance_front = 400; // защита от нуля внутри цикла
        
        move(-80, 80); // Поворот на месте налево
        delay(500);
      }
      // После успешного поворота сбрасываем накопленные ошибки, 
      // чтобы робот не рванул в стену
      error_count_L = 0;
      error_count_R = 0;
    } 
    // --- їдемо по правій стіні ---
    else {
      if (distance_right >= normal_right_distance - 1 && distance_right <= normal_right_distance + 1) {
        move(120, 120);
        error_count_L = 0;
        error_count_R = 0;
      } 
      else if (distance_right < normal_right_distance - 1) {
        error_count_L += 1;
        error_count_R = 0; // Сбрасываем противоположную ошибку
        move(70, 70 + 10 * error_count_L);
      } 
      else {
        error_count_R += 1;
        error_count_L = 0; // Сбрасываем противоположную ошибку
        move(70 + 10 * error_count_R, 70);
      }

      if (loop_count >= 17) {
        gyro.update();

        Serial3.print(-encoderLeft.getPulsePos()); // лівий енкодер
        Serial3.print("; ");
        Serial3.print(encoderRight.getPulsePos()); // правий енкодер
        Serial3.print("; ");
        Serial3.println(gyro.getAngleZ()); // кут

        loop_count = 0;
      }
    }
    delay(30);
  }
}

void setup() {
  Serial3.begin(115200); 
  gyro.begin();
  delay(1000); // ініціалізація гіроскопа

  // На Arduino — замеряем drift перед стартом
  float gyro_offset = 0;
  for(int i = 0; i < 100; i++) {
      gyro_offset += gyro.getAngleZ();
      delay(10);
  }
  gyro_offset /= 100;

// И вычитаем при каждом чтении
float theta = gyro.getAngleZ() - gyro_offset;

  // Прив'язка переривань для енкодерів
  attachInterrupt(encoderLeft.getIntNum(),  isr_encoder_left,  RISING);
  attachInterrupt(encoderRight.getIntNum(), isr_encoder_right, RISING);
}

void loop() {
  // Проверяем буфер побайтово
  while (Serial3.available() > 0) {
    char incomingChar = Serial3.read();
    
    // Проверяем: если пришла буква 'G'
    if (incomingChar == 'G') {
      isRunning = true;
      Serial3.println("=== СИГНАЛ ОТРИМАНО! ЗАПУСК ===");

      runMyRobotCode();
    }
  }
}
