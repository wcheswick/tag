/* A really simple argument processor from Plan 0 */
/* $Id: arg.h,v 1.1 2006/08/29 16:02:29 ches Exp $ */

char *argv0;
#define ARGBEGIN        for((argv0? 0: (argv0 = *argv)),argv++,argc--;\
                            argv[0] && argv[0][0]=='-' && argv[0][1];\
                            argc--, argv++) {\
                                char *_args, *_argt;\
                                char _argc;\
                                _argt = NULL;  /* To avoid compiler warning if ARGF is not used */\
                                _args = &argv[0][1];\
                                if(_args[0]=='-' && _args[1]==0){\
                                        argc--; argv++; break;\
                                }\
                                _argc = 0;\
                                while(*_args && (_argc = (*_args++)))\
                                switch(_argc)
#define ARGEND          ;};
#define ARGF()          (_argt=_args, _args="",\
                                (*_argt? _argt: argv[1]? (argc--, *++argv): 0))
#define ARGC()          _argc
