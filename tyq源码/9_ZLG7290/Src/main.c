/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  *
  * COPYRIGHT(c) 2015 STMicroelectronics
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
#include "stm32f4xx_hal.h"
#include "stm32f4xx_it.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"
#include "time.h"
/* USER CODE BEGIN Includes */
#include "zlg7290.h"
#include "stdio.h"
#include "stdlib.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
#define ZLG_READ_ADDRESS1         0x01
#define ZLG_READ_ADDRESS2         0x10
#define ZLG_WRITE_ADDRESS1        0x10
#define ZLG_WRITE_ADDRESS2        0x11
//#define BUFFER_SIZE1              (countof(Tx1_Buffer))
#define BUFFER_SIZE2              (countof(Rx2_Buffer))
#define countof(a) (sizeof(a) / sizeof(*(a)))

#define Num_Backup 3 //定义备份的数量
#define Clock_Max 0xffffffffffffffff
#define ARG_FADE_FLAG 1
#define ARG_RX2_BUFFER 2
#define ARG_READ_IN 3

#define Key 5


//uint8_t flag;//?????????????
uint8_t flag1 = 0;//?????,??????????,?????8??????
uint8_t Rx2_Buffer[8]={0};
uint8_t Rx2_Buffer_print[8]={0};
uint8_t All_Zero[8] = {0};
uint8_t Init_times = 0;

uint8_t fade_flag = 0;

uint8_t ve_Rx2; // 校验
uint8_t ve_Read;
uint8_t ve_fade;

uint8_t done_list = 0;
clock_t start, ct_time; //
double pass_time;
double pass_init_time;
clock_t last_init;

clock_t last_refresh;
uint8_t im_refresh;


uint8_t m_index;
uint32_t music[] = {261, 277, 293, 311, 329, 349, 370, 392, 415, 440, 466, 493, 523, 554, 587, 622};

uint32_t Max_delay = 1000; // ms
uint32_t Sequance = 0;

uint8_t Read_in = 0;
uint8_t Read_in_Check = 0;
uint8_t read_ok = 0;
uint8_t read_time = 0;
uint8_t ve_count = 0;

//备份结构体
typedef struct backup {
	uint8_t Rx2_Buffer[8];
	uint8_t Read_in;
	uint8_t fade_flag;
	uint8_t v_Rx2; // 校验
	uint8_t v_Read;
	uint8_t v_fade;
}Backup;

//备份结构体指针
typedef struct Node {
	Backup backup;
	struct Node* next;
}Node;

Node* Leader; // 备份链表的头结点

//uint8_t Rx1_Buffer[1]={0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/
void swtich_key(void);
void switch_flag(void);
void headLoop(void);
void bottomLoop(void);
void showScreen(void);
uint8_t* getvalue(uint8_t opt);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */


//初始化备份节点
int init_Backup() {
	int i,j;
	Leader = (Node*)malloc(sizeof(Node));
	Node* temp = Leader;
	Node* New;
	Node* wait_free[15];
	Leader->next = NULL;
	for (i = 0; i < Num_Backup - 1; i++) {
		//转变位置
		for (j = 0; j < 5; j++) {
			wait_free[i*5+j] = (Node*)malloc(sizeof(Node));
			if (wait_free[i*5+j] == NULL) {
				printf("[SysInfo] Malloc Error\n");
				return -1;
			}
		}
		New = (Node*)malloc(sizeof(Node));
		if (New == NULL) {
				printf("[SysInfo] Malloc Error\n");
				return -1;
		}
		temp->next = New;
		temp = New;
	}
	for(i = 0; i < 10;i++){
		free(wait_free[i]);
	}
	return 0;
}

//计算校验位
void compute_vbit(Backup* backup) {
	int8_t count = 0;
	int i;
	for (i = 0; i < 8; i++) {
		count += backup->Rx2_Buffer[i];
	}
	backup->v_Rx2 = count;
	backup->v_fade = backup->fade_flag;
	backup->v_Read = backup->Read_in;
}
void computer_vRx2(uint8_t Rx2_Buffer[]){
	int8_t count = 0;
	int i;
	for (i = 0; i < 8; i++) {
		count += Rx2_Buffer[i];
	}
	ve_Rx2 = count;
}
int verify_all(Backup* backup){
	int8_t count = 0;
	int i;
	for (i = 0; i < 8; i++) {
		count += backup->Rx2_Buffer[i];
	}
	if((backup->v_Rx2 == count) && (backup->v_fade == backup->fade_flag) && (backup->Read_in ==backup->v_Read))
		return 1;
	else
		return 0;
}
//验证校验位是否正确，返回1表示验证正确，返回0即错误
int verify_Rx2(uint8_t Rx2_Buffer[]) {
	ve_count = 0;
	for (int i = 0; i < 8; i++) {
		ve_count += Rx2_Buffer[i];
	}
	return ve_Rx2 == ve_count;
}

int verify_Read_in(uint8_t Read_in){
	return Read_in == ve_Read;
}
//更新备份
void update() {
	Node* temp = Leader;
	int i;
	while (temp != NULL) {
		for (i = 0; i < 8; i++) {
			temp->backup.Rx2_Buffer[i] = Rx2_Buffer[i];
		}
		temp->backup.fade_flag = fade_flag;
		temp->backup.Read_in = Read_in;
		temp = temp->next;
	}
}

void update_cer(uint8_t file){
	Node* temp = Leader;
	int i;
	while (temp != NULL) {
		switch(file){
			case ARG_FADE_FLAG:temp->backup.fade_flag = fade_flag;break;
			case ARG_RX2_BUFFER: {
				for (i = 0; i < 8; i++) {
					temp->backup.Rx2_Buffer[i] = Rx2_Buffer[i];
				}
			}break;
			case ARG_READ_IN:temp->backup.Read_in = Read_in;break;
		}
		temp = temp->next;
	}
}
//每次正确读之后写其它备份节点
void write(Backup *std) {
	Node* temp = Leader;
	while (temp != NULL) {
		temp->backup = *std;
		temp = temp->next;
	}
}
//从备份节点中读
void read() {
	Node* temp = Leader;
	int i;
	while (temp!= NULL) {
		if (verify_all(&temp->backup)) {
			for (i = 0; i < 8; i++) {
				Rx2_Buffer[i] = temp->backup.Rx2_Buffer[i];
			}
			fade_flag = temp->backup.fade_flag;
			Read_in = temp->backup.Read_in;
			write(&temp->backup); //将其他备份修改为此备份
			break;
		}	
		temp = temp->next;
	}
}

void get_Cer(int cer){
	Node* temp = Leader;
	int i;
	while (temp!= NULL) {
		if (verify_all(&temp->backup)) {
			switch(cer){
				case ARG_FADE_FLAG:
					fade_flag = temp->backup.fade_flag;
					break;
				case ARG_RX2_BUFFER: 
					for (i = 0; i < 8; i++) {
						Rx2_Buffer[i] = temp->backup.Rx2_Buffer[i];
					}
					
					break;
				case ARG_READ_IN:
					Read_in = temp->backup.Read_in;
					break;
			}
			break;
		}	
		temp = temp->next;
	}
}

void time_delay() {
	if (Max_delay) {
		uint32_t rand_delay = rand() % Max_delay;
		HAL_Delay(rand_delay);
		Max_delay -= rand_delay;
	}
}

void Mealy(int i){
	Sequance = 0;
	done_list = 0;
	switch(i){
		case 1:headLoop();bottomLoop(); done_list = 1;break;		//开始 
		case 2:bottomLoop();headLoop(); done_list = 1;break;		//取数据+显示数据 
	}
	if(done_list)
		showScreen();
	else
		printf("[SysErr] Out Of Order!\n");
} 
/*
int Mealy(int i){
	switch(i){
		case 1:bottomLoop();				//开始 
		case 2:headLoop();break;		//取数据+显示数据 
	}
}
*/
int statusJustice(){
	if((pass_time < 3 && *getvalue(ARG_FADE_FLAG) != 0)){
		return 0;
	}
	return 1;				//0 异常 1 正常 
}

uint8_t* getvalue(uint8_t opt){
	switch(opt){
		//fade_flag
		case ARG_FADE_FLAG:
			if(fade_flag != ve_fade){
				get_Cer(ARG_FADE_FLAG);
				update_cer(ARG_FADE_FLAG);
				ve_fade = fade_flag;
			}
			return &fade_flag;
				
		//Rx2_Buffer
		case ARG_RX2_BUFFER:
			if(!verify_Rx2(Rx2_Buffer))
				get_Cer(ARG_RX2_BUFFER);
				update_cer(ARG_RX2_BUFFER);
				computer_vRx2(Rx2_Buffer);
			return Rx2_Buffer;
		//Read_in
		case ARG_READ_IN:
			if(Read_in != ve_Read){
				get_Cer(ARG_READ_IN);
				update_cer(ARG_READ_IN);
				ve_Read = Read_in;
			}
			return &Read_in;
	}
	return NULL;
}


void reInitData(){
	/* USER CODE BEGIN 1 */


  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init(); // 使用之前初始化 周期性刷新
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  last_init = HAL_GetTick();
  /* USER CODE BEGIN 2 */
	/*
	printf("\n\r");
	printf("\n\r-------------------------------------------------\r\n");
	printf("\n\r FS-STM32??? ?????????\r\n");	
  */
	
	/* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
}

void headLoop(){
	srand(HAL_GetTick());
	Max_delay = 1;
	Sequance = 0;
	ct_time = HAL_GetTick();


	pass_time = (double)(ct_time - start) / 1000;
	if(pass_time > 3 && *getvalue(ARG_FADE_FLAG) == 0){
		fade_flag = 1;
		//输出休眠
		printf("[SysInfo] sleep\n");
		//更新备份
		update();
	}
	
	// 防止计时器翻转，定义Clock_Max
	pass_init_time = (double)(ct_time - last_init);
	if (pass_init_time < 0) {
		pass_init_time += (double)Clock_Max;
	}
	if(pass_init_time > 11451 && flag1 != 1){
		reInitData();
	}
	
	if(!statusJustice()){
			//恢复备份s
		read();
	}
}
void bottomLoop(){
	//Debug
	
	if(flag1 == 1) // 超时初始化处理
	{
			flag1 = 0;
			m_index = 100; //init
			start = HAL_GetTick();
			if(*getvalue(ARG_FADE_FLAG) > 0) {
				I2C_ZLG7290_Write(&hi2c1,0x70,ZLG_WRITE_ADDRESS1,getvalue(ARG_RX2_BUFFER),8);
				fade_flag = 0;
				ve_fade =fade_flag;
				update();
				//输出唤醒
				printf("[SysInfo] wake up\n");
				return ;
			}
		
			// I2C_ZLG7290_Read(&hi2c1,0x71,0x01,Rx2_Buffer + 7,1);// 不读两次处理 solved
			
			uint8_t *p1 = &Read_in;
			uint8_t *p2 = &Read_in_Check;
			read_ok = 0;
			read_time = 0;
			int rtn = 0, rtn_check = 0;
			while(!read_ok){
				rtn = I2C_ZLG7290_Read(&hi2c1,0x71,0x01,p1,1);
				rtn_check = I2C_ZLG7290_Read(&hi2c1,0x71,0x01,p2,1);
				if (rtn == -1 || rtn_check == -1) {
					break;
				}
				if (Read_in == Read_in_Check) {
					read_ok = 1;
					Read_in ^= Key;
				}
				if (++read_time > 10) {
					break;
				}
			}
			if (!read_ok) {
				reInitData();
				printf("[SysInfo] Please retype a letter\n");
				return;
			}
			
			// uint8_t *p = &Read_in;
			// I2C_ZLG7290_Read(&hi2c1,0x71,0x01,p,1);
			
			ve_Read = Read_in; // 更新read_in校验值
			uint8_t *p = getvalue(ARG_RX2_BUFFER); // 更新Rx2_Buffer校验值
			for (int i = 7; i > 0; i--) {
				Rx2_Buffer[i] = Rx2_Buffer[i-1];
			}
			//更新校验，延时，更新备份
			computer_vRx2(Rx2_Buffer);
			time_delay();
			update();
	
		Sequance++;
		if (Sequance != 1) {
			printf("Out of order");
			return;
		}
//	I2C_ZLG7290_Read(&hi2c1,0x71,0x10,Rx2_Buffer,7);//?8????
		
		switch_flag();
		
		MX_GPIO_Init(); // 更新蜂鸣器引脚
		HAL_Delay_ms(1); // 同步时钟节拍
		//不同按键伴随不同音调的声音
		for(int i = 0;i<28;i++){
			HAL_GPIO_WritePin(GPIOG,GPIO_PIN_6,GPIO_PIN_SET);//打开蜂鸣器
			HAL_Delay_ms(500000/(music[m_index - 1]));//
			HAL_GPIO_WritePin(GPIOG,GPIO_PIN_6,GPIO_PIN_RESET);//关闭蜂鸣器
			HAL_Delay_ms(500000/(music[m_index - 1]));//
		}
	}
}

void showScreen(){
	if(*getvalue(ARG_FADE_FLAG) == 0) { // 工作状态
		pass_init_time = (double)(ct_time - last_init);
		double pass_refresh_time = HAL_GetTick() - last_refresh;
		if (pass_refresh_time < 0) {
			pass_refresh_time += (double)Clock_Max;
		}
		
		if(im_refresh == 1 || pass_refresh_time > 1000){
			I2C_ZLG7290_Write(&hi2c1,0x70,ZLG_WRITE_ADDRESS1,Rx2_Buffer,BUFFER_SIZE2);
			time_delay();
			im_refresh = 0;
			last_refresh = HAL_GetTick();
		}
	} else { // 休眠状态
		for(int i = 0; i < 8; i++){
			All_Zero[i] = 0; // 清屏
		}
		I2C_ZLG7290_Write(&hi2c1,0x70,ZLG_WRITE_ADDRESS1,All_Zero,BUFFER_SIZE2);
		time_delay();
		im_refresh = 0;
		last_refresh = HAL_GetTick();
	}
}

int main(void)
{
  reInitData();
	Init_times = 3;
	while(init_Backup() != 0) {
		Init_times--;
		if (Init_times <= 0) {
			printf("[SysErr]: Init Backup Error. Programe Exit!\n");
			return -1;
		}
	}
  start = HAL_GetTick();
	last_refresh = HAL_GetTick();
  while (1)
  {
		int temp = rand() % 2 + 1;			
		Mealy(temp);	//状态开始 
		
  }
  /* USER CODE END 3 */

}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

  __PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000000); //modify

  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

void switch_flag(void){
	//校验Rx2_Buffer[0]
	Sequance++;
	if (Sequance != 2) {
		printf("Out of order");
		return;
	}
	switch(*getvalue(ARG_READ_IN) ^ Key){
			case 0x1C:
				Rx2_Buffer[0] = 0x0c;
			  m_index = 1;
				break;
			case 0x1B:
				Rx2_Buffer[0] = 0xDA;
				m_index = 2;
				break;
			case 0x1A:
				Rx2_Buffer[0] = 0xF2;
				m_index = 3;
				break;
			case 0x14:
				Rx2_Buffer[0] = 0x66;
				m_index = 4;
				break;
			case 0x13:
				Rx2_Buffer[0] = 0xB6;
				m_index = 5;
				break;
			case 0x12:
				Rx2_Buffer[0] = 0xBE;
				m_index = 6;
				break;
			case 0x0c:
				Rx2_Buffer[0] = 0xE0;
				m_index = 7;
				break;
			case 0x0B:
				Rx2_Buffer[0] = 0xFE;
				m_index = 8;			
				break;
			case 0x0A:
				Rx2_Buffer[0] = 0xE6;
				m_index = 9;
				break;
			case 0x19:
				Rx2_Buffer[0] = 0xEE;
				m_index = 10;
				break;
			case 0x11:
				Rx2_Buffer[0] = 0x3E;
				m_index = 11;
				break;
			case 0x09:
				Rx2_Buffer[0] = 0x9C;
				m_index = 12;
				break;
			case 0x01:
				Rx2_Buffer[0] = 0x7A;
				m_index = 13;
				break;
			case 0x02:
				printf("[SysInfo] Clear Screen\n");
				im_refresh = 1;
				for (int i = 0; i < 8; i++) {
					Rx2_Buffer[i] = 0;
				}
				m_index = 14;
				return;
			case 0x03:
				Rx2_Buffer[0] = 0xFC;
				m_index = 15;
				break;
			default:
				Rx2_Buffer[0] = 0xFF;
				m_index = 16;
				break;
		}
		//更新校验
		computer_vRx2(Rx2_Buffer);
		//输出按键信息
		if(Rx2_Buffer[0] == 0xFF){
			printf("[SysInfo] invalid input\n");
			return;
		}
		printf("[SysInfo] valid input code:%d\n", Rx2_Buffer[0]);
		if (!verify_Rx2(Rx2_Buffer)) {
			printf("[SysInfo] wrong with Rx2_buffer!\n");
			read();
		}
		im_refresh = 1;

}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	flag1 = 1;
}
int fputc(int ch, FILE *f)
{ 
  uint8_t tmp[1]={0};
	tmp[0] = (uint8_t)ch;
	HAL_UART_Transmit(&huart1,tmp,1,10);	
	return ch;
}
/* USER CODE END 4 */

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
