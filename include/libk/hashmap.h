#ifndef _HASHMAP_H_
#define _HASHMAP_H_

#include <libk/heap.h>
#include <libk/types.h>
#include <libk/vector.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEFAULT_BUCKET_SIZE 127
#define LOAD_FACTOR 0.75

// ~Generic~ hashmap data structure. It can store any value or ptr, and be
// keyed by any type that has a hash_<type> function declared.
// The type-specific hashmaps are generated by using the GENERATE_HASHMAP macro.
//
// Requirements for
//   - there must exist a function: uint32_t key_type_hash(key_type* key)
//   - there must exist a function: bool key_type_equals(key_type* key1, 
//     key_type* key2)  
//   - there must exist a function: void key_type_delete(key_type* key)
//
// Following functions must exist for the value types stored:
//   - copy function: void key_type_delete(key_type* key)
//   - delete function: void key_type_delete(key_type* key)
//
// The macro will generate the new, add, get, remove and delete function, as
// follows
//
// e.g. GENERATE_HASHMAP(string, int) will generate the functions:
//
//      static string_to_int_hashmap* new_string_to_int_hashmap();
//      static void add_string_to_int_hashmap(string_to_int_hashmap* map
//                                            string key, int value);
//      static bool get_string_to_int_hashmap(string_to_int_hashmap* map,
//                                            string* key, int* output);
//      static bool remove_string_to_int_hashmap(string_to_int_hashmap* map,
//                                               string* key, int* output);
//      static void delete_string_to_int_hashmap(string_to_int_hashmap* map);
//
//     
// Given the verbosity of these functions, there are macros to make life easier
// 
// e.g. string_to_int_hashmap* my_map = new_string_to_int_hashmap();
//      add(my_map, key, value);  - calls add_string_to_int_hashmap
//      get(my_map, key)          - calls get_string_to_int_hashmap
//      remove(my_map, key)       - calls remove_string_to_int_hashmap
//      delete(my_map)            - calls delete_string_to_int_hashmap
//
// Pointer values can be stored by adding an alias to a pointer type.

// TODO(psamora) Once we make our vector try to delete contents we can
// push the entry as a deletable object into it

typedef struct hashmap {
  uint32_t size;
  uint32_t capacity;
  size_t entry_size;
  void** buckets;
} hashmap __attribute__((packed));

hashmap* new_hashmap(size_t hashmap_size, size_t entry_size);
void hashmap_resize(hashmap* hashmap);

#define add(__hashmap, ...) __hashmap->add(__hashmap, __VA_ARGS__)
#define get(__hashmap, ...) __hashmap->get(__hashmap, __VA_ARGS__)
#define delete(__hashmap) __hashmap->delete(__hashmap)

#define GENERATE_HASHMAP(key_type, value_type)                                 \
  /* A single entry in the Hashmap and its copy/delete functions */            \
  typedef struct key_type##_to_##value_type##_entry {                          \
    key_type key;                                                              \
    value_type value;                                                          \
  } key_type##_to_##value_type##_entry __attribute__((packed));                \
                                                                               \
  static void copy_##key_type##_to_##value_type##_entry(                       \
        key_type##_to_##value_type##_entry to_copy,                            \
        key_type##_to_##value_type##_entry* output) {                          \
    copy_##key_type(to_copy.key, &output->key);                                \
    copy_##value_type(to_copy.value, &output->value);                          \
  }                                                                            \
                                                                               \
  static void delete_##key_type##_to_##value_type##_entry(                     \
      key_type##_to_##value_type##_entry* to_delete) {                         \
    delete_##key_type(to_delete->key);                                         \
    delete_##value_type(to_delete->value);                                     \  
  };                                                                           \
                                                                               \
  /* Each bucket in the hashmap will be a vector of entries */                 \
  GENERATE_VECTOR(key_type##_to_##value_type##_entry);                         \
                                                                               \
  typedef struct key_type##_to_##value_type##_hashmap {                        \
    uint32_t size;                                                             \
    uint32_t capacity;                                                         \
    size_t entry_size;                                                         \
    key_type##_to_##value_type##_entry_vector** buckets;                       \
    void (*add) (struct key_type##_to_##value_type##_hashmap* hashmap,         \
    	           key_type key, value_type value);                              \
    bool (*get) (struct key_type##_to_##value_type##_hashmap* hashmap,         \
    	           key_type* key, value_type* output);                           \
    void (*delete) (struct key_type##_to_##value_type##_hashmap* hashmap);     \
  } key_type##_to_##value_type##_hashmap __attribute__((packed));              \
                                                                               \
  static void add_##key_type##_to_##value_type##_hashmap(                      \
  		struct key_type##_to_##value_type##_hashmap* hashmap, key_type key,      \
  	  value_type value) {                                                      \
    if (hashmap->size > hashmap->capacity * LOAD_FACTOR) {                     \
      hashmap_resize(hashmap);                                                 \
    }                                                                          \
                                                                               \
    /* Grab the vector for this entry should be added to, if it exists */      \
    uint32_t hash = key_type##_hash(&key);                                     \
    size_t bucket = hash % hashmap->capacity;                                  \
    if (hashmap->buckets[bucket] == NULL) {                                    \
      hashmap->buckets[bucket] =                                               \
        new_##key_type##_to_##value_type##_entry_vector();                     \
    }                                                                          \
                                                                               \
    /* Creates a local entry and then push it in the bucket vector */          \
    /* TODO(psamora) Implement emplace_back on vector */                       \
    key_type##_to_##value_type##_entry entry;                                  \
    copy_##key_type(key, &entry.key);                                          \
    copy_##value_type(value, &entry.value);                                    \
    push(hashmap->buckets[bucket], entry);                                     \
    hashmap->size++;                                                           \
  }                                                                            \
                                                                               \
  static bool get_##key_type##_to_##value_type##_hashmap(                      \ 
      struct key_type##_to_##value_type##_hashmap* hashmap, key_type* key,     \
      value_type* output) {                                                    \
    /* Search for the bucket for this key */                                   \
    uint32_t hash = key_type##_hash(key);                                      \
    key_type##_to_##value_type##_entry_vector* bucket =                        \
      hashmap->buckets[hash % hashmap->capacity];                              \
    if (bucket == NULL) {                                                      \
      return false;                                                            \
    }                                                                          \
                                                                               \
    /* Searches for the entry that matches the given key */                    \
    for (size_t i = 0; i < bucket->size; i++) {                                \
      key_type##_to_##value_type##_entry entry;                                \
      if (!get(bucket, i, &entry)) {                                           \
        continue;                                                              \
      }                                                                        \
      if (key_type##_equals(*key, entry.key)) {                                \
        *output = entry.value;                                                 \
        return true;                                                           \
      }                                                                        \
    }                                                                          \
                                                                               \
    return false;                                                              \
  }                                                                            \
                                                                               \
  static void delete_##key_type##_to_##value_type##_hashmap(                   \
      struct key_type##_to_##value_type##_hashmap* hashmap) {                  \
    for (size_t i = 0; i < hashmap->capacity; i++) {                           \
      if (hashmap->buckets[i] != NULL) {                                       \
        delete(hashmap->buckets[i]);                                           \
      }                                                                        \
    }                                                                          \
    kfree(hashmap->buckets);                                                   \
    kfree(hashmap);                                                            \
  }                                                                            \
                                                                               \
  static key_type##_to_##value_type##_hashmap*                                 \
      new_##key_type##_to_##value_type##_hashmap() {                           \
    key_type##_to_##value_type##_hashmap* hashmap = new_hashmap(               \
      sizeof(key_type##_to_##value_type##_hashmap),                            \
      sizeof(key_type##_to_##value_type##_entry));                             \
    hashmap->add = add_##key_type##_to_##value_type##_hashmap;                 \
    hashmap->get = get_##key_type##_to_##value_type##_hashmap;                 \
    hashmap->delete = delete_##key_type##_to_##value_type##_hashmap;           \
    return hashmap;                                                            \
  }                                                                            \

GENERATE_HASHMAP(int, int);
// GENERATE_HASHMAP(string, int);

// #define remove(__hashmap, __key) __hashmap->remove(__hashmap, __key)
// type (*remove) (struct key_type##_to_##value_type##_hashmap* hashmap,
//                key_type key, value_type* output);

#endif  // _HASHMAP_H_