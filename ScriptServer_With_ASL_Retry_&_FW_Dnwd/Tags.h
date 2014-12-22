#ifndef _TAGS_H_
#define _TAGS_H_

struct Modpos
{
	int   layer ;
	int   offset ;
	int   msgtype ;
} ;
struct Tagwait
{
	int	msgType ;
	int	layer ;
	int	op ;
       int    typechk;
	int    bitchk;
	int    reversechk;
	int	offset ;
	int	size ;
	char *offset_char;
	char *size_char;
	char	*data ;
       char   *id;
       int   srcSize;
       Modpos	src;
} ;


struct TagModify
{
	int	size ;
       char *operation;
	char	*id ;
	Modpos	src ;
	Modpos	dst ;
};

#endif	/* _TAGS_H_ */
