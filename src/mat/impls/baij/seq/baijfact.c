#define PETSCMAT_DLL

/*
    Factorization code for BAIJ format. 
*/
#include "src/mat/impls/baij/seq/baij.h"
#include "src/inline/ilu.h"

/* ------------------------------------------------------------*/
/*
      Version for when blocks are 2 by 2
*/
#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorNumeric_SeqBAIJ_2"
PetscErrorCode MatLUFactorNumeric_SeqBAIJ_2(Mat B,Mat A,MatFactorInfo *info)
{
  Mat            C = B;
  Mat_SeqBAIJ    *a = (Mat_SeqBAIJ*)A->data,*b = (Mat_SeqBAIJ *)C->data;
  IS             isrow = b->row,isicol = b->icol;
  PetscErrorCode ierr;
  const PetscInt *r,*ic;
  PetscInt       i,j,n = a->mbs,*bi = b->i,*bj = b->j;
  PetscInt       *ajtmpold,*ajtmp,nz,row;
  PetscInt       *diag_offset=b->diag,idx,*ai=a->i,*aj=a->j,*pj;
  MatScalar      *pv,*v,*rtmp,m1,m2,m3,m4,*pc,*w,*x,x1,x2,x3,x4;
  MatScalar      p1,p2,p3,p4;
  MatScalar      *ba = b->a,*aa = a->a;
  PetscReal      shift = info->shiftinblocks;

  PetscFunctionBegin;
  ierr  = ISGetIndices(isrow,&r);CHKERRQ(ierr);
  ierr  = ISGetIndices(isicol,&ic);CHKERRQ(ierr);
  ierr  = PetscMalloc(4*(n+1)*sizeof(MatScalar),&rtmp);CHKERRQ(ierr);

  for (i=0; i<n; i++) {
    nz    = bi[i+1] - bi[i];
    ajtmp = bj + bi[i];
    for  (j=0; j<nz; j++) {
      x = rtmp+4*ajtmp[j]; x[0] = x[1] = x[2] = x[3] = 0.0;
    }
    /* load in initial (unfactored row) */
    idx      = r[i];
    nz       = ai[idx+1] - ai[idx];
    ajtmpold = aj + ai[idx];
    v        = aa + 4*ai[idx];
    for (j=0; j<nz; j++) {
      x    = rtmp+4*ic[ajtmpold[j]];
      x[0] = v[0]; x[1] = v[1]; x[2] = v[2]; x[3] = v[3];
      v    += 4;
    }
    row = *ajtmp++;
    while (row < i) {
      pc = rtmp + 4*row;
      p1 = pc[0]; p2 = pc[1]; p3 = pc[2]; p4 = pc[3];
      if (p1 != 0.0 || p2 != 0.0 || p3 != 0.0 || p4 != 0.0) { 
        pv = ba + 4*diag_offset[row];
        pj = bj + diag_offset[row] + 1;
        x1 = pv[0]; x2 = pv[1]; x3 = pv[2]; x4 = pv[3];
        pc[0] = m1 = p1*x1 + p3*x2;
        pc[1] = m2 = p2*x1 + p4*x2;
        pc[2] = m3 = p1*x3 + p3*x4;
        pc[3] = m4 = p2*x3 + p4*x4;
        nz = bi[row+1] - diag_offset[row] - 1;
        pv += 4;
        for (j=0; j<nz; j++) {
          x1   = pv[0]; x2 = pv[1]; x3 = pv[2]; x4 = pv[3];
          x    = rtmp + 4*pj[j];
          x[0] -= m1*x1 + m3*x2;
          x[1] -= m2*x1 + m4*x2;
          x[2] -= m1*x3 + m3*x4;
          x[3] -= m2*x3 + m4*x4;
          pv   += 4;
        }
        ierr = PetscLogFlops(16*nz+12);CHKERRQ(ierr);
      } 
      row = *ajtmp++;
    }
    /* finished row so stick it into b->a */
    pv = ba + 4*bi[i];
    pj = bj + bi[i];
    nz = bi[i+1] - bi[i];
    for (j=0; j<nz; j++) {
      x     = rtmp+4*pj[j];
      pv[0] = x[0]; pv[1] = x[1]; pv[2] = x[2]; pv[3] = x[3];
      pv   += 4;
    }
    /* invert diagonal block */
    w = ba + 4*diag_offset[i];
    ierr = Kernel_A_gets_inverse_A_2(w,shift);CHKERRQ(ierr);
  }

  ierr = PetscFree(rtmp);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isicol,&ic);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isrow,&r);CHKERRQ(ierr);
  C->ops->solve          = MatSolve_SeqBAIJ_2;
  C->ops->solvetranspose = MatSolveTranspose_SeqBAIJ_2;
  C->assembled = PETSC_TRUE;
  ierr = PetscLogFlops(1.3333*8*b->mbs);CHKERRQ(ierr); /* from inverting diagonal blocks */
  PetscFunctionReturn(0);
}
/*
      Version for when blocks are 2 by 2 Using natural ordering
*/
#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorNumeric_SeqBAIJ_2_NaturalOrdering"
PetscErrorCode MatLUFactorNumeric_SeqBAIJ_2_NaturalOrdering(Mat C,Mat A,MatFactorInfo *info)
{
  Mat_SeqBAIJ    *a = (Mat_SeqBAIJ*)A->data,*b = (Mat_SeqBAIJ *)C->data;
  PetscErrorCode ierr;
  PetscInt       i,j,n = a->mbs,*bi = b->i,*bj = b->j;
  PetscInt       *ajtmpold,*ajtmp,nz,row;
  PetscInt       *diag_offset = b->diag,*ai=a->i,*aj=a->j,*pj;
  MatScalar      *pv,*v,*rtmp,*pc,*w,*x;
  MatScalar      p1,p2,p3,p4,m1,m2,m3,m4,x1,x2,x3,x4;
  MatScalar      *ba = b->a,*aa = a->a;
  PetscReal      shift = info->shiftinblocks;

  PetscFunctionBegin;
  ierr = PetscMalloc(4*(n+1)*sizeof(MatScalar),&rtmp);CHKERRQ(ierr);

  for (i=0; i<n; i++) {
    nz    = bi[i+1] - bi[i];
    ajtmp = bj + bi[i];
    for  (j=0; j<nz; j++) {
      x = rtmp+4*ajtmp[j]; 
      x[0]  = x[1]  = x[2]  = x[3]  = 0.0;
    }
    /* load in initial (unfactored row) */
    nz       = ai[i+1] - ai[i];
    ajtmpold = aj + ai[i];
    v        = aa + 4*ai[i];
    for (j=0; j<nz; j++) {
      x    = rtmp+4*ajtmpold[j];
      x[0]  = v[0];  x[1]  = v[1];  x[2]  = v[2];  x[3]  = v[3];
      v    += 4;
    }
    row = *ajtmp++;
    while (row < i) {
      pc  = rtmp + 4*row;
      p1  = pc[0];  p2  = pc[1];  p3  = pc[2];  p4  = pc[3];
      if (p1 != 0.0 || p2 != 0.0 || p3 != 0.0 || p4 != 0.0) { 
        pv = ba + 4*diag_offset[row];
        pj = bj + diag_offset[row] + 1;
        x1  = pv[0];  x2  = pv[1];  x3  = pv[2];  x4  = pv[3];
        pc[0] = m1 = p1*x1 + p3*x2;
        pc[1] = m2 = p2*x1 + p4*x2;
        pc[2] = m3 = p1*x3 + p3*x4;
        pc[3] = m4 = p2*x3 + p4*x4;
        nz = bi[row+1] - diag_offset[row] - 1;
        pv += 4;
        for (j=0; j<nz; j++) {
          x1   = pv[0];  x2  = pv[1];   x3 = pv[2];  x4  = pv[3];
          x    = rtmp + 4*pj[j];
          x[0] -= m1*x1 + m3*x2;
          x[1] -= m2*x1 + m4*x2;
          x[2] -= m1*x3 + m3*x4;
          x[3] -= m2*x3 + m4*x4;
          pv   += 4;
        }
        ierr = PetscLogFlops(16*nz+12);CHKERRQ(ierr);
      } 
      row = *ajtmp++;
    }
    /* finished row so stick it into b->a */
    pv = ba + 4*bi[i];
    pj = bj + bi[i];
    nz = bi[i+1] - bi[i];
    for (j=0; j<nz; j++) {
      x      = rtmp+4*pj[j];
      pv[0]  = x[0];  pv[1]  = x[1];  pv[2]  = x[2];  pv[3]  = x[3];
      pv   += 4;
    }
    /* invert diagonal block */
    w = ba + 4*diag_offset[i];
    ierr = Kernel_A_gets_inverse_A_2(w,shift);CHKERRQ(ierr);
  }

  ierr = PetscFree(rtmp);CHKERRQ(ierr);
  C->ops->solve          = MatSolve_SeqBAIJ_2_NaturalOrdering;
  C->ops->solvetranspose = MatSolveTranspose_SeqBAIJ_2_NaturalOrdering;
  C->assembled = PETSC_TRUE;
  ierr = PetscLogFlops(1.3333*8*b->mbs);CHKERRQ(ierr); /* from inverting diagonal blocks */
  PetscFunctionReturn(0);
}

/* ----------------------------------------------------------- */
/*
     Version for when blocks are 1 by 1.
*/
#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorNumeric_SeqBAIJ_1"
PetscErrorCode MatLUFactorNumeric_SeqBAIJ_1(Mat C,Mat A,MatFactorInfo *info)
{
  Mat_SeqBAIJ    *a = (Mat_SeqBAIJ*)A->data,*b = (Mat_SeqBAIJ *)C->data;
  IS             isrow = b->row,isicol = b->icol;
  PetscErrorCode ierr;
  const PetscInt *r,*ic;
  PetscInt       i,j,n = a->mbs,*bi = b->i,*bj = b->j;
  PetscInt       *ajtmpold,*ajtmp,nz,row,*ai = a->i,*aj = a->j;
  PetscInt       *diag_offset = b->diag,diag,*pj;
  MatScalar      *pv,*v,*rtmp,multiplier,*pc;
  MatScalar      *ba = b->a,*aa = a->a;

  PetscFunctionBegin;
  ierr  = ISGetIndices(isrow,&r);CHKERRQ(ierr);
  ierr  = ISGetIndices(isicol,&ic);CHKERRQ(ierr);
  ierr  = PetscMalloc((n+1)*sizeof(MatScalar),&rtmp);CHKERRQ(ierr);

  for (i=0; i<n; i++) {
    nz    = bi[i+1] - bi[i];
    ajtmp = bj + bi[i];
    for  (j=0; j<nz; j++) rtmp[ajtmp[j]] = 0.0;

    /* load in initial (unfactored row) */
    nz       = ai[r[i]+1] - ai[r[i]];
    ajtmpold = aj + ai[r[i]];
    v        = aa + ai[r[i]];
    for (j=0; j<nz; j++) rtmp[ic[ajtmpold[j]]] =  v[j];

    row = *ajtmp++;
    while (row < i) {
      pc = rtmp + row;
      if (*pc != 0.0) {
        pv         = ba + diag_offset[row];
        pj         = bj + diag_offset[row] + 1;
        multiplier = *pc * *pv++;
        *pc        = multiplier;
        nz         = bi[row+1] - diag_offset[row] - 1;
        for (j=0; j<nz; j++) rtmp[pj[j]] -= multiplier * pv[j];
        ierr = PetscLogFlops(1+2*nz);CHKERRQ(ierr);
      }
      row = *ajtmp++;
    }
    /* finished row so stick it into b->a */
    pv = ba + bi[i];
    pj = bj + bi[i];
    nz = bi[i+1] - bi[i];
    for (j=0; j<nz; j++) {pv[j] = rtmp[pj[j]];}
    diag = diag_offset[i] - bi[i];
    /* check pivot entry for current row */
    if (pv[diag] == 0.0) {
      SETERRQ(PETSC_ERR_MAT_LU_ZRPVT,"Zero pivot");
    }
    pv[diag] = 1.0/pv[diag];
  }

  ierr = PetscFree(rtmp);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isicol,&ic);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isrow,&r);CHKERRQ(ierr);
  C->ops->solve          = MatSolve_SeqBAIJ_1;
  C->ops->solvetranspose = MatSolveTranspose_SeqBAIJ_1;
  C->assembled = PETSC_TRUE;
  ierr = PetscLogFlops(C->cmap->n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN  
#undef __FUNCT__  
#define __FUNCT__ "MatGetFactor_seqbaij_petsc"
PetscErrorCode MatGetFactor_seqbaij_petsc(Mat A,MatFactorType ftype,Mat *B)
{
  PetscInt           n = A->rmap->n;
  PetscErrorCode     ierr;

  PetscFunctionBegin;
  ierr = MatCreate(((PetscObject)A)->comm,B);CHKERRQ(ierr);
  ierr = MatSetSizes(*B,n,n,n,n);CHKERRQ(ierr);
  if (ftype == MAT_FACTOR_LU || ftype == MAT_FACTOR_ILU) {
    ierr = MatSetType(*B,MATSEQBAIJ);CHKERRQ(ierr);
    (*B)->ops->lufactorsymbolic  = MatLUFactorSymbolic_SeqBAIJ;  
    (*B)->ops->ilufactorsymbolic = MatILUFactorSymbolic_SeqBAIJ;  
  } else if (ftype == MAT_FACTOR_CHOLESKY || ftype == MAT_FACTOR_ICC) {
    ierr = MatSetType(*B,MATSEQSBAIJ);CHKERRQ(ierr);
    ierr = MatSeqSBAIJSetPreallocation(*B,1,MAT_SKIP_ALLOCATION,PETSC_NULL);CHKERRQ(ierr);
    (*B)->ops->iccfactorsymbolic      = MatICCFactorSymbolic_SeqBAIJ;
    (*B)->ops->choleskyfactorsymbolic = MatCholeskyFactorSymbolic_SeqBAIJ;
  } else SETERRQ(PETSC_ERR_SUP,"Factor type not supported");
  (*B)->factor = ftype;
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatGetFactorAvailable_seqaij_petsc"
PetscErrorCode MatGetFactorAvailable_seqbaij_petsc(Mat A,MatFactorType ftype,PetscTruth *flg)
{
  PetscFunctionBegin;
  *flg = PETSC_TRUE;
  PetscFunctionReturn(0);
}
EXTERN_C_END

/* ----------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "MatLUFactor_SeqBAIJ"
PetscErrorCode MatLUFactor_SeqBAIJ(Mat A,IS row,IS col,MatFactorInfo *info)
{
  PetscErrorCode ierr;
  Mat            C;

  PetscFunctionBegin;
  ierr = MatGetFactor(A,MAT_SOLVER_PETSC,MAT_FACTOR_LU,&C);CHKERRQ(ierr);
  ierr = MatLUFactorSymbolic(C,A,row,col,info);CHKERRQ(ierr);
  ierr = MatLUFactorNumeric(C,A,info);CHKERRQ(ierr);
  A->ops->solve            = C->ops->solve;
  A->ops->solvetranspose   = C->ops->solvetranspose;
  ierr = MatHeaderCopy(A,C);CHKERRQ(ierr);
  ierr = PetscLogObjectParent(A,((Mat_SeqBAIJ*)(A->data))->icol);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#include "src/mat/impls/sbaij/seq/sbaij.h"
#undef __FUNCT__  
#define __FUNCT__ "MatCholeskyFactorNumeric_SeqBAIJ_N"
PetscErrorCode MatCholeskyFactorNumeric_SeqBAIJ_N(Mat C,Mat A,MatFactorInfo *info)
{
  PetscErrorCode ierr;
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ*)A->data;
  Mat_SeqSBAIJ   *b=(Mat_SeqSBAIJ*)C->data;
  IS             ip=b->row;
  const PetscInt *rip;
  PetscInt       i,j,mbs=a->mbs,bs=A->rmap->bs,*bi=b->i,*bj=b->j,*bcol;
  PetscInt       *ai=a->i,*aj=a->j;
  PetscInt       k,jmin,jmax,*jl,*il,col,nexti,ili,nz;
  MatScalar      *rtmp,*ba=b->a,*bval,*aa=a->a,dk,uikdi;
  PetscReal      zeropivot,rs,shiftnz;
  PetscReal      shiftpd;
  ChShift_Ctx    sctx;
  PetscInt       newshift;

  PetscFunctionBegin;
  if (bs > 1) {
    if (!a->sbaijMat){
      ierr = MatConvert(A,MATSEQSBAIJ,MAT_INITIAL_MATRIX,&a->sbaijMat);CHKERRQ(ierr); 
    } 
    ierr = (a->sbaijMat)->ops->choleskyfactornumeric(C,a->sbaijMat,info);CHKERRQ(ierr);
    ierr = MatDestroy(a->sbaijMat);CHKERRQ(ierr);
    a->sbaijMat = PETSC_NULL; 
    PetscFunctionReturn(0); 
  }
  
  /* initialization */
  shiftnz   = info->shiftnz;
  shiftpd   = info->shiftpd;
  zeropivot = info->zeropivot;

  ierr  = ISGetIndices(ip,&rip);CHKERRQ(ierr);
  nz   = (2*mbs+1)*sizeof(PetscInt)+mbs*sizeof(MatScalar);
  ierr = PetscMalloc(nz,&il);CHKERRQ(ierr);
  jl   = il + mbs;
  rtmp = (MatScalar*)(jl + mbs);

  sctx.shift_amount = 0;
  sctx.nshift       = 0;
  do {
    sctx.chshift = PETSC_FALSE;
    for (i=0; i<mbs; i++) {
      rtmp[i] = 0.0; jl[i] = mbs; il[0] = 0;
    } 
 
    for (k = 0; k<mbs; k++){
      bval = ba + bi[k];
      /* initialize k-th row by the perm[k]-th row of A */
      jmin = ai[rip[k]]; jmax = ai[rip[k]+1];
      for (j = jmin; j < jmax; j++){
        col = rip[aj[j]];
        if (col >= k){ /* only take upper triangular entry */
          rtmp[col] = aa[j];
          *bval++  = 0.0; /* for in-place factorization */
        }
      } 
   
      /* shift the diagonal of the matrix */
      if (sctx.nshift) rtmp[k] += sctx.shift_amount;

      /* modify k-th row by adding in those rows i with U(i,k)!=0 */
      dk = rtmp[k];
      i = jl[k]; /* first row to be added to k_th row  */  

      while (i < k){
        nexti = jl[i]; /* next row to be added to k_th row */

        /* compute multiplier, update diag(k) and U(i,k) */
        ili = il[i];  /* index of first nonzero element in U(i,k:bms-1) */
        uikdi = - ba[ili]*ba[bi[i]];  /* diagonal(k) */ 
        dk += uikdi*ba[ili];
        ba[ili] = uikdi; /* -U(i,k) */

        /* add multiple of row i to k-th row */
        jmin = ili + 1; jmax = bi[i+1];
        if (jmin < jmax){
          for (j=jmin; j<jmax; j++) rtmp[bj[j]] += uikdi*ba[j];         
          /* update il and jl for row i */
          il[i] = jmin;             
          j = bj[jmin]; jl[i] = jl[j]; jl[j] = i; 
        }      
        i = nexti;         
      }

      /* shift the diagonals when zero pivot is detected */
      /* compute rs=sum of abs(off-diagonal) */
      rs   = 0.0;
      jmin = bi[k]+1; 
      nz   = bi[k+1] - jmin; 
      if (nz){
        bcol = bj + jmin;
        while (nz--){
          rs += PetscAbsScalar(rtmp[*bcol]);
          bcol++;
        }
      }

      sctx.rs = rs;
      sctx.pv = dk;
      ierr = MatCholeskyCheckShift_inline(info,sctx,k,newshift);CHKERRQ(ierr); 
      if (newshift == 1) break;    

      /* copy data into U(k,:) */
      ba[bi[k]] = 1.0/dk; /* U(k,k) */
      jmin = bi[k]+1; jmax = bi[k+1];
      if (jmin < jmax) {
        for (j=jmin; j<jmax; j++){
          col = bj[j]; ba[j] = rtmp[col]; rtmp[col] = 0.0;
        }       
        /* add the k-th row into il and jl */
        il[k] = jmin;
        i = bj[jmin]; jl[k] = jl[i]; jl[i] = k;
      }        
    } 
  } while (sctx.chshift);
  ierr = PetscFree(il);CHKERRQ(ierr);

  ierr = ISRestoreIndices(ip,&rip);CHKERRQ(ierr);
  C->assembled    = PETSC_TRUE; 
  C->preallocated = PETSC_TRUE;
  ierr = PetscLogFlops(C->rmap->N);CHKERRQ(ierr);
  if (sctx.nshift){
    if (shiftpd) {
      ierr = PetscInfo2(A,"number of shiftpd tries %D, shift_amount %G\n",sctx.nshift,sctx.shift_amount);CHKERRQ(ierr);
    } else if (shiftnz) {
      ierr = PetscInfo2(A,"number of shiftnz tries %D, shift_amount %G\n",sctx.nshift,sctx.shift_amount);CHKERRQ(ierr);
    } 
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatCholeskyFactorNumeric_SeqBAIJ_N_NaturalOrdering"
PetscErrorCode MatCholeskyFactorNumeric_SeqBAIJ_N_NaturalOrdering(Mat C,Mat A,MatFactorInfo *info)
{
  Mat_SeqBAIJ    *a=(Mat_SeqBAIJ*)A->data;
  Mat_SeqSBAIJ   *b=(Mat_SeqSBAIJ*)C->data;
  PetscErrorCode ierr;
  PetscInt       i,j,am=a->mbs; 
  PetscInt       *ai=a->i,*aj=a->j,*bi=b->i,*bj=b->j;
  PetscInt       k,jmin,*jl,*il,nexti,ili,*acol,*bcol,nz;
  MatScalar      *rtmp,*ba=b->a,*aa=a->a,dk,uikdi,*aval,*bval;
  PetscReal      zeropivot,rs,shiftnz;
  PetscReal      shiftpd;
  ChShift_Ctx    sctx;
  PetscInt       newshift;

  PetscFunctionBegin;
  /* initialization */
  shiftnz   = info->shiftnz;
  shiftpd   = info->shiftpd;
  zeropivot = info->zeropivot;

  nz   = (2*am+1)*sizeof(PetscInt)+am*sizeof(MatScalar);
  ierr = PetscMalloc(nz,&il);CHKERRQ(ierr);
  jl   = il + am;
  rtmp = (MatScalar*)(jl + am);

  sctx.shift_amount = 0;
  sctx.nshift       = 0;
  do {
    sctx.chshift = PETSC_FALSE;
    for (i=0; i<am; i++) {
      rtmp[i] = 0.0; jl[i] = am; il[0] = 0;
    }

    for (k = 0; k<am; k++){ 
    /* initialize k-th row with elements nonzero in row perm(k) of A */
      nz   = ai[k+1] - ai[k];
      acol = aj + ai[k];
      aval = aa + ai[k];
      bval = ba + bi[k];
      while (nz -- ){
        if (*acol < k) { /* skip lower triangular entries */
          acol++; aval++;
        } else {
          rtmp[*acol++] = *aval++;
          *bval++       = 0.0; /* for in-place factorization */
        }
      } 
     
      /* shift the diagonal of the matrix */
      if (sctx.nshift) rtmp[k] += sctx.shift_amount;
    
      /* modify k-th row by adding in those rows i with U(i,k)!=0 */
      dk = rtmp[k];
      i  = jl[k]; /* first row to be added to k_th row  */  

      while (i < k){
        nexti = jl[i]; /* next row to be added to k_th row */
        /* compute multiplier, update D(k) and U(i,k) */
        ili   = il[i];  /* index of first nonzero element in U(i,k:bms-1) */
        uikdi = - ba[ili]*ba[bi[i]];  
        dk   += uikdi*ba[ili];
        ba[ili] = uikdi; /* -U(i,k) */

        /* add multiple of row i to k-th row ... */
        jmin = ili + 1; 
        nz   = bi[i+1] - jmin;
        if (nz > 0){
          bcol = bj + jmin;
          bval = ba + jmin; 
          while (nz --) rtmp[*bcol++] += uikdi*(*bval++);
          /* update il and jl for i-th row */
          il[i] = jmin;            
          j = bj[jmin]; jl[i] = jl[j]; jl[j] = i; 
        }      
        i = nexti;         
      }

      /* shift the diagonals when zero pivot is detected */
      /* compute rs=sum of abs(off-diagonal) */
      rs   = 0.0;
      jmin = bi[k]+1; 
      nz   = bi[k+1] - jmin; 
      if (nz){
        bcol = bj + jmin;
        while (nz--){
          rs += PetscAbsScalar(rtmp[*bcol]);
          bcol++;
        }
      }

      sctx.rs = rs;
      sctx.pv = dk;
      ierr = MatCholeskyCheckShift_inline(info,sctx,k,newshift);CHKERRQ(ierr); 
      if (newshift == 1) break;    /* sctx.shift_amount is updated */

      /* copy data into U(k,:) */
      ba[bi[k]] = 1.0/dk;
      jmin      = bi[k]+1; 
      nz        = bi[k+1] - jmin; 
      if (nz){
        bcol = bj + jmin;
        bval = ba + jmin;
        while (nz--){
          *bval++       = rtmp[*bcol]; 
          rtmp[*bcol++] = 0.0; 
        }       
        /* add k-th row into il and jl */
        il[k] = jmin;
        i = bj[jmin]; jl[k] = jl[i]; jl[i] = k;
      }        
    } 
  } while (sctx.chshift);
  ierr = PetscFree(il);CHKERRQ(ierr);
  
  C->ops->solve                 = MatSolve_SeqSBAIJ_1_NaturalOrdering;  
  C->ops->solvetranspose        = MatSolve_SeqSBAIJ_1_NaturalOrdering;
  C->assembled    = PETSC_TRUE; 
  C->preallocated = PETSC_TRUE;
  ierr = PetscLogFlops(C->rmap->N);CHKERRQ(ierr);
    if (sctx.nshift){
    if (shiftnz) {
      ierr = PetscInfo2(A,"number of shiftnz tries %D, shift_amount %G\n",sctx.nshift,sctx.shift_amount);CHKERRQ(ierr);
    } else if (shiftpd) {
      ierr = PetscInfo2(A,"number of shiftpd tries %D, shift_amount %G\n",sctx.nshift,sctx.shift_amount);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

#include "petscbt.h"
#include "src/mat/utils/freespace.h"
#undef __FUNCT__  
#define __FUNCT__ "MatICCFactorSymbolic_SeqBAIJ"
PetscErrorCode MatICCFactorSymbolic_SeqBAIJ(Mat fact,Mat A,IS perm,MatFactorInfo *info)
{
  Mat_SeqBAIJ        *a = (Mat_SeqBAIJ*)A->data;
  Mat_SeqSBAIJ       *b;
  Mat                B;
  PetscErrorCode     ierr;
  PetscTruth         perm_identity;
  PetscInt           reallocs=0,i,*ai=a->i,*aj=a->j,am=a->mbs,bs=A->rmap->bs,*ui;
  const PetscInt     *rip;
  PetscInt           jmin,jmax,nzk,k,j,*jl,prow,*il,nextprow;
  PetscInt           nlnk,*lnk,*lnk_lvl=PETSC_NULL,ncols,ncols_upper,*cols,*cols_lvl,*uj,**uj_ptr,**uj_lvl_ptr;
  PetscReal          fill=info->fill,levels=info->levels;
  PetscFreeSpaceList free_space=PETSC_NULL,current_space=PETSC_NULL;
  PetscFreeSpaceList free_space_lvl=PETSC_NULL,current_space_lvl=PETSC_NULL;
  PetscBT            lnkbt;

  PetscFunctionBegin;
  if (bs > 1){
    if (!a->sbaijMat){
      ierr = MatConvert(A,MATSEQSBAIJ,MAT_INITIAL_MATRIX,&a->sbaijMat);CHKERRQ(ierr);
    }
    (fact)->ops->iccfactorsymbolic = MatICCFactorSymbolic_SeqSBAIJ;  /* undue the change made in MatGetFactor_seqbaij_petsc */
    ierr = MatICCFactorSymbolic(fact,a->sbaijMat,perm,info);CHKERRQ(ierr);
    PetscFunctionReturn(0); 
  }

  ierr = ISIdentity(perm,&perm_identity);CHKERRQ(ierr);
  ierr = ISGetIndices(perm,&rip);CHKERRQ(ierr);

  /* special case that simply copies fill pattern */
  if (!levels && perm_identity) { 
    ierr = MatMarkDiagonal_SeqBAIJ(A);CHKERRQ(ierr);
    ierr = PetscMalloc((am+1)*sizeof(PetscInt),&ui);CHKERRQ(ierr); 
    for (i=0; i<am; i++) {
      ui[i] = ai[i+1] - a->diag[i]; /* ui: rowlengths - changes when !perm_identity */
    }
    B = fact;
    ierr = MatSeqSBAIJSetPreallocation(B,1,0,ui);CHKERRQ(ierr);


    b  = (Mat_SeqSBAIJ*)B->data;
    uj = b->j;
    for (i=0; i<am; i++) {
      aj = a->j + a->diag[i];  
      for (j=0; j<ui[i]; j++){
        *uj++ = *aj++; 
      }
      b->ilen[i] = ui[i]; 
    }
    ierr = PetscFree(ui);CHKERRQ(ierr);
    B->factor = MAT_FACTOR_NONE;
    ierr = MatAssemblyBegin(B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr = MatAssemblyEnd(B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    B->factor = MAT_FACTOR_ICC;

    B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqBAIJ_N_NaturalOrdering;
    PetscFunctionReturn(0);
  }

  /* initialization */
  ierr  = PetscMalloc((am+1)*sizeof(PetscInt),&ui);CHKERRQ(ierr);
  ui[0] = 0; 
  ierr  = PetscMalloc((2*am+1)*sizeof(PetscInt),&cols_lvl);CHKERRQ(ierr); 

  /* jl: linked list for storing indices of the pivot rows 
     il: il[i] points to the 1st nonzero entry of U(i,k:am-1) */
  ierr = PetscMalloc((2*am+1)*sizeof(PetscInt)+2*am*sizeof(PetscInt*),&jl);CHKERRQ(ierr); 
  il         = jl + am;
  uj_ptr     = (PetscInt**)(il + am);
  uj_lvl_ptr = (PetscInt**)(uj_ptr + am);
  for (i=0; i<am; i++){
    jl[i] = am; il[i] = 0;
  }

  /* create and initialize a linked list for storing column indices of the active row k */
  nlnk = am + 1;
  ierr = PetscIncompleteLLCreate(am,am,nlnk,lnk,lnk_lvl,lnkbt);CHKERRQ(ierr);

  /* initial FreeSpace size is fill*(ai[am]+1) */
  ierr = PetscFreeSpaceGet((PetscInt)(fill*(ai[am]+1)),&free_space);CHKERRQ(ierr);
  current_space = free_space;
  ierr = PetscFreeSpaceGet((PetscInt)(fill*(ai[am]+1)),&free_space_lvl);CHKERRQ(ierr);
  current_space_lvl = free_space_lvl;

  for (k=0; k<am; k++){  /* for each active row k */
    /* initialize lnk by the column indices of row rip[k] of A */
    nzk   = 0;
    ncols = ai[rip[k]+1] - ai[rip[k]]; 
    ncols_upper = 0;
    cols        = cols_lvl + am;
    for (j=0; j<ncols; j++){
      i = rip[*(aj + ai[rip[k]] + j)];
      if (i >= k){ /* only take upper triangular entry */
        cols[ncols_upper] = i;
        cols_lvl[ncols_upper] = -1;  /* initialize level for nonzero entries */
        ncols_upper++;
      }
    }
    ierr = PetscIncompleteLLAdd(ncols_upper,cols,levels,cols_lvl,am,nlnk,lnk,lnk_lvl,lnkbt);CHKERRQ(ierr);
    nzk += nlnk;

    /* update lnk by computing fill-in for each pivot row to be merged in */
    prow = jl[k]; /* 1st pivot row */
   
    while (prow < k){
      nextprow = jl[prow];
      
      /* merge prow into k-th row */
      jmin = il[prow] + 1;  /* index of the 2nd nzero entry in U(prow,k:am-1) */
      jmax = ui[prow+1]; 
      ncols = jmax-jmin;
      i     = jmin - ui[prow];
      cols = uj_ptr[prow] + i; /* points to the 2nd nzero entry in U(prow,k:am-1) */
      for (j=0; j<ncols; j++) cols_lvl[j] = *(uj_lvl_ptr[prow] + i + j);
      ierr = PetscIncompleteLLAddSorted(ncols,cols,levels,cols_lvl,am,nlnk,lnk,lnk_lvl,lnkbt);CHKERRQ(ierr); 
      nzk += nlnk;

      /* update il and jl for prow */
      if (jmin < jmax){
        il[prow] = jmin;
        j = *cols; jl[prow] = jl[j]; jl[j] = prow;  
      } 
      prow = nextprow; 
    }  

    /* if free space is not available, make more free space */
    if (current_space->local_remaining<nzk) {
      i = am - k + 1; /* num of unfactored rows */
      i = PetscMin(i*nzk, i*(i-1)); /* i*nzk, i*(i-1): estimated and max additional space needed */
      ierr = PetscFreeSpaceGet(i,&current_space);CHKERRQ(ierr);
      ierr = PetscFreeSpaceGet(i,&current_space_lvl);CHKERRQ(ierr);
      reallocs++;
    }

    /* copy data into free_space and free_space_lvl, then initialize lnk */
    ierr = PetscIncompleteLLClean(am,am,nzk,lnk,lnk_lvl,current_space->array,current_space_lvl->array,lnkbt);CHKERRQ(ierr);

    /* add the k-th row into il and jl */
    if (nzk-1 > 0){
      i = current_space->array[1]; /* col value of the first nonzero element in U(k, k+1:am-1) */    
      jl[k] = jl[i]; jl[i] = k;
      il[k] = ui[k] + 1;
    } 
    uj_ptr[k]     = current_space->array;
    uj_lvl_ptr[k] = current_space_lvl->array; 

    current_space->array           += nzk;
    current_space->local_used      += nzk;
    current_space->local_remaining -= nzk;

    current_space_lvl->array           += nzk;
    current_space_lvl->local_used      += nzk;
    current_space_lvl->local_remaining -= nzk;

    ui[k+1] = ui[k] + nzk;  
  } 

#if defined(PETSC_USE_INFO)
  if (ai[am] != 0) {
    PetscReal af = ((PetscReal)(2*ui[am]-am))/((PetscReal)ai[am]);
    ierr = PetscInfo3(A,"Reallocs %D Fill ratio:given %G needed %G\n",reallocs,fill,af);CHKERRQ(ierr);
    ierr = PetscInfo1(A,"Run with -pc_factor_fill %G or use \n",af);CHKERRQ(ierr);
    ierr = PetscInfo1(A,"PCFactorSetFill(pc,%G) for best performance.\n",af);CHKERRQ(ierr);
  } else {
    ierr = PetscInfo(A,"Empty matrix.\n");CHKERRQ(ierr);
  }
#endif

  ierr = ISRestoreIndices(perm,&rip);CHKERRQ(ierr);
  ierr = PetscFree(jl);CHKERRQ(ierr);
  ierr = PetscFree(cols_lvl);CHKERRQ(ierr);

  /* destroy list of free space and other temporary array(s) */
  ierr = PetscMalloc((ui[am]+1)*sizeof(PetscInt),&uj);CHKERRQ(ierr);
  ierr = PetscFreeSpaceContiguous(&free_space,uj);CHKERRQ(ierr);
  ierr = PetscIncompleteLLDestroy(lnk,lnkbt);CHKERRQ(ierr);
  ierr = PetscFreeSpaceDestroy(free_space_lvl);CHKERRQ(ierr);

  /* put together the new matrix in MATSEQSBAIJ format */
  B = fact;
  ierr = MatSeqSBAIJSetPreallocation(B,1,MAT_SKIP_ALLOCATION,PETSC_NULL);CHKERRQ(ierr);

  b = (Mat_SeqSBAIJ*)B->data;
  b->singlemalloc = PETSC_FALSE;
  b->free_a       = PETSC_TRUE;
  b->free_ij       = PETSC_TRUE;
  ierr = PetscMalloc((ui[am]+1)*sizeof(MatScalar),&b->a);CHKERRQ(ierr);
  b->j    = uj;
  b->i    = ui;
  b->diag = 0;
  b->ilen = 0;
  b->imax = 0;
  b->row  = perm;
  b->pivotinblocks = PETSC_FALSE; /* need to get from MatFactorInfo */
  ierr    = PetscObjectReference((PetscObject)perm);CHKERRQ(ierr); 
  b->icol = perm;
  ierr    = PetscObjectReference((PetscObject)perm);CHKERRQ(ierr); 
  ierr    = PetscMalloc((am+1)*sizeof(PetscScalar),&b->solve_work);CHKERRQ(ierr);
  ierr    = PetscLogObjectMemory(B,(ui[am]-am)*(sizeof(PetscInt)+sizeof(MatScalar)));CHKERRQ(ierr);
  b->maxnz = b->nz = ui[am];
  
  B->info.factor_mallocs    = reallocs;
  B->info.fill_ratio_given  = fill;
  if (ai[am] != 0) {
    B->info.fill_ratio_needed = ((PetscReal)ui[am])/((PetscReal)ai[am]);
  } else {
    B->info.fill_ratio_needed = 0.0;
  }
  if (perm_identity){
    B->ops->solve           = MatSolve_SeqSBAIJ_1_NaturalOrdering;
    B->ops->solvetranspose  = MatSolve_SeqSBAIJ_1_NaturalOrdering;
    B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqBAIJ_N_NaturalOrdering;
  } else {
    (fact)->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqBAIJ_N;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatCholeskyFactorSymbolic_SeqBAIJ"
PetscErrorCode MatCholeskyFactorSymbolic_SeqBAIJ(Mat fact,Mat A,IS perm,MatFactorInfo *info)
{
  Mat_SeqBAIJ        *a = (Mat_SeqBAIJ*)A->data;
  Mat_SeqSBAIJ       *b;
  Mat                B;
  PetscErrorCode     ierr;
  PetscTruth         perm_identity;
  PetscReal          fill = info->fill;
  const PetscInt     *rip;
  PetscInt           i,mbs=a->mbs,bs=A->rmap->bs,*ai=a->i,*aj=a->j,reallocs=0,prow;
  PetscInt           *jl,jmin,jmax,nzk,*ui,k,j,*il,nextprow;
  PetscInt           nlnk,*lnk,ncols,ncols_upper,*cols,*uj,**ui_ptr,*uj_ptr;
  PetscFreeSpaceList free_space=PETSC_NULL,current_space=PETSC_NULL;
  PetscBT            lnkbt;

  PetscFunctionBegin;
  if (bs > 1) { /* convert to seqsbaij */
    if (!a->sbaijMat){
      ierr = MatConvert(A,MATSEQSBAIJ,MAT_INITIAL_MATRIX,&a->sbaijMat);CHKERRQ(ierr);
    }
    (fact)->ops->choleskyfactorsymbolic = MatCholeskyFactorSymbolic_SeqSBAIJ; /* undue the change made in MatGetFactor_seqbaij_petsc */
    ierr = MatCholeskyFactorSymbolic(fact,a->sbaijMat,perm,info);CHKERRQ(ierr); 
    PetscFunctionReturn(0); 
  }

  /* check whether perm is the identity mapping */
  ierr = ISIdentity(perm,&perm_identity);CHKERRQ(ierr);
  if (!perm_identity) SETERRQ(PETSC_ERR_SUP,"Matrix reordering is not supported");
  ierr = ISGetIndices(perm,&rip);CHKERRQ(ierr);

  /* initialization */
  ierr  = PetscMalloc((mbs+1)*sizeof(PetscInt),&ui);CHKERRQ(ierr);
  ui[0] = 0; 

  /* jl: linked list for storing indices of the pivot rows 
     il: il[i] points to the 1st nonzero entry of U(i,k:mbs-1) */
  ierr = PetscMalloc((3*mbs+1)*sizeof(PetscInt)+mbs*sizeof(PetscInt*),&jl);CHKERRQ(ierr); 
  il     = jl + mbs;
  cols   = il + mbs;
  ui_ptr = (PetscInt**)(cols + mbs);
  for (i=0; i<mbs; i++){
    jl[i] = mbs; il[i] = 0;
  }

  /* create and initialize a linked list for storing column indices of the active row k */
  nlnk = mbs + 1;
  ierr = PetscLLCreate(mbs,mbs,nlnk,lnk,lnkbt);CHKERRQ(ierr);

  /* initial FreeSpace size is fill*(ai[mbs]+1) */
  ierr = PetscFreeSpaceGet((PetscInt)(fill*(ai[mbs]+1)),&free_space);CHKERRQ(ierr);
  current_space = free_space;

  for (k=0; k<mbs; k++){  /* for each active row k */
    /* initialize lnk by the column indices of row rip[k] of A */
    nzk   = 0;
    ncols = ai[rip[k]+1] - ai[rip[k]]; 
    ncols_upper = 0;
    for (j=0; j<ncols; j++){
      i = rip[*(aj + ai[rip[k]] + j)];
      if (i >= k){ /* only take upper triangular entry */
        cols[ncols_upper] = i;
        ncols_upper++;
      }
    }
    ierr = PetscLLAdd(ncols_upper,cols,mbs,nlnk,lnk,lnkbt);CHKERRQ(ierr);
    nzk += nlnk;

    /* update lnk by computing fill-in for each pivot row to be merged in */
    prow = jl[k]; /* 1st pivot row */
   
    while (prow < k){
      nextprow = jl[prow];
      /* merge prow into k-th row */
      jmin = il[prow] + 1;  /* index of the 2nd nzero entry in U(prow,k:mbs-1) */
      jmax = ui[prow+1]; 
      ncols = jmax-jmin;
      uj_ptr = ui_ptr[prow] + jmin - ui[prow]; /* points to the 2nd nzero entry in U(prow,k:mbs-1) */
      ierr = PetscLLAddSorted(ncols,uj_ptr,mbs,nlnk,lnk,lnkbt);CHKERRQ(ierr);
      nzk += nlnk;

      /* update il and jl for prow */
      if (jmin < jmax){
        il[prow] = jmin;
        j = *uj_ptr; jl[prow] = jl[j]; jl[j] = prow;  
      } 
      prow = nextprow; 
    }  

    /* if free space is not available, make more free space */
    if (current_space->local_remaining<nzk) {
      i = mbs - k + 1; /* num of unfactored rows */
      i = PetscMin(i*nzk, i*(i-1)); /* i*nzk, i*(i-1): estimated and max additional space needed */
      ierr = PetscFreeSpaceGet(i,&current_space);CHKERRQ(ierr);
      reallocs++;
    }

    /* copy data into free space, then initialize lnk */
    ierr = PetscLLClean(mbs,mbs,nzk,lnk,current_space->array,lnkbt);CHKERRQ(ierr); 

    /* add the k-th row into il and jl */
    if (nzk-1 > 0){
      i = current_space->array[1]; /* col value of the first nonzero element in U(k, k+1:mbs-1) */    
      jl[k] = jl[i]; jl[i] = k;
      il[k] = ui[k] + 1;
    } 
    ui_ptr[k] = current_space->array;
    current_space->array           += nzk;
    current_space->local_used      += nzk;
    current_space->local_remaining -= nzk;

    ui[k+1] = ui[k] + nzk;  
  } 

#if defined(PETSC_USE_INFO)
  if (ai[mbs] != 0) {
    PetscReal af = ((PetscReal)ui[mbs])/((PetscReal)ai[mbs]);
    ierr = PetscInfo3(A,"Reallocs %D Fill ratio:given %G needed %G\n",reallocs,fill,af);CHKERRQ(ierr);
    ierr = PetscInfo1(A,"Run with -pc_factor_fill %G or use \n",af);CHKERRQ(ierr);
    ierr = PetscInfo1(A,"PCFactorSetFill(pc,%G) for best performance.\n",af);CHKERRQ(ierr);
  } else {
    ierr = PetscInfo(A,"Empty matrix.\n");CHKERRQ(ierr);
  }
#endif

  ierr = ISRestoreIndices(perm,&rip);CHKERRQ(ierr);
  ierr = PetscFree(jl);CHKERRQ(ierr);

  /* destroy list of free space and other temporary array(s) */
  ierr = PetscMalloc((ui[mbs]+1)*sizeof(PetscInt),&uj);CHKERRQ(ierr);
  ierr = PetscFreeSpaceContiguous(&free_space,uj);CHKERRQ(ierr);
  ierr = PetscLLDestroy(lnk,lnkbt);CHKERRQ(ierr);

  /* put together the new matrix in MATSEQSBAIJ format */
  B    = fact;
  ierr = MatSeqSBAIJSetPreallocation(B,bs,MAT_SKIP_ALLOCATION,PETSC_NULL);CHKERRQ(ierr);

  b = (Mat_SeqSBAIJ*)B->data;
  b->singlemalloc = PETSC_FALSE;
  b->free_a       = PETSC_TRUE;
  b->free_ij      = PETSC_TRUE;
  ierr = PetscMalloc((ui[mbs]+1)*sizeof(MatScalar),&b->a);CHKERRQ(ierr);
  b->j    = uj;
  b->i    = ui;
  b->diag = 0;
  b->ilen = 0;
  b->imax = 0;
  b->row  = perm;
  b->pivotinblocks = PETSC_FALSE; /* need to get from MatFactorInfo */
  ierr    = PetscObjectReference((PetscObject)perm);CHKERRQ(ierr); 
  b->icol = perm;
  ierr    = PetscObjectReference((PetscObject)perm);CHKERRQ(ierr); 
  ierr    = PetscMalloc((mbs+1)*sizeof(PetscScalar),&b->solve_work);CHKERRQ(ierr);
  ierr    = PetscLogObjectMemory(B,(ui[mbs]-mbs)*(sizeof(PetscInt)+sizeof(MatScalar)));CHKERRQ(ierr);
  b->maxnz = b->nz = ui[mbs];
  
  B->info.factor_mallocs    = reallocs;
  B->info.fill_ratio_given  = fill;
  if (ai[mbs] != 0) {
    B->info.fill_ratio_needed = ((PetscReal)ui[mbs])/((PetscReal)ai[mbs]);
  } else {
    B->info.fill_ratio_needed = 0.0;
  }
  if (perm_identity){
    B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqBAIJ_N_NaturalOrdering; 
  } else {
    B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqBAIJ_N;
  }
  PetscFunctionReturn(0);
}
