///////////////////////////////////////////////////////////////////////
////  Copyright 2015 Samsung Austin Semiconductor, LLC.                //
/////////////////////////////////////////////////////////////////////////
//
#ifndef _PREDICTOR_H_
#define _PREDICTOR_H_

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include "utils.h"
#include "bt9.h"
#include "bt9_reader.h"

#define PHT_CTR_MAX  3
#define PHT_CTR_INIT 2

#define HIST_LEN  4 
#define HASHED_PC_LEN 2
#define THETA 37
#define WEIGHT_MAX 63//corresponding to x in verilog
#define WEIGHT_MIN -64//corresponding to x in verilog

//NOTE competitors are allowed to change anything in this file include the following two defines
//ver2 #define FILTER_UPDATES_USING_BTB     0     //if 1 then the BTB updates are filtered for a given branch if its marker is not btbDYN
//ver2 #define FILTER_PREDICTIONS_USING_BTB 0     //if 1 then static predictions are made (btbANSF=1 -> not-taken, btbATSF=1->taken, btbDYN->use conditional predictor)

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

class PREDICTOR{

  // The state is defined for Gshare, change for your design

 private:
  //UINT32  ghr;           // global history register
  //UINT32  *pht;          // pattern history table
  //UINT32  historyLength; // history length
  //UINT32  numPhtEntries; // entries in pht 
  
  UINT64 ghr;
  INT32 *table;
  UINT64 PC_MASK;
  UINT64 count;
  

 public:


  PREDICTOR(void);
  //NOTE contestants are NOT allowed to use these versions of the functions
//ver2   bool    GetPrediction(UINT64 PC, bool btbANSF, bool btbATSF, bool btbDYN);  
//ver2   void    UpdatePredictor(UINT64 PC, OpType opType, bool resolveDir, bool predDir, UINT64 branchTarget, bool btbANSF, bool btbATSF, bool btbDYN);
//ver2   void    TrackOtherInst(UINT64 PC, OpType opType, bool branchDir, UINT64 branchTarget);

  // The interface to the functions below CAN NOT be changed
  bool    GetPrediction(UINT64 PC);  
  void    UpdatePredictor(UINT64 PC, OpType opType, bool resolveDir, bool predDir, UINT64 branchTarget);
  void    TrackOtherInst(UINT64 PC, OpType opType, bool branchDir, UINT64 branchTarget);

  // Contestants can define their own functions below
};

PREDICTOR::PREDICTOR(void){

  ghr              = 0;
  count = 0;

  table =  new INT32[(1 << HASHED_PC_LEN) * (HIST_LEN + 1)];

  for(UINT32 ii=0; ii < (1 << HASHED_PC_LEN) * (HIST_LEN + 1); ii++){
    table[ii]=0; 
  }
  PC_MASK = 0x0000;
  UINT64 temp = 0x0004;
  for(UINT32 ii=0; ii < HASHED_PC_LEN; ii++){
    PC_MASK = PC_MASK | temp;
    temp = temp << 1;
  }
  

  //print init results
  //UINT32 i;
  //UINT32 j;
  //fprintf(stderr, "init pc_mask is %llx\n", PC_MASK);
  //fprintf(stderr, "****************************************\n");
  //fprintf(stderr, "GHR: %llx\n", ghr);
  //for(i = 0; i < (1 << HASHED_PC_LEN); i++){
  //  fprintf(stderr, "table set %d:", i);
  //  for(j = 0; j < (HIST_LEN + 1); j++){
  //    fprintf(stderr, "|  %d  |", table[i * (HIST_LEN + 1) + j]);
  //  }
  //  fprintf(stderr, "\n");
  //}
  //fprintf(stderr, "****************************************\n");
}

bool   PREDICTOR::GetPrediction(UINT64 PC){
  //if(count == 20)
  //  exit(1);
  count ++;
  UINT32 table_index = (PC & PC_MASK) >> 2;


  long long int sum = 0;
  UINT32 i;
  UINT32 mask = 1;
  for(i = 0; i < (HIST_LEN + 1); i++){
    if(i == 0){
      sum += table[table_index * (HIST_LEN + 1)]; // bias 
    }
    else{
      if(ghr & mask){
        sum += table[table_index * (HIST_LEN + 1) + i];
      }
      else{
        sum -= table[table_index * (HIST_LEN + 1) + i];
      }
      mask = mask << 1;
    }
  }
  fprintf(stderr, "sum is %lld\n", sum);

  if(sum > 0){ 
    return TAKEN; 
  }
  else{
    return NOT_TAKEN; 
  }
}

void  PREDICTOR::UpdatePredictor(UINT64 PC, OpType opType, bool resolveDir, bool predDir, UINT64 branchTarget){
  UINT32 table_index = (PC & PC_MASK) >> 2;

  long long int sum = 0;
  UINT32 i;
  UINT32 mask = 1;
  for(i = 0; i < (HIST_LEN + 1); i++){
    if(i == 0){
      sum += table[table_index * (HIST_LEN + 1)]; // bias 
    }
    else{
      if(ghr & mask){
        sum += table[table_index * (HIST_LEN + 1) + i];
      }
      else{
        sum -= table[table_index * (HIST_LEN + 1) + i];
      }
      mask = mask << 1;
    }
  }

  mask = 1;
  if ((resolveDir != predDir) || (sum < THETA)){
    for(i = 0; i < (HIST_LEN + 1); i++){
      if(i == 0){
        if(resolveDir == TAKEN) {
          if(table[table_index * (HIST_LEN + 1)] != WEIGHT_MAX)
            table[table_index * (HIST_LEN + 1)] += 1;
        }
        else{
          if(table[table_index * (HIST_LEN + 1)] != WEIGHT_MIN)
            table[table_index * (HIST_LEN + 1)] -= 1;
        }
      }
      else{
        //bool past_dir = ghr & mask;
        bool past_dir = TAKEN;
        if((ghr & mask) == 0){
          past_dir = NOT_TAKEN;
        }
        if(resolveDir == past_dir){
          if(table[table_index * (HIST_LEN + 1) + i] != WEIGHT_MAX)
            table[table_index * (HIST_LEN + 1) + i] += 1;
        }
        else{
          if(table[table_index * (HIST_LEN + 1) + i] != WEIGHT_MIN)
            table[table_index * (HIST_LEN + 1) + i] -= 1;
        }
      }
      if(i != 0)
        mask = mask << 1;
    }
  }

  // update the GHR
  ghr = (ghr << 1);

  if(resolveDir == TAKEN){
    ghr++; 
  }
  
  fprintf(stderr, "****************************************\n");
  char truedir = 't';
  char preddir = 't';
  if (resolveDir == NOT_TAKEN){
    truedir = 'n';
  }
  if (predDir == NOT_TAKEN){
    preddir = 'n';
  }
  fprintf(stderr, "PC is %llx, true_dir: %c, pred_dir: %c\n", PC, truedir, preddir);
  fprintf(stderr, "GHR: %llx\n", ghr);
  UINT32 j;
  for(i = 0; i < (1 << HASHED_PC_LEN); i++){
    fprintf(stderr, "table set %d:", i);
    for(j = 0; j < (HIST_LEN + 1); j ++){
      fprintf(stderr, "|  %d  |", table[i * (HIST_LEN + 1) + j]);
    }
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "****************************************\n");
  
}


/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void    PREDICTOR::TrackOtherInst(UINT64 PC, OpType opType, bool branchDir, UINT64 branchTarget){

  // This function is called for instructions which are not
  // conditional branches, just in case someone decides to design
  // a predictor that uses information from such instructions.
  // We expect most contestants to leave this function untouched.

  return;
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////


/***********************************************************/
#endif

