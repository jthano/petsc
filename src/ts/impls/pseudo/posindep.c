/*$Id: posindep.c,v 1.52 2001/03/23 23:24:42 balay Exp balay $*/
/*
       Code for Timestepping with implicit backwards Euler.
*/
#include "src/ts/tsimpl.h"                /*I   "petscts.h"   I*/

typedef struct {
  Vec  update;      /* work vector where new solution is formed */
  Vec  func;        /* work vector where F(t[i],u[i]) is stored */
  Vec  rhs;         /* work vector for RHS; vec_sol/dt */

  /* information used for Pseudo-timestepping */

  int    (*dt)(TS,double*,void*);              /* compute next timestep, and related context */
  void   *dtctx;              
  int    (*verify)(TS,Vec,void*,double*,int*); /* verify previous timestep and related context */
  void   *verifyctx;     

  double initial_fnorm,fnorm;                  /* original and current norm of F(u) */
  double fnorm_previous;

  double     dt_increment;                  /* scaling that dt is incremented each time-step */
  PetscTruth increment_dt_from_initial_dt;  
} TS_Pseudo;

/* ------------------------------------------------------------------------------*/

#undef __FUNCT__  
#define __FUNCT__ "TSPseudoComputeTimeStep"
/*@
    TSPseudoComputeTimeStep - Computes the next timestep for a currently running
    pseudo-timestepping process.

    Collective on TS

    Input Parameter:
.   ts - timestep context

    Output Parameter:
.   dt - newly computed timestep

    Level: advanced

    Notes:
    The routine to be called here to compute the timestep should be
    set by calling TSPseudoSetTimeStep().

.keywords: timestep, pseudo, compute

.seealso: TSPseudoDefaultTimeStep(), TSPseudoSetTimeStep()
@*/
int TSPseudoComputeTimeStep(TS ts,double *dt)
{
  TS_Pseudo *pseudo = (TS_Pseudo*)ts->data;
  int       ierr;

  PetscFunctionBegin;
  ierr = PetscLogEventBegin(TS_PseudoComputeTimeStep,ts,0,0,0);CHKERRQ(ierr);
  ierr = (*pseudo->dt)(ts,dt,pseudo->dtctx);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(TS_PseudoComputeTimeStep,ts,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


/* ------------------------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "TSPseudoDefaultVerifyTimeStep"
/*@C
   TSPseudoDefaultVerifyTimeStep - Default code to verify the quality of the last timestep.

   Collective on TS

   Input Parameters:
+  ts - the timestep context
.  dtctx - unused timestep context
-  update - latest solution vector

   Output Parameters:
+  newdt - the timestep to use for the next step
-  flag - flag indicating whether the last time step was acceptable

   Level: advanced

   Note:
   This routine always returns a flag of 1, indicating an acceptable 
   timestep.

.keywords: timestep, pseudo, default, verify 

.seealso: TSPseudoSetVerifyTimeStep(), TSPseudoVerifyTimeStep()
@*/
int TSPseudoDefaultVerifyTimeStep(TS ts,Vec update,void *dtctx,double *newdt,int *flag)
{
  PetscFunctionBegin;
  *flag = 1;
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "TSPseudoVerifyTimeStep"
/*@
    TSPseudoVerifyTimeStep - Verifies whether the last timestep was acceptable.

    Collective on TS

    Input Parameters:
+   ts - timestep context
-   update - latest solution vector

    Output Parameters:
+   dt - newly computed timestep (if it had to shrink)
-   flag - indicates if current timestep was ok

    Level: advanced

    Notes:
    The routine to be called here to compute the timestep should be
    set by calling TSPseudoSetVerifyTimeStep().

.keywords: timestep, pseudo, verify 

.seealso: TSPseudoSetVerifyTimeStep(), TSPseudoDefaultVerifyTimeStep()
@*/
int TSPseudoVerifyTimeStep(TS ts,Vec update,double *dt,int *flag)
{
  TS_Pseudo *pseudo = (TS_Pseudo*)ts->data;
  int       ierr;

  PetscFunctionBegin;
  if (!pseudo->verify) {*flag = 1; PetscFunctionReturn(0);}

  ierr = (*pseudo->verify)(ts,update,pseudo->verifyctx,dt,flag);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

/* --------------------------------------------------------------------------------*/

#undef __FUNCT__  
#define __FUNCT__ "TSStep_Pseudo"
static int TSStep_Pseudo(TS ts,int *steps,double *time)
{
  Vec       sol = ts->vec_sol;
  int       ierr,i,max_steps = ts->max_steps,its,ok,lits;
  TS_Pseudo *pseudo = (TS_Pseudo*)ts->data;
  double    current_time_step;
  
  PetscFunctionBegin;
  *steps = -ts->steps;

  ierr = VecCopy(sol,pseudo->update);CHKERRQ(ierr);
  for (i=0; i<max_steps && ts->ptime < ts->max_time; i++) {
    ierr = TSPseudoComputeTimeStep(ts,&ts->time_step);CHKERRQ(ierr);
    current_time_step = ts->time_step;
    while (1) {
      ts->ptime  += current_time_step;
      ierr = SNESSolve(ts->snes,pseudo->update,&its);CHKERRQ(ierr);
      ierr = SNESGetNumberLinearIterations(ts->snes,&lits);CHKERRQ(ierr);
      ts->nonlinear_its += PetscAbsInt(its); ts->linear_its += lits;
      ierr = TSPseudoVerifyTimeStep(ts,pseudo->update,&ts->time_step,&ok);CHKERRQ(ierr);
      if (ok) break;
      ts->ptime        -= current_time_step;
      current_time_step = ts->time_step;
    }
    ierr = VecCopy(pseudo->update,sol);CHKERRQ(ierr);
    ts->steps++;
    ierr = TSMonitor(ts,ts->steps,ts->ptime,sol);CHKERRQ(ierr);
  }

  *steps += ts->steps;
  *time  = ts->ptime;
  PetscFunctionReturn(0);
}

/*------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "TSDestroy_Pseudo"
static int TSDestroy_Pseudo(TS ts)
{
  TS_Pseudo *pseudo = (TS_Pseudo*)ts->data;
  int       ierr;

  PetscFunctionBegin;
  if (pseudo->update) {ierr = VecDestroy(pseudo->update);CHKERRQ(ierr);}
  if (pseudo->func) {ierr = VecDestroy(pseudo->func);CHKERRQ(ierr);}
  if (pseudo->rhs)  {ierr = VecDestroy(pseudo->rhs);CHKERRQ(ierr);}
  if (ts->Ashell)   {ierr = MatDestroy(ts->A);CHKERRQ(ierr);}
  ierr = PetscFree(pseudo);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


/*------------------------------------------------------------*/
/*
    This matrix shell multiply where user provided Shell matrix
*/

#undef __FUNCT__  
#define __FUNCT__ "TSPseudoMatMult"
int TSPseudoMatMult(Mat mat,Vec x,Vec y)
{
  TS     ts;
  Scalar mdt,mone = -1.0;
  int    ierr;

  PetscFunctionBegin;
  ierr = MatShellGetContext(mat,(void **)&ts);CHKERRQ(ierr);
  mdt = 1.0/ts->time_step;

  /* apply user provided function */
  ierr = MatMult(ts->Ashell,x,y);CHKERRQ(ierr);
  /* shift and scale by 1/dt - F */
  ierr = VecAXPBY(&mdt,&mone,x,y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* 
    This defines the nonlinear equation that is to be solved with SNES

              (U^{n+1} - U^{n})/dt - F(U^{n+1})
*/
#undef __FUNCT__  
#define __FUNCT__ "TSPseudoFunction"
int TSPseudoFunction(SNES snes,Vec x,Vec y,void *ctx)
{
  TS     ts = (TS) ctx;
  Scalar mdt = 1.0/ts->time_step,*unp1,*un,*Funp1;
  int    ierr,i,n;

  PetscFunctionBegin;
  /* apply user provided function */
  ierr = TSComputeRHSFunction(ts,ts->ptime,x,y);CHKERRQ(ierr);
  /* compute (u^{n+1) - u^{n})/dt - F(u^{n+1}) */
  ierr = VecGetArray(ts->vec_sol,&un);CHKERRQ(ierr);
  ierr = VecGetArray(x,&unp1);CHKERRQ(ierr);
  ierr = VecGetArray(y,&Funp1);CHKERRQ(ierr);
  ierr = VecGetLocalSize(x,&n);CHKERRQ(ierr);
  for (i=0; i<n; i++) {
    Funp1[i] = mdt*(unp1[i] - un[i]) - Funp1[i];
  }
  ierr = VecRestoreArray(ts->vec_sol,&un);CHKERRQ(ierr);
  ierr = VecRestoreArray(x,&unp1);CHKERRQ(ierr);
  ierr = VecRestoreArray(y,&Funp1);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

/*
   This constructs the Jacobian needed for SNES 

             J = I/dt - J_{F}   where J_{F} is the given Jacobian of F.
*/
#undef __FUNCT__  
#define __FUNCT__ "TSPseudoJacobian"
int TSPseudoJacobian(SNES snes,Vec x,Mat *AA,Mat *BB,MatStructure *str,void *ctx)
{
  TS         ts = (TS) ctx;
  int        ierr;
  Scalar     mone = -1.0,mdt = 1.0/ts->time_step;
  PetscTruth isshell;

  PetscFunctionBegin;
  /* construct users Jacobian */
  ierr = TSComputeRHSJacobian(ts,ts->ptime,x,AA,BB,str);CHKERRQ(ierr);

  /* shift and scale Jacobian, if not a shell matrix */
  ierr = PetscTypeCompare((PetscObject)*AA,MATSHELL,&isshell);CHKERRQ(ierr);
  if (!isshell) {
    ierr = MatScale(&mone,*AA);CHKERRQ(ierr);
    ierr = MatShift(&mdt,*AA);CHKERRQ(ierr);
  }
  ierr = PetscTypeCompare((PetscObject)*BB,MATSHELL,&isshell);CHKERRQ(ierr);
  if (*BB != *AA && *str != SAME_PRECONDITIONER && !isshell) {
    ierr = MatScale(&mone,*BB);CHKERRQ(ierr);
    ierr = MatShift(&mdt,*BB);CHKERRQ(ierr);
  }

  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "TSSetUp_Pseudo"
static int TSSetUp_Pseudo(TS ts)
{
  TS_Pseudo *pseudo = (TS_Pseudo*)ts->data;
  int       ierr,M,m;

  PetscFunctionBegin;
  ierr = SNESSetFromOptions(ts->snes);CHKERRQ(ierr);
  ierr = VecDuplicate(ts->vec_sol,&pseudo->update);CHKERRQ(ierr);  
  ierr = VecDuplicate(ts->vec_sol,&pseudo->func);CHKERRQ(ierr);  
  ierr = SNESSetFunction(ts->snes,pseudo->func,TSPseudoFunction,ts);CHKERRQ(ierr);
  if (ts->Ashell) { /* construct new shell matrix */
    ierr = VecGetSize(ts->vec_sol,&M);CHKERRQ(ierr);
    ierr = VecGetLocalSize(ts->vec_sol,&m);CHKERRQ(ierr);
    ierr = MatCreateShell(ts->comm,m,M,M,M,ts,&ts->A);CHKERRQ(ierr);
    ierr = MatShellSetOperation(ts->A,MATOP_MULT,(void(*)())TSPseudoMatMult);CHKERRQ(ierr);
  }
  ierr = SNESSetJacobian(ts->snes,ts->A,ts->B,TSPseudoJacobian,ts);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
/*------------------------------------------------------------*/

#undef __FUNCT__  
#define __FUNCT__ "TSPseudoDefaultMonitor"
int TSPseudoDefaultMonitor(TS ts,int step,double time,Vec v,void *ctx)
{
  TS_Pseudo *pseudo = (TS_Pseudo*)ts->data;
  int       ierr;

  PetscFunctionBegin;
  ierr = (*PetscHelpPrintf)(ts->comm,"TS %d dt %g time %g fnorm %g\n",step,ts->time_step,time,pseudo->fnorm);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSSetFromOptions_Pseudo"
static int TSSetFromOptions_Pseudo(TS ts)
{
  TS_Pseudo *pseudo = (TS_Pseudo*)ts->data;
  int        ierr;
  PetscTruth flg;

  PetscFunctionBegin;

  ierr = PetscOptionsHead("Pseudo-timestepping options");CHKERRQ(ierr);
    ierr = PetscOptionsName("-ts_monitor","Monitor convergence","TSPseudoDefaultMonitor",&flg);CHKERRQ(ierr);
    if (flg) {
      ierr = TSSetMonitor(ts,TSPseudoDefaultMonitor,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
    }
    ierr = PetscOptionsName("-ts_pseudo_increment_dt_from_initial_dt","Increase dt as a ratio from original dt","TSPseudoIncrementDtFromInitialDt",&flg);CHKERRQ(ierr);
    if (flg) {
      ierr = TSPseudoIncrementDtFromInitialDt(ts);CHKERRQ(ierr);
    }
    ierr = PetscOptionsDouble("-ts_pseudo_increment","Ratio to increase dt","TSPseudoSetTimeStepIncrement",pseudo->dt_increment,&pseudo->dt_increment,0);CHKERRQ(ierr);
  ierr = PetscOptionsTail();CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSView_Pseudo"
static int TSView_Pseudo(TS ts,PetscViewer viewer)
{
  PetscFunctionBegin;
  PetscFunctionReturn(0);
}

/* ----------------------------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "TSPseudoSetVerifyTimeStep"
/*@
   TSPseudoSetVerifyTimeStep - Sets a user-defined routine to verify the quality of the 
   last timestep.

   Collective on TS

   Input Parameters:
+  ts - timestep context
.  dt - user-defined function to verify timestep
-  ctx - [optional] user-defined context for private data
         for the timestep verification routine (may be PETSC_NULL)

   Level: advanced

   Calling sequence of func:
.  func (TS ts,Vec update,void *ctx,double *newdt,int *flag);

.  update - latest solution vector
.  ctx - [optional] timestep context
.  newdt - the timestep to use for the next step
.  flag - flag indicating whether the last time step was acceptable

   Notes:
   The routine set here will be called by TSPseudoVerifyTimeStep()
   during the timestepping process.

.keywords: timestep, pseudo, set, verify 

.seealso: TSPseudoDefaultVerifyTimeStep(), TSPseudoVerifyTimeStep()
@*/
int TSPseudoSetVerifyTimeStep(TS ts,int (*dt)(TS,Vec,void*,double*,int*),void* ctx)
{
  int ierr,(*f)(TS,int (*)(TS,Vec,void*,double *,int *),void *);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_COOKIE);

  ierr = PetscObjectQueryFunction((PetscObject)ts,"TSPseudoSetVerifyTimeStep_C",(void **)&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(ts,dt,ctx);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSPseudoSetTimeStepIncrement"
/*@
    TSPseudoSetTimeStepIncrement - Sets the scaling increment applied to 
    dt when using the TSPseudoDefaultTimeStep() routine.

   Collective on TS

    Input Parameters:
+   ts - the timestep context
-   inc - the scaling factor >= 1.0

    Options Database Key:
$    -ts_pseudo_increment <increment>

    Level: advanced

.keywords: timestep, pseudo, set, increment

.seealso: TSPseudoSetTimeStep(), TSPseudoDefaultTimeStep()
@*/
int TSPseudoSetTimeStepIncrement(TS ts,double inc)
{
  int ierr,(*f)(TS,double);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_COOKIE);

  ierr = PetscObjectQueryFunction((PetscObject)ts,"TSPseudoSetTimeStepIncrement_C",(void **)&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(ts,inc);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "TSPseudoIncrementDtFromInitialDt"
/*@
    TSPseudoIncrementDtFromInitialDt - Indicates that a new timestep
    is computed via the formula
$         dt = initial_dt*initial_fnorm/current_fnorm 
      rather than the default update,
$         dt = current_dt*previous_fnorm/current_fnorm.

   Collective on TS

    Input Parameter:
.   ts - the timestep context

    Options Database Key:
$    -ts_pseudo_increment_dt_from_initial_dt

    Level: advanced

.keywords: timestep, pseudo, set, increment

.seealso: TSPseudoSetTimeStep(), TSPseudoDefaultTimeStep()
@*/
int TSPseudoIncrementDtFromInitialDt(TS ts)
{
  int ierr,(*f)(TS);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_COOKIE);

  ierr = PetscObjectQueryFunction((PetscObject)ts,"TSPseudoIncrementDtFromInitialDt_C",(void **)&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(ts);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "TSPseudoSetTimeStep"
/*@
   TSPseudoSetTimeStep - Sets the user-defined routine to be
   called at each pseudo-timestep to update the timestep.

   Collective on TS

   Input Parameters:
+  ts - timestep context
.  dt - function to compute timestep
-  ctx - [optional] user-defined context for private data
         required by the function (may be PETSC_NULL)

   Level: intermediate

   Calling sequence of func:
.  func (TS ts,double *newdt,void *ctx);

.  newdt - the newly computed timestep
.  ctx - [optional] timestep context

   Notes:
   The routine set here will be called by TSPseudoComputeTimeStep()
   during the timestepping process.

.keywords: timestep, pseudo, set

.seealso: TSPseudoDefaultTimeStep(), TSPseudoComputeTimeStep()
@*/
int TSPseudoSetTimeStep(TS ts,int (*dt)(TS,double*,void*),void* ctx)
{
  int ierr,(*f)(TS,int (*)(TS,double *,void *),void *);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts,TS_COOKIE);

  ierr = PetscObjectQueryFunction((PetscObject)ts,"TSPseudoSetTimeStep_C",(void **)&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(ts,dt,ctx);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

/* ----------------------------------------------------------------------------- */

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "TSPseudoSetVerifyTimeStep_Pseudo"
int TSPseudoSetVerifyTimeStep_Pseudo(TS ts,int (*dt)(TS,Vec,void*,double*,int*),void* ctx)
{
  TS_Pseudo *pseudo;

  PetscFunctionBegin;
  pseudo              = (TS_Pseudo*)ts->data;
  pseudo->verify      = dt;
  pseudo->verifyctx   = ctx;
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "TSPseudoSetTimeStepIncrement_Pseudo"
int TSPseudoSetTimeStepIncrement_Pseudo(TS ts,double inc)
{
  TS_Pseudo *pseudo = (TS_Pseudo*)ts->data;

  PetscFunctionBegin;
  pseudo->dt_increment = inc;
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "TSPseudoIncrementDtFromInitialDt_Pseudo"
int TSPseudoIncrementDtFromInitialDt_Pseudo(TS ts)
{
  TS_Pseudo *pseudo = (TS_Pseudo*)ts->data;

  PetscFunctionBegin;
  pseudo->increment_dt_from_initial_dt = PETSC_TRUE;
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "TSPseudoSetTimeStep_Pseudo"
int TSPseudoSetTimeStep_Pseudo(TS ts,int (*dt)(TS,double*,void*),void* ctx)
{
  TS_Pseudo *pseudo = (TS_Pseudo*)ts->data;

  PetscFunctionBegin;
  pseudo->dt      = dt;
  pseudo->dtctx   = ctx;
  PetscFunctionReturn(0);
}
EXTERN_C_END

/* ----------------------------------------------------------------------------- */

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "TSCreate_Pseudo"
int TSCreate_Pseudo(TS ts)
{
  TS_Pseudo  *pseudo;
  int        ierr;
  PetscTruth isshell;

  PetscFunctionBegin;
  ts->destroy         = TSDestroy_Pseudo;
  ts->view            = TSView_Pseudo;

  if (ts->problem_type == TS_LINEAR) {
    SETERRQ(PETSC_ERR_ARG_WRONG,"Only for nonlinear problems");
  }
  if (!ts->A) {
    SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Must set Jacobian");
  }

  ierr = PetscTypeCompare((PetscObject)ts->A,MATSHELL,&isshell);CHKERRQ(ierr);
  if (isshell) {
    ts->Ashell = ts->A;
  }
  ts->setup           = TSSetUp_Pseudo;  
  ts->step            = TSStep_Pseudo;
  ts->setfromoptions  = TSSetFromOptions_Pseudo;

  /* create the required nonlinear solver context */
  ierr = SNESCreate(ts->comm,SNES_NONLINEAR_EQUATIONS,&ts->snes);CHKERRQ(ierr);

  ierr = PetscNew(TS_Pseudo,&pseudo);CHKERRQ(ierr);
  PetscLogObjectMemory(ts,sizeof(TS_Pseudo));

  ierr     = PetscMemzero(pseudo,sizeof(TS_Pseudo));CHKERRQ(ierr);
  ts->data = (void*)pseudo;

  pseudo->dt_increment                 = 1.1;
  pseudo->increment_dt_from_initial_dt = PETSC_FALSE;
  pseudo->dt                           = TSPseudoDefaultTimeStep;

  ierr = PetscObjectComposeFunctionDynamic((PetscObject)ts,"TSPseudoSetVerifyTimeStep_C",
                    "TSPseudoSetVerifyTimeStep_Pseudo",
                     TSPseudoSetVerifyTimeStep_Pseudo);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)ts,"TSPseudoSetTimeStepIncrement_C",
                    "TSPseudoSetTimeStepIncrement_Pseudo",
                     TSPseudoSetTimeStepIncrement_Pseudo);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)ts,"TSPseudoIncrementDtFromInitialDt_C",
                    "TSPseudoIncrementDtFromInitialDt_Pseudo",
                     TSPseudoIncrementDtFromInitialDt_Pseudo);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)ts,"TSPseudoSetTimeStep_C",
                    "TSPseudoSetTimeStep_Pseudo",
                     TSPseudoSetTimeStep_Pseudo);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "TSPseudoDefaultTimeStep"
/*@C
   TSPseudoDefaultTimeStep - Default code to compute pseudo-timestepping.
   Use with TSPseudoSetTimeStep().

   Collective on TS

   Input Parameters:
.  ts - the timestep context
.  dtctx - unused timestep context

   Output Parameter:
.  newdt - the timestep to use for the next step

   Level: advanced

.keywords: timestep, pseudo, default

.seealso: TSPseudoSetTimeStep(), TSPseudoComputeTimeStep()
@*/
int TSPseudoDefaultTimeStep(TS ts,double* newdt,void* dtctx)
{
  TS_Pseudo *pseudo = (TS_Pseudo*)ts->data;
  double    inc = pseudo->dt_increment,fnorm_previous = pseudo->fnorm_previous;
  int       ierr;

  PetscFunctionBegin;
  ierr = TSComputeRHSFunction(ts,ts->ptime,ts->vec_sol,pseudo->func);CHKERRQ(ierr);  
  ierr = VecNorm(pseudo->func,NORM_2,&pseudo->fnorm);CHKERRQ(ierr); 
  if (pseudo->initial_fnorm == 0.0) {
    /* first time through so compute initial function norm */
    pseudo->initial_fnorm = pseudo->fnorm;
    fnorm_previous        = pseudo->fnorm;
  }
  if (pseudo->fnorm == 0.0) {
    *newdt = 1.e12*inc*ts->time_step; 
  } else if (pseudo->increment_dt_from_initial_dt) {
    *newdt = inc*ts->initial_time_step*pseudo->initial_fnorm/pseudo->fnorm;
  } else {
    *newdt = inc*ts->time_step*fnorm_previous/pseudo->fnorm;
  }
  pseudo->fnorm_previous = pseudo->fnorm;
  PetscFunctionReturn(0);
}

















