/*

MPU6050 -   Arduino
VCC     -   3.3V
GND     -   GND
SDA     -   2
SCL     -   3

*//////////////////////////////////////////////////////////////////////////////////////

//Include I2C library
#include <Wire.h>
#include <Servo.h>

//Declaring some global variables
int gx, gy, gz;
long ax, ay, az, acc_magnitude;
int temp;
long gx_cal, gy_cal, gz_cal;
long loop_timer;
float angle_pitch, angle_roll;
int angle_pitch_buffer, angle_roll_buffer;
boolean set_gyro_angles;
float angle_roll_acc, angle_pitch_acc;
float filtered_pitch, filtered_roll;
float kp, ki, kd, servo_out, measured, set_point;

Servo servo;                                                           //Initialize 'servo' as a servo

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  delay(10);                                                           //Startup delay to allow for settling
  pinMode(5, OUTPUT);                                                  //Set pin 5 as output
  servo.attach(5);                                                     //Attach 'servo' to pin 5
  Wire.begin();                                                        //start I2C. No address means master
  //Serial.begin(57600);                                                 //Serial for debugging
  
  setup_registers();                                          //Setup the registers of the MPU-6050 (500dfs / +/-8g) and start the gyro
  
  calibrate_MPU6050();                                                 //Run calibration procedure for gyro offsets
  
  loop_timer = micros();                                               //Reset the loop timer. To be used to keep a constant loop-time. Replacement for interrupts in this situation
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop(){
  read_mpu6050();                                                      //Read raw acc and gyro data from MPU-6050

  gx -= gx_cal;                                                //Subtract calibrated offsets from read gyro data
  gy -= gy_cal;                                                
  gz -= gz_cal;                                                
  
  //Gyro angle calculations
  // 0.000061068 = (1 / 250Hz)/ 65.5
  angle_pitch += gx * 0.00005089;                                   //Calculate the traveled pitch angle and add this to the angle_pitch variable
  angle_roll += gy * 0.00005089;                                    //Calculate the traveled roll angle and add this to the angle_roll variable
  
  //0.000001065 = 0.000061068 * (3.142(PI) / 180degr) The Arduino sin function is in radians
  angle_pitch += angle_roll * sin(gz * 0.000000888);               //If the IMU has yawed transfer the roll angle to the pitch angel
  angle_roll -= angle_pitch * sin(gz * 0.000000888);               //If the IMU has yawed transfer the pitch angle to the roll angel
  
  //Accelerometer angle calculations
  acc_magnitude = sqrt((ax*ax)+(ay*ay)+(az*az));  //Calculate the total accelerometer vector
  //57.296 = 1 / (3.142 / 180) The Arduino asin function is in radians
  angle_pitch_acc = asin((float)ay/acc_magnitude)* 57.296;       //Calculate the pitch angle
  angle_roll_acc = asin((float)ax/acc_magnitude)* -57.296;       //Calculate the roll angle
  
  //Accelerometer calibration offsets
  //angle_pitch_acc -= 0.0;                                              //Accelerometer calibration value for pitch
  //angle_roll_acc -= 0.0;                                               //Accelerometer calibration value for roll

  //if(set_gyro_angles){                                                   //If the IMU is already started
    angle_pitch = angle_pitch * 0.999 + angle_pitch_acc * 0.001;         //Correct the drift of the gyro pitch angle with the accelerometer pitch angle
    angle_roll = angle_roll * 0.999 + angle_roll_acc * 0.001;            //Correct the drift of the gyro roll angle with the accelerometer roll angle
  //}
  //else{                                                                  //At first start
  //  angle_pitch = angle_pitch_acc;                                       //Set the gyro pitch angle equal to the accelerometer pitch angle 
  //  angle_roll = angle_roll_acc;                                         //Set the gyro roll angle equal to the accelerometer roll angle 
  //  set_gyro_angles = true;                                              //Set the IMU started flag
  //}
  
  //To dampen the pitch and roll angles a complementary filter is used
  filtered_pitch = (filtered_pitch * 0.9) + (angle_pitch * 0.1);   //Take 90% of the output pitch value and add 10% of the raw pitch value
  filtered_roll = (filtered_roll * 0.9) + (angle_roll * 0.1);      //Take 90% of the output roll value and add 10% of the raw roll value
  
  Serial.print(filtered_pitch);
  Serial.print("  ");
  Serial.println(filtered_roll);

  //Serial.println(micros() - loop_timer);
  
  while(micros() - loop_timer < 4000);                                 //Wait until the loop_timer reaches 4000us (250Hz) before starting the next loop
  loop_timer = micros();                                               //Reset the loop timer
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void read_mpu6050(){
  Wire.beginTransmission(0x68);                                        //Start communicating with MPU6050, using its default address 0x68. Found in datasheet
  Wire.write(0x3B);                                                    //Tell MPU6050 to start at register 0x3B. Again found from data sheet
  Wire.endTransmission();                                              //Finish writing
  Wire.requestFrom(0x68,14);                                           //Request 14 bytes from MPU6050
  while(Wire.available() < 14);                                        //Wait until all bytes are received
  ax = Wire.read()<<8|Wire.read();                                     //Add the low and high byte to the ax variable
  ay = Wire.read()<<8|Wire.read();                                  
  az = Wire.read()<<8|Wire.read();                                  
  temp = Wire.read()<<8|Wire.read();                                   //Add the low and high byte to the temp variable
  gx = Wire.read()<<8|Wire.read();                                 
  gy = Wire.read()<<8|Wire.read();                                 
  gz = Wire.read()<<8|Wire.read();                                 
}

void setup_registers(){
  //Activate  MPU6050
  Wire.beginTransmission(0x68);                                        //Address the MPU6050
  Wire.write(0x6B);                                                    //Address the PWR_MGMT_1 register
  Wire.write(0x00);                                                    //Reset all values to zero
  Wire.endTransmission();                                              
  //Configure accelerometer for +/-8g
  Wire.beginTransmission(0x68);
  Wire.write(0x1C);                                                    //Send the requested starting register
  Wire.write(0x10);                                                    //Set the requested starting register
  Wire.endTransmission();                                              //End the transmission
  //Configure gyro for 500 degrees per second
  Wire.beginTransmission(0x68);
  Wire.write(0x1B);                                                    //Send the requested starting register
  Wire.write(0x08);                                                    //Set the requested starting register
  Wire.endTransmission();                                              //End the transmission
}

void calibrate_MPU6050(){
  //digitalWrite(5, HIGH);                                               //Turn LED on pin 5 on for duration of startup procedure
  
  //for (int cal_int = 0; cal_int < 10000 ; cal_int ++){                  //Take 10000 samples for a good average reading
  //  read_mpu6050();                                                    //Read from MPU6050
  //  gx_cal += gx;                                              //Set calibration offsets for x y z
  //  gy_cal += gy;                                              
  //  gz_cal += gz;
  //}
  //gx_cal /= 10000;                                                  //Average all the samples
  //gy_cal /= 10000;                                                  
  //gz_cal /= 10000;                                                  

  gx_cal = -158;                                                     //Using previously calculated calibration data from 10,000 samples
  gy_cal = 90;
  gz_cal = -77;
  
  //Serial.print("gx_cal:  ");
  //Serial.print(gx_cal);
  //Serial.print(" gy_cal:  ");
  //Serial.print(gy_cal);
  //Serial.print(" gz_cal:  ");
  //Serial.println(gz_cal);
  
  //digitalWrite(5, LOW);                                                //Turn LED off when startup is complete
}
