#pragma config(Hubs,  S1, HTMotor,  HTMotor,  HTServo,  none)
#pragma config(Sensor, S1,     ,               sensorI2CMuxController)
#pragma config(Sensor, S2,     HTCS2,          sensorNone)
#pragma config(Sensor, S3,     IRseeker,       sensorI2CCustom)
#pragma config(Motor,  mtr_S1_C1_1,     motorR,        tmotorTetrix, PIDControl, encoder)
#pragma config(Motor,  mtr_S1_C1_2,     motorL,        tmotorTetrix, PIDControl, reversed, encoder)
#pragma config(Motor,  mtr_S1_C2_1,     motorF,        tmotorTetrix, openLoop)
#pragma config(Motor,  mtr_S1_C2_2,     motorG,        tmotorTetrix, openLoop)
#pragma config(Servo,  srvo_S1_C3_1,    servoFlip,            tServoStandard)
#pragma config(Servo,  srvo_S1_C3_2,    servo2,               tServoNone)
#pragma config(Servo,  srvo_S1_C3_3,    servo3,               tServoNone)
#pragma config(Servo,  srvo_S1_C3_4,    servo4,               tServoNone)
#pragma config(Servo,  srvo_S1_C3_5,    servo5,               tServoNone)
#pragma config(Servo,  srvo_S1_C3_6,    servo6,               tServoNone)
//*!!Code automatically generated by 'ROBOTC' configuration wizard               !!*//

/////////////////////////////////////////////////////////////////////////////////////////////////////
//
//                           Autonomous Mode Code Template
//
// This file contains a template for simplified creation of an autonomous program for an TETRIX robot
// competition.
//
// You need to customize two functions with code unique to your specific robot.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

#include "JoystickDriver.c"  //Include file to "handle" the Bluetooth messages.
#include "rdpartyrobotcdr-3.3.1\drivers\hitechnic-irseeker-v2.h"

// Pause for the other alliance?
#define BEFORE_START_10MS 0
#define DRIVE_SPEED 60
#define ENCODER_TICKS_INCH 100
#define ENCODER_TICKS_90_TURN 2880

string Left = "L";
string Right = "R";

int DistanceToIR = 0; //Distance from beacon
int _dirAC = 0; //Sensor number
int acS1, acS2, acS3, acS4, acS5 = 0; //Stores IR sensor values
int maxSig = 0; // The max signal strength from the seeker.
int flipper_start_pos = 0; //Flat
int turnTime = 100; // Time (ms) to complete 90 degree turn.
// This variable is set by the MoveToIR function (It knows where the beacon is located).
string beaconDirection = "L"; // Which side of the robot is the beacon on
int irGoal = 3; // Which sensor is pointing to the left or right of the robot?
int servoMoveRange = 120; // Servo location to dump block (assumes 0 = rest position).

float InchesToTape = 18;
float InchesToRamp = 25;

/*-----------------------------------------------------------------------------*/
/*                                                                             */
/*  definitiona and variables for the motor slew rate controller.              */
/*                                                                             */
/*-----------------------------------------------------------------------------*/

#define MOTOR_NUM               5
#define MOTOR_MAX_VALUE         127
#define MOTOR_MIN_VALUE         (-127)
#define MOTOR_DEFAULT_SLEW_RATE 5      // Default will cause 375mS from full fwd to rev
#define MOTOR_FAST_SLEW_RATE    256     // essentially off
#define MOTOR_TASK_DELAY        15      // task 1/frequency in mS (about 66Hz)
#define MOTOR_DEADBAND          10

// Array to hold requested speed for the motors
int motorReq[ MOTOR_NUM ];

// Array to hold "slew rate" for the motors, the maximum change every time the task
// runs checking current mootor speed.
int motorSlew[ MOTOR_NUM ];

// Function declarations.
void Turn90(string direction);
void ResetEncoders();
void StopMotors();

/*-----------------------------------------------------------------------------*/
/*                                                                             */
/*  Task  - compares the requested speed of each motor to the current speed    */
/*          and increments or decrements to reduce the difference as necessary */
/*                                                                             */
/*-----------------------------------------------------------------------------*/

task MotorSlewRateTask()
{
	int motorIndex;
	int motorTmp;

	// Initialize stuff
	for(motorIndex=0;motorIndex<MOTOR_NUM;motorIndex++)
	{
		motorReq[motorIndex] = 0;
		motorSlew[motorIndex] = MOTOR_DEFAULT_SLEW_RATE;
	}

	// run task until stopped
	while( true )
	{
		// run loop for every motor
		for( motorIndex=0; motorIndex<MOTOR_NUM; motorIndex++)
		{
			// So we don't keep accessing the internal storage
			motorTmp = motor[ motorIndex ];

			// Do we need to change the motor value ?
			if( motorTmp != motorReq[motorIndex] )
			{
				// increasing motor value
				if( motorReq[motorIndex] > motorTmp )
				{
					motorTmp += motorSlew[motorIndex];
					// limit
					if( motorTmp > motorReq[motorIndex] )
						motorTmp = motorReq[motorIndex];
				}

				// decreasing motor value
				if( motorReq[motorIndex] < motorTmp )
				{
					motorTmp -= motorSlew[motorIndex];
					// limit
					if( motorTmp < motorReq[motorIndex] )
						motorTmp = motorReq[motorIndex];
				}

				// finally set motor
				motor[motorIndex] = motorTmp;
			}
		}

		// Wait approx the speed of motor update over the spi bus
		wait1Msec( MOTOR_TASK_DELAY );
	}
}

void initializeRobot()
{
	servoChangeRate[servoFlip] = 20; // Servo Change Rate, positions per update (20ms).
	servo[servoFlip] = flipper_start_pos;
	ResetEncoders();
	disableDiagnosticsDisplay();
}

int convert(float inches)
{
	return (int)(inches * ENCODER_TICKS_INCH);
}

void driveMotors(int lspeed, int rspeed)
{
	motorReq[motorL] = lspeed;
	motorReq[motorR] = rspeed;
}

void MoveForward ()
{
	driveMotors (DRIVE_SPEED, DRIVE_SPEED);
}

void StopMotors()
{
	driveMotors(0,0);
	wait10Msec(20);
}

void ResetEncoders()
{
	nMotorEncoder[motorL] = 0;
	nMotorEncoder[motorR] = 0;
	wait10Msec(30);
}

void GoInches(float inches, int speed)
{
	ResetEncoders();
	wait1Msec(200);
	motorReq[motorL] = speed;
	motorReq[motorR] = speed;
	//motor[motorL] = speed;
	//motor[motorR] = speed;
	while ((abs(nMotorEncoder[motorR]) + abs(nMotorEncoder[motorL])) / 2 < (convert(inches)))
	{
	}

	StopMotors();
}

// Run servo to dump block and return arm to rest.
void DumpBlock()
{
	servo[servoFlip] = servoMoveRange; //Flip the block out.
	wait1Msec(200);
	servo[servoFlip] = ServoValue[servoFlip] - servoMoveRange; //Move back to the starting position.
}

// Move back to start after a trip to the IR beacon.
void BackToStart()
{
	if (DistanceToIR == 0)
	{
		return;
	}

	//nxtDisplayTextLine(4, "Distance: %d", DistanceToIR);
	ResetEncoders();
	nMotorEncoderTarget[motorL] = DistanceToIR;
	nMotorEncoderTarget[motorR] = DistanceToIR;
	wait10Msec(30);
	motorReq[motorL] = -DRIVE_SPEED;
	motorReq[motorR] = -DRIVE_SPEED;
	while (nMotorRunState[motorL] != runStateIdle || nMotorRunState[motorR] != runStateIdle)
	{
		nxtDisplayTextLine(3, "Enc: %d", nMotorEncoder[motorL]);
		nxtDisplayTextLine(4, "RunState: %d", nMotorRunState[motorL]);
	}
}

void MovetoIR()
{
	int FindState = 1;
	bool FoundIt = false;
	nMotorEncoder[motorL] = 0;
	nMotorEncoder[motorR] = 0;
	wait1Msec(200);

	// Use a timer to prevent runaway!
	ClearTimer(T1);
	// Start moving.
	driveMotors(DRIVE_SPEED, DRIVE_SPEED);
	// Adjust timer if needed, just enough to get to the IR.
	// If we don't find it, timeout and move on.
	while(!FoundIt && time1[T1] < 5000)
	{
		switch (FindState)
		{
		case 1:
			// Look for target
			// Get the direction.
			_dirAC = HTIRS2readACDir(IRseeker);
			// Make 0 straight ahead, all positive, no left or right worry.
			_dirAC = _dirAC - 5;
			// Set the direction for the turn90 function.
		beaconDirection = _dirAC < 0 ? "L" : "R";
			// We need it without sign so left and right are ignored.
			_dirAC = abs(_dirAC);
			nxtDisplayTextLine(1, "Direction: %d", _dirAC);
			// Get the strength.
			HTIRS2readAllACStrength(IRseeker, acS1, acS2, acS3, acS4, acS5);
		maxSig = (acS1 > acS2) ? acS1 : acS2;
		maxSig = (maxSig > acS3) ? maxSig : acS3;
		maxSig = (maxSig > acS4) ? maxSig : acS4;
		maxSig = (maxSig > acS5) ? maxSig : acS5;
			nxtDisplayTextLine(2, "maxSig: %d", maxSig);

			wait10Msec(2);
			if (_dirAC >= irGoal)
			{
				StopMotors();
				DistanceToIR = nMotorEncoder[motorL];
				FindState++;
			}
			break;
		case 2:
			// Look for strongest signal.
			FindState++;
			break;
		case 3:
			// Backup a little.
			FindState++;
			FoundIt = true;
			break;
		}
	}
}

// Test functions here.
void LookForBeacon()
{
	while (true)
	{
		_dirAC = HTIRS2readACDir(IRseeker);
		// Make 0 straight ahead, all positive, no left or right worry.
		_dirAC = _dirAC - 5;
		// Set the direction for the turn90 function.
	beaconDirection = _dirAC < 0 ? "L" : "R";
		// We need it without sign so left and right are ignored.
		_dirAC = abs(_dirAC);
		nxtDisplayTextLine(1, "Direction: %d", _dirAC);
		// Get the strength.
		HTIRS2readAllACStrength(IRseeker, acS1, acS2, acS3, acS4, acS5);
	maxSig = (acS1 > acS2) ? acS1 : acS2;
	maxSig = (maxSig > acS3) ? maxSig : acS3;
	maxSig = (maxSig > acS4) ? maxSig : acS4;
	maxSig = (maxSig > acS5) ? maxSig : acS5;
		nxtDisplayTextLine(2, "maxSig: %d", maxSig);

		wait10Msec(2);
	}
}

void DriveSquareTest()
{
	Turn90(Left);
	GoInches(InchesToTape, DRIVE_SPEED);
	Turn90(Left);
	GoInches(InchesToTape, DRIVE_SPEED);
	Turn90(Left);
	GoInches(InchesToTape, DRIVE_SPEED);
	Turn90(Left);
	GoInches(InchesToTape, DRIVE_SPEED);

	Turn90(Right);
	GoInches(InchesToTape, DRIVE_SPEED);
	Turn90(Right);
	GoInches(InchesToTape, DRIVE_SPEED);
	Turn90(Right);
	GoInches(InchesToTape, DRIVE_SPEED);
	Turn90(Right);
	GoInches(InchesToTape, DRIVE_SPEED);
}

// This function uses encoders to turn.
void PointTurn(string direction)
{
	// Adjust the requested direction to reflect the actual location of the beacon.
	direction = beaconDirection == direction ? "L" : "R";
	ResetEncoders();
	nMotorEncoderTarget[motorL] = direction == "L" ? -ENCODER_TICKS_90_TURN : ENCODER_TICKS_90_TURN;
	nMotorEncoderTarget[motorR] = direction == "L" ? ENCODER_TICKS_90_TURN : -ENCODER_TICKS_90_TURN;
	wait1Msec(200);
	motorReq[motorL] = DRIVE_SPEED;
	motorReq[motorR] = -DRIVE_SPEED;
	while (nMotorRunState[motorL] != runStateIdle || nMotorRunState[motorR] != runStateIdle)
	{
	}
}

// Turn 90 degrees.
void Turn90(string direction)
{
	// Adjust the requested direction to reflect the actual location of the beacon.
	direction = beaconDirection == direction ? "L" : "R";
	motorReq[motorL] = direction == "L" ? -DRIVE_SPEED : DRIVE_SPEED;
	motorReq[motorR] = -motor[motorL];
	wait10Msec(turnTime);
	StopMotors();
}

task main()
{
	initializeRobot();
	//waitForStart();

	wait10Msec(BEFORE_START_10MS);
	StartTask(MotorSlewRateTask);

	//MovetoIR();
	//DumpBlock();
	//BackToStart();
	Turn90(Left);
	GoInches(InchesToTape, DRIVE_SPEED);
	Turn90(Right);
	GoInches(InchesToRamp, DRIVE_SPEED);

	// Test Function Here
	PointTurn(Left);
	DriveSquareTest();
	//LookForBeacon();

	StopMotors();
	wait1Msec(200);
	StopTask(MotorSlewRateTask);

	// Wait for FCS to stop us.
	while (true)
	{
		//note to self play songs
		PlaySoundFile("woops.rso");
	}
}
