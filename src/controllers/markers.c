#include "controllers/markers.h"

static inline int min(const int a, const int b){
	return a < b ? a : b;
}

int marker_controller(const struct http_request * request, char * stringToReturn, int strLength){
	fprintf(stderr, "Working with %p %s %d", (void*)request, stringToReturn, strLength);
	int status;
	int buffSize;
	int numParams;
	int page;
	long id; 
	char * buffer; 
	char tempBuf[40];
	char **convertSuccess;
	Decimal * latDegrees;
	Decimal * lonDegrees;
	Decimal * latOffset;
	Decimal * lonOffset;
	StrMap * sm;

	status = 503;
	convertSuccess = NULL;
	buffSize = (MARKER_LIMIT * sizeof(struct gs_marker))*4+1+(2*MAX_URL_LENGTH);
	bzero(tempBuf, sizeof tempBuf);
	page = 1;

	/*Buffer up a good size that will probably not get filled (hopefully)*/
	buffer = malloc(buffSize);
	if(buffer == NULL){
		status = 500;
		goto mc_nomem;
	}
	buffer = memset(buffer,0,buffSize);

	latDegrees = malloc(sizeof(Decimal));
	if (latDegrees == NULL) {
		free(buffer);
		status = 500;
		goto mc_nomem;
	} else {
		latDegrees = memset(latDegrees,0,sizeof(Decimal));
	}
	lonDegrees = malloc(sizeof(Decimal));
	if (lonDegrees == NULL) {
		free(buffer); free(latDegrees);		
		status = 500;
		goto mc_nomem;
	} else {
		lonDegrees = memset(lonDegrees,0,sizeof(Decimal));
	}
	latOffset = malloc(sizeof(Decimal));
	if (latOffset == NULL) {
		free(buffer); free(latDegrees); free(lonDegrees);
		status = 500;
		goto mc_nomem;
	} else {
		latOffset = memset(latOffset,0,sizeof(Decimal));
	}
	lonOffset = malloc(sizeof(Decimal));
	if (latDegrees == NULL) {
		free(buffer); free(latDegrees); free(lonDegrees); free(latOffset);
		status = 500;
		goto mc_nomem;
	} else {
		latDegrees = memset(latDegrees,0,sizeof(Decimal));
	}

	sm = sm_new(HASH_TABLE_CAPACITY);
	if(sm == NULL){
		status = 500;
		free(buffer); free(latDegrees); free(lonDegrees); free(latOffset); free(lonOffset);
		goto mc_nomem;
	}

	numParams = parseURL(request->url, strlen(request->url), sm);
	if(numParams > 0){
		/* Collect any parameters and convert them to the proper types */
		if (sm_exists(sm,"latdegrees")==1) {
			sm_get(sm,"latdegrees",tempBuf,sizeof tempBuf);
			(*latDegrees) = createDecimalFromString(tempBuf);
			/* Offsets only make sense if their parent coordinate is existent */
			if (sm_exists(sm,"latoffset")==1) {
				sm_get(sm,"latoffset", tempBuf, sizeof tempBuf);
				/* Validate the numericness of the offset */
				if(strtod(tempBuf,convertSuccess) != 0 && convertSuccess == NULL)
					(*latOffset) = createDecimalFromString(tempBuf);
				else{
					sm_delete(sm);
					free(buffer); free(latDegrees); free(lonDegrees); free(latOffset); free(lonOffset);
					goto mc_badLatOffset;
				} 
			} else {
				(*latOffset) = createDecimalFromString(DEFAULT_OFFSET);
			}
		} else{
			/* Sensible default for latdegrees is... to become NULL 
			 * so that we can handle using a different query without 
			 * relying on magic numbers of any kind. 
			*/
			free(latDegrees);
			latDegrees = NULL;
		}
		if (sm_exists(sm,"londegrees")==1) {
			sm_get(sm,"londegrees",tempBuf,sizeof tempBuf);
			(*lonDegrees) = createDecimalFromString(tempBuf);
			if (sm_exists(sm,"lonoffset")==1) {
				sm_get(sm,"lonoffset", tempBuf, sizeof tempBuf);
				if(strtod(tempBuf,convertSuccess) != 0 && convertSuccess == NULL)
					(*lonOffset) = createDecimalFromString(tempBuf);
				else{
					sm_delete(sm);
					free(buffer); free(latDegrees); free(lonDegrees); free(latOffset); free(lonOffset);
					goto mc_badLonOffset;
				} 
			} else {
				(*lonOffset) = createDecimalFromString(DEFAULT_OFFSET);
			}
		} else {
			free(lonDegrees);
			lonDegrees = NULL;
		}

		if(sm_exists(sm, "page") == 1)
			if(sm_get(sm, "page", tempBuf, sizeof tempBuf) == 1){
				page = atoi(tempBuf);
				if(page <= 0){
					status = 400;		
					sm_delete(sm);
					free(buffer); free(latDegrees); free(lonDegrees); free(latOffset); free(lonOffset);	
					goto mc_badpage;
				}

			}

	}else{
		/* We've got no parameters so free up the memory we don't need */\
		free(latDegrees);
		free(lonDegrees);
		free(latOffset);
		free(lonOffset);
		latDegrees = NULL;
		lonDegrees = NULL;
		latOffset  = NULL;
		lonOffset  = NULL;
	}
	/* Check Method and fly off the handle */
	switch(request->method){
		case GET:
			/* GET is the painful one for this controller:
			 * It takes _optional_ parameters, which means modifying our querying
			 * based on the parameters we have. 
			*/
			if((sm_exists(sm,"latoffset")==1) ^ (sm_exists(sm,"lonoffset")==1)){
				/*Err! if one is used, both must be used! */
				sm_delete(sm);
				free(buffer);
				if(latDegrees != NULL)
					free(latDegrees); 
				if(lonDegrees != NULL)
					free(lonDegrees); 
				if(latOffset != NULL)
					free(latOffset); 
				if(lonOffset != NULL)
					free(lonOffset);
				status = 422;
				goto mc_bothOffsets;
			}
			if(latDegrees != NULL)
				/*Let the -90.1 slide by as ok...*/
				if(*latDegrees < -90L || *latDegrees > 90L){
					sm_delete(sm);
					free(buffer);
					free(latDegrees); 
					if(lonDegrees != NULL)
						free(lonDegrees); 
					free(latOffset); 
					if(lonOffset != NULL)
						free(lonOffset);
					status = 422;
					goto mc_bothOffsets;		
				}

			if(lonDegrees != NULL)
				if(*lonDegrees < -180L || *lonDegrees > 180L){
					sm_delete(sm);
					free(buffer); 
					if(latDegrees != NULL)
						free(latDegrees); 
					if(lonDegrees != NULL)
						free(lonDegrees); 
					if(latOffset != NULL)
						free(latOffset); 
					if(lonOffset != NULL)
						free(lonOffset);
					status = 422;
					goto mc_bothOffsets;			
				}
			if(sm_exists(sm,"id")!=1){
				/* Retrieve multiple markers */
				status = marker_get(buffer,buffSize,latDegrees,lonDegrees,latOffset,lonOffset,page);
			}else{
				sm_get(sm, "id", tempBuf, sizeof tempBuf);
				id = atol(tempBuf);	
				status = marker_get_single(buffer, buffSize, id);
			}
			

			break;
		case POST:
			fprintf(stderr, "%s\n", "processing as post");
			status = marker_post(buffer,buffSize,request);
			if(status == -1){
				/* Something went terribly wrong */
				sm_delete(sm);
				free(buffer); 
				if(latDegrees != NULL)
					free(latDegrees); 
				if(lonDegrees != NULL)
					free(lonDegrees); 
				if(latOffset != NULL)
					free(latOffset); 
				if(lonOffset != NULL)
					free(lonOffset);
				goto mc_nomem;
			}
			break;
		case PUT:
			if(sm_exists(sm,"id")!=1){
				status = 400;
				sm_delete(sm);
				free(buffer); 
				if(latDegrees != NULL)
					free(latDegrees); 
				if(lonDegrees != NULL)
					free(lonDegrees); 
				if(latOffset != NULL)
					free(latOffset); 
				if(lonOffset != NULL)
					free(lonOffset);
				goto mc_missing_key;
			}
			sm_get(sm, "id", tempBuf, sizeof tempBuf);
			id = atol(tempBuf);
			status = marker_address(buffer,buffSize,id,request);
			break;
		case DELETE:
			if(sm_exists(sm,"id")!=1){
				status = 400;
				sm_delete(sm);
				free(buffer); 
				if(latDegrees != NULL)
					free(latDegrees); 
				if(lonDegrees != NULL)
					free(lonDegrees); 
				if(latOffset != NULL)
					free(latOffset); 
				if(lonOffset != NULL)
					free(lonOffset);
				goto mc_missing_key;
			}
			sm_get(sm, "id", tempBuf, sizeof tempBuf);
			id = atol(tempBuf);
			status = marker_delete(buffer, buffSize, id);
			if(status == -1){
				sm_delete(sm);
				free(buffer); 
				if(latDegrees != NULL)
					free(latDegrees); 
				if(lonDegrees != NULL)
					free(lonDegrees); 
				if(latOffset != NULL)
					free(latOffset); 
				if(lonOffset != NULL)
					free(lonOffset);
				goto mc_nomem;	
			}
			break;
		default:
			status = 501;	
			free(buffer); 
			if(latDegrees != NULL)
				free(latDegrees); 
			if(lonDegrees != NULL)
				free(lonDegrees); 
			if(latOffset != NULL)
				free(latOffset); 
			if(lonOffset != NULL)
				free(lonOffset);
			sm_delete(sm);
			goto mc_unsupportedMethod;
	}

	snprintf(stringToReturn, strLength, "%s", buffer);
	free(buffer); 
	if(latDegrees != NULL)
		free(latDegrees); 
	if(lonDegrees != NULL)
		free(lonDegrees); 
	if(latOffset != NULL)
		free(latOffset); 
	if(lonOffset != NULL)
		free(lonOffset);
	sm_delete(sm);	
	return status;

	mc_nomem:
		snprintf(stringToReturn, strLength, ERROR_STR_FORMAT, 500, NOMEM_ERROR);
		return status;

	mc_unsupportedMethod:
		snprintf(stringToReturn, strLength, ERROR_STR_FORMAT, status, BAD_METHOD_ERR);
		return status;		
	
	mc_missing_key:
		snprintf(stringToReturn, strLength, ERROR_STR_FORMAT, status, MISSING_ID_KEY);
		return status;	

	mc_badLonOffset:
		snprintf(stringToReturn, strLength, ERROR_STR_FORMAT, status, BAD_LON_OFFSET);
		return status;		
	
	mc_badLatOffset:
		snprintf(stringToReturn, strLength, ERROR_STR_FORMAT, status, BAD_LAT_OFFSET);
		return status;		

	mc_bothOffsets:
		snprintf(stringToReturn, strLength, ERROR_STR_FORMAT, status, BOTH_OFFSET_ERR);
		return status;

	mc_badpage:
		snprintf(stringToReturn, strLength, ERROR_STR_FORMAT, status, BAD_PAGE_ERR);
		return status;	


}

int marker_delete(char * buffer, int buffSize, long id){
	MYSQL *conn;
	long affected; 

	mysql_thread_init();
	conn = _getMySQLConnection();
	if(!conn){
		mysql_thread_end();
		fprintf(stderr, "%s\n", "Could not connect to mySQL on worker thread");
		return -1;
	}	

	affected = db_deleteMarker(id,conn);

	mysql_close(conn);
	mysql_thread_end();

	if(affected > 0){
		snprintf(buffer,buffSize,"{\"status_code\" : 204,\"message\" : \"Successfuly deleted marker\"}");
		return 204;
	} else {
		snprintf(buffer,buffSize,"{\"status_code\" : 404,\"message\" : \"Could not find marker with given id\"}");
		return 404;
	}
}

int marker_address(char * buffer, int buffSize, long id, const struct http_request * request){
	MYSQL *conn;
	long affected; 
	int addressed;
	char * charP; 
	char * boolVal;

	affected = addressed =  0;
	fprintf(stderr, "%ld %ld\n", affected, id);

	/* Retrieve the put data first */
	if(request->contentLength > 0){
		charP = strstr(request->data, "addressed");
		if(charP == NULL){
			snprintf(buffer, buffSize,ERROR_STR_FORMAT,422,NO_ADDRESSED_KEY);
			return 422;
		} else {
			/* Key was found, skip the field name and find the value*/
			fprintf(stderr, "%s\n", charP);
			charP+=9;
			fprintf(stderr, "%s\n", charP);
			boolVal = strstr(charP, "true");
			if(boolVal == NULL){
				boolVal = strstr(charP, "false");
				if(boolVal == NULL){
					snprintf(buffer, buffSize,ERROR_STR_FORMAT,422,NO_ADDRESSED_KEY);
					return 422;	
				}else{
					addressed = ADDRESSED_FALSE;
				}	
			}else{
				addressed = ADDRESSED_TRUE;
			}
		}
	}else{
		/* No put data bad request */
		snprintf(buffer, buffSize, ERROR_STR_FORMAT,400, NO_PUT_DATA);
		return 400;
	}
	

	mysql_thread_init();
	conn = _getMySQLConnection();
	if(!conn){
		mysql_thread_end();
		fprintf(stderr, "%s\n", "Could not connect to mySQL on worker thread");
		return -1;
	}	

	/* Mark addressed */	
	affected = db_addressMarker(id, addressed, conn);	

	mysql_close(conn);
	mysql_thread_end();

	if(affected > 0){
		snprintf(buffer, buffSize, "\"status_code\" : 200,\"message\":\"successful update\"");
	}else{
		snprintf(buffer,buffSize,"{\"status_code\" : 404,\"message\" : \"Could not find marker with given id\"}");
		return 404;
	}

	return 200;
}

int marker_post(char * buffer, int buffSize, const struct http_request * request){
	MYSQL *conn;
	struct gs_marker marker;
	struct gs_comment assocComment;
	StrMap * sm;
	int i;
	int j;
	int strFlag;
	char keyBuffer[GS_COMMENT_MAX_LENGTH+1];
	char valBuffer[GS_COMMENT_MAX_LENGTH+1];
	Decimal longitude;
	Decimal latitude;


	bzero(keyBuffer,sizeof keyBuffer);
	bzero(valBuffer,sizeof valBuffer);
	gs_marker_ZeroStruct(&marker);
	gs_comment_ZeroStruct(&assocComment);
	strFlag = 0;

	sm = sm_new(HASH_TABLE_CAPACITY);
	if(sm == NULL){
		fprintf(stderr, "sm err\n");
		return -1;
	}

	/*Parse the JSON for the information we desire */
	for(i=0; i < request->contentLength && request->data[i] != '\0'; ++i){
		/*We're at the start of a string*/
		if(request->data[i] == '"'){
			/*Go until we hit the closing qoute*/
			i++;
			for(j=0; i < request->contentLength && request->data[i] != '\0' && request->data[i] != '"' && (unsigned int)j < sizeof keyBuffer; ++j,++i){
				keyBuffer[j] = (int)request->data[i] > 64 && request->data[i] < 91 ? request->data[i] + 32 : request->data[i];
			}
			keyBuffer[j] = '\0';
			/*find the beginning of the value
			 *which is either a " or a number. So skip spaces and commas
			*/
			for(i++; i < request->contentLength && request->data[i] != '\0' && (request->data[i] == ',' || request->data[i] == ' ' || request->data[i] == ':' || request->data[i] == '\n'); ++i)
				;
			/*Skip any opening qoute */
			if(request->data[i] != '\0' && request->data[i] == '"'){
				i++;
				strFlag = 1;
			}
			for(j=0; i < request->contentLength && request->data[i] != '\0'; ++j,++i){
				if(strFlag == 0){
					if(request->data[i] == ' ' || request->data[i] == '\n')
						break; /*break out if num data*/
				}else{
					if(request->data[i] == '"' && request->data[i-1] != '\\')
						break;
				}
				valBuffer[j] = request->data[i];
			}
			valBuffer[j] = '\0';
			/* Skip any closing paren. */
			if(request->data[i] == '"')
				i++;
			if(strlen(keyBuffer) > 0 && strlen(valBuffer) > 0)
				if(sm_put(sm, keyBuffer, valBuffer) == 0)
                	fprintf(stderr, "Failed to copy parameters into hash table while parsing url\n");
		}
		strFlag = 0;
	}

	/* Verify that the data is valid */
	if(	sm_exists(sm, "type") 		!=1 || 
		sm_exists(sm, "message") 	!=1 ||
		sm_exists(sm, "latdegrees") !=1 ||
		sm_exists(sm, "londegrees") !=1 ||
		sm_exists(sm, "addressed" ) !=1	){

		sm_delete(sm);
		snprintf(buffer,buffSize,ERROR_STR_FORMAT,400,KEYS_MISSING);
		return 400;		
	}else{
		/* Extract and create the two structs */
		if(sm_get(sm,"type", valBuffer, sizeof valBuffer) == 1){
			/* Verify that it is a correct type */
			if(strncasecmp(valBuffer, CTYPE_1,COMMENTS_CTYPE_SIZE) != 0)
				if(strncasecmp(valBuffer, CTYPE_2,COMMENTS_CTYPE_SIZE) != 0)
					if(strncasecmp(valBuffer, CTYPE_3,COMMENTS_CTYPE_SIZE) != 0){				
						sm_delete(sm);
						snprintf(buffer,buffSize,ERROR_STR_FORMAT,400,BAD_TYPE_ERR);
						return 400;
					}
		}
		/* _shared_campaign_id is a global inherited from green-serv.c */
		gs_comment_setScopeId(_shared_campaign_id, &assocComment);
		gs_marker_setScopeId(_shared_campaign_id, &marker);
		gs_comment_setCommentType(valBuffer,&assocComment);

		sm_get(sm,"message",valBuffer, sizeof valBuffer);
		gs_comment_setContent(valBuffer,&assocComment);

		sm_get(sm,"londegrees",valBuffer,sizeof valBuffer);
		longitude = createDecimalFromString(valBuffer);
		gs_marker_setLongitude(longitude, &marker);

		sm_get(sm,"latdegrees",valBuffer,sizeof valBuffer);
		latitude = createDecimalFromString( valBuffer);
		gs_marker_setLatitude(latitude, &marker);

		sm_get(sm,"addressed", valBuffer, sizeof valBuffer);
		fprintf(stderr, "valbuff:%s\n", valBuffer);
		if(strncasecmp(valBuffer,"true", sizeof valBuffer) == 0)
			gs_marker_setAddressed(ADDRESSED_TRUE,&marker);
		else if(strncasecmp(valBuffer,"false",sizeof valBuffer) == 0)
			gs_marker_setAddressed(ADDRESSED_FALSE,&marker);
		else{
			sm_delete(sm);
			snprintf(buffer,buffSize,ERROR_STR_FORMAT,400,NO_ADDRESSED_KEY);
			return 400;
		}
			

	
	}

	mysql_thread_init();
	conn = _getMySQLConnection();
	if(!conn){
		mysql_thread_end();
		fprintf(stderr, "%s\n", "Could not connect to mySQL on worker thread");
		return -1;
	}	

	/* Insert comment first  because our mySQL trigger
	 * will then handle updating the comment's pin id to match the new pin
	 * that we'll submit after. If we didn't have this trigger we'd have
	 * to do things  a bit differently.
	*/
	db_insertComment(&assocComment,conn);
	if(assocComment.id == GS_COMMENT_INVALID_ID){
		snprintf(buffer,buffSize,ERROR_STR_FORMAT,422,"Could not create pin because of invalid message or type");
		goto cleanup_on_err;
	}
	gs_marker_setCommentId(assocComment.id, &marker);
	db_insertMarker(&marker,conn);
	if(marker.id == GS_MARKER_INVALID_ID){
		snprintf(buffer,buffSize,ERROR_STR_FORMAT,422,"The pin was unprocessable and could not be created");
		/* Clean up after ourselves */
		db_deleteComment(assocComment.id,conn);
		goto cleanup_on_err;
	}

	mysql_close(conn);
	mysql_thread_end();
	sm_delete(sm);

	snprintf(buffer,buffSize,"{ \"status_code\" : 200, \"message\" : \"Successful submit\" }");

	return 200;

	cleanup_on_err:
		mysql_close(conn);
		mysql_thread_end();
		sm_delete(sm);
		return -1;
}


int marker_get(char * buffer,int buffSize,Decimal * latDegrees, Decimal * lonDegrees, Decimal * latOffset,Decimal * lonOffset,int page){
	MYSQL * conn;
	struct gs_comment * comments;
	struct gs_marker * markers; 
	int numMarkers;
	int nextPage;
	char nextStr[MAX_URL_LENGTH];
	char prevStr[MAX_URL_LENGTH];
	char hybridBuffer[buffSize];
	char json[512]; /*Some large enough number to hold all the info*/
	int i;


	comments = malloc(MARKER_LIMIT * sizeof(struct gs_comment));
	if(comments == NULL){
		return -1; /* Return flag to send self to cc_nomem */
	}
	memset(comments,0,MARKER_LIMIT * sizeof(struct gs_comment));

	markers = malloc(MARKER_LIMIT * sizeof(struct gs_marker));
	if(markers == NULL){
		free(comments);
		return -1;
	}
	memset(markers,0,MARKER_LIMIT * sizeof(struct gs_marker));


	fprintf(stderr, "%s buff: %s buffsize %d lad%plod%p lao%ploo%p %d\n", "Called market get",buffer,buffSize,(void*)latDegrees,(void*)lonDegrees,(void*)latOffset,(void*)lonOffset,page);
	
	mysql_thread_init();
	conn = _getMySQLConnection();
	if(!conn){
		free(comments);
		free(markers);
		mysql_thread_end();
		fprintf(stderr, "%s\n", "Could not connect to mySQL on worker thread");
		return -1;
	}

	bzero(nextStr, sizeof nextStr);
	bzero(prevStr, sizeof prevStr);
	bzero(hybridBuffer,sizeof hybridBuffer);
	numMarkers = 0;

	if(latDegrees == NULL && lonDegrees == NULL){
		/* Easy, do a query for all the points regardless of location 
		 * Negative 1 on the page because we need to start the offset at 0
		*/
		numMarkers = db_getMarkerComments(page-1, _shared_campaign_id , markers, comments, conn);
	} else if ( lonDegrees == NULL && latDegrees != NULL) {
		/* Only caring about latdegrees */
		numMarkers = db_getMarkerCommentsLatitude(page-1, _shared_campaign_id, markers, comments, conn, latDegrees, latOffset);
	} else if ( lonDegrees != NULL && latDegrees == NULL ) {
		numMarkers = db_getMarkerCommentsLongitude(page-1, _shared_campaign_id, markers, comments, conn, lonDegrees, lonOffset);		
	} else if ( lonDegrees != NULL && latDegrees != NULL) {
		numMarkers = db_getMarkerCommentsFullFilter(page-1, _shared_campaign_id, markers, comments,  conn,latDegrees,latOffset, lonDegrees,  lonOffset);
	} else {
		/* Bad Request? not sure if it's possible to even hit this case */
		fprintf(stderr, "--%s\n", "Possible to hit here?");
	}

	if( numMarkers > MARKER_RETURNED ){
		nextPage = page+1;
		/*Need to tack on url parameters if present*/
		snprintf(nextStr,MAX_URL_LENGTH, "%spins?page=%d", BASE_API_URL, nextPage);
	} else {
		snprintf(nextStr,MAX_URL_LENGTH, "null");
	}

	if(page > 1)
		snprintf(prevStr,MAX_URL_LENGTH,"%spins?page=%d",BASE_API_URL,page-1);
	else
		snprintf(prevStr,MAX_URL_LENGTH,"null");

	/* Finally, create a hybrid json object and store it into the correct
	 * format and return the buffer to the calling function.
	*/
	for(i=0; i < min(numMarkers,MARKER_RETURNED); ++i){
		bzero(json,sizeof json);
		/* Custom hyrbid json call
		*/
		gs_markerCommentNToJSON(&markers[i], &comments[i] ,json, sizeof json);
		if(i==0)
			snprintf(hybridBuffer,buffSize,"%s",json);
		else{
			strncat(hybridBuffer,",",buffSize);
			strncat(hybridBuffer,json,buffSize);
		}			
	}

	snprintf(buffer,buffSize, MARKER_PAGE_STR, 200, hybridBuffer, min(numMarkers,MARKER_RETURNED), page-1, nextStr,prevStr);
	

	free(comments);
	free(markers);
	mysql_close(conn);
	mysql_thread_end();

	return -1;
}

/* /api/pins?id=<pin id> */
int marker_get_single(char * buffer,  int buffsize, long id){
	MYSQL *conn;
	struct gs_comment gsc;
	struct gs_marker gsm;
	char json[512];

	mysql_thread_init();
	conn = _getMySQLConnection();
	if(!conn){
		mysql_thread_end();
		fprintf(stderr, "%s\n", "Could not connect to mySQL on worker thread");
		return -1;
	}	

	db_getMarkerById(id, &gsm, conn);
	if(gsm.id == GS_MARKER_INVALID_ID){
		/* 404 */
		mysql_close(conn);
		mysql_thread_end();
		snprintf(buffer, buffsize, ERROR_STR_FORMAT, 404, "Could not find pin with given id");
		return 404;
	}else{
		db_getCommentById(gsm.commentId, &gsc, conn);
	}

	mysql_close(conn);
	mysql_thread_end();

	gs_markerCommentNToJSON(&gsm, &gsc ,json, sizeof json);

	snprintf(buffer,buffsize,"{\"status_code\" : 200,\"pin\" : %s}",json);
	return 200;
}