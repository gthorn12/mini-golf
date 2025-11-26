
// gthorn -- Semester Project //
#define F_CPU 1000000UL
#include <avr/io.h>     
#include <util/delay.h> 
#include <stdio.h>
#include <stdlib.h>  
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <util/setbaud.h>
#include "USART.h"
#include <avr/interrupt.h>
#include "oled.h"
#include "i2c.h"



//Globals
volatile bool ballScored = false;        //Flag for ball scored
volatile bool gameStarted = false;        //Flag for game start
volatile bool gameOn = false;           //Flag for game ongoing
#define PWM_TOP 19999                        //Top value for 50Hz PWM
#define SERVO_180 2499                //Servo 180 degree pulse width
#define SERVO_90 1999                 //Servo 90 degree pulse width
#define SERVO_0 1000                  //Servo 0 degree pulse width

int score = 0;                            //Score variable

volatile uint32_t elapsedTimeMs = 0;     //Elapsed time in milliseconds
#define GAME_DURATION 60000UL            //Game duration in milliseconds (60 seconds)


//PWM (PB1/OC1A, PB2/OC1B, PD6/OC0A) Servo initialization
static void pwm_init(void) {
    DDRB |= (1 << PB1);                                     // PB1 as output 

    TCCR1A |= (1 << WGM11) | (1 << COM1A1);                // Customize PWM to have 50Hz frequency
                                        
    TCCR1B |= (1 << WGM13) | (1 << WGM12) | (1 << CS10);   //Special PWM mode, no prescaling 
    
    ICR1 = PWM_TOP;                                            //Set TOP value for 50Hz
}

//ISR initializations
static void isr_init(void) {
    //INT0 - Breakbeam sensor
    EICRA |= (1 << ISC01);               //Falling edge on INT0
    EICRA &= ~(1 << ISC00);    

    EIMSK |= (1 << INT0);                //Enable INT0

    //INT1 - Start button
    EICRA |= (1 << ISC11);               //Falling edge on INT1
    EICRA &= ~(1 << ISC10);    

    EIMSK |= (1 << INT1);                //Enable INT1
}   

//Breakbeam initialization
static void breakbeam_init(void) {
    DDRD &= ~(1 << PD2); //Set PD2 (INT0) as input  
    PORTD |= (1 << PD2); //Enable pull-up resistor on PD2
}

//Start button initialization
static void start_button_init(void) {
    DDRD &= ~(1 << PD3); //Set PD3 (INT1) as input  
    PORTD |= (1 << PD3); //Enable pull-up resistor on PD3
}   

//UART initialization
void usart_init(void) {
    UBRR0H = UBRRH_VALUE;                  //Set baud rate
    UBRR0L = UBRRL_VALUE;               
#if USE_2X
    UCSR0A |= (1 << U2X0);                 //Enable double speed mode
#else
    UCSR0A &= ~(1 << U2X0);                //Disable double speed mode
#endif
    UCSR0C |= (1 << UCSZ01)|(1 << UCSZ00); //8 data bits, 1 stop bit, no parity
    UCSR0B |= (1 << TXEN0)|(1 << RXEN0);   //Enable tx and rx
}

//Interrupt service routines
ISR(INT0_vect) {
    ballScored = true; //Set flag
}

ISR(INT1_vect) {
    gameStarted = true; //Set flag
    gameOn = true;    //Set game on flag
    _delay_ms(20); //Debounce delay
}

// void servo_setAngle(uint16_t angle) {
//     if (angle > 180) angle = 180; //Limit angle to 180 degrees
//     uint16_t pulseWidth = SERVO_0 + ((SERVO_180 - SERVO_0) * angle) / 180; //Calculate pulse width
//     OCR1A = pulseWidth; //Set OCR1A to pulse width
// } 

void servo_set_pulsewidth(uint16_t pulseWidth) {
    if (pulseWidth < SERVO_0) pulseWidth = SERVO_0;       //Limit pulse width to SERVO_0
    if (pulseWidth > SERVO_180) pulseWidth = SERVO_180;   //Limit pulse width to SERVO_180
    OCR1A = pulseWidth;                                   //Set OCR1A to pulse width
}

double get_elapsed_time(void) {
    return elapsedTimeMs / 1000.0; //Convert milliseconds to seconds
}

void update_score (void) {
    uint16_t t = get_elapsed_time();
    if (t < 10.0) {
        score += 10;
    } else if (t < 30.0) {
        score += 5;
    } else {
        score += 2;
    }
}

void release_ball(void) {
    servo_set_pulsewidth(SERVO_90); //Set servo to 90 degrees to release ball
    _delay_ms(500);    //Wait for 0.5 seconds
    servo_set_pulsewidth(SERVO_0);  //Set servo back to 0 degrees
}   

void display_game_over(void) {
    ssd1306_clear();
    ssd1306_set_cursor(20, 2);
    ssd1306_print("GAME OVER!");
    ssd1306_set_cursor(20, 4);
    char buf[20];
    snprintf(buf, 20, "Score: %d", score);
    ssd1306_print(buf);
    _delay_ms(10000); //Display for 10 seconds
}

void display_game_start(void) {
    ssd1306_clear();
    ssd1306_set_cursor(10, 2);
    ssd1306_print("Game Started!");
    _delay_ms(2000); //Display for 2 seconds
    //elapse_time(2000); //May need to account for display time, but also good to give player time to get ready
    //ssd1306_clear(); 
    gameStarted = false; //Reset flag   
}

void display_made_putt(void) {
    ssd1306_clear();
    ssd1306_set_cursor(20, 2);
    ssd1306_print("Made Putt!");
    _delay_ms(2000); //Display for 2 seconds
    elapse_time(2000); //Account for display time
    ssd1306_clear();
}

uint16_t countdown(uint16_t seconds) {
    if (seconds >= 60) return 0;
    uint16_t remaining = 60 - seconds;
    return remaining;
}

void display_live_time_and_score(void){
    ssd1306_clear();
    ssd1306_set_cursor(10, 2);
    char buf[30];
    uint32_t ms = elapsedTimeMs;
    uint16_t seconds = ms / 1000;
    snprintf(buf, 30, "Time Left: %u s", countdown(seconds));
    ssd1306_print(buf);
    ssd1306_set_cursor(10, 6);
    snprintf(buf, 30, "Score: %d", score);
    ssd1306_print(buf);
}

void splash_animation(void) {
    ssd1306_clear();
    ssd1306_set_cursor(10, 2);  
    ssd1306_print("Welcome to");
    ssd1306_set_cursor(10, 4);  
    ssd1306_print("Mini Golf!");
}

void display_splash_screen(void) {
    splash_animation();
    _delay_ms(3000); //Display for 3 seconds
}

void ball_scored(void) {
    update_score();          //Update score
    ballScored = false;      //Reset flag
    release_ball();          //Release next ball
    display_made_putt();     //Display made putt
}

void end_game(void) {
    display_game_over();
    gameOn = false;    //Reset game start flag   
    score = 0;           //Reset score
    elapsedTimeMs = 0;   //Reset elapsed time
}

void elapse_time(uint16_t duration_ms) {
    while (duration_ms--) {
        _delay_ms(1);      // 1 ms is a compile-time constant
        elapsedTimeMs++;   // track elapsed time
    }
}




int main(void) {

    pwm_init();       //Initialize pwm
    usart_init();     //Initialize UART
    isr_init();       //Initialize ISRs
    breakbeam_init(); //Initialize breakbeam
    start_button_init(); //Initialize start button
    sei();            //Enable global interrupts
    initI2C();          //Initialize i2c
    ssd1306_init();        //Initialize OLED
    ssd1306_clear();      //Clear display

    servo_set_pulsewidth(SERVO_0); //Set servo to initial position

    while (1) {

        if (gameOn) {

            if (gameStarted) {
                display_game_start(); //Display game start
                release_ball(); //Release ball after start
            }

            if (ballScored) {
                ball_scored(); //Handle ball scored
                _delay_ms(20); //Debounce delay
                elapse_time(20); //Account for debounce time
            }

            elapse_time(50); //Update elapsed time

            if (elapsedTimeMs >= GAME_DURATION) {
                end_game(); //End game
            }

            if (elapsedTimeMs % 1000 == 0) {
                display_live_time_and_score(); //Update display every second
            }
        } else {
            display_splash_screen(); //Display splash screen
        }
    }
}

//Test OLED functions
// int main(void) {
//     initI2C();          //Initialize i2c
//     ssd1306_init();        //Initialize OLED
//     ssd1306_clear();      //Clear display

//     ssd1306_set_cursor(0, 0); //Set cursor to top-left
//     ssd1306_print("Hello, World!"); //Print string

//     while (1) {
//         //Main loop
//     }
// }  

//Test Breakbeam 
// int main (void) {
//     isr_init();       //Initialize ISRs 
//     breakbeam_init(); //Initialize breakbeam
//     sei();            //Enable global interrupts
//     initI2C();          //Initialize i2c
//     ssd1306_init();        //Initialize OLED
//     ssd1306_clear();      //Clear display

//     while (1) {
//         if (PIND & (1 << PD2)) {
//             ssd1306_clear();
//             ssd1306_set_cursor(0,0);
//             ssd1306_print("Beam Unbroken ");
//         } else {
//             ssd1306_clear();
//             ssd1306_set_cursor(0,0);
//             ssd1306_print("Beam Broken   ");
//         }
//         _delay_ms(50);
//     }

// }
