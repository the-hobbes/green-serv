GreenServ
========================================================================

Author:
- Ethan J. Eldridge


------------------------------------------------------------------------


GreenServ is a simple RAW request/response API server to service the
GreenUp Vermont event. Currently, the clients use a google app engine
database which has resulted in many a headache. This pet project creates 
a fast reliable. Consistent. And Speedy server program.

Setup:

Example apache virtual hosts configuration:
------------------------------------------------------------------------

	<VirtualHost *:80>
        Servername green.xenonapps.dev
        DocumentRoot /green-serv/
        ErrorLog /green-serv/error.log
        CustomLog /green-serv/access.log combined
        ProxyPass / http://localhost:10110/
        ProxyPassReverse / http://localhost:10110/
     </VirtualHost>

Example Configuration File:
------------------------------------------------------------------------
Place the configuration file into the headers directory before building.
You will need to create a config.h file, you can copy the one below if 
you'd like.


	#ifndef __CONFIG_H__
		#define __CONFIG_H__
	
		#define PORT 10110
		#define HOST "localhost"
		#define DATABASE "green_xenon"
		#define PASSWORD "pylons"
		#define USERNAME "tassadar"
        #define CAMPAIGN_ID 0L
        #define CAMPAIGN_DESC "8CharMax"
        #define BASE_API_URL "http://localhost:10110/api/"
        #define PID_FILE "GREENSERV_PID.pid"
        #define PORT_FILE "GREENSERV.port"
        
        /*Define json safe output return function 
        * If you compile with the RETURN_ON_JSON_RISK constant defined
        * then you will return early from the safe json output functions
        * if the amount allocated was not enough for the json string.
        * If you do not you may get a partial JSON back
        */
        #define RETURN_ON_JSON_RISK return -1;
        #undef RETURN_ON_JSON_RISK

        /*Define that the database connection will be threaded.
         *The only time this should be undef-ed is if you're just
         *doing some simple testing for unit tests.
        */
        #define THREADED_DB 1

        /* Define the database logging macro if you want to be able to see 
        * the database queries that are executed on each request:
        * Define as 1 for logging queries, and 0 or undefined for no logging
        */
        #define DATABASE_LOGGING 1

        /* Define the boot logging if you want to see that the socket
        * and binding creation process was succesful (recommended set to 1)
        */
        #define BOOT_LOGGING 1

        /* Define the networking logging to see the requests come and go. 
        * set the logging to level 2 to see _everything_, set to 1 to see
        * basic information. set to 0 to output no network logging
        */
        #define NETWORK_LOGGING 2

        /* Enable Ethan's hack around his mysql setup being foo-bared*/
        #define __MYSQL_INCLUDE_DIFFERENCE__ 1

        /*This is a global variable declaration.
         *The main driving file will define this variable. Calling
         *parties may use this variable as a read-only variable.
         *attempts to change the value of this variable while from a
         *worker thread is undefined behavior and I won't be held responsible.
         *The main driving file must define GREENSERV and NO OTHER files may
        */
        #ifdef GREENSERV
            int _shared_campaign_id;
        #else
            extern int _shared_campaign_id;
        #endif
	    
        /* This must be included in config.h to compile */
        #include "logging.h"

	#endif


SQL to create database structure:
------------------------------------------------------------------------

    CREATE DATABASE green_xenon;
    
    CREATE USER 'tassadar'@'localhost' IDENTIFIED BY 'pylons';
    GRANT ALL ON green_xenon.* TO 'tassadar'@'localhost';
    FLUSH PRIVILEGES;
    
    USE green_xenon;
    
    CREATE TABLE scope (
    	id INT(12) NOT NULL auto_increment PRIMARY KEY,
    	description CHAR(8) NOT NULL, -- Yeah just 8 characters
    	INDEX(`id`)
    ) ENGINE InnoDB;
    
    
    #Seed The Database with initial ancestor
    INSERT INTO scope (description) VALUES ('GREEN_UP');

    CREATE TABLE comments (
        id INT(12) NOT NULL auto_increment PRIMARY KEY,
        pin_id INT(12) NULL,
        comment_type VARCHAR(10) DEFAULT "COMMENT",
        content VARCHAR(140),
        scope_id INT(12) NOT NULL, -- this is an ancestor style query
        created_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL,
        INDEX (`scope_id`),
        CONSTRAINT FOREIGN KEY (`scope_id`) REFERENCES `scope` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
    ) ENGINE InnoDB;

    CREATE TABLE markers (
        id INT(12) NOT NULL auto_increment PRIMARY KEY,
        comment_id INT(12) NULL,
        addressed INT(1) DEFAULT 0 NOT NULL,
        scope_id INT(12), -- this is an ancestor style query
        created_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL,
        latitude DECIMAL(10, 8) NOT NULL,
        longitude DECIMAL(11, 8) NOT NULL,
        INDEX (`id`),
        CONSTRAINT FOREIGN KEY (`comment_id`) REFERENCES `comments` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
        CONSTRAINT FOREIGN KEY (`scope_id`) REFERENCES `scope` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
    ) ENGINE InnoDB;


    CREATE TABLE heatmap (
        id INT(12) NOT NULL auto_increment PRIMARY KEY,
        scope_id INT(12), -- this is an ancestor style query
        intensity INT(12), -- this is seconds worked
        created_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP, -- for dampening
        latitude DECIMAL(10, 8) NOT NULL,
        longitude DECIMAL(11, 8) NOT NULL,
        INDEX(`scope_id`),
        CONSTRAINT FOREIGN KEY (`scope_id`) REFERENCES `scope` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
    ) ENGINE InnoDB;

    CREATE TABLE report (
        id INT(12) NOT NULL auto_increment PRIMARY KEY,
        content TEXT, -- stores 64Kb.
        trace TEXT, -- stores 64Kb.
        scope_id INT(12), -- this is an ancestor style query
        origin CHAR(64) NOT NULL, -- sha256 hash
        authorize CHAR(64) NOT NULL, -- sha256 hash
        created_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        r_type CHAR(20) NOT NULL DEFAULT "INFO",
        INDEX(`scope_id`),
        CONSTRAINT FOREIGN KEY (`scope_id`) REFERENCES `scope` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
    ) ENGINE InnoDB;

    delimiter //
    CREATE TRIGGER comment_marker_ref AFTER INSERT ON markers 
        FOR EACH ROW
        BEGIN
            IF NEW.comment_id IS NOT NULL THEN
                UPDATE comments SET pin_id = NEW.id WHERE comments.id = NEW.comment_id;
            END IF;
        END;//
    delimiter ;

    delimiter //
    CREATE TRIGGER comment_type_to_upper BEFORE INSERT on comments
        FOR EACH ROW
        BEGIN
            SET NEW.comment_type = UPPER(NEW.comment_type);
        END;//
    delimiter ;


Example Compilation and Verification:
------------------------------------------------------------------------
Something that you can do to make sure you're content with your build is
to check the md5 hash of the compiled a.out file that is generated from
running make. You can check the hash generated against the last record
of the md5sum hash in the git log.

	make
	md5sum a.out
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes --num-callers=20 --track-fds=yes ./a.out

