//
//  main.cpp
//  Arduino_wrapper
//
//  Created by Allex Veldman on 15/08/15.
//  Copyright (c) 2015 Allex Veldman. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include <string>
#include "Arduino.h"
#include "SPI.h"
#include "MFRC522.h"

#define RST_PIN         25
#define SS_PIN          8

#define TLV_LOCK		0x01
#define TLV_MEM			0x02
#define TLV_NDEF		0x03
#define TLV_TERM		0xFE

#define UTF8			0x02


std::vector<MFRC522> mfrc522(5, MFRC522(SS_PIN, RST_PIN) );   // Create MFRC522 instance.


//void digitalWrite(int pin, ePinLevel level)
//{
//	printf("Weak worked!!\n");
//}

// Helper routine to dump a byte array as hex values to Serial.
void dump_byte_array(byte *buffer, byte bufferSize) {
	for (byte i = 0; i < bufferSize; i++) {
		printf("%.2X ",buffer[i]);
	}
}



// buffer needs to me at least pay_length + 11 (this includes \0 at the end)
// adds the TLV_LOCK message so can only be used once.
void createNdefTextMessage(byte *buffer, const char *payload, int pay_length)
{
	buffer[0] = TLV_NDEF;			// TLV message type
	buffer[1] = 7 + pay_length;		// message length
	buffer[2] = 0xD1;				// MB, ME, no chucks, short message, no ID_length field, NFC RTD type
	buffer[3] = 0x01;				// Type length
	buffer[4] = 3 + pay_length;		// payload length + UTF8 + language
	buffer[5] = 'T';				// Text record
	buffer[6] = UTF8;				// UTF-8 encoding
	buffer[7] = 'e';
	buffer[8] = 'n';				// English language
	memcpy(&buffer[9], payload, pay_length);
	buffer[9+pay_length] = TLV_TERM;
}



void setup(void)
{
	SPI.begin();        // Init SPI bus
	mfrc522[0].PCD_Init(); // Init MFRC522 card
	
	Serial.println(F("Scanning for new messages\n\n"));
}


void loop() {
	// Look for new cards
	if ( ! mfrc522[0].PICC_IsNewCardPresent())
		return;
	
	// Select one of the cards
	if ( ! mfrc522[0].PICC_ReadCardSerial())
		return;
	
	// Show some details of the PICC (that is: the tag/card)
	Serial.print(F("Card UID:"));
	dump_byte_array(mfrc522[0].uid.uidByte, mfrc522[0].uid.size);
	Serial.println();
	Serial.print(F("PICC type: "));
	byte piccType = mfrc522[0].PICC_GetType(mfrc522[0].uid.sak);
	Serial.println(mfrc522[0].PICC_GetTypeName(piccType));
	
	
	// Be sure to write to block > 4, block 1 - 3 might lock up your tag
	byte blockAddr = 4;
	
	// Create NDEF text record to write to PICC
	std::cout << "Insert 3 digit card #: ";
	char message[4];
	for(int i=0;i<3;i++)
	{
		std::cin >> message[i];
		if(!isdigit(message[i])) // checks if character by character is a digit or not.
		{
			std::cout << "Inputs contains non integer char.\n";
			std::cin.ignore(); //ignores the rest of the input.
			return;
		}
	}
	char x = std::cin.get();
	if(x!='\n') // if you find 1 more char available for input and that character is not the enter button.
	{
		std::cout << "Input contains more than 3 characters.\n";
		std::cin.ignore(); //ignores the rest of the characters in the input stream.
		return;
	}
	message[3] = '\0';
	byte dataBlock[10+sizeof(message)];
	createNdefTextMessage(dataBlock, message, sizeof(message) );
	
	byte status;
	byte buffer[sizeof(dataBlock) + (16 - (sizeof(dataBlock)%16)) + 2];	// Ultralight always returns 16 bytes + 2byte CRC
	
	// Read data from the block
	Serial.print(F("Reading data from block ")); Serial.print(blockAddr);
	Serial.println(F(" ..."));
	
	for (std::size_t i = 0; i<sizeof(dataBlock); i += 16)
	{
		byte size = 18;
		status = mfrc522[0].MIFARE_Read(blockAddr+(i/4), &buffer[i], &size);
		if (status != MFRC522::STATUS_OK) {
			Serial.print(F("MIFARE_Read() failed: "));
			Serial.println(mfrc522[0].GetStatusCodeName(status));
		}
	}
	
	Serial.print(F("Data in block ")); Serial.print(blockAddr); Serial.println(F(":"));
	dump_byte_array(buffer, sizeof(buffer)); Serial.println();
	Serial.println();
	
	// Write data to the block in steps of 4
	Serial.print(F("Writing data into block ")); Serial.print(blockAddr);
	Serial.println(F(" ..."));
	dump_byte_array(dataBlock, sizeof(dataBlock)); Serial.println();
	
	for (std::size_t i=0; i<sizeof(dataBlock); i+=4)
	{
		status = mfrc522[0].MIFARE_Ultralight_Write(blockAddr+(i/4), &dataBlock[i], 4);
		if (status != MFRC522::STATUS_OK) {
			Serial.print(F("MIFARE_Write() failed: "));
			Serial.println(mfrc522[0].GetStatusCodeName(status));
		}
	}
	Serial.println();

	// Read data from the block (again, should now be what we have written)
	Serial.print(F("Reading data from block ")); Serial.print(blockAddr);
	Serial.println(F(" ..."));
	
	for (std::size_t i = 0; i<sizeof(dataBlock); i += 16)
	{
		byte size = 18;
		status = mfrc522[0].MIFARE_Read(blockAddr+(i/4), &buffer[i], &size);
		if (status != MFRC522::STATUS_OK) {
			Serial.print(F("MIFARE_Read() failed: "));
			Serial.println(mfrc522[0].GetStatusCodeName(status));
		}
	}
	
	Serial.print(F("Data in block ")); Serial.print(blockAddr); Serial.println(F(":"));
	dump_byte_array(buffer, sizeof(buffer)); Serial.println();

	// Check that data in block is what we have written
	// by counting the number of bytes that are equal
	Serial.println(F("Checking result..."));
	byte count = 0;
	for (byte i = 0; i < sizeof(dataBlock); i++) {
		// Compare buffer (= what we've read) with dataBlock (= what we've written)
		if (buffer[i] == dataBlock[i])
			count++;
	}
	Serial.print(F("Number of bytes that match = ")); Serial.println(count);
	if (count == sizeof(dataBlock)) {
		Serial.println(F("Success :-)"));
	} else {
		Serial.println(F("Failure, no match :-("));
		Serial.println(F("  perhaps the write didn't work properly..."));
	}
	Serial.println();

	// Halt PICC
	mfrc522[0].PICC_HaltA();
}

byte *decodeNDEF(byte *buffer, int buffer_size)
{
	// Dirty hack because i'm lazy, look for NDEF code and add message offset
	// THIS DOES NOT WORK IF THERE IS A 0x03 BEFORE THE ACTUAL NDEF HEADER START!!!
	
	for (int i=0; i<buffer_size; i++)
	{
		if (buffer[i] == TLV_NDEF) {
			return &buffer[i+9];
		}
	}
	return NULL; // no NDEF message return NULL
}

// Check if there is a card on the selected reader
// returns card value when there is one, returns 0 otherwise
int checkForCard(MFRC522 reader)
{
	int value = 0;
	byte blockAddr = 4;
	byte status;
	byte size = 18;
	byte buffer[size];	// Ultralight always returns 16 bytes + 2byte CRC
	
	// Look for new cards
	if ( ! mfrc522[0].PICC_IsNewCardPresent())
		return 0;
	
	// Select one of the cards
	if ( ! mfrc522[0].PICC_ReadCardSerial())
		return 0;
	
	// Read data from the block
	status = mfrc522[0].MIFARE_Read(blockAddr, buffer, &size);
	if (status != MFRC522::STATUS_OK) {
		Serial.print(F("MIFARE_Read() failed: "));
		Serial.println(mfrc522[0].GetStatusCodeName(status));
		return 0;
	}
	
	byte *message = decodeNDEF(buffer, 18);
	if (message != NULL)
	{
//		std::cout << "Message: " << message << std::endl;
		value = atoi((char *)message);
	}
	
	// Halt PICC
	mfrc522[0].PICC_HaltA();
	
	return value;
}

hardwareSerial Serial;
hardwareSPI SPI;

int main(int argc, char *argv[])
{
	int value;
	setup();
	while (1) {
		value = checkForCard(mfrc522[0]);
		if (value) {
			printf("found card: %i\n",value);
		}
//		loop();
	}
	return 0;
}