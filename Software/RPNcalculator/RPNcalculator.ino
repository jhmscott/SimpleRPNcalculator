#include "EEPROM.h"
#include "Keypad.h"

#define TOP_OF_STACK          32767

#define LINE_LENGTH           10
#define MAX_QUEUE_SIZE        10
#define STACK_DISPLAY_HEIGHT  4
#define SCREEN_WIDTH          3
#define NUM_FUNCTIONS         4
#define ROWS                  6 //four rows
#define COLS                  5 //four columns
#define ERROR_TIMEOUT         1000

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
  overFlow
};

struct EEstack{
  int         topOfStack;
  numberBase  displayFormat;
};

struct queue{
	int         data[MAX_QUEUE_SIZE];
	int         size;
  int         front;
  boolean     positive;
  numberBase  entryFormat;
};

char keys[ROWS][COLS] = {
  {ROLL_UP,ROLL_DOWN,CLEAR,BACK_SPACE, ADV_FUNC},
  {'&','|','~',     '+',  FLOATING},
  {'1','2','3',     '-',  HEXADECIMAL},
  {'4','5','6',     '*',  DECIMAL},
  {'7','8','9',     '/',  OCTAL},
  {'0','.',NEGATIVE,ENTER,BINARY}
};

const char functionNames[NUM_FUNCTIONS][SCREEN_WIDTH+1] = {
  "pow",
  "qdr",
  "prs",
  "dtw"
};

byte rowPins[ROWS] = {5, 4, 3, 2}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {8, 7, 6}; //connect to the column pinouts of the keypad


Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS);

EEstack       RPNstack;
queue         buttonStore;

mode          calcMode;
mode          prevMode;
error         errorState;

char          keyPress;
byte          rowLevel;
unsigned long timeSinceError;
boolean       timerSet;

void setup() {
  intStack(&RPNstack);
  intQueue(&buttonStore);
  calcMode=integer;
  errorState=none;
  timerSet=false;
  Serial.begin(9600);
  updateSerial(&buttonStore, &RPNstack);
}

void loop() {
  //get a key from the keypad
  //keyPress = keypad.getKey();

  //if a key was pressed
  if(Serial.available() > 0){
    keyPress = Serial.read();
    //first parse the data according to the appropriate mode
    switch(calcMode){
      case integer:{
        integerMode(&RPNstack, &buttonStore, keyPress);
        break;
      }
      case floating:{
        break;
      }
      case func:{
        functionMode(&buttonStore, keyPress);
        break;
      }
    }
    //switch mode again after, it may have been changed by the mode its in  
    switch(calcMode){
      case integer:{
        updateSerial(&buttonStore, &RPNstack);
        break;
      }
      case floating:{
        break;
      }
      case func:{
        serialPrintCatalogue(&buttonStore);
        break;
      }
    }
  }
  resetError(&RPNstack, &buttonStore);  
}

//Mode functions
void functionMode(queue*qu, char data){
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
      break;  
    }
    case BACK_SPACE:{
      back(qu);
      break;
    }
    default:{
      //convert it to an int and enqueue that value
      if(data >= '0' && data <='9'){
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
        pushQueueToStack(qu, st);
        break;
      }
      case '+':{
        add(st, qu);
        break;
      }
      case '-':{
        sub(st, qu);
        break;
      }
      case '*':{
        mul(st, qu);
        break;
      }
      case '/':{
        dvd(st, qu);
        break;
      }
      case '&':{
        logicAnd(st, qu);
        break;
      }
      case '|':{
        logicOr(st, qu);
        break;
      }
      case '~':{
        logicNot(st, qu);
        break;
      }
      case NEGATIVE:{
        if(isEmpty(qu)&&!isEmpty(st)){
          int temp = Peek(st);
          pop(st);
          push(st, temp*-1);
        }
        else{
          buttonStore.positive=!buttonStore.positive;
        }
        break;
      }
      case'.':{
        //this button does nothing in integer mode
        break;  
      }
      case ROLL_UP:{
        rollUp(st,qu);
        break;
      }
      case ROLL_DOWN:{
        rollDown(st,qu);
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
        back(qu);
        break;
      }
      case ADV_FUNC:{
        prevMode = calcMode;
        calcMode = func;
        rowLevel = 0;
        break;
      }
      //otherwise one of the number buttons was pressed
      default:{
        //convert it to an int and enqueue that value
        if(charIsNumber(data, qu)){
          enqueue(qu, charToInt(data));
          break; 
        }
      }
    }
}

//STACK operations
//
//STACK-primitives
int Peek(EEstack* st){
  int data = TOP_OF_STACK;
  if(!isEmpty(st)){
    EEPROM.get(st->topOfStack - sizeof(int), data);
  }
  
  return data;
}

boolean push(EEstack* st, int data){
  if(st->topOfStack == EEPROM.length()){
    return false;
  }
  else{
    EEPROM.put(st->topOfStack, data);
    st->topOfStack+=sizeof(int);
    //put the top of stack marker above the stack
    EEPROM.put(st->topOfStack, TOP_OF_STACK);
    return true;
  }
}

boolean pop(EEstack* st){
  if(st->topOfStack == 0){
    return false;
  }
  else{
    EEPROM.put(st->topOfStack, TOP_OF_STACK);
    st->topOfStack-=sizeof(int);
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

void intStack(EEstack* st)
{
  int top = 0, data;
  EEPROM.get(top, data);
  while(data != TOP_OF_STACK){
    top+=sizeof(int);
    EEPROM.get(top, data);
  }
  st->displayFormat=decimal;
  st->topOfStack=top;  
}


//STACK -Advanced

void rollUp(EEstack* st, queue* qu){
  int temp1,temp2;
  int pos=st->topOfStack-(getQueueValue(qu)*sizeof(int));
  if(pos>=sizeof(int)&&st->topOfStack>0)
  {
    EEPROM.get(pos, temp1);
    EEPROM.get(pos-sizeof(int), temp2);
    EEPROM.put(pos, temp2);
    EEPROM.put(pos-sizeof(int), temp1);
  }
}

void rollDown(EEstack* st, queue* qu){
  int temp1,temp2;
  int pos=st->topOfStack-(getQueueValue(qu)*sizeof(int));
  if(pos>=sizeof(int)&&st->topOfStack>0)
  {
    EEPROM.get(pos+sizeof(int), temp1);
    EEPROM.get(pos, temp2);
    EEPROM.put(pos+sizeof(int), temp2);
    EEPROM.put(pos, temp1);
  }
}

//STACK - Arithmetic
boolean add(EEstack* st, queue* qu){
  int num1, num2, result;
  byte ovf;
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
  result=num1+num2;
  
  push(st, result);
  return true;
}

void sub(EEstack* st, queue* qu){
  int num1, num2;
  if(isEmpty(qu)){
    num1 = Peek(st);
    pop(st);
  }
  else{
    num1 = getQueueValue(qu);
    clearQueue(qu);
  }
  num2 = Peek(st);
  pop(st);
  push(st, num2-num1); 
}

void mul(EEstack* st, queue* qu){
  int num1, num2, result;
  if(isEmpty(qu)){
    num1 = Peek(st);
    pop(st);
  }
  else{
    num1 = getQueueValue(qu);
    clearQueue(qu);
  }
  num2 = Peek(st);
  pop(st);
  result = num1*num2;
  push(st, result);    
}

void dvd(EEstack* st, queue* qu){
  int num1, num2;
  if(isEmpty(qu)){
    num1 = Peek(st);
    pop(st);
  }
  else{
    num1 = getQueueValue(qu);
    clearQueue(qu);
  }
  num2 = Peek(st);
  pop(st);
  if(num1 == 0){
    errorState=divideByZero;
    push(st, num2);
    push(st, num1);
  }
  else{
    push(st, num2/num1);;  
  }
}

void logicAnd(EEstack* st, queue* qu){
  int num1, num2;
  if(isEmpty(qu)){
    num1 = Peek(st);
    pop(st);
  }
  else{
    num1 = getQueueValue(qu);
    clearQueue(qu);
  }
  num2 = Peek(st);
  pop(st);
  push(st, num1&num2);;  
}

void logicOr(EEstack* st, queue* qu){
  int num1, num2;
  if(isEmpty(qu)){
    num1 = Peek(st);
    pop(st);
  }
  else{
    num1 = getQueueValue(qu);
    clearQueue(qu);
  }
  num2 = Peek(st);
  pop(st);
  push(st, num1|num2);;  
}

void logicNot(EEstack* st, queue* qu){
  int num1;
  if(isEmpty(qu)){
    num1 = Peek(st);
    pop(st);
  }
  else{
    num1 = getQueueValue(qu);
    clearQueue(qu);
  }

  push(st, ~num1);;  
}
//STACK - customs
void serialPrintStack(EEstack* st){
  //position in the stack, start at the top
  int pos=0, top = st->topOfStack-sizeof(int);
  int temp;

  Serial.print('\n');
  Serial.print('\n');
  Serial.print('\n');
  if(top >= 0){
    //full stack shown for debugging purposes,but this code will be reused for the display
    /*if(top >= (STACK_DISPLAY_HEIGHT)*sizeof(int)){
      pos = top-(STACK_DISPLAY_HEIGHT-1)*sizeof(int);
    }*/
  
    while(pos <= top){
      //get a number from the stack
      EEPROM.get(pos, temp);
      //print the position in the stack followed by its number
      Serial.print((((top-pos)/sizeof(int))+1));
      Serial.print(": ");
      if(st->displayFormat == hexadecimal){
        Serial.print(temp,HEX);
      }
      else if(st->displayFormat == octal){
        Serial.print(temp,OCT);
      }
      else if(st->displayFormat == binary){
        Serial.print(temp,BIN);
      }
      else{
        Serial.print(temp); 
      }
      
      Serial.print('\n');
      //decrement position
      pos += sizeof(int);
    }
  }
}

//QUEUE 

//QUEUE - Primitives 
void intQueue(queue* qu){
  qu->size=0;
  qu->front=0;
  qu->positive=true;
  qu->entryFormat=decimal;
}
boolean isFull(queue* qu) {
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

boolean enqueue(queue* qu, int value){
  if(isFull(qu)){
    return false;  
  }
  else{
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

int Peek(queue* qu){
  return qu->data[qu->front];
}

//QUEUE - Advanced
int back(queue* qu){
  if(isEmpty(qu)){
    return false;
  }
  else{
    qu->data[qu->front+ qu->size] = -1;
    qu->size--;
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

//QUEUE - Customs

void serialPrintQueue(queue* qu){ 
  int temp = getQueueValue(qu);
  if(errorState == none){
    if(qu->entryFormat == hexadecimal){
      Serial.print(temp,HEX);
    }
    else if(qu->entryFormat == octal){
      Serial.print(temp,OCT);
    }
    else if(qu->entryFormat == binary){
      Serial.print(temp,BIN);
    }
    else{
      Serial.print(temp); 
    }
  }
  else if(errorState == divideByZero){
    Serial.print("Error: divideByZero");
  }
  else if(errorState == overFlow){
    Serial.print("Error: divideByZero");
  }
}

int getQueueValue(queue* qu){
  int temp = 0;
  int pos = qu->front;
  int qSize = qu->size;
  //while we haven't reached the end of the q
  while(qSize != 0){
    temp += qu->data[pos]*power(getBase(qu), qSize-1);
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


void pushQueueToStack(queue* qu, EEstack* st){
  int temp = 0;
  //while qu still contains items
  while(!isEmpty(qu)){
    //multiply the digit by 10 to the power of its position
    //add this to the running total
    temp += Peek(qu)*power(getBase(qu), ((qu->size)-1));
    //clear the queue as you go through it
    dequeue(qu);
  }
  
  if(!qu->positive){
    temp=temp*-1;
  }
  push(st, temp);
  qu->positive=true;
}

//Other Functions
void updateSerial(queue* qu, EEstack* st){
    //print the updated stack 
    serialPrintStack(st);
    serialPrintLine();
    serialPrintQueue(qu);
}
void serialPrintLine(){
  int i;
  for(i=0; i<LINE_LENGTH; i++){
    Serial.print("-");
  }
  Serial.print("\n");
}


int power(int base, int exponent){
  int power = 1;
  while(exponent > 0)
  {
    power=power*base;
    exponent--;
  }
  return power;
}


int charToInt(char character){
  return (int)(character-48); 
}

int getBase(queue* qu){
  int base;
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

boolean charIsNumber(char data, queue* qu){
  boolean isNum;
  switch(qu->entryFormat){
    case binary:{
      isNum = (data >= '0' && data <='1');
      break;
    }
    case octal:{
      isNum = (data >= '0' && data <='7');
      break;
    }
    case decimal:{
      isNum = (data >= '0' && data <='9');
      break;
    }
    case hexadecimal:{
      isNum = (data >= '0' && data <='9');
      break;
    }
  }
  return isNum;
}

void serialPrintCatalogue(queue* qu){
  int i;
  
  Serial.print('\n');
  Serial.print('\n');
  Serial.print('\n');
  
  for(i=STACK_DISPLAY_HEIGHT; i>0; i--){
    Serial.print(i+(NUM_FUNCTIONS*rowLevel));
    Serial.print(": ");
    Serial.print(functionNames[i+(NUM_FUNCTIONS*rowLevel)-1]);
    Serial.print('\n');
  }

  serialPrintLine();
  serialPrintQueue(qu);
}

void resetError(EEstack* st, queue* qu){
  unsigned long time = millis();
  if(errorState != none){
    if(!timerSet){
      timeSinceError = time;
      timerSet = true;  
    }
    else if(time-timeSinceError >= ERROR_TIMEOUT){
      errorState = none;
      timerSet = false;
      updateSerial(qu, st);
    }
  }
}

void changeFormat(EEstack* st, queue* qu, numberBase format){
  if(st->displayFormat==format&&qu->entryFormat==format){
    st->displayFormat=decimal;
    qu->entryFormat=decimal; 
  }
  else if(st->displayFormat==decimal&&qu->entryFormat==decimal){
    st->displayFormat=format;
    qu->entryFormat=decimal; 
  }
  else if(st->displayFormat==format&&qu->entryFormat==decimal){
    st->displayFormat=decimal;
    qu->entryFormat=format; 
  }
  else if(st->displayFormat==decimal&&qu->entryFormat==format){
    st->displayFormat=format;
    qu->entryFormat=format; 
  }
  else {
    st->displayFormat=format;
    qu->entryFormat=decimal; 
  }
}
