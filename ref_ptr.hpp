//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2005-2007
//   Nicholas Kidd  <kidd@cs.wisc.edu>
//   Thomas Reps    <reps@cs.wisc.edu>
//   David Melski   <melski@grammatech.com>
//   Akash Lal      <akash@cs.wisc.edu>
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//   1. Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//
//   2. Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in
//      the documentation and/or other materials provided with the
//      distribution.
//
//   3. Neither the name of the University of Wisconsin, Madison,
//      nor GrammaTech, Inc., nor the names of the copyright holders
//      and contributors may be used to endorse or promote
//      products derived from this software without specific prior
//      written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//////////////////////////////////////////////////////////////////////////////

#ifndef REF_PTR_H_
#define REF_PTR_H_ 1

#include <cstdio>
#include <cassert>
#include <limits.h>
#include <iostream>
#include <vector>

/* A reference counting pointer class
 *
 * This class is *NOT* thread safe.
 *
 * The templated class must have a member variable named count
 * that can be accessed from ref_ptr and modified by ref_ptr.
 * the count variable should have operator++(),operator--(), and
 * operator==( int ) defined.  As a note, this class was designed
 * with count being an unsigned integer, although there may be some
 * advantages to making it be of type RefCounter, as implemented in
 * this file. (E.g., it handles the warnings in the next paragraph.)
 *
 * Count should be initialized to 0 for proper reference
 * couting to work.  If it is desirable for the pointer/object
 * to never be deleted, initialize count >= 1.  WARNING: don't forget
 * to initialize count to 0 in the copy constructor!  (And don't
 * forget to have a copy constructor.) The assignment operator should
 * probably not change the value either. The count field should be
 * declared as mutable, since it is meta-object information. The
 * RefCounter implementation uses a mutable field, but even when using
 * RefCounter for count it is good practice to declare count mutable
 * (for documentation purposes).
 *
 * ref_ptr provides a templated function operator<< that allows 
 * the use of std::ostream << with ref_ptr<T> objects.  To take
 * advantage of this, the templated class must provide a method
 * with signature:
 *
 *      std::ostream& print( std::ostream& )
 *
 */

/* DelayDeletes (formerly GRAMMATECH_NONRECURSIVE_DELETE).
 * When the reference count of a "deep" recursive data structure goes
 * to zero, the chain of delete's can cause a stack
 * overflow. Consider a linked list implemented with ref_ptr's
 * (indentation represents function calls):
 * 
 *  - when the reference count of the head goes to 0, the list
 *    destructor is called on the head.
 * 
 *    - the list destructor calls the destructor for ref_ptr destructor
 *      on the "next" field of the head node.
 * 
 *      - the ref_ptr destructor then calls the list destructor for the
 *        2nd element on the list; NOTE: the previous destructor calls
 *        are still active on the run-time stack.
 * 
 *         - this continues until there are destructor calls on the stack
 *           for each element of the list.
 * 
 * To avoid this, we delay the recursive destructor calls from ref_ptr by
 * putting the pointers on a "death row" before deleting them. For the
 * same linked list:
 * 
 *  - when the reference count of the head goes to 0, the pointer to the
 *    list head is put on death row.
 * 
 *  - since death row is not empty, we start to clear it.
 * 
 *  - the only occupant is the pointer to the list head; the list
 *    destructor is called on the list head.
 * 
 *    - the list destructor calls the ref_ptr for the "next" field of the
 *      list head. This results in a pointer to the second element of the
 *      list being put on death row, BUT the destructor is not yet called
 *      on the 2nd element.
 * 
 *    - the list destructor call on the head completes
 * 
 *  - the destructor is called for the next element on death row (the 2nd
 *    list element).
 * 
 *  - etc.
 * 
 * The number of active list destructor calls is never greater than
 * one. This technique handles mutually recursive data structures with
 * equal ease.
 * 
 * Delayed deletes are processed in LIFO order. (Inuitively, objects
 * are deleted "depth first" in a recursive data
 * structure, which should require smaller overall death rows than a FIFO,
 * "breadth first" ordering would.)
 *
 * Delaying deletes does have a cost --- the number of instructions
 * per delete increases, and heap memory (instead of stack memory) is
 * required to store delayed deletions. I think it is roughly twice as slow,
 * but still pretty speedy (45 sec for 16M objects created and deleted?).
 *
 * Another problem with delayed deletes is that it is not thread-safe
 * (though it is unclear whether the rest of ref_ptr is thread safe).
 * 
 * NOTE: This mechanism has been set to be on by default (it is controlled
 * by the second template parameter of ref_ptr).
 */

class RefCounter
{
    friend std::ostream &
    operator<<( std::ostream& o, const RefCounter& rc )
    {
        return (o << rc.cnt);
    }

 public:
    /* Constructor: by default, when "born," no one references us. */
    RefCounter(unsigned _cnt = 0) : cnt(_cnt) { }
    ~RefCounter() { cnt = 0xdeadbeef; }

    /* Copy Constructor: we are being born, and by default, will have
       no referrers --- even if the object we are copying has
       referrers. */
    RefCounter(const RefCounter &that) : cnt(0) { }

    RefCounter & operator++()
    {
        assert(cnt < 0xdead0000);
        ++cnt;
        return *this;
    }

    const RefCounter & operator++() const
    {
        assert(cnt < 0xdead0000);
        ++cnt;
        return *this;
    }

    RefCounter & operator--()
    {
        assert(cnt < 0xdead0000);
        --cnt;
        return *this;
    }

    const RefCounter & operator--() const
    {
        assert(cnt < 0xdead0000);
        --cnt;
        return *this;
    }


    /* The assignment operator: we are copying the 'value' of
       "that", but we will still have our own set of referrers when
       done. (We are not copying "that's" referrers.) */
    RefCounter & operator=(const RefCounter &that) { return *this; }

    bool operator==(unsigned i) const { return i == cnt; }

    /* Getter of underlying count */
    unsigned get_count() const { return cnt; }

 private:
    mutable unsigned cnt;
};

inline bool operator==(unsigned i, const RefCounter &rc) { return rc == i; }

// DelayedDeleter contains one static function: delete_it. This
// function keeps a stack of "delayed" deletes, which it then empties
// (as described above under GRAMMATECH_NONRECURSIVE_DELETE). 

// Normally, delete_it would be defined in a .cpp file, but making it
// a static member function of a struct allows us to define it in the
// header --- so that clients do not have to change their make
// files. (I.e., this gives us backwards compatibility.)

struct DelayedDeleter
{
    // A DelFn_t takes a void*, casts it to an appropriate type, and
    // calls delete.
    typedef void(*DelFn_t)(const void*);

    // A DelayedDel_t is a pair of (i) a function that deletes an
    // object (given a void* to the object) and (ii) a void* to an
    // object.
    typedef std::pair<DelFn_t, const void*> DelayedDel_t;

    static void
    delete_it(DelFn_t fn, const void *ptr)
    {
        // The stack that we use to store delayed deletes (leaked 
        // memory)
        static std::vector<DelayedDel_t>* 
            pDeathRow = new std::vector<DelayedDel_t>;

        // A bound on the size of the stack when not in use
        static size_t DfltCapacity = 
            (pDeathRow->capacity() > 1024) ? pDeathRow->capacity() : 1024;

        // A flag that tells us if we're the first in a chain of deletes
        static bool b_first(true);

        // Push this "delete action" to be triggered later.
        // "delete actions" are processed in LIFO order.
        pDeathRow->push_back(DelayedDel_t(fn,ptr));

        if( b_first ) {
            // We're at the "first" of a delete chain, so we're
            // responsible for empying the stack. Toggle b_first so
            // subsequent calls leave the deleting to us.
            b_first = false;

            // Clear out all stacked "delete actions"
            while( ! pDeathRow->empty() ) {
                DelayedDel_t curr = pDeathRow->back();
                pDeathRow->pop_back();

                // This executes the delayed delete. If deleted object
                // contains ref_ptr's, then this call may result in
                // more "delayed deletes" being pushed onto *pDeathRow.
                curr.first(curr.second);
            }

            // We're done deleting reference counted objects for
            // now. Set the flag so that the next delete takes
            // responsibility for emptying death row.
            b_first = true;

            // We keep death row around so that we don't have to repeatedly
            // allocate it, but we'd like it not to be soaking up space.
            if( pDeathRow->capacity() > (unsigned)DfltCapacity ) {
                delete pDeathRow;
                pDeathRow = new std::vector<DelayedDel_t>;
            }
        } 
    }
};

// Stand-in class for constructing a NULL ref_ptr.
class NULL_ref_ptr {};

template< typename T, bool DelayDeletes = true > 
class ref_ptr
{
public:
    typedef T element_type;
    typedef unsigned int count_t;

    ref_ptr( T *t = 0 )
    {
        acquire( t );
    }

    ref_ptr( const NULL_ref_ptr & ignored ) : ptr(0) {}

    ref_ptr( const ref_ptr& rp )
    {
        // Copy construction from another ref_ptr implies that either
        // the other ref_ptr is null, or it must be pointing to
        // something that has a non-zero reference count. Let's just
        // double-check.
        assert(rp.ptr == NULL || !(rp.ptr->count == 0));

        acquire( rp.ptr );
    }

    template <typename S>
    ref_ptr<T, DelayDeletes> ( const ref_ptr<S, DelayDeletes> & rp )
    {
        acquire( rp.get_ptr() );
    }

    ~ref_ptr()
    {
        release();
    }

    ref_ptr &
    operator=( T* t )
    {
        if( ptr != t ) {
            T* old_ptr = ptr;
            acquire(t);
            release(old_ptr);
        }
        return *this;
    }

    ref_ptr &
    operator=( const ref_ptr& rp )
    {
        // Assignment from another ref_ptr implies that either
        // the other ref_ptr is null, or it must be pointing to
        // something that has a non-zero reference count. Let's just
        // double-check.
        assert(rp.ptr == NULL || !(rp.ptr->count == 0));

        return *this = rp.ptr;
    }

    template <typename S>
    ref_ptr<T, DelayDeletes> &
    operator=( const ref_ptr<S, DelayDeletes> & rp )
    {
        // Assignment from another ref_ptr implies that either
        // the other ref_ptr is null, or it must be pointing to
        // something that has a non-zero reference count. Let's just
        // double-check.
        assert(rp.get_ptr() == NULL || !(rp.get_ptr()->count == 0));

        return *this = rp.get_ptr();
    }

    // "<" is provided to facilitate ordering in containers (e.g., std::set).
    // The rest are provided for completeness.
    // A generic templatized version to check for subclassing (a la "==" below)
    // may not be particularly useful.
    bool operator<(const ref_ptr<T, DelayDeletes> & r) const
    { return ptr < r.ptr; }
    bool operator<=(const ref_ptr<T, DelayDeletes> & r) const
    { return ptr <= r.ptr; }
    bool operator>(const ref_ptr<T, DelayDeletes> & r) const
    { return ptr > r.ptr; }
    bool operator>=(const ref_ptr<T, DelayDeletes> & r) const
    { return ptr >= r.ptr; }

    // these two are needed to handle (*this == NULL) case for MSVS; MSVS
    // says it cannot figure out the type of the arg for NULL.  g++ seems
    // okay with it.  I have no idea which is correct wrt ANSI c++.
    bool operator==(const T * t) const { return ptr == t; }
    bool operator!=(const T * t) const { return ptr != t; }

    // forces compiler to verify that S is same as or subclass of T.  all
    // compare functions should go through here.
    template <typename S>
    bool
    operator==(const S * s) const
    {
        const T *t;
        (void)sizeof(t = s);
        (void)t;
        return ptr == s;
    }

    template <typename S>
    bool
    operator!=(const S * s) const
    {
        return !(this->operator==(s));
    }

    template <typename S>
    bool
    operator==(const ref_ptr<S, DelayDeletes> & that) const
    {
        return this->operator==(that.get_ptr());
    }

    template <typename S>
    bool
    operator!=(const ref_ptr<S, DelayDeletes> & that) const
    {
        return !(this->operator==(that.get_ptr()));
    }

    T * get_ptr() const { return ptr; }

    T * operator->() const { return ptr; }

    T &
    operator*() const
    {
#ifdef DBGREFPTR
        if(0 == ptr)
        {
            std::cerr << "ref_ptr::operator*: Invalid ptr.\n";
        }
#endif
        /* for NDEBUG. Make sure it crashes
           here rather than have some
           undefined behaviour. */
        assert(0 != ptr);
        return *ptr;
    }

    std::ostream &
    print( std::ostream& out )
    {
        if( ptr )
            ptr->print( out );
        else
            out << "NULL";
        return out;
    }

    bool is_empty() const { return (0 == ptr); }
    bool is_valid() const { return !(is_empty());}

private:
    T *ptr;

    template <typename S>
    void
    acquire( S *s )
    {
        // forces the compiler to recognize whether S is a subclass of T.
        ptr = s;
        if( s ) {
#ifdef DBGREFPTR
            std::cout << "Acquiring " << (void*) s << std::endl;
#endif
            ++s->count;
#ifdef DBGREFPTR
            std::cout << "Acquired " << (void*) s << " with count = "
                      << s->count << std::endl;
#endif
        }
    }

    // This is a DelFn_t that is used by DelayedDeleter::delete_it to
    // execute a delayed delete of *ptr.
    static void
    delete_fn(const void * void_ptr) {
#ifdef DBGREFPTR
        std::cout << "  (delayed delete of " << (void*) void_ptr << ")" << std::endl;
#endif
        const T* ptr = reinterpret_cast<const T*>(void_ptr);
        // Check if the object has been "reclaimed" since it was
        // queued for deletion. (Unusual, but possible.)
        if( ptr->count == 0 )
            delete ptr;
    }

    static void
    release(T * old_ptr)
    {
        if( old_ptr ) {
#ifdef DBGREFPTR
            std::cout << "Releasing " << (void*) old_ptr << std::endl;
#endif
            --old_ptr->count;
#ifdef DBGREFPTR
            std::cout << "Released " << (void*) old_ptr << " with count = "
                      << old_ptr->count << std::endl;
#endif
            if( old_ptr->count == 0 ) {
#ifdef DBGREFPTR
                std::cout << "Deleting ptr: " << (void*) old_ptr << std::endl;
#endif

#ifdef __CSURF__
                // This gets evaluated when using CodeSonar to analyze
                // this code. The call to this function gives
                // CodeSonar enough uncertainty about whether or not
                // old_ptr might be freed that it won't give
                // use-after-free or leak warnings. Without this, we
                // get far too many false positives from local copies
                // of ref_ptr's.
                extern void undefined_function_call(T * p);
                undefined_function_call(old_ptr);
#else
                if( DelayDeletes )
                    // Ask DelayedDeleter to (eventually) delete
                    // old_ptr using delete_fn.
                    DelayedDeleter::delete_it(delete_fn,old_ptr);
                else 
                    delete old_ptr;
#endif
            }
        }
    }

    void
    release()
    {
        release(ptr);
        ptr = 0;
    }
};

template< typename T > std::ostream&
operator<<(std::ostream& o,
           const ref_ptr< T > r )
{
    if( 0 != r.get_ptr() )
        return r->print(o);
    else
        return (o << "NULL");
}

// ----------------------------------------------------------------------------
// The following functions provides const, dynamic and static cast support for
// ref_ptrs.  More or less, it's a thin veil to be rid of the call to
// get_ptr() to do this yourself.  Basic functionality is modeled after that
// found in boost's shared_ptr, albeit w/o the cast_tag + constructor
// mechanism.  Usage looks like:
//
//  Derived * d = new Derived();
//  ref_ptr<Base> brp(d);
//  ref_ptr<Derived> drp(dynamic_pointer_cast<Derived>(brp));
//  if (drp.is_empty())
//      ...
//

template <class D, class B>
ref_ptr<D>
static_pointer_cast(const ref_ptr<B> & rp)
{
    return ref_ptr<D>(static_cast<D*>(rp.get_ptr()));
}

template <class D, class B>
ref_ptr<D>
dynamic_pointer_cast(const ref_ptr<B> & rp)
{
    return ref_ptr<D>(dynamic_cast<D*>(rp.get_ptr()));
}

#endif  // REF_PTR_H_
