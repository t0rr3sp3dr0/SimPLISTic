#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define CYAN    "\e[0;36m"
#define NORMAL  "\e[0;0m"


/* Not used */
char *strrstr (char *haystack, char *needle)
{
	if ((!haystack) || (!needle)) { return NULL;}

	int needleLen = strlen(needle);
	int haystackLen = strlen(haystack);

	if (needleLen > haystackLen) return NULL;

	int i = 0;
	
	for (i = haystackLen - needleLen;
	     i > 0;
	     i--)
	{
		int j = 0;
		
		for (j = 0;
		     (haystack[i + j] == needle[j]) && (j < needleLen);
		     j++);
		if (j == needleLen) {
			// FOUND!
			return (haystack+i);

		}
	    

	} // end for i

	return NULL;

} //strrstr

char *getElemContents (char *XMLBlob, char *Tag) {

	char *tagBegin = alloca(strlen(Tag) + 5);
	tagBegin[0] = '<';
	strcpy(tagBegin +1, Tag); // could be more efficient
	if (strcmp(Tag, "plist")) strcat (tagBegin, ">");

	char *tagActualBegin  = strstr(XMLBlob, tagBegin);
	// Bug: tags which are partial matches of searched tag.. 
	// should do ' ' or '>' , but whatever.
	
	// Hack: empty element:
	
	if (!tagActualBegin) {
	
			 return NULL;;

	}
	char *tagEnd = strchr (tagActualBegin, '>');
	//printf("got tag end: %p\n", tagEnd);
	if (!tagEnd) return NULL;
	char *elemContents = tagEnd +1;

	char *elemEnd = alloca(strlen(Tag) + 5);
	elemEnd[0]='<';
	elemEnd[1]='/';
	strcpy(elemEnd +2, Tag);
	strcat(elemEnd,">"); // could be more efficient

	char *sameElem = strstr(elemContents, tagBegin);
	//printf("looking for elemEnd: %s in %s\n", elemEnd, elemContents);
	char *elemEndx = strstr (elemContents, elemEnd);
	// printf("got elemEnd: %s\n", elemEndx);
	if (!elemEndx) return NULL;

	int nesting = 0;

	if (sameElem && (elemEndx > sameElem)) nesting++;

	//if (nesting) printf("SAME ELEM (%s): (%p) %d, elemEndx: %d\n",  tagBegin, sameElem,sameElem - XMLBlob, elemEndx - XMLBlob);
	while (nesting) {

		
		// So there is a nested <elem> before </elem>  
		if (sameElem) sameElem = strstr(sameElem + 1, tagBegin);
		if (! sameElem)  { // printf("!SAME ELEM - reducing nesting\n"); 
			nesting--; 
			if (elemEndx) elemEndx =  strstr(elemEndx +2, elemEnd);}
		else if (sameElem > elemEndx)  {
			// the two we saw cancel eachother out, so we are now one short
		//		printf("SAME ELEM,  but later (%d, %d), reducing nesting\n", sameElem - XMLBlob, elemEndx - XMLBlob);
			  
			 elemEndx = strstr (elemEndx + 1, elemEnd);
		}
		else { 
			
			//printf("INCREASING\n");
			nesting++;
			continue;
		}



	}

	// Out of nesting

	if (elemEndx) *elemEndx = 0;
	return (elemContents);

	

	return NULL;

}; 

char *getNextTag (char *XMLBlob) {

	static char returned[16];
	returned[0] = '\0';


	char *tagBegin = strchr (XMLBlob, '<');
	
	if (tagBegin &&
	  (tagBegin[1] == '!') &&
	  (tagBegin[2] == '-') &&
	  (tagBegin[3] == '-'))
	 {
		// comments,  like @$%#%$ netbiosd.plist
		char *comment  = strstr(XMLBlob, "-->");
		if (comment)  tagBegin = strchr (comment + 3,  '<');
		else {fprintf(stderr,"Unterminated comment!\n"); return  NULL;}
		
	
	}

	while (tagBegin && tagBegin[1] == '/') {
	  	XMLBlob = tagBegin+1; 
		tagBegin = strchr(XMLBlob, '<');

	}
	if (!tagBegin) return NULL;
	int i =  0;
	for (i = 0 ; i < 14; i++) 
	{
		if ((tagBegin[i+1] == ' ') ||
		    (tagBegin[i+1] == '>')) {
		    returned[i] = '\0';
		    return (returned);

		}

		returned[i] = tagBegin[i+1];
	

	} // end for i..
	
	
	return NULL;
}

extern int g_color;

char *simplistifyElement (char *ElementBlob, int Size, char *ArrayKey, int Nesting)  {

	char *nextElem = getNextTag (ElementBlob);

	int gotKey = 0;
	char *lastKey = NULL;
	int valNum = 0;
	int i = 0;
	while (nextElem) {
	
		for (i = 0; i < Nesting; i++) { printf("  ");}
		  
		if  (nextElem[0] == '/') return NULL;

		int elemNameLength = strlen(nextElem);
	
		if (nextElem[elemNameLength - 1] != '/') {
			char *elemContents = getElemContents (ElementBlob, nextElem);
			int elemContentsLen =0;
			if (elemContents) {
 				elemContentsLen= strlen (elemContents);
			}
			else {
				return NULL;

			//	printf("NEXT ELEM: %s\n", nextElem); return NULL;
			}
			if (strcmp(nextElem,"key") == 0) {
				lastKey = elemContents;
				gotKey++;
			}
			else {
				 if (strcmp(nextElem,"array") == 0) {
					simplistifyElement (elemContents, 0, lastKey, Nesting);
				  }
				if (strcmp(nextElem,"dict") == 0){
					if (ArrayKey) {
					 printf("\t%s%s%s[%d]:\n", g_color? CYAN:"",ArrayKey , g_color? NORMAL:"",  valNum++);	
					}
					else {
					 printf("\t%s%s%s:\n", g_color? CYAN:"",lastKey , g_color? NORMAL:"");	
					}
					 simplistifyElement (elemContents, 0, NULL, Nesting+1);

				}


				if ((strcmp(nextElem,"integer") ==0)  || (strcmp(nextElem, "string") == 0)) {
					if (ArrayKey) {
					    printf("\t%s%s%s[%d]: %s\n",
						g_color? CYAN: "", ArrayKey,  g_color ? NORMAL:"",valNum, elemContents);
						valNum++;
					}
					else
					printf("\t%s%s%s: %s\n", 
						g_color? CYAN:"", lastKey, g_color?NORMAL:"",elemContents);


				}
					
				  

				} // array
			
			ElementBlob = elemContents + elemContentsLen +  1;
			}
		
			else { 

			// non nested values:
			
			printf("\t%s%s%s: ", g_color? CYAN: "", lastKey, g_color? NORMAL:"");

			if (strcmp(nextElem,"true/") == 0) printf(" true\n");
			if (strcmp(nextElem,"dict/") == 0) printf("\n");
			if (strcmp(nextElem,"false/") == 0) printf(" false\n");

		//	lastKey= "FOO\n";

			ElementBlob = strstr(ElementBlob, nextElem) + strlen(nextElem)+1;
		//	printf("ELEMEN BLOB NOW %s\n", ElementBlob);

	}
	

		nextElem = getNextTag (ElementBlob);

	}

	return ("");
} // simplistifyElement

char *simplistify( char *XMLBlob, int Size) {

	// Given an XML blob, convert to SimPLISTic format
	
	// Override Size anyway because of NULL termination
	Size = strlen(XMLBlob);
	char *XMLBlobWork = strdup(XMLBlob);
	
	char *plistContents = getElemContents (XMLBlobWork, "plist");
	if (!plistContents) { fprintf (stderr,"Not a plist\n"); 
			free(XMLBlobWork) ; return NULL;}


	char *dictContents = getElemContents (plistContents, "dict");
	if (!dictContents) { fprintf(stderr, "plist, but not a dict"); free(XMLBlobWork); return NULL;}
	//printf("DICT: %s\n", dictContents);

	simplistifyElement(dictContents, 0, NULL,0);



	free(XMLBlobWork);
	
	return ("");
}
