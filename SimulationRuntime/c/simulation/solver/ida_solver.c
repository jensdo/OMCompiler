/*
 * This file is part of OpenModelica.
 *
 * Copyright (c) 1998-2016, Open Source Modelica Consortium (OSMC),
 * c/o Linköpings universitet, Department of Computer and Information Science,
 * SE-58183 Linköping, Sweden.
 *
 * All rights reserved.
 *
 * THIS PROGRAM IS PROVIDED UNDER THE TERMS OF THE BSD NEW LICENSE OR THE
 * GPL VERSION 3 LICENSE OR THE OSMC PUBLIC LICENSE (OSMC-PL) VERSION 1.2.
 * ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS PROGRAM CONSTITUTES
 * RECIPIENT'S ACCEPTANCE OF THE OSMC PUBLIC LICENSE OR THE GPL VERSION 3,
 * ACCORDING TO RECIPIENTS CHOICE.
 *
 * The OpenModelica software and the OSMC (Open Source Modelica Consortium)
 * Public License (OSMC-PL) are obtained from OSMC, either from the above
 * address, from the URLs: http://www.openmodelica.org or
 * http://www.ida.liu.se/projects/OpenModelica, and in the OpenModelica
 * distribution. GNU version 3 is obtained from:
 * http://www.gnu.org/copyleft/gpl.html. The New BSD License is obtained from:
 * http://www.opensource.org/licenses/BSD-3-Clause.
 *
 * This program is distributed WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, EXCEPT AS
 * EXPRESSLY SET FORTH IN THE BY RECIPIENT SELECTED SUBSIDIARY LICENSE
 * CONDITIONS OF OSMC-PL.
 *
 */

 /*! \file ida_solver.c
 */

#include <string.h>
#include <setjmp.h>

#include "omc_config.h"
#include "openmodelica.h"
#include "openmodelica_func.h"
#include "simulation_data.h"

#include "util/omc_error.h"
#include "util/memory_pool.h"

#include "simulation/options.h"
#include "simulation/simulation_runtime.h"
#include "simulation/results/simulation_result.h"
#include "simulation/solver/solver_main.h"
#include "simulation/solver/model_help.h"
#include "simulation/solver/external_input.h"
#include "simulation/solver/epsilon.h"
#include "simulation/solver/ida_solver.h"

#ifdef WITH_SUNDIALS


#include <sundials/sundials_nvector.h>
#include <nvector/nvector_serial.h>
#include <ida/ida.h>
#include <ida/ida_dense.h>



static int jacobianOwnNumColoredIDA(long int Neq, realtype tt, realtype cj,
    N_Vector yy, N_Vector yp, N_Vector rr, DlsMat Jac, void *user_data,
    N_Vector tmp1, N_Vector tmp2, N_Vector tmp3);

static int jacobianOwnNumIDA(long int Neq, realtype tt, realtype cj,
    N_Vector yy, N_Vector yp, N_Vector rr, DlsMat Jac, void *user_data,
    N_Vector tmp1, N_Vector tmp2, N_Vector tmp3);


int checkIDAflag(int flag)
{
  TRACE_PUSH
  int retVal;
  switch(flag)
  {
    case IDA_SUCCESS:
      retVal = 0;
      break;
    default:
      retVal = 1;
      break;
  }
  TRACE_POP
  return retVal;
}

void errOutputIDA(int error_code, const char *module, const char *function,
                     char *msg, void *userData)
{
  TRACE_PUSH
  DATA* data = (DATA*)(((IDA_USERDATA*)((IDA_SOLVER*)userData)->simData)->data);
  infoStreamPrint(LOG_SOLVER, 1, "#### IDA error message #####");
  infoStreamPrint(LOG_SOLVER, 0, " -> error code %d\n -> module %s\n -> function %s", error_code, module, function);
  infoStreamPrint(LOG_SOLVER, 0, " Message: %s", msg);
  messageClose(LOG_SOLVER);
  TRACE_POP
}


int residualFunctionIDA(double time, N_Vector yy, N_Vector yp, N_Vector res, void* userData)
{
  TRACE_PUSH
  DATA* data = (DATA*)(((IDA_USERDATA*)((IDA_SOLVER*)userData)->simData)->data);
  threadData_t* threadData = (threadData_t*)(((IDA_USERDATA*)((IDA_SOLVER*)userData)->simData)->threadData);

  double timeBackup;
  long i;
  int saveJumpState;
  int success = 0, retVal = 0;

  if (data->simulationInfo->currentContext == CONTEXT_ALGEBRAIC)
  {
    setContext(data, &time, CONTEXT_ODE);
  }

  timeBackup = data->localData[0]->timeValue;
  data->localData[0]->timeValue = time;

  saveJumpState = threadData->currentErrorStage;
  threadData->currentErrorStage = ERROR_INTEGRATOR;

  /* try */
#if !defined(OMC_EMCC)
  MMC_TRY_INTERNAL(simulationJumpBuffer)
#endif

  /* read input vars */
  externalInputUpdate(data);
  data->callback->input_function(data, threadData);

  /* eval input vars */
  data->callback->functionODE(data, threadData);

  /* get the difference between the temp_xd(=localData->statesDerivatives)
     and xd(=statesDerivativesBackup) */
  for(i=0; i < data->modelData->nStates; i++)
  {

    NV_Ith_S(res, i) = data->localData[0]->realVars[data->modelData->nStates + i] - NV_Ith_S(yp, i);
  }
  success = 1;
#if !defined(OMC_EMCC)
  MMC_CATCH_INTERNAL(simulationJumpBuffer)
#endif

  if (!success) {
    retVal = -1;
  }

  threadData->currentErrorStage = saveJumpState;

  data->localData[0]->timeValue = timeBackup;

  if (data->simulationInfo->currentContext == CONTEXT_ODE){
    unsetContext(data);
  }
  messageClose(LOG_SOLVER);

  TRACE_POP
  return retVal;
}

int rootsFunctionIDA(double time, N_Vector yy, N_Vector yp, double *gout, void* userData)
{
  TRACE_PUSH
  DATA* data = (DATA*)(((IDA_USERDATA*)((IDA_SOLVER*)userData)->simData)->data);
  threadData_t* threadData = (threadData_t*)(((IDA_USERDATA*)((IDA_SOLVER*)userData)->simData)->threadData);

  double timeBackup;
  int saveJumpState;

  if (data->simulationInfo->currentContext == CONTEXT_ALGEBRAIC)
  {
    setContext(data, &time, CONTEXT_EVENTS);
  }

  saveJumpState = threadData->currentErrorStage;
  threadData->currentErrorStage = ERROR_EVENTSEARCH;

  timeBackup = data->localData[0]->timeValue;
  data->localData[0]->timeValue = time;

  /* read input vars */
  externalInputUpdate(data);
  data->callback->input_function(data, threadData);
  /* eval needed equations*/
  data->callback->function_ZeroCrossingsEquations(data, threadData);

  data->callback->function_ZeroCrossings(data, threadData, gout);

  threadData->currentErrorStage = saveJumpState;
  data->localData[0]->timeValue = timeBackup;


  if (data->simulationInfo->currentContext == CONTEXT_EVENTS){
    unsetContext(data);
  }


  TRACE_POP
  return 0;
}


/* initial main ida data */
int
ida_solver_initial(DATA* data, threadData_t *threadData, SOLVER_INFO* solverInfo, IDA_SOLVER *idaData){

  TRACE_PUSH

  int flag;
  long i;
  double* tmp;

  /* sim data */
  idaData->simData = (IDA_USERDATA*)malloc(sizeof(IDA_USERDATA));
  idaData->simData->data = data;
  idaData->simData->threadData = threadData;

  /* initialize constants */
  idaData->setInitialSolution = 0;

  /* start initialization routines of sundials */
  idaData->ida_mem = IDACreate();
  if (idaData->ida_mem == NULL){
    throwStreamPrint(threadData, "##IDA## Initialization of IDA solver failed!");
  }

  idaData->y = N_VMake_Serial(data->modelData->nStates, data->localData[0]->realVars);
  idaData->yp = N_VMake_Serial(data->modelData->nStates, data->localData[0]->realVars + data->modelData->nStates);

  flag = IDAInit(idaData->ida_mem,
                 residualFunctionIDA,
                 data->simulationInfo->startTime,
                 idaData->y,
                 idaData->yp);

  /* allocate memory for jacobians calculation */
  idaData->sqrteps = sqrt(DBL_EPSILON);
  idaData->ysave = (double*) malloc(data->modelData->nStates*sizeof(double));
  idaData->delta_hh = (double*) malloc(data->modelData->nStates*sizeof(double));
  idaData->errwgt = N_VNew_Serial(data->modelData->nStates);
  idaData->newdelta = N_VNew_Serial(data->modelData->nStates);

  if (checkIDAflag(flag)){
      throwStreamPrint(threadData, "##IDA## Something goes wrong while initialize IDA solver!");
  }

  flag = IDASetUserData(idaData->ida_mem, idaData);
  if (checkIDAflag(flag)){
    throwStreamPrint(threadData, "##IDA## Something goes wrong while initialize IDA solver!");
  }

  flag = IDASetErrHandlerFn(idaData->ida_mem, errOutputIDA, idaData);
  if (checkIDAflag(flag)){
    throwStreamPrint(threadData, "##IDA## Something goes wrong while set error handler!");
  }

  /* set nominal values of the states for absolute tolerances */
  infoStreamPrint(LOG_SOLVER, 1, "The relative tolerance is %g. Following absolute tolerances are used for the states: ", data->simulationInfo->tolerance);
  tmp = (double*) malloc(data->modelData->nStates*sizeof(double));
  for(i=0; i<data->modelData->nStates; ++i)
  {
    tmp[i] = data->simulationInfo->tolerance * fmax(fabs(data->modelData->realVarsData[i].attribute.nominal), 1e-32);
    infoStreamPrint(LOG_SOLVER, 0, "##IDA## %ld. %s -> %g", i+1, data->modelData->realVarsData[i].info.name, tmp[i]);
  }
  messageClose(LOG_SOLVER);
  flag = IDASVtolerances(idaData->ida_mem,
                        data->simulationInfo->tolerance,
                        N_VMake_Serial(data->modelData->nStates,tmp));
  if (checkIDAflag(flag)){
    throwStreamPrint(threadData, "##IDA## Setting tolerances fails while initialize IDA solver!");
  }

  /* set linear solver */
  flag = IDADense(idaData->ida_mem, data->modelData->nStates);
  if (checkIDAflag(flag)){
    throwStreamPrint(threadData, "##IDA## Setting linear solver fails while initialize IDA solver!");
  }

  /* set root function */
  flag = IDARootInit(idaData->ida_mem, data->modelData->nZeroCrossings, rootsFunctionIDA);
  if (checkIDAflag(flag)){
    throwStreamPrint(threadData, "##IDA## Setting root function fails while initialize IDA solver!");
  }


  /* if FLAG_JACOBIAN is set, choose dassl jacobian calculation method */
  if (omc_flag[FLAG_JACOBIAN])
  {
    for(i=1; i< JAC_MAX;i++)
    {
      if(!strcmp((const char*)omc_flagValue[FLAG_JACOBIAN], JACOBIAN_METHOD[i])){
        idaData->jacobianMethod = (int)i;
        break;
      }
    }
    if(idaData->jacobianMethod == JAC_UNKNOWN)
    {
      if (ACTIVE_WARNING_STREAM(LOG_SOLVER))
      {
        warningStreamPrint(LOG_SOLVER, 1, "unrecognized jacobian calculation method %s, current options are:", (const char*)omc_flagValue[FLAG_JACOBIAN]);
        for(i=1; i < JAC_MAX; ++i)
        {
          warningStreamPrint(LOG_SOLVER, 0, "%-15s [%s]", JACOBIAN_METHOD[i], JACOBIAN_METHOD_DESC[i]);
        }
        messageClose(LOG_SOLVER);
      }
      throwStreamPrint(threadData,"unrecognized jacobian calculation method %s", (const char*)omc_flagValue[FLAG_JACOBIAN]);
    }
  /* default case colored numerical jacobian */
  }
  else
  {
    idaData->jacobianMethod = COLOREDNUMJAC;
  }

  /* selects the calculation method of the jacobian */
  if(idaData->jacobianMethod == COLOREDNUMJAC ||
     idaData->jacobianMethod == COLOREDSYMJAC ||
     idaData->jacobianMethod == SYMJAC)
  {
    if (data->callback->initialAnalyticJacobianA(data, threadData))
    {
      infoStreamPrint(LOG_STDOUT, 0, "Jacobian or SparsePattern is not generated or failed to initialize! Switch back to normal.");
      idaData->jacobianMethod = INTERNALNUMJAC;
    }
  }

  /* set up the appropriate function pointer */
  switch (idaData->jacobianMethod){
    case SYMJAC:
    case COLOREDSYMJAC:
      infoStreamPrint(LOG_STDOUT, 0, "The symbolic jacobian is not implemented, yet! Switch back to internal.");
      break;
    case COLOREDNUMJAC:
      /* set jacobian function */
      flag = IDADlsSetDenseJacFn(idaData->ida_mem, jacobianOwnNumColoredIDA);
      if (checkIDAflag(flag)){
        throwStreamPrint(threadData, "##IDA## Setting jacobian function fails while initialize IDA solver!");
      }
      break;
    case NUMJAC:
      /* set jacobian function */
      flag = IDADlsSetDenseJacFn(idaData->ida_mem, jacobianOwnNumIDA);
      if (checkIDAflag(flag)){
        throwStreamPrint(threadData, "##IDA## Setting jacobian function fails while initialize IDA solver!");
      }
      break;
    case INTERNALNUMJAC:
      break;
    default:
      throwStreamPrint(threadData,"unrecognized jacobian calculation method %s", (const char*)omc_flagValue[FLAG_JACOBIAN]);
      break;
  }
  infoStreamPrint(LOG_SOLVER, 0, "jacobian is calculated by %s", JACOBIAN_METHOD_DESC[idaData->jacobianMethod]);


  TRACE_POP
  return 0;
}

/* deinitial ida data */
int
ida_solver_deinitial(IDA_SOLVER *idaData){
  TRACE_PUSH

  free(idaData->simData);
  free(idaData->ysave);
  free(idaData->delta_hh);

  N_VDestroy_Serial(idaData->errwgt);
  N_VDestroy_Serial(idaData->newdelta);

  IDAFree(&idaData->ida_mem);

  TRACE_POP
  return 0;
}

/* main ida function to make a step */
int
ida_solver_step(DATA* data, threadData_t *threadData, SOLVER_INFO* solverInfo)
{
  TRACE_PUSH
  double tout = 0;
  int i = 0, flag;
  int retVal = 0, finished = FALSE;
  int saveJumpState;
  long int tmp;

  IDA_SOLVER *idaData = (IDA_SOLVER*) solverInfo->solverData;

  SIMULATION_DATA *sData = data->localData[0];
  SIMULATION_DATA *sDataOld = data->localData[1];
  MODEL_DATA *mData = (MODEL_DATA*) data->modelData;


  /* alloc all work arrays */
  N_VSetArrayPointer(data->localData[0]->realVars, idaData->y);
  N_VSetArrayPointer(data->localData[1]->realVars + data->modelData->nStates, idaData->yp);

  if (solverInfo->didEventStep)
  {
    idaData->setInitialSolution = 0;
  }

  /* reinit solver */
  if (!idaData->setInitialSolution)
  {
    flag = IDAReInit(idaData->ida_mem,
                     solverInfo->currentTime,
                     idaData->y,
                     idaData->yp);


    debugStreamPrint(LOG_SOLVER, 0, "Re-initialized IDA Solver");

    if (checkIDAflag(flag)){
      throwStreamPrint(threadData, "##IDA## Something goes wrong while reinit IDA solver after event!");
    }
    idaData->setInitialSolution = 1;
  }

  saveJumpState = threadData->currentErrorStage;
  threadData->currentErrorStage = ERROR_INTEGRATOR;

  /* try */
#if !defined(OMC_EMCC)
  MMC_TRY_INTERNAL(simulationJumpBuffer)
#endif


  /* Check that tout is not less than timeValue otherwise the solver
   * will come in trouble.
   * If that is the case we skip the current step. */
  if (solverInfo->currentStepSize < DASSL_STEP_EPS)
  {
    infoStreamPrint(LOG_SOLVER, 0, "Desired step to small try next one");
    infoStreamPrint(LOG_SOLVER, 0, "Interpolate linear");

    /* linear extrapolation */
    for(i = 0; i < data->modelData->nStates; i++)
    {
      sData->realVars[i] = sDataOld->realVars[i] + sDataOld->realVars[data->modelData->nStates+i] * solverInfo->currentStepSize;
    }
    sData->timeValue = solverInfo->currentTime + solverInfo->currentStepSize;
    data->callback->functionODE(data, threadData);
    solverInfo->currentTime = sData->timeValue;

    TRACE_POP
    return 0;
  }


  /* Calculate steps until TOUT is reached */
  tout = solverInfo->currentTime + solverInfo->currentStepSize;


  do
  {
    infoStreamPrint(LOG_SOLVER, 1, "##IDA## new step at time = %.15g", solverInfo->currentTime);

    /* read input vars */
    externalInputUpdate(data);
    data->callback->input_function(data, threadData);

    flag = IDASolve(idaData->ida_mem, tout, &solverInfo->currentTime, idaData->y, idaData->yp, IDA_NORMAL);

    /* set time to current time */
    sData->timeValue = solverInfo->currentTime;

    /* error handling */
    if ( !checkIDAflag(flag) && solverInfo->currentTime >=tout)
    {
      infoStreamPrint(LOG_SOLVER, 0, "##IDA## step to time = %.15g", solverInfo->currentTime);
      finished = TRUE;
    }
    else
    {
      if (!checkIDAflag(flag))
      {
        infoStreamPrint(LOG_SOLVER, 0, "##IDA## continue integration time = %.15g", solverInfo->currentTime);
      }
      else if (flag == IDA_ROOT_RETURN)
      {
        infoStreamPrint(LOG_SOLVER, 0, "##IDA## root found at time = %.15g", solverInfo->currentTime);
        finished = TRUE;
      }
      else
      {
        infoStreamPrint(LOG_STDOUT, 0, "##IDA## %d error occurred at time = %.15g", flag, solverInfo->currentTime);
        finished = TRUE;
        retVal = flag;
      }
    }
    /* closing new step message */
    messageClose(LOG_SOLVER);

  } while(!finished);

#if !defined(OMC_EMCC)
  MMC_CATCH_INTERNAL(simulationJumpBuffer)
#endif
  threadData->currentErrorStage = saveJumpState;

  /* if a state event occurs than no sample event does need to be activated  */
  if (data->simulationInfo->sampleActivated && solverInfo->currentTime < data->simulationInfo->nextSampleEvent)
  {
    data->simulationInfo->sampleActivated = 0;
  }

  /* save stats */
  /* steps */
  tmp = 0;
  flag = IDAGetNumSteps(idaData->ida_mem, &tmp);
  if (flag == IDA_SUCCESS)
  {
    solverInfo->solverStatsTmp[0] = tmp;
  }

  /* functionODE evaluations */
  tmp = 0;
  flag = IDAGetNumResEvals(idaData->ida_mem, &tmp);
  if (flag == IDA_SUCCESS)
  {
    solverInfo->solverStatsTmp[1] = tmp;
  }

  /* Jacobians evaluations */
  tmp = 0;
  flag = IDADlsGetNumJacEvals(idaData->ida_mem, &tmp);
  if (flag == IDA_SUCCESS)
  {
    solverInfo->solverStatsTmp[2] = tmp;
  }

  /* local error test failures */
  tmp = 0;
  flag = IDAGetNumErrTestFails(idaData->ida_mem, &tmp);
  if (flag == IDA_SUCCESS)
  {
    solverInfo->solverStatsTmp[3] = tmp;
  }

  /* local error test failures */
  tmp = 0;
  flag = IDAGetNumNonlinSolvConvFails(idaData->ida_mem, &tmp);
  if (flag == IDA_SUCCESS)
  {
    solverInfo->solverStatsTmp[4] = tmp;
  }

  infoStreamPrint(LOG_SOLVER, 0, "##IDA## Finished Integrator step.");

  TRACE_POP
  return retVal;
}


/*
 *  function calculates a jacobian matrix by
 *  numerical method finite differences
 */
static
int jacOwnNumColoredIDA(double tt, N_Vector yy, N_Vector yp, N_Vector rr, DlsMat Jac, void *userData)
{
  TRACE_PUSH
  IDA_SOLVER* idaData = (IDA_SOLVER*)userData;
  DATA* data = (DATA*)(((IDA_USERDATA*)idaData->simData)->data);
  void* ida_mem = idaData->ida_mem;
  const int index = data->callback->INDEX_JAC_A;

  /* prepare variables */
  double *states = N_VGetArrayPointer(yy);
  double *yprime = N_VGetArrayPointer(yp);
  double *delta  = N_VGetArrayPointer(rr);
  double *newdelta = N_VGetArrayPointer(idaData->newdelta);
  double *errwgt = N_VGetArrayPointer(idaData->errwgt);

  double *delta_hh = idaData->delta_hh;
  double *ysave = idaData->ysave;

  double delta_h = idaData->sqrteps;
  double delta_hhh;
  unsigned int i,j,l,ii;

  double currentStep;

  /* set values */
  IDAGetCurrentStep(ida_mem, &currentStep);
  IDAGetErrWeights(ida_mem, idaData->errwgt);

  setContext(data, &tt, CONTEXT_JACOBIAN);

  for(i = 0; i < data->simulationInfo->analyticJacobians[index].sparsePattern.maxColors; i++)
  {
    for(ii=0; ii < data->simulationInfo->analyticJacobians[index].sizeCols; ii++)
    {
      if(data->simulationInfo->analyticJacobians[index].sparsePattern.colorCols[ii]-1 == i)
      {
        delta_hhh = currentStep * yprime[ii];
        delta_hh[ii] = delta_h * fmax(fmax(fabs(states[ii]),fabs(delta_hhh)),fabs(1./errwgt[ii]));
        delta_hh[ii] = (delta_hhh >= 0 ? delta_hh[ii] : -delta_hh[ii]);
        delta_hh[ii] = (states[ii] + delta_hh[ii]) - states[ii];

        ysave[ii] = states[ii];
        states[ii] += delta_hh[ii];

        delta_hh[ii] = 1. / delta_hh[ii];
      }
    }

    residualFunctionIDA(tt, yy, yp, idaData->newdelta, userData);

    increaseJacContext(data);

    for(ii = 0; ii < data->simulationInfo->analyticJacobians[index].sizeCols; ii++)
    {
      if(data->simulationInfo->analyticJacobians[index].sparsePattern.colorCols[ii]-1 == i)
      {
        if(ii==0)
          j = 0;
        else
          j = data->simulationInfo->analyticJacobians[index].sparsePattern.leadindex[ii-1];
        while(j < data->simulationInfo->analyticJacobians[index].sparsePattern.leadindex[ii])
        {
          l  =  data->simulationInfo->analyticJacobians[index].sparsePattern.index[j];
          DENSE_ELEM(Jac, l, ii) = (newdelta[l] - delta[l]) * delta_hh[ii];
          j++;
        };
        states[ii] = ysave[ii];
      }
    }
  }
  unsetContext(data);

  TRACE_POP
  return 0;
}

/*
 * provides a numerical Jacobian to be used with DASSL
 */
static int jacobianOwnNumColoredIDA(long int Neq, double tt, double cj,
    N_Vector yy, N_Vector yp, N_Vector rr,
    DlsMat Jac, void *user_data,
    N_Vector tmp1, N_Vector tmp2, N_Vector tmp3)
{
  TRACE_PUSH
  int i;
  threadData_t* threadData = (threadData_t*)(((IDA_USERDATA*)((IDA_SOLVER*)user_data)->simData)->threadData);

  if(jacOwnNumColoredIDA(tt, yy, yp, rr, Jac, user_data))
  {
    throwStreamPrint(threadData, "Error, can not get Matrix A ");
    TRACE_POP
    return 1;
  }

  /* debug */
  if (ACTIVE_STREAM(LOG_JAC)){
    PrintMat(Jac);
  }

  /* add cj to diagonal elements and store in Jac */
  for(i = 0; i < Neq; i++)
  {
    DENSE_ELEM(Jac, i, i) -= (double) cj;
  }

  TRACE_POP
  return 0;
}

/*
 *  function calculates a jacobian matrix by
 *  numerical method finite differences
 */
static
int jacOwnNumIDA(double tt, N_Vector yy, N_Vector yp, N_Vector rr, DlsMat Jac, void *userData)
{
  TRACE_PUSH
  IDA_SOLVER* idaData = (IDA_SOLVER*)userData;
  DATA* data = (DATA*)(((IDA_USERDATA*)idaData->simData)->data);
  void* ida_mem = idaData->ida_mem;

  /* prepare variables */
  double *states = N_VGetArrayPointer(yy);
  double *yprime = N_VGetArrayPointer(yp);
  double *delta  = N_VGetArrayPointer(rr);
  double *newdelta = N_VGetArrayPointer(idaData->newdelta);
  double *errwgt = N_VGetArrayPointer(idaData->errwgt);

  double ysave;

  double delta_h = idaData->sqrteps;
  double delta_hh;
  double delta_hhh;
  double deltaInv;
  unsigned int i,j;

  double currentStep;

  /* set values */
  IDAGetCurrentStep(ida_mem, &currentStep);
  IDAGetErrWeights(ida_mem, idaData->errwgt);

  setContext(data, &tt, CONTEXT_JACOBIAN);

  for(i = 0; i < data->modelData->nStates; i++)
  {
    delta_hhh = currentStep * yprime[i];
    delta_hh = delta_h * fmax(fmax(fabs(states[i]),fabs(delta_hhh)),fabs(1./errwgt[i]));
    delta_hh = (delta_hhh >= 0 ? delta_hh : -delta_hh);
    delta_hh = (states[i] + delta_hh) - states[i];

    ysave = states[i];
    states[i] += delta_hh;

    deltaInv = 1. / delta_hh;

    residualFunctionIDA(tt, yy, yp, idaData->newdelta, userData);

    increaseJacContext(data);

    for(j = 0; j < data->modelData->nStates; j++)
    {
      DENSE_ELEM(Jac, j, i) = (newdelta[j] - delta[j]) * deltaInv;
    }
    states[i] = ysave;
  }
  unsetContext(data);

  TRACE_POP
  return 0;
}

/*
 * provides a numerical Jacobian to be used with DASSL
 */
static int jacobianOwnNumIDA(long int Neq, double tt, double cj,
    N_Vector yy, N_Vector yp, N_Vector rr,
    DlsMat Jac, void *user_data,
    N_Vector tmp1, N_Vector tmp2, N_Vector tmp3)
{
  TRACE_PUSH
  int i;
  threadData_t* threadData = (threadData_t*)(((IDA_USERDATA*)((IDA_SOLVER*)user_data)->simData)->threadData);

  if(jacOwnNumIDA(tt, yy, yp, rr, Jac, user_data))
  {
    throwStreamPrint(threadData, "Error, can not get Matrix A ");
    TRACE_POP
    return 1;
  }

  /* debug */
  if (ACTIVE_STREAM(LOG_JAC)){
    PrintMat(Jac);
  }

  /* add cj to diagonal elements and store in Jac */
  for(i = 0; i < Neq; i++)
  {
    DENSE_ELEM(Jac, i, i) -= (double) cj;
  }

  TRACE_POP
  return 0;
}

#endif
