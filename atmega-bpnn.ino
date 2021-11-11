/*
    ATMEGA328 Backpropagation neural network

    MMI codes:

      00*#               repeat the last code
      11*XX*YY*ZZ#       set input XX, hidden YY, ZZ layers before NN creation
      12*XX#             set pattern count before nn creation
      21*XX#             set learning rate divided by 100
      22*XX#             set momentum divided by 100
      23*XX#             set max epochs
      24*XX#             set desired error divided by 1000000
      25*XX#             set calculation interval ms
      31*XX*YY           assign XX pin to YY input neuron number, YY = 0..neurons-1
      32*XX*YY           assign XX pin to YY output neuron number, YY = 0..neurons-1
      33*XX*YY*ZZ#       set pattern at XX index from YY pin and ZZ value if NN created
      34*XX*YY*ZZ#       set target at XX index from YY pin and ZZ value if NN created
      41*#               create neural network
      42*#               start neural network training, press any key to abort
      43*#               enable neural network calculation
      44*#               disable neural network calculation
      51*#               save weights to EEPROM
      52*#               load weights from EEPROM
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <bpnn.h>

#include "strings.h"
#include "pins.h"
#include "scheduler.h"
#include "limits.h"

#define BUZZ_PIN 6
#define NUM_COMMANDS 18
#define DISPLAY_TIME 3 // 3x2s
#define IDLE_TIME 75 // 1,25*60s

BPNN* nn;
LiquidCrystal_I2C lcd(0x3F, 16, 2);

const byte ROWS = 4;
const byte COLS = 4;

char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

const char* MMI_COMMANDS[NUM_COMMANDS]
{
  "*00*",
  "*11*",
  "*12*",
  "*21*",
  "*22*",
  "*23*",
  "*24*",
  "*25*",
  "*31*",
  "*32*",
  "*33*",
  "*34*",
  "*41*",
  "*42*",
  "*43*",
  "*44*",
  "*51*",
  "*52*"
};

byte rowPins[ROWS] = {13, 12, 11, 8};
byte colPins[COLS] = {7, 5, 4, 3};

Keypad keypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

byte blnk = 0;
byte idle = 0;
byte entering = 0;
byte displayInfo = 0;
byte invalidMMI = 0;
byte invalidPin = 0;
byte dispCounter = 0;
byte idleCounter = 0;

String MMI = "";
String lastMMI = "";
String displayTitle = "";
String displayStr = "";

byte numInput = 1;
byte numHidden = 1;
byte numOutput = 1;
unsigned int numPattern = 0;
float learningRate = 0.5;
float momentum = 0.1;
unsigned long maxEpochs = 1000;
float desiredError = 0.01;
byte annCreated = 0;
byte trainingActive = 0;
byte annActive = 0;
unsigned int calculationInterval = 500;

float ** patterns;
float ** targets;

float* input;
float* output;
byte* inputPins;
byte* outputPins;

void setup()
{
  pinMode(BUZZ_PIN, OUTPUT);

  lcd.init();
  lcd.backlight();

  Sch.init();
  Sch.add(ledUpdate, 500);
  Sch.add(keyUpdate, 1);
  Sch.add(annCompute, 500);

  printFilledStr("WELCOME TO MMI", 0);
  printFilledStr("NEURAL NETWORK", 1);
  delay(2000);
  entering = 1;
}

/* Main Program */
void loop()
{
  Sch.dispatch();
}

void keyUpdate()
{

  char key = keypad.getKey();
  if (key)
  {
    if (idle == 1)
    {
      lcd.setBacklight(1);
      tone(BUZZ_PIN, 1000, 50);
      idleCounter = 0;
      idle = 0;
      return;
    }

    entering = 1;
    invalidPin = 0;
    displayInfo = 0;
    idleCounter = 0;

    switch (key)
    {
      case 'A':
        {
          MMI = "";
          entering = 0;
        }
        break;
      case 'B':
        {
          MMI = MMI.substring(0, MMI.length() - 1);
        }
        break;
      case 'C': MMI = ""; break;
      case 'D':
        {
          scanMMI();
          lastMMI = MMI;
          MMI = "";
        }
        break;
      default:
        {
          if (MMI.length() < 16)
          {
            MMI += key;
          }
        }
        break;
    }

    tone(BUZZ_PIN, 1000, 50);
  }
}

void ledUpdate()
{

  idleCounter += 1;
  if (idleCounter == (2 * IDLE_TIME))
  {
    lcd.setBacklight(0);
    idleCounter = 0;
    idle = 1;
  }

  if (invalidMMI == 1)
  {
    printFilledStr("ERROR", 0);
    printFilledStr("INVALID CODE", 1);
    dispCounter += 1;
    if (dispCounter >= DISPLAY_TIME)
    {
      invalidMMI = 0;
      dispCounter = 0;
    }
    return;
  }

  if (invalidPin == 1)
  {
    printFilledStr("ERROR", 0);
    printFilledStr("INVALID PIN", 1);
    dispCounter += 1;
    if (dispCounter >= DISPLAY_TIME)
    {
      invalidPin = 0;
      dispCounter = 0;
    }
    return;
  }

  if (entering == 1)
  {
    printFilledStr("ENTER MMI CODE", 0);
    if (blnk == 0)
    {
      printFilledStr(MMI, 1);
    }
    else
    {
      printFilledStr(MMI + '_', 1);
    }
    blnk += 1;
    if (blnk == 2) blnk = 0;
    return;
  }

  if (displayInfo == 1)
  {
    printFilledStr(displayTitle, 0);
    printFilledStr(displayStr, 1);
    dispCounter += 1;
    if (dispCounter >= (DISPLAY_TIME * 2))
    {
      displayInfo = 0;
      dispCounter = 0;
    }
    return;
  }

  lcd.clear();
}

void annCompute()
{
  if ((annActive == 0) || (annCreated == 0))
  {
    return;
  }

  int i, j;
  for (i = 0; i < numInput; i++)
  {
    input[i] = analogRead(inputPins[i]) / 1023.0;
  }
  output = nn->compute(input);

  for (j = 0; j < numOutput; j++)
  {
    if (output[j] < 0) output[j] = 0;
    analogWrite(outputPins[j], output[j] * 255);
  }

}

void scanMMI()
{
  int i;
  invalidMMI = 1;

  entering = 1;
  String cmd = MMI.substring(0, 4);
  String params = MMI.substring(4, MMI.length() - 1);
  String existingCmd;

  int len = MMI.length();

  for (i = 0; i < NUM_COMMANDS; i++)
  {
    existingCmd = MMI_COMMANDS[i];
    if (cmd == existingCmd)
    {
      invalidMMI = 0;
      entering = 0;
      break;
    }
  }

  if ((len > 0) && (MMI[len - 1] != '#'))
  {
    invalidMMI = 1;
    entering = 1;
  }

  if (invalidMMI == 0)
  {
    execMMI(cmd, params);
  }
}

void execMMI(String cmd, String params)
{
  String cm = cmd.substring(1, 3);
  int command = cm.toInt();
  String svalue0 = trimAll(getParam(params, '*', 0));
  String svalue1 = trimAll(getParam(params, '*', 1));
  String svalue2 = trimAll(getParam(params, '*', 2));

  switch (command)
  {
    case 00: //repeat last MMI command
      {
        if (lastMMI != "")
        {
          MMI = lastMMI;
          scanMMI();
        }
      }
      break;
    case 11:
      {
        numInput = limitUintValue(svalue0.toInt(), 1, 3);
        numHidden = limitUintValue(svalue1.toInt(), 1, 6);
        numOutput = limitUintValue(svalue2.toInt(), 1, 3);
        input = new float[numInput];
        inputPins = new byte[numInput];
        output = new float[numOutput];
        outputPins = new byte[numOutput];
      }
      break;
    case 12:
      {
        numPattern = limitUintValue(svalue0.toInt(), 1, 10);
        patterns = new float* [numPattern];
        targets = new float* [numPattern];
        int i;
        for (int i = 0; i < numPattern; i++)
        {
          patterns[i] = new float[numInput];
          targets[i] = new float[numOutput];
        }
      }
      break;

    case 21:
      {
        learningRate = (float) limitUintValue(svalue0.toInt(), 1, 32767) / 100.0;
      }
      break;
    case 22:
      {
        momentum = (float) limitUintValue(svalue0.toInt(), 1, 32767) / 100.0;
      }
      break;
    case 23:
      {
        maxEpochs = limitUintValue(svalue0.toInt(), 1, 1000000);
      }
      break;
    case 24:
      {
        desiredError = (float) limitUintValue(svalue0.toInt(), 1, 1000000) / 1000000.0;
      }
      break;
    case 25:
      {
        calculationInterval = limitUintValue(svalue0.toInt(), 50, 65535);
        Sch.setPeriod(2, calculationInterval);
      }
      break;

    case 31:
      {
        unsigned short pin = limitUintValue(svalue0.toInt(), 14, 19);
        unsigned short neuron = limitUintValue(svalue1.toInt(), 0, numInput - 1);
        invalidPin = checkAnalogPin(pin) ? 0 : 1;
        if (invalidPin == 0)
        {
          inputPins[neuron] = pin;
          pinMode(pin, INPUT);
        }
      }
      break;
    case 32:
      {
        unsigned short pin = limitUintValue(svalue0.toInt(), 0, 19);
        unsigned short neuron = limitUintValue(svalue1.toInt(), 0, numOutput - 1);
        invalidPin = checkPWMPin(pin) ? 0 : 1;
        if (invalidPin == 0)
        {
          outputPins[neuron] = pin;
          pinMode(pin, OUTPUT);
        }
      }
      break;
    case 33:
      {
        unsigned short index = limitUintValue(svalue0.toInt(), 0, numPattern - 1);
        unsigned short neuron = limitUintValue(svalue1.toInt(), 0, numInput - 1);
        unsigned short value = limitUintValue(svalue2.toInt(), 0, 1023);

        patterns[index][neuron] = value / 1023.0;
      }
      break;
    case 34:
      {
        unsigned short index = limitUintValue(svalue0.toInt(), 0, numPattern - 1);
        unsigned short neuron = limitUintValue(svalue1.toInt(), 0, numInput - 1);
        unsigned short value = limitUintValue(svalue2.toInt(), 0, 1023);

        targets[index][neuron] = value / 255.0;
      }
      break;

    case 41:
      {
        nn = new BPNN(numInput, numHidden, numOutput);
        annCreated = 1;
      }
      break;
    case 42:
      {
        if ((numPattern > 0) && (annCreated == 1))
        {
          trainingActive = 1;
          printFilledStr("NETWORK TRAINING", 0);
          printFilledStr("PLEASE WAIT...", 1);
          int endepochs; float enderror;
          nn->train(patterns, targets, numPattern, maxEpochs, desiredError, learningRate, momentum, &endepochs, &enderror);
          printFilledStr("EPOCHS: " + String(endepochs), 0);
          printFilledStr("ERROR : " + String(enderror), 1);
          delay(2000);
        }
      }
      break;
    case 43:
      {
        annActive = 1;
      }
      break;
    case 44:
      {
        annActive = 0;
      }
      break;

    case 51:
      {
        if (annCreated)
        {
          nn->save();
        }
      }
      break;
    case 52:
      {
        if (annCreated)
        {
          nn->load();
        }
      }
      break;

  }
}

void printFilledStr(String s, int row)
{
  lcd.setCursor(0, row);
  lcd.print(filledStr(s, 16));
}
