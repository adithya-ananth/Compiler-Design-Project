%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 //#include "tokens.h" //remove this when integrating with parser
#include "y.tab.h"
 /* ---------- yylval (standalone) ---------- */
 /*
typedef union {
    int   intval;
    char *str;
} YYSTYPE;


 YYSTYPE yylval;
 */

 /* ---------- Position tracking ---------- */
int line_num = 1;
int col_num  = 1;
int token_col;

 /* ---------- Token name helper ---------- */
const char *token_name(int token) {
    switch (token) {
        case T_INT:    return "T_INT";
        case T_VOID:   return "T_VOID";
        case T_IF:     return "T_IF";
        case T_ELSE:   return "T_ELSE";
        case T_WHILE:  return "T_WHILE";
        case T_FOR:    return "T_FOR";
        case T_RETURN: return "T_RETURN";

        case T_IDENT:  return "T_IDENT";
        case T_NUMBER: return "T_NUMBER";

        case T_EQ:     return "T_EQ";
        case T_NEQ:    return "T_NEQ";
        case T_LE:     return "T_LE";
        case T_GE:     return "T_GE";
        case T_AND:    return "T_AND";
        case T_OR:     return "T_OR";

	//  added on 30/1/2026	
	case T_CHAR:    return "T_CHAR";
	//case T_STRING:  return "T_STRING";
	case T_CHAR_LIT: return "T_CHAR_LIT";
	case T_STRING_LIT:    return "T_STRING_LIT";
        
	default:
            if (token < 256) {
                static char buf[8];
                snprintf(buf, sizeof(buf), "'%c'", token);
                return buf;
            }
            return "UNKNOWN";
    }
}

 /* Save starting column of token */
#define SAVE_POS() token_col = col_num
#define ADVANCE()  col_num += yyleng
%}

 /* ---------- Definitions ---------- */
DIGIT      [0-9]
LETTER     [a-zA-Z_]
IDENT      {LETTER}({LETTER}|{DIGIT})*
NUMBER     {DIGIT}+
 // the char and string added on 30/1/2026
CHAR_LITERAL   \'([^\\\n]|\\[nt'\"\\])\'
STRING_LITERAL \"([^\\\n]|\\[nt'\"\\])*\"


%%

 /* ---------- Keywords ---------- */
"int"       { SAVE_POS(); ADVANCE(); return T_INT; }
"void"      { SAVE_POS(); ADVANCE(); return T_VOID; }
"char"      { SAVE_POS(); ADVANCE(); return T_CHAR; }  // added 30/1/2026
"if"        { SAVE_POS(); ADVANCE(); return T_IF; }
"else"      { SAVE_POS(); ADVANCE(); return T_ELSE; }
"while"     { SAVE_POS(); ADVANCE(); return T_WHILE; }
"for"       { SAVE_POS(); ADVANCE(); return T_FOR; }
"return"    { SAVE_POS(); ADVANCE(); return T_RETURN; }

 /* ---------- Multi-char operators ---------- */
"=="        { SAVE_POS(); ADVANCE(); return T_EQ; }
"!="        { SAVE_POS(); ADVANCE(); return T_NEQ; }
"<="        { SAVE_POS(); ADVANCE(); return T_LE; }
">="        { SAVE_POS(); ADVANCE(); return T_GE; }
"&&"        { SAVE_POS(); ADVANCE(); return T_AND; }
"||"        { SAVE_POS(); ADVANCE(); return T_OR; }

 /* ---------- Single-char operators ---------- */
"="         { SAVE_POS(); ADVANCE(); return '='; }
"<"         { SAVE_POS(); ADVANCE(); return '<'; }
">"         { SAVE_POS(); ADVANCE(); return '>'; }
"+"         { SAVE_POS(); ADVANCE(); return '+'; }
"-"         { SAVE_POS(); ADVANCE(); return '-'; }
"*"         { SAVE_POS(); ADVANCE(); return '*'; }
"/"         { SAVE_POS(); ADVANCE(); return '/'; }
"%"         { SAVE_POS(); ADVANCE(); return '%'; }
"!"         { SAVE_POS(); ADVANCE(); return '!'; }

 /* ---------- Delimiters ---------- */
";"         { SAVE_POS(); ADVANCE(); return ';'; }
","         { SAVE_POS(); ADVANCE(); return ','; }
"("         { SAVE_POS(); ADVANCE(); return '('; }
")"         { SAVE_POS(); ADVANCE(); return ')'; }
"{"         { SAVE_POS(); ADVANCE(); return '{'; }
"}"         { SAVE_POS(); ADVANCE(); return '}'; }

 /* ---------- Literals ---------- */
{NUMBER} {
    SAVE_POS();
    yylval.intval = atoi(yytext);
    ADVANCE();
    return T_NUMBER;
}

 /* ---------- Identifiers ---------- */
{IDENT} {
    SAVE_POS();
    yylval.str = strdup(yytext);
    ADVANCE();
    return T_IDENT;
}

 /* char and string detection added on 30/1/2026 */

{CHAR_LITERAL} {
    SAVE_POS();
    char c;

    if (yytext[1] == '\\') {
        switch (yytext[2]) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case '\\': c = '\\'; break;
            case '\'': c = '\''; break;
            default:
                fprintf(stderr,
                    "Invalid escape sequence at %d:%d\n",
                    line_num, token_col);
                exit(1);
        }
    } else {
        c = yytext[1];
    }

    yylval.intval = (int)c;
    ADVANCE();
    return T_CHAR_LIT;
}


{STRING_LITERAL} {
    SAVE_POS();

    /* Remove quotes */
    int len = yyleng - 2;
    char *s = malloc(len + 1);
    strncpy(s, yytext + 1, len);
    s[len] = '\0';

    yylval.str = s;
    ADVANCE();
    return T_STRING_LIT;
}



 /* ---------- Comments ---------- */
"//".*      { /* ignore */ }
"/*"([^*]|\*+[^*/])*\*+"/"  { /* ignore */ }

 /* ---------- Whitespace ---------- */
[ \t\r]+    { ADVANCE(); }
\n          { line_num++; col_num = 1; }

 /* ---------- Errors ---------- */
. {
    fprintf(stderr,
        "Lexical error: '%s' at line %d, column %d\n",
        yytext, line_num, col_num);
    exit(1);
}

%%

 
 //---------- Standalone test driver ---------- 
 /*
int main(void) {
    int tok;
    while ((tok = yylex())) {
        printf("%-10s  %-8s  at %d:%d\n",
               token_name(tok),
               yytext,
               line_num,
               token_col);
    }
    return 0;
}
 */

int yywrap(void) {
    return 1;
}
