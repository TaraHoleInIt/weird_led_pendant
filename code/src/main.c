#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <util/delay.h>

#define NumAnimations 1
#define NumLEDs 6

#define LEDPORT PORTB
#define LEDDIR DDRB

#define A 0x01
#define B 0x02
#define C 0x04

#define MakeLED( anode, cathode ) { .anodePin = anode, .cathodePin = cathode }
#define ArraySize( a ) ( sizeof( a ) / sizeof( a[ 0 ] ) )
#define Bit( n ) ( 1 << n )

typedef struct {
    uint8_t anodePin : 4;
    uint8_t cathodePin : 4;
} LEDPin;

void initPeripherals( void );
void handleResetButton( void );
void goToSleep( void );
void turnOnLED( uint8_t ledNo );
void turnOffLEDs( void );
void animPulse( void );

const LEDPin ledDefs[ ] PROGMEM = {
    MakeLED( A, B ),    // LED0
    MakeLED( B, A ),    // LED1
    MakeLED( A, C ),    // LED2
    MakeLED( C, A ),    // LED3
    MakeLED( B, C ),    // LED4
    MakeLED( C, B )     // LED5
};

const uint8_t brightnessMasks[ 9 ] PROGMEM = {
    0b00000000,
    0b00000001,
    0b00000011,
    0b00000111,
    0b00001111,
    0b00011111,
    0b00111111,
    0b01111111,
    0b11111111
};

volatile uint8_t animationNo __attribute__( ( section( ".noinit" ) ) );
volatile uint8_t ledValues[ ] = { 0, 0, 0, 0, 0, 0 };
volatile uint32_t tickCount = 0;

int main( void ) {
    handleResetButton( );
    initPeripherals( );

    while ( 1 ) {
        goToSleep( );
    }

    return 0;
}

void initPeripherals( void ) {
    cli( );
        CCP = 0xD8; // Write protection register
        CLKPSR = 0; // CLK / 1 == 8MHz

        DIDR0 = Bit( 0 ) | Bit( 1 ) | Bit( 2 ); // Turn off digital input buffers
        PRR = Bit( PRADC );                     // Turn off ADC

        // Setup ~10KHz timer and enable timer compare interrupt
        // F = 8MHz
        // S = 1 (No prescaling)
        //
        // 10,000Hz =         F
        //             ---------------   ==   399
        //              2 * S * (1+x)
        TIMSK0 = Bit( OCIE0A );                 // Compare interrupt A
        TCCR0B = Bit( CS00 ) | Bit( WGM02 );    // CTC, No clock prescaling
        OCR0A = 399;
    sei( );
}

// ref: https://sites.google.com/site/wayneholder/using-the-avr-reset-pin-as-an-input
void handleResetButton( void ) {
    if ( bit_is_set( RSTFLR, PORF ) ) {         // Cold boot
        // Clear power on reset flag
        RSTFLR &= ~Bit( PORF );

        animationNo = 0;
    } else if ( bit_is_set( RSTFLR, EXTRF ) ) { // Warm boot
        // Clear external reset flag
        RSTFLR &= ~_BV( EXTRF );

        animationNo++;
        animationNo = ( animationNo >= NumAnimations ) ? 0 : animationNo;
    }
}

void goToSleep( void ) {
    SMCR = Bit( SE );
        asm volatile( "sleep" );
    SMCR = 0;
}

void turnOnLED( uint8_t ledNo ) {
    const LEDPin* led = &ledDefs[ ledNo ];

    LEDDIR = 0;
    LEDPORT = 0;

    LEDPORT = led->anodePin;
    LEDDIR = led->anodePin | led->cathodePin;
}

void turnOffLEDs( void ) {
    LEDDIR = 0;
    LEDPORT = 0;
}

ISR( TIM0_COMPA_vect ) {
    static uint8_t curLED = 0;
    static uint8_t curBit = 0;

    if ( brightnessMasks[ ledValues[ curLED ] ] & Bit( curBit ) )
        turnOnLED( curLED );
    else
        turnOffLEDs( );

    if ( ++curBit >= ArraySize( brightnessMasks ) ) {
        curLED++;
        curLED = ( curLED >= NumLEDs ) ? 0 : curLED;

        curBit = 0;
    }

    tickCount++;

    // TARA:
    // Please make this make sense later on
    if ( tickCount >= ( 15 * 50 ) ) {
        tickCount = 0;
        animPulse( );
    }
}

void animPulse( void ) {
    static uint8_t data[ ] = { 
        0, 0, 0, 
        0, 0, 0, 
        1, 1, 1, 
        2, 2, 2, 
        3, 3, 3, 
        4, 4, 4, 
        5, 5, 5, 
        6, 6, 6, 
        7, 7, 7, 
        8, 8, 8, 
        7, 7, 7, 
        6, 6, 6, 
        5, 5, 5, 
        4, 4, 4, 
        3, 3, 3, 
        2, 2, 2, 
        1, 1, 1, 
        0, 0, 0 
    };
    const int frames = ArraySize( data );
    static uint8_t i = 0;
    int8_t a = 0;
    int8_t b = 0;
    int8_t c = 0;

    a = ( i >= frames ) ? 0 : i;

    i++;
    i = ( i >= frames ) ? 0 : i;

    b = a + 1;
    b = ( b >= frames ) ? 0 : b;

    c = b + 1;
    c = ( c >= frames ) ? 0 : c;
    
    ledValues[ 5 ] = ledValues[ 4 ] = data[ a ];
    ledValues[ 3 ] = ledValues[ 2 ] = data[ b ];
    ledValues[ 1 ] = ledValues[ 0 ] = data[ c ];
}
