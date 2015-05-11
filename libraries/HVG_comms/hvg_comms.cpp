// hvg_comms.cpp
// Author: Mathieu Stephan
// Copyright (C) 2013 Mathieu Stephan

#include <hvg_comms.h>

HVGCOMMS::HVGCOMMS(uint16_t guid_init_addr, uint16_t guid_reged_addr, uint16_t guid_addr)
{
	_guid_reged_addr = guid_reged_addr;
	_guid_init_addr = guid_init_addr;
	_guid_addr = guid_addr;
	eth_configured = false;

	// For MAC EEPROM
	Wire.begin(); 

	//strcpy(pucHvgServer,"www.harvestgeek.com");
}

uint8_t HVGCOMMS::get_MAC( uint8_t * mac_addr ) {
  
	uint8_t i = 0;

  	Wire.beginTransmission(0x50);
  	
  	// MAC starts at 0xFA
  	Wire.write(0xFA);
	
	Wire.endTransmission();

	// Request six nb_bytes
	Wire.requestFrom(0x50,6);

	while(Wire.available()) {
		mac_addr[i] = Wire.read();
		i++;
	}

	/*
	// Use fixed MAC for debug 001EC0F5C220
	mac_addr[0] = 0x00;
	mac_addr[1] = 0x1E;
	mac_addr[2] = 0xC0;
	mac_addr[3] = 0xF5;
	mac_addr[4] = 0xC2;
	mac_addr[5] = 0x20;
	*/

	return 0;
}

uint8_t HVGCOMMS::init_HTTP_POST_connect(uint16_t nb_bytes)
{
	// Set the static IP address to use if the DHCP fails to assign
	IPAddress server(192, 99, 15, 192);

	if(w5100.connect(server, 80))
	{		
	    w5100.println(F("POST /api/guid HTTP/1.1")); 		//download text
	    w5100.println(F("From: Arduino"));
	    w5100.println(F("Host: www.harvestgeek.com"));  	// Will be needed if apache is configured for VHOSTS
	    w5100.println(F("User-Agent: HTTPTool/1.1"));
	    w5100.println(F("Connection: close"));
	    w5100.println(F("Cache-Control : no-cache"));
	    w5100.println(F("Pragma: no-cach"));
	    w5100.println(F("Expires: -1"));
	    w5100.println(F("Content-Type: application/json"));
	    w5100.print(F("Content-Length: "));
	    w5100.println(nb_bytes);
	    w5100.println();
		return RETURN_OK;
	}
	else
	{
		#ifdef HTTP_DBG_OUTPUT
			if (Serial) Serial.println(F("Harvestgeek.com HTTP con failed"));
		#endif

		return RETURN_NO_HVG;
	}
}

// Connect to google, send malformed request and check for 400 error
uint8_t HVGCOMMS::check_internet( void )
{
	uint8_t i;
	char buf[12] = {0};
	char server[] = "www.google.com";

		#ifdef HTTP_DBG_OUTPUT
			if (Serial) Serial.print(F("Internet check: "));
		#endif

	// Connect and send get request
  	if (w5100.connect(server, 80)) {
    	w5100.println(F("GET / HTTP/1.1"));
    	w5100.println(F("Host: http://www.google.com"));
    	w5100.println();
	}	
  	else return RETURN_NOK;

  	// wait 5s for reply
  	for (i=0;i<10;i++) {
  		if(w5100.available()) break;
  		else delay(500);
  	}

  	// Store reply in buf
  	if (w5100.available()) {
  		for (i=0;i<12;i++)
  			buf[i] = w5100.read();
  	}

  	w5100.stop();

  	// Check for response
  	if (strncmp(buf,"HTTP/1.1 400",12) == 0) {
		#ifdef HTTP_DBG_OUTPUT
			if (Serial) Serial.println(F("OK."));
		#endif

  		return RETURN_OK;
  	}
  	else return RETURN_NOK;
}

void HVGCOMMS::send_init_text(uint8_t* mac_addr, bool has_guid)
{
	/**********************************************/
	// {"messageType" : "BaseInit",
	// "mac" : "xxxxxxxxxxxx"}
	// or
	//{"messageType" : "BaseInit",
	//"mac" : "xxxxxxxxxxxx",
	//"guid" : "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"}
	/**********************************************/
	
	// Looper variable
	uint8_t i = 0;

  	w5100.println(F("{\"messageType\" : \"BaseInit\","));
  	w5100.print(F("\"mac\" : \""));
  	for (i=0;i<6;i++) w5100.print(mac_addr[i], HEX);

	#ifdef HTTP_DBG_OUTPUT
  		if(Serial) {
		    Serial.println(F("{\"messageType\" : \"BaseInit\","));
		    Serial.print(F("\"mac\" : \""));
			for (i=0;i<6;i++) Serial.print(mac_addr[i], HEX);
		}
	#endif

	if(has_guid == true)
	{		
		w5100.println(F("\","));
    	w5100.print(F("\"guid\" : \""));
    	for (i=0;i<GUID_LGTH;i++) w5100.print((uint8_t)guid[i], HEX);
	}
	w5100.println(F("\"}"));

	#ifdef HTTP_DBG_OUTPUT
		if(Serial) {
		    if(has_guid == true)
		    {
		      Serial.println(F("\","));
		      Serial.print(F("\"guid\" : \""));
		      for (i=0;i<GUID_LGTH;i++) Serial.print((uint8_t)guid[i], HEX);
		    }
		    Serial.println(F("\"}"));
		}	
	#endif	

	#ifdef HTTP_DBG_OUTPUT
		if (Serial) Serial.println(F("Sent init packet"));
	#endif	
}

void HVGCOMMS::send_report_text(uint8_t* mac_addr, message_log_t* mess_log)
{
	bool temp_bool = true;

	// Looper variable
	uint8_t i = 0;

  	w5100.println(F("{\"messageType\" : \"SensorData\","));
  	w5100.print(F("\"mac\" : \""));
  	for (i=0;i<6;i++) w5100.print(mac_addr[i], HEX);

	w5100.println(F("\","));
    w5100.print(F("\"guid\" : \""));
    for (i=0;i<GUID_LGTH;i++) w5100.print(guid[i], HEX);
	w5100.println(F("\","));
	w5100.print(F("\"stationPackets\" : ["));

	// Send platform reports
	for(i = 0; i < MAX_PLATFORMS; i++)
	{
		if(mess_log[i].platform_id == NO_ID_PID)
			break;

		if(mess_log[i].last_mess_sec < TIMEOUT_PLAT_REPORT)
		{         
			if(temp_bool)
				temp_bool = false;
			else

			w5100.println(F(","));
			w5100.println(F("{"));
			w5100.println(F(" \"stationType\" : \"Sensor\","));
			w5100.print(F(" \"pid\" : \""));
			w5100.print(mess_log[i].platform_id);
			w5100.println(F("\","));
			w5100.print(F(" \"temp\" : \""));
			w5100.print(mess_log[i].temperature);
			w5100.println(F("\","));
			w5100.print(F(" \"humidity\" : \""));
			w5100.print(mess_log[i].humidity);
			w5100.println(F("\","));
			w5100.print(F(" \"tempProbe\" : \""));
			w5100.print(mess_log[i].temp_ground);
			w5100.println(F("\","));
			w5100.print(F(" \"moisture\" : \""));
			w5100.print(mess_log[i].moisture);
			w5100.println(F("\","));
			w5100.print(F(" \"lux\" : \""));
			w5100.print(mess_log[i].lux);
			w5100.println(F("\","));
			w5100.print(F(" \"ir\" : \""));
			w5100.print(mess_log[i].ir);
			w5100.print(F("\" }"));
		}
	}   
	w5100.println();
	w5100.println(F(" ]"));
	w5100.println(F("}"));

	#ifdef HTTP_DBG_OUTPUT
		if(Serial) Serial.println(F("Sent report \n"));
	#endif	
}

uint8_t HVGCOMMS::parse_close_http_answer_for_field(char* field, char* val)
{
	bool parse_finished = false;
	bool field_found = false;
	bool start_val = false;
	char string_buffer[10] = {0};
	uint8_t index = 0;
	uint8_t i;
	char c;
	
  	// wait 5s for reply
  	for (i=0;i<10;i++) {
  		if(w5100.available()) break;
  		else delay(500);
  	}

	// If we received something
	if(w5100.available())
	{
		#ifdef HTTP_DBG_OUTPUT
			if(Serial) Serial.println(F("HTTP mess received"));
		#endif		

		while(w5100.available())
		{
			// Shift buffer contents
			for(i = 0; i < 9; i++)
				string_buffer[i] = string_buffer[i+1];
				
			// Receive one byte
			c = w5100.read();

			#ifdef HTTP_DBG_OUTPUT
				if(Serial) Serial.print(c);
			#endif

			if(c == '"')
			{
				if(field_found == false)
				{
					// Is it the good field?
					if(strncmp(string_buffer+9-hm_strlen(field), field, hm_strlen(field)) == 0)
						field_found = true;
				}
				else
				{
					// If we have found the field, the next " indicates start of the data
					if(start_val == false)
					{
						start_val = true;
					}
					else
					{
						if (parse_finished == false) val[index++] = 0x00;
						parse_finished = true;
					}
				}
			}
			else
			{
				string_buffer[9] = c;
				if((parse_finished == false) && (start_val == true))
				{
					val[index++] = c;
				}
			}
		}
	}
	else
	{
		#ifdef HTTP_DBG_OUTPUT
			if(Serial) Serial.println(F("HTTP Timeout"));
		#endif

		w5100.stop();

		return RETURN_TIMEOUT;
	}

	w5100.stop();

	if(parse_finished == true)
		return RETURN_OK;
	else
		return RETURN_NFOUND;
}

void HVGCOMMS::int_to_string(uint16_t value, char* string)
{
	uint16_t index = 0;
	uint16_t div_rest;
	uint16_t temp;
	uint16_t i;

	if(value == 0)
	{
		string[index] = 0x30;
		string[index + 1] = 0x00;
		return;
	}

	while(value != 0)
	{
		div_rest = (unsigned char)(value % 10);
		value = value / 10;
		string[index++] = div_rest + 0x30;
	}

	for(i = 0; i < (index >> 1); i++)
	{
		temp = string[i];
		string[i] = string[index - i - 1];
		string[index - i - 1] = temp;
	}

	string[index] = 0x00;
}

void HVGCOMMS::convert_hex_to_hexstring(char* hex_string, char* string, uint8_t nb_bytes)
{
	uint8_t l_nibble;
	uint8_t h_nibble;
	uint8_t i;
	
	for(i = 0; i < nb_bytes; i++)
	{
		l_nibble = hex_string[i] & 0x0F;
		h_nibble = (hex_string[i] & 0xF0) >> 4;
		
		if(h_nibble < 10)
			string[i*2] = h_nibble + '0';
		else
			string[i*2] = h_nibble + 'a' - 10;
		if(l_nibble < 10)
			string[i*2+1] = l_nibble + '0';
		else
			string[i*2+1] = l_nibble + 'a' - 10;
	}
	string[i*2] = 0;
}

void HVGCOMMS::convert_hexstring_to_hex(char* string, char* result)
{
	char temp_char2 = 0;
	char temp_char = 0;
	uint8_t i = 0;

	// Will only work if there is a '0' if the first byte is < 16
	while(((string[i] >= '0') && (string[i] <= '9')) || ((string[i] >= 'a') && (string[i] <= 'f')))
	{
		if(string[i] <= '9')
			temp_char2 = string[i] - '0';
		else
			temp_char2 = string[i] - 'a' + 10;
			
		if(i & 1)
		{
			temp_char = temp_char << 4;
			temp_char += temp_char2;
			result[i/2] = temp_char;
		}
		else
		{
			temp_char = temp_char2;
		}
		i++;
	}
}

uint8_t HVGCOMMS::get_new_guid(uint8_t* mac_addr)
{
	char val_buffer[33];
	uint16_t i;

	if(init_HTTP_POST_connect(INIT_PACKET_LENGTH) == RETURN_OK)
	{

		// send packet to server
		send_init_text(mac_addr, false);

		// look for the guid field in the answer
		if(parse_close_http_answer_for_field("guid", val_buffer) == RETURN_OK)
		{
			// guid has a good length
			if(hm_strlen(val_buffer) == GUID_LGTH * 2)
			{
				#ifdef HTTP_DBG_OUTPUT
					if(Serial) {
						Serial.println();
						Serial.print(F("Got new GUID : "));
					}	
				#endif					

				// Convert string
				convert_hexstring_to_hex(val_buffer, val_buffer);

				// Store GUID
				for(i = 0; i < GUID_LGTH; i++) {
					EEPROM.write(_guid_addr+i, (uint8_t)val_buffer[i]);
					#ifdef HTTP_DBG_OUTPUT
						if(Serial) Serial.print((uint8_t)val_buffer[i],HEX); 
					#endif					
				}

				#ifdef HTTP_DBG_OUTPUT
					if(Serial) Serial.println();
				#endif

				return RETURN_OK;
			}
			else
			{
				return RETURN_NOK;
			}
		}
		else
		{
			return RETURN_NOK;
		}
	}
	else
	{
		return RETURN_NO_CON_PB;
	}
}

uint8_t HVGCOMMS::get_register_status(uint8_t* mac_addr)
{
	char val_buffer[5];

	if(init_HTTP_POST_connect(QUERY_REG_LENGTH) == RETURN_OK)
	{
		send_init_text(mac_addr, true);

		if(parse_close_http_answer_for_field("status", val_buffer) == RETURN_OK)
		{
			if(hm_strlen(val_buffer) == 2)
			{
				#ifdef HTTP_DBG_OUTPUT
				if(Serial){Serial.println(F("Successfully registered"));}
				#endif									
				return RETURN_OK;
			}
			else
			{
				return RETURN_NREGISTERED;
			}
		}
		else
		{
			return RETURN_NOK;
		}
	}
	else
	{
		return RETURN_NO_CON_PB;
	}
}

uint8_t HVGCOMMS::init( void )
{
	uint8_t temp_return = 0;
	uint8_t i;	

	get_MAC(_mac_addr);

	/** Start Ethernet connection **/
	if(eth_configured == false)
	{
		#ifdef DEL_BOOT
		delay(4000);
		#endif

		#ifdef ETH_DBG_OUTPUT
			if (Serial) {
				Serial.print(F("Mac: "));
				for (i=0;i<6;i++) Serial.print(_mac_addr[i], HEX);
				Serial.println();
			}
		#endif

		if (Ethernet.begin(_mac_addr) == 0) {
			#ifdef ETH_DBG_OUTPUT
				if (Serial) Serial.println(F("Failed to configure Ethernet using DHCP"));
			#endif

			return RETURN_ETH_PB;
		}
		else eth_configured = true;
	}

	if (check_internet() != RETURN_OK) {
		#ifdef HTTP_DBG_OUTPUT
			if (Serial) Serial.println(F("No internet."));
		#endif

		return RETURN_NO_CON_PB;
	} 

	/** Check if we already have a GUID **/
	if(EEPROM.read(_guid_init_addr) == false) {

		temp_return = get_new_guid(_mac_addr);

		if(temp_return == RETURN_OK) {

			#ifdef ETH_DBG_OUTPUT
				if (Serial) {
					Serial.println();
					Serial.print(F("Got new GUID: "));
				}
			#endif

			EEPROM.write(_guid_init_addr, true);
			
			// Get guid from eeprom
			for(i = 0; i < GUID_LGTH; i++) {
				guid[i] = EEPROM.read(_guid_addr+i);

				#ifdef ETH_DBG_OUTPUT
					if (Serial) Serial.print((uint8_t) guid[i], HEX);
				#endif
			}

			#ifdef ETH_DBG_OUTPUT
					if (Serial) Serial.println();
			#endif
		}
		else 
			return temp_return;
	}

	/** Check if we already have a GUID **/
	if(EEPROM.read(_guid_init_addr) == true)
	{
		// Get guid from eeprom
		for(i = 0; i < GUID_LGTH; i++)
			guid[i] = EEPROM.read(_guid_addr + i);

		// Did the user register his base station with the server?
		if(EEPROM.read(_guid_reged_addr) == false)
		{
			temp_return = get_register_status(_mac_addr); 

			if(temp_return == RETURN_OK)
				EEPROM.write(_guid_reged_addr, true);
			else

				#ifdef ETH_DBG_OUTPUT
						if (Serial) Serial.println(F("\nGUID not registered."));
				#endif		

				return temp_return;
		}
		if(EEPROM.read(_guid_reged_addr) == true)
		{	
			#ifdef ETH_DBG_OUTPUT
					if (Serial) Serial.println(F("\nGUID registered OK."));
			#endif		

			return RETURN_OK;		
		}
	}

	return RETURN_NOK;
}

uint16_t HVGCOMMS::hm_strlen(char* ptr)
{
  uint8_t return_val = 0;

  while(*ptr++ != 0)
    return_val = return_val + 1;

  return return_val;
}

uint16_t HVGCOMMS::get_report_data_length(message_log_t* mess_log)
{
	uint16_t return_value = REPORT_FIX_LENGTH;
	char temp_string[10];
	uint8_t i;

	// Compute the decimal in string length
	for(i = 0; i < MAX_PLATFORMS; i++)
	{
		if(mess_log[i].platform_id == NO_ID_PID)
		  break;
		  
		if(mess_log[i].last_mess_sec < TIMEOUT_PLAT_REPORT)
		{  
			return_value += REPORT_ITEM_LENGTH;
			int_to_string(mess_log[i].platform_id, temp_string);
			return_value += hm_strlen(temp_string);
			int_to_string(mess_log[i].temperature, temp_string);
			return_value += hm_strlen(temp_string);
			int_to_string(mess_log[i].humidity, temp_string);
			return_value += hm_strlen(temp_string);
			int_to_string(mess_log[i].temp_ground, temp_string);
			return_value += hm_strlen(temp_string);
			int_to_string(mess_log[i].moisture, temp_string);
			return_value += hm_strlen(temp_string);
			int_to_string(mess_log[i].lux, temp_string);
			return_value += hm_strlen(temp_string);
			int_to_string(mess_log[i].ir, temp_string);
			return_value += hm_strlen(temp_string);
		}
	}

	// strip the extra ","
	if(i != 0)
	return_value--;

	return return_value;  
}

// Client function to send/receive POST report data
void HVGCOMMS::send_report(message_log_t* mess_log)
{	
	char field_return[5];
	
	if(init_HTTP_POST_connect(get_report_data_length(mess_log)) == RETURN_OK)
	{
		send_report_text(_mac_addr, mess_log);		
		parse_close_http_answer_for_field("answer", field_return);
	}
}