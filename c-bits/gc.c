#include <stdio.h>
#include <stdlib.h>
#include "gc.h"
#include "types.h"

////////////////////////////////////////////////////////////////////////////////
// Imported from types.c
////////////////////////////////////////////////////////////////////////////////

extern int  is_number(int);
extern int  is_tuple(int);
extern int  tuple_at(int* base, int i);
extern int  tuple_size(int* base);
extern int* int_addr(int);
extern int  addr_int(int*);

////////////////////////////////////////////////////////////////////////////////

extern int* HEAP_END;

typedef struct Frame_ {
  int *sp;
  int *bp;
} Frame;

typedef enum Tag_
  { VAddr
  , VStackAddr
  , VNumber
  , VBoolean
  } Tag;

typedef struct Data
  { int* addr;
    int  value;
    int  gcvalue;
  };

typedef struct Value_
  { Tag        tag;
    struct Data data;
  } Value;


int blockSize(int *addr){
  int size = tuple_size(addr) + 2;
  return (size % 2 == 0) ? size : size + 1;

}

Frame caller(int* stack_bottom, Frame frame){
  Frame callerFrame;
  int *bptr = frame.bp;
  if (bptr == stack_bottom){
    return frame;
  } else {
    callerFrame.sp = bptr + 1;
    callerFrame.bp = (int *) *bptr;
    return callerFrame;
  }
}

void print_stack(int* stack_top, int* first_frame, int* stack_bottom){
  Frame frame = {stack_top, first_frame };
  if (DEBUG) fprintf(stderr, "***** STACK: START sp=%p, bp=%p,bottom=%p *****\n", stack_top, first_frame, stack_bottom);
  do {
    if (DEBUG) fprintf(stderr, "***** FRAME: START *****\n");
    for (int *p = frame.sp; p < frame.bp; p++){
      if (DEBUG) fprintf(stderr, "  %p: %p\n", p, (int*)*p);
    }
    if (DEBUG) fprintf(stderr, "***** FRAME: END *****\n");
    frame    = caller(stack_bottom, frame);
  } while (frame.sp != stack_bottom);
  if (DEBUG) fprintf(stderr, "***** STACK: END *****\n");
}

void print_heap(int* heap, int size) {
  fprintf(stderr, "\n");
  for(int i = 0; i < size; i += 1) {
    fprintf(stderr
          , "  %d/%p: %p (%d)\n"
          , i
          , (heap + i)
          , (int*)(heap[i])
          , *(heap + i));
  }
}

int* navigateTuple(int v, int* max){
  if (is_tuple(v)) {
    int* base = int_addr(v);
    if (base > max) {
      max = base;
    }
    base[1] = 1;
    for(int i = 0; i < tuple_size(base); i++){
      max = navigateTuple(base[i+2],max);
    }
  }
  return max;
}

////////////////////////////////////////////////////////////////////////////////
// FILL THIS IN, see documentation in 'gc.h' ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
int* mark( int* stack_top       // ESP
         , int* first_frame	// EBP
         , int* stack_bottom    
         , int* heap_start)
{ 
  int* curr = stack_top;
  int* ebp  = first_frame;
  int* max  = heap_start;
  while (curr != stack_bottom) {
    curr++;
    if (curr == ebp) {
      ebp = (int*)*ebp;
    }
    else {
      max = navigateTuple(*curr,max); 
    }
  }    
  return max;
}

////////////////////////////////////////////////////////////////////////////////
// FILL THIS IN, see documentation in 'gc.h' ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
int* forward( int* heap_start
            , int* max_address)
{
  int* t_base = heap_start;
  int curr_empty = 0;
  while (t_base <= max_address) {
    int size = blockSize(t_base);
    if (t_base[1] == 1 && curr_empty != 0) {
      t_base[1] += (int)t_base- 4 * curr_empty;   
    } else if (t_base[1] != 1) {
      curr_empty += size;
    }
    t_base +=  size;
  }

  int n = (int)max_address + 4 * blockSize(max_address)- 4 * curr_empty;
  return (int*)n;
}


void redirectTuple(int* addr){
  int* base = int_addr(*addr);
  if (is_tuple(*addr) && (base[1]&1) == 1 && base[1]!= 1) {
    *addr = base[1];
    for (int i = 0; i < tuple_size(base); i++) {
      redirectTuple(base+i+2);
    }
  }
  return;
}

////////////////////////////////////////////////////////////////////////////////
// FILL THIS IN, see documentation in 'gc.h' ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void redirect( int* stack_bottom
             , int* stack_top
             , int* first_frame
             , int* heap_start
             , int* max_address )
{
  int* curr = stack_top;
  int* ebp  = first_frame;
  int* max  = heap_start;
  while (curr != stack_bottom) {
    curr++;
    if (curr == ebp) {
      curr++;
      ebp = (int*)*ebp;
    } else {
      redirectTuple(curr);
    }
  }
  return; 
}

////////////////////////////////////////////////////////////////////////////////
// FILL THIS IN, see documentation in 'gc.h' ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void compact( int* heap_start
            , int* max_address
            , int* heap_end )
{
  int* t_base = heap_start;
  int* t_start;
  print_heap(t_base, blockSize(t_base));
  while(t_base <= max_address){
    int size = blockSize(t_base);
    if (((t_base[1] & 1) == 1) && (t_base[1] != 1)) {
      int*  _base = int_addr(t_base[1]);
      t_base[1] = 0;  
      for (int i = 0; i< size+2; i++){
        _base[i] = t_base[i];
        if (t_base ==  max_address){
          t_start = _base + size;
        }
      } 
    }
    else if (t_base[1] == 1) {
      if (t_base ==  max_address) {
        t_start = t_base + size;
        fprintf(stderr,"Fresh start: t_start, %p\n",t_start);
      }
    } 
    t_base += size;
  }
  while(t_start < heap_end){
    *t_start = 0;
    t_start++;
  } 
  print_heap(t_base, blockSize(t_base));
  return;
}


////////////////////////////////////////////////////////////////////////////////
// Top-level GC function (you can leave this as is!) ///////////////////////////
////////////////////////////////////////////////////////////////////////////////

int* gc( int* stack_bottom
       , int* stack_top
       , int* first_frame
       , int* heap_start
       , int* heap_end )
{

  int* max_address = mark( stack_top
                         , first_frame
                         , stack_bottom
                         , heap_start );

  int* new_address = forward( heap_start
                            , max_address );

                     redirect( stack_bottom
                             , stack_top
                             , first_frame
                             , heap_start
                             , max_address );

                     compact( heap_start
                            , max_address
                            , heap_end );

  return new_address;
}
