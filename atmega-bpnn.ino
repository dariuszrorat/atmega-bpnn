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

#include "mmicode.h"
#include "strings.h"
#include "pins.h"
#include "scheduler.h"
#include "limits.h"

#define BUZZ_PIN 6
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

String shortcode = "";
String lastShortcode = "";
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

  Mmi.init();
  Mmi.add("*00*", repeatLastCode);
  Mmi.add("*11*", setlayers);
  Mmi.add("*12*", setPatternCount);
  Mmi.add("*21*", setLearningRate);
  Mmi.add("*22*", setMomentum);
  Mmi.add("*23*", setMaxEpochs);
  Mmi.add("*24*", setDesiredError);
  Mmi.add("*25*", setCalculationInterval);
  Mmi.add("*31*", assignInput);
  Mmi.add("*32*", assignOutput);
  Mmi.add("*33*", setPattern);
  Mmi.add("*34*", setTarget);
  Mmi.add("*41*", createNetwork);
  Mmi.add("*42*", startTraining);
  Mmi.add("*43*", enableCalculation);
  Mmi.add("*44*", disableCalculation);
  Mmi.add("*51*", saveWeights);
  Mmi.add("*52*", loadWeights);

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
          shortcode = "";
          entering = 0;
        }
        break;
      case 'B':
        {
          shortcode = shortcode.substring(0, shortcode.length() - 1);
        }
        break;
      case 'C': shortcode = ""; break;
      case 'D':
        {
          scanMMI();
          lastShortcode = shortcode;
          shortcode = "";
        }
        break;
      default:
        {
          if (shortcode.length() < 16)
          {
            shortcode += key;
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
    printError("ERROR", "INVALID CODE", &invalidMMI);
    return;
  }

  if (invalidPin == 1)
  {
    printError("ERROR", "INVALID PIN", &invalidPin);
    return;
  }

  if (entering == 1)
  {
    printEntering();
    return;
  }

  if (displayInfo == 1)
  {
    printInfo();
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
  invalidMMI = 1;
  entering = 1;
  bool execResult = Mmi.exec(shortcode);
  invalidMMI = execResult ? 0 : 1;
  entering = execResult ? 0 : 1;
}

void repeatLastCode(long val0, long val1, long val2)
{
  if (lastShortcode != "")
  {
    shortcode = lastShortcode;
    scanMMI();
  }
}

void setlayers(long val0, long val1, long val2)
{
  numInput = limitUintValue(val0, 1, 3);
  numHidden = limitUintValue(val1, 1, 6);
  numOutput = limitUintValue(val2, 1, 3);
  input = new float[numInput];
  inputPins = new byte[numInput];
  output = new float[numOutput];
  outputPins = new byte[numOutput];
}

void setPatternCount(long val0, long val1, long val2)
{
  numPattern = limitUintValue(val0, 1, 10);
  patterns = new float* [numPattern];
  targets = new float* [numPattern];
  int i;
  for (int i = 0; i < numPattern; i++)
  {
    patterns[i] = new float[numInput];
    targets[i] = new float[numOutput];
  }
}

void setLearningRate(long val0, long val1, long val2)
{
  learningRate = (float) limitUintValue(val0, 1, 32767) / 100.0;
}

void setMomentum(long val0, long val1, long val2)
{
  momentum = (float) limitUintValue(val0, 1, 32767) / 100.0;
}

void setMaxEpochs(long val0, long val1, long val2)
{
  maxEpochs = limitUintValue(val0, 1, 1000000);
}

void setDesiredError(long val0, long val1, long val2)
{
  desiredError = (float) limitUintValue(val0, 1, 1000000) / 1000000.0;
}

void setCalculationInterval(long val0, long val1, long val2)
{
  calculationInterval = limitUintValue(val0, 50, 65535);
  Sch.setPeriod(2, calculationInterval);
}

void assignInput(long val0, long val1, long val2)
{
  unsigned short pin = limitUintValue(val0, 14, 19);
  unsigned short neuron = limitUintValue(val1, 0, numInput - 1);
  invalidPin = checkAnalogPin(pin) ? 0 : 1;
  if (invalidPin == 0)
  {
    inputPins[neuron] = pin;
    pinMode(pin, INPUT);
  }
}

void assignOutput(long val0, long val1, long val2)
{
  unsigned short pin = limitUintValue(val0, 0, 19);
  unsigned short neuron = limitUintValue(val1, 0, numOutput - 1);
  invalidPin = checkPWMPin(pin) ? 0 : 1;
  if (invalidPin == 0)
  {
    outputPins[neuron] = pin;
    pinMode(pin, OUTPUT);
  }
}

void setPattern(long val0, long val1, long val2)
{
  unsigned short index = limitUintValue(val0, 0, numPattern - 1);
  unsigned short neuron = limitUintValue(val1, 0, numInput - 1);
  unsigned short value = limitUintValue(val2, 0, 1023);

  patterns[index][neuron] = value / 1023.0;
}

void setTarget(long val0, long val1, long val2)
{
  unsigned short index = limitUintValue(val0, 0, numPattern - 1);
  unsigned short neuron = limitUintValue(val1, 0, numInput - 1);
  unsigned short value = limitUintValue(val2, 0, 1023);

  targets[index][neuron] = value / 255.0;
}

void createNetwork(long val0, long val1, long val2)
{
  nn = new BPNN(numInput, numHidden, numOutput);
  annCreated = 1;
}

void startTraining(long val0, long val1, long val2)
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

void enableCalculation(long val0, long val1, long val2)
{
  annActive = 1;
}

void disableCalculation(long val0, long val1, long val2)
{
  annActive = 0;
}

void saveWeights(long val0, long val1, long val2)
{
  if (annCreated)
  {
    nn->save();
  }
}

void loadWeights(long val0, long val1, long val2)
{
  if (annCreated)
  {
    nn->load();
  }
}

void printFilledStr(String s, int row)
{
  lcd.setCursor(0, row);
  lcd.print(filledStr(s, 16));
}

void printError(String caption, String message, byte *code)
{
  printFilledStr(caption, 0);
  printFilledStr(message, 1);
  dispCounter += 1;
  if (dispCounter >= DISPLAY_TIME)
  {
    *code = 0;
    dispCounter = 0;
  }
}

void printEntering()
{
  printFilledStr("ENTER MMI CODE", 0);
  if (blnk == 0)
  {
    printFilledStr(shortcode, 1);
  }
  else
  {
    printFilledStr(shortcode + '_', 1);
  }
  blnk += 1;
  if (blnk == 2) blnk = 0;
}

void printInfo()
{
  printFilledStr(displayTitle, 0);
  printFilledStr(displayStr, 1);
  dispCounter += 1;
  if (dispCounter >= (DISPLAY_TIME * 2))
  {
    displayInfo = 0;
    dispCounter = 0;
  }
}
