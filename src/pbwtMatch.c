/*  File: pbwtMatch.c
 *  Author: Richard Durbin (rd@sanger.ac.uk)
 *  Copyright (C) Genome Research Limited, 2013-
 *-------------------------------------------------------------------
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *   http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------
 * Description: match functions in pbwt package
 * Exported functions:
 * HISTORY:
 * Last edited: Jan  2 10:14 2016 (rd)
 * * Jan  2 10:13 2016 (rd): start adding code for inexact match
 * Created: Thu Apr  4 11:55:48 2013 (rd)
 *-------------------------------------------------------------------
 */

#include "utils.h"
#include "pbwt.h"
#include "htslib/synced_bcf_reader.h"
#include <htslib/faidx.h>
#include <ctype.h>

/************ finding long (or maximal) matches within the set ************/

static Array matchLengthHist = 0 ;
static uchar **checkHapsA ;	/* section global for checking only */
static uchar **checkHapsB ;	/* section global for checking only */
static int Ncheck ;		/* section global for checking only */
static int LengthThreshold ;		/* section global for checking only */
static char *MatchOutputFileName;		/* section global for checking only */

static void checkMatchMaximal (uchar *x, uchar *y, int start, int end, int N)
{
  int i ;

  if (start && (x[start-1] == y[start-1]))
    die ("match not maximal - can extend backwards\n") ;
  if (end < N && (x[end] == y[end]))
    die ("match not maximal - can extend forwards\n") ;
  for (i = start ; i < end ; ++i)
    if (x[i] != y[i])
      die ("match not a match at %d\n", i) ;
}

static void reportMatch (int ai, int bi, int start, int end)
{
  if (start == end) return ;
  printf ("MATCH\t%d\t%d\t%d\t%d\t%d\n", ai, bi, start, end, end-start) ;

  /* following is text originally used for new sequence matching
  printf ("MATCH query %d to reference %d from %d to %d length %d\n",
          ai, bi, start, end, end - start) ;
  */

  if (isCheck)			/* check match is a real match and maximal */
    checkMatchMaximal (checkHapsA[ai], checkHapsB[bi], start, end, Ncheck) ;
}

static void matchLongWithin1 (PBWT *p, int T,
			      void (*report)(int ai, int bi, int start, int end))
/* algorithm 3 in paper */
{
  int *a = myalloc (p->M, int), *b = myalloc (p->M, int) ;
  int i, ia, ib, na = 0, nb = 0, k ;
  PbwtCursor *u = pbwtCursorCreate (p, TRUE, TRUE) ;

  for (k = 0 ; k <= p->N ; ++k)
    for (i = 0 ; i < u->M ; ++i)
      { if (u->d[i] > T)
	  { for (ia = 0 ; ia < na ; ++ia)
	      for (ib = 0 ; ib < nb ; ++ib) 
		(*report) (u->a[ia], u->a[ib], 0, k) ; /* 0 is wrong! - can't get start */
	    na = 0 ; nb = 0 ;	               /* NB because of this matches won't check */
	  }
	if (u->y[i] == 0)
	  a[na++] = u->a[i] ;
	else
	  b[nb++] = u->a[i] ;
      }
  
  free(a) ; free(b) ;  pbwtCursorDestroy (u) ;
}

static void matchLongWithin2 (PBWT *p, int T, 
			      void (*report)(int ai, int bi, int start, int end))
/* alternative giving start - it turns out in tests that this is also faster, so use it */
{
  int i, i0 = 0, ia, ib, na = 0, nb = 0, dmin, k ;
  PbwtCursor *u = pbwtCursorCreate (p, TRUE, TRUE) ;

  for (k = 0 ; k <= p->N ; ++k)
    { for (i = 0 ; i < u->M ; ++i)
	{ if (u->d[i] > k-T)
	    { if (na && nb)		/* then there is something to report */
		for (ia = i0 ; ia < i ; ++ia)
		  for (ib = ia+1, dmin = 0 ; ib < i ; ++ib)
		    { if (u->d[ib] > dmin) dmin = u->d[ib] ;
		      if (u->y[ib] != u->y[ia])
			(*report) (u->a[ia], u->a[ib], dmin, k) ;
		    }
	      na = 0 ; nb = 0 ; i0 = i ;
	    }
	  if (u->y[i] == 0)
	    na++ ;
	  else
	    nb++ ;
	}
      pbwtCursorForwardsReadAD (u, k) ;
    }

  pbwtCursorDestroy (u) ;
}

void matchMaximalWithin (PBWT *p, void (*report)(int ai, int bi, int start, int end))
/* algorithm 4 in paper */
{
  int i, j, k, m, n ;
  PbwtCursor *u = pbwtCursorCreate (p, TRUE, TRUE) ;

  for (k = 0 ; k <= p->N ; ++k)
    { for (i = 0 ; i < u->M ; ++i)
	{ m = i-1 ; n = i+1 ;
	  if (u->d[i] <= u->d[i+1])
	    while (u->d[m+1] <= u->d[i])
	      if (u->y[m--] == u->y[i] && k < p->N) goto nexti ;
	  if (u->d[i] >= u->d[i+1])
	    while (u->d[n] <= u->d[i+1])
	      if (u->y[n++] == u->y[i] && k < p->N) goto nexti ;
	  if (matchLengthHist)
	    ++array(matchLengthHist, (u->d[i]<u->d[i+1]) ? k-u->d[i] : k-u->d[i+1], int) ;
	  else
	    { for (j = m+1 ; j < i ; ++j) (*report) (u->a[i], u->a[j], u->d[i], k) ;
	      for (j = i+1 ; j < n ; ++j) (*report) (u->a[i], u->a[j], u->d[i+1], k) ;
	    }
	nexti: ;
	}
      pbwtCursorForwardsReadAD (u, k) ;
    }

  pbwtCursorDestroy (u) ;
}

/* I think there is a good alternative, where I just go down through the list, keeping
   track of some things, but it is not worked out yet, let alone implemented.
*/

void pbwtLongMatches (PBWT *p, int L) /* reporting threshold L - if 0 then maximal */
{
  int k ;
  PbwtCursor *u = pbwtCursorCreate (p, TRUE, TRUE) ;

  if (!p || !p->yz) die ("option -longWithin called without a PBWT") ;
  if (L < 0) die ("L %d for longWithin must be >= 0", L) ;

  if (isCheck) { checkHapsA = checkHapsB = pbwtHaplotypes(p) ; Ncheck = p->N ; }

  if (isStats)
    matchLengthHist = arrayReCreate (matchLengthHist, 1000000, int) ;

  if (L)
    matchLongWithin2 (p, L, reportMatch) ;
  else
    matchMaximalWithin (p, reportMatch) ;

  if (isStats)
    { int i, nTot = 0 ;
      long hTot = 0 ;

      for (i = 0 ; i < arrayMax(matchLengthHist) ; ++i)
	if (arr(matchLengthHist, i, int))
	  { nTot += arr(matchLengthHist, i, int) ;
	    hTot += arr(matchLengthHist, i, int) * i ;
	    printf ("%d\t%d\n", i, arr(matchLengthHist, i, int)) ;
	  }
      fprintf (logFile, "Average %.1f matches per sample\n", nTot/(double)p->M) ;
      fprintf (logFile, "Average length %.1f\n", hTot/(double)nTot) ;
    }

  pbwtCursorDestroy (u) ;
  if (isCheck) 
    { int j ;
    for (j = 0 ; j < p->M ; ++j)
        free (checkHapsA[j]) ;
        free (checkHapsA) ;
    }
}

/***************** match new sequences into a reference PBWT ******************/
/***************** this is quite complicated - so multiple implementations ****/

/* First implementation is without using PBWT - works on query and reference haplotypes.
   It should be O(NMQ) in time, O(NM) memory, ignoring query memory.
   NB This version only gives a representative maximal match for each query, start and end
   (the lowest reference index value).
*/

void matchSequencesNaive (PBWT *p, FILE *fp) /* fp is a pbwt file of sequences to match */
{
  PBWT *q = pbwtRead (fp) ;	/* q for "query" of course */
  uchar **query = pbwtHaplotypes (q) ; /* make the query sequences */
  uchar **reference = pbwtHaplotypes (p) ; /* haplotypes for reference */
  uchar *x, *y ;		      /* use for current query, y for current reference */
  int i, j, k, kLastMismatch, kk, iBest, N = p->N ;
  int *bestEnd = myalloc (N+1, int) ;
  int *bestSeq = mycalloc (N+1, int) ;
  int totLen = 0, nTot = 0 ;

  if (q->N != p->N) die ("query length in matchSequences %d != PBWT length %d", q->N, p->N) ;

  fprintf (logFile, "Made haplotypes: ") ; timeUpdate (logFile) ;

  if (isCheck) { checkHapsA = query ; checkHapsB = reference ; Ncheck = p->N ; }

  /* go query by query */
  for (j = 0 ; j < q->M ; ++j)
    { x = query[j] ;
      /* for each query go through targets in turn, updating bestMatch if available */
      memset (bestEnd, 0, N*sizeof(int)) ;
      bestEnd[N] = N+1 ;
      for (i = 0 ; i < p->M ; ++i)
	{ y = reference[i] ;
	  kLastMismatch = N ;	/* NB - never bigger than bestEnd[N] */
	  for (k = p->N ; k-- ; ) /* run backwards so we know end of match */
	    if (x[k] != y[k])
	      { if (kLastMismatch > bestEnd[k+1])
		  for (kk = k+1 ; bestEnd[kk] <= kLastMismatch ; ++kk)
		    { bestEnd[kk] = kLastMismatch ; bestSeq[kk] = i ; }
		kLastMismatch = k ; 
	      }
	  if (kLastMismatch > bestEnd[0])
	    for (kk = 0 ; bestEnd[kk] <= kLastMismatch ; ++kk)
	      { bestEnd[kk] = kLastMismatch ; bestSeq[kk] = i ; }
	}
      /* report best match and update stats */
      int iBest = p->M ;
      for (k = 0 ; k < p->N ; ++k)
	if (bestSeq[k] != iBest)
	  { iBest = bestSeq[k] ;
	    reportMatch (j, iBest, k, bestEnd[k]) ;
	    ++nTot ; totLen += bestEnd[k] - k ;
	  }
    }

  fprintf (logFile, "Average number of best matches %.1f, Average length %.1f\n", 
	   nTot/(double)q->M, totLen/(double)nTot) ;

  pbwtDestroy (q) ;
  for (j = 0 ; j < q->M ; ++j) free(query[j]) ; free (query) ;
  for (j = 0 ; j < p->M ; ++j) free(reference[j]) ; free (reference) ;
  free (bestSeq) ; free (bestEnd) ;
}


/* Next implementation is algorithm 4 for between group mathcing
*/

Array getSiteIndices (VCF *query, Array sites) {


    int iq = 0, is = 0;
    Site *sq = arrp(query->sites, iq, Site), *ss = arrp(sites, is, Site);
    Array Indices = arrayCreate(arrayMax(sites), int) ;
    int index=0;

    while (iq < query->N && is < arrayMax(sites))
    {
        if (sq->x < ss->x) {
            ++iq;
            ++sq;


        }
        else if (sq->x > ss->x)
        {
            ++is;
            ++ss;
        }
        else
        {

            char  *sq_alleles = dictName(variationDict, sq->varD) ;
            char *ss_alleles = dictName(variationDict, ss->varD) ;

            BOOL noAlt = sq_alleles[strlen(sq_alleles)-1] == '.' || ss_alleles[strlen(ss_alleles)-1] == '.';

            if (!noAlt && sq->varD < ss->varD)
            { ++iq ; ++sq ;
            }
            else if (!noAlt && sq->varD > ss->varD) { ++is ; ++ss ; }
            else
            {
                arr(Indices, index++, int)=iq;
                ++iq ; ++sq ;  ++is ; ++ss ;
            }

        }
    }
    return Indices;

}


void UpdateMatchOutFile (PBWT *p, char *filename)
{
    MatchOutputFileName = filename;
}





void UpdateThreshold (PBWT *p, int length)
{
    LengthThreshold = length;
}



void matchSequencesLong (PBWT *p, char *filename)
{



    FILE *fp ;
    int QueryLength = LengthThreshold ;

    int h=0;

    // Query VCF File

    VCF *query=myalloc (1, VCF);
    vcfHaplotypes(query, p, filename) ; /* make the query sequences */



    if(p->ProjectionList!=NULL)
    {



        for(int proj=0; proj<p->ProjectionList->max; proj++)
        {

            fprintf (logFile, "RUNNING NEW PROJECTION %d\n\n\n\n",proj);



            // Create new PBWT
            PBWT *p_proj=myalloc(1,PBWT);

            // Read Projection File
            if (!(fp = fopen (arr(p->ProjectionList,proj,char*),"r"))) die ("failed to open projection file %s", p->ProjectionList[proj]);
            char *chr = 0 ; Array sites = pbwtReadSitesFile (fp, &chr) ;
            if (strcmp (chr, p->chrom)) die ("chromosome mismatch in selectSites") ;


            // Scaffold current PBWT to new PBWT_PROJECTED

            p_proj = pbwtSelectSites (p, sites, TRUE) ;

            Array MyIndices = getSiteIndices(query, sites);







            // Reference PBWT File
                    uchar **reference = pbwtHaplotypes (p_proj) ; /* haplotypes for reference */

            uchar *x, *y ;                /* use for current query, and selected reference query */
            PbwtCursor *up = pbwtCursorCreate (p_proj, TRUE, TRUE) ;
            int **a, **d, **u ;		/* stored indexes; a for the Sort Order, d for the Divergence, u for the Count_O */
            int i, j, k, N = p_proj->N, M = p_proj->M ;
            int newVal = M-1 , lastLoc, matchStart, maxD, nextSeq;

            /* build indexes */

            a = myalloc (N+1,int*) ; for (i = 0 ; i < N+1 ; ++i) a[i] = myalloc (p_proj->M, int) ;
            d = myalloc (N+1,int*) ; for (i = 0 ; i < N+1 ; ++i) d[i] = myalloc (p_proj->M+1, int) ;
            u = myalloc (N,int*) ; for (i = 0 ; i < N ; ++i) u[i] = myalloc (p_proj->M+1, int) ;
            int *cc = myalloc (p_proj->N, int) ;
            int *tracker = myalloc (p_proj->N+1, int) ;
            x = myalloc(p_proj->N, uchar);

            for (k = 0 ; k < N ; ++k)
            {
                memcpy (a[k], up->a, M*sizeof(int)) ;
                memcpy (d[k], up->d, (M+1)*sizeof(int)) ;
                cc[k] = up->c ;
                pbwtCursorCalculateU (up) ;
                memcpy (u[k], up->u, (M+1)*sizeof(int)) ;
                pbwtCursorForwardsReadAD (up, k) ;
            }
            memcpy (a[k], up->a, M*sizeof(int)) ;
            memcpy (d[k], up->d, (M+1)*sizeof(int)) ;
            pbwtCursorDestroy (up) ;



            fprintf (logFile, "Made haplotypes and indices: ") ; timeUpdate (logFile) ;
            char *tag=malloc(10);
            sprintf(tag,"%d",proj+1);
            FILE *fp;

            if (!(fp = fopenTag (MatchOutputFileName,tag,"w"))) die ("failed to open %s file", MatchOutputFileName);

            /*thread each query in turn*/
            for(j=0; j<query->M; j++)
            {
                tracker[0]=M-1;
                for (k = 0 ; k < N ; ++k)
                    x[k] = query->hapData[j][arr(MyIndices,k,int)] ;
                newVal=M-1;

                for (k = 0 ; k < N ; ++k)
                {

                    lastLoc=newVal;
                    newVal = x[k]==0 ? u[k][lastLoc+1]-1 : cc[k] -1 + (lastLoc + 1 - u[k][lastLoc+1]);
                    tracker[k+1]=newVal;

                    // FIND D ABOVE

                    if(k + 1 < QueryLength)
                        continue;


                    matchStart = k;
                    y=reference[a[k+1][newVal]];
                    while(matchStart >= 0 &&  x[matchStart] == y[matchStart])
                        matchStart--;

                    maxD = matchStart+1;
                    nextSeq = newVal;

                    while(nextSeq >=0 &&  k-maxD+1 >= QueryLength) {
                        if (k==N-1 || x[k + 1] != reference[a[k + 1][nextSeq]][k + 1])
                            //fprintf (fp, "MATCH FOUND: %d and %d between Start=%d End=%d !!! Take that \n", j, a[k+1][nextSeq], maxD, k);
                            fprintf (fp, "%d %d %d %d\n", j, a[k+1][nextSeq], maxD, k);
                        if (maxD < d[k + 1][nextSeq]) maxD = d[k + 1][nextSeq];
                        nextSeq--;
                    }


                    // FIND D BELOW

                    if(newVal == M-1)
                        continue;

                    matchStart = k;
                    y=reference[a[k+1][newVal+1]];
                    while(matchStart >= 0 &&  x[matchStart] == y[matchStart])
                        matchStart--;

                    maxD = matchStart+1;
                    nextSeq = newVal+1;

                    while(nextSeq<M &&  k-maxD+1 >= QueryLength) {

                        if (k==N-1 || x[k + 1] != reference[a[k + 1][nextSeq]][k + 1])
                            //fprintf (fp, "MATCH FOUND: %d and %d between Start=%d End=%d !!! Take that \n", j, a[k+1][nextSeq], maxD, k);
                            fprintf (fp, "%d %d %d %d\n", j, a[k+1][nextSeq], maxD, k);

                        if(nextSeq==M-1)
                            break;

                        if (maxD < d[k + 1][nextSeq+1]) maxD = d[k + 1][nextSeq+1];
                        nextSeq++;
                    }

                }

            }



            /* cleanup */
            free (cc) ;
            free(tracker);
            fclose(fp);

            for (j = 0 ; j < p->M ; ++j) free(reference[j]) ; free (reference) ;
            for (j = 0 ; j < N ; ++j) free(a[j]) ; free (a) ;
            for (j = 0 ; j < N ; ++j) free(d[j]) ; free (d) ;
            for (j = 0 ; j < N ; ++j) free(u[j]) ; free (u) ;


                    int h=0;
                    free (chr) ;
                    arrayDestroy (sites) ;
        }




    }
    for (int j = 0 ; j < query->M ; ++j) free(query->hapData[j]) ; free (query->hapData) ;



}


/* Next implementation is algorithm 5 from the paper, precalculating indices in memory. 
   It should be O(NQ) time after O(NM) time index calculation. Downside is O(NM) memory,
   13NM bytes for now I think.  This can almost certainly be reduced with some work.
*/

void matchSequencesIndexed (PBWT *p, FILE *fp)
{
  PBWT *q = pbwtRead (fp) ;	/* q for "query" of course */
  uchar **query = pbwtHaplotypes (q) ; /* make the query sequences */
  uchar **reference = pbwtHaplotypes (p) ; /* haplotypes for reference */
  uchar *x, *y ;                /* use for current query, and selected reference query */
  PbwtCursor *up = pbwtCursorCreate (p, TRUE, TRUE) ;
  int **a, **d, **u ;		/* stored indexes */
  int e, f, g ;			/* start of match, and pbwt interval as in algorithm 5 */
  int e1, f1, g1 ;		/* next versions of the above, e' etc in algorithm 5 */
  int i, j, k, N = p->N, M = p->M ;
  int totLen = 0, nTot = 0 ;

  if (q->N != p->N) die ("query length in matchSequences %d != PBWT length %d", q->N, p->N) ;

  /* build indexes */

  a = myalloc (N+1,int*) ; for (i = 0 ; i < N+1 ; ++i) a[i] = myalloc (p->M, int) ;
  d = myalloc (N+1,int*) ; for (i = 0 ; i < N+1 ; ++i) d[i] = myalloc (p->M+1, int) ;
  u = myalloc (N,int*) ; for (i = 0 ; i < N ; ++i) u[i] = myalloc (p->M+1, int) ;
  int *cc = myalloc (p->N, int) ;
  for (k = 0 ; k < N ; ++k)
    { memcpy (a[k], up->a, M*sizeof(int)) ;
      memcpy (d[k], up->d, (M+1)*sizeof(int)) ;
      cc[k] = up->c ;
      pbwtCursorCalculateU (up) ;
      memcpy (u[k], up->u, (M+1)*sizeof(int)) ;
      pbwtCursorForwardsReadAD (up, k) ;
    }
  memcpy (a[k], up->a, M*sizeof(int)) ;
  memcpy (d[k], up->d, (M+1)*sizeof(int)) ;
  pbwtCursorDestroy (up) ;

  fprintf (logFile, "Made haplotypes and indices: ") ; timeUpdate (logFile) ;

  if (isCheck) { checkHapsA = query ; checkHapsB = reference ; Ncheck = p->N ; }

  /* match each query in turn */

  for (j = 0 ; j < q->M ; ++j)
    { x = query[j] ;
      e = 0 ; f = 0 ; g = M ;
      for (k = 0 ; k < N ; ++k)
	{               /* use classic FM updates to extend [f,g) interval to next position */
	  f1 = x[k] ? cc[k] + (f - u[k][f]) : u[k][f] ;
	  g1 = x[k] ? cc[k] + (g - u[k][g]) : u[k][g] ; 
	  		/* if the interval is non-zero we can just proceed */
	  if (g1 > f1)
	    { f = f1 ; g = g1 ; } /* no change to e */
	  else		/* we have reached a maximum - need to report and update e, f*,g* */
	    { for (i = f ; i < g ; ++i)		/* first report matches */
		reportMatch (j, a[k][i], e, k) ;
	      ++nTot ; totLen += k-e ;
	      		/* then update e,f,g */
	      e1 = d[k+1][f1] - 1 ; /* y[f1] and y[f1-1] diverge here, so upper bound for e */
	      if ((x[e1] == 0 && f1 > 0) || f1 == M)
		{ f1 = g1 - 1 ;
		  y = reference[a[k+1][f1]] ; while (x[e1-1] == y[e1-1]) --e1 ;
		  while (d[k+1][f1] <= e1) --f1 ;
		}
	      else if (f1 < M)
		{ g1 = f1 + 1 ;
		  y = reference[a[k+1][f1]] ; while (x[e1-1] == y[e1-1]) --e1 ;
		  while (g1 < M && d[k+1][g1] <= e1) ++g1 ;
		}
	      e = e1 ; f = f1 ; g = g1 ;
	    }
	}
      /* report the maximal matches to the end */
      for (i = f ; i < g ; ++i)
	reportMatch (j, a[k][i], e, k) ;
      ++nTot ; totLen += k-e ;
    }

  fprintf (logFile, "Average number of best matches %.1f, Average length %.1f\n", 
	   nTot/(double)q->M, totLen/(double)nTot) ;

  /* cleanup */
  free (cc) ;
  for (j = 0 ; j < q->M ; ++j) free(query[j]) ; free (query) ;
  pbwtDestroy (q) ;
  for (j = 0 ; j < p->M ; ++j) free(reference[j]) ; free (reference) ;
  for (j = 0 ; j < N ; ++j) free(a[j]) ; free (a) ;
  for (j = 0 ; j < N ; ++j) free(d[j]) ; free (d) ;
  for (j = 0 ; j < N ; ++j) free(u[j]) ; free (u) ;
}

/* Next is also based on algorithm 5, but applied in parallel to a set of sequences, and
   calculating indices on the fly from PBWT, so low memory for arbitrary reference size.
   It should be O(N(M+Q)) time but only O(N+M) memory.
*/

typedef struct {
  uchar *x ;
  int e, f, g ;			/* start of match, PBWT interval */
} MatchInfo ;

void matchSequencesDynamic (PBWT *p, FILE *fp)
{
  PBWT *q = pbwtRead (fp) ;	/* q for "query" of course */
  matchSequencesSweep (p, q, reportMatch) ;
  pbwtDestroy (q) ;
}

/* Simple and fast dynamic search - use also for imputation and painting.
   Just keep track of best match with its start. When it ends report and update.
*/

void matchSequencesSweep (PBWT *p, PBWT *q, void (*report)(int ai, int bi, int start, int end))
{
  if (q->N != p->N) die ("query length in matchSequences %d != PBWT length %d", q->N, p->N) ;
  PbwtCursor *up = pbwtCursorCreate (p, TRUE, TRUE) ;
  PbwtCursor *uq = pbwtCursorCreate (q, TRUE, TRUE) ;
  int *f = mycalloc (q->M, int) ; /* first location in *up of longest match to j'th query */
  int *d = mycalloc (q->M, int) ; /* start of longest match to j'th query */
  long totLen = 0, nTot = 0 ;

  if (isCheck) { checkHapsA = pbwtHaplotypes (q) ; checkHapsB = pbwtHaplotypes (p) ; Ncheck = p->N ; }

  int i, j, k ;
  for (k = 0 ; k < p->N ; ++k)
    { for (j = 0 ; j < q->M ; ++j)
	{ int jj = uq->a[j] ;
	  uchar x = uq->y[j] ;
	  if (up->y[f[jj]] != x)
	    { /* first see if there is any match of the same length that can be extended */
	      int iPlus = f[jj] ; /* is an index into *up greater than f[jj] */
	      while (++iPlus < p->M && up->d[iPlus] <= d[jj])
		if (up->y[iPlus] == x) { f[jj] = iPlus ; goto DONE ; }
	      /* if not, then report these matches */
	      for (i = f[jj] ; i < iPlus ; ++i) (*report) (jj, up->a[i], d[jj], k) ;
	      nTot += (iPlus - f[jj]) ; totLen += (k - d[jj])*(iPlus - f[jj]) ;
	      /* then find new top longest match that can be extended */
	      /* we extend out the interval [iMinus, iPlus] until we find this best match */
	      int iMinus = f[jj] ; /* an index into *up less than f[jj] */
	      int dPlus = (iPlus < p->M) ? up->d[iPlus] : k ;
	      int dMinus = up->d[iMinus] ;
	      while (TRUE)
		if (dMinus <= dPlus)
		  { i = -1 ;	/* impossible value */
		    while (up->d[iMinus] <= dMinus) /* up->d[0] = k+1 prevents underflow */
		      if (up->y[--iMinus] == x) i = iMinus ;
		    if (i >= 0) { f[jj] = i ; d[jj] = dMinus ; goto DONE ; }
		    dMinus = up->d[iMinus] ;
		  }
		else		/* dPlus < dMinus */
		  { while (iPlus < p->M && up->d[iPlus] <= dPlus)
		      if (up->y[iPlus] == x) { f[jj] = iPlus ; d[jj] = dPlus ; goto DONE ; }
		      else ++iPlus ;
		    dPlus = (iPlus == p->M) ? k : up->d[iPlus] ;
		    if (!iMinus && iPlus == p->M) 
		      { fprintf (logFile, "no match to query %d value %d at site %d\n", 
				 jj, x, k) ;
			d[jj] = k+1 ;
			goto DONE ; 
		      }
		  }
	    }
	DONE: ;
	}

      /* next update the match location f[] of each query */
      pbwtCursorCalculateU (up) ;
      for (j = 0 ; j < q->M ; ++j)
	{ int jj = uq->a[j] ;
	  f[jj] = pbwtCursorMap (up, uq->y[j], f[jj]) ;
	  /* trap if x == 1 and all up->y[] == 0, so d[jj] == k+1 (see above) */
	  if (f[jj] == p->M) f[jj] = 0 ; 
	}	  
	
      pbwtCursorForwardsReadAD (up, k) ;
      pbwtCursorForwardsRead (uq) ;
    }

  /* finally need to record the matches ending at p->N */
  for (j = 0 ; j < q->M ; ++j)
    { int jj = uq->a[j] ;
      (*report) (jj, up->a[f[jj]], d[jj], p->N) ;
      for (i = f[jj] ; ++i < p->M && up->d[i] <= d[jj] ; )
	(*report) (jj, up->a[i], d[jj], p->N) ;
      nTot += (i - f[jj]) ; totLen += (p->N - d[jj])*(i - f[jj]) ;
    }

  fprintf (logFile, "Average number of best matches including alternates %.1f, Average length %.1f, Av number per position %.1f\n", 
	   nTot/(double)q->M, totLen/(double)nTot, totLen/(double)(q->M*q->N)) ;

  pbwtCursorDestroy (up) ; pbwtCursorDestroy (uq) ;
  free (f) ; free (d) ;
}

/* the next version also reports sparse matches */
/* first split out the report and update section */

static int nSparseStore ;
static void (*sweepSparseReport)(int ai, int bi, int start, int end, BOOL isSparse) ;
static long totLen, nTot ;

static void reportAndUpdate (int j, int k, uchar x, PbwtCursor *up, int *f, int *d, BOOL isSparse)
/* problem that sparse arrays are only progressing at half the rate so k is too big */
{
  /* first see if there is any match of the same length that can be extended */
  int iPlus = f[j] ; /* is an index into *up greater than f[j] */
  while (++iPlus < up->M && up->d[iPlus] <= d[j])
    if (up->y[iPlus] == x) { f[j] = iPlus ; return ; }
  /* if not, then report these matches */
  int dj = isSparse ? (nSparseStore * d[j] + k % nSparseStore) : d[j] ;
  int i ; for (i = f[j] ; i < iPlus ; ++i) (*sweepSparseReport) (j, up->a[i], dj, k, isSparse) ;
  nTot += (iPlus - f[j]) ; totLen += (k - dj)*(iPlus - f[j]) ;

  if (isCheck && isSparse)	/* local sparse version of checkMatchMaximal() */
    for (i = f[j] ; i < iPlus ; ++i) 
      { uchar *x = checkHapsA[j], *y = checkHapsB[up->a[i]] ;
	if (dj >= nSparseStore && x[dj-nSparseStore] == y[dj-nSparseStore])
	  die ("match not maximal - can extend backwards") ;
	if (k < Ncheck && x[k] == y[k])
	  die ("match not maximal - can extend forwards") ;
	int ii ; for (ii = dj ; ii < k ; ii += nSparseStore)
		   if (x[ii] != y[ii]) die ("match not a match at %d", ii) ;
      }

  /* then find new top longest match that can be extended */
  /* we extend out the interval [iMinus, iPlus] until we find this best match */
  int iMinus = f[j] ; /* an index into *up less than f[j] */
  int dPlus = (iPlus < up->M) ? up->d[iPlus] : (isSparse ? k/nSparseStore : k) ;
  int dMinus = up->d[iMinus] ;
  while (TRUE)
    if (dMinus <= dPlus)
      { i = -1 ;	/* impossible value */
	while (up->d[iMinus] <= dMinus) /* up->d[0] = k+1 prevents underflow */
	  if (up->y[--iMinus] == x) i = iMinus ;
	if (i >= 0) { f[j] = i ; d[j] = dMinus ; return ; }
	dMinus = up->d[iMinus] ;
      }
    else		/* dPlus < dMinus */
      { while (iPlus < up->M && up->d[iPlus] <= dPlus)
	  if (up->y[iPlus] == x) { f[j] = iPlus ; d[j] = dPlus ; return ; }
	  else ++iPlus ;
	dPlus = (iPlus < up->M) ? up->d[iPlus] : (isSparse ? k/nSparseStore : k) ;
	if (!iMinus && iPlus == up->M) 
	  { fprintf (logFile, "no match to query %d value %d at site %d\n", j, x, k) ;
	    d[j] = 1 + (isSparse ? k/nSparseStore : k) ;
	    return ; 
	  }
      }
}

void matchSequencesSweepSparse (PBWT *p, PBWT *q, int nSparse,
	   void (*report)(int ai, int bi, int start, int end, BOOL isSparse))
{
  nSparseStore = nSparse ;
  sweepSparseReport = report ;
  if (q->N != p->N) die ("query length in matchSequences %d != PBWT length %d", q->N, p->N) ;
  PbwtCursor *up = pbwtCursorCreate (p, TRUE, TRUE) ;
  PbwtCursor *uq = pbwtCursorCreate (q, TRUE, TRUE) ;
  PbwtCursor **upp ;
  int *f = mycalloc (q->M, int) ; /* first location in *up of longest match to j'th query */
  int *d = mycalloc (q->M, int) ; /* start of longest match to j'th query */
  int **ff, **dd ;
  uchar *xp ;
  int kk ;
  if (nSparse > 1)
    { upp = myalloc (nSparse, PbwtCursor*) ; 
      ff = myalloc (nSparse, int*) ;
      dd = myalloc (nSparse, int*) ;
      for (kk = 0 ; kk < nSparse ; ++kk) 
	{ upp[kk] = pbwtNakedCursorCreate (p->M, 0) ;
	  ff[kk] = mycalloc (q->M, int) ;
	  dd[kk] = mycalloc (q->M, int) ;
	}
      xp = myalloc (p->M, uchar) ;
    }
  nTot = 0 ; totLen = 0 ;

  if (isCheck) { checkHapsA = pbwtHaplotypes (q) ; checkHapsB = pbwtHaplotypes (p) ; Ncheck = p->N ; }

  int i, j, k ;
  for (k = 0 ; k < p->N ; ++k)
    { if (nSparse > 1)
	{ kk = k % nSparse ;
	  for (j = 0 ; j < p->M ; ++j) xp[up->a[j]] = up->y[j] ;
	  for (j = 0 ; j < p->M ; ++j) upp[kk]->y[j] = xp[upp[kk]->a[j]] ;
	}

      /* first check if matches extend, and if not report and update f, d */
      for (j = 0 ; j < q->M ; ++j)
	{ int jj = uq->a[j] ;
	  uchar xq = uq->y[j] ;
	  if (up->y[f[jj]] != xq) /* this match ends here */
	    reportAndUpdate (jj, k, xq, up, f, d, FALSE) ;
	  if (nSparse > 1 && upp[kk]->y[ff[kk][jj]] != xq)
	    reportAndUpdate (jj, k, xq, upp[kk], ff[kk], dd[kk], TRUE) ;
	}
      /* next update the match location f[] of each query */
      pbwtCursorCalculateU (up) ;
      for (j = 0 ; j < q->M ; ++j)
	{ int jj = uq->a[j] ;
	  f[jj] = pbwtCursorMap (up, uq->y[j], f[jj]) ;
	  /* trap if x == 1 and all up->y[] == 0, so d[jj] == k+1 (see above) */
	  if (f[jj] == p->M) f[jj] = 0 ; 
	}

      if (nSparse > 1)		/* and the relevant sparse entry */
	{ pbwtCursorCalculateU (upp[kk]) ;
	  for (j = 0 ; j < q->M ; ++j)
	    { int jj = uq->a[j] ;
	      ff[kk][jj] = pbwtCursorMap (upp[kk], uq->y[j], ff[kk][jj]) ;
	      if (ff[kk][jj] == p->M) ff[kk][jj] = 0 ;
	    }
	  /* now move the sparse cursor forward, before updating the primary ones */
	  pbwtCursorForwardsAD (upp[kk], k/nSparse) ;
	}

	/* finish the main loop by moving the primary cursors forwards */
      pbwtCursorForwardsReadAD (up, k) ;
      pbwtCursorForwardsRead (uq) ;
    }

  /* finally need to record the matches ending at p->N */
  for (j = 0 ; j < q->M ; ++j)
    { int jj = uq->a[j] ;
      (*report) (jj, up->a[f[jj]], d[jj], p->N, FALSE) ;
      for (i = f[jj] ; ++i < p->M && up->d[i] <= d[jj] ; )
	(*report) (jj, up->a[i], d[jj], p->N, FALSE) ;
      nTot += (i - f[jj]) ; totLen += (p->N - d[jj])*(i - f[jj]) ;
    }

  if (nSparse > 1) 
    for (kk = 0 ; kk < nSparse ; ++kk)
      for (j = 0 ; j < q->M ; ++j)
	{ int jj = uq->a[j] ;
	  int dj = nSparse*dd[kk][jj] + kk ;
	  (*report) (jj, upp[kk]->a[ff[kk][jj]], dj, p->N, TRUE) ;
	  for (i = ff[kk][jj] ; ++i < p->M && upp[kk]->d[i] <= dd[kk][jj] ; )
	    (*report) (jj, upp[kk]->a[i], dj, p->N, TRUE) ;
	  nTot += (i - ff[kk][jj]) ; totLen += (p->N - dd[kk][jj])*(i - ff[kk][jj]) ;
	}

  fprintf (logFile, "Average number of best matches including alternates %.1f, Average length %.1f, Av number per position %.1f\n", 
	   nTot/(double)q->M, totLen/(double)nTot, totLen/(double)(q->M*q->N)) ;

  pbwtCursorDestroy (up) ; pbwtCursorDestroy (uq) ;
  free (f) ; free (d) ;
  if (nSparse > 1)
    { for (kk = 0 ; kk < nSparse ; ++kk)
	{ pbwtCursorDestroy (upp[kk]) ; free (ff[kk]) ; free (dd[kk]) ;	}
      free (upp) ; free (ff) ; free (dd) ; free (xp) ;
    }
}

/******************* end of file *******************/
