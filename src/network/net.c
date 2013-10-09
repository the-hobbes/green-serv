#include "network/net.h"

/* Ha, this function name is great. DO NETWORK WORK -- doNetWork! 
 * Is funny because network is what we talk over see?
*/
void* doNetWork(struct threadData* td) {
    /* Copy local data 
    struct ThreadData* data=(struct ThreadData*) td;*/
    struct http_request request;
    int bytesSent;
    int controller;
    int status;

    status = 200;
    parseRequest(&request, td->msg);

    /* Pass the request off to a handler */
    controller = determineController(request.url);
    /* Determine method and call. */
    if(controller != INVALID_CONTROLLER){

    }else{
        /* We have no clue what the client is talking about with their url */
        status = 404;
    }

    /* Log and clean up. */
    printf("url: %s\n", request.url);
    if(request.contentLength > 0)
        printf("td: %s\n", request.data);
    if(request.contentLength > 0)
        free(request.data);


    createResponse("{}",td->msg,status);
    td->msg[strlen(td->msg)] = '\0';
    
    bytesSent = send(td->clientfd,td->msg,strlen(td->msg),0);  
    printf("Sent %d bytes to the client : %s\n",bytesSent,strerror(errno));

    close(td->clientfd);

    return NULL;
}

/*
 * Parse the url and store values into the hash table
 * returns the number of values successfully placed into the hashtable
*/
int parseURL(char * url, int urlLength, StrMap * table){
    int i;
    int j;
    int pairCounter;
    char keyBuff[256];
    char valBuff[256];
    bzero(keyBuff,sizeof keyBuff);
    bzero(valBuff,sizeof valBuff);

    if(url == NULL)
        return 0;
    if(table == NULL)
        return -1; /* Err ... */
    pairCounter = 0;
    /* Find ? */
    for(i=0; url[i] != '\0' && i < urlLength; ++i)
        if(url[i] == '?')
            break;
    while(url[i] != '\0' && i < urlLength){
        for(j=0,i++; url[i] != '=' && url[i] != '\0' && i < urlLength; ++i)
            keyBuff[j++] = url[i];
        keyBuff[j] = '\0';
        for(j=0,i++; strlen(keyBuff) > 0 && url[i] != '&' && url[i] != '\0' && i < urlLength; ++i)
            valBuff[j++] = url[i];
        valBuff[j] = '\0';
        if(strlen(valBuff) > 0 && strlen(keyBuff) > 0){
            /* Place values into table */
            if(sm_put(table, keyBuff, valBuff) == 0)
                fprintf(stderr, "Failed to copy parameters into hash table while parsing url\n");
            else
                pairCounter++;
        }
        /* reset the buffers */
        bzero(keyBuff,sizeof keyBuff);
        bzero(valBuff,sizeof valBuff);
        
    }
    return pairCounter;
}

/*Create a HTTP response with the buff as content and sent out with the
 *response code of status. This function call is not safe.
*/
int createResponse(char * content, char * buff, int status){
    char timeBuffer[1000];
    time_t now;
    struct tm tm;
    bzero(timeBuffer,1000);
    now = time(NULL);

    tm  = *gmtime(&now);
    strftime(timeBuffer, sizeof timeBuffer, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    return sprintf(buff,  
                    "HTTP/1.1 %d OK\r\n"
                   "Content-Type: application/json\r\n"
                   "%sr\n"
                   "Connection: close\r\n"
                   "Content-Length: %zu\r\n\r\n%s\r\n",
                   status,
                   timeBuffer,
                   strlen(content),
                   content);
}

/*Create Socket for communications
 *returns file descriptor or -1 on err
*/
int createSocket(){
    /* perhaps SOCK_NONBLOCK one day... */
    return socket(AF_INET,SOCK_STREAM,0);   
}

/* Sets up a socket for use as a server socket on the designated port
 * returns -1 on errors, 0 otherwise
 * fd is the file descriptor of the socket to setup
 * sockserv is a pointer to a sockaddr_in struct.
 * port is the port to bind the address to
*/
int setupSockAndBind(int fd, struct sockaddr_in * sockserv, int port ){
    sockserv->sin_family = AF_INET;
    sockserv->sin_addr.s_addr = INADDR_ANY;
    sockserv->sin_port = htons(port);
    return bind(fd,(struct sockaddr *)sockserv,sizeof(*sockserv));
}

static int strnstr(char * needle, char * haystack, int haystackLen){
    int i;
    int j;
    
    int needleLen = strlen(needle);
    for(i=0; i < haystackLen && haystack[i] != '\0'; ++i){
        if( needle[0] == haystack[i] ){
            j=i;
            while(needle[j-i] == haystack[j] && j-i < needleLen)
                j++;
            if(j-i == needleLen){
                /* We made it through the haystack and
                 * and looked at all the characters without
                 * terminating early. 
                */
                 printf("found %s:%d\n",needle, i);
                 return i;
            }
        }
    }
    printf("%s%s\n", "could not find ", needle);
    return -1;
}

int parseRequest(struct http_request * requestToFill, char * requestStr){
    char buff[FIRSTLINEBUFFSIZE]; /* This is the most we'll read */
    int i;
    int methodLoc;
    int urlEnd;
    int contentLength;
    bzero(buff,FIRSTLINEBUFFSIZE);
    bzero(requestToFill->url,MAX_URL_LENGTH);
    requestToFill->url[0] = '/'; /* Default assumption */

    /* Attempt to determine the request method */
    for(i=0; i <  FIRSTLINEBUFFSIZE && requestStr[i] != '\0'; ++i){
        if(requestStr[i] == '\r')
            if(requestStr[i+1] == '\n')
                break;
        /* Copy copy copy... */
        buff[i] = requestStr[i];
    }
    methodLoc = strnstr("GET", buff, FIRSTLINEBUFFSIZE  );
    if(methodLoc == -1){
        methodLoc = strnstr("POST", buff, FIRSTLINEBUFFSIZE );
        if(methodLoc == -1){
            methodLoc = strnstr("PUT", buff, FIRSTLINEBUFFSIZE  );
            if(methodLoc == -1){
                methodLoc = strnstr("DELETE", buff, FIRSTLINEBUFFSIZE   );
                if(methodLoc == -1){
                    /* Fuck it. What'd you try to give me? */
                    fprintf(stderr, "Could not determine method: %s\n",buff );
                    requestToFill->method = UNKNOWN_METHOD;
                }else{
                    requestToFill->method = DELETE;
                    methodLoc+= 7; /* Advance past the method and space */
                }
            }else{
                requestToFill->method = PUT;
                methodLoc+= 4;
            }
        }else{
            requestToFill->method = POST;
            methodLoc+=5;
        }
    }else{
        requestToFill->method = GET;
        methodLoc+=4;
    }
    urlEnd = strnstr("HTTP", buff, FIRSTLINEBUFFSIZE );
    /* Find the url (Which will be between the method to the http version)*/
    i=methodLoc;
    for(methodLoc=0; methodLoc < MAX_URL_LENGTH && i < FIRSTLINEBUFFSIZE && requestStr[i] != '\0' && i < urlEnd; ++i){
        if(requestStr[i] != ' '){
            requestToFill->url[methodLoc] = requestStr[i];
            methodLoc++; /* Just re-using a variable instead of using a new one */
        }
    }
    /* Find out if there's any content: */
    contentLength = strnstr("Content-Length",requestStr,strlen(requestStr));
    if(contentLength != -1){
        /*Determine actual content length */
        contentLength+=strlen("Content-Length: "); /* Skip the text */
        methodLoc=0; /* ReUsing again */
        for(i=contentLength; i < FIRSTLINEBUFFSIZE && requestStr[i] != '\0' && requestStr[i] != '\r'; ++i)
            buff[methodLoc++] = requestStr[i];
        buff[i] = '\0';
        /* Attempt to parse: */
        contentLength = atoi(buff);

        if(contentLength > 0){
            /* If there's data than place it into the structure */
            if(requestToFill->method == POST || requestToFill->method == PUT){
                requestToFill->data = malloc(contentLength*sizeof(char)+1);
                if(requestToFill->data != NULL){
                    /* Find the content: */
                    i = strnstr("\r\n\r\n", requestStr, strlen(requestStr));
                    if(i != -1){
                        i+=4; /* skip newlines */
                        for(methodLoc = 0; i < (int)strlen(requestStr) && requestStr[i] != '\0'; ++i, ++methodLoc)
                            requestToFill->data[methodLoc] = requestStr[i];
                        requestToFill->data[methodLoc] = '\0';
                    }else{
                        /* Could not find content...*/
                        free(requestToFill->data);
                        fprintf(stderr, "%s\n", "Could not find start of content. memory freed");
                    }
                }else{
                    fprintf(stderr, "%s\n", "Could not allocate enough memory for content in request ");
                }
            }else{
                /* Don't preallocate memory for the struct or this will cause
                 * memory not being able to be reached
                */
                requestToFill->data = NULL;
            }
        }
    }
    /* Calling parties must use contentLenght to tell if they will need to free the memory */
    requestToFill->contentLength = contentLength;
    return contentLength;

    
}

/* Just a test function... */
int test_network(char * buffer, int bufferLength, void*(*func)(void*)){
    struct sockaddr_in sockserv,sockclient;
    int socketfd,clientfd;
    socklen_t clientsocklen;
    char buff[BUFSIZ];
    pthread_t children[NUMTHREADS];
    struct threadData data[NUMTHREADS];
    int i,j;
    #ifdef DETACHED_THREADS
    pthread_attr_t attr;
    #endif
    
    clientfd = socketfd = 0; 
    bzero(buff,BUFSIZ);
    bzero(&sockserv,sizeof(sockserv));

    socketfd = createSocket();
    printf("Socket Creation: %s\n",strerror(errno));

    setupSockAndBind(socketfd, &sockserv, PORT); 
    printf("Socket Bind: %s\n",strerror(errno));

    listen(socketfd,NUMTHREADS);
    printf("Socket Listen: %s\n%d\n",strerror(errno),errno);

    clientsocklen = sizeof socketfd;

    #ifdef DETACHED_THREADS
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    #endif

    i=j=0;
    if(errno != 13){
        for(j=0; j < 10; ++j){
            for(i=0; i < NUMTHREADS; i++){
                clientfd = accept(socketfd,(struct sockaddr*)&sockclient,&clientsocklen);
                printf("request accepted\n");

                buff[read(clientfd,buff,BUFSIZ)] = '\0';
                printf("Request recieved as \n%s\n",buff);  

                strncpy(buffer, buff ,bufferLength);

                /* A thread pool would be intelligent here */
                sprintf(data[i].msg, "%s", buffer);
                data[i].clientfd = clientfd;
                #ifndef DETACHED_THREADS
                    pthread_create(&children[i],NULL,func,&data[i]);
                #else
                    pthread_create(&children[i],&attr,func,&data[i]);
                #endif
                bzero(buff,BUFSIZ);
                bzero(buffer,bufferLength);
                
            }           
            /*Gobble Up the resources (if not detaching threads)
             *If you do want to detach threads change the define. 
             *in net.h
            */
            #ifndef DETACHED_THREADS
            for(i=0; i < NUMTHREADS; ++i)
                pthread_join(children[i],NULL);
            #endif
        }
    }
    #ifdef DETACHED_THREADS
    pthread_attr_destroy(&attr);
    #endif
    close(socketfd);
    /* Sleep a moment to hope that any running threads will finish */
    fprintf(stdout, "%s\n", "Exiting Server...");
    sleep(2);

    return 0;
}