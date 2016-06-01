/*
MOSI -> 11
MISO -> 12
SCK -> 13
*/

#define RADIO_ID 0
#define DESTINATION_RADIO_ID 1

#define PIN_RADIO_CE 9
#define PIN_RADIO_CSN 10

#include <SPI.h>
#include <NRFLite.h>

#define SERIAL_SPEED 115200
#define debug(input)        { Serial.print(input); }
#define debugln(input)      { Serial.println(input); }
#define debugln2(key,value) { Serial.print(key); Serial.println(value); }

enum RadioStates { StartSync, RunDemos };

struct RadioPacket     { uint8_t Counter; RadioStates RadioState; uint8_t Data[29]; };
struct RadioAckPacketA { uint8_t Counter; uint8_t Data[31]; };
struct RadioAckPacketB { uint8_t Counter; uint8_t Data[30]; };

NRFLite _radio(Serial);
RadioPacket _radioData;
RadioAckPacketA _radioDataAckA;
RadioAckPacketB _radioDataAckB;

const uint16_t DEMO_LENGTH_MILLIS = 4200;
const uint16_t DEMO_INTERVAL_MILLIS = 300;

uint8_t _hadInterrupt, _showMessageInInterrupt;
uint32_t _bitsPerSecond, _packetCount, _successPacketCount, _failedPacketCount, _ackAPacketCount, _ackBPacketCount;
uint64_t _currentMillis, _lastMillis, _endMillis;
float _packetLoss;

void radioInterrupt() {
	
	uint8_t tx_ok, tx_fail, rx_ready;
	_radio.whatHappened(tx_ok, tx_fail, rx_ready);
	
	if (tx_ok) {
		_successPacketCount++;
		if (_showMessageInInterrupt) debugln("...Success");
		
		uint8_t ackLength = _radio.hasAckData();
		
		while (ackLength > 0) {
			
			if (ackLength == sizeof(RadioAckPacketA)) {
				_radio.readData(&_radioDataAckA);
				_ackAPacketCount++;
			}
			else if (ackLength == sizeof(RadioAckPacketB)) {
				_radio.readData(&_radioDataAckB);
				_ackBPacketCount++;
			}
			
			ackLength = _radio.hasAckData();
		}
	}
	
	if (tx_fail) {
		_failedPacketCount++;
		if (_showMessageInInterrupt) debugln("...Failed");
	}
}

void setup() {
	
	Serial.begin(SERIAL_SPEED);
	delay(500); // Needed this to allow serial output to show the first message.
	
	if (!_radio.init(RADIO_ID, PIN_RADIO_CE, PIN_RADIO_CSN, NRFLite::BITRATE250KBPS)) {
		debugln("Cannot communicate with radio");
		while (1) {} // Sit here forever.
	}
	else {
		debugln();
		debugln("250KBPS bitrate");
		_radio.init(RADIO_ID, PIN_RADIO_CE, PIN_RADIO_CSN, NRFLite::BITRATE250KBPS);
		runDemos();
		
		debugln();
		debugln("1MBPS bitrate");
		_radio.init(RADIO_ID, PIN_RADIO_CE, PIN_RADIO_CSN, NRFLite::BITRATE1MBPS);
		runDemos();
		
		debugln();
		debugln("2MBPS bitrate");
		_radio.init(RADIO_ID, PIN_RADIO_CE, PIN_RADIO_CSN, NRFLite::BITRATE2MBPS);
		runDemos();
	}
}

void loop() {}

void runDemos() {
	startSync();
	demoPolling();
	demoInterrupts();
	demoAckPayload();
	demoPollingBitrate();
	demoInterruptsBitrate();
	demoPollingBitrateNoAck();
	demoInterruptsBitrateNoAck();
	demoPollingBitrateAckPayload();
	demoInterruptsBitrateAckPayload();
	demoPollingBitrateAllPacketSizes();
	demoPollingBitrateAllAckSizes();
}

void startSync() {
	
	debugln("  Starting sync");
	
	detachInterrupt(1);
	_radioData.RadioState = StartSync;
	
	while (!_radio.send(DESTINATION_RADIO_ID, &_radioData, sizeof(RadioPacket))) {
		debugln("  Retrying in 4 seconds");
		delay(4000);
	}
	
	_radioData.RadioState = RunDemos;
}

void demoPolling() {
	
	delay(DEMO_INTERVAL_MILLIS);
	
	debugln("Polling");
	
	_endMillis = millis() + DEMO_LENGTH_MILLIS;
	_lastMillis = millis();
	
	while (millis() < _endMillis) {
		
		if (millis() - _lastMillis > 999) {
			_lastMillis = millis();
			
			debug("  Send "); debug(++_radioData.Counter);
			
			if (_radio.send(DESTINATION_RADIO_ID, &_radioData, sizeof(RadioPacket))) {
				debugln("...Success");
			}
			else {
				debugln("...Failed");
			}
		}
	}
}

void demoInterrupts() {
	
	delay(DEMO_INTERVAL_MILLIS);

	debugln("Interrupts");
	
	attachInterrupt(1, radioInterrupt, FALLING);
	_showMessageInInterrupt = 1;
	
	_endMillis = millis() + DEMO_LENGTH_MILLIS;
	_lastMillis = millis();
	
	while (millis() < _endMillis) {
		
		if (millis() - _lastMillis > 999) {
			_lastMillis = millis();
			
			debug("  Start send "); debug(++_radioData.Counter);
			_radio.startSend(DESTINATION_RADIO_ID, &_radioData, sizeof(RadioPacket));
		}
	}
	
	detachInterrupt(1);
}

void demoAckPayload() {
	
	delay(DEMO_INTERVAL_MILLIS);

	debugln("ACK payloads");
	
	_endMillis = millis() + 1000 * 20; // Need 20 seconds to show a good demo.
	_lastMillis = millis();
	
	while (millis() < _endMillis) {
		
		if (millis() - _lastMillis > 4000) {
			_lastMillis = millis();
			
			debug("  Send "); debug(++_radioData.Counter);
			
			if (_radio.send(DESTINATION_RADIO_ID, &_radioData, sizeof(RadioPacket))) {
				
				debug("...Success");
				uint8_t ackLength = _radio.hasAckData();
				
				while (ackLength > 0) {
					
					if (ackLength == sizeof(RadioAckPacketA)) {
						_radio.readData(&_radioDataAckA);
						debug(" - Received ACK A ");
						debug(_radioDataAckA.Counter);
					}
					else if (ackLength == sizeof(RadioAckPacketB)) {
						_radio.readData(&_radioDataAckB);
						debug(" - Received ACK B ");
						debug(_radioDataAckB.Counter);
					}
					
					ackLength = _radio.hasAckData();
				}
				
				debugln();
			}
			else {
				debugln("...Failed");
			}
		}
	}
}

void demoPollingBitrate() {
	
	delay(DEMO_INTERVAL_MILLIS);
	
	debugln("Polling bitrate");
	
	_successPacketCount = 0;
	_failedPacketCount = 0;
	_endMillis = millis() + DEMO_LENGTH_MILLIS;
	_lastMillis = millis();
	
	while (millis() < _endMillis) {

		_currentMillis = millis();
		
		if (_currentMillis - _lastMillis > 999) {
			_packetCount = _successPacketCount + _failedPacketCount;
			_bitsPerSecond = sizeof(RadioPacket) * _successPacketCount * 8 / (float)(_currentMillis - _lastMillis) * 1000;
			_packetLoss = _successPacketCount / (float)_packetCount * 100.0;
			debug("  ");
			debug(_successPacketCount); debug("/"); debug(_packetCount); debug(" packets ");
			debug(_packetLoss); debug("% ");
			debug(_bitsPerSecond); debugln(" bps");
			_successPacketCount = 0;
			_failedPacketCount = 0;
			_lastMillis = _currentMillis;
		}
		
		if (_radio.send(DESTINATION_RADIO_ID, &_radioData, sizeof(RadioPacket), NRFLite::REQUIRE_ACK)) {
			_successPacketCount++;
		}
		else {
			_failedPacketCount++;
		}
	}
}

void demoPollingBitrateNoAck() {
	
	delay(DEMO_INTERVAL_MILLIS);
	
	debugln("Polling bitrate NO_ACK");
	
	_packetCount = 0;
	_endMillis = millis() + DEMO_LENGTH_MILLIS;
	_lastMillis = millis();
	
	while (millis() < _endMillis) {

		_currentMillis = millis();
		
		if (_currentMillis - _lastMillis > 999) {
			_bitsPerSecond = sizeof(RadioPacket) * _packetCount * 8 / (float)(_currentMillis - _lastMillis) * 1000;
			debug("  ");
			debug(_packetCount); debug(" packets ");
			debug(_bitsPerSecond); debugln(" bps");
			_packetCount = 0;
			_lastMillis = _currentMillis;
		}
		
		if (_radio.send(DESTINATION_RADIO_ID, &_radioData, sizeof(RadioPacket), NRFLite::NO_ACK)) {
			_packetCount++;
		}
		else {
			// NO_ACK sends are always successful.
		}
	}
}

void demoInterruptsBitrate() {
	
	delay(DEMO_INTERVAL_MILLIS);
	
	debugln("Interrupts bitrate");
	
	attachInterrupt(1, radioInterrupt, FALLING);
	_showMessageInInterrupt = 0;
	_successPacketCount = 0;
	_failedPacketCount = 0;
	
	_endMillis = millis() + DEMO_LENGTH_MILLIS;
	_lastMillis = millis();
	
	while (millis() < _endMillis) {
		
		_currentMillis = millis();
		
		if (_currentMillis - _lastMillis > 999) {
			_packetCount = _successPacketCount + _failedPacketCount;
			_bitsPerSecond = sizeof(RadioPacket) * _successPacketCount * 8 / (float)(_currentMillis - _lastMillis) * 1000;
			_packetLoss = _successPacketCount / (float)_packetCount * 100;
			debug("  ");
			debug(_successPacketCount); debug("/"); debug(_packetCount); debug(" packets ");
			debug(_packetLoss); debug("% ");
			debug(_bitsPerSecond); debugln(" bps");
			_successPacketCount = 0;
			_failedPacketCount = 0;
			_lastMillis = _currentMillis;
		}
		
		_radio.startSend(DESTINATION_RADIO_ID, &_radioData, sizeof(RadioPacket), NRFLite::REQUIRE_ACK);
	}
	
	detachInterrupt(1);
}

void demoInterruptsBitrateNoAck() {
	
	delay(DEMO_INTERVAL_MILLIS);
	
	debugln("Interrupts bitrate NO_ACK");
	
	attachInterrupt(1, radioInterrupt, FALLING);
	_showMessageInInterrupt = 0;
	_successPacketCount = 0;
	_failedPacketCount = 0;
	uint32_t sendCount = 0;
	
	_endMillis = millis() + DEMO_LENGTH_MILLIS;
	_lastMillis = millis();
	
	while (millis() < _endMillis) {
		
		_currentMillis = millis();
		
		if (_currentMillis - _lastMillis > 999) {
			_bitsPerSecond = sizeof(RadioPacket) * sendCount * 8 / (float)(_currentMillis - _lastMillis) * 1000;
			_lastMillis = _currentMillis;
			debug("  ");
			debug(sendCount); debug(" packets ");
			debug(_bitsPerSecond); debugln(" bps");
			sendCount = 0;
		}
		
		_radio.startSend(DESTINATION_RADIO_ID, &_radioData, sizeof(RadioPacket), NRFLite::NO_ACK);
		
		// Found that using _successPacketCount set in the interrupt handler was giving
		// inconsistent results and not agreeing with the received packet counts.
		// Sends are always considered a success when NO_ACK is used so we'll just track
		// the number of sends that we do and use it for the bitrate calculation.
		sendCount++;
	}
	
	detachInterrupt(1);
}

void demoPollingBitrateAckPayload() {
	
	delay(DEMO_INTERVAL_MILLIS);

	debugln("Polling bitrate ACK payload");
	
	_endMillis = millis() + DEMO_LENGTH_MILLIS;
	_lastMillis = millis();
	
	_ackAPacketCount = 0;
	_ackBPacketCount = 0;
	
	while (millis() < _endMillis) {
		
		_currentMillis = millis();
		
		if (_currentMillis - _lastMillis > 999) {
			_packetCount = _successPacketCount + _failedPacketCount;
			_bitsPerSecond = sizeof(RadioPacket) * _successPacketCount * 8 / (float)(_currentMillis - _lastMillis) * 1000;
			_packetLoss = _successPacketCount / (float)_packetCount * 100.0;
			debug("  ");
			debug(_successPacketCount); debug("/"); debug(_packetCount); debug(" packets ");
			debug(_packetLoss); debug("% A");
			debug(_ackAPacketCount);
			debug(" B");
			debug(_ackBPacketCount);
			debug(" "); debug(_bitsPerSecond); debugln(" bps");
			_successPacketCount = 0;
			_failedPacketCount = 0;
			_ackAPacketCount = 0;
			_ackBPacketCount = 0;
			_lastMillis = _currentMillis;
		}
		
		if (_radio.send(DESTINATION_RADIO_ID, &_radioData, sizeof(RadioPacket), NRFLite::REQUIRE_ACK)) {
			
			_successPacketCount++;
			
			uint8_t ackLength = _radio.hasAckData();
			
			while (ackLength > 0) {
				
				if (ackLength == sizeof(RadioAckPacketA)) {
					_radio.readData(&_radioDataAckA);
					_ackAPacketCount++;
				}
				else if (ackLength == sizeof(RadioAckPacketB)) {
					_radio.readData(&_radioDataAckB);
					_ackBPacketCount++;
				}
				
				ackLength = _radio.hasAckData();
			}
		}
		else {
			_failedPacketCount++;
		}
	}
}

void demoInterruptsBitrateAckPayload() {
	
	delay(DEMO_INTERVAL_MILLIS);

	debugln("Interrupts bitrate ACK payload");
	
	attachInterrupt(1, radioInterrupt, FALLING);
	_showMessageInInterrupt = 0;
	_successPacketCount = 0;
	_failedPacketCount = 0;
	_ackAPacketCount = 0;
	_ackBPacketCount = 0;

	_endMillis = millis() + DEMO_LENGTH_MILLIS;
	_lastMillis = millis();
	
	while (millis() < _endMillis) {
		
		_currentMillis = millis();
		
		if (_currentMillis - _lastMillis > 999) {
			_packetCount = _successPacketCount + _failedPacketCount;
			_bitsPerSecond = sizeof(RadioPacket) * _successPacketCount * 8 / (float)(_currentMillis - _lastMillis) * 1000;
			_packetLoss = _successPacketCount / (float)_packetCount * 100.0;
			debug("  ");
			debug(_successPacketCount); debug("/"); debug(_packetCount); debug(" packets ");
			debug(_packetLoss); debug("% A");
			debug(_ackAPacketCount);
			debug(" B");
			debug(_ackBPacketCount);
			debug(" "); debug(_bitsPerSecond); debugln(" bps");
			_successPacketCount = 0;
			_failedPacketCount = 0;
			_ackAPacketCount = 0;
			_ackBPacketCount = 0;
			_lastMillis = _currentMillis;
		}
		
		_radio.startSend(DESTINATION_RADIO_ID, &_radioData, sizeof(RadioPacket), NRFLite::REQUIRE_ACK);
	}
	
	detachInterrupt(1);
}

void demoPollingBitrateAllPacketSizes() {
	
	delay(DEMO_INTERVAL_MILLIS);
	
	debugln("Polling bitrate all packet sizes");
	
	_successPacketCount = 0;
	_failedPacketCount = 0;
	_lastMillis = millis();
	
	uint8_t packet[32];
	uint8_t packetSize = 1;
	
	while (1) {

		_currentMillis = millis();
		
		if (_currentMillis - _lastMillis > 2999) { // Send 3 seconds worth of packets for each size.
			
			_packetCount = _successPacketCount + _failedPacketCount;
			_bitsPerSecond = packetSize * _successPacketCount * 8 / (float)(_currentMillis - _lastMillis) * 1000;
			_packetLoss = _successPacketCount / (float)_packetCount * 100.0;
			
			debug("  Size "); debug(packetSize); debug(" ");
			debug(_successPacketCount); debug("/"); debug(_packetCount); debug(" packets ");
			debug(_packetLoss); debug("% ");
			debug(_bitsPerSecond); debugln(" bps");
			
			_successPacketCount = 0;
			_failedPacketCount = 0;
			_lastMillis = _currentMillis;
			
			if (packetSize == 32) break; // Stop demo when we reach max packet size.

			packetSize++; // Increase size of the packet to send.
			delay(DEMO_INTERVAL_MILLIS);
		}
		
		if (_radio.send(DESTINATION_RADIO_ID, &packet, packetSize, NRFLite::REQUIRE_ACK)) {
			_successPacketCount++;
		}
		else {
			_failedPacketCount++;
		}
	}
}

void demoPollingBitrateAllAckSizes() {
	
	// Sends a RadioData packet which is 32 bytes, but receives ACK packets that increase in length.
	
	delay(DEMO_INTERVAL_MILLIS);
	
	debugln("Polling bitrate all ACK sizes");
	
	_successPacketCount = 0;
	_failedPacketCount = 0;
	_lastMillis = millis();
	
	uint8_t ackPacket[32];
	uint8_t ackPacketSize;
	uint8_t lastAckPacketSize = 1;
	
	while (1) {

		_currentMillis = millis();
		
		if (_currentMillis - _lastMillis > 2999) { // Send 3 seconds worth of packets for each size.
			
			_packetCount = _successPacketCount + _failedPacketCount;
			_bitsPerSecond = sizeof(RadioPacket) * _successPacketCount * 8 / (float)(_currentMillis - _lastMillis) * 1000;
			_packetLoss = _successPacketCount / (float)_packetCount * 100.0;
			
			debug("  ACK Size "); debug(lastAckPacketSize); debug(" ");
			debug(_successPacketCount); debug("/"); debug(_packetCount); debug(" packets ");
			debug(_packetLoss); debug("% ");
			debug(_bitsPerSecond); debugln(" bps");
			
			_successPacketCount = 0;
			_failedPacketCount = 0;
			_lastMillis = _currentMillis;
			
			if (lastAckPacketSize == 32) break; // Stop demo when we reach max packet size.

			delay(DEMO_INTERVAL_MILLIS);
		}
		
		if (_radio.send(DESTINATION_RADIO_ID, &_radioData, sizeof(RadioPacket), NRFLite::REQUIRE_ACK)) {
			
			_successPacketCount++;
			ackPacketSize = _radio.hasAckData();
			
			while (ackPacketSize > 0) {
				lastAckPacketSize = ackPacketSize; // Store ACK size for display.
				_radio.readData(&ackPacket);
				ackPacketSize = _radio.hasAckData();
			}
		}
		else {
			_failedPacketCount++;
		}
	}
}