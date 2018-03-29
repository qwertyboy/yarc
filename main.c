/*
 * reflow-controller.c
 *
 * Created: 3/4/2018 11:13:09 PM
 * Author : Nathan
 */ 

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdlib.h>
#include "utils.h"
#include "spi.h"
#include "lcd.h"
#include "encoder.h"
#include "menu.h"

// process states
#define STATE_PREHEAT   0
#define STATE_SOAK      1
#define STATE_REFLOW    2
#define STATE_COOLDOWN  3

// PID constants
#define PID_KP 10
#define PID_KI 0.1
#define PID_KD 6.5

// misc variables
char printBuf[16];

int main(void){
    //-----------------------------Initialization----------------------------//
    // enable timer0 for timekeeping
    TimerInit();
    
    // set cs pins as output
    DDRB |= (1 << DDB2) | (1 << DDB1);
    // led on PD0
    DDRD |= (1 << DDD0);
    // button on PC2, set as input and enable pull-up
    DDRC &= ~(1 << DDC2);
    PORTC |= (1 << PORTC2);
    
    // enable spi
    SpiInit(SPI_CLKDIV_4);
    // enable lcd
    LcdInit(&PORTB, PORTB2);
    // create a degree symbol
    uint8_t degSymbol[] = {0x0C, 0x12, 0x12, 0x0C, 0x00, 0x00, 0x00, 0x00};
    uint8_t boxSymbol[] = {0x1F, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1F};
    LcdCreateChar(0, degSymbol);
    LcdCreateChar(1, boxSymbol);
    LcdClear();
    LcdClear();
    // enable max6675
    // initialize encoder
    EncoderInit(&PORTC, PORTC1, PORTC0);
    
    //---------------------------End Initialization--------------------------//
    
    LcdPrint("   YetAnother\nReflowController");
    Delay(2000);
    LcdClear();
    
    // profile vars
    Profile_t profile = Profiles[0];    // default to lead
    
    // wait for button to select a profile.
    LcdPrint("Select Profile:");
    while(ButtonRead(&PINC, PINC2, 50, 1000) != 1){
        // read encoder
        int16_t encoderPos = EncoderRead();
        static int16_t lastEncPos = -1;
        
        // check bounds
        if(encoderPos < 0){
            encoderPos = 0;
            EncoderSetPos(0);
        }else if(encoderPos > NUM_PROFILES - 1){
            encoderPos = NUM_PROFILES - 1;
            EncoderSetPos(NUM_PROFILES);
        }
        
        // get the profile selected by the encoder
        profile = Profiles[encoderPos];
        // update display if encoder changed
        if(encoderPos != lastEncPos){
            lastEncPos = encoderPos;
            LcdSetCursor(1, 1);
            LcdPrint("                ");  // clear line
            LcdSetCursor(1, 1);
            LcdPrint(profile.profileName);
        }
    }
    
    // display selected profile and wait for button to be held
    LcdClear();
    LcdPrint("Profile: ");
    LcdPrint(profile.profileName);
    LcdSetCursor(0, 1);
    LcdPrint("Hold start...");
    
    // blink button while waiting
    uint32_t blinkTime = 0;
    while(ButtonRead(&PINC, PINC2, 50, 1000) != 2){
        if(Millis() - blinkTime >= 1000){
            blinkTime = Millis();
            PIND |= (1 << PIND0);
        }
    }
    
    LcdClear();
    LcdPrint("Phase: ");
    while(1){
        static uint8_t phase = 0;
        static uint32_t loop10hz = 0;
        // pid variables
        static uint16_t ovenTemp = 0;
        static int16_t pidError = 0;
        static int16_t pidLastError = 0;
        static int16_t pidIntegral = 0;
        static int16_t pidDerivative = 0;
        static int16_t pidOut = 0;
        
        // 10 hz loop
        if(Millis() - loop10hz >= 200){
            loop10hz = Millis();
            // update lcd
            LcdSetCursor(7, 0);
            LcdPrint("        ");
            LcdSetCursor(7, 0);
            LcdPrint(PhaseText[phase]);
            
            // update PID parameters
            // calc error
            // ovenTemp = max6675Read() / 4;
            //ovenTemp = max31855Read();
            pidError = profile.preHeatTemp - ovenTemp;
            if(pidError < -1000){
                pidError = -1000;
            }else if(pidError > 1000){
                pidError = 1000;
            }
            
            // calc integral
            pidIntegral += pidError;
            if(pidIntegral < -1000){
                pidIntegral = -1000;
            }else if(pidIntegral > 1000){
                pidIntegral = 1000;
            }
            
            // calc derivative
            pidDerivative = pidError - pidLastError;
            if(pidDerivative < -1000){
                pidDerivative = -1000;
            }else if(pidDerivative > 1000){
                pidDerivative = 1000;
            }
            
            // update control variable
            pidOut = (PID_KP * pidError) + (PID_KI * pidIntegral) + (PID_KD * pidDerivative);
            if(pidOut < 0){
                pidOut = 0;
            }else if(pidOut > 1000){
                pidOut = 1000;
            }
            
            pidLastError = pidError;
            
            LcdSetCursor(0, 1);
            LcdPrint("p:     ");
            LcdSetCursor(3, 1);
            itoa(pidOut, printBuf, 10);
            LcdPrint(printBuf);
            LcdSetCursor(8, 1);
            LcdPrint("t:   ");
            LcdSetCursor(11, 1);
            itoa(ovenTemp, printBuf, 10);
            LcdPrint(printBuf);
        }
    }
}