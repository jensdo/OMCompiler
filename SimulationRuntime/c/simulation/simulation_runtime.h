/*
 * This file is part of OpenModelica.
 *
 * Copyright (c) 1998-CurrentYear, Linköping University,
 * Department of Computer and Information Science,
 * SE-58183 Linköping, Sweden.
 *
 * All rights reserved.
 *
 * THIS PROGRAM IS PROVIDED UNDER THE TERMS OF GPL VERSION 3 
 * AND THIS OSMC PUBLIC LICENSE (OSMC-PL). 
 * ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS PROGRAM CONSTITUTES RECIPIENT'S  
 * ACCEPTANCE OF THE OSMC PUBLIC LICENSE.
 *
 * The OpenModelica software and the Open Source Modelica
 * Consortium (OSMC) Public License (OSMC-PL) are obtained
 * from Linköping University, either from the above address,
 * from the URLs: http://www.ida.liu.se/projects/OpenModelica or  
 * http://www.openmodelica.org, and in the OpenModelica distribution. 
 * GNU version 3 is obtained from: http://www.gnu.org/copyleft/gpl.html.
 *
 * This program is distributed WITHOUT ANY WARRANTY; without
 * even the implied warranty of  MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE, EXCEPT AS EXPRESSLY SET FORTH
 * IN THE BY RECIPIENT SELECTED SUBSIDIARY LICENSE CONDITIONS
 * OF OSMC-PL.
 *
 * See the full OSMC Public License conditions for more details.
 *
 */

/*! \file simulation_runtime.h
 *
 *  This file is a C++ header file for the simulation runtime. It contains
 *  solver functions and other simulation runtime specific functions
 */

#ifndef _SIMULATION_RUNTIME_H
#define _SIMULATION_RUNTIME_H

#include "openmodelica.h"

#include "simulation_data.h"

#include "rtclock.h"
#include <stdlib.h>
#include "simulation_inline_solver.h"

#ifdef __cplusplus

#include "linearize.h"
#include "simulation_result.h"

#ifndef NO_INTERACTIVE_DEPENDENCY
#include "../../../interactive/socket.h"
extern Socket sim_communication_port;
#endif

extern "C" {

extern simulation_result *sim_result;

#endif /* cplusplus */

/* C-Interface for sim_result->emit(); */
#ifdef __cplusplus
extern "C" {
#endif /* cplusplus */
void sim_result_emit();
void sim_result_writeParameterData();
#ifdef __cplusplus
}
#endif /* cplusplus */

extern int measure_time_flag;

extern const char *linear_model_frame; /* printf format-string with holes for 6 strings */

extern int modelTermination; /* Becomes non-zero when simulation terminates. */
extern int terminationTerminate; /* Becomes non-zero when user terminates simulation. */
extern int terminationAssert; /* Becomes non-zero when model call assert simulation. */
extern int warningLevelAssert; /* Becomes non-zero when model call assert with warning level. */
extern FILE_INFO TermInfo; /* message for termination. */

extern char* TermMsg; /* message for termination. */

void setTermMsg(const char *msg);

/* defined in model code. Used to get name of variable by investigating its pointer in the state or alg vectors. */
const char* getNameReal(double* ptr);
const char* getNameInt(modelica_integer* ptr);
const char* getNameBool(modelica_boolean* ptr);
const char* getNameString(const char** ptr);


/* function for calculating state values on residual form */
/*used in DDASRT fortran function*/
int functionODE_residual(double *t, double *x, double *xprime, double *delta, fortran_integer *ires, double *rpar, fortran_integer* ipar);

/* function for calculating zeroCrossings */
/*used in DDASRT fortran function*/
int function_ZeroCrossingsDASSL(fortran_integer *neqm, double *t, double *y,
        fortran_integer *ng, double *gout, double *rpar, fortran_integer* ipar);

/* the main function of the simulation runtime!
 * simulation runtime no longer has main, is defined by the generated model code which calls this function.
 */
extern int _main_SimulationRuntime(int argc, char**argv, DATA *data);
void communicateStatus(const char *phase, double completionPercent);

#ifdef __cplusplus
}
#endif

#endif
