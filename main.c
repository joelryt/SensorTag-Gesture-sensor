//Joel Rytkönen 2544166 ja Sakari Veteläinen 2555304

#include <stdio.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/mw/display/Display.h>
#include <ti/mw/display/DisplayExt.h>
#include <ti/drivers/i2c/I2CCC26XX.h>

/* Board Header files */
#include "Board.h"

#include "wireless/comm_lib.h"
#include "sensors/bmp280.h"
#include "sensors/mpu9250.h"

/* Task */
#define STACKSIZE 2048
Char labTaskStack[STACKSIZE];
Char commTaskStack[STACKSIZE];

//Näytönpäivitysfunkio
void updateDisplay(int tunnistustila);

/* Display */
Display_Handle hDisplay;

// JTKJ: Pin configuration and variables here
// JTKJ: Painonappien konfiguraatio ja muuttujat

static PIN_Handle buttonHandle;
static PIN_State buttonState;

static PIN_Handle ledHandle;
static PIN_State ledState;

PIN_Config buttonConfig[] = {
   Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE, // Hox! TAI-operaatio
   PIN_TERMINATE // Määritys lopetetaan aina tähän vakioon
};

PIN_Config ledConfig[] = {
   Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, 
   PIN_TERMINATE // Määritys lopetetaan aina tähän vakioon
};

//Virtanapin konfiguraatio ja muuttujat
static PIN_Handle hButtonShut;
static PIN_State bStateShut;

PIN_Config buttonShut[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};
PIN_Config buttonWake[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
   PIN_TERMINATE
};

// MPU GLOBAL VARIABLES
//
// *******************************
static PIN_Handle hMpuPin;
static PIN_State MpuPinState;
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

// MPU9250 I2C CONFIG
//
// *******************************
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};
 
 //globaalit muuttujat
int press = 0;
int highfive;
int kalastus;
int choice;
char viestit[10][16] = {{' '}, {' '}, {' '}, {' '}, {' '}};
char payload[16];
enum state {VALIKKO=1, TUNNISTUS, VIESTIT, KALIBROINTI};
enum state myState = VALIKKO;
int tunnistustila;

//napille käsittelijä
void buttonFxn(PIN_Handle handle, PIN_Id pinId) {
    
   press = 1;
   // Vaihdetaan led-pinnin tilaa negaatiolla
   PIN_setOutputValue( ledHandle, Board_LED1, !PIN_getOutputValue( Board_LED1 ) );
   
   System_printf("nappia painettu\n");
}

//virtanapille käsittelijä
void buttonShutFxn(PIN_Handle handle, PIN_Id pinId) {

    Display_clear(hDisplay);
    Display_close(hDisplay);
    Task_sleep(100000 / Clock_tickPeriod);

	PIN_close(hButtonShut);
	PINCC26XX_setWakeup(buttonWake);
	Power_shutdown(NULL,0);
}


void updateDisplay(int tunnistustila) {
    if (myState == TUNNISTUS && tunnistustila == 0) {
        Display_clear(hDisplay);
        Display_print0(hDisplay, 4, 1, "Paina nappia");
        Display_print0(hDisplay, 5, 1, "aloittaaksesi");
        Display_print0(hDisplay, 6, 1, "tunnistus");
        Display_print0(hDisplay, 9, 1, "Viim. viesti:");
        Display_print0(hDisplay, 10, 1, payload);
    }
    else if (myState == TUNNISTUS && tunnistustila == 1 ) {
        Display_clear(hDisplay);
        Display_print0(hDisplay, 4, 1, "Mittaus kesken...");
        Display_print0(hDisplay, 9, 1, "Viim. viesti:");
        Display_print0(hDisplay, 10, 1, payload);
    }
    else if (myState == TUNNISTUS && tunnistustila == 2) {
        Display_clear(hDisplay);
        Display_print0(hDisplay, 4, 1, "Ylavitonen");
        Display_print0(hDisplay, 5, 1, "tunnistettu");
        Display_print0(hDisplay, 9, 1, "Viim. viesti:");
        Display_print0(hDisplay, 10, 1, payload);
    }
    else if (myState == TUNNISTUS && tunnistustila == 3) {
        Display_clear(hDisplay);
        Display_print0(hDisplay, 4, 1, "Kalastus");
        Display_print0(hDisplay, 5, 1, "tunnistettu");
        Display_print0(hDisplay, 9, 1, "Viim. viesti:");
        Display_print0(hDisplay, 10, 1, payload);
    }
    else if (myState == TUNNISTUS && tunnistustila == 4) {
        Display_clear(hDisplay);
        Display_print0(hDisplay, 4, 1, "Eletta ei");
        Display_print0(hDisplay, 5, 1, "tunnistettu");
        Display_print0(hDisplay, 9, 1, "Viim. viesti:");
        Display_print0(hDisplay, 10, 1, payload);
    }
    else if (myState == VIESTIT) {
        Display_clear(hDisplay);
        Display_print0(hDisplay, 2, 3, "Viestit:");
        int i;
        for (i=0; i<=4; i++) {
            System_printf("%s, ", viestit[i]);
        }
        System_printf("\n");
        Display_print0(hDisplay, 4, 1, viestit[0]);
        Display_print0(hDisplay, 5, 1, viestit[1]);
        Display_print0(hDisplay, 6, 1, viestit[2]);
        Display_print0(hDisplay, 7, 1, viestit[3]);
        Display_print0(hDisplay, 8, 1, viestit[4]);
    }
    else if (myState == KALIBROINTI) {
        Display_clear(hDisplay);
        Display_print0(hDisplay, 4, 1, "Kalibrointi...");
        Display_print0(hDisplay, 5, 1, "Pida laite");
        Display_print0(hDisplay, 6, 1, "paikallaan");
        Display_print0(hDisplay, 9, 1, "Viim. viesti:");
        Display_print0(hDisplay, 10, 1, payload);
    }
}

Void labTaskFxn(UArg arg0, UArg arg1) {

    I2C_Handle      i2c;
    I2C_Params      i2cParams;
    I2C_Handle      i2cMPU; // INTERFACE FOR MPU9250 SENSOR
	I2C_Params      i2cMPUParams;
	
	//muuttujien alustukset
    float ax, ay, az, gx, gy, gz;
    float ax0, ay0, az0, gyrot_sum, gyrot0, gx0, gy0, gz0;
    float ax_alku, ay_alku, az_alku, gyrot_alku;
    float ay_vali1, gx_vali1, gx_vali2;
    float ay_loppu, gx_loppu, gy_loppu, gz_loppu, ay_loppu_avg, gx_loppu_avg, gy_loppu_avg, gz_loppu_avg, gyrot_loppu_avg;
    int alkutila, vali1, vali2, lopputila, i;
    
    /* Create I2C for sensors */
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;
    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;
    
    // JTKJ: Setup the BMP280 sensor here, before its use
    // JTKJ: Sensorin alustus t�ss� kirjastofunktiolla
    
    // MPU OPEN I2C
    //
    // *******************************
    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL) {
        System_abort("Error Initializing I2CMPU\n");
    }

    // *******************************
    //
    // MPU POWER ON
    //
    // *******************************
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);

    // WAIT 100MS FOR THE SENSOR TO POWER UP
	Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();

    // *******************************
    //
    // MPU9250 SETUP AND CALIBRATION
    //
    // *******************************
	System_printf("MPU9250: Setup and calibration...\n");
	System_flush();

	mpu9250_setup(&i2cMPU);

	System_printf("MPU9250: Setup and calibration OK\n");
	System_flush();
    
    /* Display */
    Display_Params displayParams;
	displayParams.lineClearMode = DISPLAY_CLEAR_BOTH;
    Display_Params_init(&displayParams);

    hDisplay = Display_open(Display_Type_LCD, &displayParams);
    if (hDisplay == NULL) {
        System_abort("Error initializing Display\n");
    }
    
    choice = 1;
    
    while (1) {
        
        // *******************************
        //
        // MPU ASK DATA
        //
        //    Accelerometer values: ax,ay,az
        //    Gyroscope values: gx,gy,gz
        //
        // ********************************/
	    mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
	    
        if (gx < -50.0) {
            choice++;
            if (choice > 3) {
                choice = 1;
            } 
        }
        
        
        //menu
        switch (choice) {
            case 1: 
                myState = VALIKKO;
                Display_print0(hDisplay, 1, 1, "=>TUNNISTA");
                Display_print0(hDisplay, 2, 1, "  VIESTIT");
                Display_print0(hDisplay, 3, 1, "  KALIBROINTI");
                Display_print0(hDisplay, 9, 1, "Viim. viesti:");
                Display_print0(hDisplay, 10, 1, payload);
                if (press == 1) {
                    press = 0;
                    myState = TUNNISTUS;
                    tunnistustila = 0;
                    updateDisplay(tunnistustila);
                    
                    while (myState == TUNNISTUS) {
                        if (press == 1)  {
                            
                            tunnistustila = 1;
                            updateDisplay(tunnistustila);
                            
                            // Muuttujien nollaus
                            ax0 = 0, ay0 = 0, az0 = 0, gx0 = 0, gy0 = 0, gz0 = 0, gyrot_sum = 0, gyrot0 = 0;
                            ax_alku = 0, ay_alku = 0, az_alku = 0, gyrot_alku = 0;
                            gx_vali1 = 0, ay_vali1 = 0, gx_vali2 = 0, ay_loppu_avg = 0, gx_loppu_avg = 0;
                            alkutila = 0, vali1 = 0, vali2 = 0, lopputila = 0, highfive = 0, press = 0;
                            gx_loppu_avg = 0, gy_loppu_avg = 0, gz_loppu_avg = 0, gyrot_loppu_avg = 0, kalastus = 0;
                            ay_loppu = 0, gx_loppu = 0, gy_loppu = 0, gz_loppu = 0;
                            
                            System_printf("i,ax,ay,az,gx,gy,gz\n");
                            
                            for (i=0; i<=19; i++) {     //mittauksen kesto/mittausten määrä
                            
                        	    if (i2cMPU == NULL) {
                        	        System_abort("Error Initializing I2CMPU\n");
                        	    }
                        
                        	    // *******************************
                        	    //
                        	    // MPU ASK DATA
                        		//
                                //    Accelerometer values: ax,ay,az
                        	 	//    Gyroscope values: gx,gy,gz
                        		//
                        	    // ********************************/
                	            mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
                	            
                	            //muuttujat konsolille tulostamista varten
                	            char axp[8];
                                char ayp[8];
                                char azp[8];
                                char gxp[8];
                                char gyp[8];
                                char gzp[8];
                                char ax_alkup[8];
                                char ay_alkup[8];
                                char az_alkup[8];
                                char gyrot_loppu_avgp[8];
                                
                                // DO SOMETHING WITH THE DATA
                                
                                //Lähtöasento
                                if (i <= 2) {
                                    ax0 += ax;
                                    ay0 += ay;
                                    az0 += az;
                                    gx0 = gx;
                                    gy0 = gy;
                                    gz0 = gz;
                                    gyrot_sum = (gx0 + gy0 + gz0);
                                    gyrot0 += (gyrot_sum / 3);
                                }
                                
                                //Asennot välillä
                                else if (i >= 3 && i <= 8) {
                                    if (gx < -90) {
                                        gx_vali1 = 1;
                                    }
                                    if (ay > 0.5) {
                                        ay_vali1 = 1;
                                    }
                                }
                                
                                else if (i >= 9 && i <= 15 && gx > 80) {
                                    gx_vali2 = 1;
                                }
                                
                                //Loppuasento
                                else if (i >= 16 && i <= 19) {
                                    ay_loppu += ay;
                                    gx_loppu += abs(gx);
                                    gy_loppu += abs(gy);
                                    gz_loppu += abs(gz);
                                }
                                
                                //mittausarvot konsoliin
                                System_printf("%i,", i);
                                
                                sprintf(axp, "%f", ax * 100);
                                System_printf("%s,", axp);
                                
                                sprintf(ayp, "%f", ay * 100);
                                System_printf("%s,", ayp);
                                
                                sprintf(azp, "%f", az * 100);
                                System_printf("%s,", azp);
                                
                                sprintf(gxp, "%f", gx);
                                System_printf("%s,", gxp);
                                
                                sprintf(gyp, "%f", gy);
                                System_printf("%s,", gyp);
                                
                                sprintf(gzp, "%f", gz);
                                System_printf("%s\n", gzp);
                                System_flush();
                                
                                //tarkistukset mittauksen lopuksi
                                if (i == 19) {
                                    ax_alku = ax0 / 3;
                                    ay_alku = ay0 / 3;
                                    az_alku = az0 / 3;
                                    gyrot_alku = gyrot0 / 3;
                                    ay_loppu_avg = ay_loppu / 4;
                                    gx_loppu_avg = gx_loppu / 4;
                                    gy_loppu_avg = gy_loppu / 4;
                                    gz_loppu_avg = gz_loppu / 4;
                                    gyrot_loppu_avg = (gx_loppu_avg + gy_loppu_avg + gz_loppu_avg) / 3;
                                    
                                    sprintf(ax_alkup, "%f", ax_alku);
                                    System_printf("ax_alku: %s\n", ax_alkup);
                                    
                                    sprintf(ay_alkup, "%f", ay_alku);
                                    System_printf("ay_alku: %s\n", ay_alkup);
                                    
                                    sprintf(az_alkup, "%f", ay_loppu_avg);
                                    System_printf("ay_loppu_avg: %s\n", az_alkup);
                                    
                                    sprintf(gyrot_loppu_avgp, "%f", gyrot_loppu_avg);
                                    System_printf("gyrot_loppu_avg: %s\n", gyrot_loppu_avgp);
                                    System_flush();
                                    
                                    if (abs(ax_alku) < 2 && abs(ay_alku) < 2 && az_alku > -1.4 && az_alku < -0.85 && abs(gyrot_alku) < 2) {
                                        alkutila = 1;
                                        System_printf("Alkutila ok\n");
                                        System_flush();
                                    }
                                    if (ay_vali1 == 1 && gx_vali1 == 1) {
                                        vali1 = 1;
                                        System_printf("Väli 1 ok\n");
                                        System_flush();
                                    }
                                    if (gx_vali2 == 1) {
                                        vali2 = 1;
                                        System_printf("Väli 2 ok\n");
                                        System_flush();
                                    }
                                    if (ay_loppu_avg >= 0.5 && ay_loppu_avg <= 1.2 && gx_loppu_avg >= -30 && gx_loppu_avg <= 20) {
                                        lopputila = 1;
                                        System_printf("Lopputila ok\n");
                                        System_flush();
                                    }
                                    else if (gyrot_loppu_avg > 120) {
                                        lopputila = 2;
                                        System_printf("Lopputila2 ok\n");
                                        System_flush();
                                    }
                                    if (alkutila == 1 && vali1 == 1 && vali2 == 1 && lopputila == 1) {
                                        tunnistustila = 2;
                                        highfive = 1;
                                        System_printf("High-five!!!\n");
                                        System_flush();
                                        updateDisplay(tunnistustila);
                                    }
                                    else if (alkutila == 1 && vali1 == 1 && vali2 == 1 && lopputila == 2) {
                                        tunnistustila = 3;
                                        kalastus = 1;
                                        System_printf("KAHEN KILON SIIKA\n");
                                        System_flush();
                                        updateDisplay(tunnistustila);
                                    }
                                    else {
                                        tunnistustila = 4;
                                        System_printf("Elettä ei tunnistettu\n");
                                        System_flush();
                                        updateDisplay(tunnistustila);
                                    }
                                }
                        	    // WAIT 50MS
                            	Task_sleep(50000 / Clock_tickPeriod);  //mittausväli
                           }
                            myState = VALIKKO;
                        }
                    }
                    Task_sleep(4000000 / Clock_tickPeriod);
                    Display_clear(hDisplay);
                    }
                break;    
            case 2:
                myState = VALIKKO;
                Display_print0(hDisplay, 1, 1, "  TUNNISTA");
                Display_print0(hDisplay, 2, 1, "=>VIESTIT");
                Display_print0(hDisplay, 3, 1, "  KALIBROINTI");
                Display_print0(hDisplay, 9, 1, "Viim. viesti:");
                Display_print0(hDisplay, 10, 1, payload);
                if (press == 1) {
                    press = 0;
                    myState = VIESTIT;
                    updateDisplay(tunnistustila);
                    Task_sleep(4000000 / Clock_tickPeriod);
                    Display_clear(hDisplay);
                }
                break;
            case 3:
                myState = VALIKKO;
                Display_print0(hDisplay, 1, 1, "  TUNNISTA");
                Display_print0(hDisplay, 2, 1, "  VIESTIT");
                Display_print0(hDisplay, 3, 1, "=>KALIBROINTI");
                Display_print0(hDisplay, 9, 1, "Viim. viesti:");
                Display_print0(hDisplay, 10, 1, payload);
                if (press == 1) {
                    press = 0;
                    myState = KALIBROINTI;
                    updateDisplay(tunnistustila);
                    
                    System_printf("MPU9250: Setup and calibration...\n");
                	System_flush();
                
                	mpu9250_setup(&i2cMPU);
                
                	System_printf("MPU9250: Setup and calibration OK\n");
                	System_flush();
                    Task_sleep(3000000 / Clock_tickPeriod);
                    Display_clear(hDisplay);
                }
                break;
        }
        
        Task_sleep(100000 / Clock_tickPeriod);  
    }
}

/* Communication Task */
Void commTaskFxn(UArg arg0, UArg arg1) {
   uint16_t senderAddr;
   
    // Radio to receive mode
	int32_t result = StartReceive6LoWPAN();
	if(result != true) {
		System_abort("Wireless receive mode failed");
	}

    while (1) {
        if (highfive == 1) {
            highfive = 0;
            char payloadtx[16] = "highfive";
            Send6LoWPAN(IEEE80154_SERVER_ADDR, payloadtx, (strlen(payloadtx)));
            StartReceive6LoWPAN();
            
        }
        else if (kalastus == 1) {
            kalastus = 0;
            char payloadtx2[16] = "2KG SIIKA";
            Send6LoWPAN(IEEE80154_SERVER_ADDR, payloadtx2, (strlen(payloadtx2)));
            StartReceive6LoWPAN();
        }
        // If true, we have a message
    	else if (GetRXFlag() == true) {

    		// Handle the received message..
           memset(payload,0,16);
           // Luetaan viesti puskuriin payload
           Receive6LoWPAN(&senderAddr, payload, 16);
           int j;
           int i;
           for (j=3; j>=0; j--) {
               strcpy(viestit[j+1], viestit[j]);
           }
           strcpy(viestit[0], payload);
                        
           for (i=0; i<=4; i++) {
               System_printf("%s, ", viestit[i]);
           }
           System_printf("\n");
           System_flush();
           // Tulostetaan vastaanotettu viesti konsoli-ikkunaan
           System_printf("%s\n", payload);
           System_flush();
        }
    }

    	// Absolutely NO Task_sleep in this task!!
}

Int main(void) {

    // Task variables
	Task_Handle labTask;
	Task_Params labTaskParams;
	Task_Handle commTask;
	Task_Params commTaskParams;

    // Initialize board
    Board_initGeneral();
    Board_initI2C();
    
    // OPEN MPU POWER PIN
    //
    // *******************************
    hMpuPin = PIN_open(&MpuPinState, MpuPinConfig);
    if (hMpuPin == NULL) {
    	System_abort("Pin open failed!");
    }

	// JTKJ: Open and configure the button and led pins here
    // JTKJ: Painonappi- ja ledipinnit k�ytt��n t�ss�
    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if(!buttonHandle) {
      System_abort("Error initializing button pins\n");
   }
   ledHandle = PIN_open(&ledState, ledConfig);
   if(!ledHandle) {
      System_abort("Error initializing LED pins\n");
   }

   // Asetetaan painonappi-pinnille keskeytyksen käsittellijä
   // funktiossa buttonFxn yllä
   if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
      System_abort("Error registering button callback function");
   }
   
   //Virtanappi käyttöön ja sille käsittelijä
   hButtonShut = PIN_open(&bStateShut, buttonShut);
   if( !hButtonShut ) {
      System_abort("Error initializing button shut pins\n");
   }
   if (PIN_registerIntCb(hButtonShut, &buttonShutFxn) != 0) {
      System_abort("Error registering button callback function");
   }
   
	// JTKJ: Register the interrupt handler for the button
    // JTKJ: Rekister�i painonapille keskeytyksen k�sittelij�funktio

    /* Task */
    Task_Params_init(&labTaskParams);
    labTaskParams.stackSize = STACKSIZE;
    labTaskParams.stack = &labTaskStack;
    labTaskParams.priority=2;

    labTask = Task_create(labTaskFxn, &labTaskParams, NULL);
    if (labTask == NULL) {
    	System_abort("Task create failed!");
    }

    /* Communication Task */
    Init6LoWPAN(); // This function call before use!

    Task_Params_init(&commTaskParams);
    commTaskParams.stackSize = STACKSIZE;
    commTaskParams.stack = &commTaskStack;
    commTaskParams.priority=1;

    commTask = Task_create(commTaskFxn, &commTaskParams, NULL);
    if (commTask == NULL) {
    	System_abort("Task create failed!");
    }

    System_printf("Hello world!\n");
    System_flush();
    
    /* Start BIOS */
    BIOS_start();

    return (0);
}

