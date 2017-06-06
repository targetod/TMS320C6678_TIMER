#include <cerrno>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ti\platform\platform.h"
#include "ti\platform\resource_mgr.h"

#include <ti/csl/csl_tmr.h>
#include <ti/csl/csl_tmrAux.h>
#include <ti/csl/src/intc/csl_intc.h>
#include <ti/csl/src/intc/csl_intcAux.h>


/************************** Global Variables *************************/

/* INTC Objects */
CSL_IntcObj                  tmrIntcObj;
CSL_IntcContext              context;
CSL_IntcEventHandlerRecord   EventHandler[30];

platform_info p_info;

/* Counter for Timer ISR */
volatile Int32 timerISRCounter = 0;

void TimerInterruptHandler (void *arg);
Int32 intc_init (void);
Int32 test_high_continuous_timer (Uint8 IntcInstance);


/* OSAL functions for Platform Library */
uint8_t *Osal_platformMalloc (uint32_t num_bytes, uint32_t alignment)
{
    return malloc(num_bytes);
}

void Osal_platformFree (uint8_t *dataPtr, uint32_t num_bytes)
{
    /* Free up the memory */
    if (dataPtr)
    {
        free(dataPtr);
    }
}

void Osal_platformSpiCsEnter(void)
{
    /* Get the hardware semaphore.
     *
     * Acquire Multi core CPPI synchronization lock
     */
    while ((CSL_semAcquireDirect (PLATFORM_SPI_HW_SEM)) == 0);

    return;
}

void Osal_platformSpiCsExit (void)
{
    /* Release the hardware semaphore
     *
     * Release multi-core lock.
     */
    CSL_semReleaseSemaphore (PLATFORM_SPI_HW_SEM);

    return;
}



void main(void) {
   platform_init_flags init_flags;
   platform_init_config init_config;

   char message[] = "\r\nHello World.....\r\n";
   uint32_t length = strlen((char *)message);
   uint32_t i;

   /* Initialize platform with default values */

   memset(&init_flags, 0x01, sizeof(platform_init_flags));
   memset(&init_config, 0, sizeof(platform_init_config));
   if (platform_init(&init_flags, &init_config) != Platform_EOK) {
       return;
   }

   platform_uart_init();
   platform_uart_set_baudrate(115200);

   platform_get_info(&p_info);

  //  Write to the UART
   for (i = 0; i < length; i++) {
       if (platform_uart_write(message[i]) != Platform_EOK) {
           return;
       }
   }

   printf ("****************** Timer Testing  ****************\n");

     /* Initialize the INTC Module. */
     if (intc_init() < 0){
         printf ("Error: Initialization of the INTC module failed\n");
         return;
     }

     /* Initialize timer CSL module */
     CSL_tmrInit(NULL);

    /* Start the testing for the  Timer. */
    if (test_high_continuous_timer(CSL_TMR_0) < 0)
    {
        printf("Error: Testing hi Timer (Unchained)  FAILED\n");
        return;
    }
    printf("Debug: Testing hi Timer (Unchained) Passed\n");

   /* Play forever */
   while(1) {}
}


//функція ініціалізації переривання

Int32 intc_init (void)
{
   // Global Interrupt enable state
   CSL_IntcGlobalEnableState   state;

   /* INTC module initialization */
   context.eventhandlerRecord = EventHandler;
   context.numEvtEntries      = 10;
   if (CSL_intcInit(&context) != CSL_SOK)
       return -1;

   /* Enable NMIs */
   if (CSL_intcGlobalNmiEnable() != CSL_SOK)
       return -1;

   /* Enable global interrupts */
   if (CSL_intcGlobalEnable(&state) != CSL_SOK)
       return -1;

   /* INTC has been initialized successfully. */
   return 0;
}



// функція обробки переривання

void TimerInterruptHandler (void *arg)
{

   static uint32_t led_no = 0;
   /* Increment the number of interrupts detected. */
   timerISRCounter++;
   if (timerISRCounter%2 == 0){
       platform_led(led_no, PLATFORM_LED_ON, PLATFORM_USER_LED_CLASS);
   }
   else
   {
       platform_led(led_no, PLATFORM_LED_OFF, PLATFORM_USER_LED_CLASS);
       led_no = (++led_no) % p_info.led[PLATFORM_USER_LED_CLASS].count;
   }

   /* Clear the event ID. */
   CSL_intcEventClear((CSL_IntcEventId)arg);
}


// функція налаштування таймера та переривання за таймером

Int32 test_high_continuous_timer (Uint8 IntcInstance)
{
   CSL_TmrHandle               hTmr; // дескриптор таймера
   CSL_TmrObj                  TmrObj; // об'єкт таймера
   CSL_Status                  status; // статус
   CSL_TmrHwSetup              hwSetup = CSL_TMR_HWSETUP_DEFAULTS; // налаштування таймера (період...)
   CSL_IntcEventHandlerRecord  EventRecord;// запис дескриптора події
   CSL_IntcParam               vectId; // айді вектора переривання
   CSL_IntcHandle              tmrIntcHandle; // дескриптор переривання для таймера
   Uint32                      LoadValue = 83333500; // значення періоду
   CSL_TmrEnamode              TimeCountMode = CSL_TMR_ENAMODE_CONT; // неперервний режим таймера
   Uint32                      count; // лічильник

   /* Clear local data structures */
   memset(&TmrObj, 0, sizeof(CSL_TmrObj)); // обнулення обєкту таймера
   printf("Debug: Testing High Timer (Unchained) in Continuous Mode....\n");

   /**************************************************************
    ********************** INTC related code *********************
    **************************************************************/

   /* Open INTC */
   vectId = CSL_INTC_VECTID_13; // встановлюємо 13 вектор переривання

   // відкриваємо переривання для події CSL_GEM_TINTHN
   tmrIntcHandle = CSL_intcOpen(&tmrIntcObj, CSL_GEM_TINTHN, &vectId, NULL);
   if (tmrIntcHandle == NULL)
       return -1;

   /* Bind ISR to Interrupt */
   // записуємо функцію, яка буде оброблювати переривання по таймеру
   EventRecord.handler = (CSL_IntcEventHandler)&TimerInterruptHandler;
   // передаєм додатковий аргумент
   EventRecord.arg     = (void *)CSL_GEM_TINTLN;
   // зв*язуємо функцію оброблення з перериванням
   CSL_intcPlugEventHandler(tmrIntcHandle, &EventRecord);

   // Event Enable  дозволяємо подію
   CSL_intcHwControl(tmrIntcHandle, CSL_INTC_CMD_EVTENABLE, NULL);

   //********************** Timer related code ********************

   // Open the timer.  Відкриваємо таймер IntcInstance= CSL_TMR_0 для 0 ядра
   hTmr =  CSL_tmrOpen(&TmrObj, IntcInstance, NULL, &status);
   if (hTmr == NULL)
       return -1;

   // Open the timer with the defaults. встановлюємо налаштування по замовчуванню- все по нулях
   CSL_tmrHwSetup(hTmr, &hwSetup);

   // Stop the Timer - зупиняєм таймер
   CSL_tmrHwControl(hTmr, CSL_TMR_CMD_RESET_TIMHI, NULL);

   // Set the timer mode to unchained dual mode -
   // встановлюєм режим таймера unchained dual mode
   hwSetup.tmrTimerMode = CSL_TMR_TIMMODE_DUAL_UNCHAINED;
   CSL_tmrHwSetup(hTmr, &hwSetup);

   /* Reset the timer ISR Counter. */
   timerISRCounter = 0;

   // Load the period register - записуємо період таймера
   status = CSL_tmrHwControl(hTmr, CSL_TMR_CMD_LOAD_PRDHI, (void *)&LoadValue);

   // Start the timer in CONTINUOUS Mode. Запускаєм таймер в непереввному режимі
   CSL_tmrHwControl(hTmr, CSL_TMR_CMD_START_TIMHI, (void *)&TimeCountMode);

   /* Wait for the timer interrupts to fire...*/
   while (timerISRCounter <= 500);

   /* Since the HIGH Counter is operating in Continuous Mode; the value here should
    * be non-zero. Though there is a small probability that the value here could be 0. */
   CSL_tmrGetTimHiCount(hTmr, &count);
   if (count == 0)
   {
       /* Taking into account the small probability; lets confirm out again.
        * This time for sure the value should be non-zero. */
       CSL_tmrGetTimHiCount(hTmr, &count);
       if (count == 0)
           return -1;
   }

   /**************************************************************/

   /* Disable the events. */
   CSL_intcHwControl(tmrIntcHandle, CSL_INTC_CMD_EVTDISABLE, NULL);

   /* Stop the Timer */
   CSL_tmrHwControl(hTmr, CSL_TMR_CMD_RESET_TIMHI, NULL);

   /* Close the Tmr and Interrupt handles. */
   CSL_tmrClose(hTmr);
   CSL_intcClose(tmrIntcHandle);

   /* Test has completed successfully. */
   return 0;
}
