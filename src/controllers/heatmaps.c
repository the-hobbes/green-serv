#include "controllers/heatmaps.h"

static inline int min(const int a, const int b){
	return a < b ? a : b;
}

static inline void  swapCharPtr( char ** ptr1, char ** ptr2){
	char *temp = *ptr1;
    *ptr1 = *ptr2;
    *ptr2 = temp;
}

int heatmap_controller(const struct http_request * request, char ** stringToReturn, int strLength){
	int status;
	char * buffer; 
	int buffSize;
	int page;
	int raw;
	int numParams;
	int precision;
	char **convertSuccess;
	char tempBuf[40];
	StrMap * sm;
	Decimal * latDegrees;
	Decimal * lonDegrees;
	Decimal * latOffset;
	Decimal * lonOffset;


	status = 503;
	buffSize = 500;//HEATMAP_RESULTS_PER_PAGE*(sizeof(struct gs_heatmap)*4+1)+(2*MAX_URL_LENGTH);
	bzero(tempBuf, sizeof tempBuf);
	page = 1;
	precision = 8;
	convertSuccess = NULL;
	raw = FALSE;

	buffer = malloc(buffSize);
	if(buffer == NULL){
		status = 500;
		goto mh_nomem;
	}
	buffer = memset(buffer,0,buffSize);

	latDegrees = malloc(sizeof(Decimal));
	if (latDegrees == NULL) {
		free(buffer);
		status = 500;
		goto mh_nomem;
	} else {
		latDegrees = memset(latDegrees,0,sizeof(Decimal));
	}
	lonDegrees = malloc(sizeof(Decimal));
	if (lonDegrees == NULL) {
		free(buffer); free(latDegrees);		
		status = 500;
		goto mh_nomem;
	} else {
		lonDegrees = memset(lonDegrees,0,sizeof(Decimal));
	}
	latOffset = malloc(sizeof(Decimal));
	if (latOffset == NULL) {
		free(buffer); free(latDegrees); free(lonDegrees);
		status = 500;
		goto mh_nomem;
	} else {
		latOffset = memset(latOffset,0,sizeof(Decimal));
	}
	lonOffset = malloc(sizeof(Decimal));
	if (latDegrees == NULL) {
		free(buffer); free(latDegrees); free(lonDegrees); free(latOffset);
		status = 500;
		goto mh_nomem;
	} else {
		latDegrees = memset(latDegrees,0,sizeof(Decimal));
	}

	sm = sm_new(HASH_TABLE_CAPACITY);
	if(sm == NULL){
		status = 500;
		free(buffer); free(latDegrees); free(lonDegrees); free(latOffset); free(lonDegrees);
		goto mh_nomem;
	}
	numParams = parseURL(request->url, strlen(request->url), sm);
	if(numParams > 0){
		/* Parse for optional parameters of page,latdegrees,londegrees,latoffsets,lonoffsets,precision, and raw*/
		if( sm_exists(sm, "page") ==1){
			sm_get(sm,"page",tempBuf,sizeof tempBuf);
			page = atoi(tempBuf);
			if(page <= 0){
				status = 400;		
				sm_delete(sm);
				free(buffer); free(latDegrees); free(lonDegrees); free(latOffset); free(lonOffset);	
				goto mh_badpage;
			}
		}
		if( sm_exists(sm,"latdegrees") == 1){
			sm_get(sm,"latdegrees",tempBuf,sizeof tempBuf);
			if(strtod(tempBuf,convertSuccess) != 0 && convertSuccess == NULL && strncasecmp(tempBuf, "nan", 3) != 0)
				(*latDegrees) = createDecimalFromString(tempBuf);
			else{
				sm_delete(sm);
				status = 400;
				free(buffer); free(latDegrees); free(lonDegrees); free(latOffset); free(lonOffset);	
				goto mh_badlat;
			}
		}else{
			free(latDegrees);
			latDegrees = NULL;
		}
		if( sm_exists(sm, "londegrees") == 1){
			sm_get(sm,"londegrees",tempBuf,sizeof tempBuf);
			if(strtod(tempBuf,convertSuccess) != 0 && convertSuccess == NULL && strncasecmp(tempBuf, "nan", 3) != 0)
				(*lonDegrees) = createDecimalFromString(tempBuf);
			else{
				sm_delete(sm);
				status = 400;
				free(buffer); free(latDegrees); free(lonDegrees); free(latOffset); free(lonOffset);	
				goto mh_badlon;
			}
		}else{
			free(lonDegrees);
			lonDegrees = NULL;
		}
		if( sm_exists(sm, "lonoffset") == 1){
			sm_get(sm,"lonoffset",tempBuf,sizeof tempBuf);
			if(strtod(tempBuf,convertSuccess) != 0 && convertSuccess == NULL && strncasecmp(tempBuf, "nan", 3) != 0)
				(*lonOffset) = createDecimalFromString(tempBuf);
			else{
				sm_delete(sm);
				status = 400;
				free(buffer); free(latDegrees); free(lonDegrees); free(latOffset); free(lonOffset);	
				goto mh_badlonoffset;	
			}
		} else {
			(*lonOffset) = createDecimalFromString(DEFAULT_OFFSET);
		}
		if( sm_exists(sm, "latoffset") == 1){
			sm_get(sm,"latoffset",tempBuf,sizeof tempBuf);
			if(strtod(tempBuf,convertSuccess) != 0 && convertSuccess == NULL && strncasecmp(tempBuf, "nan", 3) != 0)
				(*latOffset) = createDecimalFromString(tempBuf);
			else{
				sm_delete(sm);
				status = 400;
				free(buffer); free(latDegrees); free(lonDegrees); free(latOffset); free(lonOffset);	
				goto mh_badlatoffset;	
			}
		} else {
			(*latOffset) = createDecimalFromString(DEFAULT_OFFSET);
		}
		if( sm_exists(sm, "precision") == 1){
			sm_get(sm,"precision",tempBuf, sizeof tempBuf);
			if(strtod(tempBuf,convertSuccess) != 0 && convertSuccess == NULL && strncasecmp(tempBuf, "nan", 3) != 0){
				precision = strtod(tempBuf,convertSuccess);
				if(precision < 0)
					goto hop;
			}else{
				hop:
				sm_delete(sm);
				free(buffer); 
				if(latDegrees != NULL)
					free(latDegrees); 
				if(lonDegrees != NULL)
					free(lonDegrees); 
				free(latOffset); free(lonOffset);	
				status = 400; 	
				goto bad_precision;
			}
		}
		if( sm_exists(sm, "raw") == 1){
			sm_get(sm,"raw",tempBuf, sizeof tempBuf);
			if(strncasecmp(tempBuf,"true",4) == 0)
				raw = TRUE;
			else
				raw = FALSE;
		}
	}else{
		free(lonOffset); lonOffset  = NULL;	
		free(latOffset); latOffset  = NULL;
		free(lonDegrees); lonDegrees = NULL;
		free(latDegrees); latDegrees = NULL;

	}

	switch(request->method){
		case GET:
			/* Optional Parameters cause the database query to change
			*/
			if((sm_exists(sm,"latoffset")==1) ^ (sm_exists(sm,"lonoffset")==1)){
				/*Err! if one is used, both must be used! */
				sm_delete(sm);
				free(buffer);
				FREE_NON_NULL_DEGREES_AND_OFFSETS
				status = 422;
				goto mh_bothOffsets;
			}
			if(latDegrees != NULL)
				/*Let the -90.1 slide by as ok...*/
				if(*latDegrees < -90L || *latDegrees > 90L){
					sm_delete(sm);
					free(buffer); 
					FREE_NON_NULL_DEGREES_AND_OFFSETS
					status = 422;
					goto mh_badlat;		
				}

			if(lonDegrees != NULL)
				if(*lonDegrees < -180L || *lonDegrees > 180L){
					sm_delete(sm);
					free(buffer); 
					FREE_NON_NULL_DEGREES_AND_OFFSETS
					status = 422;
					goto mh_badlon;			
				}
			status = heatmap_get(&buffer, buffSize,page, latDegrees, latOffset, lonDegrees, lonOffset, precision, raw);
			if(status == -1){
				free(buffer); 
				FREE_NON_NULL_DEGREES_AND_OFFSETS
				sm_delete(sm);
				goto mh_nomem;
			}
			break;
		case PUT:
			status = heatmap_put(buffer,buffSize,request);
			if(status == -1){
				free(buffer); 
				FREE_NON_NULL_DEGREES_AND_OFFSETS
				sm_delete(sm);	
				goto mh_nomem;
			}
			break;
		case DELETE:
		case POST:	
		default:
			status = 501;	
			free(buffer); 
			FREE_NON_NULL_DEGREES_AND_OFFSETS
			sm_delete(sm);
			goto mh_unsupportedMethod;
	}

	swapCharPtr(stringToReturn,&buffer);
	
	free(buffer); 
	FREE_NON_NULL_DEGREES_AND_OFFSETS
	sm_delete(sm);
	return status;

	ERR_LABEL_STRING_TO_RETURN(mh_unsupportedMethod,BAD_METHOD_ERR)
	ERR_LABEL_STRING_TO_RETURN(mh_nomem, NOMEM_ERROR)
	ERR_LABEL_STRING_TO_RETURN(mh_badpage, BAD_PAGE_ERR)
	ERR_LABEL_STRING_TO_RETURN(mh_badlat, LATITUDE_OUT_OF_RANGE_ERR)
	ERR_LABEL_STRING_TO_RETURN(mh_badlon, LONGITUDE_OUT_OF_RANGE_ERR)
	ERR_LABEL_STRING_TO_RETURN(mh_bothOffsets, BOTH_OFFSET_ERR)
	ERR_LABEL_STRING_TO_RETURN(bad_precision, PRECISION_ERR)
	ERR_LABEL_STRING_TO_RETURN(mh_badlatoffset, "bad lat offset")
	ERR_LABEL_STRING_TO_RETURN(mh_badlonoffset, "bad lon offset")

}

int heatmap_get(char ** buffer, int buffSize,int page, Decimal * latDegrees, Decimal * latOffset, Decimal * lonDegrees, Decimal * lonOffset, int precision, int raw){
	MYSQL * conn;
	struct gs_heatmap * heatmaps;
	int numHeatmaps;
	int nextPage;
	long max;
	char nextStr[MAX_URL_LENGTH];
	char prevStr[MAX_URL_LENGTH];
	char * tempBuf;
	char * swapBuff;
	char * decimalBuff;
	Decimal lowerLat;
	Decimal upperLat;
	Decimal lowerLon;
	Decimal upperLon;
	int resize;
	char json[512]; /*Some large enough number to hold all the info*/
	int i;

	tempBuf = malloc(sizeof(char)* buffSize);
	if(tempBuf == NULL){
		return -1;
	}
	memset(tempBuf,0,buffSize);

	decimalBuff = malloc(sizeof(char)* (DecimalWidth +1));
	if(decimalBuff == NULL){
		free(tempBuf);
		return -1;
	}
	memset(decimalBuff,0,sizeof(char) * (DecimalWidth+1));

	heatmaps = malloc(HEATMAP_RESULTS_PER_PAGE * sizeof(struct gs_heatmap));
	if(heatmaps == NULL){
		free(tempBuf);
		free(decimalBuff);
		return -1; /* Return flag to send self nomem */
	}
	memset(heatmaps,0,HEATMAP_RESULTS_PER_PAGE * sizeof(struct gs_heatmap));
	lowerLon = createDecimalFromString("-181");
	lowerLat = createDecimalFromString("-91");
	upperLon = createDecimalFromString("181");
	upperLat = createDecimalFromString("91");

	mysql_thread_init();
	conn = _getMySQLConnection();
	if(!conn){
		free(heatmaps);
		free(decimalBuff);
		free(tempBuf);
		mysql_thread_end();
		fprintf(stderr, "%s\n", "Could not connect to mySQL on worker thread");
		return -1;
	}

	bzero(nextStr, sizeof nextStr);
	bzero(prevStr, sizeof prevStr);
	numHeatmaps = 0;

	if(latDegrees == NULL && lonDegrees == NULL){
		/* Easy, do a query for all the points regardless of location 
		/ * Negative 1 on the page because we need to start the offset at 0
		*/
	} else if ( lonDegrees == NULL && latDegrees != NULL) {
		/* Only caring about latdegrees */
		add_decimals(latDegrees,latOffset,&upperLat);
		subtract_decimals(latDegrees,latOffset,&lowerLat);
	} else if ( lonDegrees != NULL && latDegrees == NULL ) {
		add_decimals(lonDegrees,lonOffset,&upperLon);
		subtract_decimals(lonDegrees,lonOffset,&lowerLon);
	} else if ( lonDegrees != NULL && latDegrees != NULL) {
		add_decimals(latDegrees,latOffset,&upperLat);
		subtract_decimals(latDegrees,latOffset,&lowerLat);
		add_decimals(lonDegrees,lonOffset,&upperLon);
		subtract_decimals(lonDegrees,lonOffset,&lowerLon);
	} else {
		/* Bad Request? not sure if it's possible to even hit this case */
		fprintf(stderr, "--%s\n", "Possible to hit here?");
	}
	/* Negative 1 on the page because we need to start the offset at 0	*/
	numHeatmaps = db_getHeatmap(page-1, _shared_campaign_id, precision, &max, lowerLat, upperLat, lowerLon, upperLon, heatmaps, conn);

	if( numHeatmaps > HEATMAP_RESULTS_RETURNED ){
		nextPage = page+1;
		/*Need to tack on url parameters if present*/
		snprintf(nextStr,MAX_URL_LENGTH, "%sheatmap?page=%d&raw=%s&precision=%i", BASE_API_URL, nextPage, raw == TRUE ? "true" : "false", precision);
		if (lonOffset != NULL) {
			formatDecimal(*lonOffset, decimalBuff);
			strncat(nextStr,"&lonOffset=",MAX_URL_LENGTH);
			strncat(nextStr,decimalBuff,MAX_URL_LENGTH);
		}
		if (latOffset != NULL) {
			formatDecimal(*latOffset, decimalBuff);
			strncat(nextStr,"&latOffset=",MAX_URL_LENGTH);
			strncat(nextStr,decimalBuff,MAX_URL_LENGTH);
		}
		if (lonDegrees != NULL) {
			formatDecimal(*lonDegrees, decimalBuff);
			strncat(nextStr,"&lonDegrees=",MAX_URL_LENGTH);
			strncat(nextStr,decimalBuff,MAX_URL_LENGTH);
		}
		if (latDegrees != NULL) {
			formatDecimal(*latDegrees, decimalBuff);
			strncat(nextStr,"&latDegrees=",MAX_URL_LENGTH);
			strncat(nextStr,decimalBuff,MAX_URL_LENGTH);
		}
	} else {
		snprintf(nextStr,MAX_URL_LENGTH, "null");
	}


	if(page > 1){
		snprintf(prevStr,MAX_URL_LENGTH,"%sheatmap?page=%d&raw=%s&precision=%if",BASE_API_URL,page-1, raw == TRUE ? "true" : "false", precision);
		if (lonOffset != NULL) {
			formatDecimal(*lonOffset, decimalBuff);
			strncat(prevStr,"&lonOffset=",MAX_URL_LENGTH);
			strncat(prevStr,decimalBuff,MAX_URL_LENGTH);
		}
		if (latOffset != NULL) {
			formatDecimal(*latOffset, decimalBuff);
			strncat(prevStr,"&latOffset=",MAX_URL_LENGTH);
			strncat(prevStr,decimalBuff,MAX_URL_LENGTH);
		}
		if (lonDegrees != NULL) {
			formatDecimal(*lonDegrees, decimalBuff);
			strncat(prevStr,"&lonDegrees=",MAX_URL_LENGTH);
			strncat(prevStr,decimalBuff,MAX_URL_LENGTH);
		}
		if (latDegrees != NULL) {
			formatDecimal(*latDegrees, decimalBuff);
			strncat(prevStr,"&latDegrees=",MAX_URL_LENGTH);
			strncat(prevStr,decimalBuff,MAX_URL_LENGTH);
		}
	} else {
		snprintf(prevStr,MAX_URL_LENGTH,"null");
	}

	resize = 1;
	for(i=0; i < min(numHeatmaps,HEATMAP_RESULTS_RETURNED); ++i){
		bzero(json,sizeof json);
		if(!raw)
			heatmaps[i].intensity = (heatmaps[i].intensity/(double)max)*100;
		gs_heatmapNToJSON(heatmaps[i], json, sizeof json);
		if(buffSize*resize < (int)(strlen(tempBuf)+strlen(json))){
			/* Resize */
			resize = resize*2;
			swapBuff = malloc(sizeof(char)*buffSize*resize);
			if(swapBuff == NULL){
				NETWORK_LOG_LEVEL_1("Failed to allocate memory for swap buffer");
				NETWORK_LOG_LEVEL_2_NUM("Failed to allocate swap buffer of bytesize", buffSize*resize);
				resize = resize/2;
			}else{
				memset(swapBuff,0,sizeof(char)*buffSize*resize);
				strcpy(swapBuff, tempBuf);
				swapCharPtr(&swapBuff,&tempBuf);
				free(swapBuff); /* free up the old memory */
			}
		}
		if(i==0)
			snprintf(tempBuf,buffSize*resize,"%s",json);
		else{
			strncat(tempBuf,",",buffSize*resize);
			strncat(tempBuf,json,buffSize*resize);
		}			
	}
	
	/* if(resize != 1){ */
		swapBuff = malloc(sizeof(char)*(buffSize*resize)+strlen(HEATMAP_PAGE_STR)+512);
		if(swapBuff == NULL){
			NETWORK_LOG_LEVEL_1("Failed to allocate memory for swap buffer");
			NETWORK_LOG_LEVEL_2_NUM("Failed to allocate swap buffer of bytesize", (int)(buffSize*resize+strlen(HEATMAP_PAGE_STR)));
		}else{
			swapCharPtr(&swapBuff,buffer);
			free(swapBuff);
		}
	/* } */

	snprintf(*buffer,(buffSize*resize) + 512, HEATMAP_PAGE_STR, 200, tempBuf, min(numHeatmaps,HEATMAP_RESULTS_RETURNED), page, nextStr,prevStr);
	free(heatmaps);
	free(tempBuf);
	free(decimalBuff);
	mysql_close(conn);
	mysql_thread_end();

	return 200;
}

int heatmap_put(char * buffer, int buffSize, const struct http_request * request){
	MYSQL *conn;
	struct gs_heatmap * heatmap;
	StrMap * sm;
	int i, j, strFlag;
	int numberHeatmapsSent;
	long intensity;
	char keyBuffer[GS_COMMENT_MAX_LENGTH+1];
	char valBuffer[GS_COMMENT_MAX_LENGTH+1];
	Decimal longitude;
	Decimal latitude;
	struct mNode * lhead;
	struct mNode * ltail;
	char **convertSuccess;

	lhead = NULL;
	ltail  =NULL;
	convertSuccess = NULL;
	bzero(keyBuffer,sizeof keyBuffer);
	bzero(valBuffer,sizeof valBuffer);
	
	strFlag = 0;

	sm = sm_new(HASH_TABLE_CAPACITY);
	if(sm == NULL){
		fprintf(stderr, "sm err\n");
		return -1;
	}

	if( validateJSON( request->data, request->contentLength ) == 0 ){
		sm_delete(sm);
		snprintf(buffer, buffSize, ERROR_STR_FORMAT, 400, MALFORMED_JSON);
		return 400;
	}

	numberHeatmapsSent = 0;
	/*Parse the JSON for the information we desire no using parseJSON here yet until refactor becuase of goto logic*/
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
					if(request->data[i] == ' ' || request->data[i] == '\n' || request->data[i] == ',' || request->data[i] == '}' || request->data[i] == ']')
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
			if(strlen(keyBuffer) > 0 && strlen(valBuffer) > 0){
				if(sm_put(sm, keyBuffer, valBuffer) == 0)
                	fprintf(stderr, "Failed to copy parameters into hash table while parsing url\n");
                else
                	numberHeatmapsSent++; /* Increase for each key val pair we find, then divide by 3 later */
            }
            /* Check if we have a full and valid point yet */
        	if( numberHeatmapsSent % 3 == 0 ){
            	if(sm_exists(sm, "secondsworked") == 1 && sm_exists(sm,"latdegrees") == 1 && sm_exists(sm,"londegrees") ==1 ){
	            	heatmap = malloc(sizeof (struct gs_heatmap));
	            	if(heatmap == NULL){
	            		sm_delete(sm);
	            		if(lhead != NULL)
	            			destroy_list(lhead);
	            		return -1;
	            	}
	            	gs_heatmap_ZeroStruct(heatmap);

	            							
					/* _shared_campaign_id is a global inherited from green-serv.c */
					gs_heatmap_setScopeId(_shared_campaign_id, heatmap);		

					sm_get(sm,"londegrees",valBuffer,sizeof valBuffer);
					if( strncasecmp(valBuffer,"null",4) == 0 ){
						sm_delete(sm);
						if(lhead != NULL)
							destroy_list(lhead);
						snprintf(buffer, buffSize, ERROR_STR_FORMAT, 422, NULL_LONGITUDE);
						return 422;
					}
					if(strtod(valBuffer,convertSuccess) != 0 && convertSuccess == NULL && strncasecmp(valBuffer, "nan", 3) != 0)
						longitude = createDecimalFromString(valBuffer);
					else{
						sm_delete(sm);
						if(lhead != NULL)
							destroy_list(lhead);
						snprintf(buffer, buffSize, ERROR_STR_FORMAT, 400, NAN_LONGITUDE);
						return 400;
					}
					gs_heatmap_setLongitude(longitude, heatmap);

					sm_get(sm,"latdegrees",valBuffer,sizeof valBuffer);
					if( strncasecmp(valBuffer,"null",4) == 0 ){
						sm_delete(sm);
						if(lhead != NULL)
							destroy_list(lhead);
						snprintf(buffer, buffSize, ERROR_STR_FORMAT, 422, NULL_LATITUDE);
						return 422;
					}
					if(strtod(valBuffer,convertSuccess) != 0 && convertSuccess == NULL && strncasecmp(valBuffer, "nan", 3) != 0)
						latitude = createDecimalFromString( valBuffer);
					else{
						sm_delete(sm);
						if(lhead != NULL)
							destroy_list(lhead);
						snprintf(buffer, buffSize, ERROR_STR_FORMAT, 400, NAN_LATITUDE);
						return 400;
					}
					gs_heatmap_setLatitude(latitude, heatmap);

					/* Check latitude and longitude ranges */
					if(!(-90L <= latitude && latitude <= 90L )){
						sm_delete(sm);
						if(lhead != NULL)
            				destroy_list(lhead);
						snprintf(buffer,buffSize,ERROR_STR_FORMAT,422,LATITUDE_OUT_OF_RANGE_ERR);
						return 422;
					}

					if(!(-180L <= longitude && longitude <= 180L)){
						sm_delete(sm);
						if(lhead != NULL)
            				destroy_list(lhead);
						snprintf(buffer,buffSize,ERROR_STR_FORMAT,422,LONGITUDE_OUT_OF_RANGE_ERR);
						return 422;
					}
					
					sm_get(sm,"secondsworked", valBuffer, sizeof valBuffer);
					if( strncasecmp(valBuffer,"null",4) == 0 ){
						sm_delete(sm);
						if(lhead != NULL)
							destroy_list(lhead);
						snprintf(buffer, buffSize, ERROR_STR_FORMAT, 422, NULL_SECONDS);
						return 422;
					}
					if(strtod(valBuffer,convertSuccess) != 0 && convertSuccess == NULL && strncasecmp(valBuffer, "nan", 3) != 0 )
						intensity = strtol(valBuffer,NULL,10);
					else{
						sm_delete(sm);
						if(lhead != NULL)
							destroy_list(lhead);
						snprintf(buffer, buffSize, ERROR_STR_FORMAT, 400, BAD_INTENSITY_ERR);
						return 400;
					}
					if(intensity > 0L)
						gs_heatmap_setIntensity(intensity, heatmap);
					else{
						if(intensity <= 0 ){
							sm_delete(sm);
							snprintf(buffer,buffSize,ERROR_STR_FORMAT,422,BAD_INTENSITY_NEG_ERR);
							if(lhead != NULL)
            					destroy_list(lhead);
							return 422;		
						}
					}
				} else {
					sm_delete(sm);
					if(lhead != NULL)
	            		destroy_list(lhead);
					snprintf(buffer,buffSize,ERROR_STR_FORMAT,400,KEYS_MISSING);
					return 400;		
				}

				if(lhead == NULL)
					ltail = lhead = create_node(lhead, heatmap );
				else
					ltail = create_node(ltail, heatmap);

				sm_delete(sm);
            	sm = sm_new(HASH_TABLE_CAPACITY);
				if(sm == NULL){
					fprintf(stderr, "sm err\n");
					if(lhead != NULL)
            			destroy_list(lhead);
					return -1;
				}
            }
		}
		strFlag = 0;
	}

	sm_delete(sm);

	mysql_thread_init();
	conn = _getMySQLConnection();
	if(!conn){
		mysql_thread_end();
		destroy_list(lhead);
		fprintf(stderr, "%s\n", "Could not connect to mySQL on worker thread");
		return -1;
	}	

	db_start_transaction(conn);
	/* Iterate over list */
	for(i=0, ltail = lhead; ltail != NULL; ltail = ltail->next,i++ ) {

		db_insertHeatmap( (struct gs_heatmap* )(ltail->data), conn);
		if( ((struct gs_heatmap*) ltail->data )->id == GS_HEATMAP_INVALID_ID){
			fprintf(stderr, "%s\n", "Unknown error occured, could not insert heatmap into database.");
			snprintf(buffer,buffSize,ERROR_STR_FORMAT,422,"Could not create heatmap. An Unknown Request Error has occured");
			destroy_list(lhead);
			mysql_close(conn);
			mysql_thread_end();	
			return 422;
		}

	}

	if(i != (numberHeatmapsSent/3) || i == 0 ){
		db_abort_transaction(conn);
		db_end_transaction(conn);
		mysql_close(conn);
		mysql_thread_end();
		destroy_list(lhead);		
		if(i == 0){
			snprintf(buffer, buffSize, ERROR_STR_FORMAT, 422, "Must send data to be processed");
			return 422;
		}else{
			snprintf(buffer,buffSize,ERROR_STR_FORMAT, 400, KEYS_MISSING);
			return 400;	
		}		
	}
	db_end_transaction(conn); /* Still call end to reset autocommit to true */

	mysql_close(conn);
	mysql_thread_end();
	destroy_list(lhead);

	snprintf(buffer,buffSize,"{ \"status_code\" : 200, \"message\" : \"Successful submit\" }");

	return 200;		
}

#undef FREE_NON_NULL_DEGREES_AND_OFFSETS
