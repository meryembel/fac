/*
 *   FAC - Flexible Atomic Code
 *   Copyright (C) 2001-2015 Ming Feng Gu
 * 
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 * 
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 * 
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mpiutil.h"
#include "parser.h"
#include "stdarg.h"
#include "init.h"

int SkipMPI(int *wid, int myrank, int nproc) {
#ifdef USE_MPI
  int r = 0;
  if (nproc > 1) {
    if (*wid != myrank) {
      r = 1;
    }
    //MPrintf(-1, "SkipMPI: %d %d %d %d\n", *wid, myrank, nproc, r);
    *wid = *wid + 1;
    if (*wid >= nproc) *wid = 0;
  }
  return r;
#else
  return 0;
#endif
}
  
void MPISeqBeg() {
#if USE_MPI == 1
  if (MPIReady()) {
    int myrank;
    int k;
    MPI_Status s;
    
    myrank = MPIRank(NULL);
    if (myrank > 0) {
      k = -1;
      MPI_Recv(&k, 1, MPI_INT, myrank-1, myrank-1, MPI_COMM_WORLD, &s);
      if (k != myrank-1) {
	printf("Error in MPISeqBeg %d %d\n", myrank, k);
      }
    }
  }
#endif
}

void MPISeqEnd() {
#if USE_MPI == 1
  if (MPIReady()) {
    int myrank;
    int nproc;
    
    myrank = MPIRank(&nproc);
    if (myrank < nproc-1) {
      MPI_Send(&myrank, 1, MPI_INT, myrank+1, myrank, MPI_COMM_WORLD);
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }
#endif
}

void MPrintf(int ir, char *format, ...) {
  va_list args;

  va_start(args, format);
#if USE_MPI == 1
  if (MPIReady()) {
    int myrank;
    int nproc;
    myrank = MPIRank(&nproc);
    if (ir < 0) {
      printf("Rank=%d, ", myrank);
      vprintf(format, args);
    } else {
      if (myrank == ir%nproc) {
	if (ir >= nproc) {
	  printf("Rank=%d, ", myrank);
	}
	vprintf(format, args);
      }
    }
  } else {
    vprintf(format, args);
  }
#else
  vprintf(format, args);
#endif
  va_end(args);
  fflush(stdout);
}

int MPIRank(int *np) {
  int k;
#if USE_MPI == 1
  if (MPIReady()) {
    MPI_Comm_rank(MPI_COMM_WORLD, &k);
    if (np) {
      MPI_Comm_size(MPI_COMM_WORLD, np);
    }
  } else {
    k = 0;
    if (np) *np = 1;
  }
#elif USE_MPI == 2
  k = omp_get_thread_num();
  if (np) *np = omp_get_num_threads();
#else
  k = 0;
  if (np) *np = 1;
#endif
  return k;
}

int MPIReady() {
#if USE_MPI == 1
  int k;
  MPI_Initialized(&k);
  return k;
#else
  return 0;
#endif
}

void InitializeMPI(char *s) {
#if USE_MPI == 1
  int argc, i, n;
  char **argv, *p;
  if (s == NULL) {
    argc = 1;
  } else {
    argc = 1 + StrSplit(s, ' ');
  }
  argv = malloc(sizeof(char *)*argc);
  argv[0] = malloc(sizeof(char));
  argv[0][0] = '\0';
  p = s;
  for (i = 1; i < argc; i++) {
    n = strlen(p);
    argv[i] = malloc(sizeof(char)*(n+1));
    strcpy(argv[i], p);
    printf("%d %s %s\n", i, p, argv[i]);
    p += n+1;
  }
  MPI_Init(&argc, &argv);
  for (i = 0; i < argc; i++) {
    free(argv[i]);
  }
  free(argv);
  SetMPIRankMBPT();
  SetMPIRankRadial();
  SetMPIRankStructure();
#endif
}

void FinalizeMPI() {
#if USE_MPI == 1
  MPI_Finalize();
#endif
}

void Abort(int r) {
#if USE_MPI == 1
  MPI_Abort(MPI_COMM_WORLD, r);
#else
  exit(r);
#endif
}

BFILE *BFileOpen(char *fn, char *md, int nb) {  
  BFILE *bf;  
  bf = malloc(sizeof(BFILE));  
  bf->p = 0;
  bf->n = 0;
  bf->eof = 0;
  bf->nbuf = nb>=0?nb:RBUFL;
  if (bf->nbuf == 0) {
    bf->nr = 1;
    bf->mr = 0;
    bf->buf = NULL;
    bf->f = fopen(fn, md);
    if (bf->f == NULL) {
      free(bf);
      return NULL;
    }
    return bf;
  }
#if USE_MPI == 1
  bf->mr = MPIRank(&bf->nr);
  if (bf->mr == 0) {
    bf->f = fopen(fn, md);
    if (bf->f == NULL) bf->p = -1;
  } else {
    bf->f = NULL;
  }
  if (bf->nr > 1) {
    MPI_Bcast(&bf->p, 1, MPI_INT, 0, MPI_COMM_WORLD);
  }
  if (bf->p < 0) {
    free(bf);
    return NULL;
  }
  if (bf->nr > 1) {
    bf->buf = malloc(bf->nbuf);
  } else {
    bf->buf = NULL;
  }
#else
  bf->nr = 1;
  bf->mr = 0;
  bf->buf = NULL;
  bf->f = fopen(fn, md);
  if (bf->f == NULL) {
    free(bf);
    return NULL;
  }
#endif
  bf->fn = malloc(strlen(fn)+1);
  strcpy(bf->fn, fn);
  return bf;
}

int BFileClose(BFILE *bf) {
  int r = 0;
#if USE_MPI == 1
  if (bf == NULL) return 0;
  if (bf->nr <= 1) {
    r = fclose(bf->f);
  } else {
    if (bf->mr == 0) {
      r = fclose(bf->f);
    }  
    free(bf->buf);
  }
#else
  r = fclose(bf->f);
#endif
  free(bf->fn);
  free(bf);
  return r;
}

size_t BFileRead(void *ptr, size_t size, size_t nmemb, BFILE *bf) {
#if USE_MPI == 1
  if (bf->nr <= 1) {
    return fread(ptr, size, nmemb, bf->f);
  }
  if (size > bf->nbuf) {
    if (bf->mr == 0) {
      printf("buffer size %d smaller than data size %d\n", (int)bf->nbuf, (int)size);
    }
    Abort(1);
  }
  int nb = bf->n - bf->p;
  int n = nb/size;
  int nr=0, nm=0, nn=0, nread;  
  nread = 0;
  while (nmemb) {
    if (n >= nmemb) {
      nr = size*nmemb;
      memcpy(ptr, bf->buf+bf->p, nr);
      bf->p += nr;
      nread += nmemb;
      return nread;
    } else if (n > 0) {
      nm = size*n;
      memcpy(ptr, bf->buf+bf->p, nm);
      ptr += nm;
      bf->p += nm;
      nb -= nm;
      nread += n;
      nmemb -= n;
    }
    if (bf->eof) break;    
    if (nb > 0) {
      memmove(bf->buf, bf->buf+bf->p, nb);    
    }
    bf->p = 0;
    bf->n = nb;
    if (bf->mr == 0) {
      nn = bf->nbuf - bf->n;
      nr = fread(bf->buf+bf->n, 1, nn, bf->f);
      if (nr < nn) {
	bf->eof = 1;
      }
      bf->n += nr;
    }
    MPI_Bcast(&bf->n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (bf->n > nb) {
      MPI_Bcast(bf->buf+nb, bf->n-nb, MPI_BYTE, 0, MPI_COMM_WORLD);
    }
    MPI_Bcast(&bf->eof, 1, MPI_INT, 0, MPI_COMM_WORLD);
    nb = bf->n - bf->p;
    n = nb/size;
  }
  return nread;
#else
  return fread(ptr, size, nmemb, bf->f);
#endif
}

char *BFileGetLine(char *s, int size1, BFILE *bf) {
#if USE_MPI == 1
  if (bf->nr <= 1) {
    return fgets(s, size1, bf->f);
  }
  int n;
  int size = size1-1;

  n = BFileRead(s, size, 1, bf);
  if (n == 1) {
    bf->p -= size;
  } else {
    size = bf->n-bf->p;
    if (size == 0) return NULL;
    memcpy(s, bf->buf+bf->p, size);
  }
  int i;
  for (i = 0; i < size; i++, bf->p++) {
    if (s[i] == '\n') {
      bf->p++;
      i++;
      break;
    }
  }
  if (bf->p == bf->nbuf) {
    bf->p = 0;
    bf->n = 0;
  }
  s[i] = '\0';
  return s;
#else
  return fgets(s, size1, bf->f);
#endif
}

void BFileRewind(BFILE *bf) {
#if USE_MPI == 1
  if (bf->nr <= 1) {
    rewind(bf->f);
    return;
  }
  bf->p = 0;
  bf->n = 0;
  bf->eof = 0;
  if (bf->mr == 0) rewind(bf->f);
#else
  rewind(bf->f);
#endif
}