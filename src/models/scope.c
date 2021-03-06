#include "models/scope.h"
#include <string.h>

/* Set the Id of the function*/
void gs_scope_setId(long id, struct gs_scope * gss){
	gss->id = id;
	/*This function isn't really neccesary but to make outside source look
	 *uniform this goes a long way to readability
	*/
}

/* Note that only 8 characters will ever be copied from desc*/
void gs_scope_setDesc(char * desc, struct gs_scope * gss){
	bzero(gss->description,GS_SCOPE_DESCRIPTION_SIZE);
	strncpy(gss->description,desc,GS_SCOPE_DESCRIPTION_SIZE-1);
}

/* Clears out a scope structure to make it ready for use.*/
void gs_scope_ZeroStruct(struct gs_scope * gss){
	bzero(gss->description,GS_SCOPE_DESCRIPTION_SIZE);
	gss->id = GS_SCOPE_INVALID_ID; 
}