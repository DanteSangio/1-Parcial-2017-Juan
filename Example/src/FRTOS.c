/*
===============================================================================
 Name        : FRTOS.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/
#include "chip.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

SemaphoreHandle_t Semaforo_1;
SemaphoreHandle_t Semaforo_2;

QueueHandle_t Cola_1;



#include <cr_section_macros.h>

// TODO: insert other include files here

// TODO: insert other definitions and declarations here
#define PORT(x) 	((uint8_t) x)
#define PIN(x)		((uint8_t) x)

#define OUTPUT		((uint8_t) 1)
#define INPUT		((uint8_t) 0)

#define MATCH0 		((int8_t) 0)
#define MATCH1 		((int8_t) 1)


void TIMER0_IRQHandler(void)
{
	uint32_t Match_Interrupcion = 2;
	BaseType_t Indicador;

	if (Chip_TIMER_MatchPending(LPC_TIMER0, MATCH0))
	{
		Chip_TIMER_ClearMatch(LPC_TIMER0, MATCH0);
		Match_Interrupcion = 0;
	}

	if (Chip_TIMER_MatchPending(LPC_TIMER0, MATCH1))
	{
		Chip_TIMER_ClearMatch(LPC_TIMER0, MATCH1);
		Match_Interrupcion = 1;
	}

	xQueueSendToBackFromISR(Cola_1, &Match_Interrupcion, &Indicador);

	portYIELD_FROM_ISR( Indicador );
}



void uC_StartUp (void)
{

	SystemCoreClockUpdate();

	/* Configuracion GPIO & IOCON */

	Chip_GPIO_Init (LPC_GPIO);

	Chip_IOCON_PinMux (LPC_IOCON , PORT(0) , PIN(22), IOCON_MODE_INACT , IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO , PORT(0) , PIN(22) , OUTPUT);

	Chip_IOCON_PinMux (LPC_IOCON , PORT(2) , PIN(10), IOCON_MODE_PULLUP , IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO , PORT(2) , PIN(10) , INPUT);

	Chip_IOCON_PinMux (LPC_IOCON , PORT(1) , PIN(31), IOCON_MODE_PULLUP , IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO , PORT(1) , PIN(31) , INPUT);

	Chip_IOCON_PinMux (LPC_IOCON , PORT(2) , PIN(3), IOCON_MODE_PULLUP , IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO , PORT(2) , PIN(3) , INPUT);

	Chip_IOCON_PinMux (LPC_IOCON , PORT(2) , PIN(4), IOCON_MODE_PULLUP , IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO , PORT(2) , PIN(4) , INPUT);

	Chip_IOCON_PinMux (LPC_IOCON , PORT(0) , PIN(20), IOCON_MODE_PULLUP , IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO , PORT(0) , PIN(20) , INPUT);

	/* Configuracion TIMER 0 */

	Chip_TIMER_Init(LPC_TIMER0);

	/* Timer setup for match and interrupt at TICKRATE_HZ */
	Chip_TIMER_Reset(LPC_TIMER0);
	Chip_TIMER_MatchEnableInt(LPC_TIMER0, MATCH0);
	Chip_TIMER_SetMatch(LPC_TIMER0, MATCH0, (SystemCoreClock / 4) / 200);
	Chip_TIMER_ResetOnMatchDisable(LPC_TIMER0, MATCH0);

	Chip_TIMER_MatchEnableInt(LPC_TIMER0, MATCH1);
	Chip_TIMER_SetMatch(LPC_TIMER0, MATCH1, (SystemCoreClock / 4) / 100);
	Chip_TIMER_ResetOnMatchEnable(LPC_TIMER0, MATCH1);
	Chip_TIMER_Enable(LPC_TIMER0);

	/* Enable timer interrupt */
	NVIC_ClearPendingIRQ(TIMER0_IRQn);
	NVIC_EnableIRQ(TIMER0_IRQn);
}


/* Toggle Led */
static void vTaskTimer(void *pvParameters)
{
	volatile uint32_t Match_Interrup;

	while (1)
	{
		xQueueReceive(Cola_1,&Match_Interrup,portMAX_DELAY);

		if (Match_Interrup == 0)
			Chip_GPIO_SetPinOutLow (LPC_GPIO , PORT(0) , PIN (22));

		if (Match_Interrup == 1)
			Chip_GPIO_SetPinOutHigh (LPC_GPIO , PORT(0) , PIN (22));
	}
}

/* Duty_Cycle */
static void vTaskDutyCycle(void *pvParameters)
{
	uint32_t Duty_cycle = 50;
	uint32_t Testigo_Up = FALSE;
	uint32_t Testigo_Down = FALSE;
	uint32_t Valor_Match;

	while (1)
	{
		vTaskDelay(50 / portTICK_PERIOD_MS);

		if (Chip_GPIO_GetPinState (LPC_GPIO , PORT(0) , PIN (20))) // Modo Manual.
		{
			if (Chip_GPIO_GetPinState (LPC_GPIO , PORT(2) , PIN (3)) && Testigo_Up == FALSE) // Si se presiona Puls_Up
			{
				if (Duty_cycle < 90)
					Duty_cycle += 10;

				Testigo_Up = TRUE;
			}

			if (Chip_GPIO_GetPinState (LPC_GPIO , PORT(2) , PIN (3)) == FALSE)
				Testigo_Up = FALSE;


			if (Chip_GPIO_GetPinState (LPC_GPIO , PORT(2) , PIN (4)) && Testigo_Down == FALSE) // Si se presiona Puls_Down
			{
				if (Duty_cycle > 10)
					Duty_cycle -= 10;

				Testigo_Down = TRUE;
			}

			if (Chip_GPIO_GetPinState (LPC_GPIO , PORT(2) , PIN (4)) == FALSE)
				Testigo_Down = FALSE;
		}

		else 	// Modo Automático.
		{
			if (Chip_GPIO_GetPinState (LPC_GPIO , PORT(2) , PIN (10)) ) // LDR == 1; Duty_Cycle = 80%.
				Duty_cycle = 80;

			else														// LDR == 0; Duty_Cycle = 80%.
				Duty_cycle = 50;

		}

		if ( Duty_cycle > 90)
			Duty_cycle = 90;

		if (Duty_cycle < 10)
			Duty_cycle = 10;

		Valor_Match = (10000 / Duty_cycle);

		Chip_TIMER_SetMatch(LPC_TIMER0, MATCH0, (SystemCoreClock / 4) / Valor_Match);
	}
}


/* Shut Down*/
static void vTaskShutDown(void *pvParameters)
{
	uint32_t SD_Testigo = FALSE;

	while (1)
	{
		if (Chip_GPIO_GetPinState (LPC_GPIO , PORT(1) , PIN (31)) == FALSE) // Condición de Shut Down
		{

			Chip_TIMER_Disable (LPC_TIMER0);
			Chip_GPIO_SetPinOutLow (LPC_GPIO , PORT(0) , PIN (22));

			SD_Testigo = TRUE;
		}

		else
		{
			if (SD_Testigo == TRUE)
			{
				SD_Testigo = FALSE;

				Chip_TIMER_Enable (LPC_TIMER0);
			}

		}
	}
}


int main(void)
{
	uC_StartUp ();
	SystemCoreClockUpdate();

	vSemaphoreCreateBinary(Semaforo_1);
	vSemaphoreCreateBinary(Semaforo_2);

	xSemaphoreTake(Semaforo_1 , portMAX_DELAY );

	Cola_1 = xQueueCreate(5, sizeof (uint32_t));

	xTaskCreate(vTaskTimer, (char *) "Task_Timer_Task",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 2UL),
				(xTaskHandle *) NULL);

	xTaskCreate(vTaskDutyCycle, (char *) "Duty_Cycle_Task",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
				(xTaskHandle *) NULL);

	xTaskCreate(vTaskShutDown, (char *) "Shut_Down_Task",
					configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 3UL),
					(xTaskHandle *) NULL);

	/* Start the scheduler */
	vTaskStartScheduler();

	/* Nunca debería arribar aquí */

    return 0;
}

