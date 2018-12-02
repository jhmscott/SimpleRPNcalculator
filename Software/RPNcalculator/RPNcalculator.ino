#include "EEPROM.h"
#include "Keypad.h"

#define DIVIDE_BY_ZERO -32768
#define MAX_QUEUE_SIZE 10
#define ROWS 4 //four rows
#define COLS 3 //three columns

struct EEstack{
  int topOfStack;
};

struct queue{
	int data[MAX_QUEUE_SIZE];
	int size;
  int front;
};


char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'0','C','E'}
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
}

void loop() {
  //get a key from the keypad
  //keyPress = keypad.getKey();

  //if a key was pressed
  if(Serial.available() > 0){
    keyPress = Serial.read();
    //if clear key pressed
    if(keyPress == 'C')
    {
      //if nothing has been entered in the button store, pop the most recent item off the stack
      if(isEmpty(&buttonStore)) {
        pop(&RPNstack);  
      } 
      //otherwise clear the button store
      else{
        clearQueue(&buttonStore);
      }
    }
    //if enter was pressed, push the contents of button store to the stack
    else if(keyPress == 'E'){
      pushQueueToStack(&buttonStore, &RPNstack);
    }
    //otherwise one of the number buttons was pressed
    else{
      //convert it to an int and enqueue that value
      enqueue(&buttonStore, charToInt(keyPress));
    }
    //print the updated stack 
    printStack(&RPNstack);
    printQueue(&buttonStore);
  }
  
 

}

void printQueue(queue* qu){
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
  Serial.print(temp);
}

void printStack(EEstack* st){
  //position in the stack, start at the top
  int pos=0, top = st->topOfStack-sizeof(int);
  int temp;

  Serial.print('\n');
  Serial.print('\n');
  Serial.print('\n');
  
  while(pos <= top){
    //get a number from the stack
    EEPROM.get(pos, temp);
    //print the position in the stack followed by its number
    Serial.print(((pos/sizeof(int))+1));
    Serial.print(": ");
    Serial.print(temp);
    Serial.print('\n');
    //decrement position
    pos += sizeof(int);
  }
}
void pushQueueToStack(queue* qu, EEstack* st){
  int temp = 0;
  //while qu still contains items
  while(!isEmpty(qu)){
    //multiply the digit by 10 to the power of its position
    //add this to the running total
    temp += Peek(qu)*power(10, ((qu->size)-1));
    
    /*Serial.print(Peek(qu));
    Serial.print('\n');
    Serial.print(temp);
    Serial.print('\n');
    Serial.print(qu->size);
    Serial.print('\n');*/
    //clear the queue as you go through it
    dequeue(qu);
  }
  push(st, temp);
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

void clearQueue(queue* qu){
  //while qu still contains items, remove the item at the front
  while(!isEmpty(qu)){
    dequeue(qu);
  }
}

int charToInt(char character){
  return (int)(character-48); 
}
void intQueue(queue* qu){
  qu->size=0;
  qu->front=0;
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

int add(EEstack* st){
  int num1, num2;
  num1 = Peek(st);
  pop(st);
  num2 = Peek(st);
  pop(st);
  return num1+num2;  
}

int sub(EEstack* st){
  int num1, num2;
  num1 = Peek(st);
  pop(st);
  num2 = Peek(st);
  pop(st);
  return num2-num1;  
}

int mul(EEstack* st){
  int num1, num2;
  num1 = Peek(st);
  pop(st);
  num2 = Peek(st);
  pop(st);
  return num1*num2;  
}

int dvd(EEstack* st){
  int num1, num2;
  num1 = Peek(st);
  pop(st);
  num2 = Peek(st);
  pop(st);
  if(num1 == 0){
	return DIVIDE_BY_ZERO;
  }
  else{
	return num2/num1;  
  }
}

void intStack(EEstack* st)
{
  st->topOfStack=0;  
}

int push(EEstack* st, int data){
  if(st->topOfStack == EEPROM.length()){
	return false;
  }
  else{
	EEPROM.put(st->topOfStack, data);
	st->topOfStack+=sizeof(int);
	return true;
  }
}

int pop(EEstack* st){
  if(st->topOfStack == 0){
	  return false;
  }
  else{
	st->topOfStack-=sizeof(int);
	return true;
  }
}

int Peek(EEstack* st){
  int data;
  EEPROM.get(st->topOfStack - sizeof(int), data);
  return data;
}
