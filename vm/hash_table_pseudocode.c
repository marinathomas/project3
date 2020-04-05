
// TODO:
// Remember that the whole point of a hash table is to map 'keys' to 'values'
// 0. Copy and paste this code into one of your .c files (or just modify this file) and then follow the
//    'find-and-replace' instructions below
// 1. Replace KEY with the key for your hash table (figure this out)
// 2. Replace VALUE with the value for your hash table (figure this out)
// 3. Replace HASH_TABLE with your hash table (figure this out)

#include "lib/kernel/hash.h"

struct hash* HASH_TABLE;

struct VALUE {
  struct hash_elem hash_elem; // Needed for initializing the hash table ... yay generic pintos hash table implementation
  KEY;
};

// Hash the VALUE elements
unsigned int my_vm_hash(const struct hash_elem *p_, void *aux UNUSED) {
  const struct VALUE *p = hash_entry(p_, struct VALUE, hash_elem);
  return hash_bytes(&p->KEY, sizeof p->KEY);
}

// Returns true if VALUE a is less than VALUE b
bool my_vm_hash_comparison(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED) {
  const struct VALUE *A = hash_entry(a_, struct VALUE, hash_elem);
  const struct VALUE *B = hash_entry(b_, struct VALUE, hash_elem);
  return A->KEY < B->KEY;
}

// Initialize the HASH_TABLE
void my_vm_hash_init() {
  hash_init(HASH_TABLE, my_vm_hash, my_vm_hash_comparison, NULL);
}

// Returns a VALUE given the KEY in HASH_TABLE
struct VALUE* my_vm_hash_lookup(KEY) {
  struct VALUE value;
  value.KEY = KEY; // This line is necessary so that the hash table can use the KEY to find the appropriate VALUE
  struct hash_elem* e = hash_find(HASH_TABLE, &value.hash_elem);
  return e != NULL ? hash_entry(e, struct VALUE, hash_elem) : NULL;
}

// Insert a VALUE into the HASH_TABLE
void my_vm_hash_insert(VALUE) {
  hash_insert(HASH_TABLE, &VALUE->hash_elem);
}
