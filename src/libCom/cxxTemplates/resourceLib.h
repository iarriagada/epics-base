/*
 *      $Id$
 *
 *      General hash table templates for fast indexing of resources
 *      of any base resource type and any resource identifier type. Fast 
 *      indexing is implemented with a hash lookup. The identifier type 
 *      implements the hash algorithm (or derives from one of the supplied 
 *      identifier types which provide a hashing routine). 
 *
 *      Unsigned integer and string identifier classes are supplied here.
 *
 *      Author  Jeffrey O. Hill 
 *              (string hash alg by Marty Kraimer and Peter K. Pearson)
 *
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *
 *  NOTES:
 *  .01 Storage for identifier must persist until an item is deleted
 *  .02 class T must derive from class ID and tsSLNode<T>
 *  
 */

//
// To Do:
// 1) Add a member function that sets the size of the hash table.
//    Dont allocate storage for the hash table until they insert
//    the first item.
//

#ifndef INCresourceLibh
#define INCresourceLibh 

#include <new>
#include <typeinfo>

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#ifndef assert // allow use of epicsAssert.h
#include <assert.h>
#endif

#include "tsSLList.h"
#include "shareLib.h"
#include "locationException.h"
typedef size_t resTableIndex;

template <class T, class ID> class resTableIter;

//
// class resTable <T, ID>
//
// This class stores resource entries of type T which can be efficiently 
// located with a hash key of type ID.
//
// NOTES: 
// 1)   class T must derive from class ID and also from class tsSLNode<T>
//
// 2)   If the "resTable::show (unsigned level)" member function is called then 
//      class T must also implement a "show (unsigned level)" member function which
//      dumps increasing diagnostics information with increasing "level" to
//      standard out.
//
// 3)   Classes of type ID must implement the following member functions:
//
//          // equivalence test
//          bool operator == (const ID &);
//
//          // ID to hash index convert (see examples below)
//          resTableIndex hash (unsigned nBitsHashIndex) const; 
//
// 4)   Storage for identifier of type ID must persist until the item of type 
//      T is deleted from the resTable
//
template <class T, class ID>
class resTable {
public:
    resTable ();
    virtual ~resTable();
    // Call " void T::show (unsigned level)" for each entry
    void show (unsigned level) const;
    void verify () const;
    int add ( T & res ); // returns -1 (id exists in table), 0 (success)
    T * remove (const ID &idIn); // remove entry
    T * lookup (const ID &idIn) const; // locate entry
    // Call (pT->*pCB) () for each entry
    void traverse ( void (T::*pCB)() );
    void traverseConst ( void (T::*pCB)() const ) const;
    unsigned numEntriesInstalled () const;
    void setTableSize ( const unsigned newTableSize );
private:
    tsSLList < T > * pTable;
    unsigned nextSplitIndex;
    unsigned hashIxMask;
    unsigned hashIxSplitMask;
    unsigned nBitsHashIxSplitMask;
    unsigned logBaseTwoTableSize;
    unsigned nInUse;
    resTableIndex hash ( const ID & idIn ) const;
    T * find ( tsSLList<T> & list, const ID & idIn ) const;
    void splitBucket ();
    unsigned tableSize () const;
    bool setTableSizePrivate ( unsigned logBaseTwoTableSize );
    resTable ( const resTable & );
    resTable & operator = ( const resTable & );
    static unsigned resTableBitMask ( const unsigned nBits );
    friend class resTableIter<T,ID>;
};

//
// class resTableIter
//
// an iterator for the resource table class
//
template <class T, class ID>
class resTableIter {
public:
    resTableIter (const resTable<T,ID> &tableIn);
    T * next ();
    T * operator () ();
private:
    tsSLIter<T> iter;
    unsigned index;
    const resTable<T,ID> &table;
};

//
// Some ID classes that work with the above template
//

//
// class intId
//
// signed or unsigned integer identifier (class T must be
// a signed or unsigned integer type)
//
// this class works as type ID in resTable <class T, class ID>
//
// 1<<MIN_INDEX_WIDTH specifies the minimum number of
// elements in the hash table within resTable <class T, class ID>.
// Set this parameter to zero if unsure of the correct minimum 
// hash table size.
//
// MAX_ID_WIDTH specifies the maximum number of ls bits in an 
// integer identifier which might be set at any time. 
//
// MIN_INDEX_WIDTH and MAX_ID_WIDTH are specified here at
// compile time so that the hash index can be produced 
// efficiently. Hash indexes are produced more efficiently 
// when (MAX_ID_WIDTH - MIN_INDEX_WIDTH) is minimized.
//
template <class T, unsigned MIN_INDEX_WIDTH=4u, 
    unsigned MAX_ID_WIDTH = sizeof(T)*CHAR_BIT>
class intId {
public:
    intId (const T &idIn);
    bool operator == (const intId &idIn) const;
    resTableIndex hash () const;
    const T getId() const;
protected:
    T id;
};

//
// class chronIntIdResTable <ITEM>
//
// a specialized resTable which uses unsigned integer keys which are
// allocated in chronological sequence
// 
// NOTE: ITEM must public inherit from chronIntIdRes <ITEM>
//
class chronIntId : public intId<unsigned, 8u, sizeof(unsigned)*CHAR_BIT> 
{
public:
    chronIntId ( const unsigned &idIn );
};

template <class ITEM>
class chronIntIdResTable : public resTable<ITEM, chronIntId> {
public:
    chronIntIdResTable ();
    virtual ~chronIntIdResTable ();
    void add ( ITEM & item );
private:
    unsigned allocId;
	chronIntIdResTable ( const chronIntIdResTable & );
	chronIntIdResTable & operator = ( const chronIntIdResTable & );
};

//
// class chronIntIdRes<ITEM>
//
// resource with unsigned chronological identifier
//
template <class ITEM>
class chronIntIdRes : public chronIntId, public tsSLNode<ITEM> {
    friend class chronIntIdResTable<ITEM>;
public:
    chronIntIdRes ();
private:
    void setId (unsigned newId);
	chronIntIdRes (const chronIntIdRes & );
};

//
// class stringId
//
// character string identifier
//
class epicsShareClass stringId {
public:
    enum allocationType {copyString, refString};
    stringId (const char * idIn, allocationType typeIn=copyString);
    virtual ~stringId();
    resTableIndex hash () const;
    bool operator == (const stringId &idIn) const;
    const char * resourceName() const; // return the pointer to the string
    void show (unsigned level) const;
private:
    stringId & operator = ( const stringId & );
    stringId ( const stringId &);
    const char * pStr;
    const allocationType allocType;
    static const unsigned char fastHashPermutedIndexSpace[256];
};

/////////////////////////////////////////////////
// resTable<class T, class ID> member functions
/////////////////////////////////////////////////

// 
// resTable::resTable ()
//
template <class T, class ID>
inline resTable<T,ID>::resTable () :
	pTable ( 0 ), nextSplitIndex ( 0 ), hashIxMask ( 0 ), 
        hashIxSplitMask ( 0 ), nBitsHashIxSplitMask ( 0 ),
        logBaseTwoTableSize ( 0 ), nInUse ( 0 ) {}

template <class T, class ID>
inline unsigned resTable<T,ID>::resTableBitMask ( const unsigned nBits )
{
    return ( 1 << nBits ) - 1;
}

//
// resTable::remove ()
//
// remove a res from the resTable
//
template <class T, class ID>
T * resTable<T,ID>::remove ( const ID &idIn ) // X aCC 361
{
    if ( this->pTable ) {
        // search list for idIn and remove the first match
        tsSLList<T> & list = this->pTable [ this->hash(idIn) ];
        tsSLIter <T> pItem = list.firstIter ();
        T *pPrev = 0;
        while ( pItem.valid () ) {
            const ID & id = *pItem;
            if ( id == idIn ) {
                if ( pPrev ) {
                    list.remove ( *pPrev );
                }
                else {
                    list.get ();
                }
                this->nInUse--;
                break;
            }
            pPrev = pItem.pointer ();
            pItem++;
        }
        return pItem.pointer ();
    }
    else {
        return 0;
    }
}

//
// resTable::lookup ()
//
template <class T, class ID>
inline T * resTable<T,ID>::lookup ( const ID &idIn ) const // X aCC 361
{
    if ( this->pTable ) {
        tsSLList<T> & list = this->pTable [ this->hash ( idIn ) ];
        return this->find ( list, idIn );
    }
    else {
        return 0;
    }
}

//
// resTable::hash ()
//
template <class T, class ID>
inline resTableIndex resTable<T,ID>::hash ( const ID & idIn ) const
{
    resTableIndex h = idIn.hash ();
    resTableIndex h0 = h & this->hashIxMask;
    if ( h0 >= this->nextSplitIndex ) {
        return h0;
    }
    return h & this->hashIxSplitMask;
}

//
// resTable<T,ID>::show
//
template <class T, class ID>
void resTable<T,ID>::show ( unsigned level ) const
{    
    const unsigned N = this->tableSize ();

    printf ( "%u bucket hash table with %u items of type %s installed\n", 
        N, this->nInUse, typeid(T).name() );

    if ( N ) {
        tsSLList<T> * pList = this->pTable;
        while ( pList < & this->pTable[N] ) {
            tsSLIter<T> pItem = pList->firstIter ();
            while ( pItem.valid () ) {
                tsSLIter<T> pNext = pItem;
                pNext++;
                pItem.pointer()->show ( level );
                pItem = pNext;
            }
            pList++;
        }
    }

    if ( level >=1u ) {
        double X = 0.0;
        double XX = 0.0;
        unsigned maxEntries = 0u;
        for ( unsigned i = 0u; i < N; i++ ) {
            tsSLIter<T> pItem = this->pTable[i].firstIter ();
            unsigned count = 0;
            while ( pItem.valid () ) {
                if ( level >= 3u ) {
                    pItem->show ( level );
                }
                count++;
                pItem++;
            }
            if ( count > 0u ) {
                X += count;
                XX += count * count;
                if ( count > maxEntries ) {
                    maxEntries = count;
                }
            }
        }
     
        double mean = X / N;
        double stdDev = sqrt( XX / N - mean * mean );
        printf ( 
    "entries per bucket: mean = %f std dev = %f max = %d\n",
            mean, stdDev, maxEntries );
        if ( X != this->nInUse ) {
            printf ("this->nInUse didnt match items counted which was %f????\n", X );
        }
    }
}

// self test
template <class T, class ID>
void resTable<T,ID>::verify () const
{
    const unsigned N = this->tableSize ();

    if ( this->pTable ) {
        assert ( this->nextSplitIndex <= this->hashIxMask + 1 );
        assert ( this->hashIxMask );
        assert ( this->hashIxMask == this->hashIxSplitMask >> 1 );
        assert ( this->hashIxSplitMask );
        assert ( this->nBitsHashIxSplitMask );
        assert ( resTableBitMask ( this->nBitsHashIxSplitMask ) 
                        == this->hashIxSplitMask );
        assert ( this->logBaseTwoTableSize );
        assert ( this->nBitsHashIxSplitMask <= this->logBaseTwoTableSize );
    }
    else {
        assert ( this->nextSplitIndex == 0 );
        assert ( this->hashIxMask == 0 );
        assert ( this->hashIxSplitMask == 0 );
        assert ( this->nBitsHashIxSplitMask == 0 );
        assert ( this->logBaseTwoTableSize == 0 );
    }

    unsigned total = 0u;
    for ( unsigned i = 0u; i < N; i++ ) {
        tsSLIter<T> pItem = this->pTable[i].firstIter ();
        unsigned count = 0;
        while ( pItem.valid () ) {
            resTableIndex index = this->hash ( *pItem );
            assert ( index == i );
            count++;
            pItem++;
        }
        total += count;
    }
    assert ( total == this->nInUse );
}


//
// resTable<T,ID>::traverse
//
template <class T, class ID>
void resTable<T,ID>::traverse ( void (T::*pCB)() ) 
{
    const unsigned N = this->tableSize ();
    for ( unsigned i = 0u; i < N; i++ ) {
        tsSLIter<T> pItem = this->pTable[i].firstIter ();
        while ( pItem.valid () ) {
            tsSLIter<T> pNext = pItem;
            pNext++;
            ( pItem.pointer ()->*pCB ) ();
            pItem = pNext;
        }
    }
}

//
// resTable<T,ID>::traverseConst
//
template <class T, class ID>
void resTable<T,ID>::traverseConst ( void (T::*pCB)() const ) const
{
    const unsigned N = this->tableSize ();
    for ( unsigned i = 0u; i < N; i++ ) {
        const tsSLList < T > & table = this->pTable[i];
        tsSLIterConst<T> pItem = table.firstIter ();
        while ( pItem.valid () ) {
            tsSLIterConst<T> pNext = pItem;
            pNext++;
            ( pItem.pointer ()->*pCB ) ();
            pItem = pNext;
        }
    }
}

template <class T, class ID>
inline unsigned resTable<T,ID>::numEntriesInstalled () const
{
    return this->nInUse;
}

template <class T, class ID>
inline unsigned resTable<T,ID>::tableSize () const // X aCC 361
{
    if ( this->pTable ) {
        return ( this->hashIxMask + 1 ) + this->nextSplitIndex;
    }
    else {
        return 0;
    }
}

// it will be more efficent to call this once prior to installing
// the first entry
template <class T, class ID>
void resTable<T,ID>::setTableSize ( const unsigned newTableSize )
{
    if ( newTableSize == 0u ) {
        return;
    }

	//
	// count the number of bits in newTableSize and round up 
    // to the next power of two
	//
    unsigned newMask = newTableSize - 1;
	unsigned nbits;
	for ( nbits = 0; nbits < sizeof (newTableSize) * CHAR_BIT; nbits++ ) {
		unsigned nBitsMask = resTableBitMask ( nbits ); 
		if ( ( newMask & ~nBitsMask ) == 0){
			break;
		}
	}
    setTableSizePrivate ( nbits );
}

template <class T, class ID>
bool resTable<T,ID>::setTableSizePrivate ( unsigned logBaseTwoTableSizeIn )
{
    // dont shrink
    if ( this->logBaseTwoTableSize >= logBaseTwoTableSizeIn ) {
        return true;
    }

    // dont allow ridiculously small tables
    if ( logBaseTwoTableSizeIn < 4 ) {
        logBaseTwoTableSizeIn = 4;
    }

    const unsigned newTableSize = 1 << logBaseTwoTableSizeIn;
#   if ! defined (__GNUC__) || __GNUC__ > 2 || ( __GNUC__ == 2 && __GNUC_MINOR__ >= 92 )
        const unsigned oldTableSize = this->pTable ? 1 << this->logBaseTwoTableSize : 0;
#   endif
    const unsigned oldTableOccupiedSize = this->tableSize ();

    tsSLList<T> * pNewTable;
    try {
        pNewTable = ( tsSLList<T> * ) 
            ::operator new ( newTableSize * sizeof ( tsSLList<T> ) );
    }
    catch ( ... ){
        if ( ! this->pTable ) {
		    throw;
        }
        return false;
    }

    // run the constructors using placement new
	unsigned i;
    for ( i = 0u; i < oldTableOccupiedSize; i++ ) {
        new ( &pNewTable[i] ) tsSLList<T> ( this->pTable[i] );
    }
    for ( i = oldTableOccupiedSize; i < newTableSize; i++ ) {
        new ( &pNewTable[i] ) tsSLList<T>;
    }
    // Run the destructors explicitly. Currently this destructor is a noop.
    // The Tornado II compiler and RedHat 6.2 will not compile ~tsSLList<T>() but 
    // since its a NOOP we can find an ugly workaround
#   if ! defined (__GNUC__) || __GNUC__ > 2 || ( __GNUC__ == 2 && __GNUC_MINOR__ >= 92 )
        for ( i = 0; i < oldTableSize; i++ ) {
            this->pTable[i].~tsSLList<T>();
        }
#   endif

    if ( ! this->pTable ) {
        this->hashIxSplitMask = resTableBitMask ( logBaseTwoTableSizeIn );
        this->nBitsHashIxSplitMask = logBaseTwoTableSizeIn;
        this->hashIxMask = this->hashIxSplitMask >> 1;
        this->nextSplitIndex = 0;
    }

    operator delete ( this->pTable );
    this->pTable = pNewTable;
    this->logBaseTwoTableSize = logBaseTwoTableSizeIn;

    return true;
}

template <class T, class ID>
void resTable<T,ID>::splitBucket ()
{
    // double the hash table when necessary
    // (this results in only a memcpy overhead, but 
    // no hashing or entry redistribution)
    if ( this->nextSplitIndex > this->hashIxMask ) {
        bool success = this->setTableSizePrivate ( this->nBitsHashIxSplitMask + 1 );
        if ( ! success ) {
            return;
        }
        this->nBitsHashIxSplitMask += 1;
        this->hashIxSplitMask = resTableBitMask ( this->nBitsHashIxSplitMask );
        this->hashIxMask = this->hashIxSplitMask >> 1;
        this->nextSplitIndex = 0;
    }

    // rehash only the items in the split bucket
    tsSLList<T> tmp ( this->pTable[ this->nextSplitIndex ] );
    this->nextSplitIndex++;
    T *pItem = tmp.get();
    while ( pItem ) {
        resTableIndex index = this->hash ( *pItem );
        this->pTable[index].add ( *pItem );
        pItem = tmp.get();
    }
}

//
// add a res to the resTable
//
template <class T, class ID>
int resTable<T,ID>::add ( T &res )
{
    if ( ! this->pTable ) {
        this->setTableSizePrivate ( 10 );
    }
    else if ( this->nInUse >= this->tableSize() ) {
        this->splitBucket ();
        tsSLList<T> &list = this->pTable[this->hash(res)];
        if ( this->find ( list, res ) != 0 ) {
            return -1;
        }
    }
    tsSLList<T> &list = this->pTable[this->hash(res)];
    if ( this->find ( list, res ) != 0 ) {
        return -1;
    }
    list.add ( res );
    this->nInUse++;
    return 0;
}

//
// find
// searches from where the iterator points to the
// end of the list for idIn
//
// iterator points to the item found upon return
// (or NULL if nothing matching was found)
//
template <class T, class ID>
T *resTable<T,ID>::find ( tsSLList<T> &list, const ID &idIn ) const
{
    tsSLIter <T> pItem = list.firstIter ();
    while ( pItem.valid () ) {
        const ID &id = *pItem;
        if ( id == idIn ) {
            break;
        }
        pItem++;
    }
    return pItem.pointer ();
}

//
// ~resTable<T,ID>::resTable()
//
template <class T, class ID>
resTable<T,ID>::~resTable() 
{
    operator delete ( this->pTable );
}

//
// resTable<T,ID>::resTable ( const resTable & )
// private - not to be used - implemented to eliminate warnings
//
template <class T, class ID>
inline resTable<T,ID>::resTable ( const resTable & )
{
}

//
// resTable<T,ID>::resTable & operator = ( const resTable & )
// private - not to be used - implemented to eliminate warnings
//
template <class T, class ID>
inline resTable<T,ID> & resTable<T,ID>::operator = ( const resTable & )
{
    return *this;
}

//////////////////////////////////////////////
// resTableIter<T,ID> member functions
//////////////////////////////////////////////

//
// resTableIter<T,ID>::resTableIter ()
//
template <class T, class ID>
inline resTableIter<T,ID>::resTableIter (const resTable<T,ID> &tableIn) : 
    iter ( tableIn.pTable ? tableIn.pTable[0].firstIter () : 
            tsSLList<T>::invalidIter() ), 
    index (1), table ( tableIn ) {} 

//
// resTableIter<T,ID>::next ()
//
template <class T, class ID> 
T * resTableIter<T,ID>::next ()
{
    if ( this->iter.valid () ) {
        T *p = this->iter.pointer ();
        this->iter++;
        return p;
    }
    unsigned N = this->table.tableSize();
    while ( true ) {
        if ( this->index >= N ) {
            return 0;
        }
        this->iter = tsSLIter<T> ( this->table.pTable[this->index++].firstIter () );
        if ( this->iter.valid () ) {
            T *p = this->iter.pointer ();
            this->iter++;
            return p;
        }
    }
}

//
// resTableIter<T,ID>::operator () ()
//
template <class T, class ID>
inline T * resTableIter<T,ID>::operator () ()
{
    return this->next ();
}

//////////////////////////////////////////////
// chronIntIdResTable<ITEM> member functions
//////////////////////////////////////////////
inline chronIntId::chronIntId ( const unsigned &idIn ) : 
    intId<unsigned, 8u, sizeof(unsigned)*CHAR_BIT> ( idIn ) {}

//
// chronIntIdResTable<ITEM>::chronIntIdResTable()
//
template <class ITEM>
inline chronIntIdResTable<ITEM>::chronIntIdResTable () : 
    resTable<ITEM, chronIntId> (), allocId(1u) {}

template <class ITEM>
inline chronIntIdResTable<ITEM>::chronIntIdResTable ( const chronIntIdResTable<ITEM> & ) :
	resTable<ITEM, chronIntId> (), allocId(1u) {}

template <class ITEM>
inline chronIntIdResTable<ITEM> & chronIntIdResTable<ITEM>::
	operator = ( const chronIntIdResTable<ITEM> & ) 
{
	return *this;
}

//
// chronIntIdResTable<ITEM>::~chronIntIdResTable()
// (not inline because it is virtual)
//
template <class ITEM>
chronIntIdResTable<ITEM>::~chronIntIdResTable() {}

//
// chronIntIdResTable<ITEM>::add()
//
// NOTE: This detects (and avoids) the case where 
// the PV id wraps around and we attempt to have two 
// resources with the same id.
//
template <class ITEM>
inline void chronIntIdResTable<ITEM>::add (ITEM &item)
{
    int status;
    do {
        item.chronIntIdRes<ITEM>::setId (allocId++);
        status = this->resTable<ITEM,chronIntId>::add (item);
    }
    while (status);
}

/////////////////////////////////////////////////
// chronIntIdRes<ITEM> member functions
/////////////////////////////////////////////////

//
// chronIntIdRes<ITEM>::chronIntIdRes
//
template <class ITEM>
inline chronIntIdRes<ITEM>::chronIntIdRes () : chronIntId (UINT_MAX) {}

//
// id<ITEM>::setId ()
//
// workaround for bug in DEC compiler
//
template <class ITEM>
inline void chronIntIdRes<ITEM>::setId (unsigned newId) 
{
    this->id = newId;
}

/////////////////////////////////////////////////
// intId member functions
/////////////////////////////////////////////////

//
// intId::intId
//
// (if this is inline SUN PRO botches the template instantiation)
template <class T, unsigned MIN_INDEX_WIDTH, unsigned MAX_ID_WIDTH>
intId<T, MIN_INDEX_WIDTH, MAX_ID_WIDTH>::intId (const T &idIn) 
    : id (idIn) {}

//
// intId::operator == ()
//
template <class T, unsigned MIN_INDEX_WIDTH, unsigned MAX_ID_WIDTH>
inline bool intId<T, MIN_INDEX_WIDTH, MAX_ID_WIDTH>::operator == // X aCC 361
        (const intId<T, MIN_INDEX_WIDTH, MAX_ID_WIDTH> &idIn) const
{
    return this->id == idIn.id;
}

//
// intId::getId ()
//
template <class T, unsigned MIN_INDEX_WIDTH, unsigned MAX_ID_WIDTH>
inline const T intId<T, MIN_INDEX_WIDTH, MAX_ID_WIDTH>::getId () const // X aCC 361
{
    return this->id;
}

//
// integerHash()
//
// converts any integer into a hash table index
//
template < class T >
inline resTableIndex integerHash ( unsigned MIN_INDEX_WIDTH,
                                  unsigned MAX_ID_WIDTH, const T &id )
{
    resTableIndex hashid = static_cast <resTableIndex> ( id );

    //
    // the intent here is to gurantee that all components of the 
    // integer contribute even if the resTableIndex returned might
    // index a small table.
    //
    // On most compilers the optimizer will unroll this loop so this
    // is actually a very small inline function
    //
    // Experiments using the microsoft compiler show that this isnt 
    // slower than switching on the architecture size and unrolling the
    // loop explicitly (that solution has resulted in portability
    // problems in the past).
    //
    unsigned width = MAX_ID_WIDTH;
    do {
        width >>= 1u;
        hashid ^= hashid>>width;
    } while (width>MIN_INDEX_WIDTH);

    //
    // the result here is always masked to the
    // proper size after it is returned to the "resTable" class
    //
    return hashid;
}


//
// intId::hash()
//
template <class T, unsigned MIN_INDEX_WIDTH, unsigned MAX_ID_WIDTH>
inline resTableIndex intId<T, MIN_INDEX_WIDTH, MAX_ID_WIDTH>::hash () const // X aCC 361
{
    return integerHash ( MIN_INDEX_WIDTH, MAX_ID_WIDTH, this->id );
}

////////////////////////////////////////////////////
// stringId member functions
////////////////////////////////////////////////////

//
// stringId::operator == ()
//
inline bool stringId::operator == 
        (const stringId &idIn) const
{
    if (this->pStr!=NULL && idIn.pStr!=NULL) {
        return strcmp(this->pStr,idIn.pStr)==0;
    }
    return false; // not equal
}

//
// stringId::resourceName ()
//
inline const char * stringId::resourceName () const
{
    return this->pStr;
}

#ifdef instantiateRecourceLib

//
// stringId::stringId()
//
stringId::stringId (const char * idIn, allocationType typeIn) :
    allocType (typeIn)
{
    if (typeIn==copyString) {
        unsigned nChars = strlen (idIn) + 1u;
        this->pStr = new char [nChars];
        memcpy ( (void *) this->pStr, idIn, nChars );
    }
    else {
        this->pStr = idIn;
    }
}

//
// stringId::show ()
//
void stringId::show (unsigned level) const
{
    if (level>2u) {
        printf ("resource id = %s\n", this->pStr);
    }
}

//
// stringId::~stringId()
//
//
// this needs to be instantiated only once (normally in libCom)
//
stringId::~stringId()
{
    if (this->allocType==copyString) {
        if (this->pStr!=NULL) {
            //
            // the microsoft and solaris compilers will 
            // not allow a pointer to "const char"
            // to be deleted 
            //
            // the HP-UX compiler gives us a warning on
            // each cast away of const, but in this case
            // it cant be avoided. 
            //
            // The DEC compiler complains that const isnt 
            // really significant in a cast if it is present.
            //
            // I hope that deleting a pointer to "char"
            // is the same as deleting a pointer to 
            // "const char" on all compilers
            //
            delete [] const_cast<char *>(this->pStr);
        }
    }
}

//
// stringId::hash()
//
// This is a modification of the algorithm described in 
// "Fast Hashing of Variable Length Text Strings", Peter K. Pearson, 
// Communications of the ACM, June 1990. The initial modifications 
// were designed by Marty Kraimer. Some additional minor optimizations
// by Jeff Hill.
//
resTableIndex stringId::hash() const
{
    const unsigned stringIdMinIndexWidth = CHAR_BIT;
    const unsigned stringIdMaxIndexWidth = sizeof ( unsigned );
    const unsigned char *pUStr = 
        reinterpret_cast < const unsigned char * > ( this->pStr );

    if ( ! pUStr ) {
        return 0u;
    }

    unsigned h0 = 0;
    unsigned h1 = 0;
    unsigned h2 = 0;
    unsigned h3 = 0;
    unsigned c;

    while ( true ) {

        c = * ( pUStr++ );
        if ( c == 0 ) {
            break;
        }
        h0 = fastHashPermutedIndexSpace [ h0 ^ c ];

        c = * ( pUStr++ );
        if ( c == 0 ) {
            break;
        }
        h1 = fastHashPermutedIndexSpace [ h1 ^ c ];

        c = * ( pUStr++ );
        if ( c == 0 ) {
            break;
        }
        h2 = fastHashPermutedIndexSpace [ h2 ^ c ];

        c = * ( pUStr++ );
        if ( c == 0 ) {
            break;
        }
        h3 = fastHashPermutedIndexSpace [ h3 ^ c ];
    }

    h0 = ( h3 << 24 ) | ( h2 << 16 ) | ( h1 << 8 ) | h0;

    return integerHash ( stringIdMinIndexWidth, stringIdMaxIndexWidth, h0 );
}

//
// This is a modification of the algorithm described in
// "Fast Hashing of Variable Length Text Strings", Peter K. 
// Pearson, Communications of the ACM, June 1990
// The modifications were designed by Marty Kraimer
//
const unsigned char stringId::fastHashPermutedIndexSpace[256] = {
 39,159,180,252, 71,  6, 13,164,232, 35,226,155, 98,120,154, 69,
157, 24,137, 29,147, 78,121, 85,112,  8,248,130, 55,117,190,160,
176,131,228, 64,211,106, 38, 27,140, 30, 88,210,227,104, 84, 77,
 75,107,169,138,195,184, 70, 90, 61,166,  7,244,165,108,219, 51,
  9,139,209, 40, 31,202, 58,179,116, 33,207,146, 76, 60,242,124,
254,197, 80,167,153,145,129,233,132, 48,246, 86,156,177, 36,187,
 45,  1, 96, 18, 19, 62,185,234, 99, 16,218, 95,128,224,123,253,
 42,109,  4,247, 72,  5,151,136,  0,152,148,127,204,133, 17, 14,
182,217, 54,199,119,174, 82, 57,215, 41,114,208,206,110,239, 23,
189, 15,  3, 22,188, 79,113,172, 28,  2,222, 21,251,225,237,105,
102, 32, 56,181,126, 83,230, 53,158, 52, 59,213,118,100, 67,142,
220,170,144,115,205, 26,125,168,249, 66,175, 97,255, 92,229, 91,
214,236,178,243, 46, 44,201,250,135,186,150,221,163,216,162, 43,
 11,101, 34, 37,194, 25, 50, 12, 87,198,173,240,193,171,143,231,
111,141,191,103, 74,245,223, 20,161,235,122, 63, 89,149, 73,238,
134, 68, 93,183,241, 81,196, 49,192, 65,212, 94,203, 10,200, 47
};

#endif // if instantiateRecourceLib is defined

#endif // INCresourceLibh

