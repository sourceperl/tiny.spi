/* 
  sample SPI Master software (using USI of ATTiny84)
  Test with nRF24L01+ modem
  debug serial : 9600,N,8,1
  code for avr-gcc
           ATTiny84 at 8 MHz (internal clock)
           fuses : Low 0xE2 High 0xDF Ext 0xFF

  code in public domain
*/

#define F_CPU 8000000UL
#include <avr/io.h>
#include <inttypes.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nRF24L01P.h"

/* ATTiny84 IO pins

             ___^___
+3,3v      -|VCC GND|- 0v
debug Tx   -|PB0 PA0|- 
LED        -|PB1 PA1|- 
RESET      -|PB3 PA2|- CE NRF (Tx/Rx)
           -|PB2 PA3|- CS NRF (SPI)
           -|PA7 PA4|- CLK (SPI)
MISO (SPI) -|PA6 PA5|- MOSI (SPI)
             -------
*/

#define TX        PB0
#define LED       PB1
#define SPI_MISO  PA6
#define SPI_MOSI  PA5
#define SPI_CLK   PA4
#define NRF_CS    PA3
#define _NRF_CS_L cbi(PORTA, NRF_CS);
#define _NRF_CS_H sbi(PORTA, NRF_CS);
#define NRF_CE    PA2
#define _NRF_CE_L cbi(PORTA, NRF_CE);
#define _NRF_CE_H sbi(PORTA, NRF_CE);
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit)) // clear bit
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))  // set bit
#define NRF_DEF_CONF (NRF_PWR_UP + NRF_EN_CRC)

/* prototypes */
// main routines
void setup(void);
void loop(void);
int main(void);
// misc routines
void serial_write(uint8_t tx_byte);
void serial_print(const char *str);
uint64_t millis(void);
uint8_t spi_transfer(uint8_t data);

/* some vars */
volatile uint64_t _millis    = 0;
volatile uint16_t _1000us    = 0;
uint64_t old_millis = 0;

// must be volatile (change and test in main and ISR)
volatile uint8_t tx_buzy = 0;
volatile uint8_t bit_index;
volatile uint8_t _tx_buffer; 


// compare match interrupt service for OCR0A
// call every 103us
ISR(TIM0_COMPA_vect) { 
  // software UART
  // send data
  if (tx_buzy) {
    if (bit_index == 0) {
      // start bit (= 0)
      cbi(PORTB, TX);
    } else if (bit_index <=8) {
      // LSB to MSB
      if (_tx_buffer & 1) {
        sbi(PORTB, TX);
      } else {
        cbi(PORTB, TX);
      }
      _tx_buffer >>= 1;        
    } else if (bit_index >= 9) {
      // stop bit (= 1)
      sbi(PORTB, TX);
      tx_buzy = 0;
    }
    bit_index++;
  }
  // millis update
  _1000us += 103;
  while (_1000us > 1000) {
    _millis++;
    _1000us -= 1000;
  }
}

// send serial data to software UART, block until UART buzy
void serial_write(uint8_t tx_byte) {
  while(tx_buzy);
  bit_index  = 0;
  _tx_buffer = tx_byte;
  tx_buzy = 1;
}

void serial_print(const char *str) {
  uint8_t i;
  for (i = 0; str[i] != 0; i++) {
    serial_write(str[i]);
  }
}

void serial_print_int(int value) {
  char buffer[9];
  itoa((int) value, &buffer[0], 16);
  serial_print(&buffer[0]);
}

// safe access to millis counter
uint64_t millis() {
  uint64_t m;
  cli();
  m = _millis;
  sei();
  return m;
}

/*
  Functions dealing with hardware USI specific jobs / settings
  it's a block function
*/
uint8_t spi_transfer(uint8_t data) {
  // data in transmit buffer
  USIDR = data;
  // clear overflow flag
  sbi(USISR, USIOIF);
  // while no overflow
  while ((USISR & _BV(USIOIF)) == 0) {
    // 3 wire mode | software clock via USITC
    USICR = (1<<USIWM0) | (1<<USICS1) | (1<<USICLK) | (1<<USITC);
  } 
  return USIDR;
}

/*** main routines ***/

void setup(void) {
  /* Port A */
  // LED IO
  sbi(DDRB,  LED); // set LED pin as output
  sbi(PORTB, LED); // turn the LED on
  // Software UART IO
  sbi(DDRB,  TX); // PB4 as output
  sbi(PORTB, TX); // serial idle level is '1'
  /* Port B */
  // SPI bus setup
  cbi(DDRA,  SPI_MISO); // MISO = input
  sbi(PORTA, SPI_MISO); // MISO = input with pullup on (DI)
  sbi(DDRA,  SPI_MOSI); // MOSI = output
  sbi(DDRA,  SPI_CLK);  // SCK  = output
  // nRF24L01+ (Chip enable activates Rx or Tx mode)
  sbi(DDRA,  NRF_CS);   // CS   = output
  sbi(DDRA,  NRF_CE);   // CE   = output
  
  /* interrup setup */
  // call ISR(TIM0_COMPA_vect) every 103us (for 9600 bauds)
  // set CTC mode : clear timer on comapre match
  // -> reset TCNTO (timer 0) when TCNTO == OCR0A
  sbi(TCCR0A, WGM01);
  // prescaler : clk/8 (1 tic = 1us for 8MHz clock)
  sbi(TCCR0B, CS01);
  // compare register A at 103 us
  OCR0A = 103;
  // interrupt on compare match OCROA == TCNT0
  sbi(TIMSK0, OCIE0A);
  // Enable global interrupts
  sei();
}

void loop(void) { 
  // every 100ms toggle LED
  if ((millis() - old_millis) > 2000) {
   // Toggle Port B pin 3 output state
   PORTB ^= 1<<LED;
   old_millis = millis();
   //serial_print("toggle LED\r\n");
  }
  // SPI
  uint16_t i;
  uint8_t nrfStatus;
  uint8_t addr[5];

  // SPI : clear MAX_RT flag in STATUS register
  serial_print("reset MAX_RT\r\n");
  _NRF_CS_L; 
  spi_transfer(NRF_W_REGISTER | NRF_STATUS);
  spi_transfer(0x10);
  _NRF_CS_H; 

  // SPI : flush Tx buffer 
  serial_print("flush Tx\r\n");
  _NRF_CS_L; 
  nrfStatus = spi_transfer(NRF_FLUSH_TX);
  _NRF_CS_H;

  // SPI : RF_CH = 2
  serial_print("RF_CH = 0x40\r\n");
  _NRF_CS_L; 
  spi_transfer(NRF_W_REGISTER | NRF_RF_CH);
  spi_transfer(0x40);
  _NRF_CS_H; 

  // SPI : read NRF Addr P0 
  _NRF_CS_L; 
  nrfStatus = spi_transfer(NRF_NOP);
  _NRF_CS_H;

  serial_print("read status 0x");
  serial_print_int(nrfStatus);
  serial_print("\r\n");

  // SPI : read NRF Tx Addr and display it
  _NRF_CS_L; 
  spi_transfer(NRF_R_REGISTER | NRF_TX_ADDR);
  for (i=0; i < 5; i++) 
    addr[i] = spi_transfer(NRF_NOP);
  _NRF_CS_H; 
    
  serial_print("read Tx AD 0x");
  for (i=0; i < 5; i++) {
    serial_print_int(addr[i]);
  }
  serial_print("\r\n");

  // SPI : write NRF default config : POWER_UP and EN_CRC / PWR_RX disable
  _NRF_CS_L; 
  spi_transfer(NRF_W_REGISTER | NRF_CONFIG);
  spi_transfer(NRF_DEF_CONF);
  _NRF_CS_H; 
   
  // SPI : write NRF Tx Addr
  _NRF_CS_L; 
  spi_transfer(NRF_W_REGISTER | NRF_TX_ADDR);
  spi_transfer(0xB3);
  spi_transfer(0xB3);
  spi_transfer(0xB3);
  spi_transfer(0xB3);
  spi_transfer(0xB3);
  _NRF_CS_H; 

  // SPI : write NRF Rx Addr P0 for auto ACK
  _NRF_CS_L; 
  spi_transfer(NRF_W_REGISTER | NRF_RX_ADDR_P0);
  spi_transfer(0xB3);
  spi_transfer(0xB3);
  spi_transfer(0xB3);
  spi_transfer(0xB3);
  spi_transfer(0xB3);
  _NRF_CS_H; 

  // SPI : write NRF Tx payload (16 bytes wide)
  _NRF_CS_L; 
  spi_transfer(NRF_W_REGISTER | NRF_W_TX_PAYLOAD);
  for (i=0; i < 16; i++) 
    spi_transfer('A');
  _NRF_CS_H; 

  // NRF CE pulse : for Tx data
  serial_print("CE pulse\r\n");
  _NRF_CE_H; 
  // wait data is transmit (loop until TX_DS and MAX_RT == 0)
  // TODO here it could be better to use IRQ from the chip 
  i = 0;
  do {
    i++;
    _delay_us(15); 
    _NRF_CS_L; 
    nrfStatus = spi_transfer(NRF_NOP);
    _NRF_CS_H;
  } while ((nrfStatus & 0x30) == 0);
  _NRF_CE_L;

  serial_print("boucle: ");
  serial_print_int(i);
  serial_print("\r\n");
  
  serial_print("status 0x");
  serial_print_int(nrfStatus);
  serial_print("\r\n");

  serial_print("\r\n");
  serial_print("\r\n");
  _delay_ms(5000);
}

/*
  Arduino like
*/
int main(void) {
  setup();
  for(;;) {
    loop();
  }
};
