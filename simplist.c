#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <time.h>

/**
  * SimPLISTic - a.k.a jlutil(j) - A plutil(1) clone only better:
  *
  * About: 
  *
  * I wrote this because I was A) fed up with AAPL's plutil B) unhappy with the then port for iOS in the Erica Utils
  * package (Ah, nostalgia!) and C) realized I needed my own solution for bplist16, which is entirely undocumented.
  * So - here it is.
  *
  * There's a makefile and this will compile across MacOS, iOS, Linux, and other POSIX
  *
  * LICENSE: FREE, ***BUT*** please give credit where due, unlike certain open sources which incorporate my code
  * but don't even bother crediting (And you now you aRE). A nice "uses code by Jonathan Levin from newOSXBook.com"
  * is all I request.
  *
  * P.S - Not checking all possible binary format malformations (Sorry, Pedro :-) - but - there's a good chance that
  *       a certain malformation which crashes this will also crash uhm... OTHER people, getting you a pretty darn
  *       useful 0-day. (You'll know what I mean when/if you find it - and let me know if you do ;-)
  *
  */

#include "color.h"
#include "base64.h"

#define VERSION	"1.0"

#define DECLARATION_XML		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
#define DECLARATION_DOCTYPE 	"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"

#define	MAGIC_XML1  0x6576206c6d783f3c
#define MAGIC_BPLIST00  0x30307473696c7062  // bplist00
#define MAGIC_BPLIST16  0x36317473696c7062  // bplist16


#ifdef LINUX
 #define ntohll be64toh
 #define ntohl be32toh
 #define ntohs be16toh
#endif
typedef unsigned char uint8_t;
typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;



const unsigned char *decodeString(unsigned char *,int, int BPListVer);
const unsigned char *decodeUnicodeString(unsigned char *,int, int BPListVer);
const unsigned char *decodeUTF8String(const unsigned char *,int,int BPListVer);
int getRef (unsigned char *Marker);
int getCountAndAdvanceMarker (unsigned char **PtrToMarker);
int getCountAndAdvanceMarker16 (unsigned char **PtrToMarker);

#ifdef WANT_MAIN
int g_color = 0;
int g_debug = 0;
const char *ver[] = {
		"@(#) \tPROGRAM: jlutil   PROJECT: J's tools (from NewOSXBook.com)\n",
		"@(#) AAPL - you should really think about simplifying those darn XMLs. It's 2017, not 1987." };

#else
extern int g_color ;
extern int g_debug;
#endif

int g_nest = 0 ;
int g_verbose  = 0;

const char *g_lastKey ="";

int g_format = 0;

int g_input = 0;
void setPlistSize(int InputSize)
{
	g_input = InputSize;

}

int getPlistSize ()
{
	return g_input;
}
int getInputSize(void)
{
	return g_input;
}
void indent ()
{
	static int first =1;

	int nest = g_nest;

	if (!first || g_format ==0) printf("\n");

	else  first =0;

	while (nest-- > 0) { printf("   ");}


}

const char *decodeData (unsigned char *Marker, int BPListVer)
{
	static char *returned = NULL;

	if (returned) free(returned); // don't want no mem leak here :-)
	int extra = 0;
        uint32_t count = (*Marker) & 0x0f;

	if (BPListVer == 16)
	{
		count = Marker[1];
	}
	else if (count == 0x0F) {
			// int count follows
			count = Marker[1];
			if ((count & 0xf0) != 0x10) { fprintf(stderr,"expected int, got 0x%x\n", count); return "?";}
			
			// Otherwise
			count = count  & 0x0f;
			switch (count)
			{
				case 0: // 1 byte
					count= Marker[2];
					extra =2;
				break;
				case 1:
					count = (Marker[2] << 8 ) + Marker[3];
					extra =3;
					break;
				case 2:
					count = (Marker[4] << 16) + (Marker[3] << 8) + Marker[2];
					extra = 4;
					break;
				default:
			
				 fprintf(stderr,"Not set up to copy more than 4 bytes for an int\n"); return "?";
			}
			

			//count = *countFollows;
		}

	// Sanity check on count would go here
	
	Marker++;
	returned = malloc (count *2);
	//memcpy(returned, Marker + extra , count);
	
	int i = 0, j =0;
        int lastBlock = count %3;
	for (i = 0 ; i < count - lastBlock ; i+=3)
	{
	encodeblock (Marker+extra+i, (unsigned char *)returned+j, 3);
	j +=4;
	}

	if (lastBlock) encodeblock (Marker+extra+i, (unsigned char *)returned+j, lastBlock);
	returned[j+4] = '\0';
	return (returned);


} // decodeData

void encodeUTF8 (unsigned char *Returned, int *Index, uint32_t Value) {

	// Algorithm is described pretty clearly in UTF8 page in wikipedia

	char	byte;
	
	int i = *Index;
	if (Value > 0x10000)
	{
		byte =  0xf0 + ((Value & 0xfc0000) >> 18);	
		Returned[*Index] = byte;
		(*Index)++;

		byte = 0x80 + ((Value & 0x03f000) >> 12);
		Returned[ *Index] = byte;
		(*Index)++;

		byte = 0x80 + ((Value & 0x000fc0) >> 6);
		Returned[ *Index] = byte;
		(*Index)++;
		
		byte = 0x80 + (Value & 0x00003f) ;
		Returned[ *Index] = byte;
		(*Index)++;
		
	}
	else if (Value  > 0x7ff) {
		byte = 0xE0 + ((Value & 0x0cf000) >> 12);
		Returned[ *Index] = byte;
		(*Index)++;

		byte = 0x80 + ((Value & 0x000fc0) >> 6);
		Returned[ *Index] = byte;
		(*Index)++;
		
		byte = 0x80 + (Value & 0x00003f) ;
		Returned[ *Index] = byte;
		(*Index)++;
	
		}
	else  if (Value < 0x7ff) {
		byte = 0xc0 + ((Value & 0x000fc0) >> 6);
		Returned[ *Index] = byte;
		(*Index)++;
		
		byte = 0x80 + (Value & 0x00003f) ;
		Returned[ *Index] = byte;
		(*Index)++;
		
		}
	else
		{
		printf("NOT YET 0x%x\n", Value);
		}
	

}

const  unsigned char *unicodeToUTF8 (unsigned char *Str, int Size)
{
	int count = Size ;/// 2;
	unsigned char * returned = malloc(count*5);

	int i = 0 ;
	int adj = 0;

	uint32_t actual = 0;
	
//	printf("UNICODETOUTF8 : %d\n", Size);
	// TODO - rewrite as short..
	int32_t j = 0;
	for (i = adj; i < count *2 ; i++)
	{
	
		if (Str[i] == '\0')
		{
			if (Str[i] > 0x7f) {
			     encodeUTF8 (returned, &j, Str[i+1]);
			}
			else { 
				 // 7-bit ASCII
				returned[j] = Str[i+1];
				j++;
			}
			i++;
			continue;
		};

		if (Str[i] >= 0xd8 && Str[i] <=0xdf) {
			actual = 0;
			unsigned short surr =  ntohs(*( (short *) (Str+i)));
			surr -= 0xd800;
			actual += surr * 0x400;
			surr = ntohs(*( (short *) (Str +i + 2)));
			surr -= 0xdc00;
			actual += surr;
			actual += 0x10000;
			// Now spit as UTF-8!

			encodeUTF8 (returned, &j, actual);
			   i+=3;
			continue;
		}
		
			actual = (Str[i] << 8 ) +
				 (Str[i+1] );

			encodeUTF8 (returned, &j, actual);
			i+=1;

		  
		
	} // count
	
	returned[j] = '\0';
	return(returned);

} // unicodeToUTF8
const  unsigned char *decodeUnicodeString (unsigned char *Marker, int Encap, int BPListVer)
{
	static unsigned char *returned = NULL;
	if (returned) free(returned);

	int count = (BPListVer == 16 ? 
			getCountAndAdvanceMarker16(&Marker) :
			getCountAndAdvanceMarker(&Marker));

	return (unicodeToUTF8(Marker, count));

}

const unsigned char *decodeUTF8String (const unsigned char *Marker, int Encap, int BPListVer){
	if (BPListVer == 16)
	{
		// BPLIST16 style
		int count = Marker[0] & 0x0f;
		if (count != 0xf)
		{
		   return (Marker + 1);
		}
		else {
			if (Marker[1] == 0x11) {
				count = Marker[2];
				return (Marker+3);
			}
			else if (Marker[1] == 0x12) {
				count = Marker[2] + (Marker[3] <<8);
				return (Marker+4);
			}
			else {fprintf(stderr,"UNKNOWN\n"); exit(0);}

			}

	}
	else
	return ((decodeString((unsigned char *)Marker, Encap, 16)));

}

const unsigned  char *decodeString (unsigned char *Marker, int Format, int BPListVer)
{
	static unsigned char returned[4096];
	unsigned char *m = Marker;

	uint32_t count =0 ;

	if (BPListVer == 16) {
		count =  getCountAndAdvanceMarker16 (&m);

		}


	else
	 { 
		count =  getCountAndAdvanceMarker (&m);
	 }
	
	if (count > 4096) { 
		fprintf(stdout, "String count (%x) > 4K\n", count); 
		exit(1);

			return ((unsigned char *)"String too large"); }

	strncpy((char *)returned,(char *) ( m) , count);
	returned[count] = 0;
	
	//@TODO: escape if string contains < or >

	char *lt = strchr ((char *)returned, '<');
	if (lt) { 
		  *lt ='x';
		}
	char *amp = strchr ((char *)returned,'&');
	if (amp) *amp='X';

			
	return (returned);
		
}


const unsigned char *decodeKey (unsigned char *Marker, int BPListVer)
{
	// We expect keys to be 0x50, 0x60 or 0x70 only

	switch (*Marker & 0xF0)
	{
		case 0x50: return (decodeString (Marker, 1, BPListVer));
		case 0x60: return (decodeUnicodeString (Marker, 1, BPListVer)); 
		case 0x70: return (decodeUTF8String (Marker, 1,BPListVer));
		case 0xe0: return ("NULL");
		default: fprintf(stderr, "Key name is not string?? Marker was 0x%x\n", *Marker);

			 return("???");
		

	} // end switch


} ; // end decodeKey

static uint64_t top_object = 0;
uint64_t offset_table_offset = 0;
int object_ref_size = 0;
int offset_int_size = 0 ;
unsigned char *offset_table_ptr = NULL;

int getObjectRefSize(void *ignored_for_now)
{
	//printf("OBJ REF IS %d\n", object_ref_size);

	return object_ref_size;
}


int getRef (unsigned char *Marker)
{
	int r = 0;
	switch (getObjectRefSize(0))
	{
		case 1: return (Marker[0]); break;
		case 2: r= ((Marker[0] << 8 )+ Marker[1]); 
			return (r);
			break;
		default: printf("NOT YET\n"); exit (0);
	}

} // getRef


uint64_t getTopObject()
{
	return top_object;
}

uint32_t num_objects = 0;

int initializeOffsetTable(unsigned char *Buf, int Size)
{

#pragma pack(0)
typedef struct  {
    uint8_t     unused[5];
    uint8_t     sortVersion;
    uint8_t     offsetIntSize;
    uint8_t     objectRefSize;
    uint64_t    numObjects;
    uint64_t    topObject;
    uint64_t    offsetTableOffset;
} bplistTrailer;

	bplistTrailer  *trailer = (bplistTrailer *) &Buf[Size - sizeof (bplistTrailer)];

#pragma pack()
	offset_table_offset = ntohll(trailer->offsetTableOffset);
	top_object = ntohll(trailer->topObject);
	object_ref_size = trailer->objectRefSize;
	offset_int_size = trailer->offsetIntSize;

	num_objects = ntohll(trailer->numObjects);
	

#if 0
	fprintf(stdout, "Unused: %d, %d, %d, %d, %d\n", trailer->unused[0],

			trailer->unused[1],
			trailer->unused[2],
			trailer->unused[3],
			trailer->unused[4]);

#endif


	offset_table_ptr = &Buf[offset_table_offset];
	
// printf("OFFSET TABLE: %x %x\n", offset_table_ptr[0], offset_table_ptr[1]);
	
	return 0;

} // initializeOffset


int initializeOffsetTable16(unsigned char *Buf, int Size)
{

#pragma pack(0)
typedef struct  {
    uint8_t     unused[5];
    uint8_t     sortVersion;
    uint8_t     offsetIntSize;
    uint8_t     objectRefSize;
    uint64_t    numObjects;
    uint64_t    topObject;
    uint64_t    offsetTableOffset;
} bplistTrailer;

	bplistTrailer  *trailer = (bplistTrailer *) &Buf[Size - sizeof (bplistTrailer)];

#pragma pack()
	offset_table_offset = ntohll(trailer->offsetTableOffset);
	top_object = ntohll(trailer->topObject);
	object_ref_size = trailer->objectRefSize;
	offset_int_size = trailer->offsetIntSize;

	num_objects = ntohll(trailer->numObjects);
	

#if 0
	fprintf(stdout, "Unused: %d, %d, %d, %d, %d\n", trailer->unused[0],

			trailer->unused[1],
			trailer->unused[2],
			trailer->unused[3],
			trailer->unused[4]);

#endif


	offset_table_ptr = &Buf[offset_table_offset];
	
// printf("OFFSET TABLE: %x %x\n", offset_table_ptr[0], offset_table_ptr[1]);
	
	return 0;

}


void dumpOffsetTable(void *ignored_for_now)
{
	int i = 0;
	fprintf(stdout,"Trailer: %d objects, %d obj ref size,  %d int size, %lld is top object, 0x%llx is offset table offset\n",
			num_objects,
			object_ref_size,
			offset_int_size,
			top_object,
			offset_table_offset);

	for (i = 0; i < num_objects; i++)
	{
		uint32_t offset = offset_table_ptr[i];
		if (offset_int_size == 2)
			offset = ntohs(*( (uint16_t*) &(offset_table_ptr[i*offset_int_size])));


		printf("Object %d: 0x%x\n", i,  offset);
	}

}

int lookupOffset (unsigned int Offset)
{
	if (!offset_table_ptr)
	{
		fprintf(stderr, "Called to lookup an offset before offset table initialized. this is wrong\n");
		exit(5);
	}


	if (g_debug)
	printf("returning 0x%x (Offset %d, int size %d)\n", offset_table_ptr [Offset * offset_int_size], Offset, offset_int_size);
	
	int val;
	switch (offset_int_size)
	{
		case 1:
			return (offset_table_ptr [Offset * offset_int_size]);

		case 2:
			val = ntohs  (((uint16_t *) offset_table_ptr) [Offset]);
			return (val);	
		
		case 4:
			return (ntohl  (((uint32_t *) offset_table_ptr) [Offset]));
		
	}


	fprintf(stderr,"Unknown offset int size: 0x%x\n", offset_int_size);
	return 0;
}



char *getObjectType(unsigned char Marker)
{
	switch (Marker & 0xF0)
	{
	    case 0x00:
		{
			return ("");
			if (Marker == 8) {  return ("false"); }
			if (Marker == 9) { return ("true"); }

			break;
		}
	
	    case 0x10: return ("integer");
	    case 0x20: return ("real");
	    case 0x30: return ("date");
	    case 0x40: return ("data");
	    case 0x50: return ("string"); 
	    case 0x60:  return ("string") ;

	    case 0x70: return( "UTF-8");

	    case 0x80: return ("UID");
	    case 0xA0: return ("array");

	    case 0xD0: return ("dict");
		
	    case 0xE0: return ("null");
	   // case 0xe0: return ("") ; // presumably, NULL


		}

			return ("unknown?Tell J!");

} // getObjectType

int getCountAndAdvanceMarker16 (unsigned char **PtrToMarker)
{
    unsigned char *marker = *PtrToMarker;
    int count = *marker & 0x0f;

    int type = *marker & 0xf0;

    if (count == 0x0F) {
	// int count follows
	count = marker[1];
	 if ((count & 0xf0) != 0x10) { fprintf(stderr,"Expected int!\n"); exit(5); } 
	// next is how many bytes.
	count = count & 0x0f;
	
	
	switch (count)
	{
		case 1: count = marker[2]; marker +=3; break;
		case 2: count = (marker[2] )  + (marker[3] << 8); marker +=4 ;

				break;
		default: printf("..UNHANDLED COUNT : %d\n", count); exit(1);
	}

		*PtrToMarker = marker;

		return (count);
	} // count == 0x0f
    else {
	  *PtrToMarker= *PtrToMarker + 1;
    	if (type == 0x20){
	//	printf("COUNT is : %d (marker[0] = %x\n",count, marker[0]);
	count = 1 << (count); 
		//printf("COUNT: %d\n",count);
	}

	  
      
	  return (marker[0] & 0xf);
	};
} // getPtrAnd

int getCountAndAdvanceMarker (unsigned char **PtrToMarker)
{
	            unsigned char *marker = *PtrToMarker;
		    int count = *marker & 0x0f;
		    if (count == 0x0F) {
			// int count follows
			 count = marker[1];
			 if ((count & 0xf0) != 0x10) { fprintf(stderr,"Expected int!\n"); exit(5); } 
			// next is how many bytes.
			count = count & 0x0f;
			
			switch (count)
			{
				case 0: count = marker[2]; marker +=3; break;
				case 1: count = (marker[2] <<8)  + marker[3]; marker +=4 ; break;
				default: printf("UNHANDLED COUNT : %d\n", count); exit(1);
			}

			*PtrToMarker = marker;

			return (count);
			} // count == 0x0f
		    else {
			  *PtrToMarker= *PtrToMarker + 1;
			  return (marker[0] & 0xf);
			};
} // getPtrAnd


const char *decodeObjectNew (unsigned char *Data, int Off, int *NextObject, int Format)
{
    // This is ripped out of the only documentation I know - CF's CFBinaryPList.c 
	int pos = 0;
	static int recursionLevel = 0;


#define DECODED_OBJECT_BUFSIZE 	(32*4096)
	char *buf = NULL;

enum {
	NULL_OBJECT	= 0,
	BOOL_FALSE 	= 0x8,
	BOOL_TRUE	= 0x9,
	URL_NO_BASE	= 0xc,
	URL_BASE	= 0xd,
	UUID		= 0xe,
	FILL		= 0xf,
	DATE		= 0x33,
	END_MARKER	= 0xe0,
};

	int ObjectSize = 0;

	switch (Data[Off]) {

		case NULL_OBJECT: ObjectSize = 1; *NextObject = Off + ObjectSize; return (strdup("NULL"));
		case BOOL_FALSE:  ObjectSize = 1; *NextObject = Off + ObjectSize;  return (strdup("false"));
		case BOOL_TRUE:	  ObjectSize = 1; *NextObject = Off + ObjectSize; return (strdup("true"));
		case URL_NO_BASE: ObjectSize = 1; *NextObject = Off + ObjectSize; return (strdup("urlnobase"));
		case URL_BASE:	  ObjectSize = 1; *NextObject = Off + ObjectSize; return (strdup("urlbase"));
		case UUID:	  ObjectSize = 17; *NextObject = Off + ObjectSize; return (strdup("uuid"));
		case FILL:	  ObjectSize = 1; *NextObject = Off + ObjectSize;  return (strdup("fill"));
		case DATE:	  // 8 byte float follows, big-endian bytes
				  ObjectSize = 8; *NextObject = Off + ObjectSize;  return (strdup("date"));

		case END_MARKER: ObjectSize = 1;  *NextObject = Off + ObjectSize;  return (strdup("NULL"));
	}

	// If we're still here this is one of the other object types

enum {
	INT		= 0x10,
	REAL		= 0x20,
	DATA		= 0x40,
	STRING_ASCII	= 0x50,
	STRING_UNICODE	= 0x60,
	STRING_UTF8	= 0x70,
	UID		= 0x80,
	ARRAY		= 0xA0,
	ORDERED_SET	= 0xB0,
	SET		= 0xC0,
	DICT		= 0xD0,
};

	
	int inObject = 0;
	int byteLen = 0;
	int stringStart = 0;
	uint64_t val = 0;

	char *returned = NULL;
	int objectType = Data[Off] & 0xF0;
	switch (objectType) {


		case INT:	

			// Caveat: CFBinaryPList.c claims:
			//   0001 0nnn       ...    # of bytes is 2^nnn, big-endian bytes
			// Wrong: Size in a bplist16 is nnn, not 2^nnn...

			ObjectSize =   Data[Off] & 0xf;
			if(g_debug)printf("INT: OBJECT SIZE: %d (%d)\n", ObjectSize, Data[Off] &0xf);
			memcpy(&val, Data+Off+1, ObjectSize);
			(ObjectSize)++; // Because of first byte
			*NextObject = Off + ObjectSize; 
			if(g_debug)printf("INT @0x%x IS %d bytes (0x%x) - NEXT IS %x\n", Off,ObjectSize, Data[Off], *NextObject);
			
			returned = (char *) malloc(21);
			sprintf(returned,"%llu", val);
			return (returned);

		case REAL:	//   0010 0nnn       ...    # of bytes is 2^nnn, big-endian bytes
			ObjectSize =  (1 << (Data[Off] & 0xf));
			//printf("REAL: OBJECT SIZE: %d (%d)\n", ObjectSize, Data[Off] &0xf);
			(ObjectSize)++; // Because of first byte
			*NextObject = Off + ObjectSize; 
			return (strdup("real"));

		case DATA:	//   0100 nnnn [int]  nnnn is number of bytes unless 1111 then int count follows, followed by bytes
		case STRING_ASCII:	//	= 0x50,
		case STRING_UNICODE:	//	= 0x60,
		case STRING_UTF8:	//	= 0x70,
			ObjectSize = Data[Off] & 0xf;

			if (ObjectSize == 0xf)
			   {
				// Next up is an integer element
				
				if ((Data[Off + 1] & 0xf0) != INT) {
					fprintf(stderr,"Error: Element at %x is not an integer (%x)\n",
						Off +1, Data[Off+1]);
					exit(0);
					*NextObject = 0;
					return (strdup("Malformed"));
				}
				// Otherwise
				byteLen = Data[Off + 1] & 0xf;
				ObjectSize = 0;
				memcpy(&ObjectSize, Data + Off + 2,byteLen);
				if (objectType == STRING_UNICODE) { 
					if (g_debug)printf("-UNICODE: Doubling 0x%x => 0x%x\n",byteLen,
					ObjectSize <<1);  
					ObjectSize <<=1;
					}

				int t = ObjectSize;
				ObjectSize += byteLen +1;

				stringStart = Off+byteLen+ 1 +1;
				byteLen = t;
			   }
			else {
				stringStart = Off + 1;
				byteLen = ObjectSize;
				if (objectType  == STRING_UNICODE) 
				{ if (g_debug) printf("--UNICODE@0x%x Doubling 0x%x => 0x%x\n",Off, byteLen,
					ObjectSize <<1);  
					byteLen <<=1;
					ObjectSize <<=1;

				}


				}

			// We know Object Size at this point

			(ObjectSize++); // Because of first byte

			*NextObject = Off + ObjectSize; 
			if (g_debug)	printf("OBJECT @0x%x: 0x%x SIZE: %x - Next object after %s string will be 0x%x\n", 
				Off, Data[Off], ObjectSize, (Data[Off] & 0xf0) == STRING_UNICODE ? "Unicode" : "normal", *NextObject);
	
			if ( objectType == STRING_UNICODE ) {
				// Want to flip bits
				short *a = (short *) malloc ((sizeof(short) * byteLen/2));
				int i = 0;
				for (i = 0 ; i < byteLen/2;i++)
				{
					a[i] = ntohs(((short *)(Data + stringStart))[i]);


				}
				return ((char *) (unicodeToUTF8(((unsigned char *)a), byteLen)));
			}

		
			if (objectType == DATA) {

				int i = 0;
				returned = (char *) malloc ((*NextObject - stringStart) * 10);
				returned[0] = '\0';
				for (i = stringStart; i < *NextObject; i++)
				{

				   sprintf(returned + strlen(returned), "\\x%02X", Data[i]);
				}
				return (returned);

			}
		
			{
			char *x = strdup (Data + stringStart);
			return x;
			}
		

			
		case UID:		//   1000 nnnn       ...             // nnnn+1 is # of bytes
			ObjectSize = (Data[Off] & 0xf) + 1;

			(ObjectSize)++;
			*NextObject = Off + ObjectSize; 	
			return (strdup("uid"));
	
		case ORDERED_SET:	//	= 0xB0,
			ObjectSize = Data[Off] & 0xf;
			*NextObject =  Off + 1;
			return (strdup("false"));
		case SET:
			ObjectSize = Data[Off] & 0xf;
			*NextObject =  Off + 1;
			return (strdup("true"));
			
		case ARRAY:		// 	= 0xA0,

			ObjectSize = Data[Off] & 0xf;
			if ((ObjectSize == 0xf) || (!ObjectSize))
			   {
				if(g_debug) printf("NEXT OBJECT for %x(%x) is @%x\n", Data[Off], Off, Data[Off+1]);

				memcpy(NextObject, Data + Off + 1, sizeof(int));
				(*NextObject)++;
			   }

			// Need to recurse here, too

			inObject = Off + 1 + 8;

			returned = malloc(DECODED_OBJECT_BUFSIZE);
			returned[0] = '\0';
			int o = 0;
			if (recursionLevel) strcat (returned,"\n");

			while (inObject < *NextObject) {
			recursionLevel++;
			int r = 0 ; 
			for (r = 0 ; r < recursionLevel; r++) 
				strcat (returned,"   ");
			char *dob = decodeObjectNew(Data, inObject, &ObjectSize, Format);
		//	printf("--FREEING  %p (%s)\n",  dob,dob);
			sprintf(returned + strlen(returned), "%d: %s\n", o, dob);
			free(dob);
			//printf("--OK\n");
			inObject = ObjectSize;
			recursionLevel--;
			if (g_debug)	printf("ARRAY -- SIZE: %x , IN OBJECT NOW %x (END: %x)\n", ObjectSize, inObject, *NextObject);
				o++;
			}
			///if (g_debug) printf ("**NEXT IS %x**\n", *NextObject);
			

			return (returned);

		case DICT:  //  dict    1101 nnnn [int]   keyref* objref* // nnnn is count, unless '1111', then int count follows
			// For Bplist16:
			
			returned = malloc(DECODED_OBJECT_BUFSIZE);
                        returned[0] = '\0';
			strcat (returned,"\n");

			ObjectSize = Data[Off] & 0xf;
			if ((ObjectSize == 0xf) || (!ObjectSize))
			   {
				memcpy(NextObject, Data + Off + 1, sizeof(int));
			   }

			// Need to recurse here on keys
			
			 inObject = Off + 1 + 8;

			if (g_debug) { printf("Dict @0x%x-0x%x\n", Off, *NextObject);  }
			int  k = 1;

			while (inObject < *NextObject) {
				recursionLevel++;
				if (k) {
				int r = 0 ; for (r = 0 ; r < recursionLevel; r++) strcat(returned,"   ");
					}
				
			 char *dob = decodeObjectNew(Data, inObject, &ObjectSize, Format);
                        sprintf(returned + strlen(returned), "%s", dob);
		//	printf("FREEING %s - %p\n", dob, dob);
                        free(dob);
			//printf("Ok\n");

				if (k) strcat(returned, ": ");
				else strcat (returned, "\n");
				 k = 1-k;

				recursionLevel--;
				inObject = ObjectSize;
				if (g_debug) printf("SIZE: %x , IN OBJECT NOW %x (End: %x)\n", ObjectSize, inObject, *NextObject);
			}

		(	*NextObject)++;
			return (returned);

		default:
			NextObject = 0;
			printf ("%x @0x%x: MALFORMED\n", Data[Off], Off);
			return ("Malformed");

	}

	
#if 0
OBJECT TABLE
        variable-sized objects

        Object Formats (marker byte followed by additional info in some cases)
        null    0000 0000                       // null object [v"1?"+ only]
        bool    0000 1000                       // false
        bool    0000 1001                       // true
        url     0000 1100       string          // URL with no base URL, recursive encoding of URL string [v"1?"+ only]
        url     0000 1101       base string     // URL with base URL, recursive encoding of base URL, then recursive encoding of URL string [v"1?"+ only]
        uuid    0000 1110                       // 16-byte UUID [v"1?"+ only]
        fill    0000 1111                       // fill byte
        int     0001 0nnn       ...             // # of bytes is 2^nnn, big-endian bytes
        real    0010 0nnn       ...             // # of bytes is 2^nnn, big-endian bytes
        date    0011 0011       ...             // 8 byte float follows, big-endian bytes

        data    0100 nnnn       [int]   ...     // nnnn is number of bytes unless 1111 then int count follows, followed by bytes
        string  0101 nnnn       [int]   ...     // ASCII string, nnnn is # of chars, else 1111 then int count, then bytes
        string  0110 nnnn       [int]   ...     // Unicode string, nnnn is # of chars, else 1111 then int count, then big-endian 2-byte uint16_t
        string  0111 nnnn       [int]   ...     // UTF8 string, nnnn is # of chars, else 1111 then int count, then bytes [v"1?"+ only]
        uid     1000 nnnn       ...             // nnnn+1 is # of bytes
                1001 xxxx                       // unused
        array   1010 nnnn       [int]   objref* // nnnn is count, unless '1111', then int count follows
        ordset  1011 nnnn       [int]   objref* // nnnn is count, unless '1111', then int count follows [v"1?"+ only]
        set     1100 nnnn       [int]   objref* // nnnn is count, unless '1111', then int count follows [v"1?"+ only]
        dict    1101 nnnn       [int]   keyref* objref* // nnnn is count, unless '1111', then int count follows
                1110 xxxx                       // unused
                1111 xxxx                       // unused

#endif

} // decodeObjectNew


const char *decodeObject (unsigned char *marker, unsigned char *Data, int Format, int BPListVer)
{

    if (getenv("JDEBUG")) fprintf(stdout,"DECODING FROM %ld \n", marker - Data);
    uint64_t val = 0;

	
	static char decoded[4096];
    // This is ripped out of the only documentation I know - CF's CFBinaryPList.c 
	int pos = 0;
#if 0

OBJECT TABLE
        variable-sized objects

        Object Formats (marker byte followed by additional info in some cases)
        null    0000 0000                       // null object [v"1?"+ only]
        bool    0000 1000                       // false
        bool    0000 1001                       // true
        url     0000 1100       string          // URL with no base URL, recursive encoding of URL string [v"1?"+ only]
        url     0000 1101       base string     // URL with base URL, recursive encoding of base URL, then recursive encoding of URL string [v"1?"+ only]
        uuid    0000 1110                       // 16-byte UUID [v"1?"+ only]
        fill    0000 1111                       // fill byte
        int     0001 0nnn       ...             // # of bytes is 2^nnn, big-endian bytes
        real    0010 0nnn       ...             // # of bytes is 2^nnn, big-endian bytes
        date    0011 0011       ...             // 8 byte float follows, big-endian bytes
        data    0100 nnnn       [int]   ...     // nnnn is number of bytes unless 1111 then int count follows, followed by bytes
        string  0101 nnnn       [int]   ...     // ASCII string, nnnn is # of chars, else 1111 then int count, then bytes
        string  0110 nnnn       [int]   ...     // Unicode string, nnnn is # of chars, else 1111 then int count, then big-endian 2-byte uint16_t
        string  0111 nnnn       [int]   ...     // UTF8 string, nnnn is # of chars, else 1111 then int count, then bytes [v"1?"+ only]
        uid     1000 nnnn       ...             // nnnn+1 is # of bytes
                1001 xxxx                       // unused
        array   1010 nnnn       [int]   objref* // nnnn is count, unless '1111', then int count follows
        ordset  1011 nnnn       [int]   objref* // nnnn is count, unless '1111', then int count follows [v"1?"+ only]
        set     1100 nnnn       [int]   objref* // nnnn is count, unless '1111', then int count follows [v"1?"+ only]
        dict    1101 nnnn       [int]   keyref* objref* // nnnn is count, unless '1111', then int count follows
                1110 xxxx                       // unused
                1111 xxxx                       // unused

#endif
	 //printf("MARKER: %x = %x\n", marker -Data, marker[0]);
	switch (*marker & 0xF0)
	{
	    case 0x00:
		{
			if (*marker == 8) { if (Format) {return ( "false");} else return ("<false/>"); }

			if (*marker == 9) { if (Format) {return ("true");} else return ("<true/>"); }


			break;
		}
	
	    case 0x10:
		{
        	  // int     0001 0nnn       ...             // # of bytes is 2^nnn, big-endian bytes
		  int 	count =  1 << (*marker & 0x07);
		
// fprintf(stderr, "%x: %x - Integer: %d bytes\n", pos, *marker, count);

	
		  switch (count)
			{
				case 1:
					val =  marker[1];
					break;
				case 2:
					val = ntohs(*(uint16_t *) (marker +1));
					break;
				case 4:
					val = ntohl(*(uint32_t *) (marker +1));
					break;
				case 8:
					val = ntohll(*(uint64_t *) (marker +1));
					break;
			}
	
		  
		  //printf("<integer bytes=\"%d\">%lld</integer>", count, val);
		  sprintf(decoded,"%lld", val);
		  return(decoded);
		
		  break;
		}
	    case 0x20:
		{
		        uint32_t count = getCountAndAdvanceMarker16 (&marker);
			if (Format ==0) return("0"); 
			else printf("real(%d)",count);
			marker += count;
			break;
		}
	    case 0x30: 

		{

			// date
			
			// Eight byte float follows, so reverse bytes.
			// Yes, this can be done with some be64_ or ntohll or something.
			unsigned char bytes[8] ;
			bytes[0] = (marker[8]);
			bytes[1] = (marker[7]);
			bytes[2] = (marker[6]);
			bytes[3] = (marker[5]);
			bytes[4] = (marker[4]);
			bytes[5] = (marker[3]);
			bytes[6] = (marker[2]);
			bytes[7] = (marker[1]);

			double *b = (double *) bytes;
			
			const double kCFAbsoluteTimeIntervalSince1970 = 978307200.0L;

			struct timeval tv;
			struct tm tm;
			tv.tv_sec = (uint32_t)(*b + kCFAbsoluteTimeIntervalSince1970);
			gmtime_r(&tv.tv_sec, &tm);

			// Could probably do strftime or something. Desired format is 2016-10-19T17:53:38Z
			sprintf(decoded, "%4d-%02d-%02dT%02d:%02d:%02dZ",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec);

			return (decoded);

		}
	    case 0x40:
		{
			// data
			return (decodeData(marker, BPListVer));
			break;
		}
	    case 0x50:
		{
			// if (Format == 0) { printf("<string>"); }
			return((char *)decodeString(marker, Format,BPListVer)); 
			//if (Format == 0) { printf("</string>"); }
			break;
		}
	    case 0x60:
		{
			return ((char *)decodeUnicodeString(marker, Format,BPListVer));
		}

	    case 0x70:
		{ 
			return((char *)decodeUTF8String(marker, Format,BPListVer));
		}

	    case 0xA0:
		{
		        uint32_t count = getCountAndAdvanceMarker (&marker);

		//	fprintf(stdout, "%x: %x - array of 0x%x ENTries\n", pos,  *marker,count);
			int ent = 0;

			uint32_t endOfArray = *((uint32_t *) marker);
			if (BPListVer == 16) {
			if (g_debug)
			  fprintf (stderr,"MArker is now @0x%lx pointing to End of Array: %x\n", 
				marker - Data, endOfArray);
			   marker +=4;
			   uint32_t zero = *((uint32_t *) marker);
			   if (zero) {
				fprintf(stderr,"Intriguing. Found non zero value at pos 0x%lx..\n",
				  marker - Data);
				}
			   marker += 4;
				
			}

			// Only increase nesting for first elem

			// Count could be 0!
			if (Format == 0) 		g_nest++;
			// Need to save lastKey
			char *tempKey = strdup(g_lastKey);

			for (ent = 0; ; ent ++)
				{
					// Different conditions //
					if (BPListVer == 0) {if (ent >= count) break;}
					if (BPListVer == 16) {
					 // Go by position:
					 int pos = marker - Data;
					//	printf("POS: 0x%x\n", pos);
					if (pos >= endOfArray) break;
					}

				 	indent(); 
					
				
					int ref = 0;
					int off = 0; 

					if (BPListVer == 0) { 
						ref = getRef(marker);
						off =lookupOffset(ref);
						
						if (getenv("JDEBUG")) printf("<!-- ENT: %d/%d! (%d, %d) -->", ent, count, ref, off);
						if (!off) { printf("OFF: 0 - %d\n",Data[0]);break;}
						}

					else
					{

					    off = (marker - Data);
					}
					char *type = getObjectType(Data[off]);
			
				  if (Format == 0) {
					 if (type[0]) printf("<%s>", type);
					}

				  else printf("%s%s[%d]:%s ", 
					g_color? CYAN : "", 
						tempKey, ent,
					g_color? NORMAL:"");

			  	  printf("%s", decodeObject(Data + off, Data, Format,BPListVer));

				  if (Format == 0) {
					if ((type[0] == 'd' ) && type[1] == 'i') indent();

					if (type[0])printf("</%s>", type);
					}
				if (Format) {g_nest--; indent(); g_nest++;}
	
				  if (BPListVer == 0) {
				  pos +=1; marker += getObjectRefSize(0);
					}

				  if (BPListVer == 16) {

    					unsigned char type = marker[0] & 0xf0;
					int count =  getCountAndAdvanceMarker16 (&marker);
				    if ((type == 0xa0) || (type == 0xd0))
					{
					    marker = Data + *(uint32_t *) (marker) +1;
					    count = -1;
					  
					}
				    else {

					   if (type == 0x60) { count *= 2; };
						marker += count;
					}
			if (g_debug)
			fprintf(stderr," - COUNT: %d, marker now points to %lx\n", 
					count, marker -Data);
					}

				} // end for
			if (!count && BPListVer != 16) {
				// for simplistic format, print out []
				if (Format)
				printf("%s%s[]%s",  g_color? CYAN : "",
                                                tempKey, 
                                        g_color? NORMAL:"");

			}

			free(tempKey);
			if (Format ==0 )g_nest--;
		    break;

		}

	    case 0xD0:
		{
		
		//fprintf (stdout,"MARKER: %x - next %x,%x,%x\n", *marker, marker[1], marker[2], marker[3]);
	//	fprintf(stdout,"DICTIONARY AT %x\n", marker - Data);
		   int count =  0;

		   if (BPListVer == 0) { count = getCountAndAdvanceMarker(&marker);}
		//printf("Count: %d, Marker[0]: %x, marker: %p\n", count,marker[0], marker);

			uint32_t endOfDict =  0;
			if (BPListVer == 16) {
			   marker ++;
			   endOfDict = *((uint32_t *) marker);
			if (g_debug)
			{
			   fprintf (stderr,"MArker is now at 0x%lx, End of Dict: %x\n", 
				marker - Data, endOfDict);
			}
			   marker +=4;
			   uint32_t zero = *((uint32_t *) marker);
			   if (zero) {
				fprintf(stderr,"Intriguing. Found non zero value at pos 0x%lx..\n",
				  marker - Data);
				}
			   marker += 4;
				
			}


		    if (g_debug && (g_format == 0))
			{
		  	   fprintf(stdout, "  %s<!-- %d, %d entries, 0x%x 0x%x !-->%s", 
			    g_color ? GREEN :"", 
		 	   g_nest,count, marker[1], marker[2],
			    g_color ? NORMAL: "");
			}

			g_nest++;
			int ent = 0;
			for (ent = 0; ; ent ++)
				{
					if ((BPListVer == 0) && (ent >= count)) break;
					if ((BPListVer == 16) && ((marker - Data) >= endOfDict)) break;
					if (!Format)
					{
					  indent();
					  printf("%s<key>%s", g_color ? RED: "" , g_color?NORMAL:"" );
					}

					// might want to verify that key names are 0x50,0x60,0x70 only
					if (g_color) fputs(CYAN,stdout);

					int ref =  0;
					int off = 0 ;
					if (BPListVer == 0) {

					 ref  = getRef(marker);
					 off  = lookupOffset(ref);
					}

	
					if (g_debug) { fprintf(stderr,"---DENT: %d MARKER: %p keyref: %x (Ref size : %d)\n", ent, marker,  ref, getObjectRefSize(Data)); }
  
					// printf("KeyREF: %x - off %x, marker: %p\n", ref, off,marker);

					if (BPListVer == 16) {
						off = (marker - Data);
					}

					g_lastKey =(char *) decodeKey(Data + off, BPListVer);

					if (BPListVer == 0) {
					ref = getRef(marker + count * getObjectRefSize(0));
					off  = lookupOffset(ref);

					}

					if (BPListVer == 16) {

				if(g_debug) fprintf(stderr,"--key MARKER: %lx = %x\n", marker -Data, marker[0]);
					int l = marker[0] & 0xf;
					if (l == 0xf)
					{
					  
						l =  marker[2] +2;
					}
			
					marker += l +1;
					off += l +1;

				if(g_debug)
					fprintf(stderr,"--value MARKER: %lx = %x\n", marker -Data, marker[0]);

					}
					// printf("OBJ REF: %x - off %x, marker: %p\n", ref, off,marker);
					char *type = strdup(getObjectType (Data[off]));
					int suppress = 0;

					if (Format && strcmp(type,"array")==0) suppress = 1;

					if (!(suppress))
					{

					if (Format) {  indent();}
					printf("%s%s%s", (g_color ? CYAN: ""), 
							g_lastKey,
							  (g_color ? NORMAL : ""));
					

					if (Format)
					{	
						printf(": ");
					}
					else 
					 { printf("%s</key>%s", g_color ? RED: "" , g_color?NORMAL:"" );
					   indent();
					}

					} // ! suppress


					// Have to do type print now due to recursion on dicts 
					if (Format ==0)
						{
						  if  (type[0]){
						  if (type[0]=='t' || type[0] == 'f') 
							printf("<%s/>", type);
						  else 
							printf("<%s>", type);
							}
						}
		
					 const char *obj = decodeObject(Data + off, Data,Format,BPListVer);
					 if (Format == 0)
						{
						if (type[0]=='t' || type[0] == 'f') {}
						else
							{
							  if(obj) printf("%s",  obj);
							}
						}
					 else   {
							if (obj) printf("%s",obj); 
							else 
							{ 
					if ((type[0] !='d') && 
					    (type[0] !='a')) // not for dicts or array
							  {
								 printf("%s", type);
	
								}

							}
						}
					//printf("</value>\n");

		
				 if (Format == 0) { 
						  if (((type[0]=='t' || type[0] == 'f')) ) {} 
						  else {
						 if (((type[0] == 'd') && (type[1] == 'i')) || (type[0] =='a') ){ 
						indent() ; } 
						if (type[0])
							printf("</%s>",type);
							}
			}
				free(type);
				// Dictionary entries are obj ref
				if (BPListVer == 0) {
				marker += getObjectRefSize(0); //offset_int_size;
				}
				else if (BPListVer == 16) {

	if (g_debug)	fprintf(stderr,"HAVE TO ADVANCE MARKER FROM %lx (%x)\n", marker - Data, marker[0]);
				    unsigned char type = marker[0] & 0xf0;
				int count =  getCountAndAdvanceMarker16 (&marker);
	
			
                                    if ((type == 0xa0) || (type == 0xd0))
                                        {
                                            marker = Data + *(uint32_t *) (marker) +1;
                                            count = -1;

                                        }
                                    else {

			if (type ==0x20) {
	/*
                          	printf(" - COUNT: 0x%x Marker: 0x%x\n", count,marker  -Data);
                        	printf(" - COUNT: 0x%x,TYPE: 0x%x, marker now points to %x\n", count, type,marker +count -Data);
*/
			}
                                           if (type == 0x60) { count *= 2; };
                                                marker += count;
                                        }

			
		// printf(", COUNT %x New Marker %x\n", count, marker - Data);
				   
				}
			} // for end;

			g_nest--;

// indent();

	//		fprintf(stdout, "</dict>");
		    
		    // dict
		    break;
		}
	    case 0xe0: {  printf("(null)");
			break; }

	    case 0x80: { 
				printf("CF$UID (%d)", *(marker+1));

				 break;
			}
	    case 0xB0: { printf("true"); break; }
	    case 0xC0: { printf("false"); break; }
	    default:
		printf("Unknown 0x%x\n",  *marker);
	
	}; //end switch
	pos++;
	marker++;

	return "";
} //decodeObject


static inline char *getNextNonWhiteSpace(char *Pos)
{
	int i = 0 ;
	while ((Pos[i] == ' ') ||
	       (Pos[i] == '\n') ||
	       (Pos[i] == '\t') ||
	       (Pos[i] == '\r')) {
		if (Pos[i] == 0) return NULL;
			i++;
		}


	return (Pos+i);

}

static inline char *getXMLElementEnd (char *Pos)
{
	int i = 0 ;
	
   	if (Pos[i] == '/') i++;
	while (isalpha(Pos[i])) i++;
	
	
	
	if ((Pos[i] != '>')  &&
		((Pos[i] == '/')  && (Pos[i+1] !='>'))) {
		printf("Malformed element\n");
	}

	return (Pos +i);
}

static inline char *getNextXMLElement(char *Pos)
{
        
	char *nws = getNextNonWhiteSpace(Pos);
	while (nws[0] != '<')
	   { 
	        nws++;
		nws = getNextNonWhiteSpace(nws);
	   }
	nws++;
	return (nws);

}
static inline char *getNextTag(char *Pos)
{
	
	int i = 0;
	while (Pos[i] != '<')
	   { 
		i++;
	   }
	return (Pos +i);
	

}

int do_XML(const unsigned char *Buf, int Size)
{

    // We want to convert XML to simplist - and do so without using on 
    // external XML APIs such as those of libxml, etc.
    char *nextElem = NULL;

    char *workBuf = strdup((char *)Buf);

#define XML_MAGIC "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"

    if (strncmp((char *)workBuf, XML_MAGIC, strlen(XML_MAGIC)) != 0)
	{
		fprintf(stderr,"Unable to find XML declaration. Is this properly formatted XML?\n");
		return 1;
	}

    // Otherwise, next up is DOCTYPE

#define DOCTYPE_MAGIC "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" "http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"

    // Apple plists HAVE to have the doctype, but third party or others don't.

    char *plist = strstr((char *)workBuf,"<plist ");

    char *simplistify( char *XMLBlob, int Size);
    simplistify (plist, strlen(plist));

	return 0;
} // do_XML


int do_bplist16(unsigned char *Buf, int Size)
{
   // bplist16 apparently has no trailer.

    
    unsigned int pos;

    g_format |= 0x80;
    pos = sizeof(MAGIC_BPLIST16);


    // At this pos, we find our top level element. This is commonly an array or a dict,
    // so we can just call decodeObject, which will recurse anyway.

    int objSize = Size;
    const char *obj = decodeObjectNew ( Buf,  pos, &objSize, g_format);


    if (!obj) { fprintf(stderr,"Error decoding first object\n"); exit(2);}
    else printf("%s\n", obj);

//	printf("%d, %d\n", Size, objSize);




   return (0);
}; // do_bplist16

int do_bplist00(unsigned char *Buf, int Size)
{
    // Start right after the BPlist header, which we know.
    // Sanity checks should go here...
    // If we're here, we trust the format :-)

    initializeOffsetTable(Buf, Size);
    if (g_debug) dumpOffsetTable(Buf);
	
    char *type = NULL;

    uint32_t off = lookupOffset(getTopObject());
    if (g_format == 0) 
		{
			fprintf(stdout, DECLARATION_XML "\n");
			fprintf(stdout, DECLARATION_DOCTYPE "\n");
			fprintf(stdout,"<plist version=\"1.0\">\n");
			// It's tempting to say <dict> here, but few property lists
			// actually use an anonmous array here (e.g. IOAcceleratorCommandSignposts
			
			// We thus need to resolve the top object
			type = getObjectType(Buf[off]);
		 	fprintf(stdout,"<%s>", type);
		}


	decodeObject (& Buf  [lookupOffset(getTopObject())], Buf, g_format, 0);
	if (g_format == 0)
	fprintf(stdout,"\n</%s>\n</plist>",type);

	printf("\n");

   return (0);
}; // do_bplist00


#define MAX_INPUT_SIZE	4*1024*1024 // if you get property lists of > 4M, change this.

void usage()
{
	fprintf(stderr,"Usage: jlutil [-v] [-x|j] [file]\n");
	fprintf(stderr,"Where: -x: xml format   -j: JSON format (not yet)   Default format is simPLISTic®\n");
	fprintf(stderr,"Where: -v: validate (find format errors of keep quiet)\n");

	fprintf(stderr, "\nRedirect a file over stdin (you don't need '-'), or specify a filename\n");
	fprintf(stderr, "\nThis is J's simPLISTic " VERSION " - supports bplist00 and (testing) bplist16\nanother free tool from http://NewOSXBook.com/\n");
	
	fprintf(stderr, "\nFor questions/comments, please use http://NewOSXBook.com/forum/\n");
	exit(1);

}



#ifdef WANT_MAIN
int main (int argc, char **argv)
{


    int inputFD = 0; // stdin by default


    g_color = (int)(uint64_t) getenv("JCOLOR");
    g_debug = (int)(uint64_t) getenv("JDEBUG");

    int g_validate = 0;
    g_format = 1;
    if (getenv("JFORMAT")) { g_format=0;}
    if (argc > 1)
	{
		if (strcmp(argv[1],"-h") ==0) usage();
		else
		if (strcmp(argv[1],"-v") ==0) g_validate = 0;
		else
		if (strcmp(argv[1],"-x") ==0) g_format = 0;
		else
		if (strcmp(argv[1],"-j") ==0) {fprintf(stderr,"JSON: Not Yet\n"); exit(0); }
		else if (argv[1][0] == '-') usage();
	

	}

    else {
		// pipe mode with defaults
		getenv("SIMPLISTIC_FORMAT");
	 }


    // autodetect our input :-)

    int arg = 1;
    for (arg = 1; arg < argc ; arg++)
	{
 		int rc = 0;

		if (argv[arg][0] != '-')
		  {
		   rc=  access(argv[arg], // const char *pathname, 
				 R_OK);   // int mode);

		if (rc == 0)
		{
			// That's our file, probably..
			inputFD = open (argv[arg], O_RDONLY);
			if (inputFD < 0) {
			fprintf(stderr,"Unable to open %s\n", argv[arg]);
			return (1);
			}
		}
		else {
			fprintf(stderr,"Unable to open %s\n", argv[arg]);
			return (1);

		     }

		}
		

	}
	
    // Read the entire file into memory

    struct stat stBuf =  {0};


    int rc =  fstat(inputFD, // int fildes, 
	           &stBuf); //struct stat *buf);

    if (rc) { fprintf(stderr,"Unable to process binary plist - can't stat\n"); exit(1); }

    unsigned char *buf;
    int inputSize = stBuf.st_size;
  
		  // Malloc and read instead;
		if (!inputSize) inputSize = MAX_INPUT_SIZE;

		buf = (unsigned char *) malloc (inputSize);
		rc = read (inputFD, buf , inputSize);
		if (g_debug) fprintf(stderr,"read %d/%d bytes\n", rc,inputSize);

		if (rc != inputSize) {
			if (inputSize != MAX_INPUT_SIZE) {
 		       fprintf(stderr,"Can neither mmap nor read data - %s\n", strerror(errno)); exit(1); 
			}
			else inputSize = rc;
		   }
	setPlistSize(inputSize );

    uint64_t	*magic = (uint64_t *)buf;

    switch (*magic)
	{
		case MAGIC_XML1:
			// XML Plist already - convert to simplist?
			if (g_debug) fprintf(stderr,"XML\n");
			if (g_format != 0) {
				do_XML (buf, inputSize);
				}
			else {

				fprintf(stderr,"Input is already XML formatted\n");
				return 1;
			     }

			break;

		case MAGIC_BPLIST00:
			if (g_debug)	fprintf(stderr,"Binary plist\n");
			if (argc >1 && strcmp(argv[1],"off") == 0)
			{
				initializeOffsetTable(buf, inputSize);
				dumpOffsetTable(buf);
				// ZZZ
				exit(0);
			}
			do_bplist00(buf,inputSize);
			break;

		case MAGIC_BPLIST16:
			if (g_debug)	fprintf(stderr,"Binary plist\n");
			if (argc >1 && strcmp(argv[1],"off") == 0)
			{
				initializeOffsetTable(buf, inputSize);
				dumpOffsetTable(buf);
				// ZZZ
				exit(0);
			}
			do_bplist16(buf,inputSize);
			break;

		default:
			
			fprintf(stderr,"Unrecognized file format : 0x%llx\n", *magic);
			exit(2);
	} // end switch


	return 0;


} // main
#endif
