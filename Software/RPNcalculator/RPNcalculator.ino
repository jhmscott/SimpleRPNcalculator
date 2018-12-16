#include "EEPROM.h"
#include "Keypad.h"

#define TOP_OF_STACK          2147483647

#define INT_MAX               2147483646
#define INT_MIN               -2147483648

#define MAX_QUEUE_SIZE        9
#define STACK_DISPLAY_HEIGHT  4
#define SCREEN_WIDTH          14
#define FUNC_NAME_LENGTH      3
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
  overFlow,
  stack,
  doesNotExist
};

struct EEstack{
  int         topOfStack;
  numberBase  displayFormat;
};

struct queue{
	byte        data[MAX_QUEUE_SIZE];
	byte        size;
  byte        front;
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

const char functionNames[NUM_FUNCTIONS][FUNC_NAME_LENGTH+1] = {
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
        functionMode(&RPNstack, &buttonStore, keyPress);
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
      break;  
    }
    case BACK_SPACE:{
      back(qu);
      break;
    }
    case ENTER:{
      if(!isEmpty(qu)) {
        long qValue = getQueueValue(qu);
        if(qValue < NUM_FUNCTIONS && qValue > 0){
          if(!callFunction((byte)qValue, st)){
            errorState =  stack;
          }
          calcMode = prevMode;
          clearQueue(qu);
        }
        else {
          errorState = doesNotExist;
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
        pushQueueToStack(qu, st);
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
long Peek(EEstack* st){
  long data = TOP_OF_STACK;
  if(!isEmpty(st)){
    EEPROM.get(st->topOfStack - sizeof(long), data);
  }
  
  return data;
}

boolean push(EEstack* st, long data){
  if(st->topOfStack == EEPROM.length()){
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
  int top = 0;
  long data;
  EEPROM.get(top, data);
  while(data != TOP_OF_STACK){
    top+=sizeof(long);
    EEPROM.get(top, data);
  }
  st->displayFormat=decimal;
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

//QUEUE 

//QUEUE - Primitives 
void intQueue(queue* qu){
  qu->size=0;               //set the size initially to 0
  qu->front=0;              //set the front initially to 0
  qu->positive=true;        //intiially make it positive
  qu->entryFormat=decimal;  //intiially make it decimal
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

byte Peek(queue* qu){
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

void clearQueue(queue* qu){
  //while qu still contains items, remove the item at the front
  while(!isEmpty(qu)){
    dequeue(qu);
  }
  qu->positive=true;
}

//QUEUE - Customs

void serialPrintQueue(queue* qu){ 
  long temp = getQueueValue(qu);
  //if there's no error just print the value in the queue
  if(errorState == none){
    if(qu->entryFormat == hexadecimal){
      Serial.print(temp,HEX);           //print in hex
    }
    else if(qu->entryFormat == octal){
      Serial.print(temp,OCT);           //print in oct
    }
    else if(qu->entryFormat == binary){
      Serial.print(temp,BIN);           //print in binary
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

//prints a line in serial monitor
void serialPrintLine(){
  byte i;
  for(i=0; i<SCREEN_WIDTH; i++){
    Serial.print("-");
  }
  Serial.print("\n");
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

void serialPrintCatalogue(queue* qu){
  byte i;
  
  Serial.print('\n');
  Serial.print('\n');
  Serial.print('\n');
  //print the names and numbers of all the functions
  for(i=STACK_DISPLAY_HEIGHT; i>0; i--){
    //print the function number
    Serial.print(i+(NUM_FUNCTIONS*rowLevel)); 
    Serial.print(": ");
    //print the function name
    Serial.print(functionNames[i+(NUM_FUNCTIONS*rowLevel)-1]);
    Serial.print('\n');
  }

  //print the line followed by the value in the queue
  serialPrintLine();
  serialPrintQueue(qu);
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
      updateSerial(qu, st);         //update the screen
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
  }
  //go from state0 to state1
  else if(st->displayFormat==decimal&&qu->entryFormat==decimal){
    st->displayFormat=format;
    qu->entryFormat=decimal; 
  }
  //go from state1 to state2
  else if(st->displayFormat==format&&qu->entryFormat==decimal){
    st->displayFormat=decimal;
    qu->entryFormat=format; 
  }
  //go from state2 to state3
  else if(st->displayFormat==decimal&&qu->entryFormat==format){
    st->displayFormat=format;
    qu->entryFormat=format; 
  }
  //if in some other unrecognized state go to state1
  //ie it moved from a different format
  else {
    st->displayFormat=format;
    qu->entryFormat=decimal; 
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
    default:{
      errorState = doesNotExist;
      break; 
    }
  }
  return success;
}
