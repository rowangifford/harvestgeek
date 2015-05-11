/*****************
 * Libraries used:
 * - nrf24  from http://airspayce.com/mikem/arduino/NRF24/
 * - hvg_comms from http://harvestgeek.com
 * - hvg_disp http://harvestgeek.com
 * - TWI - Arduino Two wire I2C driver AA
 ******************/
#include <EEPROM.h>
#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include <hvg_disp.h>
#include <hvg_comms.h>
#include <NRF24.h>

#include <avr/wdt.h>

// WDT disable function, disables WDT on POR
void wdt_init(void) __attribute__((naked)) __attribute__((section(".init3")));

void wdt_init(void)
{
   MCUSR = 0;
   wdt_disable();

   return;
}

// Software reset macro, enables WDT
#define soft_reset()        \
do                          \
{                           \
   wdt_enable(WDTO_15MS);  \
   for(;;)                 \
   {                       \
   }                       \
} while(0)

/**************************  */
/*     DEBUG DEFINES       */
/***************************/
#define DATA_OUTPUT
#define DBG_OUTPUT
// Add a 4s tempo at boot to get dhcp debug
//#define DEL_BOOT
/***************************/
/*     TYPEDEFS            */
/***************************/
// Packets sent over the air, SAME LENGTH
typedef struct _nrf_generic_packet
{
  unsigned char platform_id;
  unsigned char message_type;
  unsigned char data[12];
  unsigned short crc;
}
nrf_generic_packet_t, *p_nrf_generic_packet_t;

typedef struct _nrf_report_packet
{
  unsigned char platform_id;
  unsigned char message_type;
  unsigned short temperature;
  unsigned short humidity;
  unsigned short temp_ground;
  unsigned short moisture;
  unsigned short lux;
  unsigned short ir;
  unsigned short crc;
}
nrf_report_packet_t, *p_nrf_report_packet_t;

/***************************/
/*  STATE MACHINE DEFINES  */
/***************************/
#define NRF_INIT        (1 << 0)
#define PID_INIT        (1 << 3)

/***************************/
/* DISP STATE MACHINE DEF  */
/***************************/
#define DISP_ETH_PB     0
#define DISP_DHCP_PB    1
#define DISP_INIT       2
#define DISP_INIT_DONE  3
#define DISP_NO_CON     4
#define DISP_PB_COMS    5
#define DISP_NREG       6
#define DISP_COLLECT    7
#define DISP_NRF_PB     8
#define DISP_NO_HVG     11
#define DISP_RESET      12

/***************************/
/*       DEFINES           */
/***************************/
// NRF packet payload
#define PACKET_PAYLOAD  sizeof(nrf_generic_packet_t)
// Timeout to consider we're not hearing a station
#define TIMEOUT_PLAT_S  15
// NRF message actual payload
#define PACKET_DATAL    12
// Time between two data posts
#define SENDING_PERIOD  60
// Time between two surveys
#define SURVEY_PERIOD   10
// Current NRF channel
#define NRF_CHANNEL     82

/***************************/
/*    NRF PACKET TYPES     */
/***************************/
#define REPORT_TYPE     0
#define IDREQ_TYPE      1
#define IDREP_TYPE      2
#define SURVEY_TYPE     3

/***************************/
/*       EEPROM            */
/***************************/
// EEPROM: boot key addr
#define INIT_ADDR       0
#define INIT_ADDR2      1
// EEPROM: last given PID
#define LAST_PID_ADDR   2

// Added RG 25/06/2014
// EEPROM: PID addr
#define PID_ADDR        0
// EEPROM: our GUID info
#define GUID_INIT_ADDR  9
#define GUID_REGED_ADDR 10
#define GUID_START_ADDR 11
// EEPROM: boot key value
#define INIT_UID        0x77

/***************************/
/*          PINS           */
/***************************/
#define CSN_PIN         13
#define CE_PIN          5
#define LCD_RST         0      // Changed from 2 RG 25/06/2014, also removed LCD_A0 and LCD_CS
#define ETH_RST         12     // Changed from 4 RG 25/06/2014
#define BUTTON_PIN      4      // Added by RG 25/06/2014

/***************************/
/*       Global Vars       */
/***************************/
// Harvestgeek communications library
HVGCOMMS hvg_comms(GUID_INIT_ADDR, GUID_REGED_ADDR, GUID_START_ADDR);
// LCD library - RG 25/06/2014
HVGDISP hvg_disp;
// Our platform second counter
volatile unsigned short sec_counter = 0;
// Our platform log, for later sending
message_log_t mess_log[MAX_PLATFORMS];
// Our display state machine
unsigned char display_sm = DISP_INIT;
// Our program state machine for status
unsigned char state_machine = 0;
// Last token for PID request we received
unsigned char last_pid_token = 0;
// Buffer packet for NRF communication
nrf_generic_packet_t nrf_packet;
// NRF24 radio library
NRF24 nrf24(CE_PIN, CSN_PIN);
// Bool for survey send
bool survey_sent;
// Added RG 25/06/2014
// Conseq seconds the button has ben pressed,
volatile unsigned char button_counter = 0;
// Our platform ID
unsigned char platform_id = 0;
// Temp ID token for id request
unsigned char temp_token;
// A temporary second couter
volatile unsigned char temp_sec_counter = 0;
// Flag for GUID reset button
volatile bool RESET_FLAG = false;

// 1Hz interrupt
ISR(TIMER3_COMPA_vect)
{
  unsigned char i;

  // Added RG 25/06/2014
  temp_sec_counter++;

  // Check button and increment counter if necessary
  if (digitalRead(BUTTON_PIN) == LOW) button_counter++;
  else button_counter = 0;

  if (button_counter >= 4) RESET_FLAG = true;
  
  // Increment platform reports counters
  for (i = 0; i < MAX_PLATFORMS; i++)
  {
    if (mess_log[i].last_mess_sec != 0xFF)
      mess_log[i].last_mess_sec++;
  }
  sec_counter++;
}

unsigned char get_string_to_display(char id, char* buffer)
{
  if (display_sm == DISP_ETH_PB)
    strcpy_P(buffer, PSTR("Unable to obtain IP address, DHCP failed\0"));
  else if (display_sm == DISP_DHCP_PB)
    strcpy_P(buffer, PSTR("Unable to obtain IP address, DHCP failed\0"));
  else if (display_sm == DISP_INIT)
    strcpy_P(buffer, PSTR("Initializing...\0"));
  else if (display_sm == DISP_INIT_DONE)
    strcpy_P(buffer, PSTR("Initialization done!\0"));
  else if (display_sm == DISP_NO_CON)
    strcpy_P(buffer, PSTR("No internet\0"));
  else if (display_sm == DISP_NO_HVG)
    strcpy_P(buffer, PSTR("Error registering.  Email michael@      harvestgeek.com\0"));
  else if (display_sm == DISP_PB_COMS)
    strcpy_P(buffer, PSTR("W5100 problem\0"));
  else if (display_sm == DISP_NREG)
    strcpy_P(buffer, PSTR("Register your devicewww.harvestgeek.com /onboarding/guid\0"));
  else if (display_sm == DISP_COLLECT)
    strcpy_P(buffer, PSTR("Harvestgeek\0"));
  else if (display_sm == DISP_NRF_PB)
    strcpy_P(buffer, PSTR("NRF24 problem\0"));
  else if (display_sm == DISP_RESET)
    strcpy_P(buffer, PSTR("GUID Reset..\0"));
    
  // R.Gifford 24/06/2014
  // Return x position to center the string in buffer
  return (hvg_disp.centerString(buffer));
}

void draw(void)
{
  unsigned char i, j;

  // Edited RG 24/06/2014
  
  // Display "register your device" + guid
    if (display_sm == DISP_NREG)
  {
    i = get_string_to_display(display_sm, (char*)hvg_comms.hvg_comms_buffer);
    hvg_disp.setPrintPos(i, 0);
    hvg_disp.print((char*)hvg_comms.hvg_comms_buffer);
    
    // Delay 5s
    delay(10000);
    
    hvg_disp.clearDisplay();
    
    // Displays GUID (over two lines)
    hvg_comms.convert_hex_to_hexstring(hvg_comms.guid, (char*)hvg_comms.hvg_comms_buffer, GUID_LGTH/2);
    hvg_disp.setPrintPos(hvg_disp.centerString((char*)hvg_comms.hvg_comms_buffer), 1);
    hvg_disp.print((char*)hvg_comms.hvg_comms_buffer);

    hvg_comms.convert_hex_to_hexstring(&hvg_comms.guid[(GUID_LGTH/2)-1], (char*)hvg_comms.hvg_comms_buffer, GUID_LGTH/2);
    hvg_disp.setPrintPos(hvg_disp.centerString((char*)hvg_comms.hvg_comms_buffer), 2);
    hvg_disp.print((char*)hvg_comms.hvg_comms_buffer);
    
    // Delay 20s
    delay(20000);
  }
  else if (display_sm == DISP_ETH_PB)
  {
    i = get_string_to_display(display_sm, (char*)hvg_comms.hvg_comms_buffer);
    hvg_disp.setPrintPos(i, 1);
    hvg_disp.print((char*)hvg_comms.hvg_comms_buffer);
  }
  else if (display_sm == DISP_COLLECT)
  {
    // First line: Harvestgeek
    i = get_string_to_display(display_sm, (char*)hvg_comms.hvg_comms_buffer);
    hvg_disp.setPrintPos(i, 0);
    hvg_disp.print((char*)hvg_comms.hvg_comms_buffer);
    // Second line: number of platforms connected
    j = 0;
    for (i = 0; i < MAX_PLATFORMS; i++)
    {
      // We are at the end of the list
      if (mess_log[i].platform_id == NO_ID_PID)
        break;
      // If we received recent data from this platform
      if (mess_log[i].last_mess_sec < TIMEOUT_PLAT_REPORT)
        j++;
    }
    hvg_comms.int_to_string((uint16_t)j, (char*)hvg_comms.hvg_comms_buffer);
    i = hvg_comms.hm_strlen((char*)hvg_comms.hvg_comms_buffer);
    hvg_comms.hvg_comms_buffer[i++] = ' ';
    if (j > 1)
      strcpy_P((char*)(hvg_comms.hvg_comms_buffer + i), PSTR("platforms heard"));
    else
      strcpy_P((char*)(hvg_comms.hvg_comms_buffer + i), PSTR("platform heard"));
    hvg_disp.setPrintPos(0, 1);
    hvg_disp.print((char*)hvg_comms.hvg_comms_buffer);
  }
  else if (display_sm == DISP_NO_HVG)
  {
    // Otherwise just display the message
    i = get_string_to_display(display_sm, (char*)hvg_comms.hvg_comms_buffer);
    hvg_comms.hvg_comms_buffer[33] = 0xA0; // @ symbol
    hvg_disp.setPrintPos(i, 0);
    hvg_disp.print((char*)hvg_comms.hvg_comms_buffer);
  }
  else
  {
    // Otherwise just display the message
    i = get_string_to_display(display_sm, (char*)hvg_comms.hvg_comms_buffer);
    hvg_disp.setPrintPos(i, 0);
    hvg_disp.print((char*)hvg_comms.hvg_comms_buffer);
  }
  
  // Clear buffer
  for (i=0;i<hvgcommsBufferSize;i++) hvg_comms.hvg_comms_buffer[i] = 0x00; 
}

// R.Gifford 24/6/14
void disp_loop(void)
{
  hvg_disp.clearDisplay();
  draw();
}


// Give a PID to a new platform
unsigned char get_new_pid()
{
  unsigned char return_value;

  // Get next available PID in EEPROM and increment it
  return_value = EEPROM.read(LAST_PID_ADDR);
  EEPROM.write(LAST_PID_ADDR, return_value + 1);
  return return_value;
}

// Store a report in our memory
void store_report_message(void)
{
  nrf_report_packet_t* nrf_report_packet = (nrf_report_packet_t*)&nrf_packet;
  unsigned char i;

  // Find the index to store the message
  for (i = 0; i < MAX_PLATFORMS; i++)
  {
    if (nrf_report_packet->platform_id == mess_log[i].platform_id)
      break;
  }

  // If we didn't find the index, fill an empty slot
  if (i == MAX_PLATFORMS)
  {
    for (i = 0; i < MAX_PLATFORMS; i++)
    {
      if (mess_log[i].platform_id == NO_ID_PID)
        break;
    }
  }

  // then store the message at the correct index
  mess_log[i].platform_id = nrf_report_packet->platform_id;
  mess_log[i].last_mess_sec = 0;
  mess_log[i].temperature = nrf_report_packet->temperature;
  mess_log[i].humidity = nrf_report_packet->humidity;
  mess_log[i].temp_ground = nrf_report_packet->temp_ground;
  mess_log[i].moisture = nrf_report_packet->moisture;
  mess_log[i].lux = nrf_report_packet->lux;
  mess_log[i].ir = nrf_report_packet->ir;
}

// Send the NRF packet, returns true or false
unsigned char send_nrf_packet(unsigned char nb_resend)
{
  nrf_generic_packet_t* packet_tbs = &nrf_packet;
  unsigned char i, j;

  for (i = 0; i < nb_resend; i++)
  {
    packet_tbs->crc = packet_tbs->platform_id;
    packet_tbs->crc += packet_tbs->message_type;
    for (j = 0; j < PACKET_DATAL; j++)
      packet_tbs->crc += packet_tbs->data[j];

    if (!nrf24.setTransmitAddress((uint8_t*)"Hsens", 5))
    {
#ifdef DBG_OUTPUT
      if (Serial)Serial.println(F("setTransmitAddress failed\r"));
#endif
      return false;
    }
    else
    {
      if (!nrf24.send((uint8_t*)packet_tbs, PACKET_PAYLOAD, true))
      {
#ifdef DBG_OUTPUT
        if (Serial)Serial.println(F("send failed\r"));
#endif
        return false;
      }
      else
      {
        if (!nrf24.waitPacketSent())
        {
#ifdef DBG_OUTPUT
          if (Serial)Serial.println(F("waitPacketSent failed\r"));
#endif
          return false;
        }
        else
        {
#ifdef DBG_OUTPUT
          if (Serial)Serial.println(F("Packet sent\r"));
#endif
        }
      }
    }
  }
  return true;
}

// Send a survey packet, sending PIDs of the platforms that sent data
void send_survey_packet(void)
{
  nrf_generic_packet_t* packet_ptr = &nrf_packet;
  unsigned char i = 0, j = 0;

  packet_ptr->platform_id = NO_ID_PID;
  packet_ptr->message_type = SURVEY_TYPE;

  for (i = 0; i < MAX_PLATFORMS; i++)
  {
    // We are at the end of the list
    if (mess_log[i].platform_id == NO_ID_PID)
      break;
    // If we received recent data from this platform
    if (mess_log[i].last_mess_sec < TIMEOUT_PLAT_S)
    {
      packet_ptr->data[j] = mess_log[i].platform_id;
      j++;
    }
    // If our packet is full
    if (j == PACKET_DATAL)
    {
      send_nrf_packet(3);
      j = 0;
    }
  }

  // Send the remaining PIDs
  for (i = 0; i < (PACKET_DATAL - j); i++)
    packet_ptr->data[j + i] = NO_ID_PID;
  send_nrf_packet(3);
}

void setup()
{
  unsigned char return_hvg_comms;
  unsigned char i;
  RESET_FLAG = false;
  
  /** Start USB CDC **/
  Serial.begin(9600);
  
  // Added RG 25/06/2014
  // Define button as input and set high
  pinMode(BUTTON_PIN, INPUT);
  digitalWrite(BUTTON_PIN, HIGH);
  // Define LCD_RST as output and set high
  pinMode(LCD_RST, OUTPUT);
  digitalWrite(LCD_RST, HIGH);
  delay(10);
  digitalWrite(LCD_RST, LOW);
  delay(50);
  digitalWrite(LCD_RST, HIGH);
  delay(100);

  hvg_disp.initDisplay();

  /** Display init screen **/
  display_sm = DISP_INIT;
  disp_loop();  // R.Gifford 24/6/2014

  /**  NRF24L01+ initialization **/
  state_machine |= NRF_INIT;
  if (!nrf24.init())
    state_machine &= ~NRF_INIT;
  if (!nrf24.setChannel(NRF_CHANNEL))
    state_machine &= ~NRF_INIT;
  if (!nrf24.setThisAddress((uint8_t*)"Hbase", 5))
    state_machine &= ~NRF_INIT;
  if (!nrf24.setPayloadSize(PACKET_PAYLOAD))
    state_machine &= ~NRF_INIT;
  if (!nrf24.setRF(NRF24::NRF24DataRate250kbps, NRF24::NRF24TransmitPower0dBm))
    state_machine &= ~NRF_INIT;
  // Enable the EN_DYN_ACK feature so we can use noack
  nrf24.spiWriteRegister(NRF24_REG_1D_FEATURE, NRF24_EN_DYN_ACK);
  if ((state_machine & NRF_INIT) != NRF_INIT)
  {
    display_sm = DISP_NRF_PB;
    disp_loop();  // R.Gifford 24/6/2014
    while (1);
  }
  
  /** 1Hz Timer 1 counter initialization **/
  cli();
  TCCR3A = 0;
  TCCR3B = 0;
  // set compare match register to desired timer count:
  OCR3A = 31249;
  // turn on CTC mode:
  TCCR3B |= (1 << WGM32);
  // Set CS10 and CS12 bits for 256 prescaler:
  TCCR3B |= (1 << CS32);
  // enable timer compare interrupt:
  TIMSK3 |= (1 << OCIE3A);
  // enable global interrupts:
  sei();

  /** Initialize PID to give if the platform has never been init **/
  if((EEPROM.read(INIT_ADDR) != INIT_UID) && (EEPROM.read(INIT_ADDR2) != INIT_UID))
   {
    EEPROM.write(LAST_PID_ADDR, NO_ID_PID + 1);
    EEPROM.write(GUID_REGED_ADDR, false);
    EEPROM.write(GUID_INIT_ADDR, false);
    EEPROM.write(INIT_ADDR2, INIT_UID);
    EEPROM.write(INIT_ADDR, INIT_UID);
  }

  /** Reset Ethernet controller **/
  pinMode(ETH_RST, OUTPUT);
  digitalWrite(ETH_RST, LOW);
  delay(100);
  digitalWrite(ETH_RST, HIGH);
  delay(100);

  /** Initialize communications **/
  i = 0;

  return_hvg_comms = hvg_comms.init();

  // Don't exit until registered
  while (return_hvg_comms != RETURN_OK)
  {
    if (return_hvg_comms == RETURN_ETH_PB)
      display_sm = DISP_ETH_PB;
    else if (return_hvg_comms == RETURN_DHCP_TIMEOUT)
      display_sm = DISP_DHCP_PB;
    else if (return_hvg_comms == RETURN_NO_CON_PB)
      display_sm = DISP_NO_CON;
    else if (return_hvg_comms == RETURN_NO_HVG)
      display_sm = DISP_NO_HVG;
    else if (return_hvg_comms == RETURN_NOK)
      display_sm = DISP_PB_COMS;
    else if (return_hvg_comms == RETURN_NREGISTERED)
      display_sm = DISP_NREG;

    disp_loop();  // R.Gifford 25/06/2014
    
    delay(3000);
    
    // If GUID reset button has been pushed, reset
    if (RESET_FLAG == true) guid_reset();
    
    return_hvg_comms = hvg_comms.init();
  }

  display_sm = DISP_INIT_DONE;
  disp_loop();  // R.Gifford 25/06/2014
  delay(2000);
  display_sm = DISP_COLLECT;
  disp_loop();  // R.Gifford 25/06/2014
}

void loop(void)
{
  nrf_report_packet_t* received_report;
  unsigned char plen = PACKET_PAYLOAD;
  unsigned short temp_crc = 0;
  unsigned char i;

  nrf24.waitAvailableTimeout(200);
  // If we received a packet
  if (nrf24.available())
  {
    if (nrf24.recv((uint8_t*)&nrf_packet, &plen))
    {
#ifdef DATA_OUTPUT
      if (Serial)Serial.println(F("Data received"));
#endif

      // Compute CRC
      temp_crc = nrf_packet.platform_id;
      temp_crc += nrf_packet.message_type;
      for (i = 0; i < PACKET_DATAL; i++)
        temp_crc += nrf_packet.data[i];

      if (temp_crc == nrf_packet.crc)
      {
        if (nrf_packet.message_type == IDREQ_TYPE)
        {
          if (nrf_packet.platform_id != last_pid_token)
          {
            last_pid_token = nrf_packet.platform_id;
            // Send a new platform ID
            nrf_packet.platform_id = NO_ID_PID;
            nrf_packet.message_type = IDREP_TYPE;
            nrf_packet.data[0] = last_pid_token;
            nrf_packet.data[1] = get_new_pid();
#ifdef DATA_OUTPUT
            if (Serial)
            {
              Serial.print(F("Got PID request with token "));
              Serial.println(last_pid_token, DEC);
              Serial.print(F("Assigning PID #"));
              Serial.println(nrf_packet.data[1], DEC);
            }
#endif
            while (send_nrf_packet(3) == false);
          }
        }
        else if (nrf_packet.message_type == REPORT_TYPE)
        {
          received_report = (nrf_report_packet_t*)&nrf_packet;
          store_report_message();
#ifdef DATA_OUTPUT
          if (Serial)
          {
            Serial.print(F("Got report from platform "));
            Serial.print(received_report->platform_id, DEC);
            Serial.println(F(":"));
            Serial.print(F("Humidity: "));
            Serial.print(received_report->humidity / 10, DEC);
            Serial.print(F("."));
            Serial.println(received_report->humidity % 10, DEC);
            Serial.print(F("Temperature: "));
            Serial.print(received_report->temperature / 10, DEC);
            Serial.print(F("."));
            Serial.println(received_report->temperature % 10, DEC);
            Serial.print(F("Ground Temperature: "));
            Serial.print(received_report->temp_ground / 10, DEC);
            Serial.print(F("."));
            Serial.println(received_report->temp_ground % 10, DEC);
            Serial.print(F("Moisture: "));
            Serial.println(received_report->moisture, DEC);
            Serial.print(F("Raw IR: "));
            Serial.println(received_report->ir, DEC);
            Serial.print(F("Lux: "));
            Serial.println(received_report->lux, DEC);
            Serial.println("");
          }
#endif
        }
      }
      else
      {
#ifdef DBG_OUTPUT
        if (Serial)Serial.println(F("CRC error"));
#endif
      }
    }
    else
    {
#ifdef DBG_OUTPUT
      if (Serial)Serial.println(F("Couldn't receive data"));
#endif
    }
  }

  // Send a report to the HTTP server
  if ((sec_counter % SENDING_PERIOD) == 0)
  {
    send_survey_packet();
    hvg_comms.send_report(mess_log);
  }

  // Send a survey packet twice, space 1 sec appart in case the sensor station is sending data
  if (((sec_counter % SURVEY_PERIOD) == 0) || ((sec_counter % SURVEY_PERIOD) == 2))
  {
    if (survey_sent == false)
    {
      send_survey_packet();
      survey_sent = true;
    }
  }
  else
  {
    survey_sent = false;
  }

  // Added RG 25/06/2014
  /** If the user pressed the button for several seconds, reset GUID **/
  if (RESET_FLAG == true) guid_reset();

  // picture loop
  disp_loop();  // R.Gifford 25/06/2014
}

void guid_reset(void)
{
    /** Re-init GUID **/
    EEPROM.write(LAST_PID_ADDR, NO_ID_PID + 1);
    EEPROM.write(GUID_REGED_ADDR, false);
    EEPROM.write(GUID_INIT_ADDR, false);
    EEPROM.write(INIT_ADDR2, INIT_UID);
    EEPROM.write(INIT_ADDR, INIT_UID);
    
    display_sm = DISP_RESET;
    disp_loop();
    
    delay(2000);
    
    soft_reset();
}
