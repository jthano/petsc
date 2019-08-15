#include <../src/vec/is/sf/impls/basic/allgatherv/sfallgatherv.h>

PETSC_INTERN PetscErrorCode PetscSFBcastAndOpBegin_Gatherv(PetscSF,MPI_Datatype,const void*,void*,MPI_Op);

/*===================================================================================*/
/*              Internal routines for PetscSFPack_Allgatherv                         */
/*===================================================================================*/
PETSC_INTERN PetscErrorCode PetscSFPackGet_Allgatherv(PetscSF sf,MPI_Datatype unit,const void *rkey,const void *lkey,PetscSFPack_Allgatherv *mylink)
{
  PetscSF_Allgatherv     *dat = (PetscSF_Allgatherv*)sf->data;
  PetscSFPack_Allgatherv link;
  PetscSFPack            *p;
  PetscErrorCode         ierr;

  PetscFunctionBegin;
  ierr = PetscSFPackSetErrorOnUnsupportedOverlap(sf,unit,rkey,lkey);CHKERRQ(ierr);
  /* Look for types in cache */
  for (p=&dat->avail; (link=(PetscSFPack_Allgatherv)*p); p=&link->next) {
    PetscBool match;
    ierr = MPIPetsc_Type_compare(unit,link->unit,&match);CHKERRQ(ierr);
    if (match) {
      *p = link->next; /* Remove from available list */
      goto found;
    }
  }

  ierr = PetscNew(&link);CHKERRQ(ierr);
  ierr = PetscSFPackSetupType((PetscSFPack)link,unit);CHKERRQ(ierr);
  /* Must be init'ed to NULL so that we can safely wait on them even they are not used */
  link->request = MPI_REQUEST_NULL;
  /* One tag per link */
  ierr = PetscCommGetNewTag(PetscObjectComm((PetscObject)sf),&link->tag);CHKERRQ(ierr);

  /* DO NOT allocate link->root/leaf. We use lazy allocation since these buffers are likely not needed */
found:
  link->rkey = rkey;
  link->lkey = lkey;
  link->next = dat->inuse;
  dat->inuse = (PetscSFPack)link;

  *mylink     = link;
  PetscFunctionReturn(0);
}

/*===================================================================================*/
/*              Implementations of SF public APIs                                    */
/*===================================================================================*/

/* PetscSFGetGraph is non-collective. An implementation should not have collective calls */
PETSC_INTERN PetscErrorCode PetscSFGetGraph_Allgatherv(PetscSF sf,PetscInt *nroots,PetscInt *nleaves,const PetscInt **ilocal,const PetscSFNode **iremote)
{
  PetscErrorCode ierr;
  PetscInt       i,j,k;
  const PetscInt *range;
  PetscMPIInt    size;

  PetscFunctionBegin;
  ierr = MPI_Comm_size(PetscObjectComm((PetscObject)sf),&size);CHKERRQ(ierr);
  if (nroots)  *nroots  = sf->nroots;
  if (nleaves) *nleaves = sf->nleaves;
  if (ilocal)  *ilocal  = NULL; /* Contiguous leaves */
  if (iremote) {
    if (!sf->remote && sf->nleaves) { /* The && sf->nleaves makes sfgatherv able to inherit this routine */
      ierr = PetscLayoutGetRanges(sf->map,&range);CHKERRQ(ierr);
      ierr = PetscMalloc1(sf->nleaves,&sf->remote);CHKERRQ(ierr);
      sf->remote_alloc = sf->remote;
      for (i=0; i<size; i++) {
        for (j=range[i],k=0; j<range[i+1]; j++,k++) {
          sf->remote[j].rank  = i;
          sf->remote[j].index = k;
        }
      }
    }
    *iremote = sf->remote;
  }
  PetscFunctionReturn(0);
}

PETSC_INTERN PetscErrorCode PetscSFSetUp_Allgatherv(PetscSF sf)
{
  PetscErrorCode     ierr;
  PetscSF_Allgatherv *dat = (PetscSF_Allgatherv*)sf->data;
  PetscMPIInt        size;
  PetscInt           i;
  const PetscInt     *range;

  PetscFunctionBegin;
  ierr = MPI_Comm_size(PetscObjectComm((PetscObject)sf),&size);CHKERRQ(ierr);
  if (sf->nleaves) { /* This if (sf->nleaves) test makes sfgatherv able to inherit this routine */
    ierr = PetscMalloc1(size,&dat->recvcounts);CHKERRQ(ierr);
    ierr = PetscMalloc1(size,&dat->displs);CHKERRQ(ierr);
    ierr = PetscLayoutGetRanges(sf->map,&range);CHKERRQ(ierr);

    for (i=0; i<size; i++) {
      ierr = PetscMPIIntCast(range[i],&dat->displs[i]);CHKERRQ(ierr);
      ierr = PetscMPIIntCast(range[i+1]-range[i],&dat->recvcounts[i]);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

PETSC_INTERN PetscErrorCode PetscSFReset_Allgatherv(PetscSF sf)
{
  PetscSF_Allgatherv     *dat = (PetscSF_Allgatherv*)sf->data;
  PetscSFPack_Allgatherv link,next;
  PetscErrorCode         ierr;

  PetscFunctionBegin;
  ierr = PetscFree(dat->iranks);CHKERRQ(ierr);
  ierr = PetscFree(dat->ioffset);CHKERRQ(ierr);
  ierr = PetscFree(dat->irootloc);CHKERRQ(ierr);
  ierr = PetscFree(dat->recvcounts);CHKERRQ(ierr);
  ierr = PetscFree(dat->displs);CHKERRQ(ierr);
  if (dat->inuse) SETERRQ(PetscObjectComm((PetscObject)sf),PETSC_ERR_ARG_WRONGSTATE,"Outstanding operation has not been completed");
  for (link=(PetscSFPack_Allgatherv)dat->avail; link; link=next) {
    next = (PetscSFPack_Allgatherv)link->next;
    if (!link->isbuiltin) {ierr = MPI_Type_free(&link->unit);CHKERRQ(ierr);}
    ierr = PetscFree(link->root);CHKERRQ(ierr);
    ierr = PetscFree(link->leaf);CHKERRQ(ierr);
    ierr = PetscFree(link);CHKERRQ(ierr);
  }
  dat->avail = NULL;
  PetscFunctionReturn(0);
}

PETSC_INTERN PetscErrorCode PetscSFDestroy_Allgatherv(PetscSF sf)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscSFReset_Allgatherv(sf);CHKERRQ(ierr);
  ierr = PetscFree(sf->data);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static PetscErrorCode PetscSFBcastAndOpBegin_Allgatherv(PetscSF sf,MPI_Datatype unit,const void *rootdata,void *leafdata,MPI_Op op)
{
  PetscErrorCode         ierr;
  PetscSFPack_Allgatherv link;
  PetscMPIInt            sendcount;
  MPI_Comm               comm;
  void                   *recvbuf;
  PetscSF_Allgatherv     *dat = (PetscSF_Allgatherv*)sf->data;

  PetscFunctionBegin;
  ierr = PetscSFPackGet_Allgatherv(sf,unit,rootdata,leafdata,&link);CHKERRQ(ierr);
  ierr = PetscObjectGetComm((PetscObject)sf,&comm);CHKERRQ(ierr);
  ierr = PetscMPIIntCast(sf->nroots,&sendcount);CHKERRQ(ierr);

  if (op == MPIU_REPLACE) {
    recvbuf = leafdata;
  } else {
    if (!link->leaf) {ierr = PetscMalloc(sf->nleaves*link->unitbytes,&link->leaf);CHKERRQ(ierr);}
    recvbuf = link->leaf;
  }
  ierr = MPIU_Iallgatherv(rootdata,sendcount,unit,recvbuf,dat->recvcounts,dat->displs,unit,comm,&link->request);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PETSC_INTERN PetscErrorCode PetscSFBcastAndOpEnd_Allgatherv(PetscSF sf,MPI_Datatype unit,const void *rootdata,void *leafdata,MPI_Op op)
{
  PetscErrorCode         ierr,(*UnpackAndOp)(PetscInt,PetscInt,const PetscInt*,PetscInt,PetscSFPackOpt,void*,const void*);
  PetscSFPack_Allgatherv link;

  PetscFunctionBegin;
  ierr = PetscSFPackGetInUse(sf,unit,rootdata,leafdata,PETSC_OWN_POINTER,(PetscSFPack*)&link);CHKERRQ(ierr);
  ierr = PetscSFPackGetUnpackAndOp(sf,(PetscSFPack)link,op,&UnpackAndOp);CHKERRQ(ierr);
  ierr = MPI_Wait(&link->request,MPI_STATUS_IGNORE);CHKERRQ(ierr);

  if (op != MPIU_REPLACE) {
    if (UnpackAndOp) {ierr = (*UnpackAndOp)(sf->nleaves,link->bs,NULL,0,NULL,leafdata,link->leaf);CHKERRQ(ierr);}
#if defined(PETSC_HAVE_MPI_REDUCE_LOCAL)
    else {
      /* op might be user-defined */
      PetscMPIInt count;
      ierr = PetscMPIIntCast(sf->nleaves,&count);CHKERRQ(ierr);
      ierr = MPI_Reduce_local(link->leaf,leafdata,count,unit,op);CHKERRQ(ierr);
    }
#else
    else SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP,"No unpacking reduction operation for this MPI_Op");
#endif
  }
  ierr = PetscSFPackReclaim(sf,(PetscSFPack*)&link);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static PetscErrorCode PetscSFReduceBegin_Allgatherv(PetscSF sf,MPI_Datatype unit,const void *leafdata,void *rootdata,MPI_Op op)
{
  PetscErrorCode         ierr;
  PetscSFPack_Allgatherv link;
  PetscSF_Allgatherv     *dat = (PetscSF_Allgatherv*)sf->data;
  PetscInt               rstart;
  PetscMPIInt            rank,count,recvcount;
  MPI_Comm               comm;

  PetscFunctionBegin;
  ierr = PetscSFPackGet_Allgatherv(sf,unit,rootdata,leafdata,&link);CHKERRQ(ierr);
  ierr = PetscObjectGetComm((PetscObject)sf,&comm);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);

  if (op == MPIU_REPLACE) {
    /* REPLACE is only meaningful when all processes have the same leafdata to readue. Therefore copy from local leafdata is fine */
    ierr = PetscLayoutGetRange(sf->map,&rstart,NULL);CHKERRQ(ierr);
    ierr = PetscMemcpy(rootdata,(const char*)leafdata+(size_t)rstart*link->unitbytes,(size_t)sf->nroots*link->unitbytes);CHKERRQ(ierr);
  } else {
    /* Reduce all leafdata on rank 0, then scatter the result to root buffer, then reduce root buffer to leafdata */
    if (!rank && !link->leaf) {ierr = PetscMalloc(sf->nleaves*link->unitbytes,&link->leaf);CHKERRQ(ierr);}
    ierr = PetscMPIIntCast(sf->nleaves*link->bs,&count);CHKERRQ(ierr);
    ierr = PetscMPIIntCast(sf->nroots,&recvcount);CHKERRQ(ierr);
    ierr = MPI_Reduce(leafdata,link->leaf,count,link->basicunit,op,0,comm);CHKERRQ(ierr); /* Must do reduce with MPI builltin datatype basicunit */
    if (!link->root) {ierr = PetscMalloc(sf->nroots*link->unitbytes,&link->root);CHKERRQ(ierr);}
    ierr = MPIU_Iscatterv(link->leaf,dat->recvcounts,dat->displs,unit,link->root,recvcount,unit,0,comm,&link->request);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

PETSC_INTERN PetscErrorCode PetscSFReduceEnd_Allgatherv(PetscSF sf,MPI_Datatype unit,const void *leafdata,void *rootdata,MPI_Op op)
{
  PetscErrorCode         ierr,(*UnpackAndOp)(PetscInt,PetscInt,const PetscInt*,PetscInt,PetscSFPackOpt,void*,const void*);
  PetscSFPack_Allgatherv link;

  PetscFunctionBegin;
  ierr = PetscSFPackGetInUse(sf,unit,rootdata,leafdata,PETSC_OWN_POINTER,(PetscSFPack*)&link);CHKERRQ(ierr);
  ierr = MPI_Wait(&link->request,MPI_STATUS_IGNORE);CHKERRQ(ierr);
  if (op != MPIU_REPLACE) {
    ierr = PetscSFPackGetUnpackAndOp(sf,(PetscSFPack)link,op,&UnpackAndOp);CHKERRQ(ierr);
    if (UnpackAndOp) {ierr = (*UnpackAndOp)(sf->nroots,link->bs,NULL,0,NULL,rootdata,link->root);CHKERRQ(ierr);}
#if defined(PETSC_HAVE_MPI_REDUCE_LOCAL)
    else if (sf->nroots) {
      /* op might be user-defined */
      PetscMPIInt count;
      ierr = PetscMPIIntCast(sf->nroots,&count);CHKERRQ(ierr);
      ierr = MPI_Reduce_local(link->root,rootdata,count,unit,op);CHKERRQ(ierr);
    }
#else
    else SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP,"No unpacking reduction operation for this MPI_Op");
#endif
  }
  ierr = PetscSFPackReclaim(sf,(PetscSFPack*)&link);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static PetscErrorCode PetscSFBcastToZero_Allgatherv(PetscSF sf,MPI_Datatype unit,const void *rootdata,void *leafdata)
{
  PetscErrorCode         ierr;
  PetscSFPack_Allgatherv link;

  PetscFunctionBegin;
  ierr = PetscSFBcastAndOpBegin_Gatherv(sf,unit,rootdata,leafdata,MPIU_REPLACE);CHKERRQ(ierr);
  /* A simplified PetscSFBcastAndOpEnd_Allgatherv */
  ierr = PetscSFPackGetInUse(sf,unit,rootdata,leafdata,PETSC_OWN_POINTER,(PetscSFPack*)&link);CHKERRQ(ierr);
  ierr = MPI_Wait(&link->request,MPI_STATUS_IGNORE);CHKERRQ(ierr);
  ierr = PetscSFPackReclaim(sf,(PetscSFPack*)&link);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* This routine is very tricky (I believe it is rarely used with this kind of graph so just provide a simple but not-optimal implementation).

   Suppose we have three ranks. Rank 0 has a root with value 1. Rank 0,1,2 has a leaf with value 2,3,4 respectively. The leaves are connected
   to the root on rank 0. Suppose op=MPI_SUM and rank 0,1,2 gets root state in their rank order. By definition of this routine, rank 0 sees 1
   in root, fetches it into its leafupate, then updates root to 1 + 2 = 3; rank 1 sees 3 in root, fetches it into its leafupate, then updates
   root to 3 + 3 = 6; rank 2 sees 6 in root, fetches it into its leafupdate, then updates root to 6 + 4 = 10.  At the end, leafupdate on rank
   0,1,2 is 1,3,6 respectively. root is 10.

   One optimized implementation could be: starting from the initial state:
             rank-0   rank-1    rank-2
        Root     1
        Leaf     2       3         4

   Shift leaves rightwards to leafupdate. Rank 0 gathers the root value and puts it in leafupdate. We have:
             rank-0   rank-1    rank-2
        Root     1
        Leaf     2       3         4
     Leafupdate  1       2         3

   Then, do MPI_Scan on leafupdate and get:
             rank-0   rank-1    rank-2
        Root     1
        Leaf     2       3         4
     Leafupdate  1       3         6

   Rank 2 sums its leaf and leafupdate, scatters the result to the root, and gets
             rank-0   rank-1    rank-2
        Root     10
        Leaf     2       3         4
     Leafupdate  1       3         6

   We use a simpler implementation. From the same initial state, we copy leafdata to leafupdate
             rank-0   rank-1    rank-2
        Root     1
        Leaf     2       3         4
     Leafupdate  2       3         4

   Do MPI_Exscan on leafupdate,
             rank-0   rank-1    rank-2
        Root     1
        Leaf     2       3         4
     Leafupdate  2       2         5

   BcastAndOp from root to leafupdate,
             rank-0   rank-1    rank-2
        Root     1
        Leaf     2       3         4
     Leafupdate  3       3         6

   Copy root to leafupdate on rank-0
             rank-0   rank-1    rank-2
        Root     1
        Leaf     2       3         4
     Leafupdate  1       3         6

   Reduce from leaf to root,
             rank-0   rank-1    rank-2
        Root     10
        Leaf     2       3         4
     Leafupdate  1       3         6
*/
PETSC_INTERN PetscErrorCode PetscSFFetchAndOpBegin_Allgatherv(PetscSF sf,MPI_Datatype unit,void *rootdata,const void *leafdata,void *leafupdate,MPI_Op op)
{
  PetscErrorCode         ierr;
  PetscSFPack_Allgatherv link;
  MPI_Comm               comm;
  PetscMPIInt            count;

  PetscFunctionBegin;
  /* Copy leafdata to leafupdate */
  ierr = PetscSFPackGet_Allgatherv(sf,unit,rootdata,leafdata,&link);CHKERRQ(ierr);
  ierr = PetscMemcpy(leafupdate,leafdata,sf->nleaves*link->unitbytes);CHKERRQ(ierr);
  ierr = PetscSFPackGetInUse(sf,unit,rootdata,leafdata,PETSC_OWN_POINTER,(PetscSFPack*)&link);CHKERRQ(ierr);

  /* Exscan on leafupdate and then BcastAndOp rootdata to leafupdate */
  ierr = PetscObjectGetComm((PetscObject)sf,&comm);CHKERRQ(ierr);
  ierr = PetscMPIIntCast(sf->nleaves,&count);CHKERRQ(ierr);
  if (op == MPIU_REPLACE) {
    PetscMPIInt size,rank,prev,next;
    ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
    ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);
    prev = rank ?            rank-1 : MPI_PROC_NULL;
    next = (rank < size-1) ? rank+1 : MPI_PROC_NULL;
    ierr = MPI_Sendrecv_replace(leafupdate,count,unit,next,link->tag,prev,link->tag,comm,MPI_STATUSES_IGNORE);CHKERRQ(ierr);
  } else {ierr = MPI_Exscan(MPI_IN_PLACE,leafupdate,count,unit,op,comm);CHKERRQ(ierr);}
  ierr = PetscSFPackReclaim(sf,(PetscSFPack*)&link);CHKERRQ(ierr);
  ierr = PetscSFBcastAndOpBegin(sf,unit,rootdata,leafupdate,op);CHKERRQ(ierr);
  ierr = PetscSFBcastAndOpEnd(sf,unit,rootdata,leafupdate,op);CHKERRQ(ierr);

  /* Bcast roots to rank 0's leafupdate */
  ierr = PetscSFBcastToZero_Private(sf,unit,rootdata,leafupdate);CHKERRQ(ierr); /* Using this line makes Allgather SFs able to inherit this routine */

  /* Reduce leafdata to rootdata */
  ierr = PetscSFReduceBegin(sf,unit,leafdata,rootdata,op);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PETSC_INTERN PetscErrorCode PetscSFFetchAndOpEnd_Allgatherv(PetscSF sf,MPI_Datatype unit,void *rootdata,const void *leafdata,void *leafupdate,MPI_Op op)
{
  PetscErrorCode         ierr;

  PetscFunctionBegin;
  ierr = PetscSFReduceEnd(sf,unit,leafdata,rootdata,op);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* Get root ranks accessing my leaves */
PETSC_INTERN PetscErrorCode PetscSFGetRootRanks_Allgatherv(PetscSF sf,PetscInt *nranks,const PetscMPIInt **ranks,const PetscInt **roffset,const PetscInt **rmine,const PetscInt **rremote)
{
  PetscErrorCode ierr;
  PetscInt       i,j,k,size;
  const PetscInt *range;

  PetscFunctionBegin;
  /* Lazily construct these large arrays if users really need them for this type of SF. Very likely, they do not */
  if (sf->nranks && !sf->ranks) { /* On rank!=0, sf->nranks=0. The sf->nranks test makes this routine also works for sfgatherv */
    size = sf->nranks;
    ierr = PetscLayoutGetRanges(sf->map,&range);CHKERRQ(ierr);
    ierr = PetscMalloc4(size,&sf->ranks,size+1,&sf->roffset,sf->nleaves,&sf->rmine,sf->nleaves,&sf->rremote);CHKERRQ(ierr);
    for (i=0; i<size; i++) sf->ranks[i] = i;
    ierr = PetscArraycpy(sf->roffset,range,size+1);CHKERRQ(ierr);
    for (i=0; i<sf->nleaves; i++) sf->rmine[i] = i; /*rmine are never NULL even for contiguous leaves */
    for (i=0; i<size; i++) {
      for (j=range[i],k=0; j<range[i+1]; j++,k++) sf->rremote[j] = k;
    }
  }

  if (nranks)  *nranks  = sf->nranks;
  if (ranks)   *ranks   = sf->ranks;
  if (roffset) *roffset = sf->roffset;
  if (rmine)   *rmine   = sf->rmine;
  if (rremote) *rremote = sf->rremote;
  PetscFunctionReturn(0);
}

/* Get leaf ranks accessing my roots */
PETSC_INTERN PetscErrorCode PetscSFGetLeafRanks_Allgatherv(PetscSF sf,PetscInt *niranks,const PetscMPIInt **iranks,const PetscInt **ioffset,const PetscInt **irootloc)
{
  PetscErrorCode     ierr;
  PetscSF_Allgatherv *dat = (PetscSF_Allgatherv*)sf->data;
  MPI_Comm           comm;
  PetscMPIInt        size,rank;
  PetscInt           i,j;

  PetscFunctionBegin;
  /* Lazily construct these large arrays if users really need them for this type of SF. Very likely, they do not */
  ierr = PetscObjectGetComm((PetscObject)sf,&comm);CHKERRQ(ierr);
  ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(comm,&rank);CHKERRQ(ierr);
  if (niranks) *niranks = size;

  /* PetscSF_Basic has distinguished incoming ranks. Here we do not need that. But we must put self as the first and
     sort other ranks. See comments in PetscSFSetUp_Basic about MatGetBrowsOfAoCols_MPIAIJ on why.
   */
  if (iranks) {
    if (!dat->iranks) {
      ierr = PetscMalloc1(size,&dat->iranks);CHKERRQ(ierr);
      dat->iranks[0] = rank;
      for (i=0,j=1; i<size; i++) {if (i == rank) continue; dat->iranks[j++] = i;}
    }
    *iranks = dat->iranks; /* dat->iranks was init'ed to NULL by PetscNewLog */
  }

  if (ioffset) {
    if (!dat->ioffset) {
      ierr = PetscMalloc1(size+1,&dat->ioffset);CHKERRQ(ierr);
      for (i=0; i<=size; i++) dat->ioffset[i] = i*sf->nroots;
    }
    *ioffset = dat->ioffset;
  }

  if (irootloc) {
    if (!dat->irootloc) {
      ierr = PetscMalloc1(sf->nleaves,&dat->irootloc);CHKERRQ(ierr);
      for (i=0; i<size; i++) {
        for (j=0; j<sf->nroots; j++) dat->irootloc[i*sf->nroots+j] = j;
      }
    }
    *irootloc = dat->irootloc;
  }
  PetscFunctionReturn(0);
}

PETSC_INTERN PetscErrorCode PetscSFCreateLocalSF_Allgatherv(PetscSF sf,PetscSF *out)
{
  PetscInt       i,nroots,nleaves,rstart,*ilocal;
  PetscSFNode    *iremote;
  PetscSF        lsf;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  nroots  = sf->nroots;
  nleaves = sf->nleaves ? sf->nroots : 0;
  ierr    = PetscMalloc1(nleaves,&ilocal);CHKERRQ(ierr);
  ierr    = PetscMalloc1(nleaves,&iremote);CHKERRQ(ierr);
  ierr    = PetscLayoutGetRange(sf->map,&rstart,NULL);CHKERRQ(ierr);

  for (i=0; i<nleaves; i++) {
    ilocal[i]        = rstart + i; /* lsf does not change leave indices */
    iremote[i].rank  = 0;          /* rank in PETSC_COMM_SELF */
    iremote[i].index = i;          /* root index */
  }

  ierr = PetscSFCreate(PETSC_COMM_SELF,&lsf);CHKERRQ(ierr);
  ierr = PetscSFSetGraph(lsf,nroots,nleaves,ilocal,PETSC_OWN_POINTER,iremote,PETSC_OWN_POINTER);CHKERRQ(ierr);
  ierr = PetscSFSetUp(lsf);CHKERRQ(ierr);
  *out = lsf;
  PetscFunctionReturn(0);
}

PETSC_INTERN PetscErrorCode PetscSFCreate_Allgatherv(PetscSF sf)
{
  PetscErrorCode     ierr;
  PetscSF_Allgatherv *dat = (PetscSF_Allgatherv*)sf->data;

  PetscFunctionBegin;
  sf->ops->SetUp           = PetscSFSetUp_Allgatherv;
  sf->ops->Reset           = PetscSFReset_Allgatherv;
  sf->ops->Destroy         = PetscSFDestroy_Allgatherv;
  sf->ops->GetRootRanks    = PetscSFGetRootRanks_Allgatherv;
  sf->ops->GetLeafRanks    = PetscSFGetLeafRanks_Allgatherv;
  sf->ops->GetGraph        = PetscSFGetGraph_Allgatherv;
  sf->ops->BcastAndOpBegin = PetscSFBcastAndOpBegin_Allgatherv;
  sf->ops->BcastAndOpEnd   = PetscSFBcastAndOpEnd_Allgatherv;
  sf->ops->ReduceBegin     = PetscSFReduceBegin_Allgatherv;
  sf->ops->ReduceEnd       = PetscSFReduceEnd_Allgatherv;
  sf->ops->FetchAndOpBegin = PetscSFFetchAndOpBegin_Allgatherv;
  sf->ops->FetchAndOpEnd   = PetscSFFetchAndOpEnd_Allgatherv;
  sf->ops->CreateLocalSF   = PetscSFCreateLocalSF_Allgatherv;
  sf->ops->BcastToZero     = PetscSFBcastToZero_Allgatherv;

  ierr = PetscNewLog(sf,&dat);CHKERRQ(ierr);
  sf->data = (void*)dat;
  PetscFunctionReturn(0);
}
