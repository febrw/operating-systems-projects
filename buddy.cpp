/*
* Buddy Page Allocation Algorithm
* SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (3)
*/

/*
* STUDENT NUMBER: s1735009
*/
#include <infos/mm/page-allocator.h>
#include <infos/mm/mm.h>
#include <infos/kernel/kernel.h>
#include <infos/kernel/log.h>
#include <infos/util/math.h>
#include <infos/util/printf.h>

using namespace infos::kernel;
using namespace infos::mm;
using namespace infos::util;

#define MAX_ORDER	16
// I changed the _free_areas array to be of length MAX_ORDER+1, so it contains free lists for orders 0 to 16 inclusive, and thus is of size 17
/**
* A buddy page allocation algorithm.
*/
class BuddyPageAllocator : public PageAllocatorAlgorithm
{
private:
	/**
	* Returns the number of pages that comprise a 'block', in a given order.
	* @param order The order to base the calculation off of.
	* @return Returns the number of pages in a block, in the order.
	*/
	static inline constexpr uint64_t pages_per_block(int order)
	{
		/* The number of pages per block in a given order is simply 1, shifted left by the order number.
		* For example, in order-2, there are (1 << 2) == 4 pages in each block.
		*/
		return (1 << order);
	}

	/**
	* Returns TRUE if the supplied page descriptor is correctly aligned for the
	* given order.  Returns FALSE otherwise.
	* @param pgd The page descriptor to test alignment for.
	* @param order The order to use for calculations.
	*/
	static inline bool is_correct_alignment_for_order(const PageDescriptor *pgd, int order)
	{
		// Calculate the page-frame-number for the page descriptor, and return TRUE if
		// it divides evenly into the number pages in a block of the given order.
		return (sys.mm().pgalloc().pgd_to_pfn(pgd) % pages_per_block(order)) == 0;
	}

	/** Given a page descriptor, and an order, returns the buddy PGD.  The buddy could either be
	* to the left or the right of PGD, in the given order.
	* @param pgd The page descriptor to find the buddy for.
	* @param order The order in which the page descriptor lives.
	* @return Returns the buddy of the given page descriptor, in the given order.
	*/
	PageDescriptor *buddy_of(PageDescriptor *pgd, int order)
	{
		// (1) Make sure 'order' is within range
		if (order >= MAX_ORDER) {
			return NULL;
		}

		// (2) Check to make sure that PGD is correctly aligned in the order
		if (!is_correct_alignment_for_order(pgd, order)) {
			return NULL;
		}

		// (3) Calculate the page-frame-number of the buddy of this page.
		// * If the PFN is aligned to the next order, then the buddy is the next block in THIS order.
		// * If it's not aligned, then the buddy must be the previous block in THIS order.
		uint64_t buddy_pfn = is_correct_alignment_for_order(pgd, order + 1) ?
		sys.mm().pgalloc().pgd_to_pfn(pgd) + pages_per_block(order) :
		sys.mm().pgalloc().pgd_to_pfn(pgd) - pages_per_block(order);

		// (4) Return the page descriptor associated with the buddy page-frame-number.
		return sys.mm().pgalloc().pfn_to_pgd(buddy_pfn);
	}

	/**
	* Inserts a block into the free list of the given order.  The block is inserted in ascending order.
	* @param pgd The page descriptor of the block to insert.
	* @param order The order in which to insert the block.
	* @return Returns the slot (i.e. a pointer to the pointer that points to the block) that the block
	* was inserted into.
	*/
	PageDescriptor **insert_block(PageDescriptor *pgd, int order)
	{
		// Starting from the _free_areas array, find the slot in which the page descriptor
		// should be inserted.
		PageDescriptor **slot = &_free_areas[order];

		// Iterate whilst there is a slot, and whilst the page descriptor pointer is numerically
		// greater than what the slot is pointing to.
		while (*slot && pgd > *slot) {
			slot = &(*slot)->next_free;
		}

		// Insert the page descriptor into the linked list.
		pgd->next_free = *slot;
		*slot = pgd;

		// Return the insert point (i.e. slot)
		return slot;
	}

	/**
	* Removes a block from the free list of the given order.  The block MUST be present in the free-list, otherwise
	* the system will panic.
	* @param pgd The page descriptor of the block to remove.
	* @param order The order in which to remove the block from.
	*/
	void remove_block(PageDescriptor *pgd, int order)
	{
		// Starting from the _free_areas array, iterate until the block has been located in the linked-list.
		PageDescriptor **slot = &_free_areas[order];
		while (*slot && pgd != *slot) {
			slot = &(*slot)->next_free;
		}

		// Make sure the block actually exists.  Panic the system if it does not.
		assert(*slot == pgd);

		// Remove the block from the free list.
		*slot = pgd->next_free;
		pgd->next_free = NULL;
	}

	/**
	* Given a pointer to a block of free memory in the order "source_order", this function will
	* split the block in half, and insert it into the order below.
	* @param block_pointer A pointer to a pointer containing the beginning of a block of free memory.
	* @param source_order The order in which the block of free memory exists.  Naturally,
	* the split will insert the two new blocks into the order below.
	* @return Returns the left-hand-side of the new block.
	*/
	PageDescriptor *split_block(PageDescriptor **block_pointer, int source_order)
	{
		// Make sure there is an incoming pointer.
		assert(*block_pointer);

		// Make sure the block_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));
		// check positive
		assert(source_order>0);

		// TODO: Implement this function
		int new_order = source_order - 1;
		PageDescriptor *left = *block_pointer;
		PageDescriptor *right = buddy_of(left , new_order);

		remove_block(left,source_order);

		insert_block(left, new_order);
		insert_block(right, new_order);

		return left;
	}

	/**
	* Takes a block in the given source order, and merges it (and it's buddy) into the next order.
	* This function assumes both the source block and the buddy block are in the free list for the
	* source order.  If they aren't this function will panic the system.
	* @param block_pointer A pointer to a pointer containing a block in the pair to merge.
	* @param source_order The order in which the pair of blocks live.
	* @return Returns the new slot that points to the merged block.
	*/
	PageDescriptor **merge_block(PageDescriptor **block_pointer, int source_order)
	{
		assert(*block_pointer);

		// Make sure the area_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));
		assert(source_order<MAX_ORDER);

		PageDescriptor *protagonist = *block_pointer;
		PageDescriptor *buddy = buddy_of(protagonist, source_order);

		PageDescriptor *left, *right;

		if(buddy>protagonist){
			right = buddy;
			left = protagonist;
		}
		else {
			right = protagonist;
			left = buddy;
		}

		remove_block(left, source_order);
		remove_block(right, source_order);
		return insert_block(left, source_order + 1);
	}

public:
	/**
	* Constructs a new instance of the Buddy Page Allocator.
	*/
	BuddyPageAllocator() {
		// Iterate over each free area, and clear it.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			_free_areas[i] = NULL;
		}
	}

	/**
	* Allocates 2^order number of contiguous pages
	* @param order The power of two, of the number of contiguous pages to allocate.
	* @return Returns a pointer to the first page descriptor for the newly allocated page range, or NULL if
	* allocation failed.
	*/
	PageDescriptor *alloc_pages(int order) override
	{
		//not_implemented();
		// get pointer to location where enough space is free
		assert(0 <= order && order <= MAX_ORDER);
		PageDescriptor *new_block = _free_areas[order];
		int current_order = order;
		// if non existent, we must split to create, continue algorithm until current_order once again matches order
		while(!new_block || current_order > order){

			if(current_order > MAX_ORDER){
				// no blocks to split, no memory free at any level
				return nullptr;
			}

			if(_free_areas[current_order]){
				new_block = split_block(&_free_areas[current_order], current_order);
				// decrement current order
				--current_order;
			}
			// no free blocks in current_order, increase order and intend to split
			else {
				++current_order;
			}
		}
		remove_block(new_block,current_order);
		return new_block;
	}

	// helper function for free_pages
	// called on a buddy, to ascertain whether it is contained in the free list for said order
	PageDescriptor** buddy_free(PageDescriptor *pgd, int order){
		PageDescriptor **slot = &_free_areas[order];
		while(*slot != nullptr){
			if(*slot == pgd){return slot;}
			// otherwise ,check next_free
			slot = &(*slot) -> next_free;
		}
		return nullptr;
	}

	/**
	* Frees 2^order contiguous pages.
	* @param pgd A pointer to an array of page descriptors to be freed.
	* @param order The power of two number of contiguous pages to free.
	*/
	void free_pages(PageDescriptor *pgd, int order) override
	{
		// Make sure that the incoming page descriptor is correctly aligned
		// for the order on which it is being freed, for example, it is
		// illegal to free page 1 in order-1.
		assert(is_correct_alignment_for_order(pgd, order));
		assert(0<=order && order<=MAX_ORDER);

		// add pointer to free list off the bat
		insert_block(pgd,order);
		// no buddy or merging if we are freeing all of the memory
		if(order==MAX_ORDER){return;}

		PageDescriptor *buddy = buddy_of(pgd,order);
		// no buddy if order is MAX_ORDER
		while(buddy_free(buddy,order)!=nullptr && order<MAX_ORDER){
			pgd = *merge_block(&pgd, order);
			// by merging we increase the order by 1, and find the new buddy given this order increase
			buddy = buddy_of(pgd,++order);
		}
		return;
	}

	// helper function, ascertain whether page in a block, used in reserve_block
	// address  of pgd will be contained within lower and upper bounds of memory addresses in the block IFF it is present in the block
	bool page_in_block(PageDescriptor *block, PageDescriptor *pgd, int order){
		int ppb = pages_per_block(order);
		PageDescriptor *last_page = block + ppb;
		return (block <= pgd) && (pgd < last_page);
	}

	/**
	* Reserves a specific page, so that it cannot be allocated.
	* @param pgd The page descriptor of the page to reserve.
	* @return Returns TRUE if the reservation was successful, FALSE otherwise.
	*/
	bool reserve_page(PageDescriptor *pgd)
	{
		//not_implemented();
		// start from the top and split till we find it
		int current_order = MAX_ORDER;
		PageDescriptor *current_block = nullptr;

		while(current_order >= 0){
			if(current_order==0 && current_block!=nullptr){
				PageDescriptor **slot = buddy_free(pgd,current_order);
				if(slot!=nullptr){
					//Test with and without this assertion
					//assert(*slot==pgd);
					remove_block(*slot,current_order);
					return true;
				}
				// found, but it is not free
				else{return false;}
			}
			// block found on prior loop
			if(current_block!=nullptr){
				PageDescriptor *left = split_block(&current_block, current_order);
				// ascertain which half page is in and re-assign current block
				current_block = (page_in_block(left,pgd, current_order - 1)) ? left : buddy_of(left, current_order - 1);
				--current_order;
				continue;
			}
			current_block = _free_areas[current_order];
			// search through the linked-list of blocks for pgd if we have found a free block in the current order
			while(current_block!=nullptr){
				if(page_in_block(current_block,pgd,current_order)){break;}
				// otherwise interrogate next block with same check
				current_block = current_block->next_free;
			}
			if(current_block==nullptr){--current_order;}
		}
		// nothing found
		return false;
	}

	/**
	* Initialises the allocation algorithm.
	* @return Returns TRUE if the algorithm was successfully initialised, FALSE otherwise.
	*/
	bool init(PageDescriptor *page_descriptors, uint64_t nr_page_descriptors) override
	{
		mm_log.messagef(LogLevel::DEBUG, "Buddy Allocator Initialising pd=%p, nr=0x%lx", page_descriptors, nr_page_descriptors);

		// TODO: Initialise the free area linked list for the maximum order
		// to initialise the allocation algorithm.
		if(nr_page_descriptors==0){return false;}
		int current_order = MAX_ORDER;
		do{
			assert(current_order>0);
			// reset variables on each loop
			int current_block_size = pages_per_block(current_order);
			int nr_blocks = nr_page_descriptors / current_block_size;
			PageDescriptor *last_block = page_descriptors + (nr_blocks * current_block_size);

			while(page_descriptors < last_block){
				// build free list for current order
				insert_block(page_descriptors, current_order);
				// increment pgd pointer, decrement available pages, as they are added into the free list
				page_descriptors += current_block_size;
				nr_page_descriptors -= current_block_size;
			}
			--current_order;
		} while(nr_page_descriptors > 0);

		return true;
	}

	/**
	* Returns the friendly name of the allocation algorithm, for debugging and selection purposes.
	*/
	const char* name() const override { return "buddy"; }

	/**
	* Dumps out the current state of the buddy system
	*/
	void dump_state() const override
	{
		// Print out a header, so we can find the output in the logs.
		mm_log.messagef(LogLevel::DEBUG, "BUDDY STATE:");

		// Iterate over each free area.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			char buffer[256];
			snprintf(buffer, sizeof(buffer), "[%d] ", i);

			// Iterate over each block in the free area.
			PageDescriptor *pg = _free_areas[i];
			while (pg) {
				// Append the PFN of the free block to the output buffer.
				snprintf(buffer, sizeof(buffer), "%s%lx ", buffer, sys.mm().pgalloc().pgd_to_pfn(pg));
				pg = pg->next_free;
			}

			mm_log.messagef(LogLevel::DEBUG, "%s", buffer);
		}
	}

// changed to +1 so that if max order is 16, _free_areas has indices 0->16 inclusive
private:
	PageDescriptor *_free_areas[MAX_ORDER+1];
};

/* --- DO NOT CHANGE ANYTHING BELOW THIS LINE --- */

/*
* Allocation algorithm registration framework
*/
RegisterPageAllocator(BuddyPageAllocator);
