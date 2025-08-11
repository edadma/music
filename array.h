#ifndef ARRAY_H
#define ARRAY_H

#include <stdlib.h>

// ============================================================================
// DYNAMIC ARRAY SYSTEM
// ============================================================================

// Macro to define array type and function prototypes
#define DEFINE_ARRAY_TYPE(type_name, element_type)                                                                     \
    typedef struct {                                                                                                   \
        element_type* data;                                                                                            \
        int count;                                                                                                     \
        int capacity;                                                                                                  \
    } type_name##_array_t;                                                                                             \
                                                                                                                       \
    void type_name##_array_init(type_name##_array_t* arr);                                                             \
    void type_name##_array_free(type_name##_array_t* arr);                                                             \
    int type_name##_array_push(type_name##_array_t* arr, element_type item);                                           \
    void type_name##_array_clear(type_name##_array_t* arr);                                                            \
    int type_name##_array_shrink_to_fit(type_name##_array_t* arr);

// Macro to define array function implementations (non-static for cross-module use)
#define DEFINE_ARRAY_FUNCTIONS(type_name, element_type)                                                                \
    void type_name##_array_init(type_name##_array_t* arr) {                                                            \
        arr->data = NULL;                                                                                              \
        arr->count = 0;                                                                                                \
        arr->capacity = 0;                                                                                             \
    }                                                                                                                  \
                                                                                                                       \
    void type_name##_array_free(type_name##_array_t* arr) {                                                            \
        free(arr->data);                                                                                               \
        arr->data = NULL;                                                                                              \
        arr->count = 0;                                                                                                \
        arr->capacity = 0;                                                                                             \
    }                                                                                                                  \
                                                                                                                       \
    int type_name##_array_push(type_name##_array_t* arr, element_type item) {                                          \
        if (arr->count >= arr->capacity) {                                                                             \
            arr->capacity = arr->capacity ? arr->capacity * 2 : 16;                                                    \
            element_type* new_data = realloc(arr->data, arr->capacity * sizeof(element_type));                         \
            if (!new_data)                                                                                             \
                return 0;                                                                                              \
            arr->data = new_data;                                                                                      \
        }                                                                                                              \
        arr->data[arr->count++] = item;                                                                                \
        return 1;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    void type_name##_array_clear(type_name##_array_t* arr) { arr->count = 0; }                                         \
                                                                                                                       \
    int type_name##_array_shrink_to_fit(type_name##_array_t* arr) {                                                    \
        if (!arr || !arr->data || arr->count == 0) {                                                                   \
            return 1; /* Nothing to do or already optimal */                                                           \
        }                                                                                                              \
                                                                                                                       \
        if (arr->capacity == arr->count) {                                                                             \
            return 1; /* Already optimal */                                                                            \
        }                                                                                                              \
                                                                                                                       \
        element_type* new_data = realloc(arr->data, arr->count * sizeof(element_type));                                \
        if (!new_data) {                                                                                               \
            return 0; /* Realloc failed, but original data is still valid */                                           \
        }                                                                                                              \
                                                                                                                       \
        arr->data = new_data;                                                                                          \
        arr->capacity = arr->count;                                                                                    \
        return 1;                                                                                                      \
    }

#endif // ARRAY_H
