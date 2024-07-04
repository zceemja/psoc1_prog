

// #if DEVICE == atmega168

#include <Arduino.h>
#include <avr/io.h>

#define SCL_PIN BIT0
#define RES_PIN BIT3
#define SDA_PIN BIT4
#define LED_PIN BIT5

#define DELAY for(i=0;i<1;i++);

#include "devices.h"

#define _CLK1 PORTB|= SCL_PIN;DELAY
#define _CLK0 PORTB&=~SCL_PIN;DELAY


volatile uint16_t i;
uint8_t read_buf[2];
uint16_t dev_id = 0;

uint8_t ram_blk[64];

// firmware version
const uint16_t fw = 0x0002;


void data_send(const uint8_t *data, uint16_t len) {
  uint16_t k;
  uint8_t prev = 0;
  uint8_t curr = 1;

  // PORTB |= SDA_PIN;  // Always starts with high
  // PORTB &= ~SCL_PIN; // Make sure clock is low
  DDRB  |= SDA_PIN;  // Making sure sdata is output
  PORTB |= SDA_PIN | LED_PIN;
  for (k = 0;k < len;k++) {
    curr = (data[k/8] & BIT7 >> k%8) == 0;
    // PORTB |= SCL_PIN; // Set clock high

    if (curr != prev) {
      prev = curr;
      PORTB ^= SDA_PIN | SCL_PIN;
    }
    else
      PORTB ^= SCL_PIN;
    DELAY;
    // // if binary 0
    //   P1OUT |= BIT3; // Set clock high
      
    //   //uart_print("0", 1);
    // } else {
    // // if binary 1
    //   P1OUT |= BIT5; // Set sdata high
    //   P1OUT |= BIT3; // Set clock high, keeping timing consistent
    //   prev = 1;
    //   //uart_print("1", 1);
    // }
    
    // P1OUT ^= BIT0;
    
    //for(i=0;i<delay;i++); //delay
    // P1OUT ^= BIT0;
    PORTB ^= SCL_PIN; // Set clock low
    DELAY;
    //for(i=0;i<delay;i++); //delay
  }
  //P1OUT &= ~BIT6;
}

void wait_sdata_low() {
  PORTB &= ~(LED_PIN | SDA_PIN); // Set to high-Z
  DDRB &= ~SDA_PIN; // Setting sdata to read
  while((PINB & SDA_PIN) != 0); // wait until pin goes low

}

void data_pull() {
  uint8_t j;

  PORTB &= ~(LED_PIN | SDA_PIN); // Set to high-Z
  DDRB &= ~SDA_PIN; // Setting sdata to read
  while((PINB & SDA_PIN) != 0); // wait until pin goes low

  // One clock
  _CLK1;
  _CLK0;

  while((PINB & SDA_PIN) == 0); // wait until pin goes high
  while((PINB & SDA_PIN) != 0); // wait until pin goes low
  // This might take up to 100ms. Perhaps should check if it takes longer
  for(j=0;j<40;j++) {
    // Cycle clock 40 as per specification
    _CLK1;
    _CLK0;
  }
  // Setting config back to write mode
  // DDRB &= ~SDA_PIN; 
  // PORTB |= SDA_PIN | LED_PIN;
}

void data_read(uint8_t *buf, uint16_t size) {
  PORTB &= ~(SDA_PIN | LED_PIN);
  DDRB &= ~SDA_PIN; // Setting sdata to read

  uint8_t k;
  // Cycle clock
  _CLK1;
  _CLK0;
  // _CLK1;
  // _CLK0;

  for(k=0;k<size;k++) {
    // Init byte to zero
    _CLK1;
    _CLK0;
    if(k%8==0) buf[k/8] = 0;
    if((PINB & SDA_PIN) != 0) buf[k/8] |= (BIT7 >> k%8);
    
  }
  _CLK1;
  _CLK0;
}

uint8_t read_byte(uint8_t cmd) {
  uint8_t buf[] = {0b10100000 | (cmd >> 3), 0};
  // buf[0] &= cmd >> 3;
  buf[1] = cmd << 5;
  data_send(buf, 11);
  data_read(&buf[1], 8);
  data_send(buf, 1); // Send 1
  return buf[1];
}

void reset_device() {
  PORTB |= RES_PIN;
  for(i=0;i<500;i++);
  PORTB &= ~RES_PIN;
}

void initialise() {
  DDRB |= SCL_PIN | SDA_PIN | LED_PIN | RES_PIN;
  reset_device();
  data_send(init1, init1_len);
  data_pull();
  data_send(init2, init2_len);
  data_pull();
  data_send(init3_5V, init3_len);
}

uint16_t read_id() {
  data_send(id_setup, id_setup_len);
  data_pull();
  uint16_t result = 0;
  result = read_byte(0b11111000) << 8;
  result |= read_byte(0b11111001);
  return result;
}

uint16_t read_checksum(uint16_t dev_id) {
  if(dev_id == dev_CY8C24123A || dev_id == dev_CY8C24223A || dev_id == dev_CY8C24423A)
    data_send(checksum_1, checksum_len);
  else 
    data_send(checksum_0, checksum_len);
  data_pull();
  uint16_t result = 0;
  result = read_byte(0b11111001) << 8;
  result |= read_byte(0b11111000);
  return result;
}

void erase_all_memory() {
  data_send(bulk_erase, bulk_erase_len);
  data_pull();
}

void write_block(uint16_t dev_id, uint8_t block) {
  uint8_t payload[3];
  for(uint8_t ADDR = 0; ADDR < 64; ADDR++) {
    // WRITE BYTE
    uint8_t data = ram_blk[ADDR];
    payload[0] = 0b10010000 | (ADDR >> 3);
    payload[1] = (ADDR << 5) | (data >> 3);
    payload[2] = (data << 5) | 0b00011100;
    // 1001 0aaa  aaad dddd  ddd1 11xx
    data_send(payload, 22);
  }
  payload[0] = 0b10011111;
  payload[1] = 0b01000000 | (block >> 3);
  payload[2] = 0b00011100 | (block << 5);
  // 1001 1111  010d dddd  ddd1 11xx
  data_send(payload, 22);

  if(dev_id >= dev_CY8C27143 && dev_id <= dev_CY8C27643)
    data_send(program_block_1, program_block_len);
  else
    data_send(program_block_0, program_block_len);
  data_pull();
}

void write_secure() {
  uint8_t payload[3];
  for(uint8_t ADDR = 0; ADDR < 64; ADDR++) {
    // WRITE BYTE
    uint8_t data = ram_blk[ADDR];
    payload[0] = 0b10010000 | (ADDR >> 3);
    payload[1] = (ADDR << 5) | (data >> 3);
    payload[2] = (data << 5) | 0b00011100;
    // 1001 0aaa  aaad dddd  ddd1 11xx
    data_send(payload, 22);
  }
  
  data_send(secure, secure_len);
  data_pull();
}

void read_block(uint8_t block, uint8_t offset) {
  uint8_t payload[3];

  // SET-BLOCK-NUM
  payload[0] = 0b10011111;
  payload[1] = 0b01000000 | (block >> 3);
  payload[2] = 0b00011100 | (block << 5);
  // 1001 1111  010d dddd  ddd1 11xx
  data_send(payload, 22);
  data_send(verify_setup, verify_setup_len);
  data_pull();
    
  for(uint8_t ADDR = 0; ADDR < 64; ADDR++) {
    ram_blk[ADDR] = read_byte(ADDR + offset);
  }
}

void write_program(uint16_t dev_id, uint16_t blocks) {
  uint8_t payload[3];
  erase_all_memory();
  for(uint16_t BLK_NUM = 0; BLK_NUM < blocks; BLK_NUM++) {
    Serial.readBytes(ram_blk, 64);
    for(uint8_t ADDR = 0; ADDR < 64; ADDR++) {
      // WRITE BYTE
      uint8_t data = ram_blk[ADDR];
      payload[0] = 0b10010000 | (ADDR >> 3);
      payload[1] = (ADDR << 5) | (data >> 3);
      payload[2] = (data << 5) | 0b00011100;
      // 1001 0aaa  aaad dddd  ddd1 11xx
      data_send(payload, 22);
    }
    Serial.write(BLK_NUM);
    // SET-BLOCK-NUM
    payload[0] = 0b10011111;
    payload[1] = 0b01000000 | (BLK_NUM >> 3);
    payload[2] = 0b00011100 | (BLK_NUM << 5);
    // 1001 1111  010d dddd  ddd1 11xx
    data_send(payload, 22);

    if(dev_id >= dev_CY8C27143 && dev_id <= dev_CY8C27643)
      data_send(program_block_1, program_block_len);
    else
      data_send(program_block_0, program_block_len);
    data_pull();
  }

}

void read_program(uint16_t blocks, uint8_t offset) {
  uint8_t payload[3];
  uint8_t data;

  for(uint16_t BLK_NUM = 0; BLK_NUM < blocks; BLK_NUM++) {
    // SET-BLOCK-NUM
    payload[0] = 0b10011111;
    payload[1] = 0b01000000 | (BLK_NUM >> 3);
    payload[2] = 0b00011100 | (BLK_NUM << 5);
    // 1001 1111  010d dddd  ddd1 11xx
    data_send(payload, 22);
    data_send(verify_setup, verify_setup_len);
    data_pull();
    
    for(uint8_t ADDR = 0; ADDR < 64; ADDR++) {
      ram_blk[ADDR] = read_byte(ADDR + offset);
    }
    Serial.write(ram_blk, 64);
    Serial.write(BLK_NUM);
  }
}

void setup(void) {
  Serial.begin(9600);
  initialise();
  dev_id = read_id();
  Serial.print("> ");
}

void loop() {
  uint8_t command;
  if(Serial.readBytes(&command, 1) > 0) {
    if(command == 'd') {
      Serial.println(dev_id, HEX); // get device ID
    }
    else if(command == 'D') {  // get device ID (in binary)
      Serial.write(dev_id);
      Serial.write(dev_id >> 8);
      Serial.println(); 
    }
    else if(command == 'i') {  // reinitialise
      initialise();
      dev_id = read_id();
      Serial.println();
    }
    else if(command == 'r') {  // reset_device
      uint8_t args[2] = {0x00, 0x80};
      Serial.readBytes(args, 2);
      read_block(args[0], args[1]);
      Serial.println();
    }
    else if(command == 'e') {  // erase_memory
      erase_all_memory();
      Serial.println();
    }
    else if(command == 'w') {  // write ram block to device with next provided byte being device memory offset
      // Write block
      if(Serial.readBytes(&command, 1) > 0) {
        write_block(dev_id, command);
      }
      Serial.println();
    }
    else if(command == 'f') {   // return this firmware identification
      Serial.println(fw, HEX);
    }
    else if(command == 'F') {   // return this firmware identification (in binary)
      Serial.write(fw);
      Serial.write(fw >> 8);
      Serial.println();
    }
    else if(command == 'c') {   // Return checksum
      Serial.println(read_checksum(dev_id), HEX);
    }
    else if(command == 'C') {   // Return checksum (in binary)
      uint16_t devid = read_checksum(dev_id);
      Serial.write(devid);
      Serial.write(devid >> 8);
      Serial.println();
    }
    else if(command == 's') {    // Read ram block
      Serial.write(ram_blk, 64);
      Serial.println();
    }
    else if(command == 't') {    // Write to ram block
      Serial.readBytes(ram_blk, 64);
      Serial.println();
    }
    else if(command == 'x') {    // Write secure memory
      write_secure();
      Serial.println();
    }
    else if(command == 'a') {    // Reset
      reset_device();
      Serial.println();
    }
    else if(command == '\n') {
      Serial.println();
    }
    else {
      Serial.println("invalid");
    }
    Serial.print("> ");
  }
}
