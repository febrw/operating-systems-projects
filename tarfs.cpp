/*
 * TAR File-system Driver
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (4)
 */

/*
 * STUDENT NUMBER: s1735009
 */
#include "tarfs.h"
#include <infos/kernel/log.h>

using namespace infos::fs;
using namespace infos::drivers;
using namespace infos::drivers::block;
using namespace infos::kernel;
using namespace infos::util;
using namespace tarfs;

/**
 * TAR files contain header data encoded as octal values in ASCII.  This function
 * converts this terrible representation into a real unsigned integer.
 *
 * You DO NOT need to modify this function.
 *
 * @param data The (null-terminated) ASCII data containing an octal number.
 * @return Returns an unsigned integer number, corresponding to the input data.
 */
static inline unsigned int octal2ui(const char *data)
{
	// Current working value.
	unsigned int value = 0;

	// Length of the input data.
	int len = strlen(data);

	// Starting at i = 1, with a factor of one.
	int i = 1, factor = 1;
	while (i < len) {
		// Extract the current character we're working on (backwards from the end).
		char ch = data[len - i];

		// Add the value of the character, multipled by the factor, to
		// the working value.
		value += factor * (ch - '0');

		// Increment the factor by multiplying it by eight.
		factor *= 8;

		// Increment the current character position.
		i++;
	}

	// Return the current working value.
	return value;
}

// The structure that represents the header block present in
// TAR files.  A header block occurs before every file, this
// this structure must EXACTLY match the layout as described
// in the TAR file format description.
namespace tarfs {
	struct posix_header {         /* byte offset */
  	char name[100];               /*   0 */
  	char mode[8];                 /* 100 */
  	char uid[8];                  /* 108 */
  	char gid[8];                  /* 116 */
  	char size[12];                /* 124 */
  	char mtime[12];               /* 136 */
  	char chksum[8];               /* 148 */
  	char typeflag;                /* 156 */
  	char linkname[100];           /* 157 */
  	char magic[6];                /* 257 */
  	char version[2];              /* 263 */
  	char uname[32];               /* 265 */
  	char gname[32];               /* 297 */
  	char devmajor[8];             /* 329 */
  	char devminor[8];             /* 337 */
  	char prefix[155];             /* 345 */
                                  /* 500 */
	} __packed;
}

/**
 * Reads the contents of the file into the buffer, from the specified file offset.
 * @param buffer The buffer to read the data into.
 * @param size The size of the buffer, and hence the number of bytes to read.
 * @param off The offset within the file.
 * @return Returns the number of bytes read into the buffer.
 */
int TarFSFile::pread(void* buffer, size_t size, off_t off)
{
    if (off >= this->size()) return 0;
	// If either the buffer, or the file we want to read are empty, return 0 bytes read
	if(size == 0 || this->size() == 0) return 0;
	unsigned int idx_last = off + size;
	unsigned int block_size = _owner.block_device().block_size();
	// If buffer size exceeds file size, adjust buffer size to read to EOF only
	if (this->size() < idx_last) {
		idx_last = this->size();
	}
	// Identify which block precedes the block containing the offset byte, store in pre_block
	unsigned int pre_block = 0;
 	for (unsigned int i = 0; i*block_size < off; i++) {
		pre_block = i;
	}
	// Initialise for return
	int bytes_read = 0;
	// cast buffer from void to uint8_t pointer and create temporary buffers
	uint8_t * byte_buffer = (uint8_t *) buffer;
	uint8_t * temp_buffer = new uint8_t[block_size];

	// Read file into a temporary buffer block by block
	for (int i = pre_block; i*block_size < idx_last; ++i) {
		_owner.block_device().read_blocks(temp_buffer, _file_start_block + i, 1);
		unsigned int range = i*block_size;
		// iterate over bytes within each block
		for (unsigned int j=0; j < block_size; ++j) {
			// check we satisfy buffer size and offset constraints before reading
			if(off <= range + j && range + j < idx_last) {
				byte_buffer[bytes_read] = temp_buffer[j];
	            ++bytes_read;
			}
		}
	}
	return bytes_read;
}

/**
 * Reads all the file headers in the TAR file, and builds an in-memory
 * representation.
 * @return Returns the root TarFSNode that corresponds to the TAR file structure.
 */
TarFSNode* TarFS::build_tree()
{
	// Create the root node.
	TarFSNode *root = new TarFSNode(NULL, "", *this);
    unsigned int block_size = block_device().block_size();
    // store header and  block succeeding header block to check if we have reached two 0 blocks indicating EOF
    struct posix_header *header = (struct posix_header *) new char[block_size];
    uint8_t * succ = new uint8_t[block_size];
    // syslog.messagef(LogLevel::DEBUG, "Block Device nr-blocks=%lu", nr_blocks);
    size_t nr_blocks = block_device().block_count();
    unsigned int current_block = 0;
    TarFSNode * parent;

    // reads blocks in pairs into buffers, gets names
    // then depth first builds parent child links, for all files in TAR file
    while(current_block < nr_blocks) {
        // read blocks in pairs to later check zero blocks
        // use zero checks to check whether we are at the end of the TAR file
        block_device().read_blocks(header, current_block, 1);
        block_device().read_blocks(succ, current_block + 1, 1);
        if (is_zero_block((uint8_t*) header) && is_zero_block(succ)) {
            return root;
        }
        // get file size from header and  number of blocks needed to store it
        unsigned int size = octal2ui(header->size);
        unsigned int blocks_needed = (size % block_size == 0) ? size/block_size : (size/block_size)+1;
        // Get file path, split and store path name components
        parent = root;
        String full_path = String(header->name);
        List<String> pnc = full_path.split('/', true);
        // Iterate through components, checking child lists
        int element = 0;
        int pnc_count = pnc.count();
        while(element < pnc_count) {

            // If current element is in this parent's child list, assign parent to child
            if(parent->get_child(pnc.at(element))) {
                parent = (TarFSNode*) (parent->get_child(pnc.at(element)));
                ++element;
            }
            else {
                // otherwise, create a new child, traversing tree depth first
                TarFSNode *child = new TarFSNode(parent, pnc.at(element), *this);
                parent->add_child(pnc.at(element), child);
                if (element == pnc_count - 1) {
                    child->set_block_offset(current_block);
                    // store file size, will return 0 if leaf is a directory
                    child->size(octal2ui(header->size));
                }
                ++element;
            }
        }
        // updated current block index with number of blocks used by this file +1 for the header block
        current_block += blocks_needed + 1;
    }
	return root;
}

/**
 * Returns the size of this TarFS File
 */
unsigned int TarFSFile::size() const
{
	return octal2ui(_hdr->size);
}

/* --- YOU DO NOT NEED TO CHANGE ANYTHING BELOW THIS LINE --- */

/**
 * Mounts a TARFS filesystem, by pre-building the file system tree in memory.
 * @return Returns the root node of the TARFS filesystem.
 */
PFSNode *TarFS::mount()
{
	// If the root node has not been generated, then build it.
	if (_root_node == NULL) {
		_root_node = build_tree();
	}

	// Return the root node.
	return _root_node;
}

/**
 * Constructs a TarFS File object, given the owning file system and the block
 */
TarFSFile::TarFSFile(TarFS& owner, unsigned int file_header_block)
: _hdr(NULL),
_owner(owner),
_file_start_block(file_header_block),
_cur_pos(0)
{
	// Allocate storage for the header.
	_hdr = (struct posix_header *) new char[_owner.block_device().block_size()];

	// Read the header block into the header structure.
	_owner.block_device().read_blocks(_hdr, _file_start_block, 1);

	// Increment the starting block for file data.
	_file_start_block++;
}

TarFSFile::~TarFSFile()
{
	// Delete the header structure that was allocated in the constructor.
	delete _hdr;
}

/**
 * Releases any resources associated with this file.
 */
void TarFSFile::close()
{
	// Nothing to release.
}

/**
 * Reads the contents of the file into the buffer, from the current file offset.
 * The current file offset is advanced by the number of bytes read.
 * @param buffer The buffer to read the data into.
 * @param size The size of the buffer, and hence the number of bytes to read.
 * @return Returns the number of bytes read into the buffer.
 */
int TarFSFile::read(void* buffer, size_t size)
{
	// Read can be seen as a special case of pread, that uses an internal
	// current position indicator, so just delegate actual processing to
	// pread, and update internal state accordingly.

	// Perform the read from the current file position.
	int rc = pread(buffer, size, _cur_pos);

	// Increment the current file position by the number of bytes that was read.
	// The number of bytes actually read may be less than 'size', so it's important
	// we only advance the current position by the actual number of bytes read.
	_cur_pos += rc;

	// Return the number of bytes read.
	return rc;
}

/**
 * Moves the current file pointer, based on the input arguments.
 * @param offset The offset to move the file pointer either 'to' or 'by', depending
 * on the value of type.
 * @param type The type of movement to make.  An absolute movement moves the
 * current file pointer directly to 'offset'.  A relative movement increments
 * the file pointer by 'offset' amount.
 */
void TarFSFile::seek(off_t offset, SeekType type)
{
	// If this is an absolute seek, then set the current file position
	// to the given offset (subject to the file size).  There should
	// probably be a way to return an error if the offset was out of bounds.
	if (type == File::SeekAbsolute) {
		_cur_pos = offset;
	} else if (type == File::SeekRelative) {
		_cur_pos += offset;
	}
	if (_cur_pos >= size()) {
		_cur_pos = size() - 1;
	}
}

TarFSNode::TarFSNode(TarFSNode *parent, const String& name, TarFS& owner) : PFSNode(parent, owner), _name(name), _size(0), _has_block_offset(false), _block_offset(0)
{
}

TarFSNode::~TarFSNode()
{
}

/**
 * Opens this node for file operations.
 * @return
 */
File* TarFSNode::open()
{
	// This is only a file if it has been associated with a block offset.
	if (!_has_block_offset) {
		return NULL;
	}

	// Create a new file object, with a header from this node's block offset.
	return new TarFSFile((TarFS&) owner(), _block_offset);
}

/**
 * Opens this node for directory operations.
 * @return
 */
Directory* TarFSNode::opendir()
{
	return new TarFSDirectory(*this);
}

/**
 * Attempts to retrieve a child node of the given name.
 * @param name
 * @return
 */
PFSNode* TarFSNode::get_child(const String& name)
{
	TarFSNode *child;

	// Try to find the given child node in the children map, and return
	// NULL if it wasn't found.
	if (!_children.try_get_value(name.get_hash(), child)) {
		return NULL;
	}

	return child;
}

/**
 * Creates a subdirectory in this node.  This is a read-only file-system,
 * and so this routine does not need to be implemented.
 * @param name
 * @return
 */
PFSNode* TarFSNode::mkdir(const String& name)
{
	// DO NOT IMPLEMENT
	return NULL;
}

/**
 * A helper routine that updates this node with the offset of the block
 * that contains the header of the file that this node represents.
 * @param offset The block offset that corresponds to this node.
 */
void TarFSNode::set_block_offset(unsigned int offset)
{
	_has_block_offset = true;
	_block_offset = offset;
}

/**
 * A helper routine that adds a child node to the internal children
 * map of this node.
 * @param name The name of the child node.
 * @param child The actual child node.
 */
void TarFSNode::add_child(const String& name, TarFSNode *child)
{
	_children.add(name.get_hash(), child);
}

TarFSDirectory::TarFSDirectory(TarFSNode& node) : _entries(NULL), _nr_entries(0), _cur_entry(0)
{
	_nr_entries = node.children().count();
	_entries = new DirectoryEntry[_nr_entries];

	int i = 0;
	for (const auto& child : node.children()) {
		_entries[i].name = child.value->name();
		_entries[i++].size = child.value->size();
	}
}

TarFSDirectory::~TarFSDirectory()
{
	delete _entries;
}

bool TarFSDirectory::read_entry(infos::fs::DirectoryEntry& entry)
{
	if (_cur_entry < _nr_entries) {
		entry = _entries[_cur_entry++];
		return true;
	} else {
		return false;
	}
}

void TarFSDirectory::close()
{

}

static Filesystem *tarfs_create(VirtualFilesystem& vfs, Device *dev)
{
	if (!dev->device_class().is(BlockDevice::BlockDeviceClass)) return NULL;
	return new TarFS((BlockDevice &) * dev);
}

RegisterFilesystem(tarfs, tarfs_create);
