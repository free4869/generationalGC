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
	Node llh = NULL, llt = NULL, llc = NULL;
	GGC_double_Array arr = NULL;
	
	GGC_PUSH_3(llh, llt, llc);

	//arr = GGC_NEW_DA(double, arraySize);

	//GGC_PUSH_3(llh, llt, llc);

	llh = GGC_NEW(Node);
	GGC_WD(llh, val, 0);
	//GGC_WP(llh, prev, NULL);
	llt = llh;

	for (i = 1; i < listLength; i++){
		llc = GGC_NEW(Node);
		GGC_WD(llc, val, i);
		GGC_WP(llt, next, llc);
		GGC_WP(llc, prev, llt);
		llt = llc;
	}
	//GGC_WP(llt, next, NULL);
	
	//arr = NULL;

	llc = GGC_NEW(Node);
	GGC_WD(llc, val, listLength);
	GGC_WP(llc, prev, llt);
	GGC_WP(llt, next, llc);
	llt = llc;
	for (i = 1; i < listLength; i++){
		llc = GGC_NEW(Node);
		GGC_WD(llc, val, i);
		GGC_WP(llt, next, llc);
		GGC_WP(llc, prev, llt);
		llt = llc;
	}
	//GGC_WP(llt, next, NULL);

	
    return 0;
}
