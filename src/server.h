#include <Arduino.h>
#include "display.h"

#define PIN_TX 18 // Serial1
#define PIN_RX 19

#define ADDR 0x17
#define BAUDRATE 10400

#define TIMEOUT 500 // 1300

///VARIABLES/TYPES
static const uint8_t KWP_ACKNOWLEDGE             = 0x09; // the module has no more data to send
static const uint8_t KWP_REFUSE                  = 0x0A; // the module can not fulfill a request
static const uint8_t KWP_DISCONNECT              = 0x06; // the tester wants to disconnect from the module

static const uint8_t KWP_REQUEST_EXTRA_ID        = 0x00; // response: KWP_RECEIVE_ID_DATA (data available) / KWP_ACKNOWLEDGE (no data available)
static const uint8_t KWP_REQUEST_LOGIN           = 0x2B; // response: KWP_ACKNOWLEDGE (login successful) / KWP_REFUSE (login not successful)
static const uint8_t KWP_REQUEST_RECODE          = 0x10; // response: KWP_REQUEST_EXTRA_ID...
static const uint8_t KWP_REQUEST_FAULT_CODES     = 0x07; // response: KWP_RECEIVE_FAULT_CODES (ok) / KWP_REFUSE (fault codes not supported)
static const uint8_t KWP_REQUEST_CLEAR_FAULTS    = 0x05; // response: KWP_ACKNOWLEDGE (ok) / KWP_REFUSE (clearing fault codes not supported)
static const uint8_t KWP_REQUEST_ADAPTATION      = 0x21; // response: KWP_RECEIVE_ADAPTATION (ok) / KWP_REFUSE (invalid channel)
static const uint8_t KWP_REQUEST_ADAPTATION_TEST = 0x22; // response: KWP_RECEIVE_ADAPTATION (ok) / KWP_REFUSE (invalid channel)
static const uint8_t KWP_REQUEST_ADAPTATION_SAVE = 0x2A; // response: KWP_RECEIVE_ADAPTATION (ok) / KWP_REFUSE (invalid channel or value)
static const uint8_t KWP_REQUEST_GROUP_READING   = 0x29; // response: KWP_RECEIVE_GROUP_READING (ok) / KWP_ACKNOWLEDGE (empty group) / KWP_REFUSE (invalid group)
static const uint8_t KWP_REQUEST_GROUP_READING_0 = 0x12; // response: KWP_RECEIVE_GROUP_READING (ok) / KWP_ACKNOWLEDGE (empty group) / KWP_REFUSE (invalid group)
static const uint8_t KWP_REQUEST_READ_ROM        = 0x03; // response: KWP_RECEIVE_ROM (ok) / KWP_REFUSE (reading ROM not supported or invalid parameters)
static const uint8_t KWP_REQUEST_OUTPUT_TEST     = 0x04; // response: KWP_RECEIVE_OUTPUT_TEST (ok) / KWP_REFUSE (output tests not supported)
static const uint8_t KWP_REQUEST_BASIC_SETTING   = 0x28; // response: KWP_RECEIVE_BASIC_SETTING (ok) / KWP_ACKNOWLEDGE (empty group) / KWP_REFUSE (invalid channel or not supported)
static const uint8_t KWP_REQUEST_BASIC_SETTING_0 = 0x11; // response: KWP_RECEIVE_BASIC_SETTING (ok) / KWP_ACKNOWLEDGE (empty group) / KWP_REFUSE (invalid channel or not supported)

static const uint8_t KWP_RECEIVE_ID_DATA         = 0xF6; // request: connect/KWP_REQUEST_EXTRA_ID/KWP_REQUEST_RECODE
static const uint8_t KWP_RECEIVE_FAULT_CODES     = 0xFC; // request: KWP_REQUEST_FAULT_CODES
static const uint8_t KWP_RECEIVE_ADAPTATION      = 0xE6; // request: KWP_REQUEST_ADAPTATION/KWP_REQUEST_ADAPTATION_TEST/KWP_REQUEST_ADAPTATION_SAVE
static const uint8_t KWP_RECEIVE_GROUP_READING   = 0xE7; // request: KWP_REQUEST_GROUP_READING
static const uint8_t KWP_RECEIVE_ROM             = 0xFD; // request: KWP_REQUEST_READ_ROM
static const uint8_t KWP_RECEIVE_OUTPUT_TEST     = 0xF5; // request: KWP_REQUEST_OUTPUT_TEST
static const uint8_t KWP_RECEIVE_BASIC_SETTING   = 0xF4; // request: KWP_REQUEST_BASIC_SETTING


bool awake = false;
bool initial_condition = HIGH;

bool connected = false;

uint8_t block_counter = 0;
/**
 * @brief Read OBD input from ECU
 *
 * @return uint8_t The incoming byte or -1 if timeout
 */
int16_t OBD_read()
{
  unsigned long timeout = millis() + TIMEOUT;
  while (!Serial1.available())
  {
    if (millis() >= timeout)
    {
      Serial.println("ERROR: OBD_read() timeout 500 ms");
      return -1;
    }
  }
  int16_t data = Serial1.read();
 // Serial.print("--> ");
  //Serial.println(data, HEX);
  // ECHO
  Serial1.write(data);
  return data;
}

/**
 * @brief Write data to the ECU, wait 5ms before each write to ensure connectivity.
 *
 * @param data The data to send.
 */
void OBD_write(uint8_t data)
{
  delay(10);
  Serial1.write(data);
}

/**
 * @brief Send a request to the ECU
 *
 * @param s Array where the data is stored
 * @param size The size of the request
 * @return true If no errors occured, will resume
 * @return false If errors occured, will disconnect
 */
bool KWP_send_block(uint8_t *s, int size)
{
    //Serial.print("Sending ");
    //for (uint8_t i = 0; i < size; i++)
    //{
    //    Serial.print(s[i], HEX);
    //    Serial.print(" ");
    //}
    //Serial.println();

  for (uint8_t i = 0; i < size; i++)
  {
    uint8_t data = s[i];
    OBD_write(data);

    if (i < size - 1) {
        int16_t complement = OBD_read();
        if (complement != (data ^ 0xFF)) {
            Serial.print("Received: ");
            Serial.print(complement, HEX);
            Serial.print(" Expected: ");
            Serial.println(data ^ 0xFF, HEX);
            return false;
        }
    }
  }
  block_counter++;
  return true;
}

bool KWP_send_syncbytes() {
    uint8_t s[32] = {0x55, 0x01, 0x8A};
    uint8_t size = 3;
    Serial.print("Sending ");
    for (uint8_t i = 0; i < size; i++)
    {
        Serial.print(s[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    for (uint8_t i = 0; i < size; i++)
    {
        uint8_t data = s[i];
        OBD_write(data);

        if (i == 2) {
            int16_t complement = OBD_read();
            if (complement != (data ^ 0xFF)) {
                Serial.print("Received: ");
                Serial.print(complement, HEX);
                Serial.print(" Expected: ");
                Serial.println(data ^ 0xFF, HEX);
                return false;
            }
        }
    }
    block_counter++;
    return true;
}


uint8_t rpm_a = 100;
uint8_t rpm_b = 50;
uint8_t kmh_a = 100;
uint8_t kmh_b = 4;
uint8_t fuel_a = 100;
uint8_t fuel_b = 40;
uint8_t odo_a = 160;
uint8_t odo_b = 3;
long iteration = 0;
bool KWP_send_group_reading(uint8_t group) {
    uint8_t buf[16] = {0x0F, block_counter, KWP_RECEIVE_GROUP_READING, 0x07, kmh_a, kmh_b, 0x01, rpm_a, rpm_b, 0x25, 0x00, 0x01, 0x2C, 0x0A, 0x11, 0x03};
    switch(group) {
        case 1:
            rpm_b++;
            kmh_b++;
            return (KWP_send_block(buf, 16));
        case 2: // 0x24 0x13 0x0C 0x05 // oil temp
            buf[3] = 0x24;
            buf[4] = odo_a;
            buf[5] = odo_b;

            buf[6] = 0x13;
            buf[7] = fuel_a;
            buf[8] = fuel_b;
            
            buf[9] = 0x0C;
            buf[10] = 0x01;
            buf[11] = 0x01;
            
            buf[12] = 0x05;
            buf[13] = 0x0A;
            buf[14] = 0x81;
            iteration++;
            if (iteration % 6 == 0 && fuel_b > 1) {
                fuel_b--;
            }
            if (iteration % 5 == 0) {
                if (odo_b == 0xFF) {
                    odo_a++;
                }
                odo_b++;
            }
            return (KWP_send_block(buf, 16));
        case 3: // 0x05 0x17 0x05 // coolant temp // oil temp
            buf[3] = 0x05;
            buf[4] = 0x0A;
            buf[5] = 0x81;

            buf[6] = 0x17;
            buf[7] = 0x01;
            buf[8] = 0x01;
            
            buf[9] = 0x05;
            buf[10] = 0x0A;
            buf[11] = 0x81;
            
            buf[12] = 0x00;
            buf[13] = 0x01;
            buf[14] = 0x01;
            return (KWP_send_block(buf, 16));
        break;
    }
}

bool KWP_send_fault_codes() {
  uint8_t buf[16] = {0x0F, block_counter, KWP_RECEIVE_FAULT_CODES, 0x46, 0x5A, 0xA3, 0x40, 0x71, 0x23, 0x46, 0x1E, 0x23, 0x46, 0x20, 0x23, 0x03};
  return (KWP_send_block(buf, 16));
}

bool KWP_send_fault_codes_empty() {
  uint8_t buf[7] = {0x06, block_counter, KWP_RECEIVE_FAULT_CODES, 0xFF, 0xFF, 0x88, 0x03};
  return (KWP_send_block(buf, 7));
}

/**
 * @brief The default way to keep the ECU awake is to send an Acknowledge Block.
 * Alternatives include Group Readings..
 *
 * @return true No errors
 * @return false Errors, disconnect
 */
bool KWP_send_ack()
{
  uint8_t buf[4] = {0x03, block_counter, 0x09, 0x03};
  return (KWP_send_block(buf, 4));
}

bool KWP_send_devicedata() {
    uint8_t s[32] = {0x0F, block_counter, 0xF6, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x20, 0x20, 0x03};
    uint8_t size = 0x0F + 1;
    return KWP_send_block(s, size);
}

bool KWP_receive_ack() {
  unsigned long timeout = millis() + TIMEOUT;
  int16_t data = 0;
  uint8_t s[32];
  int recvcount = 0;

  while(recvcount < 4) {
    while (Serial1.available()) {

        data = OBD_read();
        if (data == -1)
        {
            Serial.println("receive ack error AVA=0 or empty buffer");
            return false;
        }
        s[recvcount] = data;
        recvcount++;

        if (recvcount >= 4) {
            timeout = millis() + TIMEOUT;
            break;
        }
        

        if (recvcount == 2 && data != block_counter)
        {
            Serial.print("block counter error Received: ");
            Serial.print(data, HEX);
            Serial.print(" Expected: ");
            Serial.println(block_counter, HEX);
            return false;
        }

            delay(5);
            OBD_write(data ^ 0xFF); // send complement ack
        

        timeout = millis() + TIMEOUT;

        // debugstrnum(F(" - KWP_receive_block: Added timeout. ReceiveCount: "), (uint8_t)recvcount);
        // debug(F(". Processed data: "));
        // debughex(data);
        // debugstrnumln(F(". ACK compl: "), ((!ackeachbyte) && (recvcount == size)) || ((ackeachbyte) && (recvcount < size)));
    }


    if (millis() >= timeout)
    {
      Serial.print("Timeout - recvcount = ");
      Serial.println(recvcount);
      return false;
    }
  }
  
  if (s[0] != 0x03 || s[1] != block_counter || s[2] != 0x09 || s[3] != 0x03) {
    Serial.println("received gibberish - expected ack block");
    return false;
  }

    block_counter++;

    return true;
}

// 9600 baud, addr 0x01
bool wait_5baud() {
    pinMode(PIN_RX, INPUT);
    initial_condition = digitalRead(PIN_RX);
    Serial.print("initial_condition ");
    Serial.println(initial_condition);
    if (initial_condition == LOW) {
        initial_condition = HIGH;
        return false;
    }
    Serial.println("waiting 10400 baud 0x17 addr PIN_RX_19 ...");
    g.print("Waiting 5baud..", LEFT, rows[4]);
    bool ready = false;
    while (!ready) {
        if (digitalRead(PIN_RX) == LOW) {
            ready = true;
        }
    }
    const int timeout_temp_bug = 3000;
    unsigned long timeout_temp_bug_start = millis();
    while (digitalRead(PIN_RX) == HIGH) { 
        if (millis() - timeout_temp_bug_start > timeout_temp_bug) {
            return false;
        }
        delay(100);
    }
    Serial.println("initial_condition changed");
    g.setColor(TFT_GREEN);
    g.print("---", RIGHT, rows[4]);
    g.setColor(font_color);
    bool bits[10];
    bits[0] = LOW;
    Serial.print(bits[0]);
    delay(200);
    for (uint8_t i = 1; i < sizeof(bits); i++)
    {
        bits[i] = digitalRead(PIN_RX);
        delay(200);
    }
    Serial.print(" ");
    for (uint8_t i = 0; i < sizeof(bits); i++)
    {
        Serial.print(bits[i]);
        Serial.print(" ");
        g.printNumI(bits[i], cols[2 + i*2], rows[5], 1);
    }
    Serial.println();
    for (uint8_t i = 1; i < sizeof(bits); i++) { 
        if (((i <= 1 || i == 2 || i == 3 || i == 5 || i == 8 || i >= 9) && bits[i] == LOW) // 0111010011
                || ((i == 4 || i == 6 || i == 7) && bits[i] == HIGH)) {
            Serial.println("wrong 5baud address");
            initial_condition = HIGH;
            g.print("   ", RIGHT, rows[4]);
            return false;
        }
    }
    g.setColor(TFT_GREEN);
    g.print("---", RIGHT, rows[5]);
    return true;
}

bool KWP_receive_block(uint8_t buff[], uint8_t &received_count, uint8_t &message_type) {
    uint8_t recvcount = 0;
    unsigned long timeout = millis() + TIMEOUT;

    bool received_message_end = false;
    while (!received_message_end) {
        while (Serial1.available()) {
            uint8_t data = OBD_read();
            if (data == -1) {
                Serial.println("data = -1");
                return false;
            }
            buff[recvcount] = data;
            recvcount++;

            switch(recvcount) {
                case 0:
                    Serial.println("recv 0 should not happen!!");
                    break;
                case 1:
                    if (data < 3 || data > 12) {
                        Serial.print("WARNING: very long message ahead");
                    }
                    //Serial.print("message length: ");
                    //Serial.println(data, HEX);
                break;
                case 2:
                    if (data != block_counter) {
                        Serial.print("WARNING: block counter does not match");
                    }
                break;
                case 3:
                    message_type = data;
                    //Serial.print("message type: ");
                    //Serial.println(data, HEX);
                break;
                default:
                    if (data == 0x03) {
                        if ((message_type == KWP_REQUEST_GROUP_READING && recvcount > 4) // Group reading
                                || (message_type == KWP_REQUEST_FAULT_CODES && recvcount > 3) // fault codes
                                || (message_type != KWP_REQUEST_GROUP_READING && message_type != KWP_REQUEST_FAULT_CODES)  // Default
                            ) {
                            received_message_end = true;
                            received_count = recvcount;
                            block_counter++;
                            return true;
                        }
                    }
                    
                break;
            }
            
            OBD_write(data ^ 0xFF); // send complement ack
            timeout = millis() + TIMEOUT;

        }
        
        if (millis() >= timeout)
        {
            Serial.print("Timeout - recvcount = ");
            Serial.println(recvcount);
            error_timeout();
            return false;
        }
    }
    
    received_count = recvcount;
    block_counter++;
    return true;
}

void reset() {
    connected = false;
    awake = false;
    block_counter = 0;
    initial_condition = HIGH;
    
    Serial.println("Waiting 3 sec");
    delay(3000);
    for (uint8_t i = 4; i < 20; i++) {
        clearRow(i);
    }
    Serial1.end();
}

bool wakeup() {
    if (!wait_5baud()) {
        Serial.println("5baud error");
        g.setColor(TFT_RED);
        g.print("xxx", RIGHT, rows[5]);
        g.setColor(font_color);
        reset();
        return false;
    } 
    awake = true;
    initial_condition = HIGH;
    Serial.println("5baud success");
    //Serial.println("Waiting 75 ms");
    //delay(75);
    return true;
}

bool connect() {

    Serial1.begin(BAUDRATE);
    if (!KWP_send_syncbytes()) {
        Serial.println("syncbytes error");
        reset();
        return false;
    }
    g.print("Sending syncbytes..", LEFT, rows[6]);
    Serial.println("syncbytes success");

    for (uint8_t i = 0; i < 4; i++) {
        Serial.print("-> sending device data ");
        Serial.print(i+1);
        Serial.println(" / 4");
        if (!KWP_send_devicedata()) {
            Serial.println("send device data error");
            g.setColor(TFT_RED);
            g.print("xxx", RIGHT, rows[6]);
            g.setColor(font_color);
            reset();
            return false;
        }
        if (i < 3) {
            Serial.print("---> receive ack block ");
            Serial.print(i+1);
            Serial.println(" / 4");
            if (!KWP_receive_ack()) {
                Serial.println("receive ack block error");
                g.setColor(TFT_RED);
                g.print("xxx", RIGHT, rows[6]);
                g.setColor(font_color);
                reset();
                return false;
            }
        }
    }
    Serial.println("device data success");
    g.setColor(TFT_GREEN);
    g.print("---", RIGHT, rows[6]);

    //g.print("Sending ack..", LEFT, rows[7]);
    //Serial.println("sending ack block");
    //if (!KWP_send_ack()) {
    //    Serial.println("send ack error");
    //    g.setColor(TFT_RED);
    //    g.print("failed", RIGHT, rows[7]);
    //    g.setColor(font_color);
    //    reset();
    //    return false;
    //}
    connected = true;

    g.setColor(TFT_YELLOW);
    g.print("connected", RIGHT, rows[7]);
    g.setColor(font_color);
    draw_line_on_row(8);
    draw_line_on_row(12);
    g.setColor(TFT_GREEN);
    Serial.println("------------------------------------------");
    Serial.println("|               connected                |");
    Serial.println("|----------------------------------------|");
    Serial.println("|keep alive by sending ack within 500 ms |");
    Serial.println("|----------------------------------------|");
    return true; 
}
