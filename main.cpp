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
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <getopt.h>
#include "Arduino.h"
#include "SPI.h"
#include "MFRC522.h"

#define RST_PIN         4 // reset pins are connected to VDD directly but MCP lib still needs a RST pin
#define P1_SS_PIN       8
#define P2_SS_PIN		24
#define P3_SS_PIN		23

#define TLV_LOCK		0x01
#define TLV_MEM			0x02
#define TLV_NDEF		0x03
#define TLV_TERM		0xFE

#define UTF8			0x02

#define MAX_PLAYERS		4

#define SHIFT_SER		8
#define SHIFT_OE		25
#define SHIFT_CLK		24
#define SHIFT_CLR		23


//std::vector<MFRC522> mfrc522(MAX_PLAYERS*5, MFRC522(SS_PIN, RST_PIN) );   // Create 5 MFRC522 instances.
std::vector<MFRC522> mfrc522;

int overwrite = 0;

static void help(void) __attribute__ ((noreturn));
// Print help
static void help(void)
{
	printf("Usage: main  [-h] [-o] \n"
		   "Read Roborally play card or overwrite their value\n"
		   "	-o,	--overwrite		overwrite value on card\n");
	exit(1);
}

static int parse_args(int argc, char **argv)
{
	int option_index = 0;
	int c = 0;
	
	static struct option long_options[] = {
		{"help",		no_argument,		0, 'h'},
		{"overwrite",	no_argument,		0, 'o'},
		{0, 0, 0, 0}
	};
	
	while ((c = getopt_long(argc, argv, "ho", long_options, &option_index)) != -1)
	{
		switch (c)
		{
			case 'h':
				help();
				break;
				
			case 'o':
				overwrite = 1;
				break;
				
			case '?':
				help();
				break;
				
			default:
				abort();
		}
	}
	
	return 0;
}

void initShift(void)
{
	pinMode(SHIFT_SER, OUTPUT);
	pinMode(SHIFT_OE, OUTPUT);
	pinMode(SHIFT_CLK, OUTPUT);
	pinMode(SHIFT_CLR, OUTPUT);
	
	_digitalWrite(SHIFT_SER, LOW);
	_digitalWrite(SHIFT_CLK, LOW);
	_digitalWrite(SHIFT_OE, HIGH);
	_digitalWrite(SHIFT_CLR, HIGH);
}

void shiftWrite(int value)
{
	_digitalWrite(SHIFT_OE, 1);
	_digitalWrite(SHIFT_CLR, 0);
	usleep(5);
	_digitalWrite(SHIFT_CLR, 1);
	for (int i=16; i>0; i--)
	{
		//
		_digitalWrite(SHIFT_SER, (value >> i) & 0x01);
		usleep(5);
		_digitalWrite(SHIFT_CLK, 1);
		usleep(5);
		_digitalWrite(SHIFT_CLK, 0);
		usleep(5);
	}
	// add one clock pulse for transition from shift register to storage register
	// clocked data for this last pulse is 1 so no chips get selected by accident
	_digitalWrite(SHIFT_SER, 1);
	usleep(5);
	_digitalWrite(SHIFT_CLK, 1);
	usleep(5);
	_digitalWrite(SHIFT_CLK, 0);
	usleep(5);
	_digitalWrite(SHIFT_OE, 0);
}


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


// buffer needs to be at least pay_length + 11 (this includes \0 at the end)
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
	mfrc522.push_back(MFRC522(P1_SS_PIN,RST_PIN));
	mfrc522.push_back(MFRC522(P2_SS_PIN,RST_PIN));
	mfrc522.push_back(MFRC522(P3_SS_PIN,RST_PIN));
	
	mfrc522.at(0).PCD_Init(); // Init MFRC522 card
	mfrc522.at(1).PCD_Init(); // Init MFRC522 card
	mfrc522.at(2).PCD_Init(); // Init MFRC522 card
}

// Loop that allows for rewriting the NDEF message on the PICC
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
	if ( ! reader.PICC_IsNewCardPresent())
		return 0;
	
	// Select one of the cards
	if ( ! reader.PICC_ReadCardSerial())
		return 0;
	
	// Read data from the block
	status = reader.MIFARE_Read(blockAddr, buffer, &size);
	if (status != MFRC522::STATUS_OK) {
		Serial.print(F("MIFARE_Read() failed: "));
		Serial.println(reader.GetStatusCodeName(status));
		return 0;
	}
	
	byte *message = decodeNDEF(buffer, 18);
	if (message != NULL)
	{
//		std::cout << "Message: " << message << std::endl;
		value = atoi((char *)message);
	}
	
	// Halt PICC
	reader.PICC_HaltA();
	
	return value;
}

hardwareSerial Serial;
hardwareSPI SPI;
pthread_mutex_t mutexPlayer[MAX_PLAYERS] = {PTHREAD_MUTEX_INITIALIZER};
int playerCards[MAX_PLAYERS][5] = {{0x00}};
pthread_mutex_t mutexTime = PTHREAD_MUTEX_INITIALIZER;
double checkTime;


void *monitorCardThread(void *nrOfPlayers)
{
	int nrPlayers = (int)nrOfPlayers;
	int local_players[MAX_PLAYERS][5] = {{0x00}};
	int index[MAX_PLAYERS] = {0x00};
	int value=0;

	while (1)
	{
		for (int player=0; player<nrPlayers; player++)
		{
			struct timeval t1, t2;
			double elapsedTime;
			
			// start timer
			gettimeofday(&t1, NULL);
			
			value = checkForCard(mfrc522.at(player));
			if ( (value != 0) && (value != local_players[player][ index[player] ]) )
			{
				pthread_mutex_lock( &mutexPlayer[player] );
				playerCards[player][ index[player] ] = value;
				pthread_mutex_unlock( &mutexPlayer[player] );
				local_players[player][ index[player] ] = value;
				
				index[player]++;
				if (index[player] >= 5)
				{
					index[player] = 0;
				}
				
				// get stop time
				gettimeofday(&t2, NULL);
				
				// compute the elapsed time in millisec
				elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
				elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;   // us to ms
				pthread_mutex_lock( &mutexTime);
				checkTime = elapsedTime;
				pthread_mutex_unlock( &mutexTime);
			}
		}
	}
	return NULL;
}

void *printValuesThread(void *nrOfPlayers)
{
	int nrPlayers = (int)nrOfPlayers;
	while(1)
	{
		printf("\e[1;1H\e[2J");
		for (int player=0; player< nrPlayers; player++) {
			pthread_mutex_lock( &mutexPlayer[player] );
			
			printf("player %i:\t%.3i, %.3i, %.3i, %.3i, %.3i              \n",
				   player,
				   playerCards[player][0],
				   playerCards[player][1],
				   playerCards[player][2],
				   playerCards[player][3],
				   playerCards[player][4]);
			pthread_mutex_unlock( &mutexPlayer[player] );
		}
		pthread_mutex_lock( &mutexTime);
		printf("Last get time: %.2fms\n",checkTime);
		pthread_mutex_unlock( &mutexTime);
		usleep(5000);
	}
	return NULL;
}

int main(int argc, char *argv[])
{
	int numberOfPlayers = 3;
	int value;
	
	parse_args(argc, argv);
	
	setup();
	
	if (overwrite)
	{
			while (1) {
		//		value = checkForCard(mfrc522[0]);
		//		if (value) {
		//			printf("found card: %i\n",value);
		//		}
				loop();
			}
	}
	pthread_mutex_init(&mutexPlayer[0], NULL);
	pthread_mutex_init(&mutexPlayer[1], NULL);
	
	pthread_t thread_id0, thread_id1;
	
	pthread_create( &thread_id0, NULL, monitorCardThread, (void *)numberOfPlayers );
	pthread_create( &thread_id1, NULL, printValuesThread, (void *)numberOfPlayers );
	
	pthread_join( thread_id0, NULL);
	pthread_join( thread_id1, NULL);
	
	printf("Proper Stop\n"); // Not sure how to reach this because ctl+c terminates the program..

	

	return 0;
}