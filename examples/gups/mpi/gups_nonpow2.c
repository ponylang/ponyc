/* ----------------------------------------------------------------------
   gups = algorithm for the HPCC RandomAccess (GUPS) benchmark
          implements a hypercube-style synchronous all2all

   Steve Plimpton, sjplimp@sandia.gov, Sandia National Laboratories
   www.cs.sandia.gov/~sjplimp
   Copyright (2006) Sandia Corporation
------------------------------------------------------------------------- */

/* random update GUPS code, non-power-of-2 number of procs (pow 2 is OK)
   compile with -DCHECK to check if table updates happen on correct proc */

#include "stdio.h"
#include "stdlib.h"
#include "mpi.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))

/* machine defs
   compile with -DLONG64 if a "long" is 64 bits
   else compile with no setting if "long long" is 64 bit */

#ifdef LONG64
#define POLY 0x0000000000000007UL
#define PERIOD 1317624576693539401L
#define ZERO64B 0L
typedef long s64Int;
typedef unsigned long u64Int;
#define U64INT MPI_UNSIGNED_LONG
#else
#define POLY 0x0000000000000007ULL
#define PERIOD 1317624576693539401LL
#define ZERO64B 0LL
typedef long long s64Int;
typedef unsigned long long u64Int;
#define U64INT MPI_LONG_LONG_INT
#endif

u64Int HPCC_starts(s64Int n);

int main(int narg, char **arg)
{
  int me,nprocs;
  int i,iterate,niterate;
  int nlocal,logtable,index;
  int ipartner,ndata,nsend,nkeep,nrecv,maxndata,maxnfinal,nexcess;
  int nbad,chunk,chunkbig,npartition,nlower,nupper,proclo,procmid,nfrac;
  double t0,t0_all,Gups;
  u64Int *table,*data,*send,*offsets;
  u64Int ran,datum,nglobal,nglobalm1,nupdates,offset,indexmid,nstart;
  u64Int ilong,nexcess_long,nbad_long;
  MPI_Status status;

  MPI_Init(&narg,&arg);
  MPI_Comm_rank(MPI_COMM_WORLD,&me);
  MPI_Comm_size(MPI_COMM_WORLD,&nprocs);

  /* command line args = N M chunk
     N = length of global table is 2^N
     M = # of update sets per proc
     chunk = # of updates in one set */

  if (narg != 4) {
    if (me == 0) printf("Syntax: gups N M chunk\n");
    MPI_Abort(MPI_COMM_WORLD,1);
  }

  logtable = atoi(arg[1]);
  niterate = atoi(arg[2]);
  chunk = atoi(arg[3]);

  /* nglobal = entire table (power of 2)
     nlocal = size of my portion (not a power of 2)
     nglobalm1 = global size - 1 (for index computation)
     offsets[i] = starting index in global table of proc I's portion
     offset = starting index in global table of 1st entry in local table */

  nglobal = ((u64Int) 1) << logtable;
  nglobalm1 = nglobal - 1;
  nstart = (double) me / nprocs * nglobal;
  offsets = (u64Int *) malloc((nprocs+1)*sizeof(u64Int));
  MPI_Allgather(&nstart,1,U64INT,offsets,1,U64INT,MPI_COMM_WORLD);
  offsets[nprocs] = nglobal;
  nlocal = offsets[me+1] - offsets[me];
  offset = offsets[me];

  /* allocate local memory
     16 factor insures space for messages that (randomly) exceed chunk size */

  chunkbig = 16*chunk;

  table = (u64Int *) malloc(nlocal*sizeof(u64Int));
  data = (u64Int *) malloc(chunkbig*sizeof(u64Int));
  send = (u64Int *) malloc(chunkbig*sizeof(u64Int));

  if (!table || !data || !send) {
    if (me == 0) printf("Table allocation failed\n");
    MPI_Abort(MPI_COMM_WORLD,1);
  }

  /* initialize my portion of global array
     global array starts with table[i] = i */

  for (i = 0; i < nlocal; i++) table[i] = i + offset;

  /* start my random # partway thru global stream */

  nupdates = (u64Int) nprocs * chunk * niterate;
  ran = HPCC_starts(nupdates/nprocs*me);

  /* loop:
       generate chunk random stream values per proc
       communicate datums to correct processor via hypercube routing
       use received values to update local table */

  maxndata = 0;
  maxnfinal = 0;
  nexcess = 0;
  nbad = 0;

  MPI_Barrier(MPI_COMM_WORLD);
  t0 = -MPI_Wtime();

  for (iterate = 0; iterate < niterate; iterate++) {
    for (i = 0; i < chunk; i++) {
      ran = (ran << 1) ^ ((s64Int) ran < ZERO64B ? POLY : ZERO64B);
      data[i] = ran;
    }
    ndata = chunk;

    npartition = nprocs;
    proclo = 0;
    while (npartition > 1) {
      nlower = npartition/2;
      nupper = npartition - nlower;
      procmid = proclo + nlower;
      indexmid = offsets[procmid];

      nkeep = nsend = 0;
      if (me < procmid) {
	for (i = 0; i < ndata; i++) {
	  if ((data[i] & nglobalm1) >= indexmid) send[nsend++] = data[i];
	  else data[nkeep++] = data[i];
	}
      } else {
	for (i = 0; i < ndata; i++) {
	  if ((data[i] & nglobalm1) < indexmid) send[nsend++] = data[i];
	  else data[nkeep++] = data[i];
	}
      }

      /* if partition halves are equal, exchange message with 1 partner
	 if upper half = lower half + 1:
	   if in lower half, send/recv 2 messages
	     1st exchange with me+nlower, 2nd exchange with me+nlower+1
	     1st send has first 
	       nfrac = (nlower - (me-proclo)) / nupper of my data 
	     2nd send has remainder of my data
	   if not first or last proc of upper half, send/recv 2 messages
	     1st exchange with me-nlower, 2nd exchange with me-nlower-1
	     2nd send has first
	       nfrac = (me-procmid) / nlower of my data 
	     1st send has remainder of my data
	   if first or last proc of upper half, send/recv 1 message
	     each exchanges with first/last proc of lower half
	     send all my data
	   always recv whatever is sent */

      if (nlower == nupper) {
	if (me < procmid) ipartner = me + nlower;
	else ipartner = me - nlower;
	MPI_Sendrecv(send,nsend,U64INT,ipartner,0,&data[nkeep],
		     chunkbig,U64INT,ipartner,0,MPI_COMM_WORLD,&status);
	MPI_Get_count(&status,U64INT,&nrecv);
	ndata = nkeep + nrecv;
	maxndata = MAX(maxndata,ndata);
      } else {
	if (me < procmid) {
	  nfrac = (nlower - (me-proclo)) * nsend / nupper;
	  ipartner = me + nlower;
	  MPI_Sendrecv(send,nfrac,U64INT,ipartner,0,&data[nkeep],
		       chunkbig,U64INT,ipartner,0,MPI_COMM_WORLD,&status);
	  MPI_Get_count(&status,U64INT,&nrecv);
	  nkeep += nrecv;
	  MPI_Sendrecv(&send[nfrac],nsend-nfrac,U64INT,ipartner+1,0,
		       &data[nkeep],chunkbig,U64INT,
		       ipartner+1,0,MPI_COMM_WORLD,&status);
	  MPI_Get_count(&status,U64INT,&nrecv);
	  ndata = nkeep + nrecv;
	} else if (me > procmid && me < procmid+nlower) {
	  nfrac = (me - procmid) * nsend / nlower;
	  ipartner = me - nlower;
	  MPI_Sendrecv(&send[nfrac],nsend-nfrac,U64INT,ipartner,0,&data[nkeep],
		       chunkbig,U64INT,ipartner,0,MPI_COMM_WORLD,&status);
	  MPI_Get_count(&status,U64INT,&nrecv);
	  nkeep += nrecv;
	  MPI_Sendrecv(send,nfrac,U64INT,ipartner-1,0,&data[nkeep],
		       chunkbig,U64INT,ipartner-1,0,MPI_COMM_WORLD,&status);
	  MPI_Get_count(&status,U64INT,&nrecv);
	  ndata = nkeep + nrecv;
	} else {
	  if (me == procmid) ipartner = me - nlower;
	  else ipartner = me - nupper;
	  MPI_Sendrecv(send,nsend,U64INT,ipartner,0,&data[nkeep],
		       chunkbig,U64INT,ipartner,0,MPI_COMM_WORLD,&status);
	  MPI_Get_count(&status,U64INT,&nrecv);
	  ndata = nkeep + nrecv;
	}
      }

      if (me < procmid) npartition = nlower;
      else {
	proclo = procmid;
	npartition = nupper;
      }
    }
    maxnfinal = MAX(maxnfinal,ndata);
    if (ndata > chunk) nexcess += ndata-chunk;

    for (i = 0; i < ndata; i++) {
      datum = data[i];
      index = (datum & nglobalm1) - offset;
      table[index] ^= datum;
    }

#ifdef CHECK
    for (i = 0; i < ndata; i++) {
      index = (datum & nglobalm1) - offset;
      if (index < 0 || index >= nlocal) nbad++;
    }
#endif
  }

  MPI_Barrier(MPI_COMM_WORLD);
  t0 += MPI_Wtime();

  /* stats */

  MPI_Allreduce(&t0,&t0_all,1,MPI_DOUBLE,MPI_SUM,MPI_COMM_WORLD);
  t0 = t0_all/nprocs;

  i = maxndata;
  MPI_Allreduce(&i,&maxndata,1,MPI_INT,MPI_MAX,MPI_COMM_WORLD);
  i = maxnfinal;
  MPI_Allreduce(&i,&maxnfinal,1,MPI_INT,MPI_MAX,MPI_COMM_WORLD);
  ilong = nexcess;
  MPI_Allreduce(&ilong,&nexcess_long,1,U64INT,MPI_SUM,MPI_COMM_WORLD);
  ilong = nbad;
  MPI_Allreduce(&ilong,&nbad_long,1,U64INT,MPI_SUM,MPI_COMM_WORLD);

  nupdates = (u64Int) niterate * nprocs * chunk;
  Gups = nupdates / t0 / 1.0e9;

  if (me == 0) {
    printf("Number of procs: %d\n",nprocs);
    printf("Vector size: %lld\n",nglobal);
    printf("Max datums during comm: %d\n",maxndata);
    printf("Max datums after comm: %d\n",maxnfinal);
    printf("Excess datums (frac): %lld (%g)\n",
	   nexcess_long,(double) nexcess_long / nupdates);
    printf("Bad locality count: %lld\n",nbad_long);
    printf("Update time (secs): %9.3f\n",t0);
    printf("Gups: %9.6f\n",Gups);
  }

  /* clean up */

  free(table);
  free(data);
  free(send);
  free(offsets);
  MPI_Finalize();
}

/* start random number generator at Nth step of stream
   routine provided by HPCC */

u64Int HPCC_starts(s64Int n)
{
  int i, j;
  u64Int m2[64];
  u64Int temp, ran;

  while (n < 0) n += PERIOD;
  while (n > PERIOD) n -= PERIOD;
  if (n == 0) return 0x1;

  temp = 0x1;
  for (i=0; i<64; i++) {
    m2[i] = temp;
    temp = (temp << 1) ^ ((s64Int) temp < 0 ? POLY : 0);
    temp = (temp << 1) ^ ((s64Int) temp < 0 ? POLY : 0);
  }

  for (i=62; i>=0; i--)
    if ((n >> i) & 1)
      break;

  ran = 0x2;
  while (i > 0) {
    temp = 0;
    for (j=0; j<64; j++)
      if ((ran >> j) & 1)
        temp ^= m2[j];
    ran = temp;
    i -= 1;
    if ((n >> i) & 1)
      ran = (ran << 1) ^ ((s64Int) ran < 0 ? POLY : 0);
  }

  return ran;
}
