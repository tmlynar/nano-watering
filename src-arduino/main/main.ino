#include <EtherCard.h>
#include <EEPROM.h>
#include <OneButton.h>

#define DEFAULT_VALUE 15
#define DEFAULT_VALUE_POS 8
byte defaultValue = DEFAULT_VALUE;

#define MODE_MIN 0
#define MODE_SEC 1

typedef struct {
  byte pin;
  byte value;
  uint32_t timestmap;
  byte mode;
} OUTPUT_STRUCT;

OUTPUT_STRUCT outputs[] = {
  {2, 0, 0, MODE_MIN}, 
  {3, 0, 0, MODE_MIN}, 
  {4, 0, 0, MODE_MIN}, 
  {5, 0, 0, MODE_MIN}, 
  {6, 0, 0, MODE_MIN}, 
  {7, 0, 0, MODE_MIN}, 
  {9, 0, 0, MODE_SEC}, 
  {10, 0, 0, MODE_SEC}
};

int MODES[] = {
  60,
  1
};

OneButton inputs[] {
  OneButton(14, true),
  OneButton(15, true),
  OneButton(16, true),
  OneButton(17, true)
 };


void setup(){
  Serial.begin(57600);
  Serial.println("\n[WML]");
  delay(100);

  EEPROM.get(DEFAULT_VALUE_POS, defaultValue);
  if (defaultValue == 0) {
    defaultValue = DEFAULT_VALUE;
  }
  
  etherInit();
  
  for (byte i = 0; i < sizeof(outputs) / sizeof(OUTPUT_STRUCT); i++){
    byte value;
    EEPROM.get(i, value);
    digitalWrite(outputs[i].pin, 1);
    pinMode(outputs[i].pin, OUTPUT);
    if (value != 0xFF) {
      outputs[i].value = value;
      outputs[i].timestmap = (uint32_t) millis() / 1000;
      EEPROM.get(16 + i, value);
      if (value == MODE_SEC || value == MODE_MIN) {
        outputs[i].mode = value;
      }
    }
  }

  inputs[0].attachClick(button1ClickFunction);
  inputs[0].attachDoubleClick(button1DoubleClickFunction);
  inputs[0].attachLongPressStart(button1LongPressStartFunction);

  inputs[1].attachClick(button2ClickFunction);
  inputs[1].attachDoubleClick(button2DoubleClickFunction);

  inputs[2].attachClick(button3ClickFunction);
  inputs[2].attachDoubleClick(button3DoubleClickFunction);

  inputs[3].attachClick(button4ClickFunction);
  inputs[3].attachDoubleClick(button4DoubleClickFunction);
}

void toggleOutputs(byte* idxs, byte sizeofArray) {
  Serial.println(sizeof(idxs));
  Serial.println(sizeofArray);
  bool allIsOff = true;
  for (int i =0; i < sizeofArray; i++) {
    if (outputs[idxs[i]].value != 0) {
      allIsOff = false;
      break;
    }
  }
  byte value = allIsOff ? defaultValue : 0;
  for (int i =0; i < sizeofArray; i++) {
      Serial.print("toggleOutputs "); Serial.println(idxs[i]);
      processOutput(idxs[i], value);
  }
}

void button1ClickFunction() {
  byte toutputs[] = {0, 1};
  toggleOutputs(toutputs, sizeof(toutputs));
}

void button1DoubleClickFunction() {
  byte toutputs[] = {1};
  toggleOutputs(toutputs, sizeof(toutputs));
}

void button1LongPressStartFunction() {
  byte toutputs[sizeof(outputs) / sizeof(OUTPUT_STRUCT)];
  for (int i =0; i < sizeof(toutputs); i++) {
      Serial.print("Long "); Serial.print(sizeof(toutputs)); Serial.print(" : "); Serial.println(i);  
      toutputs[i] = i;
  }
  toggleOutputs(toutputs, sizeof(toutputs));
}

void button2ClickFunction() {
  byte toutputs[] = {2, 3};
  toggleOutputs(toutputs, sizeof(toutputs));
}
void button2DoubleClickFunction() {
  byte toutputs[] = {3};
  toggleOutputs(toutputs, sizeof(toutputs));
}
void button3ClickFunction() {
  byte toutputs[] = {4, 5};
  toggleOutputs(toutputs, sizeof(toutputs));
}
void button3DoubleClickFunction() {
  byte toutputs[] = {5};
  toggleOutputs(toutputs, sizeof(toutputs));
}
void button4ClickFunction() {
  byte toutputs[] = {6, 7};
  toggleOutputs(toutputs, sizeof(toutputs));
}
void button4DoubleClickFunction() {
  byte toutputs[] = {7};
  toggleOutputs(toutputs, sizeof(toutputs));
  etherInit();
}

uint32_t lastT = 0;
uint32_t lastLoopMills = 0;

void loop () {
  networkLoop();

  // outputs
  uint32_t t = millis();
  if (t - lastT >= 250) {
    lastT = t;
    for (byte i = 0; i < sizeof(outputs) / sizeof(OUTPUT_STRUCT); i++) {
      OUTPUT_STRUCT output = outputs[i];
      byte targetState = HIGH;
      if (output.value > 0) {
        if ((uint32_t) (t/ 1000) - output.timestmap > MODES[output.mode] * output.value)
        {
          outputs[i].value = 0;
          EEPROM.write(i, 0);
        }
        else {
          targetState = LOW;
        }
      }
      byte currentState = digitalRead(output.pin);
      if (currentState != targetState) {
        digitalWrite(output.pin, targetState);
        break;
      }
    }
  }
  if (((uint32_t) millis() - lastLoopMills) > 10) {
    for (byte i = 0; i < sizeof(inputs) / sizeof(OneButton); i++){
      inputs[i].tick();
    }
    lastLoopMills = millis();
  }
}

void processOutput(byte index, int value) {
  if (outputs[index].value != value) {
      EEPROM.write(index, value);
  }
  if (value == 0) {
    outputs[index].timestmap = (uint32_t) 0;
    outputs[index].value = (byte) 0;
  }
  else {
    outputs[index].timestmap = (uint32_t) millis() / 1000;
    outputs[index].value = value;
  }
  Serial.print ("processOutput[");Serial.print (index);Serial.print ("]=");Serial.print (value); Serial.print (" "); Serial.print (" "); Serial.println (outputs[index].value);
}
