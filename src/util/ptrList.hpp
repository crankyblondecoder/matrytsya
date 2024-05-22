// *** Doubly linked List of Pointers Template ***

#ifndef PTRLIST_H
#define PTRLIST_H

/**
 * Linked list of pointers to type T.
 */
template <class T> class ptrListItem
{
	public:

		virtual ~ptrListItem();

		ptrListItem();
		ptrListItem(const ptrListItem<T>& item);

		ptrListItem(T* ptr, ptrListItem<T>* chain, bool delObjPtdTo, bool insert = false);

		// Linked list functions.
		ptrListItem<T>* next();
		ptrListItem<T>* prev();
		ptrListItem<T>* first();
		ptrListItem<T>* last();

		/** Return item at index. */
		ptrListItem<T>* atIndex(int index);

		void replace(T* ptr, bool delObjPtdTo);

		bool hasItem(T* ptr, bool traverse);

		virtual ptrListItem<T>* getItem(T* ptr);

		T* getPtr();

		/// Number of items in list.
		virtual unsigned count();

		/// Link a node into chain after this one.
		void link(ptrListItem<T>* linkTo, bool next);

		/// Unlink this node from chain.
		void unlink();

		void unlinkPrev(ptrListItem<T>* newPrev);
		void unlinkNext(ptrListItem<T>* newNext);

		void cascadeDelete();  // Whole list is being deleted.

	protected:

		void setNext(ptrListItem<T>* next);
		void setPrevious(ptrListItem<T>* prev);

		T* pointer;
		bool delObj;  // Delete the object this item points to when item is deleted.

		ptrListItem<T>* prevLnk;  // Linked list.
		ptrListItem<T>* nextLnk;

	private:

		// Assignment is not allowed.
		ptrListItem<T>& operator= (const ptrListItem<T>& item);
};

/**
 * Container for ptrListItem
 * @note Indexes are 0 based.
 */
template <class T> class ptrList
{
	public:

		virtual ~ptrList();

		ptrList();
		ptrList(const ptrList<T>& list);

		ptrList<T>& operator= (const ptrList<T>& list);

		void copyFrom(const ptrList<T>& list);

		/// Get the first pointer in the list.
		T* first();

		/// Get the last pointer in the list.
		T* last();

		/// Get the previous pointer in the list.
		T* prev();

		/// Get the next pointer in the list.
		T* next();

		/// Get the current pointer in the list.
		T* current();

		/**
	 	 * Make item at index current and return it.
	 	 * @param index Index of pointer to find. Index 0 is first item in list.
	 	 */
		T* atIndex(int index);

		/// Delete all items at once.
		virtual void clear();

		/// Delete all pointers after current pointer.
		virtual void truncate();

		/**
		 * Append pointer to list
		 * @param ptr Pointer to append.
		 * @param delObj Delete object ptr points to when list item is deleted.
		*/
		virtual void append(T* ptr, bool delObj);

		virtual void insert(T* ptr, int index, bool delObj);
		virtual void replace(T* ptr, bool delObj);
		virtual void move(T* ptr, int toIndex);
		virtual void move(int fromIndex, int toIndex); //!< Move item from one index to another.
		virtual void move(T* ptr, T* after);
		virtual void remove(T* ptr);
		virtual void remove();

		/** Return True if Item "ptr" is Part of List */
		virtual bool hasItem(T* ptr);

		/** Find Item's Index. */
		virtual int itemIndex(T* ptr);

		virtual unsigned count();

	protected:

		/// Current ptrListItem.
		ptrListItem<T>* cItem;

		/// First item in list.
		ptrListItem<T>* fItem;
};

// *** Implementation - Appears here so any type can be used ***

template <class T> ptrListItem<T>::~ptrListItem()
{
	// Delete object pointed to if required.
	if (delObj && pointer) delete pointer;

	// Unlink this item from chain.
	unlink();
}

template <class T> ptrListItem<T>::ptrListItem()
{
	pointer = 0;
	prevLnk = 0;
	nextLnk = 0;
}

template <class T> ptrListItem<T>::ptrListItem(const ptrListItem<T>& item)
{
	// Copy constructor.
	// -----------------
	// Notes:   Copies pointer NOT the object being pointed to.

	pointer = item.pointer;

	delObj = false;  // The pointer is being copied so don't mark it for deletion.

	prevLnk = 0;
	nextLnk = 0;
}

template <class T> ptrListItem<T>::ptrListItem(T* ptr, ptrListItem<T>* chain, bool delObjPtdTo, bool insert)
{
	// Constructor
	// -----------
	// ptr:         Pointer to store.
	// chain:       ptrListItem to link to or to insert this item before.
	// delObjPtdTo: Delete object this item points to when item is deleted.
	// insert:      Insert into list instead of the default which is to append at the end.

	pointer = ptr;
	delObj = delObjPtdTo;

	// Set next link.

	if(insert && chain)
	{
		nextLnk = chain;
	}
	else
	{
		// Put at end of chain.
		nextLnk = 0;
	}

	// Set previous link.

	if(chain)
	{
		if(insert)
		{
			prevLnk = chain -> prev();

			if(chain -> prev())
			{
				(chain -> prev()) -> setNext(this);
			}
			else
			{
				chain -> setPrevious(this);
			}
		}
		else
		{
			prevLnk = chain -> last();

			// Link into chain.
			(chain -> last()) -> setNext(this);
		}
	}
	else
	{
		prevLnk = 0;
	}
}

template <class T> ptrListItem<T>* ptrListItem<T>::next()
{
	return nextLnk;
}

template <class T> ptrListItem<T>* ptrListItem<T>::prev()
{
	return prevLnk;
}

template <class T> ptrListItem<T>* ptrListItem<T>::first()
{
	ptrListItem* cSelect;
	ptrListItem* rSelect = 0;

	if(prevLnk)
	{
		cSelect = prevLnk;
	}
	else
	{
		cSelect = 0;
		rSelect = this;
	}

	while(cSelect)
	{
		rSelect = cSelect;
		cSelect = cSelect -> prev();
	}

	return rSelect;
}

template <class T> ptrListItem<T>* ptrListItem<T>::last()
{
	ptrListItem* cSelect;
	ptrListItem* rSelect = 0;

	if(nextLnk)
	{
		cSelect = nextLnk;
	}
	else
	{
		cSelect = 0;
		rSelect = this;
	}

	while(cSelect)
	{
		rSelect = cSelect;
		cSelect = cSelect -> next();
	}

	return rSelect;
}

template <class T> ptrListItem<T>* ptrListItem<T>::atIndex(int index)
{
	ptrListItem<T>* itm = first();
	int count = 0;

	while(itm && count < index)
	{
		count ++;
		itm = itm -> next();
	}

	return itm;
}

template <class T> bool ptrListItem<T>::hasItem(T* ptr, bool traverse)
{
	// Does the chain of items hold given pointer
	// ------------------------------------------
	// ptr:         Pointer to item.
	// traverse:    Move forward through primSelects linked by the "next" variable.

	if(ptr == pointer) return true;  // Don't process anymore already found match.

	if(!traverse) return false;  // No point in continuing traverse is false.

	bool found = false;

	ptrListItem<T>* itm = next();

	while(itm && (!found))
	{
		found =  itm -> hasItem(ptr, false);

		itm = itm -> next();
	}

	return found;
}

template <class T> ptrListItem<T>* ptrListItem<T>::getItem(T* ptr)
{
	// Get item if pointer is part of list
	// -----------------------------------
	// ptr:     Pointer to search for.
	//
	// Notes:   Only a forward traversal is performed.

	if(ptr == pointer) return this;  // Don't process anymore already found match.

	bool found = false;

	ptrListItem<T>* itemFound = 0;

	ptrListItem<T>* itm = next();

	while(itm && (!found))
	{
		found = itm -> hasItem(ptr, false);

		if(found) itemFound = itm;

		itm = itm -> next();
	}

	return itemFound;
}

template <class T> unsigned ptrListItem<T>::count()
{
	// Return number of nodes in list
	// ------------------------------

	ptrListItem<T>* itm = first();
	unsigned count = 0;

	while(itm)
	{
		count ++;
		itm = itm -> next();
	}

	return count;
}

template <class T> void ptrListItem<T>::setNext(ptrListItem<T>* next)
{
	// Set the "next" node of this node.
	// ---------------------------------

	if(next == nextLnk) return;  // Stops recursive endless loop.

	ptrListItem<T>* oldNext = nextLnk;

	nextLnk = next;

	// If there is already a next node reset it's previous to the new node.
	if(oldNext) oldNext -> setPrevious(next);
}

template <class T> void ptrListItem<T>::setPrevious(ptrListItem<T>* prev)
{

	// Set the previous node to "prev"
	// -------------------------------

	if(prev == prevLnk) return;

	ptrListItem<T>* oldPrev = prevLnk;

	prevLnk = prev;

	// If there is already a previous node then reset it's next to this node.
	if(oldPrev) oldPrev -> setNext(prev);
}

template <class T> void ptrListItem<T>::link(ptrListItem<T>* linkTo, bool next)
{
	// Link a node into chain after this one.
	// --------------------------------------
	// linkTo:    Node to link to.
	// next:      If true then link node after this one otherwise before this one.

	if(next)
	{
		if(nextLnk)
		{
			linkTo -> setNext(nextLnk);

			nextLnk -> setPrevious(linkTo);
		}

		linkTo -> setPrevious(this);

		nextLnk = linkTo;
	}
	else
	{
		if(prevLnk)
		{
			linkTo -> setPrevious(prevLnk);

			prevLnk -> setNext(linkTo);
		}

		linkTo -> setNext(this);

		prevLnk = linkTo;
	}
}

template <class T> void ptrListItem<T>::unlink()
{
	// Unlink this object from chain
	// -----------------------------

	if(prevLnk) prevLnk -> unlinkNext(nextLnk);
	if(nextLnk) nextLnk -> unlinkPrev(prevLnk);

	nextLnk = 0;
	prevLnk = 0;
}

template <class T> void ptrListItem<T>::unlinkNext(ptrListItem<T>* newNext)
{
	// Unlink the next object in chain
	// -------------------------------
	// newNext:     The object which is now being linked as next.

	nextLnk = newNext;
}

template <class T> void ptrListItem<T>::unlinkPrev(ptrListItem<T>* newPrev)
{
	// Unlink the prev object in chain
	// -------------------------------
	// newPrev:     The object which is now being linked as prev.

	prevLnk = newPrev;
}

template <class T> T* ptrListItem<T>::getPtr()
{
	return pointer;
}

template <class T> void ptrListItem<T>::cascadeDelete()
{
	// Cascade down the linked list to delete entire list.
	// ---------------------------------------------------

	if(nextLnk)
	{
		nextLnk -> cascadeDelete();
		delete nextLnk;
	}
}

template <class T> void ptrListItem<T>::replace(T* ptr, bool delObjPtdTo)
{
	// Replace object pointed to with ptr.
	// -----------------------------------
	// ptr:             New object to point to.
	// delObjPtdTo:     Owns pointer and can / should delete it.

	if(pointer && delObj) delete pointer;

	pointer = ptr;

	delObj = delObjPtdTo;
}

// *** Pointer List Object ***

template <class T> ptrList<T>::~ptrList()
{
	if(! cItem) return;

	// Delete all associated items.

	ptrListItem<T>* delItem = cItem -> last();
	ptrListItem<T>* prevItem;

	while(delItem)
	{
		prevItem = delItem -> prev();
		delete delItem;
		delItem = prevItem;
	}
}

template <class T> ptrList<T>::ptrList()
{
	cItem = 0;
	fItem = 0;
}

template <class T> ptrList<T>::ptrList(const ptrList<T>& list)
{
	// Copy Constructor.
	// -----------------

	copyFrom(list);
}

template <class T> ptrList<T>& ptrList<T>::operator= (const ptrList<T>& list)
{
	clear();

	copyFrom(list);
}

template <class T> void ptrList<T>::copyFrom (const ptrList<T>& list)
{
	// Copy contents of list into this.
	// --------------------------------

	cItem = 0;
	fItem = 0;

	ptrListItem<T>* itemToCopy = list.fItem;

	ptrListItem<T>* newItem;

	ptrListItem<T>* newCItem;

	// Copy pointers across but they can't be automatically deleted.

	while(itemToCopy)
	{
		newItem = new ptrListItem<T>(itemToCopy -> getPtr(), cItem, false, false);

		if(!fItem) fItem = newItem;

		if(itemToCopy == list.cItem) newCItem = newItem;

		itemToCopy = itemToCopy -> next();
	}

	// Point new list to same position.
	cItem = newCItem;
}

template <class T> void ptrList<T>::clear()
{
	if(!cItem) return;

	ptrListItem<T>* delItem = cItem -> last();
	ptrListItem<T>* prevItem;

	while(delItem)
	{
		prevItem = delItem -> prev();
		delete delItem;
		delItem = prevItem;
	}

	cItem = 0;
	fItem = 0;
}

template <class T> void ptrList<T>::truncate()
{
	if(cItem) cItem -> cascadeDelete();
}

template <class T> void ptrList<T>::append(T* ptr, bool delObj)
{
	ptrListItem<T>* item = new ptrListItem<T>(ptr, cItem, delObj);

	cItem = item;

	if(! item -> prev()) fItem = item;
}

template <class T> void ptrList<T>::insert(T* ptr, int index, bool delObj)
{
	// Insert pointer into list
	// ------------------------
	// ptr:     Pointer to append.
	// index:   Index to insert new pointer into.
	// delObj:  Delete object ptr points to when list item is deleted.

	ptrListItem<T>* item;

	// atIndex will set the current item to the one found at the index.
	if(this -> atIndex(index))
	{
		item = new ptrListItem<T>(ptr, cItem, delObj, true);
	}
	else
	{
		item = new ptrListItem<T>(ptr, cItem, delObj, false);
	}

	cItem = item;

	if(!item -> prev()) fItem = item;
}

template <class T> void ptrList<T>::replace(T* ptr, bool delObj)
{
	// Replace current items pointer with ptr.
	// ---------------------------------------
	// ptr:         Pointer to now use.
	// delObj:      List owns object.

	if(cItem) cItem -> replace(ptr, delObj);
}

template <class T> void ptrList<T>::move(T* ptr, int toIndex)
{
	// Move pointer to different position in list.
	// -------------------------------------------
	// ptr:        Pointer to move within list.
	// toIndex:    Index to move item to.

	if(!cItem) return;

	int fromIndex = itemIndex(ptr);

	move(fromIndex, toIndex);
}

template <class T> void ptrList<T>::move(int fromIndex, int toIndex)
{
	//! @param fromIndex Index to move from.
	//! @param toIndex Index to move to.

	if(!cItem) return;

	ptrListItem<T>* itemToMove = cItem -> atIndex(fromIndex);

	if(itemToMove)
	{
		ptrListItem<T>* itemAtPosition = cItem -> atIndex(toIndex);

		if(itemAtPosition && itemToMove != itemAtPosition)
		{
			if(!itemToMove -> prev()) fItem = itemToMove -> next(); // itemToMove was the first item.

			itemToMove -> unlink();

			if(fromIndex < toIndex)
			{
				// itemToMove was before itemAtPosition.
				itemAtPosition -> link(itemToMove, true);
			}
			else
			{
				itemAtPosition -> link(itemToMove, false);
			}

			if(!itemToMove -> prev()) fItem = itemToMove; // itemToMove is now the first item.
		}
	}
}

template <class T> void ptrList<T>::move(T* ptr, T* toPositionOf)
{
	// Move pointer to different position in list.
	// -------------------------------------------
	// ptr:            Pointer to move within list.
	// toPositionOf:   Move into position in list that this pointer occupies.

	if(!cItem) return;

	if(ptr == toPositionOf) return;

	ptrListItem<T>* itemToMove = cItem -> getItem(ptr);

	ptrListItem<T>* itemAtPosition = cItem -> getItem(toPositionOf);

	if(itemToMove && itemAtPosition)
	{
		if(!itemToMove -> prev()) fItem = itemToMove -> next(); // itemToMove is the first item.

		itemToMove -> unlink();

		itemAtPosition -> link(itemToMove, false);

		cItem = itemToMove;

		if (!itemToMove -> prev()) fItem = itemToMove;
	}
}

template <class T> void ptrList<T>::remove(T* ptr)
{
	// Remove pointer from list
	// ------------------------
	// ptr:     Pointer to remove.

	if(cItem)
	{
		ptrListItem<T>* item = fItem -> getItem(ptr);

		if(item)
		{
			if(cItem == item)
			{
				// About to delete the current item so get new cItem.
				if(item -> prev())
				{
					// Make current item previous in list.
					cItem = item -> prev();
				}
				else
				{
					// If no previous in list then make current item next in list.
					cItem = item -> next();
				}
			}

			// Deleting first item so set new first item.
			if(fItem == item) fItem = item -> next();

			delete item;
		}
	}
}

/**
 * Remove current item from list
 */
template <class T> void ptrList<T>::remove()
{
	if(!cItem) return;

	ptrListItem<T>* prevItem = cItem -> prev();
	ptrListItem<T>* nextItem = cItem -> next();

	delete cItem;

	if(nextItem)
	{
		cItem = nextItem;
		if(!prevItem) fItem = nextItem;
	}
	else if(prevItem)
	{
		cItem = prevItem;
	}
	else
	{
		cItem = 0;
		fItem = 0;
	}
}

template <class T> T* ptrList<T>::first()
{
	if(cItem)
	{
		cItem = fItem;
		return (cItem -> getPtr());
	}

	return 0;
}

template <class T> T* ptrList<T>::last()
{
	if(cItem)
	{
		ptrListItem<T>* item = cItem -> last();
		cItem = item;
		return (item -> getPtr());
	}

	return 0;
}

template <class T> T* ptrList<T>::prev()
{
	if(cItem)
	{
		ptrListItem<T>* item = cItem -> prev();

		if(item)
		{
			cItem = item;
			return (item -> getPtr());
		}
		else
		{
			return 0;
		}
	}

	return 0;
}

template <class T> T* ptrList<T>::next()
{
	if(cItem)
	{
		ptrListItem<T>* item = cItem -> next();

		if(item)
		{
			cItem = item;
			return (item -> getPtr());
		}
		else
		{
			return 0;
		}
	}

	return 0;
}

template <class T> unsigned ptrList<T>::count()
{
	if(cItem) return ( cItem -> count() );

	return 0;
}

template <class T> T* ptrList<T>::current()
{
	// Get current item
	// ----------------

	if(cItem) return (cItem -> getPtr());

	return 0;
}

template <class T> T* ptrList<T>::atIndex(int index)
{
	if(cItem)
	{
		ptrListItem<T>* item = cItem -> atIndex(index);

		if(item)
		{
			cItem = item;
			return (item -> getPtr());
		}
		else
		{
			return 0;
		}
	}

	return 0;
}

template <class T> bool ptrList<T>::hasItem(T* ptr)
{
	if(fItem)
	{
		return fItem -> hasItem(ptr, true);
	}

	return false;
}

template <class T> int ptrList<T>::itemIndex(T* ptr)
{
	if(!fItem) return -1;

	ptrListItem<T>* item = fItem;

	int index = 0;

	while(item && (item -> getPtr() != ptr))
	{
		index ++;

		item = item -> next();
	}

	return index;
}

#endif



