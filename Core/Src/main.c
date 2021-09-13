/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "stm32l152c_discovery.h"
#include "stm32l152c_discovery_glass_lcd.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc;

DAC_HandleTypeDef hdac;

LCD_HandleTypeDef hlcd;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

//Calculate the periods of the notes with 1/f, f being the frequency given in the guide :

#define DO 382
#define RE 341
#define MI 304
#define FA 286

#define CORRECTO 100
#define INCORRECTO 1000

unsigned short tiempo = 500; //The time we want each digit of the displayed sequence to last //500, meaning 0.5 seconds
unsigned short secuencia=0;//To control the ISR of TIM4
unsigned short seno[16] = {2048,2831,3495,3939,4095,3939,3495,2831,2048,1264,600,156,0,156,600,1264}; //The sine signal for the DAC
//The values of the sine array where from https://www.daycounter.com/Calculators/Sine-Generator-Calculator.phtml
int i = 0; //To go through the sine signal

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC_Init(void);
static void MX_LCD_Init(void);
static void MX_TS_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_DAC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void TIM4_IRQHandler(void) {
	if ((TIM4->SR & 0x0002)!=0){ // If the comparison is successful, then the IRQ is launched and this ISR is executed.
		secuencia++; // Increase in 1 the sequence
		TIM4->CCR1 += tiempo;	// Update the comparison value, adding 500 steps = 0.5 seconds
		TIM4->SR = 0x0000;	// Clear all flags
	}
}

void TIM3_IRQHandler(void){
  if((TIM3->SR & 0x0002 )!=0) // If the comparison is successful, then the IRQ is launched and this ISR is executed.
    {
      TIM3->SR=0; // Clear all flags
      if(i<16){ //We go over the sine array and get it through the DAC
        DAC->DHR12R2  = seno[i];
        i++;
      }
      if(i ==16) {
        i=0;
      }
    }
}

//this is a function to wait the given time, we use it to delay some operation in order to give the board time to "catch up"
//we also use it to display the win/wrong the time we want as well as the level we are in
void espera(int tiempo) {
	int i;
	for (i=0; i<tiempo; i++);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC_Init();
  MX_LCD_Init();
  MX_TS_Init();
  MX_TIM2_Init();
  MX_TIM4_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  MX_DAC_Init();
  /* USER CODE BEGIN 2 */

  //Variables

  unsigned short value; //Stores the value read from the potentiometer
  unsigned short level = 0; //Stores the level of difficulty we are playing depending on "value"
  unsigned short display[6]; //Stores the sequence we create using the random function. It's of length 6 because that's the longer sequence possible
  int num_display = 0; //Stores the number of time we've reproduced the sequence
  int num_items = 0; //Manages how many items of the sequence we reproduce each time, to reproduce the sequence gradually
  unsigned char secuencia_ant = 0; //Helps manage the timer 4
  unsigned short current_display; //Stores the item of "display" we are displaying currently
  unsigned short num_buttons_press = 0; //Stores how many buttons have been pressed
  int incorrect; //Stores if the player press the wrong button
  char USART_display[72] = "\rGAMES PLAYED: 00 | PLAYS WON: 00 | PLAYS LOSE: 00 | PLAYS ABANDONED: 00"; //To display the USAR info
  char abandon = '0';//Is set to '1' when a game is started and only change to '0' if the player loses or wins
  char game_ant; //Stores the value of each of the game variables before being update in order to be able to count up to 99 and not just 9, due to ASCII

  BSP_LCD_GLASS_Init();
  BSP_LCD_GLASS_BarLevelConfig(0);
  BSP_LCD_GLASS_Clear();

  // USER button configuration (PA0)
  // PA0 as digital input(00)
  GPIOA->MODER &= ~(1 << (0*2 +1));
  GPIOA->MODER &= ~(1 << (0*2));

  // button1 configuration (PA11)
  // PA11 as digital input(00) with pull-up
  GPIOA->MODER &= ~(1 << (11*2 +1));
  GPIOA->MODER &= ~(1 << (11*2));

  // button2 configuration (PA12)
  // PA12 as digital input(00) with pull-up
  GPIOA->MODER &= ~(1 << (12*2 +1));
  GPIOA->MODER &= ~(1 << (12*2));

  // button3 configuration (PB2)
  // PB2 as digital input(00) with pull-up
  GPIOB->MODER &= ~(1 << (2*2 +1));
  GPIOB->MODER &= ~(1 << (2*2));

  // button4 configuration (PH1)
  //PH1 as digital input(00) with pull-up
  GPIOB->MODER &= ~(1 << (1*2 +1));
  GPIOB->MODER &= ~(1 << (1*2));

  // ADC configuration
  GPIOA->MODER |= 0x00000300;	// PA4 as analog
  ADC1->CR2 &= ~(0x00000001);	// ADON = 0 (ADC powered off)
  ADC1->CR1 = 0x00000000;	// OVRIE = 0 (Overrun IRQ disabled)
  // RES = 00 (resolution = 12 bits)
  // SCAN = 0 (scan mode disabled)
  // EOCIE = 0 (EOC IRQ disabled)
  ADC1->CR2 = 0x00000400;	// EOCS = 1 (EOC to be activated after each conversion)
  // DELS = 000 (no delay)
  // CONT = 0 (single conversion)
  ADC1->SQR1 = 0x00000000;	// 1 channel in the sequence
  ADC1->SQR5 = 0x00000004;	// Channel is AIN4
  ADC1->CR2 |= 0x00000001;	// ADON = 1 (ADC powered on)

  // TIM4 configuration, this one is to display the sequence 0.5 seconds
  TIM4->CR1 = 0x0000; // ARPE = 0 -> No PWM, it is TOC
  // CEN = 0; Counter OFF
  TIM4->CR2 = 0x0000; // Always "0" in this course
  TIM4->SMCR = 0x0000; // Always "0" in this course
  // Counter behaviour setting: PSC, CNT, ARR and CCRx
  TIM4->PSC = 31999;	// Pre-scaler=32000 -> F_counter=32000000/32000 = 1000 steps/second
  TIM4->CNT = 0;	// Initialize the counter to 0
  TIM4->ARR = 0xFFFF; // Recommended value = FFFF
  TIM4->CCR1 = tiempo;	// Record that stores the value for the comparison.
  // It is initialized to the first value = 500 steps = 0.5 seconds
  // Selecting IRQ or not: DIER
  TIM4->DIER = 0x0002; // An IRQ is generated as the comparison is successful -> CCyIE = 1
  //Output
  TIM4->CCMR1 = 0x0000; // CCyS = 0	(TOC)
  // OCyM = 000 (no external output)
  // OCyPE = 0 (no pre-load)
  TIM4->CCER = 0x0000;	// CCyP = 0	(always in TOC)
  // CCyE = 0	(external output disabled)
  // Counter enabling
  TIM4->CR1 |= 0x0001;	// CEN = 1 -> Start counter
  TIM4->EGR |= 0x0001;	// UG = 1 -> Generate an update event to update all registers
  TIM4->SR = 0;	// Clear counter flags
  // Enabling TIM4_IRQ at NVIC (position 30).
  NVIC->ISER[0] |= (1 << 30);

  /*//Configuration of TIM2: SOUNDS WITHOUT DAC

  //TIM2 configuration
  GPIOA->MODER &= ~(0x00000400);	//PA5 set to 0
  GPIOA->AFR[0] |=0x00100000; //PA5 as TIM2 (AF1-0001)
  TIM2->CR1 = 0x0080;  // ARPE = 0 -> Is TOC
  // CEN = 0; Counter off
  TIM2->CR2 = 0x0000;
  TIM2->SMCR = 0x0000;
  TIM2->PSC = 319; // Pre-scalated= 320 (PSC = Pre-scalated - 1)
  //-> F_conunter = 32000000/320 = 100000 steps/seconds
  TIM2->CNT = 0; // Initialize the value of the counter to 0
  TIM2->ARR = 383;//FFFF, highest note DO =382
  TIM2->CCR1 = 191;// ARR/2
  TIM2->DIER = 0x0000;
  TIM2->CCMR1 = 0x0068; // CCyS = 0 (TOC, PWM)
  TIM2->CCER = 0x0001;
  TIM2->EGR |= 0x0001;
  TIM2->SR = 0; 		// Clear all flags
*/

  // DAC Configuration
  GPIOA->MODER |= 0x00000C00;       // PA5 as analogsignal
  DAC->CR = 0x00010000;             // Configuration and enabling of DAC number 2

  //This one is for the DAC signal
  TIM3->CR1 = 0x0000;  // ARPE = 0 -> Is TOC
  // CEN = 0; Counter off
  TIM3->CR2 = 0x0000;
  TIM3->SMCR = 0x0000;
  TIM3->PSC = 319; // Pre-scalated= 320 (PSC = Pre-scalated - 1)
  //-> F_conunter = 32000000/320 = 100000 steps/seconds
  TIM3->CNT = 0; // Initialize the value of the counter to 0
  TIM3->ARR = 0xFFFF;//FFFF
  TIM3->DIER = 0x0001;
  TIM3->CCMR1 = 0x0068; // CCyS = 0 (TOC, PWM)
  TIM3->CCER = 0x0001;
  TIM3->EGR |= 0x0001;
  TIM3->SR = 0;     // Clear all flags
  NVIC->ISER[0] |= (1<<29);


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1){

    //While the board is on, if we press any of the color buttons its sound is reproduced and its color is displayed in the LCD
    //Until we press the USER that starts the game
    //This is done in case the player wants to familiarize with the sounds/buttons

/*//To reproduce without DAC
	  if ((GPIOA->IDR&0x0800)==0) { // If PA11 = 0 (button1 pressed)
		  BSP_LCD_GLASS_Clear();
		  BSP_LCD_GLASS_DisplayString((uint8_t *)" WHITE");
		  TIM2->ARR = MI;
		  TIM2->CCR1 = MI/2;
		  TIM2->CR1 |= 0x0001;
		  while ((GPIOA->IDR&0x0800)==0) { }
		  TIM2->SR = 0; // Clear all flags
		  TIM2->CR1 &=~ (0x0001);// CEN = 0 -> Deactivate the counter
		  TIM2->CNT = 0; // Set the value of the counter to 0
		  BSP_LCD_GLASS_Clear();
	  }
	  if ((GPIOA->IDR&0x1000)==0) { // If PA12 = 0 (button2 pressed)
		  BSP_LCD_GLASS_Clear();
		  BSP_LCD_GLASS_DisplayString((uint8_t *)" GREEN");
		  TIM2->ARR = FA;
		  TIM2->CCR1 = FA/2;
		  TIM2->CR1 |= 0x0001;
		  while ((GPIOA->IDR&0x1000)==0) { }
		  TIM2->SR = 0; // Clear all flags
		  TIM2->CR1 &=~ (0x0001); // CEN = 0 -> Deactivate the counter
		  TIM2->CNT = 0; // Set the value of the counter to 0
		  BSP_LCD_GLASS_Clear();
	 }
	 if ((GPIOB->IDR&0x0004)==0) { // If PB2 = 0 (button3 pressed)
	    BSP_LCD_GLASS_Clear();
	    BSP_LCD_GLASS_DisplayString((uint8_t *)" BLUE");
	    TIM2->ARR = RE;
	    TIM2->CCR1 = RE/2;
	    TIM2->CR1 |= 0x0001;
	    while ((GPIOB->IDR&0x0004)==0) { }
	    TIM2->SR = 0; // Clear all flags
	    TIM2->CR1 &=~ (0x0001);// CEN = 0 -> Deactivate the counter
	    TIM2->CNT = 0; // Set the value of the counter to 0
	    BSP_LCD_GLASS_Clear();
	 }
	 if ((GPIOH->IDR&0x0002)==0) { // If PH1 = 0 (button4 pressed)
	    BSP_LCD_GLASS_Clear();
	    BSP_LCD_GLASS_DisplayString((uint8_t *)" RED");
	    TIM2->ARR = DO;
	    TIM2->CCR1 = DO/2;
	    TIM2->CR1 |= 0x0001;
	    while ((GPIOH->IDR&0x0002)==0) { }
	    TIM2->SR = 0; // Clear all flags
	    TIM2->CR1 &=~ (0x0001);// CEN = 0 -> Deactivate the counter
	    TIM2->CNT = 0; // Set the value of the counter to 0
	    BSP_LCD_GLASS_Clear();
	 }
*/

    if ((GPIOA->IDR&0x0800)==0) { // If PA11 = 0 (button1 pressed)
      BSP_LCD_GLASS_Clear();
      BSP_LCD_GLASS_DisplayString((uint8_t *)" WHITE");
      TIM3->ARR = MI/16;
      TIM3->CR1 |= 0x0001;
      TIM3->SR=1;
      while ((GPIOA->IDR&0x0800)==0) { }
      TIM3->SR = 0; // Clear all flags
      TIM3->CR1 &=~ (0x0001);// CEN = 0 -> Deactivate the counter
      TIM3->CNT = 0; // Set the value of the counter to 0
      BSP_LCD_GLASS_Clear();
    }
    if ((GPIOA->IDR&0x1000)==0) { // If PA12 = 0 (button2 pressed)
      BSP_LCD_GLASS_Clear();
      BSP_LCD_GLASS_DisplayString((uint8_t *)" GREEN");
      TIM3->ARR = FA/16;
      TIM3->CR1 |= 0x0001;
      TIM3->SR=1;
      while ((GPIOA->IDR&0x1000)==0) { }
      TIM3->SR = 0; // Clear all flags
      TIM3->CR1 &=~ (0x0001); // CEN = 0 -> Deactivate the counter
      TIM3->CNT = 0; // Set the value of the counter to 0
      BSP_LCD_GLASS_Clear();
   }
   if ((GPIOB->IDR&0x0004)==0) { // If PB2 = 0 (button3 pressed)
      BSP_LCD_GLASS_Clear();
      BSP_LCD_GLASS_DisplayString((uint8_t *)" BLUE");
      TIM3->ARR = RE/16;
      TIM3->CR1 |= 0x0001;
      TIM3->SR=1;
      while ((GPIOB->IDR&0x0004)==0) { }
      TIM3->SR = 0; // Clear all flags
      TIM3->CR1 &=~ (0x0001);// CEN = 0 -> Deactivate the counter
      TIM3->CNT = 0; // Set the value of the counter to 0
      BSP_LCD_GLASS_Clear();
   }
   if ((GPIOH->IDR&0x0002)==0) { // If PH1 = 0 (button4 pressed)
      BSP_LCD_GLASS_Clear();
      BSP_LCD_GLASS_DisplayString((uint8_t *)" RED");
      TIM3->ARR = DO/16;
      TIM3->CR1 |= 0x0001;
      TIM3->SR=1;
      while ((GPIOH->IDR&0x0002)==0) { }
      TIM3->SR = 0; // Clear all flags
      TIM3->CR1 &=~ (0x0001);// CEN = 0 -> Deactivate the counter
      TIM3->CNT = 0; // Set the value of the counter to 0
      BSP_LCD_GLASS_Clear();
   }

	 //Once we press the USER button, the game really starts

	 if ((GPIOA->IDR&0x00000001)!=0) { // If PA0 = 1 (USER pressed)
	   //First of all, we configure the USART transmission. The maximum number it stores is 99
	   //We update the number of times the player has played, with an if-else sentence to be able to get numbers up to 99
	   game_ant = USART_display[16];
	   if(game_ant == '9'){
	     USART_display[15]++;
	     USART_display[16]='0';
	   }
	   else{
	     USART_display[16]++;
	   }
	   //We update the number of times the player has played
	   game_ant = USART_display[71];
	   if(abandon == '1'){ //If abandon didn't change from '1', it means the player didn't finish the game
	     if(game_ant == '9'){
	       USART_display[70]++;
	       USART_display[71]='0';
	     }
	     else{
	       USART_display[71]++;
	     }
	   }
	   HAL_UART_Transmit(&huart1, (uint8_t*)USART_display, 72, 10000);//We transmit the updated info

	   //Then, we read from the value of the potentiometer and store it in the variable value
	    while ((GPIOA->IDR&0x00000001)!=0) { // If PA0 = 1 (USER pressed),
	     //We wait to avoid rebounds
	     espera(70000);
	    }
		  // Start conversion
		  while ((ADC1->SR&0x0040)==0); // While ADONS = 0, i.e., ADC is not ready
		  // to convert, I wait
		  ADC1->CR2 |= 0x40000000; // When ADONS = 1, I start conversion (SWSTART = 1)
		  // Wait till conversion is finished
		  while ((ADC1->SR&0x0002)==0); // If EOC = 0, i.e., the conversion is not finished, I wait
		  value = ADC1->DR; // When EOC = 1, I take the result and store it in a variable called value

		  //Depending on the value of said variable, the player will chose the level (that varies the sequence duration)
		  if (value>3069 && value<=4100){ //If it has a value between 4100 (its maximum more or less, as the potentiometer value are very unstable we rounded the number to avoid errors) and 3070,
		                                  //the chosen level is 1, meaning the sequence is 3 items long
			 level = 3;
			 BSP_LCD_GLASS_Clear();
			 BSP_LCD_GLASS_DisplayString((uint8_t *)" LV 1"); //The LCD displays the level for 0.5 seconds
			 espera(2500000);
			 BSP_LCD_GLASS_Clear();
		  }
		  if (value>2046 && value<=3069){ //If it has a value between 3069 and 2047 the chosen level is 2, meaning the sequence will be 4 items long
		   level = 4;
		   BSP_LCD_GLASS_Clear();
			 BSP_LCD_GLASS_DisplayString((uint8_t *)" LV 2");
			 espera(2500000);
			 BSP_LCD_GLASS_Clear();
		  }
		  if (value>1023 && value<=2046){ //If it has a value between 2046 and 1024 the chosen level is 3, meaning the sequence will be 5 items long
		   level = 5;
		   BSP_LCD_GLASS_Clear();
			 BSP_LCD_GLASS_DisplayString((uint8_t *)" LV 3");
			 espera(2500000);
			 BSP_LCD_GLASS_Clear();
		  }
		  if (value>=0000 && value<=1023){ //If it has a value between 1024 and 0 the chosen level is 4, meaning the sequence will be 6 items long
			 level = 6;
			 BSP_LCD_GLASS_Clear();
			 BSP_LCD_GLASS_DisplayString((uint8_t *)" LV 4");
			 espera(2500000);
			 BSP_LCD_GLASS_Clear();
		  }

		  //Each time USER is pressed we re-initialize the variables that manage the game:
		  num_display = 0;
		  num_items = 0;
		  incorrect = 0;
		  abandon = '1';
	  }

	  //Generate random numbers between 0 and 3, to store in an array
	  //The array will be as long as the sequence we chose (read from the potentiometer), although it's default value is 6 (the longest possible sequence)
	  for(int j=0; j<level;j++){
		  unsigned short sequence = rand() % 4;
		  display[j]=sequence;
	  }

	  //Now this is the code of the game itself after we've selected and displayed the value

	  while(num_display<level && (GPIOA->IDR&0x00000001)==0){ //This is executed until the whole sequence has been reproduced, or until we press the user button to re-initialize the game
		  while(num_items<num_display+1){ //With this while we reproduce the sequence gradually, first the item 1, and i we press the correct button then item 1 + the next, if we press the correct buttons,
		                          //the previous items plus one more are reproduced, and so on until we press a wrong button or the sequence is complete
			  if (secuencia_ant != secuencia) {	// If the ISR has changed the sequence
				  secuencia_ant = secuencia;	// Store new sequence to be used next time
				  current_display=display[num_items]; //We store in current display the item we want to reproduce and we go upgrading it each time the button(s) are pressed correctly
				  num_buttons_press = 0; //We re-set the number of buttons pressed to 0
				  //Now for each of the random values we reproduce it's sound and display it's color
				  if(current_display == 0){
					  TIM3->ARR = MI/16;
					  TIM3->CR1 |= 0x0001;
					  TIM3->SR=1;
					  BSP_LCD_GLASS_Clear();
					  BSP_LCD_GLASS_DisplayString((uint8_t *)" WHITE");
				  }
				  if(current_display == 1){
					  TIM3->ARR = FA/16;
					  TIM3->CR1 |= 0x0001;
					  TIM3->SR=1;
					  BSP_LCD_GLASS_Clear();
					  BSP_LCD_GLASS_DisplayString((uint8_t *)" GREEN");
				  }
				  if(current_display == 2){
					  TIM3->ARR = RE/16;
					  TIM3->CR1 |= 0x0001;
					  TIM3->SR=1;
					  BSP_LCD_GLASS_Clear();
					  BSP_LCD_GLASS_DisplayString((uint8_t *)" BLUE");
				  }
				  if(current_display == 3){
					  TIM3->ARR = DO/16;
					  TIM3->CR1 |= 0x0001;
					  TIM3->SR=1;
					  BSP_LCD_GLASS_Clear();
					  BSP_LCD_GLASS_DisplayString((uint8_t *)" RED");
				  }
				  TIM3->SR = 0;// Clear all flags
				  TIM3->CR1 &=~ (0x0001);// CEN = 0 -> Deactivate the counter
				  TIM3->CNT = 0; // Set the value of the counter to 0
				  if (num_items==num_display){//To clean the LCD once we've displayed the last digit of the sequence
					  espera(1250000); //We wait (only 0.25s) because if not the last digit isn't displayed quite well
					  BSP_LCD_GLASS_Clear();
				  }
				  num_items++;
			  }//End of if that manages the timer
		  }//End of while that reproduces gradually the sequence

		  //Now, for each button pressed we display its sound and color, for as long as the button is pressed
		  //We also upgrade the number of buttons pressed each time one is
		  //We check if with the array that stores the sequence if the button pressed is correct, if it is not we upgrade the "incorrect" variable

		  if ((GPIOA->IDR&0x0800)==0) { // If PA11 = 0 (button1 pressed)
			  BSP_LCD_GLASS_Clear();
			  BSP_LCD_GLASS_DisplayString((uint8_t *)" WHITE");
			  TIM3->ARR = MI/16;
			  TIM3->CR1 |= 0x0001;
			  TIM3->SR=1;
			  while ((GPIOA->IDR&0x0800)==0) { }
			  TIM3->SR = 0; // Clear all flags
			  TIM3->CR1 &=~ (0x0001);// CEN = 0 -> Deactivate the counter
			  TIM3->CNT = 0; // Set the value of the counter to 0
			  BSP_LCD_GLASS_Clear();
			  num_buttons_press += 1;
		  	  if (0 != display[num_buttons_press-1]){
		  		  incorrect += 1;
		  	  }
		  }
		  if ((GPIOA->IDR&0x1000)==0) { // If PA12 = 0 (button2 pressed)
			  BSP_LCD_GLASS_Clear();
			  BSP_LCD_GLASS_DisplayString((uint8_t *)" GREEN");
			  TIM3->ARR = FA/16;
			  TIM3->CR1 |= 0x0001;
			  TIM3->SR=1;
			  while ((GPIOA->IDR&0x1000)==0) { }
			  TIM3->SR = 0; // Clear all flags
			  TIM3->CR1 &=~ (0x0001);// CEN = 0 -> Deactivate the counter
			  TIM3->CNT = 0; // Set the value of the counter to 0
			  BSP_LCD_GLASS_Clear();
			  num_buttons_press += 1;
		  	  if (1 != display[num_buttons_press-1]){
		  		  incorrect += 1;
		  	  }
		  }
		  if ((GPIOB->IDR&0x0004)==0) { // If PB2 = 0 (button3 pressed)
		    BSP_LCD_GLASS_Clear();
		    BSP_LCD_GLASS_DisplayString((uint8_t *)" BLUE");
		    TIM3->ARR = RE/16;
		    TIM3->CR1 |= 0x0001;
		    TIM3->SR=1;
		    while ((GPIOB->IDR&0x0004)==0) { }
			  TIM3->SR = 0; // Clear all flags
			  TIM3->CR1 &=~ (0x0001);// CEN = 0 -> Deactivate the counter
			  TIM3->CNT = 0; // Set the value of the counter to 0
			  BSP_LCD_GLASS_Clear();
			  num_buttons_press += 1;
			  if (2 != display[num_buttons_press-1]){
			    incorrect += 1;
			  }
		  }
		  if ((GPIOH->IDR&0x0002)==0) { // If PH1 = 0 (button4 pressed)
		  	 BSP_LCD_GLASS_Clear();
		  	 BSP_LCD_GLASS_DisplayString((uint8_t *)" RED");
		  	 TIM3->ARR = DO/16;
		  	 TIM3->CR1 |= 0x0001;
		  	 TIM3->SR=1;
		  	 while ((GPIOH->IDR&0x0002)==0) { }
		  	 TIM3->SR = 0; // Clear all flags
		  	 TIM3->CR1 &=~ (0x0001);// CEN = 0 -> Deactivate the counter
		  	 TIM3->CNT = 0; // Set the value of the counter to 0
		  	 BSP_LCD_GLASS_Clear();
		  	 num_buttons_press += 1;
		  	if (3 != display[num_buttons_press-1]){
		  		incorrect += 1;
		  	}
		  }

		  //If the incorrect variable is different from 0, it means that the player pressed a wrong button, and so immediately the word "WRONG" will be displayed along with a sound, for 0.5 seconds
		  //and the game will finish until we press USER again to start a new one
		  //we also change "abandon" to '0', meaning we finished the game, we also increment "lose"

		  if (incorrect != 0){
		    BSP_LCD_GLASS_Clear();
			  BSP_LCD_GLASS_DisplayString((uint8_t *)" WRONG");
			  TIM3->ARR = INCORRECTO/16;
			  TIM3->CR1 |= 0x0001;
			  TIM3->SR=1;
		 	  espera(2500000);
		 	  TIM3->SR = 0; // Clear all flags
			  TIM3->CR1 &=~ (0x0001);// CEN = 0 -> Deactivate the counter
			  TIM3->CNT = 0; // Set the value of the counter to 0
			  BSP_LCD_GLASS_Clear();
			  num_display = level;
			  abandon = '0';
			  //We update the number of times the player has lose, we use an if-else to be able to count up to 99
			  game_ant = USART_display[49];
			  if(game_ant == '9'){
			    USART_display[48]++;
			    USART_display[49]='0';
			  }
			  else{
			    USART_display[49]++;
			  }
			  HAL_UART_Transmit(&huart1, (uint8_t*)USART_display, 62, 10000);//We transmit the updated info
		  }

		  //If the number of buttons pressed is the same as the number of displayed items, and the "incorrect" variable is 0, meaning all the buttons pressed were right
		  //we re-set to 0 the variable "t" that manages the while that reproduce the sequence gradually, we increment the number of displays we've made, we re-set the clock
		  //We way 0.2 seconds in order to be clear for the player that we are re-reproducing the sequence + 1 item (or that the player has win)
		  //We've added this "espera" because, if not, the next display will be too close to the release of the button and it wasn't very user-friendly

		  if (num_buttons_press == num_display+1 && num_buttons_press!=0){
			  if(incorrect == 0){
				  num_items=0;
				  num_display+=1;
				  secuencia = 0;
				  espera(1000000);

				  //If have displayed the whole sequence and there is no errors, the word "WIN" is displayed along with a sound for 0.5 seconds
				  //and the game finishes until USER button is pressed to start another one
				  //we also change "abandon" to '0', meaning we finished the game, we also increment "win"

				  if (num_display == level){

					  TIM3->ARR = CORRECTO/16;
					  TIM3->CR1 |= 0x0001;
					  TIM3->SR=1;
					  BSP_LCD_GLASS_Clear();
					  BSP_LCD_GLASS_DisplayString((uint8_t *)" WIN");
					  espera(1000000);
					  TIM3->SR = 0; // Clear all flags
					  TIM3->CR1 &=~ (0x0001);// CEN = 0 -> Deactivate the counter
					  TIM3->CNT = 0; // Set the value of the counter to 0
					  espera(1000000);
					  TIM3->ARR = CORRECTO/16;
					  TIM3->CR1 |= 0x0001;
					  TIM3->SR=1;
					  espera(1000000);
					  TIM3->SR = 0; // Clear all flags
					  TIM3->CR1 &=~ (0x0001);// CEN = 0 -> Deactivate the counter
					  TIM3->CNT = 0; // Set the value of the counter to 0
					  BSP_LCD_GLASS_Clear();
					  abandon = '0';
					  //We update the number of times the player has win and use an if-else to be able to count up to 99
				     game_ant = USART_display[32];
				     if(game_ant == '9'){
				       USART_display[31]++;
				       USART_display[32]='0';
				     }
				     else{
				       USART_display[32]++;
				     }
					  HAL_UART_Transmit(&huart1, (uint8_t*)USART_display, 62, 10000);//We transmit the updated info
				  }
			  }
		  }//close if to check the pressed sequence
	  }//close game function while

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLL_DIV3;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LCD;
  PeriphClkInit.LCDClockSelection = RCC_RTCCLKSOURCE_LSE;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC_Init(void)
{

  /* USER CODE BEGIN ADC_Init 0 */

  /* USER CODE END ADC_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC_Init 1 */

  /* USER CODE END ADC_Init 1 */
  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc.Instance = ADC1;
  hadc.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc.Init.Resolution = ADC_RESOLUTION_12B;
  hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc.Init.LowPowerAutoWait = ADC_AUTOWAIT_DISABLE;
  hadc.Init.LowPowerAutoPowerOff = ADC_AUTOPOWEROFF_DISABLE;
  hadc.Init.ChannelsBank = ADC_CHANNELS_BANK_A;
  hadc.Init.ContinuousConvMode = DISABLE;
  hadc.Init.NbrOfConversion = 1;
  hadc.Init.DiscontinuousConvMode = DISABLE;
  hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc.Init.DMAContinuousRequests = DISABLE;
  if (HAL_ADC_Init(&hadc) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_4CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC_Init 2 */

  /* USER CODE END ADC_Init 2 */

}

/**
  * @brief DAC Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC_Init(void)
{

  /* USER CODE BEGIN DAC_Init 0 */

  /* USER CODE END DAC_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC_Init 1 */

  /* USER CODE END DAC_Init 1 */
  /** DAC Initialization
  */
  hdac.Instance = DAC;
  if (HAL_DAC_Init(&hdac) != HAL_OK)
  {
    Error_Handler();
  }
  /** DAC channel OUT2 config
  */
  sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC_Init 2 */

  /* USER CODE END DAC_Init 2 */

}

/**
  * @brief LCD Initialization Function
  * @param None
  * @retval None
  */
static void MX_LCD_Init(void)
{

  /* USER CODE BEGIN LCD_Init 0 */

  /* USER CODE END LCD_Init 0 */

  /* USER CODE BEGIN LCD_Init 1 */

  /* USER CODE END LCD_Init 1 */
  hlcd.Instance = LCD;
  hlcd.Init.Prescaler = LCD_PRESCALER_1;
  hlcd.Init.Divider = LCD_DIVIDER_16;
  hlcd.Init.Duty = LCD_DUTY_1_4;
  hlcd.Init.Bias = LCD_BIAS_1_4;
  hlcd.Init.VoltageSource = LCD_VOLTAGESOURCE_INTERNAL;
  hlcd.Init.Contrast = LCD_CONTRASTLEVEL_0;
  hlcd.Init.DeadTime = LCD_DEADTIME_0;
  hlcd.Init.PulseOnDuration = LCD_PULSEONDURATION_0;
  hlcd.Init.MuxSegment = LCD_MUXSEGMENT_DISABLE;
  hlcd.Init.BlinkMode = LCD_BLINKMODE_OFF;
  hlcd.Init.BlinkFrequency = LCD_BLINKFREQUENCY_DIV8;
  if (HAL_LCD_Init(&hlcd) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LCD_Init 2 */

  /* USER CODE END LCD_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}

/**
  * @brief TS Initialization Function
  * @param None
  * @retval None
  */
static void MX_TS_Init(void)
{

  /* USER CODE BEGIN TS_Init 0 */

  /* USER CODE END TS_Init 0 */

  /* USER CODE BEGIN TS_Init 1 */

  /* USER CODE END TS_Init 1 */
  /* USER CODE BEGIN TS_Init 2 */

  /* USER CODE END TS_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin : PH1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PB2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PA11 PA12 */
  GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
