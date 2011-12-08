/*
 * This file is part of OpenModelica.
 *
 * Copyright (c) 1998-CurrentYear, Link�ping University,
 * Department of Computer and Information Science,
 * SE-58183 Link�ping, Sweden.
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
 * from Link�ping University, either from the above address,
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

/* File: modelica.h
 * Description: This is the C header file for the C code generated from
 * Modelica. It includes e.g. the C object representation of the builtin types
 * and arrays, etc.
 */

#ifndef OPENMODELICAFUNC_H_
#define OPENMODELICAFUNC_H_

#include "simulation_data.h"

#include "memory_pool.h"
#include "index_spec.h"
#include "boolean_array.h"
#include "integer_array.h"
#include "real_array.h"
#include "string_array.h"
#include "modelica_string.h"
#include "matrix.h"
#include "division.h"
#include "utility.h"

#include "model_help.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * this is used for initialize the DATA structure that is used in
 * all the generated functions.
 * The parameter controls what vectors should be initilized in
 * in the structure. Usually you can use the "ALL" flag which
 * initilizes all the vectors. This is needed for example in those ocasions
 * when another process have allocated the needed vectors.
 * Make sure that you call this function first because it sets the non-initialize
 * pointer to 0.
 *
 * This flag should be the same for second argument in callExternalObjectDestructors
 * to avoid memory leak.
 */
/* _X_DATA* initializeDataStruc(); */ /*create in model code */
void initializeDataStruc_X_2(_X_DATA *data);

/* Function for calling external object constructors */
void
callExternalObjectConstructors(_X_DATA *data);
/* Function for calling external object deconstructors */
void
callExternalObjectDestructors(_X_DATA *_data);

/* function for calculating ouput values */
/*used in DDASRT fortran function*/
int functionODE(_X_DATA *data);          /* functionODE with respect to start-values */
int functionAlgebraics(_X_DATA *data);   /* functionAlgebraics with respect to start-values */
int functionAliasEquations(_X_DATA *data);


/*   function for calculating all equation sorting order
  uses in EventHandle  */
int
functionDAE(_X_DATA *data, int *needToIterate);


/* functions for input and output */
int input_function(_X_DATA*);
int output_function(_X_DATA*);

/* function for storing value histories of delayed expressions
 * called from functionDAE_output()
 */
int
function_storeDelayed(_X_DATA *data);

/* function for calculating states on explicit ODE form */
/*used in functionDAE_res function*/
int functionODE_inline(_X_DATA *data, double stepsize);

/* function for calculate initial values from initial equations and fixed start attibutes */
int initial_function(_X_DATA *data);

/* function for calculate residual values for the initial equations and fixed start attibutes */
int initial_residual(_X_DATA *data, double lambda, double* initialResiduals);

/* function for calculating bound parameters that depend on other parameters, e.g. parameter Real n=1/m; */
int bound_parameters(_X_DATA *data);

/* function for checking for asserts and terminate */
int checkForAsserts(_X_DATA *data);

/* functions for event handling */
int function_onlyZeroCrossings(_X_DATA *data, double* gout, double* t);
int function_updateSample(_X_DATA *data);
int checkForDiscreteChanges(_X_DATA *data);

/* function for initializing time instants when sample() is activated */
void function_sampleInit(_X_DATA *data);
void function_initMemoryState();

/* function for calculation Jacobian */
int functionJacA(_X_DATA* data, double* jac);
int functionJacB(_X_DATA* data, double* jac);
int functionJacC(_X_DATA* data, double* jac);
int functionJacD(_X_DATA* data, double* jac);

extern const char *linear_model_frame; /* printf format-string with holes for 6 strings */

#ifdef __cplusplus
}
#endif

#endif
