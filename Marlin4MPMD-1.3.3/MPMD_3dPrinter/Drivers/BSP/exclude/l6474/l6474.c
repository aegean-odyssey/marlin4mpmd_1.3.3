/**
  ******************************************************************************
  * @file    l6474.c
  * @author  IPC Rennes
  * @version V1.5.0
  * @date    November 12, 2014
  * @brief   L6474 driver (fully integrated microstepping motor driver)
  * @note    (C) COPYRIGHT 2014 STMicroelectronics
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2014 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "l6474.h"

/* Private constants  ---------------------------------------------------------*/

    
/** @addtogroup BSP
  * @{
  */   
   
/** @defgroup L6474
  * @{
  */   

/* Private constants ---------------------------------------------------------*/    

/** @defgroup L6474_Private_Constants
  * @{
  */   

/// Error while initialising the SPI
#define L6474_ERROR_0   (0x8000)   
/// Error: Bad SPI transaction
#define L6474_ERROR_1   (0x8001)
    
/// Maximum number of steps
#define MAX_STEPS         (0x7FFFFFFF)

/// Maximum frequency of the PWMs in Hz
#define L6474_MAX_PWM_FREQ   (10000)

/// Minimum frequency of the PWMs in Hz
#define L6474_MIN_PWM_FREQ   (2)
    
/**
  * @}
  */ 
    
/* Private variables ---------------------------------------------------------*/

/** @defgroup L6474_Private_Variables
  * @{
  */       
    
/// Function pointer to flag interrupt call back
void (*flagInterruptCallback)(void);
/// Function pointer to error handler call back
void (*errorHandlerCallback)(uint16_t);
static volatile uint8_t numberOfDevices;
static uint8_t spiTxBursts[L6474_CMD_ARG_MAX_NB_BYTES][MAX_NUMBER_OF_DEVICES];
static uint8_t spiRxBursts[L6474_CMD_ARG_MAX_NB_BYTES][MAX_NUMBER_OF_DEVICES];
static volatile bool spiPreemtionByIsr = FALSE;
static volatile bool isrFlag = FALSE;
static uint16_t l6474DriverInstance = 0;

/// L6474 Device Paramaters structure
deviceParams_t devicePrm[MAX_NUMBER_OF_DEVICES];


/**
  * @}
  */ 

/* Private function prototypes -----------------------------------------------*/

/** @defgroup L6474_Private_functions
  * @{
  */  
void L6474_ApplySpeed(uint8_t pwmId, uint16_t newSpeed);
void L6474_ComputeSpeedProfile(uint8_t deviceId, uint32_t nbSteps);
int32_t L6474_ConvertPosition(uint32_t abs_position_reg); 
void L6474_ErrorHandler(uint16_t error);
void L6474_FlagInterruptHandler(void);                      
void L6474_SendCommand(uint8_t deviceId, uint8_t param);
void L6474_SetRegisterToPredefinedValues(uint8_t deviceId);
void L6474_WriteBytes(uint8_t *pByteToTransmit, uint8_t *pReceivedByte);    
void L6474_SetDeviceParamsToPredefinedValues(void);
void L6474_StartMovement(uint8_t deviceId);
void L6474_StepClockHandler(uint8_t deviceId);  
uint8_t L6474_Tval_Current_to_Par(double Tval);
uint8_t L6474_Tmin_Time_to_Par(double Tmin);

/**
  * @}
  */ 


/** @defgroup L6474_Exported_Variables
  * @{
  */       

/// L6474 motor driver functions pointer structure 
motorDrv_t   l6474Drv = 
{
  L6474_Init,
  L6474_ReadId,
  L6474_AttachErrorHandler,
  L6474_AttachFlagInterrupt,
  0,
  L6474_FlagInterruptHandler,
  L6474_GetAcceleration,
  L6474_GetCurrentSpeed,
  L6474_GetDeceleration,
  L6474_GetDeviceState,
  L6474_GetFwVersion,
  L6474_GetMark,
  L6474_GetMaxSpeed,
  L6474_GetMinSpeed,
  L6474_GetPosition,
  L6474_GoHome,
  L6474_GoMark,
  L6474_GoTo,
  L6474_HardStop,
  L6474_Move,
  L6474_ResetAllDevices,
  L6474_Run,
  L6474_SetAcceleration,
  L6474_SetDeceleration,
  L6474_SetHome,
  L6474_SetMark,
  L6474_SetMaxSpeed,
  L6474_SetMinSpeed,
  L6474_SoftStop,
  L6474_StepClockHandler,    
  L6474_WaitWhileActive,
  L6474_CmdDisable,
  L6474_CmdEnable,
  L6474_CmdGetParam,
  L6474_CmdGetStatus,
  L6474_CmdNop,
  L6474_CmdSetParam,
  L6474_ReadStatusRegister,
  L6474_ReleaseReset,
  L6474_Reset,
  L6474_SelectStepMode,
  L6474_SetDirection,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  L6474_ErrorHandler,
  0
};

/**
  * @}
  */ 

/** @defgroup Device_Control_Functions
  * @{
  */   

/******************************************************//**
 * @brief  Attaches a user callback to the error Handler.
 * The call back will be then called each time the library 
 * detects an error
 * @param[in] callback Name of the callback to attach 
 * to the error Hanlder
 * @retval None
 **********************************************************/
void L6474_AttachErrorHandler(void (*callback)(uint16_t))
{
  errorHandlerCallback = (void (*)(uint16_t))callback;
}

/******************************************************//**
 * @brief  Attaches a user callback to the flag Interrupt
 * The call back will be then called each time the status 
 * flag pin will be pulled down due to the occurrence of 
 * a programmed alarms ( OCD, thermal pre-warning or 
 * shutdown, UVLO, wrong command, non-performable command)
 * @param[in] callback Name of the callback to attach 
 * to the Flag Interrupt
 * @retval None
 **********************************************************/
void L6474_AttachFlagInterrupt(void (*callback)(void))
{
  flagInterruptCallback = (void (*)())callback;
}

/******************************************************//**
 * @brief Starts the L6474 library
 * @param[in] nbDevices Number of L6474 devices to use (from 1 to 3)
 * @retval None
 **********************************************************/
void L6474_Init(uint8_t nbDevices)
{
  uint32_t i;
  numberOfDevices = nbDevices;
  
  l6474DriverInstance++;
  
  /* Initialise the GPIOs */
  BSP_MotorControlBoard_GpioInit(nbDevices);
  
  if(BSP_MotorControlBoard_SpiInit() != 0)
  {
    /* Initialization Error */
    L6474_ErrorHandler(L6474_ERROR_0);
  } 

  /* Initialise the PWMs used for the Step clocks ----------------------------*/
  switch (nbDevices)
  {
    case 7:
      BSP_MotorControlBoard_PwmInit(6);            
    case 6:
      BSP_MotorControlBoard_PwmInit(5);        
    case 5:
      BSP_MotorControlBoard_PwmInit(4);    
    case 4:
      BSP_MotorControlBoard_PwmInit(3);
    case 3:
      BSP_MotorControlBoard_PwmInit(2);
    case 2:
      BSP_MotorControlBoard_PwmInit(1);
    case 1:
      BSP_MotorControlBoard_PwmInit(0);
    default:
      ;
  }
 
  /* Initialise the L6474s ------------------------------------------------*/
  
  /* Standby-reset deactivation */
  BSP_MotorControlBoard_ReleaseReset();
  
  /* Let a delay after reset */
  BSP_MotorControlBoard_Delay(1); 
  
  /* Set all registers and context variables to the predefined values from l6474_target_config.h */
  L6474_SetDeviceParamsToPredefinedValues();
  
  /* Disable L6474 powerstage */
  for (i = 0; i < nbDevices; i++)
  {
    L6474_CmdDisable(i);
    /* Get Status to clear flags after start up */
    L6474_CmdGetStatus(i);
  }
}

/******************************************************//**
 * @brief Returns the acceleration of the specified device
 * @param[in] deviceId (from 0 to 2)
 * @retval Acceleration in pps^2
 **********************************************************/
uint16_t L6474_GetAcceleration(uint8_t deviceId)
{                                                  
  return (devicePrm[deviceId].acceleration);
}            

/******************************************************//**
 * @brief Returns the current speed of the specified device
 * @param[in] deviceId (from 0 to 2)
 * @retval Speed in pps
 **********************************************************/
uint16_t L6474_GetCurrentSpeed(uint8_t deviceId)
{
  return devicePrm[deviceId].speed;
}

/******************************************************//**
 * @brief Returns the deceleration of the specified device
 * @param[in] deviceId (from 0 to 2)
 * @retval Deceleration in pps^2
 **********************************************************/
uint16_t L6474_GetDeceleration(uint8_t deviceId)
{                                                  
  return (devicePrm[deviceId].deceleration);
}          

/******************************************************//**
 * @brief Returns the device state
 * @param[in] deviceId (from 0 to 2)
 * @retval State (ACCELERATING, DECELERATING, STEADY or INACTIVE)
 **********************************************************/
motorState_t L6474_GetDeviceState(uint8_t deviceId)
{
  return devicePrm[deviceId].motionState;
}

/******************************************************//**
 * @brief Returns the FW version of the library
 * @param None
 * @retval L6474_FW_VERSION
 **********************************************************/
uint8_t L6474_GetFwVersion(void)
{
  return (L6474_FW_VERSION);
}

/******************************************************//**
 * @brief  Return motor handle (pointer to the L6474 motor driver structure)
 * @param None
 * @retval Pointer to the motorDrv_t structure
 **********************************************************/
motorDrv_t* L6474_GetMotorHandle(void)
{
  return (&l6474Drv);
}

/******************************************************//**
 * @brief  Returns the mark position  of the specified device
 * @param[in] deviceId (from 0 to 2)
 * @retval Mark register value converted in a 32b signed integer 
 **********************************************************/
int32_t L6474_GetMark(uint8_t deviceId)
{
  return L6474_ConvertPosition(L6474_CmdGetParam(deviceId,L6474_MARK));
}

/******************************************************//**
 * @brief  Returns the max speed of the specified device
 * @param[in] deviceId (from 0 to 2)
 * @retval maxSpeed in pps
 **********************************************************/
uint16_t L6474_GetMaxSpeed(uint8_t deviceId)
{                                                  
  return (devicePrm[deviceId].maxSpeed);
}

/******************************************************//**
 * @brief  Returns the min speed of the specified device
 * @param[in] deviceId (from 0 to 2)
 * @retval minSpeed in pps
 **********************************************************/
uint16_t L6474_GetMinSpeed(uint8_t deviceId)
{                                                  
  return (devicePrm[deviceId].minSpeed);
}                                                     

/******************************************************//**
 * @brief  Returns the ABS_POSITION of the specified device
 * @param[in] deviceId (from 0 to 2)
 * @retval ABS_POSITION register value converted in a 32b signed integer
 **********************************************************/
int32_t L6474_GetPosition(uint8_t deviceId)
{
  return L6474_ConvertPosition(L6474_CmdGetParam(deviceId,L6474_ABS_POS));
}



/******************************************************//**
 * @brief  Requests the motor to move to the home position (ABS_POSITION = 0)
 * @param[in] deviceId (from 0 to 2)
 * @retval None
 **********************************************************/
void L6474_GoHome(uint8_t deviceId)
{
  L6474_GoTo(deviceId, 0);
} 
  
/******************************************************//**
 * @brief  Requests the motor to move to the mark position 
 * @param[in] deviceId (from 0 to 2)
 * @retval None
 **********************************************************/
void L6474_GoMark(uint8_t deviceId)
{
	uint32_t mark;

	mark = L6474_ConvertPosition(L6474_CmdGetParam(deviceId,L6474_MARK));
	L6474_GoTo(deviceId,mark);  
}

/******************************************************//**
 * @brief  Requests the motor to move to the specified position 
 * @param[in] deviceId (from 0 to 2)
 * @param[in] targetPosition absolute position in steps
 * @retval None
 **********************************************************/
void L6474_GoTo(uint8_t deviceId, int32_t targetPosition)
{
  motorDir_t direction;
  int32_t steps;
  
  /* Eventually deactivate motor */
  if (devicePrm[deviceId].motionState != INACTIVE) 
  {
    L6474_HardStop(deviceId);
  }

  /* Get current position */
  devicePrm[deviceId].currentPosition = L6474_ConvertPosition(L6474_CmdGetParam(deviceId,L6474_ABS_POS));
  
  /* Compute the number of steps to perform */
  steps = targetPosition - devicePrm[deviceId].currentPosition;
  
  if (steps >= 0) 
  {
    devicePrm[deviceId].stepsToTake = steps;
    direction = FORWARD;
    
  } 
  else 
  {
    devicePrm[deviceId].stepsToTake = -steps;
    direction = BACKWARD;
  }
  
  if (steps != 0) 
  {
    
    devicePrm[deviceId].commandExecuted = MOVE_CMD;
        
    /* Direction setup */
    L6474_SetDirection(deviceId,direction);

    L6474_ComputeSpeedProfile(deviceId, devicePrm[deviceId].stepsToTake);
    
    /* Motor activation */
    L6474_StartMovement(deviceId);
  }  
}

/******************************************************//**
 * @brief  Immediatly stops the motor and disable the power bridge
 * @param[in] deviceId (from 0 to 2)
 * @retval None
 **********************************************************/
void L6474_HardStop(uint8_t deviceId) 
{
  /* Disable corresponding PWM */
  BSP_MotorControlBoard_PwmStop(deviceId);

  /* Disable power stage */
  L6474_CmdDisable(deviceId);

  /* Set inactive state */
  devicePrm[deviceId].motionState = INACTIVE;
  devicePrm[deviceId].commandExecuted = NO_CMD;
  devicePrm[deviceId].stepsToTake = MAX_STEPS;  
}

/******************************************************//**
 * @brief  Moves the motor of the specified number of steps
 * @param[in] deviceId (from 0 to 2)
 * @param[in] direction FORWARD or BACKWARD
 * @param[in] stepCount Number of steps to perform
 * @retval None
 **********************************************************/
void L6474_Move(uint8_t deviceId, motorDir_t direction, uint32_t stepCount)
{
  /* Eventually deactivate motor */
  if (devicePrm[deviceId].motionState != INACTIVE) 
  {
    L6474_HardStop(deviceId);
  }
  
  if (stepCount != 0) 
  {
    devicePrm[deviceId].stepsToTake = stepCount;
    
    devicePrm[deviceId].commandExecuted = MOVE_CMD;
    
    devicePrm[deviceId].currentPosition = L6474_ConvertPosition(L6474_CmdGetParam(deviceId,L6474_ABS_POS));
    
    /* Direction setup */
    L6474_SetDirection(deviceId,direction);

    L6474_ComputeSpeedProfile(deviceId, stepCount);
    
    /* Motor activation */
    L6474_StartMovement(deviceId);
  }  
}

/******************************************************//**
 * @brief Read id
 * @param None
 * @retval Id of the l6474 Driver Instance
 **********************************************************/
uint16_t L6474_ReadId(void)
{
  return(l6474DriverInstance);
}

/******************************************************//**
 * @brief Resets all L6474 devices
 * @param None
 * @retval None
 **********************************************************/
void L6474_ResetAllDevices(void)
{
 	uint8_t loop;
 	
 	for (loop = 0; loop < numberOfDevices; loop++)
 	{
   	/* Stop movement and disable power stage*/
  	L6474_HardStop(loop);
  }
	L6474_Reset();
  BSP_MotorControlBoard_Delay(1); // Reset pin must be forced low for at least 10us
	BSP_MotorControlBoard_ReleaseReset();
  BSP_MotorControlBoard_Delay(1); 
}

/******************************************************//**
 * @brief  Runs the motor. It will accelerate from the min 
 * speed up to the max speed by using the device acceleration.
 * @param[in] deviceId (from 0 to 2)
 * @param[in] direction FORWARD or BACKWARD
 * @retval None
 **********************************************************/
void L6474_Run(uint8_t deviceId, motorDir_t direction)
{
  /* Eventually deactivate motor */
  if (devicePrm[deviceId].motionState != INACTIVE) 
  {
    L6474_HardStop(deviceId);
  }
  
	/* Direction setup */
	L6474_SetDirection(deviceId,direction);

	devicePrm[deviceId].commandExecuted = RUN_CMD;

	/* Motor activation */
	L6474_StartMovement(deviceId); 
}

/******************************************************//**
 * @brief  Changes the acceleration of the specified device
 * @param[in] deviceId (from 0 to 2)
 * @param[in] newAcc New acceleration to apply in pps^2
 * @retval true if the command is successfully executed, else false
 * @note The command is not performed is the device is executing 
 * a MOVE or GOTO command (but it can be used during a RUN command)
 **********************************************************/
bool L6474_SetAcceleration(uint8_t deviceId,uint16_t newAcc)
{                                                  
  bool cmdExecuted = FALSE;
  if ((newAcc != 0)&&
      ((devicePrm[deviceId].motionState == INACTIVE)||
       (devicePrm[deviceId].commandExecuted == RUN_CMD)))
  {
    devicePrm[deviceId].acceleration = newAcc;
    cmdExecuted = TRUE;
  }    
  return cmdExecuted;
}            

/******************************************************//**
 * @brief  Changes the deceleration of the specified device
 * @param[in] deviceId (from 0 to 2)
 * @param[in] newDec New deceleration to apply in pps^2
 * @retval true if the command is successfully executed, else false
 * @note The command is not performed is the device is executing 
 * a MOVE or GOTO command (but it can be used during a RUN command)
 **********************************************************/
bool L6474_SetDeceleration(uint8_t deviceId, uint16_t newDec)
{                                                  
  bool cmdExecuted = FALSE;
  if ((newDec != 0)&& 
      ((devicePrm[deviceId].motionState == INACTIVE)||
       (devicePrm[deviceId].commandExecuted == RUN_CMD)))
  {
    devicePrm[deviceId].deceleration = newDec;
    cmdExecuted = TRUE;
  }      
  return cmdExecuted;
}        

/******************************************************//**
 * @brief  Set current position to be the Home position (ABS pos set to 0)
 * @param[in] deviceId (from 0 to 2)
 * @retval None
 **********************************************************/
void L6474_SetHome(uint8_t deviceId)
{
  L6474_CmdSetParam(deviceId, L6474_ABS_POS, 0);
}
 
/******************************************************//**
 * @brief  Sets current position to be the Mark position 
 * @param[in] deviceId (from 0 to 2)
 * @retval None
 **********************************************************/
void L6474_SetMark(uint8_t deviceId)
{
  uint32_t mark = L6474_CmdGetParam(deviceId,L6474_ABS_POS);
  L6474_CmdSetParam(deviceId,L6474_MARK, mark);
}

/******************************************************//**
 * @brief  Changes the max speed of the specified device
 * @param[in] deviceId (from 0 to 2)
 * @param[in] newMaxSpeed New max speed  to apply in pps
 * @retval true if the command is successfully executed, else false
 * @note The command is not performed is the device is executing 
 * a MOVE or GOTO command (but it can be used during a RUN command).
 **********************************************************/
bool L6474_SetMaxSpeed(uint8_t deviceId, uint16_t newMaxSpeed)
{                                                  
  bool cmdExecuted = FALSE;
  if ((newMaxSpeed >= L6474_MIN_PWM_FREQ)&&
      (newMaxSpeed <= L6474_MAX_PWM_FREQ) &&
      (devicePrm[deviceId].minSpeed <= newMaxSpeed) &&
      ((devicePrm[deviceId].motionState == INACTIVE)||
       (devicePrm[deviceId].commandExecuted == RUN_CMD)))
  {
    devicePrm[deviceId].maxSpeed = newMaxSpeed;
    cmdExecuted = TRUE;
  }
  return cmdExecuted;
}                                                     

/******************************************************//**
 * @brief  Changes the min speed of the specified device
 * @param[in] deviceId (from 0 to 2)
 * @param[in] newMinSpeed New min speed  to apply in pps
 * @retval true if the command is successfully executed, else false
 * @note The command is not performed is the device is executing 
 * a MOVE or GOTO command (but it can be used during a RUN command).
 **********************************************************/
bool L6474_SetMinSpeed(uint8_t deviceId, uint16_t newMinSpeed)
{                                                  
  bool cmdExecuted = FALSE;
  if ((newMinSpeed >= L6474_MIN_PWM_FREQ)&&
      (newMinSpeed <= L6474_MAX_PWM_FREQ) &&
      (newMinSpeed <= devicePrm[deviceId].maxSpeed) && 
      ((devicePrm[deviceId].motionState == INACTIVE)||
       (devicePrm[deviceId].commandExecuted == RUN_CMD)))
  {
    devicePrm[deviceId].minSpeed = newMinSpeed;
    cmdExecuted = TRUE;
  }  
  return cmdExecuted;
}                 

/******************************************************//**
 * @brief  Stops the motor by using the device deceleration
 * @param[in] deviceId (from 0 to 2)
 * @retval true if the command is successfully executed, else false
 * @note The command is not performed is the device is in INACTIVE state.
 **********************************************************/
bool L6474_SoftStop(uint8_t deviceId)
{	
  bool cmdExecuted = FALSE;
  if (devicePrm[deviceId].motionState != INACTIVE)
  {
    devicePrm[deviceId].commandExecuted = SOFT_STOP_CMD;
    cmdExecuted = TRUE;
  }
  return (cmdExecuted);
}

/******************************************************//**
 * @brief  Locks until the device state becomes Inactive
 * @param[in] deviceId (from 0 to 2)
 * @retval None
 **********************************************************/
void L6474_WaitWhileActive(uint8_t deviceId)
 {
	/* Wait while motor is running */
	while (L6474_GetDeviceState(deviceId) != INACTIVE);
}

/**
  * @}
  */
                         
/** @defgroup L6474_Control_Functions
  * @{
  */   

/******************************************************//**
 * @brief  Issue the Disable command to the L6474 of the specified device
 * @param[in] deviceId (from 0 to 2)
 * @retval None
 **********************************************************/
void L6474_CmdDisable(uint8_t deviceId)
{
  L6474_SendCommand(deviceId, L6474_DISABLE);
}

/******************************************************//**
 * @brief  Issues the Enable command to the L6474 of the specified device
 * @param[in] deviceId (from 0 to 2)
 * @retval None
 **********************************************************/
void L6474_CmdEnable(uint8_t deviceId)
{
  L6474_SendCommand(deviceId, L6474_ENABLE);
}

/******************************************************//**
 * @brief  Issues the GetParam command to the L6474 of the specified device
 * @param[in] deviceId (from 0 to 2)
 * @param[in] param Register adress (L6474_ABS_POS, L6474_MARK,...)
 * @retval Register value
 **********************************************************/
uint32_t L6474_CmdGetParam(uint8_t deviceId, uint32_t param)
{
  uint32_t i;
  uint32_t spiRxData;
  uint8_t maxArgumentNbBytes = 0;
  uint8_t spiIndex = numberOfDevices - deviceId - 1;
  bool itDisable = FALSE;  
  
  do
  {
    spiPreemtionByIsr = FALSE;
    if (itDisable)
    {
      /* re-enable BSP_MotorControlBoard_EnableIrq if disable in previous iteration */
      BSP_MotorControlBoard_EnableIrq();
      itDisable = FALSE;
    }
  
    for (i = 0; i < numberOfDevices; i++)
    {
      spiTxBursts[0][i] = L6474_NOP;
      spiTxBursts[1][i] = L6474_NOP;
      spiTxBursts[2][i] = L6474_NOP;
      spiTxBursts[3][i] = L6474_NOP;
      spiRxBursts[1][i] = 0;
      spiRxBursts[2][i] = 0;
      spiRxBursts[3][i] = 0;    
    }
    switch (param)
    {
      case L6474_ABS_POS: ;
      case L6474_MARK:
        spiTxBursts[0][spiIndex] = ((uint8_t)L6474_GET_PARAM )| (param);
        maxArgumentNbBytes = 3;
        break;
      case L6474_EL_POS: ;
      case L6474_CONFIG: ;
      case L6474_STATUS:
        spiTxBursts[1][spiIndex] = ((uint8_t)L6474_GET_PARAM )| (param);
        maxArgumentNbBytes = 2;
        break;
      default:
        spiTxBursts[2][spiIndex] = ((uint8_t)L6474_GET_PARAM )| (param);
        maxArgumentNbBytes = 1;
    }
    
    /* Disable interruption before checking */
    /* pre-emption by ISR and SPI transfers*/
    BSP_MotorControlBoard_DisableIrq();
    itDisable = TRUE;
  } while (spiPreemtionByIsr); // check pre-emption by ISR
    
  for (i = L6474_CMD_ARG_MAX_NB_BYTES-1-maxArgumentNbBytes;
       i < L6474_CMD_ARG_MAX_NB_BYTES;
       i++)
  {
     L6474_WriteBytes(&spiTxBursts[i][0],
                          &spiRxBursts[i][0]);
  }
  
  spiRxData = ((uint32_t)spiRxBursts[1][spiIndex] << 16)|
              (spiRxBursts[2][spiIndex] << 8) |
              (spiRxBursts[3][spiIndex]);
  
  /* re-enable BSP_MotorControlBoard_EnableIrq after SPI transfers*/
  BSP_MotorControlBoard_EnableIrq();
    
  return (spiRxData);
}

/******************************************************//**
 * @brief  Issues the GetStatus command to the L6474 of the specified device
 * @param[in] deviceId (from 0 to 2)
 * @retval Status Register value
 * @note Once the GetStatus command is performed, the flags of the status register
 * are reset. This is not the case when the status register is read with the
 * GetParam command (via the functions L6474ReadStatusRegister or L6474CmdGetParam).
 **********************************************************/
uint16_t L6474_CmdGetStatus(uint8_t deviceId)
{
  uint32_t i;
  uint16_t status;
  uint8_t spiIndex = numberOfDevices - deviceId - 1;
  bool itDisable = FALSE;  
  
  do
  {
    spiPreemtionByIsr = FALSE;
    if (itDisable)
    {
      /* re-enable BSP_MotorControlBoard_EnableIrq if disable in previous iteration */
      BSP_MotorControlBoard_EnableIrq();
      itDisable = FALSE;
    }

    for (i = 0; i < numberOfDevices; i++)
    {
       spiTxBursts[0][i] = L6474_NOP;
       spiTxBursts[1][i] = L6474_NOP;
       spiTxBursts[2][i] = L6474_NOP;
       spiRxBursts[1][i] = 0;
       spiRxBursts[2][i] = 0;
    }
    spiTxBursts[0][spiIndex] = L6474_GET_STATUS;

    /* Disable interruption before checking */
    /* pre-emption by ISR and SPI transfers*/
    BSP_MotorControlBoard_DisableIrq();
    itDisable = TRUE;
  } while (spiPreemtionByIsr); // check pre-emption by ISR

  for (i = 0; i < L6474_CMD_ARG_NB_BYTES_GET_STATUS + L6474_RSP_NB_BYTES_GET_STATUS; i++)
  {
     L6474_WriteBytes(&spiTxBursts[i][0], &spiRxBursts[i][0]);
  }
  status = (spiRxBursts[1][spiIndex] << 8) | (spiRxBursts[2][spiIndex]);
  
  /* re-enable BSP_MotorControlBoard_EnableIrq after SPI transfers*/
  BSP_MotorControlBoard_EnableIrq();
  
  return (status);
}

/******************************************************//**
 * @brief  Issues the Nop command to the L6474 of the specified device
 * @param[in] deviceId (from 0 to 2)
 * @retval None
 **********************************************************/
void L6474_CmdNop(uint8_t deviceId)
{
  L6474_SendCommand(deviceId, L6474_NOP);
}

/******************************************************//**
 * @brief  Issues the SetParam command to the L6474 of the specified device
 * @param[in] deviceId (from 0 to 2)
 * @param[in] param Register adress (L6474_ABS_POS, L6474_MARK,...)
 * @param[in] value Value to set in the register
 * @retval None
 **********************************************************/
void L6474_CmdSetParam(uint8_t deviceId,
                       uint32_t param,
                       uint32_t value)
{
  uint32_t i;
  uint8_t maxArgumentNbBytes = 0;
  uint8_t spiIndex = numberOfDevices - deviceId - 1;
  bool itDisable = FALSE;  
  do
  {
    spiPreemtionByIsr = FALSE;
    if (itDisable)
    {
      /* re-enable BSP_MotorControlBoard_EnableIrq if disable in previous iteration */
      BSP_MotorControlBoard_EnableIrq();
      itDisable = FALSE;
    }
    for (i = 0; i < numberOfDevices; i++)
    {
      spiTxBursts[0][i] = L6474_NOP;
      spiTxBursts[1][i] = L6474_NOP;
      spiTxBursts[2][i] = L6474_NOP;
      spiTxBursts[3][i] = L6474_NOP;
    }
    switch (param)
  {
    case L6474_ABS_POS: ;
    case L6474_MARK:
        spiTxBursts[0][spiIndex] = param;
        spiTxBursts[1][spiIndex] = (uint8_t)(value >> 16);
        spiTxBursts[2][spiIndex] = (uint8_t)(value >> 8);
        maxArgumentNbBytes = 3;
        break;
    case L6474_EL_POS: ;
    case L6474_CONFIG:
        spiTxBursts[1][spiIndex] = param;
        spiTxBursts[2][spiIndex] = (uint8_t)(value >> 8);
        maxArgumentNbBytes = 2;
        break;
    default:
        spiTxBursts[2][spiIndex] = param;
        maxArgumentNbBytes = 1;
    }
    spiTxBursts[3][spiIndex] = (uint8_t)(value);
    
    /* Disable interruption before checking */
    /* pre-emption by ISR and SPI transfers*/
    BSP_MotorControlBoard_DisableIrq();
    itDisable = TRUE;
  } while (spiPreemtionByIsr); // check pre-emption by ISR
 
  /* SPI transfer */
  for (i = L6474_CMD_ARG_MAX_NB_BYTES-1-maxArgumentNbBytes;
       i < L6474_CMD_ARG_MAX_NB_BYTES;
       i++)
  {
     L6474_WriteBytes(&spiTxBursts[i][0],&spiRxBursts[i][0]);
  }
  /* re-enable BSP_MotorControlBoard_EnableIrq after SPI transfers*/
  BSP_MotorControlBoard_EnableIrq();
}

/******************************************************//**
 * @brief  Reads the Status Register value
 * @param[in] deviceId (from 0 to 2)
 * @retval Status register valued
 * @note The status register flags are not cleared 
 * at the difference with L6474CmdGetStatus()
 **********************************************************/
uint16_t L6474_ReadStatusRegister(uint8_t deviceId)
{
  return (L6474_CmdGetParam(deviceId,L6474_STATUS));
}

/******************************************************//**
 * @brief  Releases the L6474 reset (pin set to High) of all devices
 * @param  None
 * @retval None
 **********************************************************/
void L6474_ReleaseReset(void)
{ 
  BSP_MotorControlBoard_ReleaseReset(); 
}

/******************************************************//**
 * @brief  Resets the L6474 (reset pin set to low) of all devices
 * @param  None
 * @retval None
 **********************************************************/
void L6474_Reset(void)
{
  BSP_MotorControlBoard_Reset(); 
}

/******************************************************//**
 * @brief  Set the stepping mode 
 * @param[in] deviceId (from 0 to 2)
 * @param[in] stepMod from full step to 1/16 microstep as specified in enum motorStepMode_t
 * @retval None
 **********************************************************/
void L6474_SelectStepMode(uint8_t deviceId, motorStepMode_t stepMod)
{
  uint8_t stepModeRegister;
  L6474_STEP_SEL_t l6474StepMod;
  
  switch (stepMod)
  {
    case STEP_MODE_FULL:
      l6474StepMod = L6474_STEP_SEL_1;
      break;
    case STEP_MODE_HALF:
      l6474StepMod = L6474_STEP_SEL_1_2;
      break;    
    case STEP_MODE_1_4:
      l6474StepMod = L6474_STEP_SEL_1_4;
      break;        
    case STEP_MODE_1_8:
      l6474StepMod = L6474_STEP_SEL_1_8;
      break;       
    case STEP_MODE_1_16:
    default:
      l6474StepMod = L6474_STEP_SEL_1_16;
      break;       
  }

  /* Eventually deactivate motor */
  if (devicePrm[deviceId].motionState != INACTIVE) 
  {
    L6474_HardStop(deviceId);
  }
  
  /* Read Step mode register and clear STEP_SEL field */
  stepModeRegister = (uint8_t)(0xF8 & L6474_CmdGetParam(deviceId,L6474_STEP_MODE)) ;
  
  /* Apply new step mode */
  L6474_CmdSetParam(deviceId, L6474_STEP_MODE, stepModeRegister | (uint8_t)l6474StepMod);

  /* Reset abs pos register */
  L6474_SetHome(deviceId);
}

/******************************************************//**
 * @brief  Specifies the direction 
 * @param[in] deviceId (from 0 to 2)
 * @param[in] dir FORWARD or BACKWARD
 * @note The direction change is only applied if the device 
 * is in INACTIVE state
 * @retval None
 **********************************************************/
void L6474_SetDirection(uint8_t deviceId, motorDir_t dir)
{
  if (devicePrm[deviceId].motionState == INACTIVE)
  {
    devicePrm[deviceId].direction = dir;
    BSP_MotorControlBoard_SetDirectionGpio(deviceId, dir);
  }
}

/**
  * @}
  */

/** @addtogroup L6474_Private_functions
  * @{
  */  

/******************************************************//**
 * @brief  Updates the current speed of the device
 * @param[in] deviceId (from 0 to 2)
 * @param[in] newSpeed in pps
 * @retval None
 **********************************************************/
void L6474_ApplySpeed(uint8_t deviceId, uint16_t newSpeed)
{
  if (newSpeed < L6474_MIN_PWM_FREQ)
  {
    newSpeed = L6474_MIN_PWM_FREQ;  
  }
  if (newSpeed > L6474_MAX_PWM_FREQ)
  {
    newSpeed = L6474_MAX_PWM_FREQ;
  }
  
  devicePrm[deviceId].speed = newSpeed;

  switch (deviceId)
  {
    case  0:
      BSP_MotorControlBoard_PwmXSetFreq(newSpeed);
      break;
    case 1:
      BSP_MotorControlBoard_PwmYSetFreq(newSpeed);
      break;
    case 2:
      BSP_MotorControlBoard_PwmZSetFreq(newSpeed);
      break;
    case 3:
      BSP_MotorControlBoard_PwmE1SetFreq(newSpeed);
      break;
#ifdef BSP_HEAT_E2_PIN
    case 4:
      BSP_MotorControlBoard_PwmE2SetFreq(newSpeed);
      break;
    case 5:
      BSP_MotorControlBoard_PwmE3SetFreq(newSpeed);
      break;
#endif
    default:
      break; //ignore error
  }
}

/******************************************************//**
 * @brief  Computes the speed profile according to the number of steps to move
 * @param[in] deviceId (from 0 to 2)
 * @param[in] nbSteps number of steps to perform
 * @retval None
 * @note Using the acceleration and deceleration of the device,
 * this function determines the duration in steps of the acceleration,
 * steady and deceleration phases.
 * If the total number of steps to perform is big enough, a trapezoidal move
 * is performed (i.e. there is a steady phase where the motor runs at the maximum
 * speed.
 * Else, a triangular move is performed (no steady phase: the maximum speed is never
 * reached.
 **********************************************************/
void L6474_ComputeSpeedProfile(uint8_t deviceId, uint32_t nbSteps)
{
  uint32_t reqAccSteps; 
	uint32_t reqDecSteps;
   
  /* compute the number of steps to get the targeted speed */
  uint16_t minSpeed = devicePrm[deviceId].minSpeed;
  reqAccSteps = (devicePrm[deviceId].maxSpeed - minSpeed);
  reqAccSteps *= (devicePrm[deviceId].maxSpeed + minSpeed);
  reqDecSteps = reqAccSteps;
  reqAccSteps /= (uint32_t)devicePrm[deviceId].acceleration;
  reqAccSteps /= 2;

  /* compute the number of steps to stop */
  reqDecSteps /= (uint32_t)devicePrm[deviceId].deceleration;
  reqDecSteps /= 2;

	if(( reqAccSteps + reqDecSteps ) > nbSteps)
	{	
    /* Triangular move  */
    /* reqDecSteps = (Pos * Dec) /(Dec+Acc) */
    uint32_t dec = devicePrm[deviceId].deceleration;
    uint32_t acc = devicePrm[deviceId].acceleration;
    
    reqDecSteps =  ((uint32_t) dec * nbSteps) / (acc + dec);
    if (reqDecSteps > 1)
    {
      reqAccSteps = reqDecSteps - 1;
      if(reqAccSteps == 0)
      {
        reqAccSteps = 1;
      }      
    }
    else
    {
      reqAccSteps = 0;
    }
    devicePrm[deviceId].endAccPos = reqAccSteps;
    devicePrm[deviceId].startDecPos = reqDecSteps;
	}
	else
	{	 
    /* Trapezoidal move */
    /* accelerating phase to endAccPos */
    /* steady phase from  endAccPos to startDecPos */
    /* decelerating from startDecPos to stepsToTake*/
    devicePrm[deviceId].endAccPos = reqAccSteps;
    devicePrm[deviceId].startDecPos = nbSteps - reqDecSteps - 1;
	}
}

/******************************************************//**
 * @brief  Converts the ABS_POSITION register value to a 32b signed integer
 * @param[in] abs_position_reg value of the ABS_POSITION register
 * @retval operation_result 32b signed integer corresponding to the absolute position 
 **********************************************************/
int32_t L6474_ConvertPosition(uint32_t abs_position_reg)
{
	int32_t operation_result;

  if (abs_position_reg & L6474_ABS_POS_SIGN_BIT_MASK) 
  {
		/* Negative register value */
		abs_position_reg = ~abs_position_reg;
		abs_position_reg += 1;

		operation_result = (int32_t) (abs_position_reg & L6474_ABS_POS_VALUE_MASK);
		operation_result = -operation_result;
  } 
  else 
  {
		operation_result = (int32_t) abs_position_reg;
	}
	return operation_result;
}

/******************************************************//**
 * @brief Error handler which calls the user callback (if defined)
 * @param[in] error Number of the error
 * @retval None
 **********************************************************/
void L6474_ErrorHandler(uint16_t error)
{
  if (errorHandlerCallback != 0)
  {
    (void) errorHandlerCallback(error);
  }
  else   
  {
    while(1)
    {
      /* Infinite loop */
    }
  }
}

/******************************************************//**
 * @brief  Handlers of the flag interrupt which calls the user callback (if defined)
 * @param None
 * @retval None
 **********************************************************/
void L6474_FlagInterruptHandler(void)
{
  if (flagInterruptCallback != 0)
  {
    /* Set isr flag */
    isrFlag = TRUE;
    
    flagInterruptCallback();
    
    /* Reset isr flag */
    isrFlag = FALSE;   
  }
}

/******************************************************//**
 * @brief  Sends a command without arguments to the L6474 via the SPI
 * @param[in] deviceId (from 0 to 2)
 * @param[in] param Command to send 
 * @retval None
 **********************************************************/
void L6474_SendCommand(uint8_t deviceId, uint8_t param)
{
  uint32_t i;
  uint8_t spiIndex = numberOfDevices - deviceId - 1;
  bool itDisable = FALSE;  
  
  do
  {
    spiPreemtionByIsr = FALSE;
    if (itDisable)
    {
      /* re-enable BSP_MotorControlBoard_EnableIrq if disable in previous iteration */
      BSP_MotorControlBoard_EnableIrq();
      itDisable = FALSE;
    }
  
    for (i = 0; i < numberOfDevices; i++)
    {
      spiTxBursts[3][i] = L6474_NOP;     
    }
    spiTxBursts[3][spiIndex] = param;
    
    /* Disable interruption before checking */
    /* pre-emption by ISR and SPI transfers*/
    BSP_MotorControlBoard_DisableIrq();
    itDisable = TRUE;
  } while (spiPreemtionByIsr); // check pre-emption by ISR

  L6474_WriteBytes(&spiTxBursts[3][0], &spiRxBursts[3][0]); 
  
  /* re-enable BSP_MotorControlBoard_EnableIrq after SPI transfers*/
  BSP_MotorControlBoard_EnableIrq();
}

/******************************************************//**
 * @brief  Sets the registers of the L6474 to their predefined values 
 * from l6474_target_config.h
 * @param[in] deviceId (from 0 to 2)
 * @retval None
 **********************************************************/
void L6474_SetRegisterToPredefinedValues(uint8_t deviceId)
{
  L6474_CmdSetParam(deviceId,
                    L6474_ABS_POS,
                    0);
  L6474_CmdSetParam(deviceId,
                    L6474_EL_POS,
                    0);
  L6474_CmdSetParam(deviceId,
                    L6474_MARK,
                    0);
  switch (deviceId)
  {
    case 0:
      L6474_CmdSetParam(deviceId,
                        L6474_TVAL,
                        L6474_Tval_Current_to_Par(L6474_CONF_PARAM_TVAL_DEVICE_0));
      L6474_CmdSetParam(deviceId,
                              L6474_T_FAST,
                              (uint8_t)L6474_CONF_PARAM_TOFF_FAST_DEVICE_0 |
                              (uint8_t)L6474_CONF_PARAM_FAST_STEP_DEVICE_0);
      L6474_CmdSetParam(deviceId,
                              L6474_TON_MIN,
                              L6474_Tmin_Time_to_Par(L6474_CONF_PARAM_TON_MIN_DEVICE_0)
                                );
      L6474_CmdSetParam(deviceId,
                              L6474_TOFF_MIN,
                              L6474_Tmin_Time_to_Par(L6474_CONF_PARAM_TOFF_MIN_DEVICE_0));
      L6474_CmdSetParam(deviceId,
                        L6474_OCD_TH,
                        L6474_CONF_PARAM_OCD_TH_DEVICE_0);
      L6474_CmdSetParam(deviceId,
                        L6474_STEP_MODE,
                        (uint8_t)L6474_CONF_PARAM_STEP_SEL_DEVICE_0 |
                        (uint8_t)L6474_CONF_PARAM_SYNC_SEL_DEVICE_0);
      L6474_CmdSetParam(deviceId,
                        L6474_ALARM_EN,
                        L6474_CONF_PARAM_ALARM_EN_DEVICE_0);
      L6474_CmdSetParam(deviceId,
                        L6474_CONFIG,
                        (uint16_t)L6474_CONF_PARAM_CLOCK_SETTING_DEVICE_0 |
                        (uint16_t)L6474_CONF_PARAM_TQ_REG_DEVICE_0 |
                        (uint16_t)L6474_CONF_PARAM_OC_SD_DEVICE_0 |
                        (uint16_t)L6474_CONF_PARAM_SR_DEVICE_0 |
                        (uint16_t)L6474_CONF_PARAM_TOFF_DEVICE_0);
      break;
    case 1:
      L6474_CmdSetParam(deviceId,
                        L6474_TVAL,
                        L6474_Tval_Current_to_Par(L6474_CONF_PARAM_TVAL_DEVICE_1));
      L6474_CmdSetParam(deviceId,
                        L6474_T_FAST,
                        (uint8_t)L6474_CONF_PARAM_TOFF_FAST_DEVICE_1 |
                        (uint8_t)L6474_CONF_PARAM_FAST_STEP_DEVICE_1);
      L6474_CmdSetParam(deviceId,
                        L6474_TON_MIN,
                        L6474_Tmin_Time_to_Par(L6474_CONF_PARAM_TON_MIN_DEVICE_1));
      L6474_CmdSetParam(deviceId,
                        L6474_TOFF_MIN,
                        L6474_Tmin_Time_to_Par(L6474_CONF_PARAM_TOFF_MIN_DEVICE_1));
      L6474_CmdSetParam(deviceId,
                        L6474_OCD_TH,
                        L6474_CONF_PARAM_OCD_TH_DEVICE_1);
      L6474_CmdSetParam(deviceId,
                        L6474_STEP_MODE,
                        (uint8_t)L6474_CONF_PARAM_STEP_SEL_DEVICE_1 |
                        (uint8_t)L6474_CONF_PARAM_SYNC_SEL_DEVICE_1);
      L6474_CmdSetParam(deviceId,
                        L6474_ALARM_EN,
                        L6474_CONF_PARAM_ALARM_EN_DEVICE_1);
      L6474_CmdSetParam(deviceId,
                        L6474_CONFIG,
                        (uint16_t)L6474_CONF_PARAM_CLOCK_SETTING_DEVICE_1 |
                        (uint16_t)L6474_CONF_PARAM_TQ_REG_DEVICE_1 |
                        (uint16_t)L6474_CONF_PARAM_OC_SD_DEVICE_1 |
                        (uint16_t)L6474_CONF_PARAM_SR_DEVICE_1 |
                        (uint16_t)L6474_CONF_PARAM_TOFF_DEVICE_1);
      break;
    case 2:
      L6474_CmdSetParam(deviceId,
                        L6474_TVAL,
                        L6474_Tval_Current_to_Par(L6474_CONF_PARAM_TVAL_DEVICE_2));
      L6474_CmdSetParam(deviceId,
                        L6474_T_FAST,
                        (uint8_t)L6474_CONF_PARAM_TOFF_FAST_DEVICE_2 |
                        (uint8_t)L6474_CONF_PARAM_FAST_STEP_DEVICE_2);
      L6474_CmdSetParam(deviceId,
                        L6474_TON_MIN,
                        L6474_Tmin_Time_to_Par(L6474_CONF_PARAM_TON_MIN_DEVICE_2));
      L6474_CmdSetParam(deviceId,
                        L6474_TOFF_MIN,
                        L6474_Tmin_Time_to_Par(L6474_CONF_PARAM_TOFF_MIN_DEVICE_2));
      L6474_CmdSetParam(deviceId,
                        L6474_OCD_TH,
                        L6474_CONF_PARAM_OCD_TH_DEVICE_2);
      L6474_CmdSetParam(deviceId,
                        L6474_STEP_MODE,
                        (uint8_t)L6474_CONF_PARAM_STEP_SEL_DEVICE_2 |
                        (uint8_t)L6474_CONF_PARAM_SYNC_SEL_DEVICE_2);
      L6474_CmdSetParam(deviceId,
                        L6474_ALARM_EN,
                        L6474_CONF_PARAM_ALARM_EN_DEVICE_2);
      L6474_CmdSetParam(deviceId,
                        L6474_CONFIG,
                        (uint16_t)L6474_CONF_PARAM_CLOCK_SETTING_DEVICE_2 |
                        (uint16_t)L6474_CONF_PARAM_TQ_REG_DEVICE_2 |
                        (uint16_t)L6474_CONF_PARAM_OC_SD_DEVICE_2 |
                        (uint16_t)L6474_CONF_PARAM_SR_DEVICE_2 |
                        (uint16_t)L6474_CONF_PARAM_TOFF_DEVICE_2);
      break;
   case 3:
      L6474_CmdSetParam(deviceId,
                        L6474_TVAL,
                        L6474_Tval_Current_to_Par(L6474_CONF_PARAM_TVAL_DEVICE_3));
      L6474_CmdSetParam(deviceId,
                        L6474_T_FAST,
                        (uint8_t)L6474_CONF_PARAM_TOFF_FAST_DEVICE_3 |
                        (uint8_t)L6474_CONF_PARAM_FAST_STEP_DEVICE_3);
      L6474_CmdSetParam(deviceId,
                        L6474_TON_MIN,
                        L6474_Tmin_Time_to_Par(L6474_CONF_PARAM_TON_MIN_DEVICE_3));
      L6474_CmdSetParam(deviceId,
                        L6474_TOFF_MIN,
                        L6474_Tmin_Time_to_Par(L6474_CONF_PARAM_TOFF_MIN_DEVICE_3));
      L6474_CmdSetParam(deviceId,
                        L6474_OCD_TH,
                        L6474_CONF_PARAM_OCD_TH_DEVICE_3);
      L6474_CmdSetParam(deviceId,
                        L6474_STEP_MODE,
                        (uint8_t)L6474_CONF_PARAM_STEP_SEL_DEVICE_3 |
                        (uint8_t)L6474_CONF_PARAM_SYNC_SEL_DEVICE_3);
      L6474_CmdSetParam(deviceId,
                        L6474_ALARM_EN,
                        L6474_CONF_PARAM_ALARM_EN_DEVICE_3);
      L6474_CmdSetParam(deviceId,
                        L6474_CONFIG,
                        (uint16_t)L6474_CONF_PARAM_CLOCK_SETTING_DEVICE_3 |
                        (uint16_t)L6474_CONF_PARAM_TQ_REG_DEVICE_3 |
                        (uint16_t)L6474_CONF_PARAM_OC_SD_DEVICE_3 |
                        (uint16_t)L6474_CONF_PARAM_SR_DEVICE_3 |
                        (uint16_t)L6474_CONF_PARAM_TOFF_DEVICE_3);
      break;      
   case 4:
      L6474_CmdSetParam(deviceId,
                        L6474_TVAL,
                        L6474_Tval_Current_to_Par(L6474_CONF_PARAM_TVAL_DEVICE_4));
      L6474_CmdSetParam(deviceId,
                        L6474_T_FAST,
                        (uint8_t)L6474_CONF_PARAM_TOFF_FAST_DEVICE_4 |
                        (uint8_t)L6474_CONF_PARAM_FAST_STEP_DEVICE_4);
      L6474_CmdSetParam(deviceId,
                        L6474_TON_MIN,
                        L6474_Tmin_Time_to_Par(L6474_CONF_PARAM_TON_MIN_DEVICE_4));
      L6474_CmdSetParam(deviceId,
                        L6474_TOFF_MIN,
                        L6474_Tmin_Time_to_Par(L6474_CONF_PARAM_TOFF_MIN_DEVICE_4));
      L6474_CmdSetParam(deviceId,
                        L6474_OCD_TH,
                        L6474_CONF_PARAM_OCD_TH_DEVICE_4);
      L6474_CmdSetParam(deviceId,
                        L6474_STEP_MODE,
                        (uint8_t)L6474_CONF_PARAM_STEP_SEL_DEVICE_4 |
                        (uint8_t)L6474_CONF_PARAM_SYNC_SEL_DEVICE_4);
      L6474_CmdSetParam(deviceId,
                        L6474_ALARM_EN,
                        L6474_CONF_PARAM_ALARM_EN_DEVICE_4);
      L6474_CmdSetParam(deviceId,
                        L6474_CONFIG,
                        (uint16_t)L6474_CONF_PARAM_CLOCK_SETTING_DEVICE_4 |
                        (uint16_t)L6474_CONF_PARAM_TQ_REG_DEVICE_4 |
                        (uint16_t)L6474_CONF_PARAM_OC_SD_DEVICE_4 |
                        (uint16_t)L6474_CONF_PARAM_SR_DEVICE_4 |
                        (uint16_t)L6474_CONF_PARAM_TOFF_DEVICE_4);
      break;         
   case 5:
      L6474_CmdSetParam(deviceId,
                        L6474_TVAL,
                        L6474_Tval_Current_to_Par(L6474_CONF_PARAM_TVAL_DEVICE_5));
      L6474_CmdSetParam(deviceId,
                        L6474_T_FAST,
                        (uint8_t)L6474_CONF_PARAM_TOFF_FAST_DEVICE_5 |
                        (uint8_t)L6474_CONF_PARAM_FAST_STEP_DEVICE_5);
      L6474_CmdSetParam(deviceId,
                        L6474_TON_MIN,
                        L6474_Tmin_Time_to_Par(L6474_CONF_PARAM_TON_MIN_DEVICE_5));
      L6474_CmdSetParam(deviceId,
                        L6474_TOFF_MIN,
                        L6474_Tmin_Time_to_Par(L6474_CONF_PARAM_TOFF_MIN_DEVICE_5));
      L6474_CmdSetParam(deviceId,
                        L6474_OCD_TH,
                        L6474_CONF_PARAM_OCD_TH_DEVICE_5);
      L6474_CmdSetParam(deviceId,
                        L6474_STEP_MODE,
                        (uint8_t)L6474_CONF_PARAM_STEP_SEL_DEVICE_5 |
                        (uint8_t)L6474_CONF_PARAM_SYNC_SEL_DEVICE_5);
      L6474_CmdSetParam(deviceId,
                        L6474_ALARM_EN,
                        L6474_CONF_PARAM_ALARM_EN_DEVICE_5);
      L6474_CmdSetParam(deviceId,
                        L6474_CONFIG,
                        (uint16_t)L6474_CONF_PARAM_CLOCK_SETTING_DEVICE_5 |
                        (uint16_t)L6474_CONF_PARAM_TQ_REG_DEVICE_5 |
                        (uint16_t)L6474_CONF_PARAM_OC_SD_DEVICE_5 |
                        (uint16_t)L6474_CONF_PARAM_SR_DEVICE_5 |
                        (uint16_t)L6474_CONF_PARAM_TOFF_DEVICE_5);
      break;        
   case 6:
      L6474_CmdSetParam(deviceId,
                        L6474_TVAL,
                        L6474_Tval_Current_to_Par(L6474_CONF_PARAM_TVAL_DEVICE_6));
      L6474_CmdSetParam(deviceId,
                        L6474_T_FAST,
                        (uint8_t)L6474_CONF_PARAM_TOFF_FAST_DEVICE_6 |
                        (uint8_t)L6474_CONF_PARAM_FAST_STEP_DEVICE_6);
      L6474_CmdSetParam(deviceId,
                        L6474_TON_MIN,
                        L6474_Tmin_Time_to_Par(L6474_CONF_PARAM_TON_MIN_DEVICE_6));
      L6474_CmdSetParam(deviceId,
                        L6474_TOFF_MIN,
                        L6474_Tmin_Time_to_Par(L6474_CONF_PARAM_TOFF_MIN_DEVICE_6));
      L6474_CmdSetParam(deviceId,
                        L6474_OCD_TH,
                        L6474_CONF_PARAM_OCD_TH_DEVICE_6);
      L6474_CmdSetParam(deviceId,
                        L6474_STEP_MODE,
                        (uint8_t)L6474_CONF_PARAM_STEP_SEL_DEVICE_6 |
                        (uint8_t)L6474_CONF_PARAM_SYNC_SEL_DEVICE_6);
      L6474_CmdSetParam(deviceId,
                        L6474_ALARM_EN,
                        L6474_CONF_PARAM_ALARM_EN_DEVICE_6);
      L6474_CmdSetParam(deviceId,
                        L6474_CONFIG,
                        (uint16_t)L6474_CONF_PARAM_CLOCK_SETTING_DEVICE_6 |
                        (uint16_t)L6474_CONF_PARAM_TQ_REG_DEVICE_6 |
                        (uint16_t)L6474_CONF_PARAM_OC_SD_DEVICE_6 |
                        (uint16_t)L6474_CONF_PARAM_SR_DEVICE_6 |
                        (uint16_t)L6474_CONF_PARAM_TOFF_DEVICE_6);
      break;          
    default: ;
  }
}

/******************************************************//**
 * @brief  Sets the parameters of the device to predefined values
 * from l6474_target_config.h
 * @param None
 * @retval None
 **********************************************************/
void L6474_SetDeviceParamsToPredefinedValues(void)
{
  uint32_t i;

  devicePrm[0].acceleration = L6474_CONF_PARAM_ACC_DEVICE_0;
  devicePrm[0].deceleration = L6474_CONF_PARAM_DEC_DEVICE_0;
  devicePrm[0].maxSpeed = L6474_CONF_PARAM_MAX_SPEED_DEVICE_0;
  devicePrm[0].minSpeed = L6474_CONF_PARAM_MIN_SPEED_DEVICE_0;
  
  devicePrm[1].acceleration = L6474_CONF_PARAM_ACC_DEVICE_1;
  devicePrm[1].deceleration = L6474_CONF_PARAM_DEC_DEVICE_1;
  devicePrm[1].maxSpeed = L6474_CONF_PARAM_MAX_SPEED_DEVICE_1;
  devicePrm[1].minSpeed = L6474_CONF_PARAM_MIN_SPEED_DEVICE_1;
  
  devicePrm[2].acceleration = L6474_CONF_PARAM_ACC_DEVICE_2;
  devicePrm[2].deceleration = L6474_CONF_PARAM_DEC_DEVICE_2;
  devicePrm[2].maxSpeed = L6474_CONF_PARAM_MAX_SPEED_DEVICE_2;
  devicePrm[2].minSpeed = L6474_CONF_PARAM_MIN_SPEED_DEVICE_2;
  
  for (i = 0; i < MAX_NUMBER_OF_DEVICES; i++)
  {
    devicePrm[i].accu = 0;
    devicePrm[i].currentPosition = 0;
    devicePrm[i].endAccPos = 0;
    devicePrm[i].relativePos = 0;
    devicePrm[i].startDecPos = 0;
    devicePrm[i].stepsToTake = 0;
    devicePrm[i].speed = 0;
    devicePrm[i].commandExecuted = NO_CMD;
    devicePrm[i].direction = FORWARD;
    devicePrm[i].motionState = INACTIVE;
  }
  
  for (i = 0; i < numberOfDevices; i++)
  {
    L6474_SetRegisterToPredefinedValues(i);
  }   
}

/******************************************************//**
 * @brief Initialises the bridge parameters to start the movement
 * and enable the power bridge
 * @param[in] deviceId (from 0 to 2)
 * @retval None
 **********************************************************/
void L6474_StartMovement(uint8_t deviceId)  
{
  /* Enable L6474 powerstage */
  L6474_CmdEnable(deviceId);
  if (devicePrm[deviceId].endAccPos != 0)
  {
    devicePrm[deviceId].motionState = ACCELERATING;
  }
  else
  {
    devicePrm[deviceId].motionState = DECELERATING;    
  }
  devicePrm[deviceId].accu = 0;
  devicePrm[deviceId].relativePos = 0;
  L6474_ApplySpeed(deviceId, devicePrm[deviceId].minSpeed);
}

/******************************************************//**
 * @brief  Handles the device state machine at each ste
 * @param[in] deviceId (from 0 to 2)
 * @retval None
 * @note Must only be called by the timer ISR
 **********************************************************/
void L6474_StepClockHandler(uint8_t deviceId)
{
  /* Set isr flag */
  isrFlag = TRUE;
  
  /* Incrementation of the relative position */
  devicePrm[deviceId].relativePos++;

  switch (devicePrm[deviceId].motionState) 
  {
    case ACCELERATING: 
    {
        uint32_t relPos = devicePrm[deviceId].relativePos;
        uint32_t endAccPos = devicePrm[deviceId].endAccPos;
        uint16_t speed = devicePrm[deviceId].speed;
        uint32_t acc = ((uint32_t)devicePrm[deviceId].acceleration << 16);
        
        if ((devicePrm[deviceId].commandExecuted == SOFT_STOP_CMD)||
            ((devicePrm[deviceId].commandExecuted != RUN_CMD)&&  
             (relPos == devicePrm[deviceId].startDecPos)))
        {
          devicePrm[deviceId].motionState = DECELERATING;
          devicePrm[deviceId].accu = 0;
        }
        else if ((speed >= devicePrm[deviceId].maxSpeed)||
                 ((devicePrm[deviceId].commandExecuted != RUN_CMD)&&
                  (relPos == endAccPos)))
        {
          devicePrm[deviceId].motionState = STEADY;
        }
        else
        {
          bool speedUpdated = FALSE;
          /* Go on accelerating */
          if (speed == 0) speed =1;
          devicePrm[deviceId].accu += acc / speed;
          while (devicePrm[deviceId].accu >= (0X10000L))
          {
            devicePrm[deviceId].accu -= (0X10000L);
            speed +=1;
            speedUpdated = TRUE;
          }
          
          if (speedUpdated)
          {
            if (speed > devicePrm[deviceId].maxSpeed)
            {
              speed = devicePrm[deviceId].maxSpeed;
            }    
            devicePrm[deviceId].speed = speed;
            L6474_ApplySpeed(deviceId, devicePrm[deviceId].speed);
          }
        }
        break;
    }
    case STEADY: 
    {
      uint16_t maxSpeed = devicePrm[deviceId].maxSpeed;
      uint32_t relativePos = devicePrm[deviceId].relativePos;
      if  ((devicePrm[deviceId].commandExecuted == SOFT_STOP_CMD)||
           ((devicePrm[deviceId].commandExecuted != RUN_CMD)&&
            (relativePos >= (devicePrm[deviceId].startDecPos))) ||
           ((devicePrm[deviceId].commandExecuted == RUN_CMD)&&
            (devicePrm[deviceId].speed > maxSpeed)))
      {
        devicePrm[deviceId].motionState = DECELERATING;
        devicePrm[deviceId].accu = 0;
      }
      else if ((devicePrm[deviceId].commandExecuted == RUN_CMD)&&
               (devicePrm[deviceId].speed < maxSpeed))
      {
        devicePrm[deviceId].motionState = ACCELERATING;
        devicePrm[deviceId].accu = 0;
      }
      break;
    }
    case DECELERATING: 
    {
      uint32_t relativePos = devicePrm[deviceId].relativePos;
      uint16_t speed = devicePrm[deviceId].speed;
      uint32_t deceleration = ((uint32_t)devicePrm[deviceId].deceleration << 16);
      if (((devicePrm[deviceId].commandExecuted == SOFT_STOP_CMD)&&(speed <=  devicePrm[deviceId].minSpeed))||
          ((devicePrm[deviceId].commandExecuted != RUN_CMD)&&
           (relativePos >= devicePrm[deviceId].stepsToTake)))
      {
        /* Motion process complete */
        L6474_HardStop(deviceId);
      }
      else if ((devicePrm[deviceId].commandExecuted == RUN_CMD)&&
               (speed <= devicePrm[deviceId].maxSpeed))
      {
        devicePrm[deviceId].motionState = STEADY;
      }
      else
      {
        /* Go on decelerating */
        if (speed > devicePrm[deviceId].minSpeed)
        {
          bool speedUpdated = FALSE;
          if (speed == 0) speed =1;
          devicePrm[deviceId].accu += deceleration / speed;
          while (devicePrm[deviceId].accu >= (0X10000L))
          {
            devicePrm[deviceId].accu -= (0X10000L);
            if (speed > 1)
            {  
              speed -=1;
            }
            speedUpdated = TRUE;
          }
        
          if (speedUpdated)
          {
            if (speed < devicePrm[deviceId].minSpeed)
            {
              speed = devicePrm[deviceId].minSpeed;
            }  
            devicePrm[deviceId].speed = speed;
            L6474_ApplySpeed(deviceId, devicePrm[deviceId].speed);
          }
        }
      }
      break;
    }
    default: 
    {
      break;
    }
  }  
  /* Set isr flag */
  isrFlag = FALSE;
}

/******************************************************//**
 * @brief Converts mA in compatible values for TVAL register 
 * @param[in] Tval
 * @retval TVAL values
 **********************************************************/
inline uint8_t L6474_Tval_Current_to_Par(double Tval)
{
  return ((uint8_t)(((Tval - 31.25)/31.25)+0.5));
}

/******************************************************//**
 * @brief Convert time in us in compatible values 
 * for TON_MIN register
 * @param[in] Tmin
 * @retval TON_MIN values
 **********************************************************/
inline uint8_t L6474_Tmin_Time_to_Par(double Tmin)
{
  return ((uint8_t)(((Tmin - 0.5)*2)+0.5));
}

/******************************************************//**
 * @brief  Write and receive a byte via SPI
 * @param[in] pByteToTransmit pointer to the byte to transmit
 * @param[in] pReceivedByte pointer to the received byte
 * @retval None
 **********************************************************/
void L6474_WriteBytes(uint8_t *pByteToTransmit, uint8_t *pReceivedByte)
{
  if (BSP_MotorControlBoard_SpiWriteBytes(pByteToTransmit, pReceivedByte, numberOfDevices) != 0)
  {
    L6474_ErrorHandler(L6474_ERROR_1);
  }
  
  if (isrFlag)
  {
    spiPreemtionByIsr = TRUE;
  }
}

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
