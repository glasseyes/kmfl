
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/stat.h>

#include "ConvertUTF.h"
#include "compiler.h"
#include "yacc.h"

#ifdef _WIN32
	#include <io.h>
	#include <conio.h>
	#include "getopt.h"
	#define strcasecmp	_stricmp
	#define rindex strrchr
	#define seek _lseek
	#define DIRDELIM	'\\'
	char *GetInputFile(void);
#else
	#include <getopt.h>
	#include <sys/types.h>
	#include <unistd.h>
	#define O_BINARY 	0
	#define DIRDELIM	'/'
#endif

#include <fcntl.h>

// Compiler options
int opt_debug=0;
int opt_force=0;
int opt_verbose=0;

// The keyboard being compiled, and version string
KEYBOARD keyboard, *kbp=&keyboard;
char Version[6]=BASE_VERSION FILE_VERSION;	// Concatenate keyboard version and file version

char *fname="(stdin)";
int warncount=0, warnlimit=10, errlimit=10, file_format=KF_UNICODE;

DEADKEY *last_deadkey=NULL;
STORE *last_store=NULL;

extern FILE *yyin;

int main(int argc, char *argv[]) 
{
	GROUP *gp;
	BYTE BOM[4]={0};
	long filesize=0;
	int opt,nopt=0;

	while((opt=getopt(argc,argv,"dfvy"))!=EOF) 
	{
		switch (opt) 
		{
		case 'd':
			opt_debug=1;
			break;
		case 'f':
			opt_force=1;
			break;
		case 'v':
			opt_verbose = 1;
			break;
		case 'y':
			yydebug = 1;
			break;
		}
		nopt++;
	}

	if(argc > nopt+1) fname=argv[argc-1];

#ifdef _WIN32
	else if(!(fname=GetInputFile())) exit(0);
#endif

	// Set warnings level
	if(opt_verbose) errlimit = warnlimit = 1000;

	// Open input file
	yyin =  fopen(fname,"r");
	if(!yyin)
	{
		char *ftmp;
		ftmp = (char *)checked_alloc(strlen(fname)+6,1);
		strcpy(ftmp,fname); strcat(ftmp,".kmn");
		yyin = fopen(ftmp,"r");
		free(ftmp);
	}
	if(!yyin) fail(1,"cannot open %s",fname);

	// Initialize defaults and parameters
	errcount=0;
	kbp->deadkeys = NULL;
	kbp->mode = KF_ANSI;		// Must be ANSI if not specified

	// Check for BOM at start of file 
	fread(BOM,3,1,yyin);
	if(BOM[0] == 0xEF && BOM[1] == 0xBB && BOM[2] == 0xBF)
	{
		file_format = KF_UNICODE;	// Set file format to Unicode if file is UTF-8 or UTF-16 
	}
	else
	{
		fseek(yyin,0,SEEK_SET);
		file_format = KF_ANSI;		// Set file format to ansi
	}

	if(BOM[0] == 0xFF && BOM[1] == 0xFE)	// Is it UTF-16?
	{
		yyin = UTF16toUTF8(yyin);	// Make a temporary UTF-8 copy of the file
		file_format = KF_UNICODE;	// And set file format to Unicode 
	}

	// Define the reserved-name stores as the first numbered stores
	initialize_special_stores();

	// Parse the input file with yacc 
	yyparse(); fflush(stdout);	
	fclose(yyin); 

	// Complete keyboard header and and check it for validity
	check_keyboard(kbp);

	// Exit on error (unless forced)
	if (errcount > 0 && !opt_force) 
	{
		fail(2,"%d error%s and %d warning%s",errcount,(errcount==1?"":"s"),
			warncount,(warncount==1?"":"s"));
	}

	// Sort the rules in each group
	for(gp=kbp->groups; gp; gp=gp->next) sort_rules(gp);

	// Save the output file
	if((filesize=save_keyboard(fname)) > 0)
	{
		if(errcount > 0 || warncount > 0)
		{
			if(errcount > 0 ) 
				fprintf(stderr,"  Warning: %d error%s ignored - compiled keyboard may fail!\n",
					errcount,(errcount==1?"":"s"));

			if(warncount > warnlimit)
				fprintf(stderr,"  Total warnings: %d\n",warncount);
		}

		if(Version[2] > '0') 
			fprintf(stderr,"Keyboard '%s' (Version %c.%c%c) compiled to %ld bytes\n",
				kbp->name,Version[0],Version[1],Version[2],filesize);
		else
			fprintf(stderr,"Keyboard '%s' (Version %c.%c) compiled to %ld bytes\n",
				kbp->name,Version[0],Version[1],filesize);
	}
	else
		fail(3,"unable to save output file!");

#ifdef _WIN32	
	if(opt_debug) getch();
#endif	

	exit(errcount);
}

// Complete keyboard header, and check for validity
void check_keyboard(KEYBOARD *kbp)
{
	UTF32 *p1; 
	UTF8 *p2;
	STORE *sp;
	ITEM *p;
	char *np;
	unsigned int n;

	// Has keyboard name been defined?
	sp = &kbp->stores[SS_NAME];
	if(sp->items == NULL || *sp->items == 0)
	{
		np = rindex(fname,'/');
		if(np == NULL) np = rindex(fname, '\\');
		if(np == NULL) np = rindex(fname, ':');
		if(np == NULL) np = fname-1;
		
		for(n=0,np++; n<NAMELEN && *np && *np!='.'; n++,np++)
			*(kbp->name+n) = *np;
		*(kbp->name+n) = 0;

		warn(0,"Keyboard name set by default to '%s'",kbp->name);
	}
	else
	{
		// There should be no MS bytes set, but make sure!
		for(n=0,p=sp->items; n<sp->len && *p!=0; n++,p++) *p &= 0xffffff;
		
		p1 = (UTF32 *)sp->items; p2 = (UTF8 *)kbp->name;
		ConvertUTF32toUTF8((const UTF32 **)&p1,(sp->items+sp->len),&p2,(UTF8 *)(kbp->name+NAMELEN+1),0);
		*p2 = 0;
	}
}

// Write the compiled keyboard to the output file
long save_keyboard(char *infile)
{
	XKEYBOARD xkbd={{0}};
	XSTORE xstore;
	XGROUP xgroup;
	XRULE xrule;
	STORE *sp, *sp1;
	GROUP *gp, *gp1;
	RULE *rp;
	DEADKEY *dp, *dp1;
	ITEM item;
	char *outfile, *stfile, *rtfile, *pdot; 
	unsigned long stt, rtt, i, j, n, out, index, size, offset;
	struct stat fstat;
		
	// Create file names from input file name
	n = strlen(infile)+6;

	if(!(outfile=(char *)malloc(n))) return(-1);
	strcpy(outfile,infile); pdot = rindex(outfile,'.');
	if(pdot) strcpy(pdot, ".kmfl"); else strcat(outfile,".kmfl");

	if(!(stfile=(char *)malloc(n))) return(-1);
	strcpy(stfile,infile); pdot = rindex(stfile,'.');
	if(pdot) strcpy(pdot,".stt"); else strcat(stfile,".stt");

	if(!(rtfile=(char *)malloc(n))) return(-1);
	strcpy(rtfile,infile); pdot = rindex(rtfile,'.');
	if(pdot) strcpy(pdot,".rtt"); else strcat(rtfile,".rtt");

	// Open output file, rule table and string table
	if((out=open(outfile,O_WRONLY|O_BINARY|O_CREAT|O_TRUNC,00666)) < 0) return(-2);
	if((stt=open(stfile,O_RDWR|O_BINARY|O_CREAT|O_TRUNC,00666)) < 0) return(-2);
	if((rtt=open(rtfile,O_RDWR|O_BINARY|O_CREAT|O_TRUNC,00666)) < 0) return(-2);

	// Fill the compiled keyboard header structure
	memcpy(&xkbd,kbp,sizeof(XKEYBOARD));	

	// Set tag and version
	memcpy(&xkbd.id,"KMFL",4);
	memcpy(&xkbd.version,Version,4);

	// Write the keyboard header
	write(out,&xkbd,sizeof(XKEYBOARD));

	// Save each store, saving its contents in the string table (no nulls)
	for(n=0,index=0,sp=kbp->stores; n<kbp->nstores; n++,sp=sp->next)
	{		
		if(sp->len > 0)
		{
			write(stt,sp->items,sp->len*ITEMSIZE);	
		}
		if(sp->items) free(sp->items);	// free string memory 
		xstore.len = sp->len;
		xstore.items = index;
		write(out,&xstore,sizeof(XSTORE));
		index += sp->len;
	}
		
	// Save each group, saving rules in the rule table and rule strings in the string table
	for(j=0,gp=kbp->groups,offset=0; j<kbp->ngroups; j++,gp=gp->next)
	{
		xgroup.flags = gp->flags;
		xgroup.nrules = gp->nrules;
		xgroup.rule1 = offset;
		xgroup.mrlen = gp->mrlen;
		xgroup.nmrlen = gp->nmrlen;
		
		if(gp->mrlen > 0)
		{	
			write(stt,gp->match,gp->mrlen*ITEMSIZE);
			free(gp->match);	// free string memory 
			xgroup.match = index;
			index += gp->mrlen;
		}
		else xgroup.match = UNDEFINED;
		
		if(gp->nmrlen > 0)
		{	
			write(stt,gp->nomatch,gp->nmrlen*ITEMSIZE);
			free(gp->nomatch); // free string memory 
			xgroup.nomatch = index;
			index += gp->nmrlen;
		}
		else xgroup.nomatch = UNDEFINED;

		write(out,&xgroup,sizeof(XGROUP));

		for(i=0,rp=gp->rules; i<gp->nrules; i++,rp=rp->next)
		{
			xrule.ilen = rp->ilen;
			xrule.olen = rp->olen;
			write(stt,rp->lhs,rp->ilen*ITEMSIZE);
			free(rp->lhs);		// free string memory 
			xrule.lhs = index;
			index += rp->ilen;
			write(stt,rp->rhs,rp->olen*ITEMSIZE);
			free(rp->rhs);		// free string memory 
			xrule.rhs = index;
			index += rp->olen;

			write(rtt,&xrule,sizeof(XRULE));
			offset++;
		}
	}

	// Append rule and string tables to output
	lseek(rtt,0,SEEK_SET); 
	for(n=0; n<offset; n++)
	{
		read(rtt,&xrule,sizeof(XRULE));
		write(out,&xrule,sizeof(XRULE));
	}

	lseek(stt,0,SEEK_SET);	
	for(n=0; n<index; n++)
	{
		read(stt,&item,ITEMSIZE);
		write(out,&item,ITEMSIZE);
	}

	close(rtt);	close(stt);	close(out);
	remove(rtfile); remove(stfile);

	stat(outfile,&fstat); 
	size = fstat.st_size;

	// Free deadkey memory
	for(dp=kbp->deadkeys; dp!=NULL; dp=dp1) 
	{
		dp1=dp->next; free(dp);
	}

	// Free stores memory
	for(sp=kbp->stores; sp!=NULL; sp=sp1)
	{
		sp1=sp->next; free(sp);
	}

	// Free groups and rules
	for(gp=kbp->groups; gp!=NULL; gp=gp1)
	{
		if(gp->rules) free(gp->rules);
		gp1=gp->next;
		free(gp);
	}

	return size;
}

// Routines for manipulating rule groups 

// Create a new group
GROUP *new_group(char *name)
{  
	GROUP *gp, *gp1;

	if((gp=find_group(name)) == NULL)
	{
		gp = (GROUP *)checked_alloc(sizeof(GROUP),1);
		strncpy(gp->name,name,NAMELEN);

		// and add it to the linked list of groups
		if(kbp->groups)
		{
			for(gp1=kbp->groups; gp1->next; gp1=gp1->next);
			gp1->next = gp;	
			kbp->ngroups++;
		}
		else
		{
			kbp->groups = gp;
			kbp->ngroups = 1;
		}
	}

	return gp;
}

// Find a group by name
GROUP *find_group(char *name)
{
	GROUP *gp;
	for(gp=kbp->groups; gp!=NULL; gp=gp->next)
	{
		if(strcasecmp(name,gp->name) == 0) return gp;
	}
	return NULL;
}

// Find the group number of a named group, and create a new group if necessary
int group_number(char *name)
{
	GROUP *gp;
	int n;
	
	for(n=0,gp=kbp->groups; gp!=NULL; gp=gp->next,n++)
	{
		if(strcasecmp(name,gp->name) == 0) return n;
	}
	
	// Create a new group and save the name	
	gp = new_group(name);

	return n;
}

// Count the groups in a linked list of groups
int count_groups(GROUP *gp)
{
	int n;

	for(n=0; gp!=NULL; gp=gp->next) n++;

	return n;
}

// Save the name of the starting group and set the mode flag
void set_start_group(char *name, int mode)
{
	kbp->group1 = group_number(name);
	kbp->mode = mode;
}

// Routines for manipulating rules

// Create a new rule for this group
RULE *new_rule(GROUP *gp, ITEM *lhs, ITEM *rhs, int line)
{
	RULE *rp;

	// Check first for match and nomatch rules
	switch(ITEM_TYPE(*lhs))
	{
	case ITEM_MATCH:
		if(count_items(lhs) == 1) 
		{
			gp->match = rhs;
			gp->mrlen = count_items(rhs);
			check_rhs(gp->match,gp->mrlen,gp,line);
		}
		else
		{
			error(line,"'match' must be the only item on the input side of a rule");
		}
		return NULL;
	case ITEM_NOMATCH:
		if(count_items(lhs) == 1) 
		{
			gp->nomatch = rhs;
			gp->nmrlen = count_items(rhs);
			check_rhs(gp->nomatch,gp->nmrlen,gp,line);
		}
		else
		{
			error(line,"'nomatch' must be the only item on the input side of a rule");
		}
		return NULL;
	}

	// Otherwise allocate memory for a rule and initialize the rule
	rp = (RULE *)checked_alloc(sizeof(RULE),1);
	rp->lhs = lhs; 
	rp->rhs = rhs;
	rp->ilen = count_items(rp->lhs);
	rp->olen = count_items(rp->rhs);
	rp->next = NULL;
	rp->line = line;

	// Check the rule for validity
	check_rule(rp,gp);
	return rp;	
}

// Add a rule to a rule list
RULE *add_rule(RULE *rp, RULE *prules)
{
	if(rp)		// must allow for null rules (blank lines)
	{
		rp->next = prules;
		return rp;
	}
	else 
	{
		return prules;
	}
}

// Count the rules in a linked list of rules
int count_rules(RULE *rp)
{
	int n;

	for(n=0; rp!=NULL; rp=rp->next) n++;

	return n;
}

// Check the left hand side of a rule, remove any '+' keystroke marker items,
//   and expand any outs() keywords found
ITEM *check_lhs(ITEM *lhs, unsigned int ilen, GROUP *gp, int line)
{
	STORE *sp;
	ITEM *p, *p1, *ip, *ip0;
	unsigned int i, j, k, newlen, goodplus=0, badplus=0;
	char *fmt="%s cannot be used on the left hand side of a rule";

	// Check for and remove any TOK_PLUS items, and warn if appropriate
	for(i=0,p=lhs; i<ilen; i++, p++)
	{
		if(ITEM_TYPE(*p) == ITEM_PLUS)
		{
			if((gp->flags & GF_USEKEYS) && (i == ilen-2)) 
			{
				goodplus = 1;
			}
			else
			{
				badplus = 1;
			}
			for(j=i, p1=p; j<ilen; j++) *p1 = *(p1+1);	//include trailing null
			ilen = ilen-1;
			i--, p--;
		}
	}
	*p = 0;		// ensure rule string terminated after compacting 

	if((gp->flags & GF_USEKEYS) && !goodplus && (Version[0] > '3'))		
	{
		warn(line,"'+' should be used before the keystroke"); 
	}

	if(badplus)
	{
		if(Version[0] > '5')
			error(line,"use '+' only immediately before keystroke");
		else 
			warn(line,"'+' used incorrectly (but ignored)");
	}

	// Make keystroke character explicitly a keysym (unless any, etc.)
	if((gp->flags & GF_USEKEYS) == GF_USEKEYS)
	{
		p = lhs+ilen-1;
		if(ITEM_TYPE(*p) == ITEM_CHAR) *p = MAKE_ITEM(ITEM_KEYSYM,*p);
	}

	if(ilen != count_items(lhs)) fail(1,"fatal compiler error");

	// Expand any outs() keywords on the left hand side of rules
	for(i=0, p=lhs; i<ilen; i++, p++)
	{
		if(ITEM_TYPE(*p) == ITEM_OUTS)
		{
			sp = find_store(store_name(*p & 0xffff));
			if(sp && (sp->len > 0))
			{
				newlen = ilen + sp->len - 1;
				ip = ip0 = (ITEM *)checked_alloc(newlen,sizeof(ITEM));
				for(j=0; j<i; j++) *ip++ = *(lhs+j);
				for(k=0; k<sp->len; k++) *ip++ = *(sp->items+k);
				for(++j; j<ilen; j++) *ip++ = *(lhs+j);
				*ip = 0; free(lhs); lhs = ip0; 
				ilen = newlen; 
				i--; p = lhs + i; 
			}
			else
			{
				error(line,"illegal use of 'outs()' keyword");
			}
		}
	}

	if(ilen != count_items(lhs)) fail(1,"fatal compiler error");

	// Check for illegal keywords
	for(i=0, p=lhs; i<ilen; i++, p++)
	{
		switch(ITEM_TYPE(*p))
		{
		case ITEM_RETURN:
			error(line,fmt,"'return'");
			break;
		case ITEM_BEEP:
			error(line,fmt,"'beep'");
			break;
		case ITEM_USE:
			error(line,fmt,"'use()'");
			break;
		case ITEM_CALL:
			error(line,fmt,"'call()'");
			break;
		}
	}
	return lhs;	// return the pointer to the (possibly new) string
}

// Check the right hand side of a rule for illegal keywords
void check_rhs(ITEM *rhs, unsigned int olen, GROUP *gp, int line)
{
	ITEM *p;
	unsigned int i;
	char *fmt="%s cannot be used on the right hand side of a rule";

	for(i=0, p=rhs; i<olen; i++, p++)
	{
		switch(ITEM_TYPE(*p))
		{
		case ITEM_MATCH:
			error(line,fmt,"'match'");
			break;
		case ITEM_NOMATCH:
			error(line,fmt,"'nomatch'");
			break;
		case ITEM_ANY:
			error(line,fmt,"'any()'");
			break;
		case ITEM_CALL:
			error(line,"call() keyword is not implemented");
			break;
		case ITEM_USE:	// test for and warn if group used recursively
			if((unsigned)group_number(gp->name) == (*p & 0xffff))
				warn(line,"rule group used recursively (use() refers to the containing group)");
			break;
		}
	}
}

// Check that the match and output strings only contain legal items
void check_rule(RULE *rp, GROUP *gp)
{
	if((rp->ilen != count_items(rp->lhs)) || (rp->olen != count_items(rp->rhs))) 
		fail(1,"fatal compiler error");

	// Check and adjust the lhs of the rule
	rp->lhs = check_lhs(rp->lhs,rp->ilen,gp,rp->line);
	rp->ilen = count_items(rp->lhs);	// must recalculate, as string may have been changed

	// Check the rhs of the rule
	check_rhs(rp->rhs,rp->olen,gp,rp->line);
}

// Rule comparison
int compare_rules(const void *arg1, const void *arg2)
{
	RULE *rp1, *rp2;
	
	rp1 = (RULE *)arg1;
	rp2 = (RULE *)arg2;

	// Check rule length first
	if(rp1->ilen > rp2->ilen) return -1;
	if(rp1->ilen < rp2->ilen) return 1;

	// Then compare line numbers - process according to source code sequence
	if(rp1->line < rp2->line) return -1;
	if(rp1->line > rp2->line) return 1;

	return 0;
}

// Sort the rules in each group
void sort_rules(GROUP *gp)
{
	RULE *rp, *rp1, *rptemp, *rpnext;
	UINT n;

	// Don't sort trivial groups!  
	if(gp->nrules < 2) return;

	// Create a single linear array of rules and free the existing linked list
	rptemp = (RULE *)checked_alloc(2*gp->nrules,sizeof(RULE));
	for(n=0,rp=gp->rules,rp1=rptemp; n<gp->nrules; n++,rp=rpnext,rp1++) 
	{
		*rp1 = *rp; rpnext = rp->next; free(rp);
	}
	
	// Sort the array
	qsort((void *)rptemp,(size_t)gp->nrules,sizeof(RULE),compare_rules);

	// Relink the list
	gp->rules = rptemp;
	for(n=0,rp=rptemp; n<gp->nrules; n++,rp++) rp->next = rp+1;
	rp->next = NULL;
}

// Routines for manipulating deadkeys

// Create a deadkey (I don't think this is ever needed - see deadkey_number)
DEADKEY *new_deadkey(char *name)
{  
	DEADKEY *dp;

	if((dp=find_deadkey(name)) == NULL)
	{
		dp = (DEADKEY *)checked_alloc(sizeof(DEADKEY),1);
		if(last_deadkey != NULL) last_deadkey->next = dp;
		last_deadkey = dp;
		if(kbp->deadkeys == NULL) kbp->deadkeys = dp;	// initialize pointer to list of deadkeys
	}
	strncpy(dp->name,name,NAMELEN);

	return dp;
}

// Find a deadkey by name
DEADKEY *find_deadkey(char *name)
{
	DEADKEY *p;
	for(p=kbp->deadkeys; p!=NULL; p=p->next)
	{
		if(strcasecmp(name,p->name) == 0) return p;
	}
	return NULL;
}

// Find a deadkey number
int deadkey_number(char *name)
{
	DEADKEY *dp, *dp0=NULL, *dp1;
	int n;
	for(n=0,dp=kbp->deadkeys; dp!=NULL; dp0=dp,dp=dp->next,n++)
	{
		if(strcasecmp(name,dp->name) == 0) return n;
	}
	// Create the deadkey if this is the first reference
	dp1 = (DEADKEY *)checked_alloc(sizeof(DEADKEY),1);
	if(dp0) dp0->next = dp1; else kbp->deadkeys = dp1;
	
	kbp->ndeadkeys++;	
	strncpy(dp1->name,name,NAMELEN);

	return n;
}


// Routines for manipulating stores

// Add a store to the list of stores and set its contents
STORE *new_store(char *name, ITEM *ip0, int line)
{  
	STORE *sp, *osp;
	ITEM *ip, *ipt, *osip;
	int len;

	if((sp=find_store(name)) != NULL)
	{
		if(sp->items != NULL) 
		{	free(sp->items);
			if(sp->len > 0) 
				warn(line,"overwriting previous contents of store %s",name);
			sp->items = NULL;
		}
	}
	else
	{
		sp = (STORE *)checked_alloc(sizeof(STORE), 1);
		if(kbp->stores == NULL) kbp->stores = sp;	// initialize pointer to list of stores
		strncpy(sp->name,name,NAMELEN);
		if(last_store != NULL) last_store->next = sp;
		last_store = sp;
		sp->next = NULL;
	}

	// Allocate memory for this store, expanding any outs()
	if(ip0) 
	{
		for(len=0,ip=ip0; *ip; ip++)
		{
			switch(ITEM_TYPE(*ip))
			{
			case ITEM_CHAR:
			case ITEM_KEYSYM:
			case ITEM_DEADKEY:
			case ITEM_BEEP:
				len++;
				break;
			case ITEM_OUTS:
				if((osp=find_store(store_name(*ip & 0xffff))))
					len += osp->len;
				break;
			}
		}

		sp->items = checked_alloc(len+1,4);
		sp->len = len;

		for(ip=ip0,ipt=sp->items; *ip; ip++)
		{
			switch(ITEM_TYPE(*ip))
			{
			case ITEM_CHAR:
			case ITEM_KEYSYM:
			case ITEM_DEADKEY:
			case ITEM_BEEP:
				*ipt++ = *ip;
				break;
			case ITEM_OUTS:
				if((osp=find_store(store_name(*ip & 0xffff))))
				{
					for(osip=osp->items;osip && *osip; osip++)
						*ipt++ = *osip;
				}
				break;
			default:
				error(line,"illegal item in store");
				break;
			}
		}
	
		// Process special store commands and copy to header as required
		if(*name == '&') process_special_store(name,sp,line);

		// Free the item list (probably OK to do this here, but must check...)
		free(ip0);
	}
	else	// allow for empty stores, just in case
	{
		sp->items = checked_alloc(1,4);
		sp->len = 0;
	}

	return sp;
}

STORE *new_store_from_string(char *name, char *string,int line)
{
	return new_store(name,items_from_string(string,line),line);
}

// Find a store by name
STORE *find_store(char *name)
{
	STORE *p;

	if(name != NULL) 
	{
		for(p=kbp->stores; p!=NULL; p=p->next)
		{
			if(strcasecmp(name,p->name) == 0) return p;
		}
	}
	return NULL;
}

// Find a store number
int store_number(char *name)
{
	STORE *p;
	int n;
	for(n=0,p=kbp->stores; p!=NULL; p=p->next,n++)
	{
		if(strcasecmp(name,p->name) == 0) return n;
	}
	return UNDEFINED;
}

// Find a store name
char *store_name(int number)
{
	STORE *sp;
	int n;
	for(n=0,sp=kbp->stores; sp!=NULL; sp=sp->next,n++)
	{
		if(n == number) return sp->name;
	}
	return NULL;
}

// Count the stores in a linked list of stores
int count_stores(STORE *sp)
{
	int n;

	for(n=0; sp!=NULL; sp=sp->next) n++;

	return n;
}

// Create a new itemlist 
ITEM *new_list(ITEM q)
{
	ITEM *p;
	p = (ITEM *)checked_alloc(2,4);
	*p = q; 
	return p;
}

// Add an item to an itemlist
ITEM *add_item_to_list(ITEM *s1, ITEM q)
{
	ITEM *p, *p0;
	int i, n;

	if((n=count_items(s1)) == 0) return new_list(q);
	
	p = p0 = (ITEM *)checked_alloc(n+1,4);
	*p++ = q;
	for(i=0; i<n; i++) *p++ = *(s1+i);
	*p = 0;
	free(s1);
	return p0;
}

// Count items in an item list
unsigned int count_items(ITEM *p)
{
	int n;

	for(n=0; *p; p++, n++);

	return n;
}

// Concatenate two item lists
ITEM *add_lists(ITEM *s1, ITEM *s2)
{
	ITEM *p, *p0;
	int i, n1, n2;
	n1 = count_items(s1); n2 = count_items(s2);
	p = p0 = (ITEM *)checked_alloc(n1+n2+1,4);
	for(i=0; i<n2; i++) *p++ = *(s2+i);
	for(i=0; i<n1; i++)	*p++ = *(s1+i);
	*p = 0;
	free(s1); free(s2);
	return p0;
}

// Create a new string
char *new_string(int q) 
{
	char *p;
	p = (char *)checked_alloc(2,1);
	*p = (char)q;
	return p;
}

// Add a character to the start of a string
char *add_char(char *sp, int q)
{
	char *p;
	int n;
	
	if(!sp) return new_string(q);

	n = strlen(sp);
	p = (char *)checked_alloc(n+2,1);
	*p = (char)q; strcpy(p+1, sp);
	return p;
}

// Convert a string to an item list, and allocate memory for the list
ITEM *items_from_string(char *sp, int line)
{
	ITEM *p, *p0;
	int i, n;
	ConversionResult result;
	
	n = strlen(sp);
	p = p0 = (ITEM *)checked_alloc(n+1,sizeof(ITEM));	
	// Convert UTF-8 strings to Unicode if and only if file format is UTF-8 
	// Note: The allocated memory will be longer than needed,but the string will be 
	// released again as soon as the string is copied to a store or rule
	if(file_format == KF_UNICODE)
	{
		result = ConvertUTF8toUTF32((const UTF8 **)&sp,(sp+n),(UTF32 **)&p,(UTF32 *)(p0+n+1),0);
		// Use ANSI conversion if UTF-8 conversion failed (for compatibility with old source files)
		if(result != 0)
		{
			warn(line,"file format is UTF-8, but non-UTF-8 characters found and converted as ANSI");
			for(i=0; i<n; i++) *p++ = *(sp+i);
		}
	}
	else
	{
		for(i=0; i<n; i++) *p++ = *(BYTE *)(sp+i);
	}
	*p = 0;

	return p0;
}

// Convert an item list to a simple string (mainly for debugging)
char *items_to_string(ITEM *p)
{
	static char temp[256];

	char *sp, *sp1;
	int n;
	n = count_items(p);
	
	for(sp=temp,sp1=temp+240,*sp=0; (*p) && (sp<sp1); p++) 
	{
		switch(ITEM_TYPE(*p))
		{
		case ITEM_CHAR:
			if(*p&0xffff80)
				sp += sprintf(sp,"[0x%lx]",(*p&0xffffff));
			else
				*sp++ = (char)*p; 
			break;
		case ITEM_KEYSYM:
			sp += sprintf(sp,"[key %lx,0x%lx]",(*p&0xff0000)>>16,(*p&0xff));
			break;
		case ITEM_ANY:
			sp += sprintf(sp,"[any %u]",(unsigned)(*p&0xffff));
			break;
		case ITEM_INDEX:
			sp += sprintf(sp,"[index %ld,%lu]",(*p&0xff0000)>>16,(*p&0xffff));
			break;
		case ITEM_OUTS:
			sp += sprintf(sp,"[outs %lu]",(*p&0xffff));
			break;
		case ITEM_DEADKEY:
			sp += sprintf(sp,"[dk %lu]",(*p&0xffff));
			break;
		case ITEM_CONTEXT:
			if(*p & 0xff)
				sp += sprintf(sp,"[context %lu]",(*p&0xff));
			else
				sp += sprintf(sp,"[context]");
			break;
		case ITEM_NUL:
			sp += sprintf(sp,"[nul]");
			break;			
		case ITEM_RETURN:
			sp += sprintf(sp,"[return]");
			break;
		case ITEM_BEEP:
			sp += sprintf(sp,"[beep]");
			break;
		case ITEM_USE:
			sp += sprintf(sp,"[use %lu]",(*p&0xffff));
			break;
		case ITEM_MATCH:
			sp += sprintf(sp,"[match]");
			break;
		case ITEM_NOMATCH:
			sp += sprintf(sp,"[nomatch]");
			break;
		case ITEM_PLUS:
			sp += sprintf(sp,"[+]");
			break;
		case ITEM_CALL:
			sp += sprintf(sp,"[use %lu]",(*p&0xffff));
			break;
		}
	}
	*sp = 0;

	return temp;
}

// List of special stores with reserved names
char *special_stores[] = {
	"&name","&version","&hotkey","&language","&layout","&copyright","&message",
	"&bitmap","&mnemoniclayout","&ethnologuecode",
	"&capsalwaysoff","&capsononly","&shiftfreescaps","&author"};

// Initialize special stores (create dummy entries)
void initialize_special_stores(void)
{
	int i;
	for(i=0; i<sizeof(special_stores)/sizeof(char *); i++)
	{
		new_store(special_stores[i],NULL,0);
	}
}

// Copy special store contents to keyboard header as required
void process_special_store(char *name, STORE *sp, int line)
{
	int n, kbver;
	
	// Identify store by name
	for(n=0;special_stores[n];n++)
	{
		if(strcasecmp(name,special_stores[n]) == 0 ) break;
	}

	// Specific action required by header
	switch(n)
	{
	case SS_NAME:
		break;
	case SS_HOTKEY:
		kbp->hotkey = sp->items[0];
		break;
	case SS_MNEMONIC:
		kbp->layout = (sp->items[0] != '0');
		break;
	case SS_CAPSOFF:
		kbp->capsoff = 1;
		break;
	case SS_CAPSON:
		kbp->capson = 1;
		break;
	case SS_CAPSFREE:
		kbp->capsfree = 1;
		break;
	case SS_VERSION:
		kbver = (int)(100.0*atof(items_to_string(sp->items))+0.5);
		sprintf(Version,"%3.3d%1.1s",kbver,FILE_VERSION);
		break;
	case SS_BITMAP:
		check_bitmap_file(sp,line);
		break;
	case SS_COPYRIGHT:
	case SS_MESSAGE:
	case SS_LAYOUT:
	case SS_AUTHOR:
	case SS_LANGUAGE:
	case SS_ETHNOLOGUE:
		break;
	default:
		warn(line-1,"unrecognized special store '&%s'",name);		
		return;
	}		
}

// Error, warning and debug messages

void fail(int errcode, char *s, ...)
{
	char t[512];
	va_list v1;
	
	va_start(v1,s); 
	vsnprintf(t,511,s,v1);
	va_end(v1);
	
	fprintf(stderr, "*** Compilation failed: %s ***\n", t);
	fflush(stderr);

#ifdef _WIN32	
	if(opt_debug) getch();
#endif	

	exit(-errcode);
}

void error(int lineno,char *s, ...)
{
	char t[512];
	va_list v1;

	if(++errcount > errlimit) return;

	va_start(v1,s); 
	vsnprintf(t,511,s,v1);
	va_end(v1);

	if(lineno)
		fprintf(stderr, "  Error: %s (line %d)\n", t, lineno);
	else
		fprintf(stderr, "  Error: %s\n", t);

	if(errcount == errlimit) 
		fprintf(stderr, "    -------(remaining errors unreported)-------\n");
}

void warn(int lineno,char *s, ...)
{
	char t[512];
	va_list v1;
	
	if(++warncount > warnlimit) return;

	va_start(v1,s); 
	vsnprintf(t,511,s,v1);
	va_end(v1);
	
	if(lineno)
		fprintf(stderr, "  Warning: %s (line %d)\n", t, lineno);
	else
		fprintf(stderr, "  Warning: %s\n", t);

	if(warncount == warnlimit) 
		fprintf(stderr, "    -------(remaining warnings unreported)-------\n");
}

void debug(int lineno,char *s, ...)
{
	char t[512];
	va_list v1;
	
	if(!opt_debug) return;

	va_start(v1,s); 
	vsnprintf(t,511,s,v1);
	va_end(v1);

	if(lineno)
		fprintf(stderr, "Debug: %s (line %d)\n", t, lineno);
	else
		fprintf(stderr, "Debug: %s\n", t);
}

// Checked memory allocation, always allocating one more element than requested
void *checked_alloc(size_t n, size_t sz)
{
	void *p;

	if(!(p=calloc(n+1,sz))) fail(4,"out of memory!");
	
	return p;
}

// Convert the first (Unicode) character of a UTF-8 (or ANSI) text string to a keysym
ITEM string_to_keysym(char *sp, int line)
{
	ITEM *ip, keysym=0;
	unsigned int iplen;
	
	ip = items_from_string(sp,line);
	if(ip) 
	{
		iplen = count_items(ip);
		if(iplen > 0) keysym = *ip;
		if(iplen > 1) warn(line,"only the first character can be used in a keysym");
		free(ip);
	}
	if(keysym == 0)	error(line,"illegal keysym/virtual character key");

	return keysym;
}

// Combine shift state and key code as required for Linux
ITEM make_keysym(ITEM state, ITEM q)
{
	// Mask character and state

	q &= 0xffff; state &= 0xff;

	if(((q & 0xff00) == 0) && isalpha(q)) 
	{
		if(((state & KS_SHIFT) == 0) && ((state & KS_CAPS) == 0)) q += 0x20;
		else if(((state & KS_SHIFT) != 0) && ((state & KS_CAPS) != 0)) q += 0x20;
	}

	return q | (state<<16) | (ITEM_KEYSYM<<24);
}

// Find the first file that matches the given path, ignoring case
// Substitute the actual name, and return (or return NULL)
#ifdef _WIN32
// Windows version
char *find_first_match(char *path)
{
	char *p=NULL;
	struct stat fstat;

	if(stat(path,&fstat) == 0)
	{
		p = rindex(path,DIRDELIM);
		return (p==NULL ? path : p+1);
	}
	else 
	{
		return NULL;
	}
}
#else
// Linux version: needs to be researched later
char *find_first_match(char *path)
{
	char *p=NULL;
	struct stat fstat;

	if(stat(path,&fstat) == 0)
	{
		p = rindex(path,DIRDELIM);
		return (p==NULL ? path : p+1);
	}
	else 
	{
		return NULL;
	}
}
#endif

// Check that the bitmap file exists as specified, or else that a file with an accepted variation
// of the name exists (different case, .bmp or .png suffixes)
int check_bitmap_file(STORE *sp, int line)
{
	char *p,*bmp_path=NULL,tname[64];
	struct stat fstat;
	UINT i;
	UTF32 *p1,*titems=NULL;
	UTF8 *p2;

	p1 = sp->items; p2 = tname;
	ConvertUTF32toUTF8((const UTF32 **)&p1,(sp->items+sp->len),&p2,(UTF8 *)(tname+63),0);
	*p2 = 0;

	if((p=rindex(fname,DIRDELIM)) != NULL) 
	{
		bmp_path = (char *)checked_alloc((p-fname+1)+strlen(tname)+6,1);
		strncpy(bmp_path,fname,p-fname+1); 
		strcpy(bmp_path+(p-fname+1),tname);
	}
	else
	{
		bmp_path = (char *)checked_alloc(strlen(tname)+6,1);
		strcpy(bmp_path,tname);
	}

	// First test if the file exists exactly as in BITMAP statement
	if(stat(bmp_path,&fstat) == 0)
	{
		free(bmp_path);	// file exists
		return 0;
	}

	// Search for a case-variation (irrelevant for Windows)
	p = find_first_match(bmp_path);

	// If no extension specified, search next for files with extensions .bmp and .png
	if((p == NULL) && (strchr(tname,'.') == NULL))
	{
		// Add .bmp 
		strcat(bmp_path,".bmp");
		p = find_first_match(bmp_path);
		if(p == NULL)
		{
			// Add .png
			strcpy(bmp_path+strlen(bmp_path)-4,".png");
			p = find_first_match(bmp_path);
		}
	}

	// Issue appropriate warnings and replace file name in store
	if(p == NULL)
	{
		warn(line,"The bitmap file '%s' was not found. Create a suitable bitmap of that name "
			"and copy it with the compiled keyboard",tname);
	}
	else
	{
		warn(line,"A bitmap named '%s' was found and will be referred to in the compiled keyboard "
			"instead of '%s'",p,tname);
		
		if(sp->len)free(sp->items);
		// First allocate more than will be needed
		titems = (UTF32 *)checked_alloc(strlen(p)+1,sizeof(UTF32));
		p2 = p; p1 = titems;					
		ConvertUTF8toUTF32((const UTF8 **)&p2,(p+strlen(p)),(UTF32 **)&p1,p1+strlen(p),0);
		sp->len = (UINT)(p1 - titems);	// Then reallocate to exact length
		sp->items = (ITEM *)checked_alloc(sp->len,sizeof(ITEM));
		for(i=0; i<sp->len; i++) *(sp->items+i) = *(titems+i);
		free(titems);
	}
	
	if(bmp_path) free(bmp_path);
	return 1;
}

// Create a temporary file with a UTF-8 copy of a UTF-16 input file
FILE *UTF16toUTF8(FILE *fp)
{
	FILE *fp8;
	unsigned short t16[512];
	unsigned char t8[2048];
	const UTF16 *p16,*p16a;
	UTF8 *p8;

	if((fp8=tmpfile()) == NULL) return NULL;

#ifdef _WIN32
	 _setmode(_fileno(fp),_O_BINARY);
#endif
	
	fseek(fp,2,SEEK_SET);
	while(fread(t16,2,1,fp))
	{
		p16 = t16; p8 = t8; p16a = p16+1;
		if(ConvertUTF16toUTF8(&p16,p16a,&p8,t8+2047,0) == 0)
			fwrite(t8,1,(size_t)(p8-t8),fp8);
		else 
			fail(1,"unable to convert Unicode file, illegal or malformed UTF16 sequence");		
	}

	fseek(fp8,0,SEEK_SET);
	
	return fp8;
}


#ifdef _WIN32

#include <windows.h>

#define OFN_DONTADDTORECENT	0x2000000 

char *GetInputFile(void)
{
	OPENFILENAME OFN={0};
	static char FileName[MAX_PATH], KeyboardName[32];

	OFN.lStructSize = sizeof(OPENFILENAME);
	OFN.hwndOwner = NULL;
	OFN.lpstrFilter = "Uncompiled Keyboard Files (*.kmn)\0*.kmn\0";
	OFN.lpstrCustomFilter = NULL;
	OFN.nFilterIndex = 1;
	OFN.lpstrFile = FileName;
	OFN.nMaxFile = MAX_PATH-1;
	OFN.lpstrFileTitle = KeyboardName;
	OFN.nMaxFileTitle = 32;
	OFN.lpstrInitialDir = NULL;

	OFN.lpstrTitle = "Choose keyboard to compile";
	OFN.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOCHANGEDIR 
		| OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;
	OFN.lpstrDefExt = "kmn";
	OFN.lpfnHook = NULL;
	return(GetOpenFileName(&OFN) != 0 ? FileName : NULL);
}
#endif
