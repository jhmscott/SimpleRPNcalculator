#include "EEPROM.h"
#include "Keypad.h"

#define DIVIDE_BY_ZERO -32768
#define TOP_OF_STACK 32767
#define LINE_LENGTH 10
#define MAX_QUEUE_SIZE 10
#define STACK_DISPLAY_HEIGHT 4
#define ROWS 6 //four rows
#define COLS 4 //four columns

struct EEstack{
  int topOfStack;
};

struct queue{
	int data[MAX_QUEUE_SIZE];
	int size;
  int front;
  boolean positive;
};


char keys[ROWS][COLS] = {
  {'U','D','C','B'},
  {'\0','\0','\0','+'},
  {'1','2','3','-'},
  {'4','5','6','*'},
  {'7','8','9', '/'},
  {'0','.','N', 'E'}
};

byte rowPins[ROWS] = {5, 4, 3, 2}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {8, 7, 6}; //connect to the column pinouts of the keypad

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS);

EEstack RPNstack;
queue   buttonStore;
char    keyPress;

void setup() {
  intStack(&RPNstack);
  intQueue(&buttonStore);
  Serial.begin(9600);
  printStack(&RPNstack);
  printLine();
  printQueue(&buttonStore);
}

void loop() {
  //get a key from the keypad
  //keyPress = keypad.getKey();

  //if a key was pressed
  if(Serial.available() > 0){
    keyPress = Serial.read();
    //if clear key pressed
    switch(keyPress){
     case 'C': {
        //if nothing has been entered in the button store, pop the most recent item off the stack
        if(isEmpty(&buttonStore)&&!isEmpty(&RPNstack)) {
          pop(&RPNstack);  
        }  
        //otherwise clear the button store
        else{
          clearQueue(&buttonStore);
        }
        break;  
      }
      //if enter was pressed, push the contents of button store to the stack
      case 'E': {
        pushQueueToStack(&buttonStore, &RPNstack);
        break;
      }
      case '+':{
        add(&RPNstack, &buttonStore);
        break;
      }
      case '-':{
        sub(&RPNstack, &buttonStore);
        break;
      }
      case '*':{
        mul(&RPNstack, &buttonStore);
        break;
      }
      case '/':{
        dvd(&RPNstack, &buttonStore);
        break;
      }
      case 'N':{
        if(isEmpty(&buttonStore)&&!isEmpty(&RPNstack)){
          int temp = Peek(&RPNstack);
          pop(&RPNstack);
          push(&RPNstack, temp*-1);
        }
        else{
          buttonStore.positive=!buttonStore.positive;
        }
        break;
      }
      case'.':{
        //do nothing now, will be used later when float support is added
        break;  
      }
      case'U':{
        rollUp(&RPNstack,&buttonStore);
        break;
      }
      case'D':{
        rollDown(&RPNstack,&buttonStore);
        break;
      }
      //otherwise one of the number buttons was pressed
      default:{
        //convert it to an int and enqueue that value
        enqueue(&buttonStore, charToInt(keyPress));
        break;
      }
    }
    //print the updated stack 
    printStack(&RPNstack);
    printLine();
    printQueue(&buttonStore);
  }
  
 

}

//STACK operations
//
//STACK-primitives
int Peek(EEstack* st){
  int data;
  EEPROM.get(st->topOfStack - sizeof(int), data);
  return data;
}

int push(EEstack* st, int data){
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

int pop(EEstack* st){
  if(st->topOfStack == 0){
    return false;
  }
  else{
    EEPROM.put(st->topOfStack, TOP_OF_STACK);
    st->topOfStack-=sizeof(int);
    return true;
  }
}

int isEmpty(EEstack* st){
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
void add(EEstack* st, queue* qu){
  int num1, num2;
  if(!isEmpty(qu)){
    pushQueueToStack(qu, st);
  }
  num1 = Peek(st);
  pop(st);
  num2 = Peek(st);
  pop(st);
  push(st, num1+num2);  
}

void sub(EEstack* st, queue* qu){
  int num1, num2;
  if(!isEmpty(qu)){
    pushQueueToStack(qu, st);
  }
  num1 = Peek(st);
  pop(st);
  num2 = Peek(st);
  pop(st);
  push(st, num2-num1); 
}

void mul(EEstack* st, queue* qu){
  int num1, num2;
  if(!isEmpty(qu)){
    pushQueueToStack(qu, st);
  }
  num1 = Peek(st);
  pop(st);
  num2 = Peek(st);
  pop(st);
  push(st, num1*num2);;  
}

void dvd(EEstack* st, queue* qu){
  int num1, num2;
  if(!isEmpty(qu)){
    pushQueueToStack(qu, st);
  }
  num1 = Peek(st);
  pop(st);
  num2 = Peek(st);
  pop(st);
  if(num1 == 0){
  push(st, DIVIDE_BY_ZERO);
  }
  else{
    push(st, num2/num1);;  
  }
}

//STACK - customs
void printStack(EEstack* st){
  //position in the stack, start at the top
  int pos=0, top = st->topOfStack-sizeof(int);
  int temp;

  Serial.print('\n');
  Serial.print('\n');
  Serial.print('\n');
  if(top >= 0){
    if(top >= (STACK_DISPLAY_HEIGHT)*sizeof(int)){
      pos = top-(STACK_DISPLAY_HEIGHT-1)*sizeof(int);
    }
  
    while(pos <= top){
      //get a number from the stack
      EEPROM.get(pos, temp);
      //print the position in the stack followed by its number
      Serial.print((((top-pos)/sizeof(int))+1));
      Serial.print(": ");
      if(temp == DIVIDE_BY_ZERO){
        Serial.print("Error: divide by Zero");
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
}

int isFull(queue* qu) {
  if(qu->size== MAX_QUEUE_SIZE){
    return true;
  }
  else {
    return false;
  }
}

int isEmpty(queue* qu){
  if(qu->size == 0){
    return true;
  }
  else{
    return false;
  }
}

int enqueue(queue* qu, int value){
  if(isFull(qu)){
    return false;  
  }
  else{
     qu->data[(qu->front + qu->size) % MAX_QUEUE_SIZE] = value; 
     qu->size++;
     return true;
  }
}

int dequeue(queue* qu){
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


void clearQueue(queue* qu){
  //while qu still contains items, remove the item at the front
  while(!isEmpty(qu)){
    dequeue(qu);
  }
  qu->positive=true;
}

//QUEUE - Customs


void printQueue(queue* qu){ 
  Serial.print(getQueueValue(qu));
}

int getQueueValue(queue* qu){
  int temp = 0;
  int pos = qu->front;
  int qSize = qu->size;
  //while we haven't reached the end of the q
  while(qSize != 0){
    temp += qu->data[pos]*power(10, qSize-1);
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
    temp += Peek(qu)*power(10, ((qu->size)-1));
    
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

void printLine(){
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
