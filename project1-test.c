#include <stdio.h>
#include <stdlib.h>

#include "ggggc/gc.h"

GGC_TYPE(Node)
	GGC_MPTR(Node, prev);
	GGC_MPTR(Node, next);
	GGC_MDATA(long, val);
GGC_END_TYPE(Node,
	GGC_PTR(Node, prev)
	GGC_PTR(Node, next)
	)

static const int arraySize = 1000000;
static const int listLength = 250000;

int main(int argc, char* argv[])
{
	int i;
	int k;
	int n = 0;
	Node llh = NULL, llt = NULL, llc = NULL;
	GGC_double_Array arr = NULL;
	
	GGC_PUSH_4(arr, llh, llt, llc);


	llh = GGC_NEW(Node);
	GGC_WD(llh, val, 0);
	llt = llh;
	
	while (n < 8){
		arr = GGC_NEW_DA(double, arraySize);
		printf("Free array of length %d.\n", arraySize);

		for (i = 1; i < listLength; i++){
			llc = GGC_NEW(Node);
			GGC_WD(llc, val, i);
			GGC_WP(llt, next, llc);
			GGC_WP(llc, prev, llt);
			llt = llc;
		}
	

		for (i = 0; i < listLength; i++){
			llc = GGC_NEW(Node);
			k = i + listLength;
			GGC_WD(llc, val, k);
			GGC_WP(llt, next, llc);
			GGC_WP(llc, prev, llt);
			llt = llc;
		}
		n++;
		printf("Doubly linked list of length %d constructed.\n", listLength * n);
	}
	//GGC_WP(llt, next, NULL);

	
    return 0;
}
