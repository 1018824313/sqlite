/*
** 2014 May 31
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
******************************************************************************
**
*/
#ifndef _FTS5INT_H
#define _FTS5INT_H

#include "sqliteInt.h"
#include "fts3_tokenizer.h"


/*
** Maximum number of prefix indexes on single FTS5 table. This must be
** less than 32. If it is set to anything large than that, an #error
** directive in fts5_index.c will cause the build to fail.
*/
#define FTS5_MAX_PREFIX_INDEXES 31

#define FTS5_DEFAULT_NEARDIST 10

/**************************************************************************
** Interface to code in fts5_config.c. fts5_config.c contains contains code
** to parse the arguments passed to the CREATE VIRTUAL TABLE statement.
*/

typedef struct Fts5Config Fts5Config;

/*
** An instance of the following structure encodes all information that can
** be gleaned from the CREATE VIRTUAL TABLE statement.
*/
struct Fts5Config {
  sqlite3 *db;                    /* Database handle */
  char *zDb;                      /* Database holding FTS index (e.g. "main") */
  char *zName;                    /* Name of FTS index */
  int nCol;                       /* Number of columns */
  char **azCol;                   /* Column names */
  int nPrefix;                    /* Number of prefix indexes */
  int *aPrefix;                   /* Sizes in bytes of nPrefix prefix indexes */
  sqlite3_tokenizer *pTokenizer;  /* Tokenizer instance for this table */
};

int sqlite3Fts5ConfigParse(sqlite3*, int, const char**, Fts5Config**, char**);
void sqlite3Fts5ConfigFree(Fts5Config*);

int sqlite3Fts5ConfigDeclareVtab(Fts5Config *pConfig);

int sqlite3Fts5Tokenize(
  Fts5Config *pConfig,            /* FTS5 Configuration object */
  const char *pText, int nText,   /* Text to tokenize */
  void *pCtx,                     /* Context passed to xToken() */
  int (*xToken)(void*, const char*, int, int, int, int)    /* Callback */
);

void sqlite3Fts5Dequote(char *z);

/*
** End of interface to code in fts5_config.c.
**************************************************************************/

/**************************************************************************
** Interface to code in fts5_index.c. fts5_index.c contains contains code
** to access the data stored in the %_data table.
*/

typedef struct Fts5Index Fts5Index;
typedef struct Fts5IndexIter Fts5IndexIter;

/*
** Values used as part of the flags argument passed to IndexQuery().
*/
#define FTS5INDEX_QUERY_PREFIX 0x0001       /* Prefix query */
#define FTS5INDEX_QUERY_ASC    0x0002       /* Docs in ascending rowid order */
#define FTS5INDEX_QUERY_MATCH  0x0004       /* Use the iMatch arg to Next() */
#define FTS5INDEX_QUERY_DELETE 0x0008       /* Visit delete markers */

/*
** Create/destroy an Fts5Index object.
*/
int sqlite3Fts5IndexOpen(Fts5Config *pConfig, int bCreate, Fts5Index**, char**);
int sqlite3Fts5IndexClose(Fts5Index *p, int bDestroy);

/*
** for(
**   pIter = sqlite3Fts5IndexQuery(p, "token", 5, 0);
**   0==sqlite3Fts5IterEof(pIter);
**   sqlite3Fts5IterNext(pIter)
** ){
**   i64 iDocid = sqlite3Fts5IndexDocid(pIter);
** }
*/

/*
** Open a new iterator to iterate though all docids that match the 
** specified token or token prefix.
*/
Fts5IndexIter *sqlite3Fts5IndexQuery(
  Fts5Index *p,                   /* FTS index to query */
  const char *pToken, int nToken, /* Token (or prefix) to query for */
  int flags                       /* Mask of FTS5INDEX_QUERY_X flags */
);

/*
** Docid list iteration.
*/
int  sqlite3Fts5IterEof(Fts5IndexIter*);
void sqlite3Fts5IterNext(Fts5IndexIter*, i64 iMatch);
int sqlite3Fts5IterSeek(Fts5IndexIter*, i64 iDocid);
i64  sqlite3Fts5IterDocid(Fts5IndexIter*);

/*
** Position list iteration.
**
**   for(
**     iPos=sqlite3Fts5IterFirstPos(pIter, iCol); 
**     iPos>=0; 
**     iPos=sqlite3Fts5IterNextPos(pIter)
**   ){
**     // token appears at position iPos of column iCol of the current document
**   }
*/
int sqlite3Fts5IterFirstPos(Fts5IndexIter*, int iCol);
int sqlite3Fts5IterNextPos(Fts5IndexIter*);

/*
** Close an iterator opened by sqlite3Fts5IndexQuery().
*/
void sqlite3Fts5IterClose(Fts5IndexIter*);

/*
** Insert or remove data to or from the index. Each time a document is 
** added to or removed from the index, this function is called one or more
** times.
**
** For an insert, it must be called once for each token in the new document.
** If the operation is a delete, it must be called (at least) once for each
** unique token in the document with an iCol value less than zero. The iPos
** argument is ignored for a delete.
*/
void sqlite3Fts5IndexWrite(
  Fts5Index *p,                   /* Index to write to */
  int iCol,                       /* Column token appears in (-ve -> delete) */
  int iPos,                       /* Position of token within column */
  const char *pToken, int nToken  /* Token to add or remove to or from index */
);

/*
** Indicate that subsequent calls to sqlite3Fts5IndexWrite() pertain to
** document iDocid.
*/
void sqlite3Fts5IndexBeginWrite(
  Fts5Index *p,                   /* Index to write to */
  i64 iDocid                      /* Docid to add or remove data from */
);

/*
** Flush any data stored in the in-memory hash tables to the database.
**
** This is called whenever (a) the main transaction is committed or (b) a 
** new sub-transaction is opened.
*/
void sqlite3Fts5IndexFlush(Fts5Index *p);

int sqlite3Fts5IndexSync(Fts5Index *p);

/*
** Discard any data stored in the in-memory hash tables. Do not write it
** to the database. Additionally, assume that the contents of the %_data
** table may have changed on disk. So any in-memory caches of %_data 
** records must be invalidated.
**
** This is called (a) whenever a main or sub-transaction is rolled back, 
** and (b) whenever the read transaction is closed.
*/
int sqlite3Fts5IndexRollback(Fts5Index *p);

/*
** Retrieve and clear the current error code, respectively.
*/
int sqlite3Fts5IndexErrcode(Fts5Index*);
void sqlite3Fts5IndexReset(Fts5Index*);

/*
** Get (bSet==0) or set (bSet!=0) the "averages" record.
*/
void sqlite3Fts5IndexAverages(Fts5Index *p, int bSet, int nAvg, int *aAvg);

/*
** Functions called by the storage module as part of integrity-check.
*/
u64 sqlite3Fts5IndexCksum(Fts5Config*,i64,int,int,const char*,int);
int sqlite3Fts5IndexIntegrityCheck(Fts5Index*, u64 cksum);

/* Called during startup to register a UDF with SQLite */
int sqlite3Fts5IndexInit(sqlite3*);

void sqlite3Fts5IndexPgsz(Fts5Index *p, int pgsz);

/*
** End of interface to code in fts5_index.c.
**************************************************************************/

/**************************************************************************
** Interface to code in fts5_storage.c. fts5_storage.c contains contains 
** code to access the data stored in the %_content and %_docsize tables.
*/
typedef struct Fts5Storage Fts5Storage;

int sqlite3Fts5StorageOpen(Fts5Config*, Fts5Index*, int, Fts5Storage**, char**);
int sqlite3Fts5StorageClose(Fts5Storage *p, int bDestroy);

int sqlite3Fts5DropTable(Fts5Config*, const char *zPost);
int sqlite3Fts5CreateTable(Fts5Config*, const char*, const char*, char **pzErr);

int sqlite3Fts5StorageDelete(Fts5Storage *p, i64);
int sqlite3Fts5StorageInsert(Fts5Storage *p, sqlite3_value **apVal, int, i64*);

int sqlite3Fts5StorageIntegrity(Fts5Storage *p);

#define FTS5_STMT_SCAN_ASC  0     /* SELECT rowid, * FROM ... ORDER BY 1 ASC */
#define FTS5_STMT_SCAN_DESC 1     /* SELECT rowid, * FROM ... ORDER BY 1 DESC */
#define FTS5_STMT_LOOKUP    2     /* SELECT rowid, * FROM ... WHERE rowid=? */

int sqlite3Fts5StorageStmt(Fts5Storage *p, int eStmt, sqlite3_stmt **);
void sqlite3Fts5StorageStmtRelease(Fts5Storage *p, int eStmt, sqlite3_stmt*);



/*
** End of interface to code in fts5_storage.c.
**************************************************************************/


/**************************************************************************
** Interface to code in fts5_expr.c. 
*/
typedef struct Fts5Expr Fts5Expr;
typedef struct Fts5Parse Fts5Parse;
typedef struct Fts5Token Fts5Token;
typedef struct Fts5ExprPhrase Fts5ExprPhrase;
typedef struct Fts5ExprNearset Fts5ExprNearset;

struct Fts5Token {
  const char *p;                  /* Token text (not NULL terminated) */
  int n;                          /* Size of buffer p in bytes */
};

int sqlite3Fts5ExprNew(
  Fts5Config *pConfig, 
  Fts5Index *pIdx, 
  const char *zExpr,
  Fts5Expr **ppNew, 
  char **pzErr
);

int sqlite3Fts5ExprFirst(Fts5Expr *p);
int sqlite3Fts5ExprNext(Fts5Expr *p);
int sqlite3Fts5ExprEof(Fts5Expr *p);
i64 sqlite3Fts5ExprRowid(Fts5Expr *p);

void sqlite3Fts5ExprFree(Fts5Expr *p);

// int sqlite3Fts5IterFirstPos(Fts5Expr*, int iCol, int *piPos);
// int sqlite3Fts5IterNextPos(Fts5Expr*, int *piPos);

/* Called during startup to register a UDF with SQLite */
int sqlite3Fts5ExprInit(sqlite3*);

/*******************************************
** The fts5_expr.c API above this point is used by the other hand-written
** C code in this module. The interfaces below this point are called by
** the parser code in fts5parse.y.  */

void sqlite3Fts5ParseError(Fts5Parse *pParse, const char *zFmt, ...);

Fts5Expr *sqlite3Fts5ParseExpr(
  Fts5Parse *pParse, 
  int eType, 
  Fts5Expr *pLeft, 
  Fts5Expr *pRight, 
  Fts5ExprNearset *pNear
);

Fts5ExprPhrase *sqlite3Fts5ParseTerm(
  Fts5Parse *pParse, 
  Fts5ExprPhrase *pPhrase, 
  Fts5Token *pToken,
  int bPrefix
);

Fts5ExprNearset *sqlite3Fts5ParseNearset(
  Fts5Parse*, 
  Fts5ExprNearset*,
  Fts5ExprPhrase* 
);

void sqlite3Fts5ParsePhraseFree(Fts5ExprPhrase*);
void sqlite3Fts5ParseNearsetFree(Fts5ExprNearset*);

void sqlite3Fts5ParseSetDistance(Fts5Parse*, Fts5ExprNearset*, Fts5Token*);
void sqlite3Fts5ParseSetColumn(Fts5Parse*, Fts5ExprNearset*, Fts5Token*);
void sqlite3Fts5ParseFinished(Fts5Parse *pParse, Fts5Expr *p);
void sqlite3Fts5ParseNear(Fts5Parse *pParse, Fts5Token*);


/*
** End of interface to code in fts5_expr.c.
**************************************************************************/

#endif