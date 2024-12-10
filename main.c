/*******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for over the wire(otw) 
* firmware upgrade using device firmware upgrade middleware and 
* PDL APIs for ModusToolbox.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2024, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/


/*******************************************************************************
* Header Files
*******************************************************************************/
#include "cy_pdl.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "mtb_hal.h"
#include "cy_dfu.h"
#include <string.h>


#include "cy_dfu.h"
#include "dfu_user.h"

#include "transport_i2c.h"
#include "cy_dfu.h"
#include "cy_dfu_logging.h"
#include "mtb_hal_i2c.h"
#include "cy_scb_i2c.h"
#include "cy_sysint.h"
#include "cybsp.h"

/*******************************************************************************
* Macros
*******************************************************************************/
/*GPIO ISR Callback argument structure */
typedef struct handler_data_struct{
    uint8_t count;
    uint8_t pend;
}handler_data;

/* Timeout for Cy_DFU_Continue(), in milliseconds */
#define DFU_SESSION_TIMEOUT_MS          (20u)
/* DFU idle timeout: 300 seconds */
#define DFU_IDLE_TIMEOUT_MS             (300000u)
/* DFU session timeout: 5 seconds */
#define DFU_COMMAND_TIMEOUT_MS          (5000u)

/*Transport switch state*/
#define TRANSPORT_SWITCH_PENDING        (1u)
#define TRANSPORT_SWITCH_CLEAR          (0u)

/*Interrupt Priority*/
#define GPIO_INTERRUPT_PRIORITY         (7u)

#define BOOT_IMAGE
#ifdef BOOT_IMAGE
/* LED toggle: 1 seconds */
#define LED_TOGGLE_INTERVAL_MS          (1000u)
#elif defined(UPDATE_IMAGE)
#define LED_TOGGLE_INTERVAL_MS          (500u)
#endif

/*******************************************************************************
* Global Variables
*******************************************************************************/
/*GPIO ISR Callback argument
 * count : Track ISR count for transport switch
 * pend :  Track the pending state of transport switch
 */
handler_data gpio_arg={
        .count = 0,
        .pend = 0
};

const char * const dfu_transport_str[] =
{
        [CY_DFU_I2C] = "I2C",
        [CY_DFU_UART] = "UART",
        [CY_DFU_SPI]  = "SPI"
};

/* DFU params, used to configure DFU. */
cy_stc_dfu_params_t dfu_params;

/* For the Retarget -IO (Debug UART) usage */
static cy_stc_scb_uart_context_t    DEBUG_UART_context;           /** UART context */
static mtb_hal_uart_t               DEBUG_UART_hal_obj;           /** Debug UART HAL object  */

/* For DFU I2C interface */
static mtb_hal_i2c_t                dfuI2cHalObj;                 /* I2C transport HAL object  */
static cy_stc_scb_i2c_context_t     dfuI2cContext;                /* I2C transport PDL context structure*/


/*******************************************************************************
* Function Prototypes
*******************************************************************************/
static uint32_t counter_timeout_seconds(uint32_t seconds, uint32_t timeout);
char* dfu_status_in_str(cy_en_dfu_status_t dfu_status);
static void check_transport(handler_data *arg, cy_en_dfu_transport_t * transport );
void dfuI2cIsr(void);
void dfuI2cTransportCallback(cy_en_dfu_transport_i2c_action_t action);


/*******************************************************************************
* Function Definitions
*******************************************************************************/
void dfuI2cIsr(void)
{
    mtb_hal_i2c_process_interrupt(&dfuI2cHalObj);
}

void dfuI2cTransportCallback(cy_en_dfu_transport_i2c_action_t action)
{
    if (action == CY_DFU_TRANSPORT_I2C_INIT)
    {
        Cy_SCB_I2C_Enable(DFU_I2C_HW);
        CY_DFU_LOG_INF("I2C transport is enabled");
    }
    else if (action == CY_DFU_TRANSPORT_I2C_DEINIT)
    {
        Cy_SCB_I2C_Disable(DFU_I2C_HW, &dfuI2cContext);
        CY_DFU_LOG_INF("I2C transport is disabled");
    }
}


/*******************************************************************************
 * Function Name: main
 ********************************************************************************
 * Summary:
 * This is the main function. it initialize DFU to receive and program application
 * images to device memory. It also blinks an LED for every 50 DFU Loop
 * (~ 1 second interval) count.
 *
 * Parameters:
 *  none
 *
 * Return:
 *  int
 *
 *******************************************************************************/
int main(void)
{
    cy_rslt_t result;
    int bootOption = 0x00;

    /* DFU Initialization*/
    uint32_t count = 0;
    uint32_t timeout_seconds = 0;
    cy_en_dfu_status_t dfu_status;
    uint32_t dfu_state = CY_DFU_STATE_NONE;

    /* Buffer to store DFU commands. */
    CY_ALIGN(4) static uint8_t dfu_buffer[CY_DFU_SIZEOF_DATA_BUFFER];

    /* Buffer for DFU data packets for transport API. */
    CY_ALIGN(4) static uint8_t dfu_packet[CY_DFU_SIZEOF_CMD_BUFFER];

    /* Assign DFU serial interface default selection */
    cy_en_scb_i2c_status_t pdlI2cStatus;
    cy_en_sysint_status_t  pdlSysIntStatus;
    cy_en_dfu_transport_t dfu_transport = CY_DFU_I2C;

    /* Configure DFU parameters */
    cy_stc_dfu_params_t dfu_params = {
            .timeout = DFU_SESSION_TIMEOUT_MS,
            .dataBuffer = &dfu_buffer[0],
            .packetBuffer = &dfu_packet[0],
    };

#if defined (CY_DEVICE_SECURE)
    cyhal_wdt_t wdt_obj;

    /* Clear watchdog timer so that it doesn't trigger a reset */
    result = cyhal_wdt_init(&wdt_obj, cyhal_wdt_get_max_timeout_ms());
    CY_ASSERT(CY_RSLT_SUCCESS == result);
    cyhal_wdt_free(&wdt_obj);
#endif /* #if defined (CY_DEVICE_SECURE) */

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Debug UART init */
    result = (cy_rslt_t)Cy_SCB_UART_Init(DEBUG_UART_HW, &DEBUG_UART_config, &DEBUG_UART_context);

    /* UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    Cy_SCB_UART_Enable(DEBUG_UART_HW);

    /* Setup the HAL UART */
    result = mtb_hal_uart_setup(&DEBUG_UART_hal_obj, &DEBUG_UART_hal_config, &DEBUG_UART_context, NULL);

    /* HAL UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    result = cy_retarget_io_init(&DEBUG_UART_hal_obj);

    /* HAL retarget_io init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");

    printf("****************** "
           "PSOC Control C3 DFU MW based Firmware upgrade Code Example "
           "****************** \r\n\n");

    printf("Application is successfully launched \r\n\n");
    printf("Image Type: %s \r\n\n", IMG_TYPE_MSG);
    printf("Version: %s \r\n\n", IMG_VER_MSG);

    pdlI2cStatus = Cy_SCB_I2C_Init(DFU_I2C_HW, &DFU_I2C_config, &dfuI2cContext);
    if (CY_SCB_I2C_SUCCESS != pdlI2cStatus)
    {
        CY_DFU_LOG_ERR("Error during I2C PDL initialization. Status: %X", pdlI2cStatus);
        CY_ASSERT(0);
    }

    result = mtb_hal_i2c_setup(&dfuI2cHalObj, &DFU_I2C_hal_config, &dfuI2cContext, NULL);
    if (CY_RSLT_SUCCESS != result)
    {
        CY_DFU_LOG_ERR("Error during I2C HAL initialization. Status: %lX", result);
    }
    else
    {
        cy_stc_sysint_t i2cIsrCfg =
        {
            .intrSrc = DFU_I2C_IRQ,
            .intrPriority = 3U
        };
        pdlSysIntStatus = Cy_SysInt_Init(&i2cIsrCfg, dfuI2cIsr);
        if (CY_SYSINT_SUCCESS != pdlSysIntStatus)
        {
            CY_DFU_LOG_ERR("Error during I2C Interrupt initialization. Status: %X", pdlSysIntStatus);
        }
        else
        {
            NVIC_EnableIRQ((IRQn_Type) i2cIsrCfg.intrSrc);
            CY_DFU_LOG_INF("I2C transport is initialized");
        }
    }

    printf("https://github.com/Infineon/"
           "Code-Examples-for-ModusToolbox-Software\r\n\n");

    cy_stc_dfu_transport_i2c_cfg_t i2cTransportCfg =
    {
        .i2c = &dfuI2cHalObj,
        .callback = dfuI2cTransportCallback,
    };

    Cy_DFU_TransportI2cConfig(&i2cTransportCfg);

    /* Initialize DFU Structure. */
    dfu_status = Cy_DFU_Init(&dfu_state, &dfu_params);

    if (CY_DFU_SUCCESS != dfu_status) {
        printf("DFU initialization failed \r\n");
        CY_ASSERT(0);
    }

    /* Initialize the User LED */
    bootOption = strcmp (IMG_TYPE_MSG, "UPDATE");

    /* Initialize DFU communication. */
    Cy_DFU_TransportStart(dfu_transport);

    printf("\r\n Starting DFU %s transport \r\n ",
                dfu_transport_str[dfu_transport]);

    for (;;) {

        dfu_status = Cy_DFU_Continue(&dfu_state, &dfu_params);
        count++;
        if (CY_DFU_STATE_FINISHED == dfu_state) {
            count = 0u;
            if (CY_DFU_SUCCESS == dfu_status) {
                printf("DFU_STATE_FINISHED: %s \r\n",dfu_status_in_str(dfu_status));
                printf("Launching Boot \r\n");
                NVIC_SystemReset();

            } else {
                Cy_DFU_Init(&dfu_state, &dfu_params);
                printf("DFU_STATE_FINISHED: %s \r\n",
                        dfu_status_in_str(dfu_status));
            }
        } else if (CY_DFU_STATE_FAILED == dfu_state) {
            count = 0u;
            Cy_DFU_Init(&dfu_state, &dfu_params);
            printf("DFU_STATE_FAILED: %s \r\n", dfu_status_in_str(dfu_status));
        }
        else if (dfu_state == CY_DFU_STATE_UPDATING) {
            timeout_seconds = (count >= counter_timeout_seconds(
                    DFU_COMMAND_TIMEOUT_MS,
                    DFU_SESSION_TIMEOUT_MS)) ? 1U : 0u;

            /*
             * if no command has been received during 5 seconds when the loading
             * has started then restart loading.
             */
            if (dfu_status == CY_DFU_SUCCESS) {
                count = 0u;
            } else if (dfu_status == CY_DFU_ERROR_TIMEOUT) {
                if (timeout_seconds != 0u) {
                    count = 0u;

                    /* Restart DFU. */
                    check_transport(&gpio_arg, &dfu_transport);
                }
            } else {
                count = 0u;

                /* Delay because Transport still may be sending error response to a host. */
                Cy_SysLib_Delay(DFU_SESSION_TIMEOUT_MS);
            }
        }

        /* Blink once per seconds */
        if ((count % counter_timeout_seconds(LED_TOGGLE_INTERVAL_MS,
                DFU_SESSION_TIMEOUT_MS)) == 0u) {

            /* Invert the USER LED state */
            if (0x0 == bootOption)
                Cy_GPIO_Inv(CYBSP_USER_LED_PORT, CYBSP_USER_LED2_PIN);
            else
                Cy_GPIO_Inv(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);
        }

        if ((count >= counter_timeout_seconds(DFU_IDLE_TIMEOUT_MS,
                DFU_SESSION_TIMEOUT_MS)) && (dfu_state == CY_DFU_STATE_NONE)) {

            /*
             * In case, no valid user application, lets start fresh all over.
             * This is just for demonstration. Final application can change
             * in to either assert, reboot, enter low power mode etc,
             * based on use case requirements.
             */
            count = 0;
        }
        Cy_SysLib_Delay(1);
    }
}

/*******************************************************************************
 * Function Name: counter_timeout_seconds
 ********************************************************************************
 * Summary:
 *  Function to return DFU loop count for various target timeout .
 *
 * Parameters:
 *  seconds - time in seconds for particular timeout
 *  timeout - timeout for particular DFU loop
 *
 * Return:
 *  count - Total DFU Loop count value for particular timeout.
 *
 *******************************************************************************/
static uint32_t counter_timeout_seconds(uint32_t seconds, uint32_t timeout) {
    uint32_t count = 1;

    if (timeout != 0) {
        count = ((seconds) / timeout);
    }

    return count;
}

/*******************************************************************************
 * Function Name: check_transport
 ********************************************************************************
 * Summary:
 * This is the function to check for pending transport switch request. It switches
 * DFU transport dynamically on GPIO interrupt request.
 *
 * Parameters:
 *  arg : callback data argument to check and switch transport
 *  transport : current DFU Transport
 *
 * Return:
 *  void
 *
 *******************************************************************************/
static void check_transport(handler_data* arg, cy_en_dfu_transport_t * transport )
{
    if (arg!=NULL && transport!=NULL)
    {
        if(TRANSPORT_SWITCH_PENDING == arg->pend)
        {
            Cy_DFU_TransportReset();
            Cy_DFU_TransportStop();
            arg->count = arg->count % 3;

            switch(arg->count)
            {
                case 0:
                    *transport = CY_DFU_I2C;
                    printf("\r\n Transport Updated to I2C \r\n ");
                    break;
                case 1:
                    *transport = CY_DFU_SPI;
                    printf("\r\n Transport Updated to SPI \r\n ");
                    break;
                case 2:
                    *transport = CY_DFU_UART;
                    printf("\r\n Transport Updated to UART \r\n ");
                    break;
                case 3:                
                    break;
                default:
                    break;
            };
            Cy_DFU_TransportStart(*(transport));
            arg->pend = TRANSPORT_SWITCH_CLEAR;
        }
        else
            Cy_DFU_TransportReset();
    }
}

/*******************************************************************************
 * Function Name: dfu_status_in_str
 ********************************************************************************
 * Summary:
 * This function shall return a string (consists of brief description)
 * with respect to DFU status code
 *
 * Parameters:
 *  dfu_status
 *
 * Return:
 *  string pointer
 *
 *******************************************************************************/
char* dfu_status_in_str(cy_en_dfu_status_t dfu_status)
{
    switch (dfu_status) {
    case CY_DFU_SUCCESS:
        return "DFU: success";

    case CY_DFU_ERROR_VERIFY:
        return "DFU:Verification failed";

    case CY_DFU_ERROR_LENGTH:
        return "DFU: The length the packet is outside of the expected range";

    case CY_DFU_ERROR_DATA:
        return "DFU: The data in the received packet is invalid";

    case CY_DFU_ERROR_CMD:
        return "DFU: The command is not recognized";

    case CY_DFU_ERROR_CHECKSUM:
        return "DFU: The checksum does not match the expected value ";

    case CY_DFU_ERROR_ADDRESS:
        return "DFU: The wrong address";

    case CY_DFU_ERROR_TIMEOUT:
        return "DFU: The command timed out";

    case CY_DFU_ERROR_BAD_PARAM:
        return "DFU: One or more of input parameters are invalid";

    case CY_DFU_ERROR_UNKNOWN:
        return "DFU: did not recognize error";

    default:
        return "Not recognized DFU status code";
    }
}
/* [] END OF FILE */
