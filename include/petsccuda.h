#if !defined(PETSCCUDA_H)
#define PETSCCUDA_H

#include <petscvec.h>
#include <cublas_v2.h>

PETSC_EXTERN PetscErrorCode VecCUDAGetArray(Vec v, PetscScalar **a);
PETSC_EXTERN PetscErrorCode VecCUDARestoreArray(Vec v, PetscScalar **a);

PETSC_EXTERN PetscErrorCode VecCUDAGetArrayRead(Vec v, const PetscScalar **a);
PETSC_EXTERN PetscErrorCode VecCUDARestoreArrayRead(Vec v, const PetscScalar **a);

PETSC_EXTERN PetscErrorCode VecCUDAGetArrayWrite(Vec v, PetscScalar **a);
PETSC_EXTERN PetscErrorCode VecCUDARestoreArrayWrite(Vec v, PetscScalar **a);

PETSC_EXTERN PetscErrorCode VecCUDAPlaceArray(Vec, PetscScalar *);
PETSC_EXTERN PetscErrorCode VecCUDAReplaceArray(Vec, PetscScalar *);
PETSC_EXTERN PetscErrorCode VecCUDAResetArray(Vec);
PETSC_EXTERN PetscErrorCode PetscCUBLASGetHandle(cublasHandle_t *handle);

#endif
