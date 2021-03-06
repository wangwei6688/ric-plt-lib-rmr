// : vi ts=4 sw=4 noet :
/*
==================================================================================
	    Copyright (c) 2019 Nokia
	    Copyright (c) 2018-2019 AT&T Intellectual Property.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
==================================================================================
*/

/*
	Mmemonic:	mbuf_api_static_test.c
	Abstract:	Test the message buffer  funcitons. These are meant to be included at compile
				time by the test driver.

	Author:		E. Scott Daniels
	Date:		3 April 2019
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>

#include "rmr.h"
#include "rmr_agnostic.h"

// ---------------------------------------------------------------------------------------------

int mbuf_api_test( ) {
	unsigned char* c;
	int i;
	int state;
	int errors = 0;
	char*	buf;
	void*	ptr;
	rmr_mbuf_t*	mbuf;
	rmr_mbuf_t*	mbuf2;
	rmr_mbuf_t*	mbuf3;
	uta_mhdr_t*	hdr;
	unsigned char src_buf[256];
	unsigned char dest_buf[256];


	// --- dummy up a message buffer --------------------------------------------------------
	mbuf = (rmr_mbuf_t *) malloc( sizeof( *mbuf ) );
	if( mbuf == NULL ) {
		fprintf( stderr, "[FAIL] tester cannot allocate memory: mbuf\n" );
		exit( 1 );
	}
	memset( mbuf, 0, sizeof( *mbuf ) );

	mbuf->tp_buf = (void *) malloc( sizeof( char ) * 1024 );		// add a dummy header/payload
	memset( mbuf->tp_buf, 0, sizeof( char ) * 1024 );

	mbuf->header = mbuf->tp_buf;
	mbuf->alloc_len = 1024;
	mbuf->payload = PAYLOAD_ADDR( mbuf->header );
	hdr = (uta_mhdr_t *) mbuf->header;
	mbuf->xaction = hdr->xid;




	// --- test payload field  access functions ---------------------------------------------------
	memset( src_buf, 0, sizeof( src_buf ) );
	rmr_bytes2payload( mbuf, NULL, strlen( src_buf) );				// errno should be set on return
	errors += fail_if( errno == 0, "buf copy to payload with nil src returned good errno" );

	rmr_bytes2payload( NULL, src_buf, strlen( src_buf) );			// errno should be set on return
	errors += fail_if( errno == 0, "buf copy to payload with nil mbuf returned good errno" );

	mbuf->state = 1;											// force it to something to test that it was set
	rmr_bytes2payload( mbuf, src_buf, strlen( src_buf) );
	errors += fail_if( mbuf->state != RMR_OK, "buf copy to payload returned bad state in mbuf" );

	rmr_bytes2payload( mbuf, src_buf, 8192 );						// bust the limit
	errors += fail_if( mbuf->state == RMR_OK, "huge buf copy to payload returned good state in mbuf" );
	errors += fail_if( errno == 0, "huge buf copy to payload returned good state in errno" );


	snprintf( src_buf, sizeof( src_buf ), "This is some text in the buffer" );
	rmr_str2payload( mbuf, src_buf );							// this uses bytes2payload, so only one invocation needed


	// --- test meid field  access functions ---------------------------------------------------
	errno = 0;
	i = rmr_bytes2meid( NULL, src_buf, RMR_MAX_MEID );
	errors += fail_if( errno == 0, "(errno) attempt to copy bytes to meid with nil message" );
	errors += fail_if( i > 0, "(rv) attempt to copy bytes to meid with nil message" );

	errno = 0;
	i = rmr_bytes2meid( mbuf, NULL, RMR_MAX_MEID );
	errors += fail_if( errno == 0, "(errno) attempt to copy bytes to meid with nil source buffer" );
	errors += fail_if( i > 0, "(rv) attempt to copy bytes to meid with nil message" );

	errno = 0;
	strncpy( src_buf, "a very long string that should trigger an error when stuffing it into meid", sizeof( src_buf ) );		// ensure no zero byte at field length
	i = rmr_bytes2meid( mbuf, src_buf, RMR_MAX_MEID + 1 );
	errors += fail_if( errno == 0, "(errno) attempt to copy bytes to meid with large source buffer" );
	errors += fail_if( i != RMR_MAX_MEID, "(rv) attempt to copy bytes to meid with large source buffer" );
	
	errno = 0;
	i = rmr_bytes2meid( mbuf, src_buf, RMR_MAX_MEID );			// it's not 0 terminated at length expect failure
	errors += fail_if( errno == 0, "(errno) attempt to copy non-zero termianted bytes to meid with large source buffer" );
	errors += fail_if( i != RMR_MAX_MEID, "(rv) attempt to copy bytes to meid with large source buffer" );

	errno = 0;
	memset( src_buf, 0, RMR_MAX_MEID+2 );
	strcpy( src_buf, "smaller bytes" );
	i = rmr_bytes2meid( mbuf, src_buf, RMR_MAX_MEID  );
	errors += fail_if( errno != 0, "copy bytes to meid; expected errno to be ok" );
	errors += fail_if( i != RMR_MAX_MEID, "copy bytes to meid; expected return value to be max meid len" );



	errno = 0;
	snprintf( src_buf, sizeof( src_buf ), "meid-fits" );
	i = rmr_str2meid( NULL, src_buf );
	errors += fail_if( errno == 0, "(errno) attempt to copy string to meid with nil message" );
	errors += fail_if( i == RMR_OK, "(rv) attempt to copy string to meid with nil message" );

	errno = 0;
	i = rmr_str2meid( mbuf, NULL );
	errors += fail_if( errno == 0, "(errno) attempt to copy string to meid with nil source buffer" );
	errors += fail_if( i == RMR_OK, "(rv) attempt to copy string to meid with nil message" );

	errno = 0;
	i = rmr_str2meid( mbuf, src_buf );
	errors += fail_if( errno != 0, "copy string to meid; expected errno to be ok" );
	errors += fail_if( i != RMR_OK, "copy string to meid; expected return value to be RMR_OK" );

	errno = 0;
	snprintf( src_buf, sizeof( src_buf ), "meid-should-be-too-large-to-fit-in-the-meid" );
	i = rmr_str2meid( mbuf, src_buf );
	errors += fail_if( errno == 0, "(errno) attempt to copy string to meid with large source buffer" );
	errors += fail_if( i == RMR_OK, "(rv) attempt to copy string to meid with large source buffer" );


	snprintf( src_buf, sizeof( src_buf ), "test-meid" );
	rmr_str2meid( mbuf, src_buf );

	errno = 0;
	c = rmr_get_meid( NULL, NULL );
	errors += fail_if( c != NULL, "get meid with nil message buffer" );
	errors += fail_if( errno == 0, "(errno bad) get meid with nil msg buffer" );

	c = rmr_get_meid( mbuf, NULL );			// should allocate and return c
	errors += fail_if( c == NULL, "get meid with nil dest pointer (did not allocate a buffer)" );
	errors += fail_if( strcmp( c, "test-meid" ) != 0, "did not get expected meid from mbuffer" );
	if( c ) {
		free( c );
		c = NULL;
	}

	c = rmr_get_meid( mbuf, c );
	errors += fail_if( c == NULL, "get meid with a dest pointer returned no pointer" );
	errors += fail_if( strcmp( c, "test-meid" ) != 0, "did not get expected meid from mbuffer" );
	if( c ) {
		free( c );
		c = NULL;
	}


	// --- test transaction field  access functions ---------------------------------------------------
	snprintf( src_buf, sizeof( src_buf ), "xaction-test##################." );		// full 32 bytes

	errno = 0;
	i = rmr_bytes2xact( NULL, src_buf, RMR_MAX_XID );
	errors += fail_if( errno == 0, "(errno) attempt to copy bytes to xact with nil message" );
	errors += fail_if( i > 0, "(rv) attempt to copy bytes to xact with nil message" );

	errno = 0;
	i = rmr_bytes2xact( mbuf, NULL, RMR_MAX_XID );
	errors += fail_if( errno == 0, "(errno) attempt to copy bytes to xact with nil source buffer" );
	errors += fail_if( i > 0, "(rv) attempt to copy bytes to xact with nil message" );

	errno = 0;
	i = rmr_bytes2xact( mbuf, src_buf, RMR_MAX_XID + 1 );
	errors += fail_if( errno == 0, "(errno) attempt to copy bytes to xact with large source buffer" );
	errors += fail_if( i != RMR_MAX_XID, "(rv) attempt to copy bytes to xact with large source buffer" );

	errno = 0;
	i = rmr_bytes2xact( mbuf, src_buf, RMR_MAX_XID  );
	errors += fail_if( errno != 0, "copy bytes to xact; expected errno to be ok" );
	errors += fail_if( i != RMR_MAX_XID, "copy bytes to xact; expected return value to be max xact len" );

	errno = 0;
	ptr = rmr_get_xact( NULL, NULL );
	errors += fail_if( errno == 0, "get xaction with nil msg did not set errno" );
	errors += fail_not_nil( ptr, "get xaction with nil msg did not return a nil pointer" );
	
	errno = 999;
	ptr = rmr_get_xact( mbuf, NULL );
	errors += fail_if( errno == 999, "get xaction with valid msg and nil dest didn't clear errno" );
	errors += fail_not_equal( errno, 0, "get xaction with valid msg and nil dest set errno (a)" );
	errors += fail_if_nil( ptr, "get xaction with valid msg  and nil dest did not return a valid pointer" );
	if( ptr ) {
		i = strcmp( ptr, src_buf );
		errors += fail_not_equal( i, 0, "get xaction did not fetch expected string cmp return (a) was not 0" );
		free( ptr );
		ptr = NULL;
	}
	
	errno = 999;
	ptr = rmr_get_xact( mbuf, dest_buf );
	errors += fail_if( errno == 999, "get xaction with valid msg and nil dest didn't clear errno" );
	errors += fail_not_equal( errno, 0, "get xaction with valid msg and nil dest set errno (a)" );
	errors += fail_if_nil( ptr, "get xaction with valid msg  and valid dest did not return a valid pointer" );
	errors += fail_if( ptr != dest_buf, "get xaction did not return pointer to dest string" );
	if( ptr == dest_buf ) {
		i = strcmp( ptr, src_buf );
		errors += fail_not_equal( i, 0, "get xaction into dest string did not fetch expected string cmp return (a) was not 0" );
	}

	errno = 0;
	snprintf( src_buf, sizeof( src_buf ), "xact-fits" );
	i = rmr_str2xact( NULL, src_buf );
	errors += fail_if( errno == 0, "(errno) attempt to copy string to xact with nil message" );
	errors += fail_if( i == RMR_OK, "(rv) attempt to copy string to xact with nil message" );

	errno = 0;
	i = rmr_str2xact( mbuf, NULL );
	errors += fail_if( errno == 0, "(errno) attempt to copy string to xact with nil source buffer" );
	errors += fail_if( i == RMR_OK, "(rv) attempt to copy string to xact with nil message" );

	errno = 0;
	i = rmr_str2xact( mbuf, src_buf );
	errors += fail_if( errno != 0, "copy string to xact; expected errno to be ok" );
	errors += fail_if( i != RMR_OK, "copy string to xact; expected return value to be RMR_OK" );

	errno = 0;
	snprintf( src_buf, sizeof( src_buf ), "xact-should-be-too-large-to-fit-in-the-xact" );
	i = rmr_str2xact( mbuf, src_buf );
	errors += fail_if( errno == 0, "(errno) attempt to copy string to xact with large source buffer" );
	errors += fail_if( i == RMR_OK, "(rv) attempt to copy string to xact with large source buffer" );


	rmr_free_msg( mbuf );

	// ------------ trace data tests ----------------------------------------------------------------
	// CAUTION: to support standalone mbuf api tests, the underlying buffer reallocation functions are NOT used
	//			if this is driven by the mbuf_api_test.c driver

	mbuf = test_mk_msg( 2048, 0 );		// initially no trace size to force realloc

	state = TRACE_OFFSET( mbuf->header ) - PAYLOAD_OFFSET( mbuf->header );		// no trace data, payload and trace offset should be the same
	errors += fail_not_equal( state, 0, "trace offset and payload offset do NOT match when trace data is absent" );

	state = rmr_get_trlen( mbuf );
	errors += fail_not_equal( state, 0, "initial trace len reported (a) does not match expected (b)" );

	state = rmr_set_trace( NULL, src_buf, 100 );				// coverage test on nil check
	errors += fail_not_equal( state, 0, "set trace with nil msg didn't return expected 0 status" );

	state = rmr_set_trace( mbuf, src_buf, 0 );				// coverage test on length check
	errors += fail_not_equal( state, 0, "set trace with 0 len didn't return expected 0 status" );

	state = rmr_get_trace( NULL, src_buf, 100 );				// coverage test on nil check
	errors += fail_not_equal( state, 0, "get trace with nil msg didn't return expected 0 status" );

	state = rmr_get_trace( mbuf, NULL, 100 );					// coverage test on nil check
	errors += fail_not_equal( state, 0, "get trace with nil dest didn't return expected 0 status" );

	state = rmr_get_trlen( NULL );								// coverage test on nil check
	errors += fail_not_equal( state, 0, "get trace length with nil msg didn't return expected 0 status" );

	ptr = rmr_trace_ref( NULL, NULL );
	errors += fail_not_nil( ptr, "trace ref returned non-nil pointer when given nil message" );

	mbuf = test_mk_msg( 2048, 0 );		// initially no trace size to force realloc
	ptr = rmr_trace_ref( mbuf, NULL );
	errors += fail_not_nil( ptr, "trace ref returned non-nil pointer when given a message without trace data" );

	i = 100;								// ensure that i is reset when there is no trace data
	ptr = rmr_trace_ref( mbuf, &i );
	errors += fail_not_nil( ptr, "trace ref returned non-nil pointer when given a message without trace data" );
	errors += fail_not_equal( i, 0, "trace ref returned non zero size (a) when no trace info in message" );

	i = 100;
	mbuf = test_mk_msg( 2048, 113 );		// alloc with a trace data area
	ptr = rmr_trace_ref( mbuf, &i );
	errors += fail_if_nil( ptr, "trace ref returned nil pointer when given a message with trace data" );
	errors += fail_not_equal( i, 113, "trace ref returned bad size (a), expected (b)" );


	src_buf[0] = 0;
	state = rmr_set_trace( mbuf, "foo bar was here", 17 );		// should force a realloc
	errors += fail_not_equal( state, 17, "bytes copied to trace (a) did not match expected size (b)" );

	state = rmr_get_trace( mbuf, src_buf, 17 );
	errors += fail_not_equal( state, 17, "bytes retrieved from trace (a) did not match expected size (b)" );

	state = rmr_get_trlen( mbuf );
	errors += fail_not_equal( state, 17, "trace len reported (a) does not match expected (b)" );
	state = strcmp( src_buf, "foo bar was here" );
	errors+= fail_not_equal( state, 0, "compare of pulled trace info did not match" );

	state = TRACE_OFFSET( mbuf->header ) - PAYLOAD_OFFSET( mbuf->header );		// when there is a trace area these should NOT be the same
	errors += fail_if_equal( state, 0, "trace offset and payload offset match when trace data is present" );


											// second round of trace testing, allocating a message with a trace size that matches
	mbuf = test_mk_msg( 2048, 17 );			// trace size that matches what we'll stuff in, no realloc
	state = rmr_get_trlen( mbuf );
	errors += fail_not_equal( state, 17, "alloc with trace size: initial trace len reported (a) does not match expected (b)" );

	src_buf[0] = 0;
	state = rmr_set_trace( mbuf, "foo bar was here", 17 );		// should force a realloc
	errors += fail_not_equal( state, 17, "bytes copied to trace (a) did not match expected size (b)" );

	state = rmr_get_trace( mbuf, src_buf, 17 );
	errors += fail_not_equal( state, 17, "bytes retrieved from trace (a) did not match expected size (b)" );
	state = strcmp( src_buf, "foo bar was here" );
	errors+= fail_not_equal( state, 0, "compare of pulled trace info did not match" );

	i = rmr_get_trlen( mbuf );


	// ------------- source field tests ------------------------------------------------------------
	// we cannot force anything into the message source field, so no way to test the content, but we
	// can test pointers and expected nils

	buf = rmr_get_src( NULL, src_buf );					// coverage test for nil msg check
	errors += fail_not_nil( buf, "rmr_get_src returned a pointer when given a nil message" );

	buf = rmr_get_src( mbuf, NULL );					// coverage test for nil dst check
	errors += fail_not_nil( buf, "rmr_get_src returned a pointer when given a nil dest buffer" );

	buf = rmr_get_src( mbuf, src_buf );
	errors += fail_not_equalp( buf, src_buf, "rmr_get_src didn't return expexted buffer pointer" );

	buf = rmr_get_srcip( NULL, NULL );
	errors += fail_not_nil( buf, "get_srcip did not return nil when given nil pointers" );

	buf = rmr_get_srcip( mbuf, NULL );
	errors += fail_not_nil( buf, "get_srcip did not return nil when given nil destination" );

	buf = rmr_get_srcip( mbuf, src_buf );
	errors += fail_not_equalp( buf, src_buf, "rmr_get_srcip didn't return expexted buffer pointer" );

	test_set_ver( mbuf, 2 );							// set older message version to ensure properly handled
	buf = rmr_get_srcip( mbuf, src_buf );


	return errors > 0;			// overall exit code bad if errors
}
