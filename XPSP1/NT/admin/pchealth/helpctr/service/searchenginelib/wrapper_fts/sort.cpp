#include "stdafx.h"

#define CUTOFF 8        // testing shows that this is good value

int __fastcall CompareIntValues(const void *pval1, const void *pval2)
{
    return ((SEARCH_RESULT *)pval2)->dwRank - ((SEARCH_RESULT *)pval1)->dwRank;
}

void __inline Swap(void* pb1, void* pb2, UINT width)
{
    BYTE  tmp[256];
    BYTE* tmp2 = NULL;
    BYTE* ptr;

    if(width > sizeof(tmp))
    {
        tmp2 = new BYTE[width]; if(!tmp2) return;

        ptr = tmp2;
    }
    else
    {
        ptr = tmp;
    }

    ::CopyMemory( ptr, pb1, width );
    ::CopyMemory( pb1, pb2, width );
    ::CopyMemory( pb2, ptr, width );

    delete [] tmp2;
}

static void InsertionSort(char *lo, char *hi, unsigned width,
    int (__fastcall *compare)(const void *, const void *))
{
    char *p, *max;

    /*
     * Note: in assertions below, i and j are alway inside original bound
     * of array to sort.
     */

    while (hi > lo) {
        /* A[i] <= A[j] for i <= j, j > hi */
        max = lo;
        for (p = lo + width; p <= hi; p += width) {
            /* A[i] <= A[max] for lo <= i < p */
            if (compare(p, max) > 0) {
                max = p;
            }
            /* A[i] <= A[max] for lo <= i <= p */
        }

        /* A[i] <= A[max] for lo <= i <= hi */

        Swap(max, hi, width);

        /* A[i] <= A[hi] for i <= hi, so A[i] <= A[j] for i <= j, j >= hi */

        hi -= width;

        // A[i] <= A[j] for i <= j, j > hi, loop top condition established
    }
    /* A[i] <= A[j] for i <= j, j > lo, which implies A[i] <= A[j] for i < j,
       so array is sorted */
}

void QSort(void *pbase, UINT num, UINT width,
    int (__fastcall *compare)(const void *, const void *))
{
    char *lo, *hi;              // ends of sub-array currently sorting
    char *mid;                  // points to middle of subarray
    char *loguy, *higuy;        // traveling pointers for partition step
    unsigned size;              // size of the sub-array
    char *lostk[30], *histk[30];
    int stkptr;         // stack for saving sub-array to be processed

    /* Note: the number of stack entries required is no more than
       1 + log2(size), so 30 is sufficient for any array */

    if (num < 2 || width == 0)
        return;                 // nothing to do

    stkptr = 0;                 // initialize stack

    lo = (char*) pbase;
    hi = (char *) pbase + width * (num-1);      // initialize limits

    /* this entry point is for pseudo-recursion calling: setting
       lo and hi and jumping to here is like recursion, but stkptr is
       prserved, locals aren't, so we preserve stuff on the stack */
recurse:

    size = (hi - lo) / width + 1;               // number of el's to sort

    // below a certain size, it is faster to use a O(n^2) sorting method

    if (size <= CUTOFF) {
         InsertionSort(lo, hi, width, compare);
    }
    else {
        /*
         * First we pick a partititioning element. The efficiency of the
         * algorithm demands that we find one that is approximately the
         * median of the values, but also that we select one fast. Using the
         * first one produces bad performace if the array is already sorted,
         * so we use the middle one, which would require a very wierdly
         * arranged array for worst case performance. Testing shows that a
         * median-of-three algorithm does not, in general, increase
         * performance.
         */

        mid = lo + (size / 2) * width;      // find middle element
        Swap(mid, lo, width);               // swap it to beginning of array

        /*
         * We now wish to partition the array into three pieces, one
         * consisiting of elements <= partition element, one of elements
         * equal to the parition element, and one of element >= to it. This
         * is done below; comments indicate conditions established at every
         * step.
         */

        loguy = lo;
        higuy = hi + width;

        /*
         * Note that higuy decreases and loguy increases on every
         * iteration, so loop must terminate.
         */

        for (;;) {
            /* lo <= loguy < hi, lo < higuy <= hi + 1,
               A[i] <= A[lo] for lo <= i <= loguy,
               A[i] >= A[lo] for higuy <= i <= hi */

            do  {
                loguy += width;
            } while (loguy <= hi && compare(loguy, lo) <= 0);

            /* lo < loguy <= hi+1, A[i] <= A[lo] for lo <= i < loguy,
               either loguy > hi or A[loguy] > A[lo] */

            do  {
                higuy -= width;
            } while (higuy > lo && compare(higuy, lo) >= 0);

            /* lo-1 <= higuy <= hi, A[i] >= A[lo] for higuy < i <= hi,
               either higuy <= lo or A[higuy] < A[lo] */

            if (higuy < loguy)
                break;

            /* if loguy > hi or higuy <= lo, then we would have exited, so
               A[loguy] > A[lo], A[higuy] < A[lo],
               loguy < hi, highy > lo */

            Swap(loguy, higuy, width);

            /* A[loguy] < A[lo], A[higuy] > A[lo]; so condition at top
               of loop is re-established */
        }

        /*     A[i] >= A[lo] for higuy < i <= hi,
               A[i] <= A[lo] for lo <= i < loguy,
               higuy < loguy, lo <= higuy <= hi
           implying:
               A[i] >= A[lo] for loguy <= i <= hi,
               A[i] <= A[lo] for lo <= i <= higuy,
               A[i] = A[lo] for higuy < i < loguy */

        Swap(lo, higuy, width);         // put partition element in place

        /* OK, now we have the following:
              A[i] >= A[higuy] for loguy <= i <= hi,
              A[i] <= A[higuy] for lo <= i < higuy
              A[i] = A[lo] for higuy <= i < loguy    */

        /* We've finished the partition, now we want to sort the subarrays
           [lo, higuy-1] and [loguy, hi].
           We do the smaller one first to minimize stack usage.
           We only sort arrays of length 2 or more.*/

        if ( higuy - 1 - lo >= hi - loguy ) {
            if (lo + width < higuy) {
                lostk[stkptr] = lo;
                histk[stkptr] = higuy - width;
                ++stkptr;
            }                           // save big recursion for later

            if (loguy < hi) {
                lo = loguy;
                goto recurse;           // do small recursion
            }
        }
        else {
            if (loguy < hi) {
                lostk[stkptr] = loguy;
                histk[stkptr] = hi;
                ++stkptr;               // save big recursion for later
            }

            if (lo + width < higuy) {
                hi = higuy - width;
                goto recurse;           // do small recursion
            }
        }
    }

    /*
     * We have sorted the array, except for any pending sorts on the
     * stack. Check if there are any, and do them.
     */

    --stkptr;
    if (stkptr >= 0) {
        lo = lostk[stkptr];
        hi = histk[stkptr];
        goto recurse;           // pop subarray from stack
    }
    else
        return;                 // all subarrays done
}
