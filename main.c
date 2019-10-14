/*
 * SW compiler program implemented in C
 *
 * The program has been tested on CodeBlocks  16.01 rev 10702
 *
 * Student: Tao Wei 陶玮
 *
 * 使用方法：
 * 运行后输入SW源程序文件名
 * 回答是否输出虚拟机代码
 * 回答是否输出符号表
 * fcode.txt输出虚拟机代码
 * foutput.txt输出源文件、出错示意（如有错）、各行对应的生成代码首地址（如无错）
 * fresult.txt输出运行结果
 * ftable.txt输出符号表
 */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#define bool int            /* 因为C语言没有bool类型 */
#define true 1              /* 所以做这3行处理 */
#define false 0

#define norw 29             /* 保留字个数 10+19 */
#define txmax 100           /* 符号表容量 */
#define nmax 14             /* 数字的最大位数 */
#define al 10               /* 标识符的最大长度 10?*/

#define maxerr 30           /* 允许的最多错误数 */
#define amax 2048           /* 地址上界*/
#define cxmax 200           /* 最多的虚拟机代码数 */
#define stacksize 500       /* 运行时数据栈元素最多为500个 */
#define levmax 1            /* 最大允许过程嵌套声明层数*/


/* 符号 */
enum symbol {
    nul,                    /* 未定义 */
    ident,                  /* 标识符 */
    number,                 /*  数字  */
    plus,   minus,          /*  +  -  */
    times,  slash,          /*  *  /  */
    eql,                    /*   ==   */
    neq,                    /*   !=   */
    lss,    leq,            /*  <  <= */
    gtr,    geq,            /*  >  >= */
    lparen, rparen,         /*  (  )  */
    lbrace, rbrace,         /*  {  }  */
    semicolon,              /*   ;    */
    threepoints,            /*  ...   */
    becomes,                /*   =    */
    plusone,    minusone,   /* ++, -- */
    mod,                    /*   %    */
    colon,                  /*   :    */
    comma,                  /*   ,    */
    lbrack, rbrack,         /*  [  ]  */
    ifsym,     elsesym,   whilesym,   forsym,    insym,
    readsym,   printsym,  callsym,    varsym,    funcsym,
    boolsym,   constsym,  floatsym,   oddsym,    switchsym,
    casesym,   breaksym,  repeatsym,  notsym,    defaultsym,
    andsym,    orsym,     exitsym,    tointsym,  tofloatsym,
    truesym,   falsesym,  returnsym,  eofsym,    continuesym,
};
#define symnum 57           // 27+29+1


/* 符号表中的类型 */
enum object {
    variable,               /*  变量  */
    function,               /*  函数  */
    boolean,                /*  布尔  */
    constant,               /*  常数  */
    floatnum,               /*  实数  */
};


/* 虚拟机代码指令 */
enum fct {
    lit,                    /* lit 0, a 将数a置入栈顶 */
    opr,                    /* opr 0, a 一组算术关系运算符 返回调用程序 */
    lod,                    /* lod 1, a 将1,a形成的栈地址变量值置入栈顶 */
    sto,                    /* sto 1, a 将栈顶值存到由1,a形成的栈地址变量 */
    cal,                    /* cal 1, a 调用子程序 */
    ini,                    /* int 0, a 预留a个存储位置 (int写作ini避免与C语言中的INT混淆) */
    jmp,                    /* jmp 0, a 无条件转移 */
    jpc,                    /* jpc 0, a 条件转移 (当s(t)==0时) */
};
#define fctnum 8


/* 虚拟机代码结构 */
struct instruction
{
    enum fct f;             /* 虚拟机代码指令 */
    int l;                  /* 引用层与声明层的层次差 */
    float a;                /* 根据f的不同而不同 */
};

struct parameter
{
    char name[al];
    enum object type;
};

struct parameter p[10];     /* 函数内可传递最多10个参数 */

/* 全局变量 (中间过程使用) */
bool listswitch ;           /* 显示虚拟机代码与否 */
bool tableswitch ;          /* 显示符号表与否 */
struct instruction code[cxmax]; /* 存放虚拟机代码的数组 */
char mnemonic[fctnum][5];   /* 虚拟机代码指令名称 */
bool declbegsys[symnum];    /* 表示声明开始的符号集合 */
bool statbegsys[symnum];    /* 表示语句开始的符号集合 */
bool facbegsys[symnum];     /* 表示因子开始的符号集合 */

char ch;                    /* 存放当前读取的字符，getch 使用 */
enum symbol sym;            /* 当前的符号 */
char id[al+1];              /* 当前ident，多出的一个字节用于存放0 */
float num;                  /* 当前number */
int cc, ll;                 /* getch使用的计数器，cc表示当前字符(ch)的位置 */
int cx;                     /* 虚拟机代码指针, 取值范围[0, cxmax-1]*/
char line[81];              /* 读取行缓冲区 */
char a[al+1];               /* 临时字符串，多出的一个字节用于存放0 */
char word[norw][al];        /* 保留字 */
enum symbol wsym[norw];     /* 保留字对应的符号值 */
enum symbol ssym[256];      /* 单字符的符号值 */

bool isend = false;         /* 文件是否读完 */
bool isjmp[10];             /* statement_list语句中可否有continue和break语句 */
bool isfor = false;         /* 当前执行语句是否在for-in语句中(循环体内除外)内 */
bool isfloat = false;       /* 标识符所存数据类型是否是小数 */
int circlenum = 0;          /* 当前位于几个循环体内 */
int continue_cx[10][100];   /* continue语句位置 */
int continue_n[10];         /* continue语句个数 */
int break_cx[10][100];      /* break语句位置 */
int break_n[10];            /* break语句个数 */
int exit_cx[100];           /* exit语句位置(最多100处) */
int exit_n = 0;             /* exit语句个数 */
int parameter_n = 0;        /* 函数()内参数个数 */
bool is_return = false;     /* 函数是否有返回值 */

char current[10];           /* 当前所在函数名 */
int varmax = 0;             /* 全局变量的最大位置 */

/* 符号表结构 */
struct tablestruct
{
    char name[al];          /* 名字 */
    enum object kind;       /* 类型 */
    int val;                /* 数值，仅const使用 */
    int level;              /* 所处层, 仅const不使用 */
    int adr;                /* (开始)地址, 仅const不使用 */
    int size;               /* 需要分配的数据区空间, 仅function, array使用 */
    int para_n;             /* 函数参数个数, 仅function使用 */
    bool isreturn;          /* 是否有返回值, 仅function使用 */
};
struct tablestruct table[txmax]; /* 符号表 */


/* 全局变量 (输入输出相关) */
FILE* fin;                  /* 输入源文件 */
FILE* foutput;              /* 输出文件及出错示意（如有错）、各行对应的生成代码首地址（如无错） */
FILE* ftable;               /* 输出符号表 */
FILE* fcode;                /* 输出虚拟机代码 */
FILE* fresult;              /* 输出执行结果 */
char fname[al];             /* 输入字符串 (文件名, 是否选择) */
int err;                    /* 错误计数器 */


/* 函数申明 */
void error(int n);
void getsym();
void getch();
void init();

void program(int tx, bool* fsys, int lev);
void var_declaration_list(int* ptx, int* pdx, int lev);
void statement_list(int* ptx, bool* fsys, int lev);
void expression(int* ptx, bool* fsys, int lev);
void condition(int* ptx, bool* fsys, int lev);
void term(int* ptx, bool* fsys, int lev);
void factor(int* ptx, bool* fsys, int lev);

int position(char* idt, int tx);
void enter(enum object k, int* ptx, int* pdx, int lev);

void gen(enum fct x, int y, float z);
void test(bool* s1, bool* s2, int n);
int inset(int e, bool* s);
int addset(bool* sr, bool* s1, bool* s2, int n);
int subset(bool* sr, bool* s1, bool* s2, int n);
int mulset(bool* sr, bool* s1, bool* s2, int n);
void interpret();
void listcode(int cx0);
void listall();
int base(int l, float* s, int b);


/* 主程序 */
int main()
{
    bool nxtlev[symnum];                    /* 后继符号集合 */

    printf("Input SW file?   ");
    scanf("%s", fname);                     /* 输入文件名 */

    if ((fin = fopen(fname, "r")) == NULL)  /* 文件打开失败 */
    {
        printf("Can't open the input file:(\n");
        exit(1);
    }

    ch = fgetc(fin);
    if (ch == EOF)                          /*  文件为空  */
    {
        printf("The input file is empty:(\n");
        fclose(fin);
        exit(1);
    }
    rewind(fin);                            /* 将文件内部的位置指针重新指向一个流（数据流/文件）的开头 */

    if ((foutput = fopen("foutput.txt", "w")) == NULL)
    {
        printf("Can't open the output file:(\n");
        exit(1);
    }

    if ((ftable = fopen("ftable.txt", "w")) == NULL)
    {
        printf("Can't open ftable.txt file!:(\n");
        exit(1);
    }

    printf("List object codes?(Y/N)");      /* 是否输出虚拟机代码 */
    scanf("%s", fname);
    listswitch = (fname[0]=='y' || fname[0]=='Y');

    printf("List symbol table?(Y/N)");      /* 是否输出符号表 */
    scanf("%s", fname);
    tableswitch = (fname[0]=='y' || fname[0]=='Y');

    init();                                 /* 初始化 */

    addset(nxtlev, declbegsys, statbegsys, symnum);
    program(0, nxtlev, 0);                  /* 处理分程序 */

    if (sym != eofsym)
    {
        printf("We cannot read the file over:(\n");
        exit(1);
    }

    if (err == 0)
    {
        printf("\n===Parsing success!===\n");
        fprintf(foutput,"\n===Parsing success!===\n");

        if ((fcode = fopen("fcode.txt", "w")) == NULL)
        {
            printf("Can't open fcode.txt file!\n");
            exit(1);
        }

        if ((fresult = fopen("fresult.txt", "w")) == NULL)
        {
            printf("Can't open fresult.txt file!\n");
            exit(1);
        }

        listall();                          /* 输出所有代码 */
        fclose(fcode);

        interpret();                        /* 调用解释执行程序 */
        fclose(fresult);
      }
    else
    {
        printf("\n%d errors in SW program!\n",err);
        fprintf(foutput,"\n%d errors in SW program!\n",err);
    }

    fclose(ftable);
    fclose(foutput);
    fclose(fin);

    return 0;
}


/* 初始化 */
void init()
{
    int i;

    /* 设置单字符符号 */
    for (i = 0; i < 256; i++)
    {
        ssym[i] = nul;
    }
    ssym['+'] = plus;
    ssym['-'] = minus;
    ssym['*'] = times;
    ssym['/'] = slash;
    ssym['('] = lparen;
    ssym[')'] = rparen;
    ssym['{'] = lbrace;
    ssym['}'] = rbrace;
    ssym['='] = becomes;
    ssym[';'] = semicolon;
    ssym['%'] = mod;
    ssym[':'] = colon;
    ssym[','] = comma;
    ssym['['] = lbrack;
    ssym[']'] = rbrack;

    /* 设置保留字名字,按照字母顺序，便于二分查找 */
    strcpy(&(word[0][0]), "and");
    strcpy(&(word[1][0]), "bool");
    strcpy(&(word[2][0]), "break");
    strcpy(&(word[3][0]), "call");
    strcpy(&(word[4][0]), "case");
    strcpy(&(word[5][0]), "const");
    strcpy(&(word[6][0]), "continue");
    strcpy(&(word[7][0]), "default");
    strcpy(&(word[8][0]), "else");
    strcpy(&(word[9][0]), "exit");
    strcpy(&(word[10][0]), "false");
    strcpy(&(word[11][0]), "float");
    strcpy(&(word[12][0]), "for");
    strcpy(&(word[13][0]), "func");
    strcpy(&(word[14][0]), "if");
    strcpy(&(word[15][0]), "in");
    strcpy(&(word[16][0]), "not");
    strcpy(&(word[17][0]), "odd");
    strcpy(&(word[18][0]), "or");
    strcpy(&(word[19][0]), "print");
    strcpy(&(word[20][0]), "read");
    strcpy(&(word[21][0]), "repeat");
    strcpy(&(word[22][0]), "return");
    strcpy(&(word[23][0]), "switch");
    strcpy(&(word[24][0]), "tofloat");
    strcpy(&(word[25][0]), "toint");
    strcpy(&(word[26][0]), "true");
    strcpy(&(word[27][0]), "var");
    strcpy(&(word[28][0]), "while");

    /* 设置保留字符号 */
    wsym[0] = andsym;
    wsym[1] = boolsym;
    wsym[2] = breaksym;
    wsym[3] = callsym;
    wsym[4] = casesym;
    wsym[5] = constsym;
    wsym[6] = continuesym;
    wsym[7] = defaultsym;
    wsym[8] = elsesym;
    wsym[9] = exitsym;
    wsym[10] = falsesym;
    wsym[11] = floatsym;
    wsym[12] = forsym;
    wsym[13] = funcsym;
    wsym[14] = ifsym;
    wsym[15] = insym;
    wsym[16] = notsym;
    wsym[17] = oddsym;
    wsym[18] = orsym;
    wsym[19] = printsym;
    wsym[20] = readsym;
    wsym[21] = repeatsym;
    wsym[22] = returnsym;
    wsym[23] = switchsym;
    wsym[24] = tofloatsym;
    wsym[25] = tointsym;
    wsym[26] = truesym;
    wsym[27] = varsym;
    wsym[28] = whilesym;

    /* 设置指令名称 */
    strcpy(&(mnemonic[lit][0]), "lit");
    strcpy(&(mnemonic[opr][0]), "opr");
    strcpy(&(mnemonic[lod][0]), "lod");
    strcpy(&(mnemonic[sto][0]), "sto");
    strcpy(&(mnemonic[cal][0]), "cal");
    strcpy(&(mnemonic[ini][0]), "int");
    strcpy(&(mnemonic[jmp][0]), "jmp");
    strcpy(&(mnemonic[jpc][0]), "jpc");

    /* 设置符号集 */
    for (i = 0; i < symnum; i++)
    {
        declbegsys[i] = false;
        statbegsys[i] = false;
        facbegsys[i] = false;
    }

    /* 设置声明开始符号集 */
    declbegsys[varsym] = true;
    declbegsys[funcsym] = true;
    declbegsys[boolsym] = true;
    declbegsys[constsym] = true;
    declbegsys[floatsym] = true;

    /* 设置语句开始符号集 */
    statbegsys[ifsym] = true;
    statbegsys[whilesym] = true;
    statbegsys[readsym] = true;
    statbegsys[printsym] = true;
    statbegsys[ident] = true;
    statbegsys[tointsym] = true;
    statbegsys[tofloatsym] = true;
    statbegsys[forsym] = true;
    statbegsys[callsym] = true;
    statbegsys[switchsym] = true;
    statbegsys[repeatsym] = true;
    statbegsys[exitsym] = true;

    /* 设置因子开始符号集 */
    facbegsys[ident] = true;
    facbegsys[number] = true;
    facbegsys[lparen] = true;
    facbegsys[tointsym] = true;
    facbegsys[tofloatsym] = true;
    facbegsys[truesym] = true;
    facbegsys[falsesym] = true;

    /* 初始化break, continue记录个数为零,
     * 初始化标记为不在循环体中 */
    for(i = 0; i < 10; i++)
    {
        break_n[i] = 0;
        isjmp[i] = false;
        continue_n[i] = 0;
    }

    err = 0;
    cc = ll = cx = 0;
    ch = ' ';
}


/* 用数组实现集合的集合运算 */

/* 是否在集合中的真假运算 */
int inset(int e, bool* s)
{
    return s[e];
}

/* 并运算 s1Us2 */
int addset(bool* sr, bool* s1, bool* s2, int n)
{
    int i;
    for (i = 0; i < n; i++)
    {
        sr[i] = s1[i] || s2[i];
    }
    return 0;
}

/* 差运算 s1-s2 */
int subset(bool* sr, bool* s1, bool* s2, int n)
{
    int i;
    for (i=0; i<n; i++)
    {
        sr[i] = s1[i] && (!s2[i]);
    }
    return 0;
}

/* 交运算 s1ns2*/
int mulset(bool* sr, bool* s1, bool* s2, int n)
{
    int i;
    for (i=0; i<n; i++)
    {
        sr[i] = s1[i] && s2[i];
    }
    return 0;
}


/* 出错处理，打印出错位置和错误编码 */
void error(int n)
{
    char space[81];
    memset(space,32,81);    /* 32为asii码表的空格 初始化字符串为全空格*/
    space[cc-1]=0;          /* 出错时当前符号已经读完，所以cc-1 */

    printf("**%s^%d\n", space, n);
    fprintf(foutput,"**%s^%d\n", space, n);

    err = err + 1;
    if (err > maxerr)
    {
        printf("There are too many errors to handle!:(\n");
        exit(1);
    }
}


/*
 * 过滤空格和两种表示方法的注释，读取一个字符
 * 每次读一行，存入line缓冲区，line被getsym取空后再读一行
 * 被函数getsym调用
 */
void getch()
{
    if (cc == ll)           /* 判断缓冲区中是否有字符，若无字符，则读入下一行字符到缓冲区中 */
    {                       /* 无缓存情况 */
        if (feof(fin))      /* feof(fin)非零则文件结束 */
        {
            isend = true;
        }

        printf("%d ", cx);  /* 输出虚拟机代码指针 */
        fprintf(foutput,"%d ", cx);

        ll = 0;             /* 文件没有结束 */
        cc = 0;

        ch = ' ';
        while (ch != 10)   /* 10为asii码表的换行键 */
        {
            char pre = ch;
            if (EOF == fscanf(fin,"%c", &ch) || (pre == '/' && ch == '/' ))   /* 如果读取到文件结束/行注释 */
            {
                if(ll != 0)                         /* 该行头两个字符不是"//" */
                {
                    line[ll] = 0;                   /* 字符串结束\0, 即该行缓存结束 */
                }
                if(pre == '/' && ch == '/')
                {
                    while(EOF != fscanf(fin, "%c", &ch) && ch != 10)          /* 忽略掉"//"后面的内容直至读到'\n' */
                    {
                        ;
                    }
                    if(ch !=10)
                        isend = true;

                    if(ll == 0)                     /* 该行头两个字符就是"//" */
                    {
                        ch = ' ';
                        continue;
                    }

                    printf("\n");
                    fprintf(foutput,"\n");
                }
                else
                {
                    isend = true;
                }
                break;
            }
            if(pre == '/' && ch == '*')
            {
                line[ll] = 0;
                bool isenter = false;               /* 块注释内是否有换行符, 有则输出一个换行 */
                fscanf(fin, "%c", &pre);
                fscanf(fin, "%c", &ch);
                if(pre == '\n')
                    isenter = true;
                while(pre != '*' || ch != '/')
                {
                    pre = ch;
                    if(pre == '\n')
                        isenter = true;
                    if (EOF == fscanf(fin,"%c", &ch))
                    {
                        error(29);                  // 缺少"*/"
                    }
                }
                if(ll == 0)                         // 该行头两个字符就是"/*"
                {
                    ch = ' ';
                    continue;
                }
                if(isenter == true)
                {
                    printf("\n");
                    fprintf(foutput,"\n");
                }
                break;
            }
            if(pre == '/')
            {
                printf("%c", pre);                   /* 通过上面判断未跳出循环则'/'非注释所用可以输出 */
                fprintf(foutput, "%c", pre);
                line[ll] = pre;
                ll++;
            }
            if(ch != '/')
            {
                printf("%c", ch);                   /* 当前字符为'/', 是否用于注释有待进一步判断 */
                fprintf(foutput, "%c", ch);
                line[ll] = ch;
                ll++;
            }
        }
    }

    if(isend == true && ll == 0)
        ch = 0;
    else
    {
        ch = line[cc];          /* 有缓存情况 */
        cc++;
    }
}


/* 词法分析，获取一个符号 */
void getsym()
{
    int i, j, k;
    if(ch == 0)
        sym = eofsym;
    else
    {
        while (ch == ' ' || ch == 10 || ch == 9)    /* 过滤空格、换行和制表符 */
        {
            getch();
        }

        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) /* 当前单词是标识符或是保留字 */
        {
            /* 单词保存进字符串a中 */
            k = 0;
            do {
                if(k < al)
                {
                    a[k] = ch;
                    k++;
                }
                getch();
            }while ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'));
            a[k] = 0;

            strcpy(id, a);

            /* 搜索当前单词是否为保留字，使用二分法查找 */
            i = 0;
            j = norw - 1;
            do {
                k = (i + j) / 2;
                if (strcmp(id, word[k]) <= 0)
                {
                    j = k - 1;
                }
                if (strcmp(id, word[k]) >= 0)
                {
                    i = k + 1;
                }
            } while (i <= j);
            if (i-1 > j)        /* 当前单词为保留字 */
            {
                sym = wsym[k];
                if(sym == wsym[10])
                {
                    sym = number;
                    num = 0; // false
                }
                else if(sym == wsym[26])
                {
                    sym = number;
                    num = 1; // true
                }
            }
            else                /* 当前单词为标识符 */
            {
                sym = ident;
            }
        }
        else if (ch >= '0' && ch <= '9') /* 当前的单词是数字 */
        {
            sym = number;

            /* 获取数字的值 */
            num = 0;
            k = 0;
            do {
                num = 10 * num + ch - '0';
                k++;
                getch();
            } while (ch >= '0' && ch <= '9');

            float weight = 0.1;
            if(isfor == false && ch == '.')
            {
                getch();
                do{
                    num += weight * (ch - '0');
                    weight *= 0.1;
                    k++;
                    getch();
                } while(ch >= '0' && ch <= '9');
            }
            k--;
            if (k > nmax)       /* 数字位数太多 */
            {
                error(30);
            }
        }
        else if(ch == '!')
        {
            getch();
            if(ch == '=')
            {
                sym = neq;
                getch();
            }
            else
            {
                sym = nul;      /* 不能识别的符号 */
            }
        }
        else if(ch == '.')
        {
            getch();
            if(ch == '.')
            {
                getch();
                if(ch == '.')
                {
                    sym = threepoints;
                    getch();
                }
                else
                {
                    sym = nul;  /* 不能识别的符号 */
                }
            }
            else
            {
                sym = nul;      /* 不能识别的符号 */
            }
        }
        else if (ch == '<')     /* 检测小于或小于等于符号 */
        {
            getch();
            if (ch == '=')
            {
               sym = leq;
               getch();
            }
            else
            {
               sym = lss;
            }
        }
        else if (ch == '>')     /* 检测大于或大于等于符号 */
        {
            getch();
            if (ch == '=')
            {
                sym = geq;
                getch();
            }
            else
            {
                sym = gtr;
            }
        }
        else if (ch == '=')     /* 检测等于符号 */
        {
            getch();
            if (ch == '=')
            {
                sym = eql;
                getch();
            }
            else
            {
                sym = becomes;  /* 赋值符号 */
            }
        }
        else if (ch == '+')
        {
            getch();
            if (ch == '+')
            {
                sym = plusone;
                getch();
            }
            else
            {
                sym = plus;
            }
        }
        else if (ch == '-')
        {
            getch();
            if (ch == '-')
            {
                sym = minusone;
                getch();
            }
            else
            {
                sym = minus;
            }
        }
        else if (ch == 0)
        {
            sym = eofsym;
        }
        else
        {
            sym = ssym[ch];     /* 当符号不满足上述条件时，全部按照单字符符号处理 */
            getch();
        }
    }
}


/*
 * 生成虚拟机代码
 *
 * x: instruction.f;
 * y: instruction.l;
 * z: instruction.a;
 */
void gen(enum fct x, int y, float z)
{
    if (cx >= cxmax)
    {
        printf("Program is too long!X(\n");               /* 生成的虚拟机代码程序过长 */
        exit(1);
    }
    if ( z >= amax)
    {
        printf("Displacement address is too big!8(\n");   /* 地址偏移越界 */
        exit(1);
    }
    code[cx].f = x;
    code[cx].l = y;
    code[cx].a = z;
    cx++;
}


/*
 * 测试当前符号是否合法
 *
 * 在语法分析程序的入口和出口处调用测试函数test，
 * 检查当前单词进入和退出该语法单位的合法性
 *
 * s1:  需要的单词集合
 * s2:  如果不是需要的单词，在某一出错状态时，
 *      可恢复语法分析继续正常工作的补充单词符号集合
 * n:   错误编号
 */
void test(bool* s1, bool* s2, int n)
{
    if (!inset(sym, s1))
    {
        error(n);
        /* 当检测不通过时，不停获取符号，直到它属于需要的集合或补救的集合 */
        while ((!inset(sym,s1)) && (!inset(sym,s2)))
        {
            getsym();
        }
    }
}


/*
 * 编译程序主体
 *
 * tx:     符号表当前尾指针
 * fsys:   当前模块后继符号集合
 * lev:    当前分程序所在层
 */
void program(int tx, bool* fsys, int lev)
{
    int i;
    int dx;                 /* 记录数据分配的相对地址 */
    int tx0;                /* 保留初始tx */
    int cx0;                /* 保留初始cx */
    bool nxtlev[symnum];    /* 在下级函数的参数中，符号集合均为值参，但由于使用数组实现，
                               传递进来的是指针，为防止下级函数改变上级函数的集合，开辟
                               新的空间传递给下级函数*/

    dx = 3;                 /* 三个空间用于存放静态链SL、动态链DL和返回地址RA  */
    tx0 = tx;               /* 记录本层标识符的初始位置 */
    table[tx0].adr = cx;    /* 记录当前层代码的开始位置 */
    gen(jmp, 0, 0);         /* 产生跳转指令，跳转位置未知暂时填0 */

    if (lev > levmax)       /* 嵌套层数过多 */
    {
        error(32);
    }

    getsym();

    var_declaration_list(&tx, &dx, lev);
    varmax = tx;

    while (sym == funcsym)  /* 遇到过程声明符号，开始处理过程声明 */
    {
        getsym();
        strcpy(current, id);
        printf("id:%s\n", id);
        if (sym == ident)
        {
            enter(function, &tx, &dx, lev);   /* 填写符号表 */
            getsym();
        }
        else
        {
            error(41);      /* function后应为标识符 */
        }

        if (sym == lparen)
        {
            getsym();
        }
        else
        {
            error(17);     /* 函数名后少了"(" */
        }

        parameter_n = 0;
        if (sym == varsym || sym == boolsym || sym == floatsym)
        {
            enum object type;
            if(sym == varsym)
            {
                type = variable;
            }
            else if(sym == boolsym)
            {
                type = boolean;
            }
            else
            {
                type = floatnum;
            }
            getsym();
            if (sym == ident)
            {
                strcpy(p[parameter_n].name, id);
                p[parameter_n++].type = type;
                getsym();
            }
            else
            {
                error(43);
            }

            while(sym == comma)
            {
                getsym();
                if (sym == varsym || sym == boolsym || sym == floatsym)
                {
                    enum object type;
                    if(sym == varsym)
                    {
                        type = variable;
                    }
                    else if(sym == boolsym)
                    {
                        type = boolean;
                    }
                    else
                    {
                        type = floatnum;
                    }
                    getsym();
                    if (sym == ident)
                    {
                        strcpy(p[parameter_n].name, id);
                        p[parameter_n++].type = type;
                        getsym();
                    }
                    else
                    {
                        error(43);
                    }
                }
                else
                {
                    error(44);
                    break;
                }
            }
        }

        if (sym == rparen)
        {
            getsym();
        }
        else
        {
            error(18);      /* 函数名后少了")" */
        }
        if (sym == lbrace)
        {
            //getsym();     program内开头有getsym()故不需要此处的getsym()
        }
        else
        {
            error(19);     /* 函数名后少了"{" */
        }

        memcpy(nxtlev, fsys, sizeof(bool) * symnum);
        nxtlev[rbrace] = true;

        // start: program(tx, nxtlev, lev + 1);   /* 嵌套调用, 又因为levmax=1, 所以下一层不可以有func定义*/
        int ii,idx,itx0,icx0;
        bool inxtlev[symnum];
        idx = 3;
        itx0 = tx;
        table[itx0].adr = cx;
        gen(jmp, 0, 0);
        if(lev+1 > levmax)
        {
            error(32);
        }
        for(ii = 0;ii< parameter_n;ii++)
        {
            strcpy(id, p[ii].name);
            enter(p[ii].type, &tx, &idx, lev+1);
        }
        getsym();
        var_declaration_list(&tx, &idx, lev+1);
        memcpy(inxtlev, statbegsys, sizeof(bool)*symnum);
        test(inxtlev, declbegsys, 7);
        code[table[itx0].adr].a = cx;
        table[itx0].adr = cx;
        table[itx0].size = idx;
        table[itx0].para_n = parameter_n;
        icx0 = cx;
        gen(ini, 0, idx);
        if(tableswitch)                     /* 输出符号表 */
        {
            for (ii = 1; ii <= tx; ii++)
            {
                switch(table[ii].kind)
                {
                    case variable:
                        printf("    %d var   %s ", ii, table[ii].name);
                        printf("lev=%d addr=%d\n", table[ii].level, table[ii].adr);
                        fprintf(ftable, "    %d var   %s ", ii, table[ii].name);
                        fprintf(ftable, "lev=%d addr=%d\n", table[ii].level, table[ii].adr);
                        break;
                    case boolean:
                        printf("    %d bool  %s ", ii, table[ii].name);
                        printf("lev=%d addr=%d\n", table[ii].level, table[ii].adr);
                        fprintf(ftable, "    %d bool  %s ", ii, table[ii].name);
                        fprintf(ftable, "lev=%d addr=%d\n", table[ii].level, table[ii].adr);
                        break;
                    case constant:
                        printf("    %d const %s ", ii, table[ii].name);
                        printf("val=%d\n", table[ii].val);
                        fprintf(ftable, "    %d const %s ", ii, table[ii].name);
                        fprintf(ftable, "val=%d\n", table[ii].val);
                        break;
                    case floatnum:
                        printf("    %d float %s ", ii, table[ii].name);
                        printf("lev=%d addr=%d\n", table[ii].level, table[ii].adr);
                        fprintf(ftable, "    %d float %s ", ii, table[ii].name);
                        fprintf(ftable, "lev=%d addr=%d\n", table[ii].level, table[ii].adr);
                        break;
                    case function:
                        printf("    %d func  %s ", ii, table[ii].name);
                        printf("lev=%d addr=%d size=%d para_n=%d isreturn=%d\n", table[ii].level, table[ii].adr, table[ii].size, table[ii].para_n, table[ii].isreturn);
                        fprintf(ftable, "    %d func  %s ", ii, table[ii].name);
                        fprintf(ftable, "lev=%d addr=%d size=%d para_n=%d isreturn=%d\n", table[ii].level, table[ii].adr, table[ii].size, table[ii].para_n, table[ii].isreturn);
                        break;
                }
            }
            printf("\n");
            fprintf(ftable, "\n");
        }

        memcpy(inxtlev, fsys, sizeof(bool) * symnum);  /* 每个后继符号集合都包含上层后继符号集合，以便补救 */
        inxtlev[rbrace] = true;
        statement_list(&tx, inxtlev, lev+1);

        is_return = false;
        if (sym == returnsym)
        {
            getsym();
            memcpy(inxtlev, fsys, sizeof(bool) * symnum);
            inxtlev[semicolon] = true;           /* 后继符号为';' */
            expression(&tx, inxtlev, lev + 1);
            is_return = true;

            if(sym == semicolon)
                getsym();
            else
            {
                error(5);
            }
        }
        table[itx0].isreturn = is_return;

        while (exit_n > 0)
        {
            code[exit_cx[--exit_n]].a = cx;
        }

        gen(opr, 0, 0);                               /* 每个过程出口都要使用的释放数据段指令 */

        for(ii = 0; ii < 10; ii++)
        {
            if(break_n[ii] != 0)
            {
                error(23);
            }
        }
        listcode(icx0);                                /* 输出本分程序生成的代码 */
        // end: program(tx, nxtlev, lev + 1);   /* 嵌套调用, 又因为levmax=1, 所以下一层不可以有func定义*/

        if (sym == rbrace)
        {
            getsym();
            memcpy(nxtlev, statbegsys, sizeof(bool) * symnum);
            nxtlev[funcsym] = true;
            test(nxtlev, fsys, 6);
        }
        else
        {
            error(20);     /* 函数名后少了"}" */
        }
    }
    memcpy(nxtlev, statbegsys, sizeof(bool) * symnum);
    test(nxtlev, declbegsys, 7);

    code[table[tx0].adr].a = cx;        /* 把前面生成的跳转语句的跳转位置改成当前位置 */
    table[tx0].adr = cx;                /* 记录当前过程代码地址 */
    table[tx0].size = dx;               /* 声明部分中每增加一条声明都会给dx增加1，声明部分已经结束，dx就是当前过程数据的size */

    cx0 = cx;
    gen(ini, 0, dx);                    /* 生成指令，此指令执行时在数据栈中为被调用的过程开辟dx个单元的数据区 */

    if(tableswitch)                     /* 输出符号表 */
    {
        for (i = 1; i <= tx; i++)
        {
            switch(table[i].kind)
            {
                case variable:
                    printf("    %d var   %s ", i, table[i].name);
                    printf("lev=%d addr=%d\n", table[i].level, table[i].adr);
                    fprintf(ftable, "    %d var   %s ", i, table[i].name);
                    fprintf(ftable, "lev=%d addr=%d\n", table[i].level, table[i].adr);
                    break;
                case function:
                    printf("    %d func  %s ", i, table[i].name);
                    printf("lev=%d addr=%d size=%d para_n=%d isreturn=%d\n", table[i].level, table[i].adr, table[i].size, table[i].para_n, table[i].isreturn);
                    fprintf(ftable, "    %d func  %s ", i, table[i].name);
                    fprintf(ftable, "lev=%d addr=%d size=%d para_n=%d isreturn=%d\n", table[i].level, table[i].adr, table[i].size, table[i].para_n, table[i].isreturn);
                    break;
                case boolean:
                    printf("    %d bool  %s ", i, table[i].name);
                    printf("lev=%d addr=%d\n", table[i].level, table[i].adr);
                    fprintf(ftable, "    %d bool  %s ", i, table[i].name);
                    fprintf(ftable, "lev=%d addr=%d\n", table[i].level, table[i].adr);
                    break;
                case constant:
                    printf("    %d const %s ", i, table[i].name);
                    printf("val=%d\n", table[i].val);
                    fprintf(ftable, "    %d const %s ", i, table[i].name);
                    fprintf(ftable, "val=%d\n", table[i].val);
                    break;
                case floatnum:
                    printf("    %d float %s ", i, table[i].name);
                    printf("lev=%d addr=%d\n", table[i].level, table[i].adr);
                    fprintf(ftable, "    %d float %s ", i, table[i].name);
                    fprintf(ftable, "lev=%d addr=%d\n", table[i].level, table[i].adr);
                    break;
            }
        }
        printf("\n");
        fprintf(ftable, "\n");
    }

    memcpy(nxtlev, fsys, sizeof(bool) * symnum);  /* 每个后继符号集合都包含上层后继符号集合，以便补救 */

    current[0] = 0;
    statement_list(&tx, nxtlev, lev);

    while (exit_n > 0)
    {
        code[exit_cx[--exit_n]].a = cx;
    }

    gen(opr, 0, 0);                               /* 每个过程出口都要使用的释放数据段指令 */

    for(i = 0; i < 10; i++)
    {
        if(break_n[i] != 0)
        {
            error(23);
        }
    }


    if (!feof(fin))
    {
        memset(nxtlev, 0, sizeof(bool) * symnum); /* 分程序没有补救集合 */
        test(fsys, nxtlev, 8);                    /* 检测后继符号正确性 */
    }

    listcode(cx0);                                /* 输出本分程序生成的代码 */
}


/*
 * 在符号表中加入一项
 *
 * k:      标识符的种类
 * ptx:    符号表尾指针的指针，为了可以改变符号表尾指针的值
 * pdx:    dx为当前应分配的变量的相对地址，分配后要增加1
 * lev:    标识符所在的层次
 */
void enter(enum object k, int* ptx, int* pdx, int lev)
{
    (*ptx)++;
    strcpy(table[(*ptx)].name, id);     /* 符号表的name域记录标识符的名字 */
    table[(*ptx)].kind = k;
    switch (k)
    {
        case variable:                  /* 变量 */
            table[(*ptx)].level = lev;
            table[(*ptx)].adr = (*pdx);
            (*pdx)++;
            break;
        case function:                  /* 函数 */
            table[(*ptx)].level = lev;
            break;
        case boolean:
            table[(*ptx)].level = lev;
            table[(*ptx)].adr = (*pdx);
            (*pdx)++;
            break;
        case constant:
            if (num > amax)
            {
                error(30);              /* 常数越界 */
                num = 0;
            }
            table[(*ptx)].val = num;    /* 登记常数的值 */
            break;
        case floatnum:
            table[(*ptx)].level = lev;
            table[(*ptx)].adr = (*pdx);
            (*pdx)++;
            break;
    }
}


/*
 * 查找标识符在符号表中的位置，从tx开始倒序查找标识符
 * 找到则返回在符号表中的位置，否则返回0
 *
 * id:    要查找的名字
 * tx:    当前符号表尾指针
 */
int position(char* id, int tx)
{
    int i;
    strcpy(table[0].name, id);
    i = tx;
    while (strcmp(table[i].name, id) != 0 && strcmp(table[i].name, current)!= 0)
    {
        i--;
    }
    if(strcmp(table[i].name, id) != 0)
    {
        i = varmax;
        while (strcmp(table[i].name, id) != 0)
        {
            i--;
        }
    }
    return i;
}


/* 变量声明处理 */
void var_declaration_list(int* ptx, int* pdx, int lev)
{
    bool nxtlev[symnum];
    memcpy(nxtlev, declbegsys, sizeof(bool) * symnum);
    nxtlev[funcsym] = false;
    while (inset(sym, nxtlev))              /* 遇到变量声明符号，开始处理变量声明 */
    {
        enum object type;
        if(sym == varsym)
        {
            type = variable;
        }
        else if(sym == boolsym)
        {
            type = boolean;
        }
        else if(sym == constsym)
        {
            type = constant;
        }
        else if(sym == floatsym)
        {
            type = floatnum;
        }
        getsym();
        if (sym == ident && type != constant)
        {
            getsym();
            if (sym != lbrack)
                enter(type, ptx, pdx, lev); /* 填写符号表 */
        }
        else if(sym == ident && type == constant)
        {
            getsym();
            if (sym == eql || sym == becomes)
            {
                if (sym == eql)
                {
                    error(1);           /* 把=写成了== */
                }
                getsym();
                if (sym == number)
                {
                    enter(constant, ptx, pdx, lev);
                    getsym();
                }
                else
                {
                    error(2);           /* 常量声明中的=后应是数字 */
                }
            }
            else
            {
                error(3);               /* 常量声明中的标识符后应是= */
            }
        }
        else
        {
            error(42);                 /* var后面应是标识符 */
        }

        /* 是否是数组 */
        if (sym == lbrack)
        {
            getsym();
            if(sym == number)
            {
                int tmpi = 0;
                while(tmpi < num)
                {
                    enter(type, ptx, pdx, lev);
                    table[(*ptx)].size = tmpi;
                    tmpi++;
                }
                getsym();
            }
            else
                error(26);

            if (sym == rbrack)
            {
                getsym();
            }
            else
                error(27);
        }

        if (sym == semicolon)
        {
            getsym();
        }
        else
        {
            error(5);                 /* 漏掉了分号 */
        }
    }
}


/* 输出目标代码清单 */
void listcode(int cx0)
{
    int i;
    if (listswitch)
    {
        printf("\n");
        for (i = cx0; i < cx; i++)
        {
            printf("%d %s %d %f\n", i, mnemonic[code[i].f], code[i].l, code[i].a);
        }
    }
}

/* 输出所有目标代码并写入fcode文件 */
void listall()
{
    int i;
    if (listswitch)
    {
        for (i = 0; i < cx; i++)
        {
            printf("%d %s %d %f\n", i, mnemonic[code[i].f], code[i].l, code[i].a);
            fprintf(fcode,"%d %s %d %f\n", i, mnemonic[code[i].f], code[i].l, code[i].a);
        }
    }
}

/* 语句处理 */
void statement_list(int* ptx, bool* fsys, int lev)
{
    int i, cx1, cx2;
    bool nxtlev[symnum];
    while (inset(sym, statbegsys)||( (sym == continuesym || sym == breaksym ) && isjmp[circlenum] == true ) )  /* 遇到变量声明符号，开始处理变量声明 */
    {
        if (sym == ifsym)
        {
            getsym();
            memcpy(nxtlev, fsys, sizeof(bool) * symnum);
            nxtlev[lbrace] = true;  /* 后继符号为'{' */
            condition(ptx, nxtlev, lev); /* 调用条件处理 */

            if (sym == lbrace)
            {
                getsym();
            }
            else
            {
                error(52);         /* 疑似少了"{" */
            }

            cx1 = cx;               /* 保存当前指令地址 */
            gen(jpc, 0, 0);         /* 生成条件跳转指令, 跳转地址未知, 暂写作0 */
            if(sym != rbrace)
                statement_list(ptx, nxtlev, lev);
            code[cx1].a = cx;       /* 经statement处理后, cx为then后语句执行完的位置, 它正是前面未定的跳转地址, 此时进行回填 */

            if (sym == rbrace)
            {
                getsym();
            }
            else
            {
                error(53);          /* 疑似缺少"}" */
            }

            if(sym == elsesym)
            {
                cx2 = cx;
                gen(jmp, 0, 0);

                getsym();
                if (sym == lbrace)
                {
                    getsym();
                }
                else
                {
                    error(54);      /* 疑似缺少"{" */
                }

                if(sym != rbrace)
                    statement_list(ptx, nxtlev, lev);

                code[cx2].a = cx;
                code[cx1].a = code[cx1].a + 1;

                if (sym == rbrace)
                {
                    getsym();
                }
                else
                {
                    error(55);      /* 疑似缺少"}" */
                }
            }
        }
        else if (sym == whilesym)
        {
            circlenum++;
            cx1 = cx;               /* 保存判断条件操作的位置 */
            getsym();
            memcpy(nxtlev, fsys, sizeof(bool) * symnum);
            nxtlev[lbrace] = true;  /* 后继符号为'{' */
            condition(ptx, nxtlev, lev); /* 调用条件处理 */
            cx2 = cx;               /* 保存循环体的结束的下一个位置 */
            gen(jpc, 0, 0);         /* 生成条件跳转，但跳出循环的地址未知，标记为0等待回填 */

            if (sym == lbrace)
            {
                getsym();
            }
            else
            {
                error(56);          /* 疑似少了"{" */
            }

            fsys[continuesym] = true;
            fsys[breaksym] = true;
            isjmp[circlenum] = true;
            if(sym != rbrace)
                statement_list(ptx, fsys, lev);
            isjmp[circlenum] = false;

            if (sym == rbrace)
            {
                getsym();
            }
            else
            {
                error(57);         /* 疑似缺少"}" */
            }

            gen(jmp, 0, cx1);
            code[cx2].a = cx;

            while (break_n[circlenum] > 0)
            {
                code[break_cx[circlenum][--break_n[circlenum]]].a = cx;
            }
            while (continue_n[circlenum] > 0)
            {
                code[continue_cx[circlenum][--continue_n[circlenum]]].a = cx1;
            }
            circlenum--;
        }
        else if (sym == readsym)
        {
            getsym();

            if (sym != lparen)
            {
                error(34);          /* 格式错误，应是左括号 */
            }
            else
            {
                getsym();
                if (sym == ident)
                {
                    i = position(id, *ptx); /* 查找要读的变量 */
                }
                else
                {
                    i = 0;
                }

                if (i == 0)
                {
                    error(4);       /* read语句括号中的标识符应该是声明过的变量 */
                }
                else
                {
                    if(table[i].kind == variable)
                    {
                        gen(opr, 0, 16);/* 生成输入指令, 读取值到栈顶 */
                    }
                    else if(table[i].kind == floatnum)
                    {
                        gen(opr, 0, 24);
                    }
                    else if(table[i].kind == boolean)
                    {
                        gen(opr, 0, 25);
                    }
                    gen(sto, lev-table[i].level, table[i].adr); /* 将栈顶内容送入变量单元中 */
                }

                getsym();
            }
            if(sym != rparen)
            {
                error(33);          /* 格式错误，应是右括号 */
                while (!inset(sym, fsys))   /* 出错补救，直到遇到上层函数的后继符号 */
                {
                    getsym();
                }
            }
            else
            {
                getsym();
            }
        }
        else if (sym == printsym)
        {
            getsym();
            if (sym != lparen)
            {
                error(34);          /* 格式错误，应是左括号 */
            }
            else
            {
                getsym();
                memcpy(nxtlev, fsys, sizeof(bool) * symnum);
                nxtlev[rparen] = true;
                if (sym == ident)
                {
                    i = position(id, *ptx); /* 查找要读的变量 */
                }
                else
                {
                    i = 0;
                }


                if (i == 0)
                {
                    error(4);      /* print语句括号中的标识符应该是声明过的变量 */
                }
                else
                {
                    getsym();

                    /* 是否是数组 */
                    if(sym == lbrack)
                    {
                        getsym();
                        if(sym == number)
                        {
                            num = table[i].size - num;
                            i -= num;
                            getsym();
                        }
                        else
                        {
                            error(26);
                        }

                        if(sym == rbrack)
                        {
                            getsym();
                        }
                        else
                            error(27);
                    }

                    if(table[i].kind == constant)
                    {
                        gen(lit, 0, table[i].val);
                    }
                    else
                        gen(lod, lev-table[i].level, table[i].adr);

                    if(table[i].kind == variable || table[i].kind == constant)
                    {
                        gen(opr, 0, 14);    /* 生成输出指令，输出栈顶的值 */
                    }
                    else if(table[i].kind == floatnum)
                    {
                        gen(opr, 0, 22);
                    }
                    else if(table[i].kind == boolean)
                    {
                        gen(opr, 0, 23);
                    }
                    gen(opr, 0, 15);    /* 生成换行指令 */
                }

            }
            if(sym != rparen)
            {
                error(33);          /* 格式错误，应是右括号 */
                while (!inset(sym, fsys))   /* 出错补救，直到遇到上层函数的后继符号 */
                {
                    getsym();
                }
            }
            else
            {
                getsym();
            }
        }
        else if (sym == ident)
        {
            i = position(id, *ptx); /* 查找标识符在符号表中的位置 */

            if (i == 0)
            {
                printf("%d\n", sym);
                error(4);           /* 标识符未声明 */
            }
            else
            {
                if( (table[i].kind != variable && table[i].kind != boolean && table[i].kind != floatnum )|| table[i].kind == constant)
                {
                    error(12);      /* 赋值语句中，赋值号左部标识符应该是变量 */
                    i = 0;
                }
                else
                {
                    getsym();

                    /* 是否是数组 */
                    if(sym == lbrack)
                    {
                        getsym();
                        if(sym == number)
                        {
                            num = table[i].size - num;
                            i -= num;
                            getsym();
                        }
                        else
                        {
                            error(26);
                        }

                        if(sym == rbrack)
                        {
                            getsym();
                        }
                        else
                            error(27);
                    }

                    if(sym == eql || sym == becomes)
                    {
                        if (sym == eql)
                        {
                            error(1);           /* 把=写成了== */
                        }
                        getsym();

                        memcpy(nxtlev, fsys, sizeof(bool) * symnum);
                        nxtlev[semicolon] = true;

                        if(table[i].kind == variable)
                        {
                            isfloat = false;
                        }
                        else if(table[i].kind == floatnum)
                        {
                            isfloat = true;
                        }

                        int ii;
                        if(sym == callsym)
                        {
                            getsym();
                            if (sym == ident && table[ii = position(id, *ptx)].kind == function)
                            {
                                getsym();
                                if (sym != lparen)
                                {
                                    error(17);           /* 格式错误，应是左括号 */
                                }
                                else
                                {
                                    getsym();
                                }

                                if (sym == plus || sym == minus || inset(sym, facbegsys))
                                {
                                    int tn = 0;
                                    memcpy(nxtlev, fsys, sizeof(bool) * symnum);
                                    nxtlev[comma] = true;
                                    nxtlev[rparen] = true;
                                    expression(ptx, nxtlev, lev);
                                    tn++;
                                    while(sym == comma)
                                    {
                                        getsym();
                                        expression(ptx, nxtlev, lev);
                                        tn++;
                                    }

                                    if(tn != table[ii].para_n)
                                    {
                                        error(45);     /* 调用函数参数个数不对 */
                                    }

                                    gen(opr, tn, 28);
                                    gen(opr, tn, 30);
                                    gen(opr, tn, 29);
                                }


                                if (table[ii].kind == function)
                                {
                                    gen(cal, lev-table[ii].level, table[ii].adr); /* 生成call指令 */
                                    if(table[ii].isreturn == true)
                                    {
                                        gen(opr, table[ii].size, 31);
                                    }
                                }
                                else
                                {
                                    error(15);          /* 标识符类型应为函数 */
                                }

                                if(sym != rparen)
                                {
                                    error(18);          /* 格式错误，应是右括号 */
                                }
                                else
                                {
                                    getsym();
                                }
                            }
                        }
                        else
                            expression(ptx, nxtlev, lev);   /* 处理赋值符号右侧表达式 */

                        if(i != 0)
                        {
                            /* expression将执行一系列指令，但最终结果将会保存在栈顶，执行sto命令完成赋值 */
                            gen(sto, lev-table[i].level, table[i].adr);
                        }
                    }
                    else if(sym == plusone)
                    {
                        if(i != 0)
                        {
                            gen(lod, lev-table[i].level, table[i].adr);
                        }
                        gen(lit, 0, 1);
                        gen(opr, 0, 2);
                        gen(sto, lev-table[i].level, table[i].adr);
                        getsym();
                    }
                    else if(sym == minusone)
                    {
                        if(i != 0)
                        {
                            gen(lod, lev-table[i].level, table[i].adr);
                        }
                        gen(lit, 0, 1);
                        gen(opr, 0, 3);
                        gen(sto, lev-table[i].level, table[i].adr);
                        getsym();
                    }
                    else
                    {
                        error(13);                  /* 没有检测到赋值符号 */
                    }
                }
            }
        }
        else if (sym == tointsym)
        {
            getsym();
            if(sym == lparen)
            {
                getsym();
            }
            else
            {
                error(10);
            }
            if(sym == ident)            /* 因子为变量 */
            {
                i = position(id, *ptx); /* 查找标识符在符号表中的位置 */
                if (i == 0)
                {
                    error(4);           /* 标识符未声明 */
                }
                else
                {
                    switch (table[i].kind)
                    {
                        case constant:  /* 标识符为常量 */
                            error(21);
                            getsym();
                            break;
                        case boolean:
                            error(21);
                            getsym();
                            break;
                        case floatnum:
                            gen(lod, lev-table[i].level, table[i].adr); /* 找到变量地址并将其值入栈 */
                            gen(opr, 0, 20);//20 is to int
                            table[i].kind = variable;
                            getsym();
                            break;
                        case variable:  /* 标识符为变量 */
                            error(21);
                            getsym();
                            break;
                        case function:  /* 标识符为过程 */
                            error(21);
                            getsym();
                            break;
                    }
                }
            }
            if(sym == rparen)
            {
                getsym();
            }
            else
            {
                error(11);
            }
        }
        else if (sym == tofloatsym)
        {
            getsym();
            if(sym == lparen)
            {
                getsym();
            }
            else
            {
                error(10);
            }
            if(sym == ident)            /* 因子为变量 */
            {
                i = position(id, *ptx); /* 查找标识符在符号表中的位置 */
                if (i == 0)
                {
                    error(4);           /* 标识符未声明 */
                }
                else
                {
                    switch (table[i].kind)
                    {
                        case constant:  /* 标识符为常量 */
                            error(22);
                            getsym();
                            break;
                        case boolean:
                            error(22);
                            getsym();
                            break;
                        case floatnum:
                            error(22);
                            getsym();
                            break;
                        case variable:  /* 标识符为变量 */
                            gen(lod, lev-table[i].level, table[i].adr); /* 找到变量地址并将其值入栈 */
                            gen(opr, 0, 21);//21 is to float
                            table[i].kind = floatnum;
                            getsym();
                            break;
                        case function:  /* 标识符为过程 */
                            error(22);  /* 不能为过程 */
                            getsym();
                            break;
                    }
                }
            }
            if(sym == rparen)
            {
                getsym();
            }
            else
            {
                error(11);
            }
        }
        else if (sym == forsym)
        {
            circlenum++;
            isfor = true;
            getsym();

            int center = 0;             /* for与in之间的标识符位置标记 */
            if (sym == ident)
            {
                i = position(id, *ptx); /* 查找要读的变量 */
                center = i;
            }
            else
            {
                i = 0;
            }

            if (i == 0)
            {
                error(4);
            }

            getsym();

            if (sym == insym)
            {
                getsym();
            }
            else
            {
                error(49);             /* 缺少"in" */
            }

            if (sym == ident)
            {
                i = 0;
                i = position(id, *ptx); /* 查找标识符在符号表中的位置 */
                if (i == 0)
                {
                    error(4);           /* 标识符未声明 */
                }
                else
                {
                    if(table[i].kind != variable && table[i].kind != constant)
                    {
                        error(46);      /* for-in语句中，省略号左部标识符应该是整型变量 */
                        i = 0;
                    }
                    else
                    {
                        if (center != 0)
                        {
                            memcpy(nxtlev, fsys, sizeof(bool) * symnum);
                            nxtlev[threepoints] = true;
                            expression(ptx, nxtlev, lev);
                            /* expression将执行一系列指令，但最终结果将会保存在栈顶，执行sto命令完成赋值 */
                            gen(sto, lev-table[center].level, table[center].adr);
                        }
                        // getsym(); 删掉这一行是因为expression里面已经有getsym()了:P
                    }
                }
            }
            else if (sym == number)
            {
                memcpy(nxtlev, fsys, sizeof(bool) * symnum);
                nxtlev[threepoints] = true;
                expression(ptx, nxtlev, lev);   /* 处理赋值符号右侧表达式 */
                if(center != 0)
                {
                    /* expression将执行一系列指令，但最终结果将会保存在栈顶，执行sto命令完成赋值 */
                    gen(sto, lev-table[center].level, table[center].adr);
                }
                // getsym(); //删掉这一行是因为expression里面已经有getsym()了:P
            }
            else
            {
                error(46);         /* "in"后所接数字/变量有问题 */
            }

            if (sym == threepoints)
            {
                getsym();
            }
            else
            {
                error(48);         /* 缺少"..." */
            }

            if (sym == ident)
            {
                cx1 = cx;

                i = 0;
                i = position(id, *ptx); /* 查找标识符在符号表中的位置 */
                if (i == 0)
                {
                    error(4);           /* 标识符未声明 */
                }
                else
                {
                    if (table[i].kind != variable && table[i].kind != constant)
                    {
                        error(47);      /* for-in语句中，省略号右部标识符应该是变量 */
                        i = 0;
                    }
                    else
                    {
                        if (center != 0)
                        {
                            gen(lod, lev-table[center].level, table[center].adr);

                            memcpy(nxtlev, fsys, sizeof(bool) * symnum);
                            nxtlev[lbrace] = true;
                            expression(ptx, nxtlev, lev);

                            /* expression将执行一系列指令，但最终结果将会保存在栈顶，执行opr命令完成<=的比较 */
                            gen(opr, 0, 13);
                        }
                        // getsym(); 删掉这一行是因为expression里面已经有getsym()了:P
                    }
                }
                cx2 = cx;
                gen(jpc, 0 ,0);
            }
            else if (sym == number)
            {
                cx1 = cx;
                getsym();
                memcpy(nxtlev, fsys, sizeof(bool) * symnum);
                nxtlev[lbrace] = true;

                if(center != 0)
                {
                    gen(lod, lev-table[center].level, table[center].adr);
                    if (num > amax)
                    {
                        error(30); /* 数越界 */
                        num = 0;
                    }
                    gen(lit, 0, num);
                    gen(opr, 0, 13);
                }
                cx2 = cx;
                gen(jpc, 0 ,0);
            }
            else
            {
                error(47);         /* "..."后所接数字/变量有问题 */
            }

            if (sym == lbrace)
            {
                getsym();
            }
            else
            {
                error(50);         /* 疑似少了"{" */
            }

            isfor = false;
            isjmp[circlenum] = true;
            if(sym != rbrace)
                statement_list(ptx, fsys, lev);
            isjmp[circlenum] = false;

            while (continue_n[circlenum] > 0)
            {
                code[continue_cx[circlenum][--continue_n[circlenum]]].a = cx;
            }

            /* center指向的标识符递增一 */
            gen(lit, 0, 1);
            gen(lod, lev-table[center].level, table[center].adr);
            gen(opr, 0, 2);
            gen(sto, lev-table[center].level, table[center].adr);

            gen(jmp, 0, cx1);       /* 返回循环条件判断 */

            code[cx2].a = cx;

            if (sym == rbrace)
            {
                getsym();
            }
            else
            {
                error(51);         /* 疑似缺少"}" */
            }

            while (break_n[circlenum] > 0)
            {
                code[break_cx[circlenum][--break_n[circlenum]]].a = cx;
            }
            circlenum--;
        }
        else if (sym == callsym)
        {
            getsym();

            if (sym != ident)
            {
                error(14);          /* call后应为标识符 */
            }
            else
            {
                i = position(id, *ptx);
                if (i == 0)
                {
                    error(4);       /* 函数名未找到 */
                }
                getsym();
            }
            if (sym != lparen)
            {
                error(34);          /* 格式错误，应是左括号 */
            }
            else
            {
                getsym();
            }

            if (sym == plus || sym == minus || inset(sym, facbegsys))
            {
                int tn = 0;
                memcpy(nxtlev, fsys, sizeof(bool) * symnum);
                nxtlev[comma] = true;
                nxtlev[rparen] = true;
                expression(ptx, nxtlev, lev);
                tn++;
                while(sym == comma)
                {
                    getsym();
                    expression(ptx, nxtlev, lev);
                    tn++;
                }

                if(tn != table[i].para_n)
                {
                    error(45);     /* 调用函数参数个数不对 */
                }

                gen(opr, tn, 28);
                gen(opr, tn, 30);
                gen(opr, tn, 29);
            }


            if (table[i].kind == function)
            {
                gen(cal, lev-table[i].level, table[i].adr); /* 生成call指令 */
            }
            else
            {
                error(15);          /* call后标识符类型应为函数 */
            }

            if(sym != rparen)
            {
                error(33);          /* 格式错误，应是右括号 */
            }
            else
            {
                getsym();
            }
        }
        else if (sym == switchsym)
        {
            int case_cx[10];
            int case_n = 0;
            getsym();
            memcpy(nxtlev, fsys, sizeof(bool) * symnum);
            nxtlev[lbrace] = true;           /* 后继符号为'{' */
            expression(ptx, nxtlev, lev);    /* 调用条件处理 */

            if(sym == lbrace)
            {
                getsym();
                while(sym == casesym)
                {
                    getsym();
                    if (sym == number)
                    {
                        if (num > amax)
                        {
                            error(30);      /* 数越界 */
                            num = 0;
                        }
                        gen(lit, 0, num);
                        gen(opr, 0, 18);
                        cx1 = cx;           /* 保存当前指令地址 */
                        gen(jpc, 0, 0);
                        getsym();
                    }
                    else
                    {
                        error(36);
                    }

                    if (sym == colon)
                    {
                        getsym();
                        if(sym != breaksym)
                            statement_list(ptx, fsys, lev);
                    }
                    else
                    {
                        error(37);
                    }

                    if(sym == breaksym)
                    {
                        getsym();
                    }
                    else
                    {
                        error(38);
                    }

                    if (sym == semicolon)
                    {
                        getsym();
                    }
                    else
                    {
                        error(40);
                    }

                    case_cx[case_n++] = cx;
                    gen(jmp, 0, 0);

                    code[cx1].a = cx;
                }
                if(sym == defaultsym)
                {
                    getsym();
                    if(sym == colon)
                    {
                        gen(opr, 0, 17);
                        getsym();
                        if(sym != breaksym)
                            statement_list(ptx, fsys, lev);
                    }
                    else
                    {
                        error(37);
                    }
                    if(sym == breaksym)
                    {
                        getsym();
                    }
                    else
                    {
                        error(38);
                    }
                    if (sym == semicolon)
                    {
                        getsym();
                    }
                    else
                    {
                        error(40);
                    }
                    if(sym == rbrace)
                    {
                        getsym();
                    }
                    else
                    {
                        error(35);
                    }
                }
                else
                {
                    error(39);
                }
                while(case_n > 0)
                    code[case_cx[--case_n]].a = cx;
            }
        }
        else if (sym == repeatsym)
        {
            circlenum++;
            getsym();
            cx1 = cx;
            if (sym == lbrace)
            {
                getsym();
            }
            else
            {
                error(56);          /* 疑似少了"{" */
            }

            fsys[rbrace] = true;
            isjmp[circlenum] = true;
            if(sym != rbrace)
                statement_list(ptx, fsys, lev);
            isjmp[circlenum] = false;

            if (sym == rbrace)
            {
                getsym();
            }
            else
            {
                error(57);         /* 疑似缺少"}" */
            }

            if(sym == whilesym)
            {
                getsym();
                memcpy(nxtlev, fsys, sizeof(bool) * symnum);
                nxtlev[semicolon] = true;  /* 后继符号为'{' */
                condition(ptx, nxtlev, lev); /* 调用条件处理 */
                cx2 = cx;
                gen(jpc, 0, 0);
                gen(jmp, 0, cx1);
                code[cx2].a = cx;

                while (break_n[circlenum] > 0)
                {
                    code[break_cx[circlenum][--break_n[circlenum]]].a = cx;
                }

                while (continue_n[circlenum] > 0)
                {
                    code[continue_cx[circlenum][--continue_n[circlenum]]].a = cx1;
                }

                circlenum--;
            }
        }
        else if (sym == continuesym && isjmp[circlenum] == true)
        {
            continue_cx[circlenum][continue_n[circlenum]++] = cx;
            gen(jmp, 0, 0);
            getsym();
        }
        else if (sym == breaksym && isjmp[circlenum] == true)
        {
            break_cx[circlenum][break_n[circlenum]++] = cx;
            gen(jmp, 0, 0);
            getsym();
        }
        else if(sym == exitsym)
        {
            exit_cx[exit_n++] = cx;
            gen(jmp, 0, 0);
            getsym();
        }

        if (sym == semicolon)
        {
            getsym();
        }
        else
        {
            printf("%d\n", sym);
            error(5);               /* 漏掉了分号 */
        }
    }
}


/* 表达式处理 */
void expression(int* ptx, bool* fsys, int lev)
{
    enum symbol addop;              /* 用于保存正负号 */
    bool nxtlev[symnum];

    if(sym == plus || sym == minus || sym == notsym) /* 表达式开头有正负号，此时当前表达式被看作一个正的或负的项 */
    {
        addop = sym;                /* 保存开头的正负号 */
        getsym();
        memcpy(nxtlev, fsys, sizeof(bool) * symnum);
        nxtlev[plus] = true;
        nxtlev[minus] = true;
        term(ptx, nxtlev, lev);     /* 处理项 */
        if (addop == minus)
        {
            gen(opr, 0, 1);         /* 如果开头为负号生成取负指令 */
        }
        if (addop == notsym)
        {
            gen(opr, 0, 26);
        }
    }
    else                            /* 此时表达式被看作项的加减 */
    {
        memcpy(nxtlev, fsys, sizeof(bool) * symnum);
        nxtlev[plus] = true;
        nxtlev[minus] = true;
        term(ptx, nxtlev, lev);     /* 处理项 */
    }
    while (sym == plus || sym == minus)
    {
        addop = sym;
        getsym();
        memcpy(nxtlev, fsys, sizeof(bool) * symnum);
        nxtlev[plus] = true;
        nxtlev[minus] = true;
        term(ptx, nxtlev, lev);     /* 处理项 */
        if (addop == plus)
        {
            gen(opr, 0, 2);         /* 生成加法指令 */
        }
        else
        {
            gen(opr, 0, 3);         /* 生成减法指令 */
        }
    }
}


/* 项处理 */
void term(int* ptx, bool* fsys, int lev)
{
    enum symbol mulop;              /* 用于保存乘除法符号 */
    bool nxtlev[symnum];

    memcpy(nxtlev, fsys, sizeof(bool) * symnum);
    nxtlev[times] = true;
    nxtlev[slash] = true;
    nxtlev[mod] = true;
    nxtlev[andsym] = true;
    nxtlev[orsym] = true;
    factor(ptx, nxtlev, lev);       /* 处理因子 */
    while(sym == times || sym == slash || sym == mod)
    {
        mulop = sym;
        getsym();
        factor(ptx, nxtlev, lev);
        if(mulop == times)
        {
            gen(opr, 0, 4);         /* 生成乘法指令 */
        }
        else if(mulop == slash)
        {
            if(isfloat == false)
            {
                gen(opr, 0, 5);     /* 生成除法指令 */
            }
            if(isfloat == true)
            {
                gen(opr, 0, 27);
            }
        }
        else
        {
            gen(opr, 0, 7);         /* 生成取余指令 */
        }
    }
    while(sym == andsym || sym == orsym )
    {
        mulop = sym;
        getsym();
        factor(ptx, nxtlev, lev);
        if(mulop == andsym)
        {
            gen(opr, 0, 4);         /* 生成乘法指令 */
        }
        else if(mulop == orsym)
        {
            gen(opr, 0, 19);         /* 生成或指令 */
        }
    }
}


/* 因子处理 */
void factor(int* ptx, bool* fsys, int lev)
{
    int i;
    bool nxtlev[symnum];
    test(facbegsys, fsys, 24);      /* 检测因子的开始符号 */
    while(inset(sym, facbegsys))    /* 循环处理因子 */
    {
        if(sym == ident)            /* 因子为变量 */
        {
            i = position(id, *ptx); /* 查找标识符在符号表中的位置 */
            if (i == 0)
            {
                error(4);           /* 标识符未声明 */
            }
            else
            {
                switch (table[i].kind)
                {
                    case constant:  /* 标识符为常量 */
                        gen(lit, 0, table[i].val);  /* 直接把常量的值入栈 */
                        getsym();
                        break;
                    case boolean:
                        getsym();
                        /* 是否是数组 */
                        if(sym == lbrack)
                        {
                            getsym();
                            if(sym == number)
                            {
                                num = table[i].size - num;
                                i -= num;
                                getsym();
                            }
                            else
                            {
                                error(26);
                            }

                            if(sym == rbrack)
                            {
                                getsym();
                            }
                            else
                                error(27);
                        }
                        gen(lod, lev-table[i].level, table[i].adr); /* 找到变量地址并将其值入栈 */
                        break;
                    case floatnum:
                        getsym();
                        /* 是否是数组 */
                        if(sym == lbrack)
                        {
                            getsym();
                            if(sym == number)
                            {
                                num = table[i].size - num;
                                i -= num;
                                getsym();
                            }
                            else
                            {
                                error(26);
                            }

                            if(sym == rbrack)
                            {
                                getsym();
                            }
                            else
                                error(27);
                        }
                        gen(lod, lev-table[i].level, table[i].adr); /* 找到变量地址并将其值入栈 */
                        isfloat = true;
                        break;
                    case variable:  /* 标识符为变量 */
                        getsym();
                        /* 是否是数组 */
                        if(sym == lbrack)
                        {
                            getsym();
                            if(sym == number)
                            {
                                num = table[i].size - num;
                                i -= num;
                                getsym();
                            }
                            else
                            {
                                error(26);
                            }

                            if(sym == rbrack)
                            {
                                getsym();
                            }
                            else
                                error(27);
                        }
                        gen(lod, lev-table[i].level, table[i].adr); /* 找到变量地址并将其值入栈 */
                        if(sym == plusone)
                        {
                            gen(lit, 0, 1);
                            gen(opr, 0, 2);
                            gen(sto, lev-table[i].level, table[i].adr);
                            gen(lod, lev-table[i].level, table[i].adr);
                            getsym();
                        }
                        else if(sym == minusone)
                        {
                            gen(lit, 0, 1);
                            gen(opr, 0, 3);
                            gen(sto, lev-table[i].level, table[i].adr);
                            gen(lod, lev-table[i].level, table[i].adr);
                            getsym();
                        }
                        break;
                    case function:  /* 标识符为过程 */
                        error(16);  /* 不能为过程 */
                        getsym();
                        break;
                }
            }
        }
        else
        {
            if(sym == number)       /* 因子为数 */
            {
                if (num > amax)
                {
                    error(30);      /* 数越界 */
                    num = 0;
                }
                gen(lit, 0, num);
                getsym();
            }
            else
            {
                if (sym == lparen)  /* 因子为表达式 */
                {
                    getsym();
                    memcpy(nxtlev, fsys, sizeof(bool) * symnum);
                    nxtlev[rparen] = true;
                    expression(ptx, nxtlev, lev);
                    if (sym == rparen)
                    {
                        getsym();
                    }
                    else
                    {
                        error(28);  /* 缺少右括号 */
                    }
                }
                else if (sym == tointsym)
                {
                    getsym();
                    if(sym == lparen)
                    {
                        getsym();
                    }
                    else
                    {
                        error(10);
                    }
                    if(sym == ident)            /* 因子为变量 */
                    {
                        i = position(id, *ptx); /* 查找标识符在符号表中的位置 */
                        if (i == 0)
                        {
                            error(4);           /* 标识符未声明 */
                        }
                        else
                        {
                            switch (table[i].kind)
                            {
                                case constant:  /* 标识符为常量 */
                                    error(21);
                                    getsym();
                                    break;
                                case boolean:
                                    error(21);
                                    getsym();
                                    break;
                                case floatnum:
                                    gen(lod, lev-table[i].level, table[i].adr); /* 找到变量地址并将其值入栈 */
                                    gen(opr, 0, 20);//20 is to int
                                    gen(lod, lev-table[i].level, table[i].adr);
                                    table[i].kind = variable;
                                    getsym();
                                    break;
                                case variable:  /* 标识符为变量 */
                                    error(21);
                                    getsym();
                                    break;
                                case function:  /* 标识符为过程 */
                                    error(21);
                                    getsym();
                                    break;
                            }
                        }
                    }
                    if(sym == rparen)
                    {
                        getsym();
                    }
                    else
                    {
                        error(11);
                    }
                }
                else if (sym == tofloatsym)
                {
                    getsym();
                    if(sym == lparen)
                    {
                        getsym();
                    }
                    else
                    {
                        error(10);
                    }
                    if(sym == ident)            /* 因子为变量 */
                    {
                        i = position(id, *ptx); /* 查找标识符在符号表中的位置 */
                        if (i == 0)
                        {
                            error(4);           /* 标识符未声明 */
                        }
                        else
                        {
                            switch (table[i].kind)
                            {
                                case constant:  /* 标识符为常量 */
                                    error(22);
                                    getsym();
                                    break;
                                case boolean:
                                    error(22);  /* 不能为过程 */
                                    getsym();
                                    break;
                                case floatnum:
                                    error(22);  /* 不能为过程 */
                                    getsym();
                                    break;
                                case variable:  /* 标识符为变量 */
                                    gen(lod, lev-table[i].level, table[i].adr); /* 找到变量地址并将其值入栈 */
                                    gen(opr, 0, 21);//21 is to float
                                    gen(lod, lev-table[i].level, table[i].adr);
                                    table[i].kind = floatnum;
                                    getsym();
                                    break;
                                case function:  /* 标识符为过程 */
                                    error(22);  /* 不能为过程 */
                                    getsym();
                                    break;
                            }
                        }
                    }
                    if(sym == rparen)
                    {
                        getsym();
                    }
                    else
                    {
                        error(11);
                    }
                }
                else if (sym == truesym)
                {
                    gen(lit, 0, 1);
                    getsym();
                }
                else if (sym == falsesym)
                {
                    gen(lit, 0, 0);
                    getsym();
                }
            }
        }
        memset(nxtlev, 0, sizeof(bool) * symnum);
        nxtlev[lparen] = true;
        test(fsys, nxtlev, 25);     /* 一个因子处理完毕，遇到的单词应在fsys集合中 */
                                    /* 如果不是，报错并找到下一个因子的开始，使语法分析可以继续运行下去 */
    }
}


/*  条件处理 */
void condition(int* ptx, bool* fsys, int lev)
{
    enum symbol relop;
    bool nxtlev[symnum];

    if(sym == oddsym)   /* 准备按照odd运算处理 */
    {
        getsym();
        expression(ptx, fsys, lev);
        gen(opr, 0, 6); /* 生成odd指令 */
    }
    else
    {
        /* 逻辑表达式处理 */
        memcpy(nxtlev, fsys, sizeof(bool) * symnum);
        nxtlev[eql] = true;
        nxtlev[neq] = true;
        nxtlev[lss] = true;
        nxtlev[leq] = true;
        nxtlev[gtr] = true;
        nxtlev[geq] = true;
        expression(ptx, nxtlev, lev);
        if (sym != eql && sym != neq && sym != lss && sym != leq && sym != gtr && sym != geq)
        {
            error(9);                  /* 应该为关系运算符 */
        }
        else
        {
            relop = sym;
            getsym();
            expression(ptx, fsys, lev);
            switch (relop)
            {
                case eql:
                    gen(opr, 0, 8);
                    break;
                case neq:
                    gen(opr, 0, 9);
                    break;
                case lss:
                    gen(opr, 0, 10);
                    break;
                case geq:
                    gen(opr, 0, 11);
                    break;
                case gtr:
                    gen(opr, 0, 12);
                    break;
                case leq:
                    gen(opr, 0, 13);
                    break;
                default:
                    break;
            }
        }
    }
}

/* 解释程序 */
void interpret()
{
    int p = 0;                      /* 指令指针 */
    int b = 1;                      /* 指令基址 */
    int t = 0;                      /* 栈顶指针 */
    struct instruction i;           /* 存放当前指令 */
    float fs[stacksize];            /* 小数栈 */

    printf("Start SW\n");
    fprintf(fresult,"Start SW\n");
    fs[0] = 0;                       /* s[0]不用 */
    fs[1] = 0;                       /* 主程序的三个联系单元均置为0 */
    fs[2] = 0;
    fs[3] = 0;
    do {
        i = code[p];                /* 读当前指令 */
        p = p + 1;
        switch (i.f)
        {
            case lit:               /* 将常量a的值取到栈顶 */
                t = t + 1;
                fs[t] = i.a;
                break;
            case opr:               /* 数学、逻辑运算 */
                switch ((int)i.a)
                {
                    case 0:         /* 函数调用结束后返回 */
                        t = b - 1;
                        p = (int)fs[t + 3];
                        b = (int)fs[t + 2];
                        break;
                    case 1:         /* 栈顶元素取反 */
                        fs[t] = - fs[t];
                        break;
                    case 2:         /* 次栈顶项加上栈顶项，退两个栈元素，相加值进栈 */
                        t = t - 1;
                        fs[t] = fs[t] + fs[t + 1];
                        break;
                    case 3:         /* 次栈顶项减去栈顶项 */
                        t = t - 1;
                        fs[t] = fs[t] - fs[t + 1];
                        break;
                    case 4:         /* 次栈顶项乘以栈顶项 */
                        t = t - 1;
                        fs[t] = fs[t] * fs[t + 1];
                        break;
                    case 5:         /* 次栈顶项除以栈顶项 */
                        t = t - 1;
                        fs[t] = (int)fs[t] / (int)fs[t + 1];
                        break;
                    case 6:         /* 栈顶元素的奇偶判断 */
                        fs[t] = (int)fs[t] % 2;
                        break;
                    case 7:         /* 次栈顶项取余栈顶项 */
                        t = t - 1;
                        fs[t] = (int)fs[t] % (int)fs[t + 1];
                        break;
                    case 8:         /* 次栈顶项与栈顶项是否相等 */
                        t = t - 1;
                        fs[t] = (fs[t] == fs[t + 1]);
                        break;
                    case 9:         /* 次栈顶项与栈顶项是否不等 */
                        t = t - 1;
                        fs[t] = (fs[t] != fs[t + 1]);
                        break;
                    case 10:        /* 次栈顶项是否小于栈顶项 */
                        t = t - 1;
                        fs[t] = (fs[t] < fs[t + 1]);
                        break;
                    case 11:        /* 次栈顶项是否大于等于栈顶项 */
                        t = t - 1;
                        fs[t] = (fs[t] >= fs[t + 1]);
                        break;
                    case 12:        /* 次栈顶项是否大于栈顶项 */
                        t = t - 1;
                        fs[t] = (fs[t] > fs[t + 1]);
                        break;
                    case 13:        /* 次栈顶项是否小于等于栈顶项 */
                        t = t - 1;
                        fs[t] = (fs[t] <= fs[t + 1]);
                        break;
                    case 14:        /* 栈顶值输出 */
                        printf("%d", (int)fs[t]);
                        fprintf(fresult, "%d", (int)fs[t]);
                        t = t - 1;
                        break;
                    case 15:        /* 输出换行符 */
                        printf("\n");
                        fprintf(fresult,"\n");
                        break;
                    case 16:        /* 读入一个输入置于栈顶 */
                        t = t + 1;
                        printf("?");
                        fprintf(fresult, "?");
                        int tmp;
                        scanf("%d", &tmp);
                        fs[t] = tmp;
                        fprintf(fresult, "%d\n", tmp);
                        break;
                    case 17:
                        t = t - 1;
                        break;
                    case 18:        /* 次栈顶项与栈顶项是否相等 */
                        fs[t] = (fs[t] == fs[t - 1]);
                        break;
                    case 19:
                        fs[t] = fs[t] + fs[t - 1];
                        if(fs[t]>=1)
                            fs[t] = 1;
                        else
                            fs[t] = 0;
                        break;
                    case 20:
                        fs[t] = (int)fs[t];
                        break;
                    case 21:
                        fs[t] = (float)fs[t];
                        break;
                    case 22:        /* 栈顶值输出 */
                        printf("%f", fs[t]);
                        fprintf(fresult, "%f", fs[t]);
                        t = t - 1;
                        break;
                    case 23:        /* 栈顶值输出 */
                        if(fs[t] == 1)
                        {
                            printf("true");
                            fprintf(fresult, "true");
                        }
                        else if(fs[t] == 0)
                        {
                            printf("false");
                            fprintf(fresult, "false");
                        }
                        else
                        {
                            error(31);
                        }
                        t = t - 1;
                        break;
                    case 24:        /* 读入一个输入置于栈顶 */
                        t = t + 1;
                        printf("?");
                        fprintf(fresult, "?");
                        scanf("%f", &fs[t]);
                        fprintf(fresult, "%f\n", fs[t]);
                        break;
                    case 25:        /* 读入一个输入置于栈顶 */
                        t = t + 1;
                        printf("?");
                        fprintf(fresult, "?");
                        char inputtmp[6];
                        scanf("%s", inputtmp);
                        if (strcmp(inputtmp,"true") == 0)
                        {
                            fs[t] = 1;
                            fprintf(fresult, "%s\n", "true");
                        }
                        else if(strcmp(inputtmp, "false") == 0)
                        {
                            fs[t] = 0;
                            fprintf(fresult, "%s\n", "false");
                        }
                        break;
                    case 26:
                        if(fs[t] == 0)
                        {
                            fs[t] = 1;
                        }
                        else
                        {
                            fs[t] = 0;
                        }
                        break;
                    case 27:         /* 次栈顶项除以栈顶项 */
                        t = t - 1;
                        fs[t] = fs[t] / fs[t + 1];
                        break;
                    case 28:
                        {
                            int ti;
                            for(ti = 0;ti<(int)i.l;ti++)
                            {
                                fs[t+3+(int)i.l] = fs[t];
                                t--;
                            }
                        }
                        break;
                    case 29:
                        t-=3+(int)i.l;
                        break;
                    case 30:
                        {
                            int ti;
                            for(ti = 0;ti<3+(int)i.l;ti++)
                            {
                                t++;
                                fs[t] = fs[t+(int)i.l];
                            }
                        }
                        break;
                    case 31:
                        t++;
                        fs[t] = fs[t+(int)i.l];
                        break;
                }
                break;
            case lod:               /* 取相对当前过程的数据基地址为a的内存的值到栈顶 */
                t = t + 1;
                fs[t] = fs[base(i.l, fs, b) + (int)i.a];
                break;
            case sto:               /* 栈顶的值存到相对当前过程的数据基地址为a的内存 */
                fs[base(i.l, fs, b) + (int)i.a] = fs[t];
                t = t - 1;
                break;
            case cal:               /* 调用子过程 */
                fs[t + 1] = base(i.l, fs, b); /* 将父过程基地址入栈，即建立静态链 */
                fs[t + 2] = b;       /* 将本过程基地址入栈，即建立动态链 */
                fs[t + 3] = p;       /* 将当前指令指针入栈，即保存返回地址 */
                b = t + 1;          /* 改变基地址指针值为新过程的基地址 */
                p = (int)i.a;            /* 跳转 */
                break;
            case ini:               /* 在数据栈中为被调用的过程开辟a个单元的数据区 */
                t = t + (int)i.a;
                break;
            case jmp:               /* 直接跳转 */
                p = (int)i.a;
                break;
            case jpc:               /* 条件跳转 */
                if (fs[t] == 0)
                    p = (int)i.a;
                t = t - 1;
                break;
        }
    } while (p != 0);
    printf("End sw\n");
    fprintf(fresult,"End sw\n");
}

/* 通过过程基址求上l层过程的基址 */
int base(int l, float* s, int b)
{
    int b1;
    b1 = b;
    while (l > 0)
    {
        b1 = (int)s[b1];
        l--;
    }
    return b1;
}
