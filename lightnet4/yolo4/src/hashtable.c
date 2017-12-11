#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "kalmanbox.h"

#define SIZE 50

typedef struct DataItem {
   int data;
   int key;
   kalmanbox* element;
}DataItem;

//DataItem* hashArray[SIZE];
DataItem* dummyItem;
DataItem* item;

int hashCode(int key) {
   return key % SIZE;
}

DataItem* hashsearch(DataItem** hashArray, int key) {
   //get the hash
   int hashIndex = hashCode(key);
   int internalclock=0;

   //move in array until an empty

   while(internalclock!=SIZE) {

	  if(hashArray[hashIndex]!=NULL){
	      if(hashArray[hashIndex]->key == key){
	    	  return hashArray[hashIndex];
	      }
	  }

      //go to next cell
      ++hashIndex;

      //wrap around the table
      hashIndex %= SIZE;
      internalclock++;
   }
   printf("objectIndex %i not found!\n", key);
   assert(0 && "Hashtable Fails to Find the Element!\n");
   return NULL;
}

void hashinsert(DataItem** hashArray, int key, kalmanbox* element) {

   DataItem *item = (DataItem*) malloc(sizeof(DataItem));
   item->key = key;
   item->element=element;

   //get the hash
   int hashIndex = hashCode(key);

   //move in array until an empty or deleted cell
   while(hashArray[hashIndex] != NULL && hashArray[hashIndex]->key != -1) {
      //go to next cell
      ++hashIndex;

      //wrap around the table
      hashIndex %= SIZE;
   }

   hashArray[hashIndex] = item;
}

void hashUpdate(DataItem** hashArray, int objectIndex, kalmanbox* element){
	int key=objectIndex;
	int hashIndex=hashCode(key);
	int internalclock=0;

	while(internalclock!=SIZE){
		if(hashArray[hashIndex]!=NULL){
			if(hashArray[hashIndex]->key==objectIndex){
				hashArray[hashIndex]->element=element;
				return;
			}
		}

		++hashIndex;
	    hashIndex %= SIZE;
	    internalclock++;


	}


	assert(0 && "Hashtable Fails to Find the Element to Update!\n");
	return;
}

void hashdelete(DataItem** hashArray, int removedkey) {
   int key = removedkey;

   //get the hash
   int hashIndex = hashCode(key);
   int internalclock=0;

   //move in array until an empty
   while(internalclock!=SIZE) {

	  if(hashArray[hashIndex] != NULL){
	      if(hashArray[hashIndex]->key == key) {

	         //assign a dummy item at deleted position
	    	 free(hashArray[hashIndex]->element);
	    	 free(hashArray[hashIndex]);
	         hashArray[hashIndex] = dummyItem;
	         return;
	      }
	  }


      //go to next cell
      ++hashIndex;

      //wrap around the table
      hashIndex %= SIZE;
      internalclock++;
   }

   assert(0 && "Hashtable Fails to Deleting Elements!\n");
   return;
}

void hashdisplay(DataItem** hashArray) {
   int i = 0;
   printf("\t");
   for(i = 0; i<SIZE; i++) {

      if(hashArray[i] != NULL)
         printf(" (%d,%0.0f,%0.0f)",hashArray[i]->key,hashArray[i]->element->y_k->data.fl[0],hashArray[i]->element->y_k->data.fl[1]);
      else
         printf(" !~~! ");
   }

   printf("\n");
}
