#include "ggggc-internals.h"

/* publics */
struct GGGGC_PointerStack *ggggc_pointerStack, *ggggc_pointerStackGlobals;

/* internals */
struct GGGGC_Pool *ggggc_poolList;
struct GGGGC_FreeObject *ggggc_freeList;
int full;
struct GGGGC_Pool *ggggc_curPool;
struct GGGGC_Pool *ggggc_fromPool;
struct GGGGC_Pool *ggggc_curFromPool;
struct GGGGC_Pool *ggggc_toPool;
struct GGGGC_Pool *ggggc_curToPool;
struct GGGGC_Pool *ggggc_edenPoolList;
struct GGGGC_Pool *ggggc_curEdenPool;
struct GGGGC_OldGenPool *ggggc_oldGenPoolList;
struct GGGGC_OldGenPool *ggggc_curOldGenPool;
struct GGGGC_Descriptor *ggggc_descriptorDescriptors[GGGGC_WORDS_PER_POOL/GGGGC_BITS_PER_WORD+sizeof(struct GGGGC_Descriptor)];
