#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include "EEPROM.h"
#include "Keypad.h"

#define TOP_OF_STACK          2147483647
#define INT_STACK_MAX         512
#define INT_MAX               2147483646
#define INT_MIN               -2147483648
#define NO_DECIMAL            100

#define MAX_QUEUE_SIZE        9
#define STACK_DISPLAY_HEIGHT  4
#define SCREEN_WIDTH          14
#define FUNC_NAME_LENGTH      3
#define NUM_FUNCTIONS         5
#define ROWS                  6 //four rows
#define COLS                  5 //four columns
#define ERROR_TIMEOUT         1000
#define NUM_DIGITS_PI         4

#define ROLL_UP               'U'
#define ROLL_DOWN             'D'
#define CLEAR                 'C'
#define NEGATIVE              'N'
#define ENTER                 'E'
#define BACK_SPACE            'B'
#define BINARY                'I'
#define OCTAL                 'O'
#define DECIMAL               'T'
#define HEXADECIMAL           'H'
#define FLOATING              'F'
#define ADV_FUNC              'A'
#define SIN                   '&'
#define COS                   '|'
#define TAN                   '~'
#define PUSH_PI               OCTAL
#define CHANGE_ANGLE          BINARY
#define IMAGINARY             DECIMAL
#define COMPLEX_MODE          HEXADECIMAL

#define LOGO16_GLCD_HEIGHT    16
#define LOGO16_GLCD_WIDTH     16

#define BATTERY_HEIGHT        8
#define BATTERY_WIDTH         5
#define ANGLE_SYMBOL_HEIGHT   8
#define ANGLE_SYMBOL_WIDTH    8

enum numberBase{
  binary,
  octal,
  decimal,
  hexadecimal
};

enum mode{
  integer,
  floating,
  func
};

enum error{
  none,
  divideByZero,
  overFlow,
  stack,
  doesNotExist
};

enum complexFormat{
  polar,
  rectangular,
  binomial,
  ECEbinomial
};

enum angleMode{
  degrees,
  radians,
  gradians
};

struct ComplexFloat{
  float   real;
  float   imaginary;
  boolean top;
  byte    numDecimals;
};

struct EEstack{
  int         topOfStack;
  numberBase  displayFormat;
};

struct FPstack{
  int             topOfStack;
  complexFormat   displayFormat;
};

struct queue{
	byte        data[MAX_QUEUE_SIZE];
	byte        size;
  byte        front;
  boolean     positive;
  numberBase  entryFormat;
};

struct FPqueue{
  byte        data[MAX_QUEUE_SIZE];
  byte        size;
  byte        front;
  boolean     positive;
  boolean     real;
  byte        decimal;
};

static const unsigned char PROGMEM logo16_glcd_bmp[] = {
  B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000 
  };
  
static const uint8_t PROGMEM EMPTY_BATTERY[] = { 
  B00100000,
  B01110000,
  B01010000,
  B01010000,
  B01010000,
  B01010000,
  B01110000,
  B00000000
};

static const uint8_t PROGMEM QUARTER_BATTERY[] = { 
  B00100000,
  B01110000,
  B01010000,
  B01010000,
  B01010000,
  B01110000,
  B01110000,
  B00000000
};

static const uint8_t PROGMEM HALF_BATTERY[] = { 
  B00100000,
  B01110000,
  B01010000,
  B01010000,
  B01110000,
  B01110000,
  B01110000,
  B00000000
};

static const uint8_t PROGMEM THREE_QUARTER_BATTERY[] = { 
  B00100000,
  B01110000,
  B01010000,
  B01110000,
  B01110000,
  B01110000,
  B01110000,
  B00000000
};

static const uint8_t PROGMEM FULL_BATTERY[] = { 
  B00100000,
  B01110000,
  B01110000,
  B01110000,
  B01110000,
  B01110000,
  B01110000,
  B00000000
};

static const uint8_t PROGMEM RADIANS_SYMBOL[] = { 
  B00001110,
  B00001010,
  B00001010,
  B00010000,
  B00100000,
  B01000000,
  B11111000,
  B00000000
};

static const uint8_t PROGMEM DEGREES_SYMBOL[] = { 
  B00001110,
  B00001010,
  B00001110,
  B00010000,
  B00100000,
  B01000000,
  B11111000,
  B00000000
};
char keys[ROWS][COLS] = {
  {ROLL_UP,ROLL_DOWN,CLEAR,BACK_SPACE, ADV_FUNC},
  {'&','|','~',     '/',  FLOATING},
  {'7','8','9',     '*',  HEXADECIMAL},
  {'4','5','6',     '-',  DECIMAL},
  {'1','2','3',     '+',  OCTAL},
  {'0','.',NEGATIVE,ENTER,BINARY}
};

const char functionNames[NUM_FUNCTIONS][FUNC_NAME_LENGTH+1] = {
  "pow",
  "qdr",
  "prs",
  "dtw",
  "sqr"
};

byte rowPins[ROWS] = {0, 1,  A0, 9, A1, 6}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {8, A2, A3, A4, A5}; //connect to the column pinouts of the keypad

// SCK is LCD serial clock (SCLK) - this is pin 13 on Arduino Uno
// MOSI is LCD DIN - this is pin 11 on an Arduino Uno
// pin 5 - Data/Command select (D/C)
// pin 4 - LCD chip select (CS)
// pin 3 - LCD reset (RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(5, 4, 3);
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS);

EEstack       RPNstack;
FPstack       FLTstack;
queue         buttonStore;
FPqueue       FLTstore;

mode          calcMode;
mode          prevMode;
error         errorState;

ComplexFloat  floatingTopOfStack;

char          keyPress;
byte          rowLevel;
unsigned long timeSinceError;
boolean       timerSet; 
angleMode     angle;

void setup() {
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  //Serial.begin(9600);
  floatingTopOfStack.top=true;
  //Serial.print("serial started\n");
  intStack(&RPNstack);
  intStack(&FLTstack);
  //Serial.print("initiated stack\n");
  intQueue(&buttonStore);
  intQueue(&FLTstore);
  //Serial.print("initiated queue\n");
  
  calcMode=integer;
  errorState=none;
  timerSet=false;
  rowLevel=0;
  angle=degrees;
  
  display.begin();
  display.setContrast(60);
  display.display();
  delay(2000);
  display.clearDisplay();
  
  
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0,0);
  
  //updateSerial(&buttonStore, &RPNstack);
  updateDisplay(&buttonStore, &RPNstack);

  //discard the first value from the keypad(garbage value)
  keypad.getKey();
  
}

void loop() {
  //get a key from the keypad
  keyPress = keypad.getKey();

  //if a key was pressed
  if(keyPress != '\0'  && keypad.keyStateChanged()){
    //first parse the data according to the appropriate mode
    switch(calcMode){
      case integer:{
        integerMode(&RPNstack, &buttonStore, keyPress);
        break;
      }
      case floating:{
        floatingMode(&FLTstack, &FLTstore, keyPress);
        break;
      }
      case func:{
        functionMode(&RPNstack, &buttonStore, keyPress);
        break;
      }
    }
    //switch mode again after, it may have been changed by the mode its in  
    switch(calcMode){
      case integer:{
        //updateSerial(&buttonStore, &RPNstack);
        updateDisplay(&buttonStore, &RPNstack);
        break;
      }
      case floating:{
        updateDisplay(&FLTstore, &FLTstack);
        break;
      }
      case func:{
        displayPrintCatalogue(&buttonStore);
        //serialPrintCatalogue(&buttonStore);
        break;
      }
    }
  }
  //check the error and reset it if it's timed out
  resetError(&RPNstack, &buttonStore);  
}

//Mode functions
void functionMode(EEstack* st, queue*qu, char data){
  switch(data){
    case ADV_FUNC:{
      calcMode=prevMode;
      break;
    }
    case CLEAR: {
      //if nothing has been entered in the button store, pop the most recent item off the stack
      if(!isEmpty(qu)) {
        clearQueue(qu);
      }
      //otherwise return to the previous mode
      else{
        calcMode = prevMode;
      }
      break;  
    }
    
    case BACK_SPACE:{
      //back space the queue
      back(qu);
      break;
    }
    case ROLL_UP:{
      //navigate through the menu
      if(rowLevel < NUM_FUNCTIONS/STACK_DISPLAY_HEIGHT){
        rowLevel++;
      }
      break;
    }
    case ROLL_DOWN:{
      //navigate through the menu
      if(rowLevel > 0){
        rowLevel--;
      }
    }
    case ENTER:{
      if(!isEmpty(qu)) {
        long qValue = getQueueValue(qu);
        //check if qvalue is in the range of functions
        if(qValue <= NUM_FUNCTIONS && qValue > 0){
          //call the funtion and if it returns false, then there was a stack erros
          if(!callFunction((byte)qValue, st)){
            errorState =  stack;
          }
          calcMode = prevMode;
          clearQueue(qu);
        }
        //if it's outside the range, set an error code
        else {
          errorState = doesNotExist;
          calcMode = prevMode;
        }
      }
    }
    default:{
      //convert it to an int and enqueue that value
      if(charIsNumber(data, qu)){
        enqueue(qu, charToInt(data));
        break; 
      }
    }
  }  
}

void integerMode(EEstack* st, queue* qu, char data){
  switch(data){
     case CLEAR: {
        //if nothing has been entered in the button store, pop the most recent item off the stack
        if(isEmpty(qu)&&!isEmpty(st)) {
          pop(st);  
        }  
        //otherwise clear the button store
        else{
          clearQueue(qu);
        }
        break;  
      }
      //if enter was pressed, push the contents of button store to the stack
      case ENTER: {
        if(!isEmpty(qu)){
          pushQueueToStack(qu, st);
        }
        else if(!isEmpty(st)){
          copy(st);
        }
        break;
      }
      case '+':{
        if(!add(st, qu)){
          errorState = stack;
        }
        break;
      }
      case '-':{
        if(!sub(st, qu)){
          errorState = stack;
        }
        break;
      }
      case '*':{
        if(!mul(st, qu)){
          errorState = stack;
        }
        break;
      }
      case '/':{
        if(!dvd(st, qu)){
          errorState = stack;
        }
        break;
      }
      case '&':{
        if(!logicAnd(st, qu)){
          errorState = stack;
        }
        break;
      }
      case '|':{
        if(!logicOr(st, qu)){
          errorState = stack;
        }
        break;
      }
      case '~':{
        if(!logicNot(st, qu)){
          errorState = stack;
        }
        break;
      }
      case NEGATIVE:{
        if(isEmpty(qu)&&!isEmpty(st)){
          long temp = Peek(st);
          pop(st);
          push(st, temp*-1);
        }
        else{
          qu->positive=!qu->positive;
        }
        break;
      }
      case'.':{
        //this button does nothing in integer mode
        break;  
      }
      case ROLL_UP:{
        if(!isEmpty(qu)){
          rollUp(st,qu); 
        }
        break;
      }
      case ROLL_DOWN:{
        if(!isEmpty(qu)){
          rollDown(st,qu); 
        }
        break;
      }
      case BINARY:{
        changeFormat(st, qu, binary);
        break;
      }
      case OCTAL:{
        changeFormat(st, qu, octal);
        break;
      }
      case DECIMAL:{
        st->displayFormat=decimal;
        qu->entryFormat=decimal; 
        break;
      }
      case HEXADECIMAL:{
        changeFormat(st, qu, hexadecimal);
        break;
      }
      case BACK_SPACE:{
        if(!back(qu)){
          pop(st); 
        }
        break;
      }
      case ADV_FUNC:{
        prevMode = calcMode;
        calcMode = func;
        rowLevel = 0;
        break;
      }
      case FLOATING:{
        prevMode = calcMode;
        calcMode = floating;
        break;
      }
      //otherwise one of the number buttons was pressed
      default:{
        //convert it to an int and enqueue that value
        if(charIsNumber(data, qu)){
          if((!isEmpty(qu))&&(getQueueValue(qu)==0)){
            if(data != '0'){
              clearQueue(qu);
              enqueue(qu, charToInt(data));
            }
          }
          else{
            enqueue(qu, charToInt(data));
          }
          break; 
        }
      }
    }
}

void floatingMode(FPstack* st, FPqueue* qu, char data){
  switch(data){
    case CLEAR:{
      if(isEmpty(qu)&&!isEmpty(st)) {
          pop(st);  
        }  
        //otherwise clear the button store
        else{
          clearQueue(qu);
        }
        break;
    }
    case ENTER: {
      if(!isEmpty(qu)){
        pushQueueToStack(qu, st);
      }
      else if(!isEmpty(st)){
        copy(st);
      }
      break;
    }
    case FLOATING:{
      calcMode=integer;
      break;
    }
    case BACK_SPACE:{
      if(!back(qu)){
        pop(st); 
      }
      break;
    }
    case NEGATIVE:{
      if(isEmpty(qu)&&!isEmpty(st)){
        ComplexFloat temp = Peek(st);
        pop(st);
        temp.real = temp.real*-1.0;
        push(st, temp);
      }
      else{
        qu->positive=!qu->positive;
      }
      break;
    }
    case '+':{
      if(!add(st, qu)){
        errorState = stack;
      }
      break;
    }
    case '-':{
      if(!sub(st, qu)){
        errorState = stack;
      }
      break;
    }
    case '*':{
      if(!mul(st, qu)){
        errorState = stack;
      }
      break;
    }
    case '/':{
      if(!dvd(st, qu)){
        errorState = stack;
      }
      break;
    }
    case PUSH_PI:{
      //create a Complex float with real value pi
      ComplexFloat tempToPush;
      tempToPush.real = PI;
      tempToPush.imaginary = 0.0;
      tempToPush.top = false;
      tempToPush.numDecimals = NUM_DIGITS_PI;
      //push it to the stack
      push(st, tempToPush);
      break;
    }
    case SIN:{
      if(!RPNsin(st, qu)){
        errorState = stack; 
      }
      break;
    }
    case COS:{
      if(!RPNcos(st, qu)){
        errorState = stack; 
      }
      break;
    }
    case TAN:{
      if(!RPNtan(st, qu)){
        errorState = stack; 
      }
      break;
    }
    case CHANGE_ANGLE:{
      if(angle == degrees){
        angle = radians;
      }
      else{
        angle = degrees;
      }
      break;
    }
    case IMAGINARY:{
      qu->real=!(qu->real);
      break;
    }
    case COMPLEX_MODE:{
      if(st->displayFormat == binomial){
        st->displayFormat = ECEbinomial;
      }
      else if(st->displayFormat == ECEbinomial){
        st->displayFormat = polar;
      }
      else{
        st->displayFormat = binomial;
      }
      break;
    }
    default: {
      if(charIsNumber(data)){
        enqueue(qu, charToInt(data));
      }
      else if(data == '.'&&qu->decimal == NO_DECIMAL){
        enqueue(qu, '.');
      }
      break; 
    }
  }
}
//STACK operations
//
//STACK-primitives
long Peek(EEstack* st){
  long data = TOP_OF_STACK;
  if(!isEmpty(st)){
    EEPROM.get(st->topOfStack - sizeof(long), data);
  }
  
  return data;
}

ComplexFloat Peek(FPstack* st){
  ComplexFloat data = floatingTopOfStack;
  if(!isEmpty(st)){
    EEPROM.get(st->topOfStack - sizeof(ComplexFloat), data);
  }
  
  return data;
}


boolean push(EEstack* st, long data){
  if(st->topOfStack == INT_STACK_MAX){
    return false;
  }
  else{
    EEPROM.put(st->topOfStack, data);
    st->topOfStack+=sizeof(long);
    //put the top of stack marker above the stack
    EEPROM.put(st->topOfStack, TOP_OF_STACK);
    return true;
  }
}

boolean push(FPstack* st, ComplexFloat data){
  if(st->topOfStack == EEPROM.length()){
    return false;
  }
  else{
    data.top=false;
    EEPROM.put(st->topOfStack, data);
    st->topOfStack+=sizeof(ComplexFloat);
    //put the top of stack marker above the stack
    EEPROM.put(st->topOfStack, floatingTopOfStack);
    return true;
  }
}

boolean pop(EEstack* st){
  if(st->topOfStack == 0){
    return false;
  }
  else{
    EEPROM.put(st->topOfStack, TOP_OF_STACK);
    st->topOfStack-=sizeof(long);
    return true;
  }
}

boolean pop(FPstack* st){
  if(st->topOfStack == INT_STACK_MAX+sizeof(ComplexFloat)){
    return false;
  }
  else{
    EEPROM.put(st->topOfStack, floatingTopOfStack);
    st->topOfStack-=sizeof(ComplexFloat);
    return true;
  }
}

boolean isEmpty(EEstack* st){
  if(st->topOfStack == 0){
    return true;
  }
  else{
    return false;
  }
}

boolean isEmpty(FPstack* st){
  if(st->topOfStack == INT_STACK_MAX+sizeof(ComplexFloat)){
    return true;
  }
  else{
    return false;
  }
}

void intStack(EEstack* st)
{
  int top = 0;
  long data;
  EEPROM.get(top, data);
  while(data != TOP_OF_STACK){
    top+=sizeof(long);
    if(top == INT_STACK_MAX){
      top = 0;
      break;
    }
    EEPROM.get(top, data);
  }
  st->displayFormat=decimal;
  st->topOfStack=top;  
}

void intStack(FPstack* st)
{
  int top = INT_STACK_MAX+sizeof(ComplexFloat);
  ComplexFloat data;
  EEPROM.get(top, data);
  while(data.top != true){
    top+=sizeof(ComplexFloat);
    if(top == EEPROM.length()){
      top = INT_STACK_MAX+sizeof(ComplexFloat);
      break;
    }
    EEPROM.get(top, data);
  }
  st->displayFormat=binomial;
  st->topOfStack=top;  
}
//STACK -Advanced

void rollUp(EEstack* st, queue* qu){
  long temp1,temp2;
  int pos=st->topOfStack-(getQueueValue(qu)*sizeof(long));
  if(pos>=sizeof(long)&&st->topOfStack>0)
  {
    EEPROM.get(pos, temp1);
    EEPROM.get(pos-sizeof(long), temp2);
    EEPROM.put(pos, temp2);
    EEPROM.put(pos-sizeof(long), temp1);
  }
}

void rollDown(EEstack* st, queue* qu){
  long temp1,temp2;
  int pos=st->topOfStack-(getQueueValue(qu)*sizeof(long));
  if(pos>=sizeof(long)&&st->topOfStack>0)
  {
    EEPROM.get(pos+sizeof(long), temp1);
    EEPROM.get(pos, temp2);
    EEPROM.put(pos+sizeof(long), temp2);
    EEPROM.put(pos, temp1);
  }
}

void copy(EEstack* st){
  long temp = Peek(st);
  push(st, temp);
}

void copy(FPstack* st){
  ComplexFloat temp = Peek(st);
  push(st, temp);
}

//STACK - Arithmetic
boolean add(EEstack* st, queue* qu){
  long num1, num2;
  if(isEmpty(qu)){
    num1 = Peek(st);
    //if you can't pop the stack, then the stack is empty, 
    //so you can complete the operation
    if(!pop(st)){
      return false;
    }
  }
  else{
    num1 = getQueueValue(qu);
    clearQueue(qu);
  }
  num2 = Peek(st);
  
  //if you can't pop the stack, then the stack is empty, 
  //so you can complete the operation
  if(!pop(st)){
    push(st, num1);
    return false;
  }

  if(addOverflow(num1, num2)){
    push(st, num2);
    push(st, num1);
    errorState = overFlow;
    return true;
  }
  
  push(st, num1+num2);
  return true;
}

boolean add(FPstack* st, FPqueue* qu){
  ComplexFloat num1, num2, result;
  float real, imaginary;
  if(isEmpty(qu)){
    num1 = Peek(st);
    //if you can't pop the stack, then the stack is empty, 
    //so you can complete the operation
    if(!pop(st)){
      return false;
    }
  }
  else{
    num1.real = getQueueValue(qu);
    num1.imaginary = 0.0;
    num1.numDecimals=getNumDecimals(qu);
    clearQueue(qu);
  }
  num2 = Peek(st);
  
  //if you can't pop the stack, then the stack is empty, 
  //so you can complete the operation
  if(!pop(st)){
    num1.top=false;
    push(st, num1);
    return false;
  }

  real=num1.real+num2.real;
  imaginary=num1.imaginary+num2.imaginary;
  result.real=real;
  result.imaginary=imaginary;
  result.top=false;

  if(num1.numDecimals >= num2.numDecimals){
    result.numDecimals=num1.numDecimals;
  }
  else{
    result.numDecimals=num2.numDecimals;
  }
  
  push(st, result);
  return true;
}

boolean sub(EEstack* st, queue* qu){
  long num1, num2;
  if(isEmpty(qu)){
    num1 = Peek(st);
    //if you can't pop the stack, then the stack is empty, 
    //so you can complete the operation
    if(!pop(st)){
      return false;
    }
  }
  else{
    num1 = getQueueValue(qu);
    clearQueue(qu);
  }
  num2 = Peek(st);
  //if you can't pop the stack, then the stack is empty, 
  //so you can complete the operation
  if(!pop(st)){
    push(st, num1);
    return false;
  }

  if(addOverflow(num1*-1, num2)){
    push(st, num2);
    push(st, num1);
    errorState = overFlow;
    return true;
  }
  push(st, num2-num1); 
  return true;
}

boolean sub(FPstack* st, FPqueue* qu){
  ComplexFloat num1, num2, result;
  float real, imaginary;
  if(isEmpty(qu)){
    num1 = Peek(st);
    //if you can't pop the stack, then the stack is empty, 
    //so you can complete the operation
    if(!pop(st)){
      return false;
    }
  }
  else{
    num1.real = getQueueValue(qu);
    num1.imaginary = 0.0;
    num1.numDecimals = getNumDecimals(qu);
    clearQueue(qu);
  }
  num2 = Peek(st);
  //if you can't pop the stack, then the stack is empty, 
  //so you can complete the operation
  if(!pop(st)){
    num1.top = false;
    push(st, num1);
    return false;
  }

  real = num2.real - num1.real;
  imaginary = num2.imaginary - num1.imaginary;
  result.real=real;
  result.imaginary=imaginary;
  result.top=false;

  if(num1.numDecimals >= num2.numDecimals){
    result.numDecimals=num1.numDecimals;
  }
  else{
    result.numDecimals=num2.numDecimals;
  }
  
  push(st, result); 
  return true;
}

boolean mul(EEstack* st, queue* qu){
  long num1, num2, result;
  if(isEmpty(qu)){
    num1 = Peek(st);
    //if you can't pop the stack, then the stack is empty, 
    //so you can complete the operation
    if(!pop(st)){
      return false;
    }
  }
  else{
    num1 = getQueueValue(qu);
    clearQueue(qu);
  }
  num2 = Peek(st);
  //if you can't pop the stack, then the stack is empty, 
  //so you can complete the operation
  if(!pop(st)){
    push(st, num1);
    return false;
  }
  result = num1*num2;
  if(mulOverflow(num1, num2, result)){
    push(st, num2);
    push(st, num1);
    errorState = overFlow;
    return true;
  }
  push(st, result); 
  return true;   
}

boolean mul(FPstack* st, FPqueue* qu){
  ComplexFloat num1, num2, result;
  
  
  if(isEmpty(qu)){
    num1 = Peek(st);
    //if you can't pop the stack, then the stack is empty, 
    //so you can complete the operation
    if(!pop(st)){
      return false;
    }
  }
  else{
    num1.real = getQueueValue(qu);
    num1.imaginary = 0.0;
    num1.numDecimals = getNumDecimals(qu);
    clearQueue(qu);
  }
  num2 = Peek(st);
  //if you can't pop the stack, then the stack is empty, 
  //so you can complete the operation
  if(!pop(st)){
    num1.top = false;
    push(st, num1);
    return false;
  }
  result.real = num1.real*num2.real;
  if(num1.numDecimals >= num2.numDecimals){
    result.numDecimals=num1.numDecimals;
  }
  else{
    result.numDecimals=num2.numDecimals;
  }
  
  push(st, result); 
  return true;   
}

boolean dvd(EEstack* st, queue* qu){
  long num1, num2;
  if(isEmpty(qu)){
    num1 = Peek(st);
    //if you can't pop the stack, then the stack is empty, 
    //so you can complete the operation
    if(!pop(st)){
      return false;
    }
  }
  else{
    num1 = getQueueValue(qu);
    clearQueue(qu);
  }
  num2 = Peek(st);
  //if you can't pop the stack, then the stack is empty, 
  //so you can complete the operation
  if(!pop(st)){
    push(st, num1);
    return false;
  }
  if(num1 == 0){
    errorState=divideByZero;
    push(st, num2);
    push(st, num1);
    
  }
  else{
    push(st, num2/num1);;  
  }
  return true;
}

boolean dvd(FPstack* st, FPqueue* qu){
  ComplexFloat num1, num2, result;
  if(isEmpty(qu)){
    num1 = Peek(st);
    //if you can't pop the stack, then the stack is empty, 
    //so you can complete the operation
    if(!pop(st)){
      return false;
    }
  }
  else{
    num1.real = getQueueValue(qu);
    num1.numDecimals = getNumDecimals(qu);
    num1.imaginary = 0;
    clearQueue(qu);
  }
  num2 = Peek(st);
  //if you can't pop the stack, then the stack is empty, 
  //so you can complete the operation
  if(!pop(st)){
    num1.top=false;
    push(st, num1);
    return false;
  }
  if(num1.real == 0.0){
    errorState=divideByZero;
    push(st, num2);
    push(st, num1);
    
  }
  else{
    result.real = num2.real/num1.real;
    result.imaginary = 0.0;
    result.top=false;
    if(num1.numDecimals >= num2.numDecimals){
      result.numDecimals=num1.numDecimals;
    }
    else{
      result.numDecimals=num2.numDecimals;
    }
    push(st, result); 
  }
  return true;
}

boolean logicAnd(EEstack* st, queue* qu){
  long num1, num2;
  if(isEmpty(qu)){
    num1 = Peek(st);
    //if you can't pop the stack, then the stack is empty, 
    //so you can complete the operation
    if(!pop(st)){
      return false;
    }
  }
  else{
    num1 = getQueueValue(qu);
    clearQueue(qu);
  }
  num2 = Peek(st);
  //if you can't pop the stack, then the stack is empty, 
  //so you can complete the operation
  if(!pop(st)){
    push(st, num1);
    return false;
  }
  push(st, num1&num2);
  return true;  
}

boolean logicOr(EEstack* st, queue* qu){
  long num1, num2;
  if(isEmpty(qu)){
    num1 = Peek(st);
    //if you can't pop the stack, then the stack is empty, 
    //so you can complete the operation
    if(!pop(st)){
      return false;
    }
  }
  else{
    num1 = getQueueValue(qu);
    clearQueue(qu);
  }
  num2 = Peek(st);
  //if you can't pop the stack, then the stack is empty, 
  //so you can complete the operation
  if(!pop(st)){
    push(st, num1);
    return false;
  }
  push(st, num1|num2);
  return true;  
}

boolean logicNot(EEstack* st, queue* qu){
  long num1;
  if(isEmpty(qu)){
    num1 = Peek(st);
    //if you can't pop the stack, then the stack is empty, 
    //so you can complete the operation
    if(!pop(st)){
      return false;
    }
  }
  else{
    num1 = getQueueValue(qu);
    clearQueue(qu);
  }

  push(st, ~num1);
  return true;  
}

boolean RPNsin(FPstack* st, FPqueue* qu){
  ComplexFloat num1, result;
  if(isEmpty(qu)){
    num1 = Peek(st);
    //if you can't pop the stack, then the stack is empty, 
    //so you can complete the operation
    if(!pop(st)){
      return false;
    }
  }
  else{
    num1.real = getQueueValue(qu);
    clearQueue(qu);
  }
  if(angle == degrees){
    result.real=sin(2.0*PI*num1.real/360.0);
  }
  else{
    result.real=sin(num1.real);
  }
  
  result.imaginary=0.0;
  result.numDecimals=num1.numDecimals;

  push(st, result);
  return true;  
}

boolean RPNcos(FPstack* st, FPqueue* qu){
  ComplexFloat num1, result;
  if(isEmpty(qu)){
    num1 = Peek(st);
    //if you can't pop the stack, then the stack is empty, 
    //so you can complete the operation
    if(!pop(st)){
      return false;
    }
  }
  else{
    num1.real = getQueueValue(qu);
    clearQueue(qu);
  }

  if(angle == degrees){
    result.real=cos(2.0*PI*num1.real/360.0);
  }
  else{
    result.real=cos(num1.real);
  }
  result.imaginary=0.0;
  result.numDecimals=num1.numDecimals;

  push(st, result);
  return true;  
}

boolean RPNtan(FPstack* st, FPqueue* qu){
  ComplexFloat num1, result;
  if(isEmpty(qu)){
    num1 = Peek(st);
    //if you can't pop the stack, then the stack is empty, 
    //so you can complete the operation
    if(!pop(st)){
      return false;
    }
  }
  else{
    num1.real = getQueueValue(qu);
    clearQueue(qu);
  }
  if(angle == degrees){
    result.real=tan(2.0*PI*num1.real/360.0);
  }
  else{
    result.real=tan(num1.real);
  }
  
  result.real=tan(num1.real);
  result.imaginary=0.0;
  result.numDecimals=num1.numDecimals;

  push(st, result);
  return true;  
}

boolean pwr(EEstack* st){
  long base, exponent;

  exponent = Peek(st);
  if(!pop(st)){
    return false;
  }
  base = Peek(st);
  if(!pop(st)){
    push(st, exponent);
    return false;
  }
  push(st, power(base, exponent));
  return true;
}

boolean qdr(EEstack* st){
  long a, b, c, root1, root2;

  c = Peek(st);
  if(!pop(st)){
    return false;
  }
  
  b = Peek(st);
  if(!pop(st)){
    push(st, c);
    return false;
  }
  
  a = Peek(st);
  if(!pop(st)){
    push(st, b);
    push(st, c);
    return false;
  }

  root1 = ((0-b)+sqrt((b*b)-(4*a*c)))/(2*a);
  root2 = ((0-b)-sqrt((b*b)-(4*a*c)))/(2*a);

  push(st, root1);
  push(st, root2);
  return true;
  
}

boolean rr(EEstack* st){
  long r1, r2;
  r1 = Peek(st);
  if(!pop(st)){
    return false;
  }

  r2 = Peek(st);
  if(!pop(st)){
    push(st, r1);
    return false;
  }
  push(st,((r1*r2)/(r1+r2)));
  return true;
}

boolean sqr(EEstack* st){
  long num;
  num = Peek(st);
  if(!pop(st)){
    return false;
  }
  push(st, num*num);
  return true;  
}
//STACK - customs
void serialPrintStack(EEstack* st){
  //position in the stack, start at the top
  int pos=0, top = st->topOfStack-sizeof(long);
  long temp;

  Serial.print('\n');
  Serial.print('\n');
  Serial.print('\n');
  if(top >= 0){
    //full stack shown for debugging purposes,but this code will be reused for the display
    /*if(top >= (STACK_DISPLAY_HEIGHT)*sizeof(int)){
      pos = top-(STACK_DISPLAY_HEIGHT-1)*sizeof(int);
    }*/
  
    while(pos <= top){
      EEPROM.get(pos, temp);                      //get a number from the stack
      Serial.print((((top-pos)/sizeof(long))+1));  //print the position in the stack 
      Serial.print(": ");
      if(st->displayFormat == hexadecimal){
        Serial.print(temp,HEX);                   //print the value in hex
      }
      else if(st->displayFormat == octal){
        Serial.print(temp,OCT);                   //print the value in oct
      }
      else if(st->displayFormat == binary){
        Serial.print(temp,BIN);                   //print the value in binary
      }
      else{
        Serial.print(temp);                       //print the value in decimal(default)
      }
      
      Serial.print('\n');
      //decrement position
      pos += sizeof(long);
    }
  }
}

void displayPrintStack(EEstack* st){
  //position in the stack, start at the top
  int pos=0, top = st->topOfStack-sizeof(long);
  long temp;
  
  if(top >= 0){
    //full stack shown for debugging purposes,but this code will be reused for the display
    if(top >= (STACK_DISPLAY_HEIGHT)*sizeof(long)){
      pos = top-(STACK_DISPLAY_HEIGHT-1)*sizeof(long);
    }
    if(top < (STACK_DISPLAY_HEIGHT)*sizeof(long)){
      int i = STACK_DISPLAY_HEIGHT-1-top/sizeof(long);
      for(i; i>0; i--){
        display.print('\n');
      }
    }
    while(pos <= top){
      EEPROM.get(pos, temp);                      //get a number from the stack
      display.print((((top-pos)/sizeof(long))+1));  //print the position in the stack 
      display.print(": ");
      if(st->displayFormat == hexadecimal){
        display.print(temp,HEX);                   //print the value in hex
      }
      else if(st->displayFormat == octal){
        display.print(temp,OCT);                   //print the value in oct
      }
      else if(st->displayFormat == binary){
        display.print(temp,BIN);                   //print the value in binary
      }
      else{
        display.print(temp);                       //print the value in decimal(default)
      }
      
      display.print('\n');
      //decrement position
      pos += sizeof(long);
    }
  }
  else {
    display.print("\n\n\n\n");
  }
}

void displayPrintStack(FPstack* st){
  //position in the stack, start at the top
  int pos=INT_STACK_MAX+sizeof(ComplexFloat), top = st->topOfStack-sizeof(ComplexFloat);
  ComplexFloat temp;
  
  if(top >= INT_STACK_MAX+sizeof(ComplexFloat)){
    if(top >= INT_STACK_MAX+sizeof(ComplexFloat)+(STACK_DISPLAY_HEIGHT)*sizeof(ComplexFloat)){
      pos = top-(STACK_DISPLAY_HEIGHT-1)*sizeof(ComplexFloat);
    }
    else{
      int i = STACK_DISPLAY_HEIGHT-(top-INT_STACK_MAX)/sizeof(ComplexFloat);
      for(i; i>0; i--){
        display.print('\n');
      }
    }
    while(pos <= top){
      EEPROM.get(pos, temp);                        //get a number from the stack
      display.print((((top-pos)/sizeof(ComplexFloat))+1));  //print the position in the stack 
      display.print(": ");
      if(st->displayFormat == binomial || st->displayFormat == ECEbinomial){
        if(onlyReal(temp)){
          display.print(temp.real, temp.numDecimals);   //print the value in decimal(default)
        }
        else if(onlyImaginary(temp)){
          display.print(temp.imaginary, temp.numDecimals);   //print the value in decimal(default)
          display.print(imaginaryUnit(st));
        }
        //temp has a real and imaginary component
        else{
          display.print(temp.real, temp.numDecimals);
          display.print("+");
          display.print(temp.imaginary, temp.numDecimals);
          display.print(imaginaryUnit(st));
        }
      }
      else{
        display.print(magnitude(temp), temp.numDecimals);
        display.print("<");
        display.print(argument(temp), 1);
      }
      
      display.print('\n');
      //decrement position
      pos += sizeof(ComplexFloat);
    }
  }
  else {
    display.print("\n\n\n\n");
  }
}
//QUEUE 

//QUEUE - Primitives 
void intQueue(queue* qu){
  qu->size=0;               //set the size initially to 0
  qu->front=0;              //set the front initially to 0
  qu->positive=true;        //intiially make it positive
  qu->entryFormat=decimal;  //intiially make it decimal
}

void intQueue(FPqueue* qu){
  qu->size=0;               //set the size initially to 0
  qu->front=0;              //set the front initially to 0
  qu->positive=true;        //intiially make it positive
  qu->real=true;
  qu->decimal=NO_DECIMAL;
}

boolean isFull(queue* qu) {
  if(qu->size== MAX_QUEUE_SIZE){
    return true;
  }
  else {
    return false;
  }
}

boolean isFull(FPqueue* qu) {
  if(qu->size== MAX_QUEUE_SIZE){
    return true;
  }
  else {
    return false;
  }
}


boolean isEmpty(queue* qu){
  if(qu->size == 0){
    return true;
  }
  else{
    return false;
  }
}


boolean isEmpty(FPqueue* qu){
  if(qu->size == 0){
    return true;
  }
  else{
    return false;
  }
}

boolean enqueue(queue* qu, byte value){
  if(isFull(qu)){
    return false;  
  }
  else{
     qu->data[(qu->front + qu->size) % MAX_QUEUE_SIZE] = value; 
     qu->size++;
     return true;
  }
}

boolean enqueue(FPqueue* qu, byte value){
  if(isFull(qu)){
    return false;  
  }
  else{
    if(value == '.'){
      qu->decimal=qu->size;  
    }
    qu->data[(qu->front + qu->size) % MAX_QUEUE_SIZE] = value; 
    qu->size++;
    return true;
  }
}

boolean dequeue(queue* qu){
  if(isEmpty(qu)){
    return false;
  }
  else{
    qu->data[qu->front] = -1;
    qu->front = (qu->front+1) % MAX_QUEUE_SIZE;
    qu->size--;
    return true;
  }
}

boolean dequeue(FPqueue* qu){
  if(isEmpty(qu)){
    return false;
  }
  else{
    if(qu->data[qu->front] == '.'){
      qu->decimal=NO_DECIMAL;
    }
    qu->data[qu->front] = -1;
    qu->front = (qu->front+1) % MAX_QUEUE_SIZE;
    qu->size--;
    return true;
  }
}

byte Peek(queue* qu){
  return qu->data[qu->front];
}

byte Peek(FPqueue* qu){
  return qu->data[qu->front];
}


//QUEUE - Advanced
//remove an item from the back of the queue
boolean back(queue* qu){
  if(isEmpty(qu)){
    return false;
  }
  else{
    qu->data[qu->front+ qu->size] = -1;   //remove the item from the back
    qu->size--;                           //decrement teh size
    return true;
  }
}

boolean back(FPqueue* qu){
  if(isEmpty(qu)){
    return false;
  }
  else{
    if(qu->data[qu->front+ qu->size] == '.'){
      qu->decimal=NO_DECIMAL;
    }
    qu->data[qu->front+ qu->size] = -1;   //remove the item from the back
    qu->size--;                           //decrement teh size
    return true;
  }
}

void clearQueue(queue* qu){
  //while qu still contains items, remove the item at the front
  while(!isEmpty(qu)){
    dequeue(qu);
  }
  qu->positive=true;
}

void clearQueue(FPqueue* qu){
  //while qu still contains items, remove the item at the front
  while(!isEmpty(qu)){
    dequeue(qu);
  }
  qu->positive=true;
  qu->real=true;
  qu->decimal=NO_DECIMAL;
}

//QUEUE - Customs

void serialPrintQueue(queue* qu){ 
  long temp = getQueueValue(qu);
  //if there's no error just print the value in the queue
  if(errorState == none){
    if(qu->entryFormat == hexadecimal){
      Serial.print(temp,HEX);           //print in hex
      for(byte i = 0; i < (SCREEN_WIDTH-4-(qu->size)); i++){
        Serial.print(" ");
      }
      Serial.print("HEX");
    }
    else if(qu->entryFormat == octal){
      Serial.print(temp,OCT);           //print in oct
      for(byte i = 0; i < (SCREEN_WIDTH-4-(qu->size)); i++){
        Serial.print(" ");
      }
      Serial.print("OCT");
    }
    else if(qu->entryFormat == binary){
      Serial.print(temp,BIN);           //print in binary
      for(byte i = 0; i < (SCREEN_WIDTH-4-(qu->size)); i++){
        Serial.print(" ");
      }
      Serial.print("BIN");
    }
    else{
      Serial.print(temp);               //by default pint in decimal
    }
  }

  ///otherwise print the error in place of the queue
  else if(errorState == divideByZero){
    Serial.print("Error: Divide0");
  }
  else if(errorState == overFlow){
    Serial.print("Error: OverF");
  }
  else if(errorState == stack){
    Serial.print("Error: Stack");
  }
  else if(errorState == doesNotExist){
    Serial.print("Error: Func");  
  }
}




void displayPrintQueue(queue* qu){ 
  long temp = getQueueValue(qu);
  //if there's no error just print the value in the queue
  if(errorState == none){
    if(!isEmpty(qu)){
      if(qu->entryFormat == hexadecimal){
        display.print(temp,HEX);           //print in hex
        for(byte i = 0; i < (SCREEN_WIDTH-4-(qu->size)); i++){
          display.print(" ");
        }
        display.print("H");
      }
      else if(qu->entryFormat == octal){
        display.print(temp,OCT);           //print in oct
        for(byte i = 0; i < (SCREEN_WIDTH-4-(qu->size)); i++){
          display.print(" ");
        }
        display.print("O");
      }
      else if(qu->entryFormat == binary){
        display.print(temp,BIN);           //print in binary
        for(byte i = 0; i < (SCREEN_WIDTH-4-(qu->size)); i++){
          display.print(" ");
        }
        display.print("B");
      }
      else{
        display.print(temp);               //by default pint in decimal
      }
    }
  }

  ///otherwise print the error in place of the queue
  else if(errorState == divideByZero){
    display.print("Error: Divide0");
  }
  else if(errorState == overFlow){
    display.print("Error: OverF");
  }
  else if(errorState == stack){
    display.print("Error: Stack");
  }
  else if(errorState == doesNotExist){
    display.print("Error: Func");  
  }
  
  display.drawBitmap(79,40, getBattery(), BATTERY_WIDTH, BATTERY_HEIGHT, BLACK);
}


void displayPrintQueue(FPqueue* qu){ 
  float temp = getQueueValue(qu);
  //if there's no error just print the value in the queue
  if(errorState == none){
    if(!isEmpty(qu)){
      display.print(temp,getNumDecimals(qu));               //by default print in decimal
      if(!qu->real){
        display.print(imaginaryUnit(&FLTstack));
      }
    }
  }
  ///otherwise print the error in place of the queue
  else if(errorState == divideByZero){
    display.print("Error: Divide0");
  }
  else if(errorState == overFlow){
    display.print("Error: OverF");
  }
  else if(errorState == stack){
    display.print("Error: Stack");
  }
  else if(errorState == doesNotExist){
    display.print("Error: Func");  
  }
  display.drawBitmap(71,40, getAngleMode(), ANGLE_SYMBOL_WIDTH, ANGLE_SYMBOL_HEIGHT, BLACK);
  display.drawBitmap(79,40, getBattery(), BATTERY_WIDTH, BATTERY_HEIGHT, BLACK);
}

byte getNumDecimals(FPqueue* qu){
  if(qu->decimal != NO_DECIMAL){
    return qu->size-qu->decimal-1;
  }
  return 0;
}

long getQueueValue(queue* qu){
  long temp = 0;
  byte pos = qu->front;
  byte qSize = qu->size;
  //while we haven't reached the end of the q 
  while(qSize != 0){
    temp += (long)(qu->data[pos])*power(getBase(qu), qSize-1);
    //move the position by one
    pos = (pos+1)% MAX_QUEUE_SIZE;
    //decrement the size
    qSize--; 
  }
  if(!qu->positive){
    temp=temp*-1;
  }
  return temp;
}

float getQueueValue(FPqueue* qu){
  float temp = 0;
  byte pos = qu->front;
  char qSize = qu->size;
  char qEnd = 0;
  //while we haven't reached the end of the q
  if(qu->decimal != NO_DECIMAL){
    qEnd=(char)qu->decimal-(char)qSize+1;
    qSize+=qEnd-1;
  }
  while(qSize != qEnd){
    if(qu->data[pos] != '.'){
      temp += (float)(qu->data[pos])*pow(10.0, (float)qSize-1);
      qSize--; 
    }
    //move the position by one
    pos = (pos+1)% MAX_QUEUE_SIZE;
    //decrement the size
    
  }
  if(!qu->positive){
    temp=temp*-1;
  }
  return temp;
}

boolean setQueueValue(queue* qu, long value){
  byte i, digit, base = getBase(qu);
  long temp = value/base;

  while(temp != 0){
    temp = temp/base;
    i++;
  }

  clearQueue(qu);
  temp = value;

  while(i > 0){
    i--;
    digit = temp/power(base, i);
    temp = temp - digit*power(base, i);
    enqueue(qu, digit);
  }
}


void pushQueueToStack(queue* qu, EEstack* st){
  long temp = 0;
  //while qu still contains items
  while(!isEmpty(qu)){
    //multiply the digit by 10 to the power of its position
    //add this to the running total
    temp += (long)Peek(qu)*power(getBase(qu), ((qu->size)-1));
    //clear the queue as you go through it
    dequeue(qu);
  }

  //if the number is negative, multiply by -1 before pushing
  if(!qu->positive){
    temp=temp*-1;
  }
  push(st, temp);
  qu->positive=true;
}

void pushQueueToStack(FPqueue* qu, FPstack* st){
  float temp = 0;
  ComplexFloat CF;
  //while qu still contains items
  temp = getQueueValue(qu);

  if(qu->real){
    CF.real=temp;
    CF.imaginary=0.0;
  }
  else{
    CF.real=0.0;
    CF.imaginary=temp;
  }
  
  CF.numDecimals=getNumDecimals(qu);
  
  push(st, CF);
  clearQueue(qu);
  qu->positive=true;
}

//OVERFLOW DETECTION
boolean addOverflow(long x, long y){
  if ((y > 0 && x > INT_MAX - y) || (y < 0 && x < INT_MIN - y)){
    return true;
  }
  else{
    return false;
  }
}

boolean mulOverflow(long x, long y, long result){
  if((result/x) == y && (result/y) == x){
    return false;
  }
  else{
    return true;
  }
}

//OTHER FUNCTIONS
void updateSerial(queue* qu, EEstack* st){
    //print the updated stack 
    serialPrintStack(st);
    serialPrintLine();
    serialPrintQueue(qu);
}

void updateDisplay(queue* qu, EEstack* st){
    display.clearDisplay();
    display.setCursor(0,0);
    displayPrintStack(st);
    displayPrintLine();
    displayPrintQueue(qu);
    display.display();
}

void updateDisplay(FPqueue* qu, FPstack* st){
    display.clearDisplay();
    display.setCursor(0,0);
    displayPrintStack(st);
    displayPrintLine();
    displayPrintQueue(qu);
    display.display();
}
//prints a line in serial monitor
void serialPrintLine(){
  byte i;
  for(i=0; i<SCREEN_WIDTH; i++){
    Serial.print("-");
  }
  Serial.print("\n");
}

void displayPrintLine(){
  byte i;
  for(i=0; i<SCREEN_WIDTH; i++){
    display.print("-");
  }
  display.print("\n");
}

//returns the base to the power of the exponent
//only works for integers
long power(long base, long exponent){
  long power = 1;
  
  //loop until the exponent is 0
  while(exponent > 0){
    //multiply the power by the base
    power=power*base;
    exponent--;
  }
  return power;
}

//converts the char to the corrisponding integer number(between 0 and 9)
byte charToInt(char character){
  return (byte)(character-48); 
}

//returns the base of the entry format
byte getBase(queue* qu){
  byte base;
  switch(qu->entryFormat){
    case binary:{
      base = 2;
      break;
    }
    case octal:{
      base = 8;
      break;
    }
    case decimal:{
      base = 10;
      break;
    }
    case hexadecimal:{
      base = 16;
      break;
    }
  }
  return base;
}

//returns true if the char is a number in the current entry format
boolean charIsNumber(char data, queue* qu){
  boolean isNum;
  //switch the entry format
  switch(qu->entryFormat){
    case binary:{
      isNum = (data >= '0' && data <='1');    //if between 0 and 1 for binary
      break;
    }
    case octal:{
      isNum = (data >= '0' && data <='7');    //if between 0 and 7 for octal
      break;
    }
    case decimal:{
      isNum = (data >= '0' && data <='9');    //if between 0 and 9 for decimal
      break;
    }
    case hexadecimal:{
      isNum = (data >= '0' && data <='9');    //if between 0 and 9 for hexadecimal
      break;
    }
  }
  return isNum;
}
boolean charIsNumber(char data){
  boolean isNum;
  //switch the entry format
  isNum = (data >= '0' && data <='9');    //if between 0 and 9 for hexadecimal
  return isNum;
}


void serialPrintCatalogue(queue* qu){
  byte i, funcNum;
  
  Serial.print('\n');
  Serial.print('\n');
  Serial.print('\n');
  //print the names and numbers of all the functions
  for(i=STACK_DISPLAY_HEIGHT; i>0; i--){
    funcNum = i+(STACK_DISPLAY_HEIGHT*rowLevel);
    //print the function number
    if(funcNum <= NUM_FUNCTIONS){
      Serial.print(funcNum); 
      Serial.print(": ");
      //print the function name
      Serial.print(functionNames[funcNum-1]);
    }
    Serial.print('\n');
  }

  //print the line followed by the value in the queue
  serialPrintLine();
  serialPrintQueue(qu);
}

void displayPrintCatalogue(queue* qu){
  byte i, funcNum;
  
  display.clearDisplay();
  //print the names and numbers of all the functions
  for(i=STACK_DISPLAY_HEIGHT; i>0; i--){
    funcNum = i+(STACK_DISPLAY_HEIGHT*rowLevel);
    //print the function number
    if(funcNum <= NUM_FUNCTIONS){
      display.print(funcNum); 
      display.print(": ");
      //print the function name
      display.print(functionNames[funcNum-1]);
    }
    display.print('\n');
  }

  //print the line followed by the value in the queue
  displayPrintLine();
  displayPrintQueue(qu);
  display.display();
}

void resetError(EEstack* st, queue* qu){
  unsigned long time = millis();    //save the current time
  if(errorState != none){           //if there is an error
    if(!timerSet){                  //if the timer hasn't been set
      timeSinceError = time;        //save the current time to a global variable
      timerSet = true;              //set the timer flag, so we no the timer has been checked
    }
    //if the time since the error ocured is greater than or equal to the timeout
    else if(time-timeSinceError >= ERROR_TIMEOUT){
      errorState = none;            //reset the error
      timerSet = false;             //reset the timer flag
      updateDisplay(qu, st);         //update the screen
    }
  }
}

//state0 - both entry and displays in decimal
//state1 - display format is the specified format
//state2 - entry format is the specified format
//state3 - both entry and displays in the specified format
void changeFormat(EEstack* st, queue* qu, numberBase format){
  //go from state back to state0
  if(st->displayFormat==format&&qu->entryFormat==format){
    st->displayFormat=decimal;
    qu->entryFormat=decimal; 
    digitalWrite(LED_BUILTIN, LOW);
  }
  //go from state0 to state1
  else if(st->displayFormat==decimal&&qu->entryFormat==decimal){
    st->displayFormat=format;
    qu->entryFormat=decimal;
    digitalWrite(LED_BUILTIN, HIGH); 
  }
  //go from state1 to state2
  else if(st->displayFormat==format&&qu->entryFormat==decimal){
    st->displayFormat=decimal;
    qu->entryFormat=format; 
    digitalWrite(LED_BUILTIN, LOW);
  }
  //go from state2 to state3
  else if(st->displayFormat==decimal&&qu->entryFormat==format){
    st->displayFormat=format;
    qu->entryFormat=format; 
    digitalWrite(LED_BUILTIN, HIGH); 
  }
  //if in some other unrecognized state go to state1
  //ie it moved from a different format
  else {
    st->displayFormat=format;
    qu->entryFormat=decimal; 
    digitalWrite(LED_BUILTIN, HIGH); 
  }
}


boolean callFunction(byte funcNum, EEstack * st){
  boolean success = true;
  switch(funcNum){
    case 1:{
      success = pwr(st);
      break;
    }
    case 2:{
      success = qdr(st);
      break;
    }
    case 3:{
      success = rr(st);
      break;
    }
    case 5:{
      success = sqr(st);
      break;
    }
    default:{
      //errorState = doesNotExist;
      break; 
    }
  }
  return success;
}
const uint8_t* getBattery(){
  if(analogRead(A7) < 547){
    return EMPTY_BATTERY;
  }
  else if(analogRead(A7) < 582){
    return QUARTER_BATTERY;
  }
  else if(analogRead(A7) < 618){
    return QUARTER_BATTERY;
  }
  else{
    return FULL_BATTERY;
  }
}

const uint8_t* getAngleMode(){
  //switch the angle mode and return a pointer to  the symbol
  switch(angle){
    case radians:{
      return RADIANS_SYMBOL;
      break;
    }
    case degrees:{
      return DEGREES_SYMBOL;
      break;
    }
    default:{
      return NULL;
      break;
    }
  }
}

//complex number operations

boolean onlyReal(ComplexFloat num){
  if(num.imaginary==0.0){
    return true;
  }
  else{
    return false;
  }
}

boolean onlyImaginary(ComplexFloat num){
  if(num.real == 0.0){
    return true;
  }
  else{
    return false;
  }
}

float magnitude(ComplexFloat num){
  return sqrt(num.real*num.real+num.imaginary*num.imaginary);
}

float argument(ComplexFloat num){
  if(angle == radians){
    return atan(num.imaginary/num.real);
  }
  else{
    return (360/(2*PI))*atan(num.imaginary/num.real);
  }
}

char imaginaryUnit(FPstack* st){
  if(st->displayFormat == ECEbinomial){
    return 'j';
  }
  else{
    return 'i';
  }
}
