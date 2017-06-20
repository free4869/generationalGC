/*
 * The collector
 *
 * Copyright (c) 2014, 2015 Gregor Richards
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#if defined(unix) || defined(__unix) || defined(__unix__) || \
    (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#if _POSIX_VERSION
#include <sys/mman.h>
#endif

/* REMOVE THIS FOR SUBMISSION! */
//#include <gc.h>
/* --- */

#include "ggggc/gc.h"
#include "ggggc-internals.h"

#ifdef __cplusplus
extern "C" {
#endif


#if defined(GGGGC_USE_MALLOC)
#define GGGGC_ALLOCATOR_MALLOC 1
#include "allocate-malloc.c"

#elif _POSIX_ADVISORY_INFO >= 200112L
#define GGGGC_ALLOCATOR_POSIX_MEMALIGN 1
#include "allocate-malign.c"

#elif defined(MAP_ANON)
#define GGGGC_ALLOCATOR_MMAP 1
#include "allocate-mmap.c"

#elif defined(_WIN32)
#define GGGGC_ALLOCATOR_VIRTUALALLOC 1
#include "allocate-win-valloc.c"

#else
#warning GGGGC: No allocator available other than malloc!
#define GGGGC_ALLOCATOR_MALLOC 1
#include "allocate-malloc.c"

#endif

void ggggc_fullCollect();

int inEdenGen(void* addr){
	struct GGGGC_Pool* poolIt;
	poolIt = ggggc_edenPoolList;
	while(poolIt){
		if (GGGGC_POOL_OF(addr) == poolIt)
			return 1;
		poolIt = poolIt->next;
	}
	return 0;
}

int inFromPools(void* addr){
	struct GGGGC_Pool* poolIt;
	poolIt = ggggc_fromPool;
	while(poolIt){
		if (GGGGC_POOL_OF(addr) == poolIt)
			return 1;
		poolIt = poolIt->next;
	}
	return 0;
}

int inOldGen(void* addr){
	struct GGGGC_Pool* poolIt;
	poolIt = ggggc_oldGenPoolList;
	while(poolIt){
		if (GGGGC_POOL_OF(addr) == poolIt)
			return 1;
		poolIt = poolIt->next;
	}
	return 0;
}


static struct GGGGC_Pool *newPool(int mustSucceed)
{
    struct GGGGC_Pool *ret;

    ret = NULL;
    if (!ret) ret = (struct GGGGC_Pool *) allocPool(mustSucceed);

    if (!ret) return NULL;

    /* set it up */
    ret->next = NULL;
    ret->free = ret->start;
    ret->end = (ggc_size_t *) ((unsigned char *) ret + GGGGC_POOL_BYTES);

    return ret;
}

void* ggggc_freeAlloc(struct GGGGC_Descriptor *descriptor){
	struct GGGGC_FreeObject *curFreeObj = ggggc_freeList;
	struct GGGGC_FreeObject *newFreeObj;
	struct GGGGC_FreeObject *prevFreeObj = NULL;
	struct GGGGC_Descriptor *fragDescriptor;
	ggc_size_t primitiveSize;
	ggc_size_t fragSize;
	ggc_size_t i;

	while (curFreeObj && (curFreeObj->header.descriptor__ptr->size < descriptor->size)){
		prevFreeObj = curFreeObj;
		curFreeObj = curFreeObj->next;
	}	
	if (curFreeObj){
		primitiveSize = curFreeObj->header.descriptor__ptr->size;
		fragSize = primitiveSize -  descriptor->size;
		//add a new descriptor for the generated fragment.
		if (prevFreeObj == NULL)
		{
			if (fragSize == 0){
				ggggc_freeList = curFreeObj->next;
				((struct GGGGC_Descriptor*) curFreeObj)->header.descriptor__ptr = descriptor;
			}
			else if (fragSize == 1){
				ggggc_freeList = curFreeObj->next;
				((struct GGGGC_Descriptor*) curFreeObj)->header.descriptor__ptr = descriptor;
			}
			else{
				fragDescriptor = ggggc_allocateDescriptor(fragSize, 1);
				newFreeObj = (struct GGGGC_FreeObject*)((ggc_size_t*)curFreeObj + descriptor->size);
				ggggc_freeList->next = newFreeObj;
				newFreeObj->next = curFreeObj->next;
				newFreeObj->header.descriptor__ptr = fragDescriptor;
			}
		}
		else
		{
			if (fragSize < 2){
				prevFreeObj->next = curFreeObj->next;
				((struct GGGGC_Descriptor*) curFreeObj)->header.descriptor__ptr = descriptor;
			}
			else{
				fragDescriptor = ggggc_allocateDescriptor(fragSize, 1);
				newFreeObj = (struct GGGGC_FreeObject*)((ggc_size_t*)curFreeObj + descriptor->size);
				prevFreeObj->next = newFreeObj;
				newFreeObj->next = curFreeObj->next;
				newFreeObj->header.descriptor__ptr = fragDescriptor;
			}
		}
		for (i = 1; i < primitiveSize; i++) 
			((ggc_size_t*)curFreeObj)[i] = 0;
		 
		return curFreeObj;
	}

	return NULL;
}



struct GGGGC_Descriptor* forward(void* addr){
	struct GGGGC_Descriptor *desc;
	void *forwardedAddr;
	struct GGGGC_Pool *poolIt;
	desc = *((struct GGGGC_Descriptor**)addr);

	if (inEdenGen(addr)){
		if (desc->size > ggggc_curToPool->end - ggggc_curToPool->free)
		{
			if (!ggggc_curToPool->next)
			{
				ggggc_curToPool->next = newPool(1);
				poolIt = ggggc_fromPool;
				while(poolIt->next)
					poolIt = poolIt->next;
				poolIt->next = newPool(1);
			}

			ggggc_curToPool = ggggc_curToPool->next;
		}

		forwardedAddr = ggggc_curToPool->free;
		ggggc_curToPool->free = ggggc_curToPool->free + desc->size;
		memcpy(forwardedAddr, addr, desc->size * sizeof(ggc_size_t));
	}
	else if (inFromPools(addr)){
		void *ret = ggggc_freeAlloc(desc);
		if (ret){
			forwardedAddr = (void*)(((ggc_size_t)forwardedAddr) | (ggc_size_t)2);
			*((void**)addr) = forwardedAddr;
			return ret;
		}
		
		if (desc->size > ggggc_curOldGenPool->end - ggggc_curOldGenPool->free)
			ggggc_curOldGenPool = ggggc_curOldGenPool->next;
		if (!ggggc_curOldGenPool){
			ggggc_fullCollect();
			ret = ggggc_freeAlloc(desc);
			forwardedAddr = (void*)(((ggc_size_t)forwardedAddr) | (ggc_size_t)2);
			*((void**)addr) = forwardedAddr;
			return ret;
		}

		forwardedAddr = ggggc_curOldGenPool->free;
		ggggc_curOldGenPool->free = ggggc_curOldGenPool->free + desc->size;
		memcpy(forwardedAddr, addr, desc->size * sizeof(ggc_size_t));
	}
	forwardedAddr = (void*)(((ggc_size_t)forwardedAddr) | (ggc_size_t)2);
	*((void**)addr) = forwardedAddr;
	return forwardedAddr;
}

int alreadyForwarded(void* addr){
	void *forwardedAddr;
	forwardedAddr = *((void**)addr);
	if ((ggc_size_t)forwardedAddr & 2)
		return 1;

	return 0;
}

void copyFromRoot(void* ptr){
	struct GGGGC_Descriptor *desc;
	void *offset;
	void *objPtr;
	void *forwardedAddr;
	void *childAddr;
	
	objPtr = *((void**) ptr);
	if (!objPtr)
		return;
	if (inOldGen(objPtr))
		return;
	if (alreadyForwarded(objPtr)){
		forwardedAddr = *((void **)objPtr);
		forwardedAddr = (void*)(((ggc_size_t)forwardedAddr) - 2);
		*(void**)ptr = forwardedAddr;
		return;
	}
	forwardedAddr = forward(objPtr);
	forwardedAddr = (void*)(((ggc_size_t)forwardedAddr) - 2);
	*(void**)ptr = forwardedAddr;
	objPtr = *((void**) ptr);
	void *stack[1024 * 512];
	ggc_size_t bottom = 0;
	ggc_size_t top = 1;
	stack[0] = objPtr;
	while (top - bottom != 0){
		ggc_size_t i;
		top--;
		objPtr = stack[top];
		desc = *((struct GGGGC_Descriptor**)objPtr);
		if (!desc)
			return;
		ggc_size_t size = desc->size;
		ggc_size_t mask = 1;
		ggc_size_t bitmap = desc->pointers[0];

		for (i = 0; i < size; i++){
			if ((bitmap & mask) != 0){
				offset = (void*)((ggc_size_t*)objPtr + i);
				childAddr = *((void**)offset);
				if (childAddr && !inOldGen(childAddr)){
					if (alreadyForwarded(childAddr)){
						forwardedAddr = *((void**)childAddr);
						forwardedAddr = (void*)(((ggc_size_t)forwardedAddr) - 2);
						*(void**)offset = forwardedAddr;
					}
					else{	
						forwardedAddr = forward(childAddr);
						forwardedAddr = (void*)(((ggc_size_t)forwardedAddr) - 2);
						*(void**)offset = forwardedAddr;
						stack[top] = forwardedAddr;
						top++;
					}
					if (inOldGen(objPtr) && !inOldGen(forwardedAddr)){
						((struct GGGGC_OldGenPool*)GGGGC_POOL_OF(objPtr))->remember[GGGGC_CARD_OF(objPtr)] = 1;
					}
				}
			}
			mask = mask<<1;
		}	
	}
}

void exchangePool(){
	struct GGGGC_Pool* temp;
	struct GGGGC_Pool* poolIt;
	ggc_size_t* start;
	ggc_size_t* end;
	
	temp = ggggc_fromPool;
	ggggc_fromPool = ggggc_toPool;
	ggggc_toPool = temp;
	ggggc_curFromPool = ggggc_curToPool;
	ggggc_curToPool = ggggc_toPool;
}


void copyPhase(){
	struct GGGGC_PointerStack *curSt = ggggc_pointerStack;
	void* ptr;
	void* obj;
	struct GGGGC_OldGenPool *poolIt = ggggc_oldGenPoolList;
	ggc_size_t i;
	while(curSt){
		for (i = 0; i < curSt->size; i++){
			ptr = (curSt->pointers)[i];
			copyFromRoot(ptr);
		}
		curSt = curSt->next;
	}

	while(poolIt){
		for (i = 0; i < GGGGC_CARDS_PER_POOL; i++){
			if (poolIt->remember[i]){
				ptr = poolIt + i * GGGGC_CARD_SIZE + poolIt->firstObjs[i];
				while(GGGGC_CARD_OF(ptr) == i){
					obj = *((void**)ptr);
					if (!inOldGen(obj)){
						copyFromRoot(ptr);
						ptr += ((struct GGGGC_Descriptor*)obj)->header.descriptor__ptr->size;
					}
				}
			}
		}
		poolIt = poolIt->next;
	}
	exchangePool();
}



int isMarked(struct GGGGC_Header *header){
	return (ggc_size_t)header->descriptor__ptr & 1;
}

void mark(struct GGGGC_Header *header){
	header->descriptor__ptr = (struct GGGGC_Descriptor*)((ggc_size_t)header->descriptor__ptr | 1);
}

void markRoot(void* ptr){
	struct GGGGC_Descriptor *desc;
	void *childPtr;
	void *objPtr;
	void *offset;
	struct GGGGC_Header* objHeader;
	struct GGGGC_Header* childHeader;
	
	objPtr = *((struct GGGGC_Descriptor**) ptr);
	if (!objPtr)
		return;
	desc = *((struct GGGGC_Descriptor**)objPtr);
	if (!desc)
		return;	

	void *stack[1024 * 512];
	ggc_size_t bottom = 0;
	ggc_size_t top = 1;
	stack[0] = objPtr;
	while (top - bottom != 0){
		ggc_size_t i;
		top--;
		objPtr = stack[top];
		if ((ggc_size_t)objPtr & ((ggc_size_t)1))
			objPtr = (void*)((ggc_size_t)objPtr - 1); 
		desc = *((struct GGGGC_Descriptor**)objPtr);
		if (!desc)
			continue;
		ggc_size_t size = desc->size;
		ggc_size_t mask = 1;
		ggc_size_t bitmap = desc->pointers[0];
		objHeader = (struct GGGGC_Header*)objPtr;

		if (!isMarked(objHeader)){
			for (i = 0; i < size; i++){
				if ((bitmap & mask) != 0){
					offset = (ggc_size_t*)objPtr + i;
					childPtr = *((struct GGGGC_Descriptor**)offset);
					childHeader = (struct GGGGC_Header*)childPtr;
					if (childPtr && !isMarked(childHeader)){
						stack[top] = childPtr;
						top++;
					}
				}
				mask = mask<<1;
			}
			mark((struct GGGGC_Header*)objPtr);
		}
	}
}


void markPhase(){
	struct GGGGC_PointerStack *curSt = ggggc_pointerStack;
	ggc_size_t i;
	void* ptr;
	while(curSt){
		for (i = 0; i < curSt->size; i++){
			ptr = (curSt->pointers)[i];
			markRoot(ptr);
		}
		curSt = curSt->next;
	}
}

void sweepPhase(){
	ggc_size_t* start;
	ggc_size_t* end;
	ggc_size_t survivors;
	if (!ggggc_curOldGenPool) 
		return;
	ggggc_freeList = NULL;
	
	struct GGGGC_Descriptor *desc;
	
	struct GGGGC_Pool *poolIt = ggggc_oldGenPoolList;
	while (poolIt){
		start = &poolIt->start;
 		end = poolIt->free;
		survivors = 0;

		while (start < end){
			desc = *((struct GGGGC_Descriptor**) start);
			while(!desc){
				start += 1;
				desc = *((struct GGGGC_Descriptor**) start);
			}
			if (isMarked((struct GGGGC_Header*)start)){
				desc = (struct GGGGC_Descriptor *)((ggc_size_t)desc  - 1);

				((struct GGGGC_Descriptor*) start)->header.descriptor__ptr = desc;
				survivors += desc->size;
			} 
			else{ 
				((struct GGGGC_FreeObject*) start)->next = ggggc_freeList;
				ggggc_freeList = (struct GGGGC_FreeObject*)start;
			}
			start += desc->size;	
		}
		poolIt->survivors = survivors;
		poolIt = poolIt->next;
	}
}


/* run a collection */
void ggggc_fullCollect(){
	markPhase();
	sweepPhase();
}

void ggggc_collect()
{
	copyPhase();
}

/* explicitly yield to the collector */
int ggggc_yield()
{
	if (full){
		ggggc_collect();
		full = 0;
	}
	return 0;
}
#ifdef __cplusplus
}
#endif
