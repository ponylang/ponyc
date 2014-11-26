/* ----------------------------------------------------------------------
   gups = algorithm for the HPCC RandomAccess (GUPS) benchmark
          implements a hypercube-style synchronous all2all

   Steve Plimpton, sjplimp@sandia.gov, Sandia National Laboratories
   www.cs.sandia.gov/~sjplimp
   Copyright (2006) Sandia Corporation

   optimizations implemented by Ron Brightwell and Keith Underwood (SNL)
------------------------------------------------------------------------- */

/* random update GUPS code with optimizations, power-of-2 number of procs
   compile with -DCHECK to check if table updates happen on correct proc */

#include "stdio.h"
#include "stdlib.h"
#include "mpi.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define MAXLOGPROCS (20)
#define CHUNK  1024
/* Optimize CHUNK2 to make sure that we minimize aliasing in the cache -KDU */
#define CHUNKBIG 10240
/* RCHUNK is constant because we have log2(N) of them, but they are used
 * independently on successive iterations (i.e. they don't interfere
 * with each other in cache -KDU
 */
#define RCHUNK 4096
#define PITER 8

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

void sort_data (u64Int *source, u64Int *nomatch, u64Int *match, int number,
		int *nnomatch, int *nmatch, int mask_shift);
inline update_table (u64Int *data, u64Int *table, int number, int nlocalm1);
u64Int HPCC_starts(s64Int n);

int main(int narg, char **arg)
{
  int me,nprocs;
  int i,j,k,iterate,niterate;
  int nlocal,nlocalm1,logtable,index,logtablelocal;
  int logprocs,ipartner,ndata,nsend,nkeep,nkept,nrecv;
  int maxndata,maxnfinal,nexcess;
  int nbad;
  double t0,t0_all,Gups;
  u64Int *table,*data,*send, *keep_data;
#ifndef USE_BLOCKING_SEND
  u64Int *send1,*send2;
#endif
  u64Int *recv[PITER][MAXLOGPROCS];
  u64Int ran,datum,procmask,nglobal,offset,nupdates;
  u64Int ilong,nexcess_long,nbad_long;
  MPI_Status status;
  MPI_Request request[PITER][MAXLOGPROCS];
  MPI_Request srequest;

  MPI_Init(&narg,&arg);
  MPI_Comm_rank(MPI_COMM_WORLD,&me);
  MPI_Comm_size(MPI_COMM_WORLD,&nprocs);

  /* command line args = N M
     N = length of global table is 2^N
     M = # of 1024-update sets per proc */

  if (narg != 3) {
    if (me == 0) printf("Syntax: gups N M\n");
    MPI_Abort(MPI_COMM_WORLD,1);
  }

  logtable = atoi(arg[1]);
  niterate = atoi(arg[2]);

  /* insure Nprocs is power of 2 */

  i = 1;
  while (i < nprocs) i *= 2;
  if (i != nprocs) {
    if (me == 0) printf("Must run on power-of-2 procs\n");
    MPI_Abort(MPI_COMM_WORLD,1);
  }

  /* nglobal = entire table
     nlocal = size of my portion
     nlocalm1 = local size - 1 (for index computation)
     logtablelocal = log of table size I store
     offset = starting index in global table of 1st entry in local table */

  logprocs = 0;
  while (1 << logprocs < nprocs) logprocs++;

  nglobal = ((u64Int) 1) << logtable;
  nlocal = nglobal / nprocs;
  nlocalm1 = nlocal - 1;
  logtablelocal = logtable - logprocs;
  offset = (u64Int) nlocal * me;

  /* allocate local memory */

  table = (u64Int *) malloc(nlocal*sizeof(u64Int));
  data = (u64Int *) malloc(CHUNKBIG*sizeof(u64Int));

  if (!table || !data) {
    if (me == 0) printf("Table allocation failed\n");
    MPI_Abort(MPI_COMM_WORLD,1);
  }

#ifdef USE_BLOCKING_SEND
  send = (u64Int *) malloc(CHUNKBIG*sizeof(u64Int));
  if (!send) {
    if (me == 0) printf("Table allocation failed\n");
    MPI_Abort(MPI_COMM_WORLD,1);
  }
#else
  send1 = (u64Int *) malloc(CHUNKBIG*sizeof(u64Int));
  send2 = (u64Int *) malloc(CHUNKBIG*sizeof(u64Int));
  send = send1;
  if (!send1 || !send2) {
    if (me == 0) printf("Table allocation failed\n");
    MPI_Abort(MPI_COMM_WORLD,1);
  }
#endif

  for (j = 0; j < PITER; j++)
    for (i=0; i<logprocs; i++) {
      if ((recv[j][i] = (u64Int *)malloc(sizeof(u64Int)*RCHUNK)) == NULL) {
         printf("Recv buffer allocation failed\n");
         MPI_Abort(MPI_COMM_WORLD,1);
      }
    }

  /* initialize my portion of global array
     global array starts with table[i] = i */

  for (i = 0; i < nlocal; i++) table[i] = i + offset;

  /* start my random # partway thru global stream */

  nupdates = (u64Int) nprocs * CHUNK * niterate;
  ran = HPCC_starts(nupdates/nprocs*me);

  /* loop:
       generate CHUNK random values per proc
       communicate datums to correct processor via hypercube routing
       use received values to update local table */

  maxndata = 0;
  maxnfinal = 0;
  nexcess = 0;
  nbad = 0;

  MPI_Barrier(MPI_COMM_WORLD);
  t0 = -MPI_Wtime();

  for (iterate = 0; iterate < niterate; iterate++) {
    int iter_mod = iterate % PITER;
    for (i = 0; i < CHUNK; i++) {
      ran = (ran << 1) ^ ((s64Int) ran < ZERO64B ? POLY : ZERO64B);
      data[i] = ran;
    }
    nkept = CHUNK;
    nrecv = 0;

    if (iter_mod == 0)
      for (k=0; k < PITER; k++)
        for (j=0; j<logprocs; j++) {
          ipartner = (1 << j) ^ me;
          MPI_Irecv(recv[k][j],RCHUNK,U64INT,ipartner,0,MPI_COMM_WORLD,
	    &request[k][j]);
        }

    for (j = 0; j < logprocs; j++) {
      nkeep = nsend = 0;
#ifndef USE_BLOCKING_SEND
      send = (send == send1) ? send2 : send1;
#endif
      ipartner = (1 << j) ^ me;
      procmask = ((u64Int) 1) << (logtablelocal + j);
      if (ipartner > me) {
      	sort_data(data,data,send,nkept,&nkeep,&nsend,logtablelocal+j);
        if (j > 0) {
          MPI_Wait(&request[iter_mod][j-1],&status);
          MPI_Get_count(&status,U64INT,&nrecv);


      	  sort_data(recv[iter_mod][j-1],data,send,nrecv,&nkeep,
	    &nsend,logtablelocal+j);
        }
      } else {
        sort_data(data,send,data,nkept,&nsend,&nkeep,logtablelocal+j);
        if (j > 0) {
          MPI_Wait(&request[iter_mod][j-1],&status);
          MPI_Get_count(&status,U64INT,&nrecv);
          sort_data(recv[iter_mod][j-1],send,data,nrecv,&nsend,
		    &nkeep,logtablelocal+j);
        }
      }
#ifdef USE_BLOCKING_SEND
      MPI_Send(send,nsend,U64INT,ipartner,0,MPI_COMM_WORLD);
#else
      if (j > 0) MPI_Wait(&srequest,&status);
      MPI_Isend(send,nsend,U64INT,ipartner,0,MPI_COMM_WORLD,&srequest);
#endif
      if (j == (logprocs - 1)) {
	update_table(data, table, nkeep, nlocalm1);
      }
      maxndata = MAX(maxndata,nkept+nrecv);
      nkept = nkeep;
    }

    if (logprocs == 0) {
      update_table(data, table, nkept, nlocalm1);
    } else {
      MPI_Wait(&request[iter_mod][j-1],&status);
      MPI_Get_count(&status,U64INT,&nrecv);
      update_table(recv[iter_mod][j-1], table, nrecv, nlocalm1);
#ifndef USE_BLOCKING_SEND
      MPI_Wait(&srequest,&status);
#endif
    }

    ndata = nkept + nrecv;
    maxndata = MAX(maxndata,ndata);
    maxnfinal = MAX(maxnfinal,ndata);
    if (ndata > CHUNK) nexcess += ndata - CHUNK;

#ifdef CHECK
    procmask = ((u64Int) (nprocs-1)) << logtablelocal;
    for (i = 0; i < nkept; i++)
      if ((data[i] & procmask) >> logtablelocal != me) nbad++;
    for (i = 0; i < nrecv; i++)
      if ((recv[iter_mod][j-1][i] & procmask) >> logtablelocal != me) nbad++;
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

  nupdates = (u64Int) niterate * nprocs * CHUNK;
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

  for (j = 0; j < PITER; j++)
    for (i = 0; i < logprocs; i++) free(recv[j][i]);
  free(table);
  free(data);
#ifdef USE_BLOCKING_SEND
  free(send);
#else
  free(send1);
  free(send2);
#endif
  MPI_Finalize();
}

/* This sort is manually unrolled to make sure the compiler can see
 * the parallelism -KDU
 */

void sort_data(u64Int *source, u64Int *nomatch, u64Int *match, int number,
	       int *nnomatch, int *nmatch, int mask_shift)
{
  int div_num = number / 8;
  int loop_total = div_num * 8;
  u64Int procmask = ((u64Int) 1) << mask_shift;
  int i;
  u64Int *buffers[2];
  int counts[2];

  buffers[0] = nomatch;
  counts[0] = *nnomatch;
  buffers[1] = match;
  counts[1] = *nmatch;

  for (i = 0; i < div_num; i++) {
    int dindex = i*8;
    int myselect[8];
    myselect[0] = (source[dindex] & procmask) >> mask_shift;
    myselect[1] = (source[dindex+1] & procmask) >> mask_shift;
    myselect[2] = (source[dindex+2] & procmask) >> mask_shift;
    myselect[3] = (source[dindex+3] & procmask) >> mask_shift;
    myselect[4] = (source[dindex+4] & procmask) >> mask_shift;
    myselect[5] = (source[dindex+5] & procmask) >> mask_shift;
    myselect[6] = (source[dindex+6] & procmask) >> mask_shift;
    myselect[7] = (source[dindex+7] & procmask) >> mask_shift;
    buffers[myselect[0]][counts[myselect[0]]++] = source[dindex];
    buffers[myselect[1]][counts[myselect[1]]++] = source[dindex+1];
    buffers[myselect[2]][counts[myselect[2]]++] = source[dindex+2];
    buffers[myselect[3]][counts[myselect[3]]++] = source[dindex+3];
    buffers[myselect[4]][counts[myselect[4]]++] = source[dindex+4];
    buffers[myselect[5]][counts[myselect[5]]++] = source[dindex+5];
    buffers[myselect[6]][counts[myselect[6]]++] = source[dindex+6];
    buffers[myselect[7]][counts[myselect[7]]++] = source[dindex+7];
  }

  for (i = loop_total; i < number; i++) {
    u64Int mydata = source[i];
    if (mydata & procmask) buffers[1][counts[1]++] = mydata;
    else buffers[0][counts[0]++] = mydata;
  }

  *nnomatch = counts[0];
  *nmatch = counts[1];
}

inline update_table(u64Int *data, u64Int *table, int number, int nlocalm1)
{
/* DEEP_UNROLL doesn't seem to improve anything at this time */
/* Manual unrolling is a significant win if -Msafeptr is used -KDU */
#ifdef DEEP_UNROLL
  int div_num = number / 16;
  int loop_total = div_num * 16;
#else
  int div_num = number / 8;
  int loop_total = div_num * 8;
#endif

  int i;
  for (i = 0; i < div_num; i++) {
#ifdef DEEP_UNROLL
    const int dindex = i*16;
#else
    const int dindex = i*8;
#endif
    u64Int index0 = data[dindex] & nlocalm1;
    u64Int index1 = data[dindex+1] & nlocalm1;
    u64Int index2 = data[dindex+2] & nlocalm1;
    u64Int index3 = data[dindex+3] & nlocalm1;
    u64Int index4 = data[dindex+4] & nlocalm1;
    u64Int index5 = data[dindex+5] & nlocalm1;
    u64Int index6 = data[dindex+6] & nlocalm1;
    u64Int index7 = data[dindex+7] & nlocalm1;
    u64Int ltable0 = table[index0];
    u64Int ltable1 = table[index1];
    u64Int ltable2 = table[index2];
    u64Int ltable3 = table[index3];
    u64Int ltable4 = table[index4];
    u64Int ltable5 = table[index5];
    u64Int ltable6 = table[index6];
    u64Int ltable7 = table[index7];
#ifdef DEEP_UNROLL
    u64Int index8 = data[dindex+8] & nlocalm1;
    u64Int index9 = data[dindex+9] & nlocalm1;
    u64Int index10 = data[dindex+10] & nlocalm1;
    u64Int index11 = data[dindex+11] & nlocalm1;
    u64Int index12 = data[dindex+12] & nlocalm1;
    u64Int index13 = data[dindex+13] & nlocalm1;
    u64Int index14 = data[dindex+14] & nlocalm1;
    u64Int index15 = data[dindex+15] & nlocalm1;
    u64Int ltable8 = table[index8];
    u64Int ltable9 = table[index9];
    u64Int ltable10 = table[index10];
    u64Int ltable11 = table[index11];
    u64Int ltable12 = table[index12];
    u64Int ltable13 = table[index13];
    u64Int ltable14 = table[index14];
    u64Int ltable15 = table[index15];
#endif
    table[index0] = ltable0 ^ data[dindex];
    table[index1] = ltable1 ^ data[dindex+1];
    table[index2] = ltable2 ^ data[dindex+2];
    table[index3] = ltable3 ^ data[dindex+3];
    table[index4] = ltable4 ^ data[dindex+4];
    table[index5] = ltable5 ^ data[dindex+5];
    table[index6] = ltable6 ^ data[dindex+6];
    table[index7] = ltable7 ^ data[dindex+7];
#ifdef DEEP_UNROLL
    table[index8] = ltable8 ^ data[dindex+8];
    table[index9] = ltable9 ^ data[dindex+9];
    table[index10] = ltable10 ^ data[dindex+10];
    table[index11] = ltable11 ^ data[dindex+11];
    table[index12] = ltable12 ^ data[dindex+12];
    table[index13] = ltable13 ^ data[dindex+13];
    table[index14] = ltable14 ^ data[dindex+14];
    table[index15] = ltable15 ^ data[dindex+15];
#endif
  }

  for (i = loop_total; i < number; i++) {
    u64Int datum = data[i];
    int index = datum & nlocalm1;
    table[index] ^= datum;
  }
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
